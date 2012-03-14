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
 | Authors: Davide Mendolia <dmendolia@php.net>                         |
 |          Patrick Allaert <patrickallaert@php.net>                    |
 +----------------------------------------------------------------------+
*/

#include <sqlite3.h>
#include <time.h>
#include "php_apm.h"
#include "php_ini.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/php_filestat.h"
#include "driver_sqlite3.h"
#ifdef NETWARE
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

static int event_callback_event_info(void *info, int num_fields, char **fields, char **col_name);
static int event_callback(void *data, int num_fields, char **fields, char **col_name);
static int slow_request_callback(void *data, int num_fields, char **fields, char **col_name);
static int event_callback_count(void *count, int num_fields, char **fields, char **col_name);
static long get_table_count(char * table);

#ifdef PHP_WIN32
#define PHP_JSON_API __declspec(dllexport)
#else
#define PHP_JSON_API
#endif

PHP_JSON_API void php_json_encode(smart_str *buf, zval *val TSRMLS_DC);

ZEND_EXTERN_MODULE_GLOBALS(apm)

ZEND_DECLARE_MODULE_GLOBALS(apm_sqlite3)

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

static PHP_INI_MH(OnUpdateDBFile)
{
	if (new_value && new_value_length > 0) {
		snprintf(APM_S3_G(db_file), MAXPATHLEN, "%s/%s", new_value, DB_FILE);

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

APM_DRIVER_CREATE(sqlite3)

PHP_INI_BEGIN()
	/* Boolean controlling whether the driver is active or not */
	STD_PHP_INI_BOOLEAN("apm.sqlite_enabled",                "1",               PHP_INI_PERDIR, OnUpdateBool,   enabled,         zend_apm_sqlite3_globals, apm_sqlite3_globals)
	/* error_reporting of the driver */
	STD_PHP_INI_ENTRY("apm.sqlite_error_reporting",          NULL,              PHP_INI_ALL,    OnUpdateAPMsqlite3ErrorReporting,   error_reporting, zend_apm_sqlite3_globals, apm_sqlite3_globals)
	/* Path to the SQLite database file */
	STD_PHP_INI_ENTRY("apm.sqlite_max_event_insert_timeout", "100",             PHP_INI_ALL,    OnUpdateLong,   timeout,         zend_apm_sqlite3_globals, apm_sqlite3_globals)
	/* Max timeout to wait for storing the event in the DB */
	STD_PHP_INI_ENTRY("apm.sqlite_db_path",                  "/var/php/apm/db", PHP_INI_ALL,    OnUpdateDBFile, db_path,         zend_apm_sqlite3_globals, apm_sqlite3_globals)
PHP_INI_END()

/* Insert an event in the backend */
void apm_driver_sqlite3_insert_event(int type, char * error_filename, uint error_lineno, char * msg, char * trace, char * uri, char * ip TSRMLS_DC)
{
	char *sql;
	int ip_int = 0;
	struct in_addr ip_addr;

	if (ip && (inet_pton(AF_INET, ip, &ip_addr) == 1)) {
		ip_int = ntohl(ip_addr.s_addr);
	}

	/* Builing SQL insert query */
	sql = sqlite3_mprintf("INSERT INTO event (ts, type, file, line, message, backtrace, uri, ip, cookies) VALUES (%d, %d, %Q, %d, %Q, %Q, %Q, %d);",
		                  (long)time(NULL), type, error_filename ? error_filename : "", error_lineno, msg ? msg : "", trace ? trace : "", uri ? uri : "", ip_int);
	/* Executing SQL insert query */
	sqlite3_exec(APM_S3_G(event_db), sql, NULL, NULL, NULL);

	sqlite3_free(sql);
}

int apm_driver_sqlite3_minit(int module_number)
{
	REGISTER_INI_ENTRIES();
	return SUCCESS;
}

int apm_driver_sqlite3_rinit()
{
	/* Opening the sqlite database file */
	if (sqlite3_open(APM_S3_G(db_file), &APM_S3_G(event_db))) {
		/*
		 Closing DB file and stop loading the extension
		 in case of error while opening the database file
		 */
		sqlite3_close(APM_S3_G(event_db));
		return FAILURE;
	}

	sqlite3_busy_timeout(APM_S3_G(event_db), APM_S3_G(timeout));

	/* Making the connection asynchronous, not waiting for data being really written to the disk */
	sqlite3_exec(APM_S3_G(event_db), "PRAGMA synchronous = OFF", NULL, NULL, NULL);

	return SUCCESS;
}

int apm_driver_sqlite3_mshutdown()
{
	return SUCCESS;
}

int apm_driver_sqlite3_rshutdown()
{
	sqlite3_close(APM_S3_G(event_db));
	return SUCCESS;
}

void apm_driver_sqlite3_insert_slow_request(float duration, char * script_filename)
{
	char *sql;

	/* Building SQL insert query */
	sql = sqlite3_mprintf("INSERT INTO slow_request (ts, duration, file) VALUES (datetime(), %f, %Q);",
						  USEC_TO_SEC(duration), script_filename);

	/* Executing SQL insert query */
	sqlite3_exec(APM_S3_G(event_db), sql, NULL, NULL, NULL);

	sqlite3_free(sql);
}

/* {{{ proto bool apm_get_sqlite_events([, int limit[, int offset[, int order[, bool asc]]]])
   Returns JSON with all events */
PHP_FUNCTION(apm_get_sqlite_events)
{
	sqlite3 *db;
	long order = APM_ORDER_ID;
	long limit = 25;
	long offset = 0;
	char *sql;
	zend_bool asc = 0;
	int odd_event_list = 1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|lllb", &limit, &offset, &order, &asc) == FAILURE) {
		return;
	}

	/* Opening the sqlite database file */
	if (sqlite3_open(APM_S3_G(db_file), &db)) {
		/* Closing DB file and returning false */
		sqlite3_close(db);
		RETURN_FALSE;
	}

	if (order < 1 || order > 5) {
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
							  file, ip, line, message FROM event ORDER BY %d %s LIMIT %d OFFSET %d", order, asc ? "ASC" : "DESC", limit, offset);
	sqlite3_exec(db, sql, event_callback, &odd_event_list, NULL);

	sqlite3_free(sql);
	sqlite3_close(db);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool apm_get_sqlite_slow_requests([, int limit[, int offset[, int order[, bool asc]]]])
   Returns JSON with all slow requests */
PHP_FUNCTION(apm_get_sqlite_slow_requests)
{
	sqlite3 *db;
	long order = APM_ORDER_ID;
	long limit = 25;
	long offset = 0;
	char *sql;
	zend_bool asc = 0;
	int odd_slow_request = 1;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|lllb", &limit, &offset, &order, &asc) == FAILURE) {
		return;
	}

	/* Opening the sqlite database file */
	if (sqlite3_open(APM_S3_G(db_file), &db)) {
		/* Closing DB file and returning false */
		sqlite3_close(db);
		RETURN_FALSE;
	}

	sql = sqlite3_mprintf("SELECT id, ts, duration, file FROM slow_request ORDER BY %d %s LIMIT %d OFFSET %d", order, asc ? "ASC" : "DESC", limit, offset);
	sqlite3_exec(db, sql, slow_request_callback, &odd_slow_request, NULL);

	sqlite3_free(sql);
	sqlite3_close(db);
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto int apm_get_sqlite_events_count()
   Return the number of events */
PHP_FUNCTION(apm_get_sqlite_events_count)
{
	long count;

	count = get_table_count("event");
	if (count == -1) {
		RETURN_FALSE;
	}
	RETURN_LONG(count);
}
/* }}} */

/* {{{ proto int apm_get_sqlite_events_count()
   Return the number of slow requests */
PHP_FUNCTION(apm_get_sqlite_slow_requests_count)
{
	long count;

	count = get_table_count("slow_request");
	if (count == -1) {
		RETURN_FALSE;
	}
	RETURN_LONG(count);
}
/* }}} */

/* {{{ proto array apm_get_sqlite_event_into(int eventID)
   Returns all information available on a request */
PHP_FUNCTION(apm_get_sqlite_event_info)
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
	if (sqlite3_open(APM_S3_G(db_file), &db)) {
		/* Closing DB file and returning false */
		sqlite3_close(db);
		RETURN_FALSE;
	}

	sql = sqlite3_mprintf("SELECT id, ts, type, file, line, message, backtrace, ip FROM event WHERE id = %d", id);
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
	add_assoc_long(return_value, "ip", info.ip);
}
/* }}} */

/* Function called for the row returned by event info query */
static int event_callback_event_info(void *info, int num_fields, char **fields, char **col_name)
{
	(*(apm_event_info *) info).ts = atoi(fields[1]);
	(*(apm_event_info *) info).file = estrdup(fields[3]);
	(*(apm_event_info *) info).line = atoi(fields[4]);
	(*(apm_event_info *) info).type = atoi(fields[2]);
	(*(apm_event_info *) info).message = estrdup(fields[5]);
	(*(apm_event_info *) info).stacktrace = estrdup(fields[6]);
	(*(apm_event_info *) info).ip = atoi(fields[7]);

	return 0;
}

/* Function called for every row returned by event query */
static int event_callback(void *data, int num_fields, char **fields, char **col_name)
{
	smart_str file = {0};
	smart_str msg = {0};
	zval zfile, zmsg;
	struct in_addr myaddr;
	unsigned long n;
	char datetime[20] = {0};
	time_t ts;
#ifdef HAVE_INET_PTON
    char ip_str[40];
#else
	char *ip_str;
#endif

	Z_TYPE(zfile) = IS_STRING;
	Z_STRVAL(zfile) = fields[3];
	Z_STRLEN(zfile) = strlen(fields[3]);

	Z_TYPE(zmsg) = IS_STRING;
	Z_STRVAL(zmsg) = fields[6];
	Z_STRLEN(zmsg) = strlen(fields[6]);

	php_json_encode(&file, &zfile TSRMLS_CC);
	php_json_encode(&msg, &zmsg TSRMLS_CC);

	smart_str_0(&file);
	smart_str_0(&msg);

	n = strtoul(fields[4], NULL, 0);

	myaddr.s_addr = htonl(n);

#ifdef HAVE_INET_PTON
    inet_ntop(AF_INET, &myaddr, ip_str, sizeof(ip_str));
#else
	ip_str = inet_ntoa(myaddr);
#endif

	ts = atoi(fields[1]);
	strftime(datetime, sizeof(datetime), "%Y-%m-%d %H:%M:%S", localtime(&ts));

	php_printf("{id:\"%s\", cell:[\"%s\", \"%s\", \"%s\", %s, \"%s\", \"%s\", %s]},\n",
               fields[0], fields[0], datetime, fields[2], file.c, fields[5], ip_str, msg.c);

	smart_str_free(&file);
	smart_str_free(&msg);

	return 0;
}

/* Function called for every row returned by slow request query */
static int slow_request_callback(void *data, int num_fields, char **fields, char **col_name)
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

/* Returns the number of rows of a table */
static long get_table_count(char * table)
{
	sqlite3 *db;
	char *sql;
	long count;

	/* Opening the sqlite database file */
	if (sqlite3_open(APM_S3_G(db_file), &db)) {
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
