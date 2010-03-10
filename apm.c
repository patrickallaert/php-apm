/*
 +----------------------------------------------------------------------+
 |  APM stands for Alternative PHP Monitor                              |
 +----------------------------------------------------------------------+
 | Copyright (c) 2008-2010  Davide Mendolia, Patrick Allaert            |
 +----------------------------------------------------------------------+
 | This source file is subject to version 3.01 of the PHP license,      |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.php.net/license/3_01.txt                                  |
 | If you did not receive a copy of the PHP license and are unable to   |
 | obtain it through the world-wide-web, please send a note to          |
 | license@php.net so we can mail you a copy immediately.               |
 +----------------------------------------------------------------------+
*/

#define APM_VERSION "1.0.0beta2"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* gettimeofday */
#ifdef PHP_WIN32
# include "win32/time.h"
#else
# include "main/php_config.h"
#endif

#include <sqlite3.h>
#include <time.h>
#include "php.h"
#include "php_ini.h"
#include "zend_exceptions.h"
#include "zend_alloc.h"
#include "php_apm.h"
#include "backtrace.h"
#include "ext/standard/info.h"
#include "ext/date/php_date.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/php_filestat.h"

#define DB_FILE "events"
#define SEC_TO_USEC(sec) ((sec) * 1000000.00)
#define USEC_TO_SEC(usec) ((usec) / 1000000.00)

void (*old_error_cb)(int type, const char *error_filename,
                     const uint error_lineno, const char *format,
                     va_list args);

void apm_error_cb(int type, const char *error_filename,
                  const uint error_lineno, const char *format,
                  va_list args);

void apm_throw_exception_hook(zval *exception TSRMLS_DC);

static int event_callback_html(void *, int, char **, char **);
static int event_callback_json(void *, int, char **, char **);
static int slow_request_callback_html(void *, int, char **, char **);
static int slow_request_callback_json(void *, int, char **, char **);
static int event_callback_count(void *count, int num_fields, char **fields, char **col_name);
static int event_callback_event_info(void *filename, int num_fields, char **fields, char **col_name);
static long get_table_count(char * table);
static int perform_db_access_checks(const char *path TSRMLS_DC);
static void insert_event(int, char *, uint, char * TSRMLS_DC);

#ifdef PHP_WIN32
#define PHP_JSON_API __declspec(dllexport)
#else
#define PHP_JSON_API
#endif

PHP_JSON_API void php_json_encode(smart_str *buf, zval *val TSRMLS_DC);

static int odd_event_list = 1;
static int odd_slow_request = 1;

/* recorded timestamp for the request */
struct timeval begin_tp;

function_entry apm_functions[] = {
		PHP_FE(apm_get_events, NULL)
		PHP_FE(apm_get_slow_requests, NULL)
		PHP_FE(apm_get_events_count, NULL)
		PHP_FE(apm_get_slow_requests_count, NULL)
		PHP_FE(apm_get_event_info, NULL)
	{NULL, NULL, NULL}
};

zend_module_entry apm_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"apm",
	apm_functions,
	PHP_MINIT(apm),
	PHP_MSHUTDOWN(apm),
	PHP_RINIT(apm),	
	PHP_RSHUTDOWN(apm),
	PHP_MINFO(apm),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1.0",
#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_APM
ZEND_GET_MODULE(apm)
#endif

ZEND_DECLARE_MODULE_GLOBALS(apm)

static PHP_INI_MH(OnUpdateDBFile)
{
	if (new_value && new_value_length > 0) {
		snprintf(APM_G(db_file), MAXPATHLEN, "%s/%s", new_value, DB_FILE);

		if (perform_db_access_checks(new_value TSRMLS_CC) == FAILURE) {
			APM_G(enabled) = 0;
			APM_G(event_enabled) = 0;
		}
	} else {
		APM_G(enabled) = 0;
		APM_G(event_enabled) = 0;
	}
	return OnUpdateString(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
}

PHP_INI_BEGIN()
	/* Boolean controlling whether the extension is globally active or not */
	STD_PHP_INI_BOOLEAN("apm.enabled",                "1",                      PHP_INI_ALL, OnUpdateBool,   enabled,                zend_apm_globals, apm_globals)
	/* Boolean controlling whether the event monitoring is active or not */
	STD_PHP_INI_BOOLEAN("apm.event_enabled",          "1",                      PHP_INI_ALL, OnUpdateBool,   event_enabled,          zend_apm_globals, apm_globals)
	/* Boolean controlling whether the slow request monitoring is active or not */
	STD_PHP_INI_BOOLEAN("apm.slow_request_enabled",   "1",                      PHP_INI_ALL, OnUpdateBool,   slow_request_enabled,   zend_apm_globals, apm_globals)
	/* Boolean controlling whether the the stacktrace should be generated or not */
	STD_PHP_INI_BOOLEAN("apm.stacktrace_enabled",     "1",                      PHP_INI_ALL, OnUpdateBool,   stacktrace_enabled,     zend_apm_globals, apm_globals)
	/* Path to the SQLite database file */
	STD_PHP_INI_ENTRY("apm.max_event_insert_timeout", "100",                    PHP_INI_ALL, OnUpdateLong,   timeout,                zend_apm_globals, apm_globals)
	/* Max timeout to wait for storing the event in the DB */
	STD_PHP_INI_ENTRY("apm.db_path",                  "/var/php/apm/db",        PHP_INI_ALL, OnUpdateDBFile, db_path,                zend_apm_globals, apm_globals)
	/* Time (in ms) before a request is considered 'slow' */
	STD_PHP_INI_ENTRY("apm.slow_request_duration",    "100",                    PHP_INI_ALL, OnUpdateLong,   slow_request_duration,  zend_apm_globals, apm_globals)
PHP_INI_END()

static void apm_init_globals(zend_apm_globals *apm_globals)
{
}

PHP_MINIT_FUNCTION(apm)
{
	ZEND_INIT_MODULE_GLOBALS(apm, apm_init_globals, NULL);
	REGISTER_INI_ENTRIES();

	REGISTER_LONG_CONSTANT("APM_ORDER_ID", APM_ORDER_ID, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("APM_ORDER_TIMESTAMP", APM_ORDER_TIMESTAMP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("APM_ORDER_TYPE", APM_ORDER_TYPE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("APM_ORDER_DURATION", APM_ORDER_DURATION, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("APM_ORDER_FILE", APM_ORDER_FILE, CONST_CS | CONST_PERSISTENT);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(apm)
{
	UNREGISTER_INI_ENTRIES();

	/* Restoring saved error callback function */
	zend_error_cb = old_error_cb;

	return SUCCESS;
}

PHP_RINIT_FUNCTION(apm)
{
	/* Storing actual error callback function for later restore */
	old_error_cb = zend_error_cb;
	
	if (APM_G(enabled)) {	
		if (APM_G(event_enabled)) {
			/* storing timestamp of request */
			gettimeofday(&begin_tp, NULL);
		}
		/* Opening the sqlite database file */
		if (sqlite3_open(APM_G(db_file), &APM_G(event_db))) {
			/*
			 Closing DB file and stop loading the extension
			 in case of error while opening the database file
			 */
			sqlite3_close(APM_G(event_db));
			return FAILURE;
		}

		sqlite3_busy_timeout(APM_G(event_db), APM_G(timeout));

		/* Making the connection asynchronous, not waiting for data being really written to the disk */
		sqlite3_exec(APM_G(event_db), "PRAGMA synchronous = OFF", NULL, NULL, NULL);

		/* Replacing current error callback function with apm's one */
		zend_error_cb = apm_error_cb;
		zend_throw_exception_hook = apm_throw_exception_hook;
	}
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(apm)
{
	if (APM_G(enabled) && APM_G(slow_request_enabled)) {
		float duration;
		struct timeval end_tp;

		gettimeofday(&end_tp, NULL);

		/* Request longer than accepted thresold ? */
		duration = SEC_TO_USEC(end_tp.tv_sec - begin_tp.tv_sec) + end_tp.tv_usec - begin_tp.tv_usec;
		if (duration > 1000.0 * APM_G(slow_request_duration)) {
			zval **array;
			zval **token;
			char *script_filename = NULL;
			char *sql;

			if (zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &array) == SUCCESS &&
				Z_TYPE_PP(array) == IS_ARRAY &&
#if (PHP_MAJOR_VERSION < 6)
				zend_hash_find
#else
				zend_ascii_hash_find
#endif
					(Z_ARRVAL_PP(array), "SCRIPT_FILENAME", sizeof("SCRIPT_FILENAME"), (void **) &token) == SUCCESS) {
#if (PHP_MAJOR_VERSION < 6)
				script_filename = Z_STRVAL_PP(token);
#else
				script_filename = zend_unicode_to_ascii(Z_USTRVAL_PP(token), Z_USTRLEN_PP(token) TSRMLS_CC);
#endif
			}

			/* Building SQL insert query */
			sql = sqlite3_mprintf("INSERT INTO slow_request (ts, duration, file) VALUES (datetime(), %f, %Q);",
			                      USEC_TO_SEC(duration), script_filename);

			/* Executing SQL insert query */
			sqlite3_exec(APM_G(event_db), sql, NULL, NULL, NULL);

			sqlite3_free(sql);
		}
		
	}

	/* Restoring saved error callback function */
	zend_error_cb = old_error_cb;
	zend_throw_exception_hook = NULL;
	return SUCCESS;
}

PHP_MINFO_FUNCTION(apm)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "APM support", "enabled");
	php_info_print_table_row(2, "Version", APM_VERSION);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

/* {{{ void apm_error(int type, const char *format, ...)
   This function provides a hook for error */
void apm_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
	TSRMLS_FETCH();
	
	if (APM_G(event_enabled)) {
		char *msg;
		va_list args_copy;

		/* A copy of args is needed to be used for the old_error_cb */
		va_copy(args_copy, args);
		vspprintf(&msg, 0, format, args_copy);
		va_end(args_copy);

		/* We need to see if we have an uncaught exception fatal error now */
		if (type == E_ERROR && strncmp(msg, "Uncaught exception", 18) == 0) {

		} else {
			insert_event(type, (char *) error_filename, error_lineno, msg TSRMLS_CC);
		}
		efree(msg);
	}

	/* Calling saved callback function for error handling */
	old_error_cb(type, error_filename, error_lineno, format, args);
}
/* }}} */


void apm_throw_exception_hook(zval *exception TSRMLS_DC)
{
	if (APM_G(event_enabled)) {
		zval *message, *file, *line;
		zend_class_entry *default_ce, *exception_ce;

		if (!exception) {
			return;
		}

		default_ce = zend_exception_get_default(TSRMLS_C);
		exception_ce = zend_get_class_entry(exception TSRMLS_CC);

		message = zend_read_property(default_ce, exception, "message", sizeof("message")-1, 0 TSRMLS_CC);
		file =    zend_read_property(default_ce, exception, "file",    sizeof("file")-1,    0 TSRMLS_CC);
		line =    zend_read_property(default_ce, exception, "line",    sizeof("line")-1,    0 TSRMLS_CC);

		insert_event(E_ERROR, Z_STRVAL_P(file), Z_LVAL_P(line), Z_STRVAL_P(message) TSRMLS_CC);
	}
}


/* {{{ proto bool apm_get_events([, int limit[, int offset[, int order[, bool asc[, bool json]]]]]) U
   Returns HTML/JSON with all events */
PHP_FUNCTION(apm_get_events)
{
	sqlite3 *db;
	long order = APM_ORDER_ID;
	long limit = 25;
	long offset = 0;
	char *sql;
	zend_bool json = 0;
	zend_bool asc = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|lllbb", &limit, &offset, &order, &asc, &json) == FAILURE) {
		return;
	}

	/* Opening the sqlite database file */
	if (sqlite3_open(APM_G(db_file), &db)) {
		/* Closing DB file and returning false */
		sqlite3_close(db);
		RETURN_FALSE;
	}

	if (!json) {
		php_printf("<table id=\"event-list\"><tr><th>#</th><th>Time</th><th>Type</th><th>File</th><th>Line</th><th>Message</th><th>Backtrace</th></tr>\n");
		odd_event_list = 1;
	}

	if (order < 1 || order > 4) {
		order = 1;
	}

	sql = sqlite3_mprintf("SELECT id, ts, CASE type \
                          WHEN 1 THEN 'E_ERROR' \
                          WHEN 2 THEN 'E_WARNING' \
                          WHEN 4 THEN 'E_PARSE' \
                          WHEN 8 THEN 'E_NOTICE' \
                          WHEN 16 THEN 'E_CORE_ERROR' \
                          WHEN 32 THEN 'E_CORE_WARNING' \
                          WHEN 64 THEN 'E_COMPILE_ERROR' \
                          WHEN 128 THEN 'E_COMPILE_WARNING' \
                          WHEN 256 THEN 'E_USER_ERROR' \
                          WHEN 512 THEN 'E_USER_WARNING' \
                          WHEN 1024 THEN 'E_USER_NOTICE' \
                          WHEN 2048 THEN 'E_STRICT' \
                          WHEN 4096 THEN 'E_RECOVERABLE_ERROR' \
                          WHEN 8192 THEN 'E_DEPRECATED' \
                          WHEN 16384 THEN 'E_USER_DEPRECATED' \
                          END, \
							  file, line, message, backtrace FROM event ORDER BY %d %s LIMIT %d OFFSET %d", order, asc ? "ASC" : "DESC", limit, offset);
	sqlite3_exec(db, sql, json ? event_callback_json : event_callback_html, NULL, NULL);
	if (!json) {
		php_printf("</table>");
	}

	sqlite3_free(sql);
	sqlite3_close(db);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool apm_get_slow_requests([, int limit[, int offset[, int order[, bool asc[, bool json]]]]]) U
   Returns HTML/JSON with all slow requests */
PHP_FUNCTION(apm_get_slow_requests)
{
	sqlite3 *db;
	long order = APM_ORDER_ID;
	long limit = 25;
	long offset = 0;
	char *sql;
	zend_bool json = 0;
	zend_bool asc = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|lllbb", &limit, &offset, &order, &asc, &json) == FAILURE) {
		return;
	}

	/* Opening the sqlite database file */
	if (sqlite3_open(APM_G(db_file), &db)) {
		/* Closing DB file and returning false */
		sqlite3_close(db);
		RETURN_FALSE;
	}

	if (!json) {
		php_printf("<table id=\"slow-request-list\"><tr><th>#</th><th>Time</th><th>Duration</th><th>File</th></tr>\n");
		odd_slow_request = 1;
	}
	
	sql = sqlite3_mprintf("SELECT id, ts, duration, file FROM slow_request ORDER BY %d %s LIMIT %d OFFSET %d", order, asc ? "ASC" : "DESC", limit, offset);
	sqlite3_exec(db, sql, json ? slow_request_callback_json : slow_request_callback_html, NULL, NULL);

	if (!json) {
		php_printf("</table>");
	}

	sqlite3_free(sql);
	sqlite3_close(db);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int apm_get_events_count() U
   Return the number of events */
PHP_FUNCTION(apm_get_events_count)
{
	long count;

	count = get_table_count("event");
	if (count == -1) {
		RETURN_FALSE;
	}
	RETURN_LONG(count);
}
/* }}} */

/* {{{ proto int apm_get_events_count() U
   Return the number of slow requests */
PHP_FUNCTION(apm_get_slow_requests_count)
{
	long count;

	count = get_table_count("slow_request");
	if (count == -1) {
		RETURN_FALSE;
	}
	RETURN_LONG(count);
}
/* }}} */

/* {{{ proto array apm_get_event_into(int eventID) U
   Returns all information available on a request */
PHP_FUNCTION(apm_get_event_info)
{
	sqlite3 *db;
	long id = 0;
	char *sql;
	apm_event_info info;
	info.file = NULL;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &id) == FAILURE) {
		return;
	}

	/* Opening the sqlite database file */
	if (sqlite3_open(APM_G(db_file), &db)) {
		/* Closing DB file and returning false */
		sqlite3_close(db);
		RETURN_FALSE;
	}

	sql = sqlite3_mprintf("SELECT id, ts, type, file, line, message, backtrace FROM event WHERE id = %d", id);
	sqlite3_exec(db, sql, event_callback_event_info, &info, NULL);

	sqlite3_free(sql);
	sqlite3_close(db);

	if (info.file == NULL) {
		RETURN_FALSE;
	}

	array_init(return_value);

	add_assoc_long(return_value, "timestamp", info.ts);
	add_assoc_string(return_value, "file", info.file, 1);
	add_assoc_long(return_value, "line", info.line);
	add_assoc_long(return_value, "type", info.type);
	add_assoc_string(return_value, "message", info.message, 1);
	add_assoc_string(return_value, "stacktrace", info.stacktrace, 1);
}
/* }}} */

/* Returns the number of rows of a table */
static long get_table_count(char * table)
{
	sqlite3 *db;
	char *sql;
	long count;

	/* Opening the sqlite database file */
	if (sqlite3_open(APM_G(db_file), &db)) {
		/* Closing DB file and returning false */
		sqlite3_close(db);
		return -1;
	}

	sql = sqlite3_mprintf("SELECT COUNT(*) FROM %s", table);
	sqlite3_exec(db, sql, event_callback_count, &count, NULL);

	sqlite3_free(sql);
	sqlite3_close(db);

	return count;
}

/* Function called for every row returned by event query (html version) */
static int event_callback_html(void *data, int num_fields, char **fields, char **col_name)
{
	php_printf("<tr class=\"%s %s\"><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td><pre>%s</pre></td></tr>\n",
               fields[2], odd_event_list ? "odd" : "even", fields[0], fields[1], fields[2], fields[3], fields[4], fields[5], fields[6]);
	odd_event_list = !odd_event_list;

	return 0;
}

/* Function called for the row returned by event info query */
static int event_callback_event_info(void *info, int num_fields, char **fields, char **col_name)
{
	// Most logic here is to transform the date from string format to unix timestamp
	timelib_time *t, *now;
	timelib_tzinfo *tzi;
	int error1, error2;
	long ts;
	struct timelib_error_container *error;

	tzi = get_timezone_info(TSRMLS_C);
	now = timelib_time_ctor();
	now->tz_info = tzi;
	now->zone_type = TIMELIB_ZONETYPE_ID;
	timelib_unixtime2local(now, (timelib_sll) time(NULL));

	t = timelib_strtotime(fields[1], strlen(fields[1]), &error, timelib_builtin_db());
	error1 = error->error_count;
	timelib_error_container_dtor(error);
	timelib_fill_holes(t, now, TIMELIB_NO_CLONE);
	timelib_update_ts(t, tzi);
	ts = timelib_date_to_int(t, &error2);

	timelib_time_dtor(now);
	timelib_time_dtor(t);

	(*(apm_event_info *) info).ts = (!error1 && !error2) ? ts : -1;
	(*(apm_event_info *) info).file = estrdup(fields[3]);
	(*(apm_event_info *) info).line = atoi(fields[4]);
	(*(apm_event_info *) info).type = atoi(fields[2]);
	(*(apm_event_info *) info).message = estrdup(fields[5]);
	(*(apm_event_info *) info).stacktrace = estrdup(fields[6]);

	return 0;
}

/* Function called for every row returned by event query (json version) */
static int event_callback_json(void *data, int num_fields, char **fields, char **col_name)
{
	smart_str file = {0};
	smart_str msg = {0};
	smart_str trace = {0};
	zval zfile, zmsg, ztrace;

	Z_TYPE(zfile) = IS_STRING;
	Z_STRVAL(zfile) = fields[3];
	Z_STRLEN(zfile) = strlen(fields[3]);

	Z_TYPE(zmsg) = IS_STRING;
	Z_STRVAL(zmsg) = fields[5];
	Z_STRLEN(zmsg) = strlen(fields[5]);

	Z_TYPE(ztrace) = IS_STRING;
	Z_STRVAL(ztrace) = fields[6];
	Z_STRLEN(ztrace) = strlen(fields[6]);

	php_json_encode(&file, &zfile TSRMLS_CC);
	php_json_encode(&msg, &zmsg TSRMLS_CC);
	php_json_encode(&trace, &ztrace TSRMLS_CC);

	smart_str_0(&file);
	smart_str_0(&msg);
	smart_str_0(&trace);

	php_printf("{id:\"%s\", cell:[\"%s\", \"%s\", \"%s\", %s, \"%s\", %s, %s]},\n",
               fields[0], fields[0], fields[1], fields[2], file.c, fields[4], msg.c, trace.c);

	smart_str_free(&file);
	smart_str_free(&msg);
	smart_str_free(&trace);

	return 0;
}

/* Function called for every row returned by slow request query (html version) */
static int slow_request_callback_html(void *data, int num_fields, char **fields, char **col_name)
{
	php_printf("<tr class=\"%s\"><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
               odd_slow_request ? "odd" : "even", fields[0], fields[1], fields[2], fields[3]);
	odd_slow_request = !odd_slow_request;

	return 0;
}

/* Function called for every row returned by slow request query (json version) */
static int slow_request_callback_json(void *data, int num_fields, char **fields, char **col_name)
{
	smart_str file = {0};
	zval zfile;

	Z_TYPE(zfile) = IS_STRING;
	Z_STRVAL(zfile) = fields[3];
	Z_STRLEN(zfile) = strlen(fields[3]);

	php_json_encode(&file, &zfile TSRMLS_CC);

	smart_str_0(&file);

	php_printf("{id:\"%s\", cell:[\"%s\", \"%s\", \"%s\", %s]},\n",
               fields[0], fields[0], fields[1], fields[2], file.c);

	smart_str_free(&file);

	return 0;
}

/* Callback function called for COUNT(*) queries */
static int event_callback_count(void *count, int num_fields, char **fields, char **col_name)
{
	*(long *) count = atol(fields[0]);

	return 0;
}

/* Perform access checks on the DB path */
static int perform_db_access_checks(const char *path TSRMLS_DC)
{
	zend_bool is_dir;
	zval *stat;

	MAKE_STD_ZVAL(stat);
	php_stat(path, strlen(path), FS_IS_DIR, stat TSRMLS_CC);
	
	is_dir = Z_BVAL_P(stat);
	zval_dtor(stat);
	FREE_ZVAL(stat);
		
	/* Does db_path exists ? */
	if (!is_dir) {
		zend_error(E_CORE_WARNING, "APM cannot be enabled, '%s' is not directory", path);
		return FAILURE;
	}

	if (VCWD_ACCESS(path, W_OK | R_OK | X_OK) != SUCCESS) {
		zend_error(E_CORE_WARNING, "APM cannot be enabled, %s needs to readable, writable and executable", path);
		return FAILURE;
	}
	
	return SUCCESS;
}

/* Insert an event in the backend */
static void insert_event(int type, char * error_filename, uint error_lineno, char * msg TSRMLS_DC)
{
	/* sql variables */
	char *sql;
	smart_str trace_str = {0};

	if (APM_G(stacktrace_enabled)) {
		append_backtrace(&trace_str TSRMLS_CC);
		smart_str_0(&trace_str);
	}

	/* Builing SQL insert query */
	sql = sqlite3_mprintf("INSERT INTO event (ts, type, file, line, message, backtrace) VALUES (datetime(), %d, %Q, %d, %Q, %Q);",
		                  type, error_filename ? error_filename : "", error_lineno, msg ? msg : "", (APM_G(stacktrace_enabled) && trace_str.c) ? trace_str.c : "");
	/* Executing SQL insert query */
	sqlite3_exec(APM_G(event_db), sql, NULL, NULL, NULL);

	smart_str_free(&trace_str);
	sqlite3_free(sql);
}
