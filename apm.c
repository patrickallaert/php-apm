/*
    APM stands for Alternative PHP Monitor
    Copyright (C) 2008  Davide Mendolia, Patrick Allaert

    This file is part of APM.

    APM is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    APM is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with APM.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/time.h>
#include <sqlite3.h>
#include <sys/stat.h>
#include <unistd.h>
#include "php.h"
#include "php_ini.h"
#include "zend_exceptions.h"
#include "php_apm.h"
#include "ext/standard/info.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/php_var.h"
#include "Zend/zend_builtin_functions.h"

#define DB_FILE "/events"
#define SEC_TO_USEC(sec) ((sec) * 1000000.00)
#define USEC_TO_SEC(usec) ((usec) / 1000000.00)

ZEND_API void (*old_error_cb)(int type, const char *error_filename,
                              const uint error_lineno, const char *format,
                              va_list args);
void apm_error_cb(int type, const char *error_filename, 
                  const uint error_lineno, const char *format,
                  va_list args);

void apm_throw_exception_hook(zval *exception TSRMLS_DC);

static int callback(void *, int, char **, char **);
static int callback_slow_request(void *, int, char **, char **);
static int perform_db_access_checks();

sqlite3 *event_db;
char *db_file;
static int odd_event_list = 1;
static int odd_slow_request = 1;

/* recorded timestamp for the request */
struct timeval begin_tp;

function_entry apm_functions[] = {
        PHP_FE(apm_get_events, NULL)
        PHP_FE(apm_get_slow_requests, NULL)
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

PHP_INI_BEGIN()
	/* Boolean controlling whether the extension is globally active or not */
	STD_PHP_INI_BOOLEAN("apm.enabled",                "1",                      PHP_INI_ALL, OnUpdateBool,   enabled,                zend_apm_globals, apm_globals)
	/* Boolean controlling whether the event monitoring is active or not */
	STD_PHP_INI_BOOLEAN("apm.event_enabled",          "1",                      PHP_INI_ALL, OnUpdateBool,   event_enabled,          zend_apm_globals, apm_globals)
	/* Boolean controlling whether the slow request monitoring is active or not */
	STD_PHP_INI_BOOLEAN("apm.slow_request_enabled",   "1",                      PHP_INI_ALL, OnUpdateBool,   slow_request_enabled,   zend_apm_globals, apm_globals)
	/* Path to the SQLite database file */
	STD_PHP_INI_ENTRY("apm.max_event_insert_timeout", "100",                    PHP_INI_ALL, OnUpdateLong,   timeout,                zend_apm_globals, apm_globals)
	/* Max timeout to wait for storing the event in the DB */
	STD_PHP_INI_ENTRY("apm.db_path",                  "/var/php/apm/db",        PHP_INI_ALL, OnUpdateString, db_path,                zend_apm_globals, apm_globals)
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

	if (APM_G(enabled)) {
		sqlite3 *db;

		if (perform_db_access_checks() == FAILURE) {
			return FAILURE;
		}

		/* Defining full path to db file */
		db_file = (char *) malloc((strlen(APM_G(db_path)) + strlen(DB_FILE) + 1) * sizeof(char));

		strcpy(db_file, APM_G(db_path));
		strcat(db_file, DB_FILE);

		/* Opening the sqlite database file */
		if (sqlite3_open(db_file, &db)) {
			/*
			 Closing DB file and stop loading the extension
			 in case of error while opening the database file
			 */
			sqlite3_close(db);
			zend_error(E_CORE_WARNING, "APM cannot be loaded, %s cannot be opened/created", db_file);
			return FAILURE;
		}

		/* Executing SQL creation table query */
		if (APM_G(event_enabled)) {
			sqlite3_exec(
				db,
				"CREATE TABLE IF NOT EXISTS event ( \
				    id INTEGER PRIMARY KEY AUTOINCREMENT, \
				    ts TEXT NOT NULL, \
				    type INTEGER NOT NULL, \
				    file TEXT NOT NULL, \
				    line INTEGER NOT NULL, \
				    message TEXT NOT NULL, \
				    backtrace TEXT NOT NULL)",
				NULL, NULL, NULL);
		}
		if (APM_G(slow_request_enabled)) {
			sqlite3_exec(
				db,
				"CREATE TABLE IF NOT EXISTS slow_request ( \
				    id INTEGER PRIMARY KEY AUTOINCREMENT, \
				    ts TEXT NOT NULL, \
				    duration FLOAT NOT NULL, \
				    file TEXT NOT NULL)",
				NULL, NULL, NULL);
		}
		sqlite3_close(db);
	}

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
			struct timezone begin_tz;
			
			/* storing timestamp of request */
			gettimeofday(&begin_tp, &begin_tz);
		}
		/* Opening the sqlite database file */
		if (sqlite3_open(db_file, &event_db)) {
			/*
			 Closing DB file and stop loading the extension
			 in case of error while opening the database file
			 */
			sqlite3_close(event_db);
			return FAILURE;
		}

		sqlite3_busy_timeout(event_db, APM_G(timeout));

		/* Making the connection asynchronous, not waiting for data being really written to the disk */
		sqlite3_exec(event_db, "PRAGMA synchronous = OFF", NULL, NULL, NULL);

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
		struct timezone end_tz;

		gettimeofday(&end_tp, &end_tz);

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
			sqlite3_exec(event_db, sql, NULL, NULL, NULL);

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
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

/* {{{ void apm_error(int type, const char *format, ...)
 *    This function provides a hook for error */
void apm_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
	if (APM_G(event_enabled)) {
		char *msg, *sql, *backtrace;
		va_list args_copy;

		/* A copy of args is needed to be used for the old_error_cb */
		va_copy(args_copy, args);
		vspprintf(&msg, 0, format, args_copy);

		/* We need to see if we have an uncaught exception fatal error now */
		if (type == E_ERROR && strncmp(msg, "Uncaught exception", 18) == 0) {

		} else {
			/*Fetch the stacktrace and serialize it before storing it*/
			zval *return_value;
			zend_bool provide_object = 1;
			MAKE_STD_ZVAL(return_value);
			zend_fetch_debug_backtrace(return_value, 1, provide_object TSRMLS_CC);

			smart_str buf = {0};
			php_serialize_data_t var_hash;
			PHP_VAR_SERIALIZE_INIT(var_hash);
			php_var_serialize(&buf, &return_value, &var_hash TSRMLS_CC);
			PHP_VAR_SERIALIZE_DESTROY(var_hash);
			backtrace = buf.c;

			/* Builing SQL insert query */
			sql = sqlite3_mprintf("INSERT INTO event (ts, type, file, line, message, backtrace) VALUES (datetime(), %d, %Q, %d, %Q, %Q);",
				                  type, error_filename, error_lineno, msg, backtrace);
			/* Executing SQL insert query */
			sqlite3_exec(event_db, sql, NULL, NULL, NULL);

			smart_str_free(&buf);
			sqlite3_free(sql);
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
		char *sql;
		zval *message, *file, *line;
		zend_class_entry *default_ce, *exception_ce;
		char *exception_trace;

		if (!exception) {
			return;
		}

		default_ce = zend_exception_get_default(TSRMLS_C);
		exception_ce = zend_get_class_entry(exception TSRMLS_CC);

		message = zend_read_property(default_ce, exception, "message", sizeof("message")-1, 0 TSRMLS_CC);
		file =    zend_read_property(default_ce, exception, "file",    sizeof("file")-1,    0 TSRMLS_CC);
		line =    zend_read_property(default_ce, exception, "line",    sizeof("line")-1,    0 TSRMLS_CC);

		zval *return_value;
		zend_bool provide_object = 1;
		MAKE_STD_ZVAL(return_value);
		zend_fetch_debug_backtrace(return_value, 1, provide_object TSRMLS_CC);

		smart_str buf = {0};
		php_serialize_data_t var_hash;
		PHP_VAR_SERIALIZE_INIT(var_hash);
		php_var_serialize(&buf, &return_value, &var_hash TSRMLS_CC);
		PHP_VAR_SERIALIZE_DESTROY(var_hash);

		/* Builing SQL insert query */
		sql = sqlite3_mprintf("INSERT INTO event (ts, type, file, line, message, backtrace) VALUES (datetime(), %d, %Q, %d, %Q, %Q);",
			                  E_ERROR, Z_STRVAL_P(file), Z_LVAL_P(line), Z_STRVAL_P(message), buf.c);
		/* Executing SQL insert query */
		sqlite3_exec(event_db, sql, NULL, NULL, NULL);

		smart_str_free(&buf);
		sqlite3_free(sql);
	}
}


/* Return an HTML table with all events */
PHP_FUNCTION(apm_get_events)
{
	sqlite3 *db;
	/* Opening the sqlite database file */
	if (sqlite3_open(db_file, &db)) {
		/* Closing DB file and returning false */
		sqlite3_close(db);
		RETURN_FALSE;
	}

	/* Results are printed in an HTML table */
	odd_event_list = 1;
	php_printf("<table id=\"event-list\"><tr><th>#</th><th>Time</th><th>Type</th><th>File</th><th>Line</th><th>Message</th><th>Backtrace</th></tr>\n");
	sqlite3_exec(db, "SELECT id, ts, CASE type \
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
                          file, line, message, backtrace FROM event", callback, NULL, NULL);
	php_printf("</table>");

	sqlite3_close(db);
	RETURN_TRUE;
}

/* Return an HTML table with all events */
PHP_FUNCTION(apm_get_slow_requests)
{
	sqlite3 *db;
	/* Opening the sqlite database file */
	if (sqlite3_open(db_file, &db)) {
		/* Closing DB file and returning false */
		sqlite3_close(db);
		RETURN_FALSE;
	}

	/* Results are printed in an HTML table */
	odd_slow_request = 1;
	php_printf("<table id=\"slow-request-list\"><tr><th>#</th><th>Time</th><th>Duration</th><th>File</th></tr>\n");
	sqlite3_exec(db, "SELECT id, ts, duration, file FROM slow_request", callback_slow_request, NULL, NULL);
	php_printf("</table>");

	sqlite3_close(db);
	RETURN_TRUE;
}

/* Function called for every row returned by event query */
static int callback(void *data, int num_fields, char **fields, char **col_name)
{
	php_printf("<tr class=\"%s %s\"><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
                   fields[2], odd_event_list ? "odd" : "even", fields[0], fields[1], fields[2], fields[3], fields[4], fields[5], fields[6]);
	odd_event_list = !odd_event_list;

	return 0;
}

/* Function called for every row returned by slow request query */
static int callback_slow_request(void *data, int num_fields, char **fields, char **col_name)
{
	php_printf("<tr class=\"%s\"><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
                   odd_slow_request ? "odd" : "even", fields[0], fields[1], fields[2], fields[3]);
	odd_slow_request = !odd_slow_request;

	return 0;
}

/* Perform access checks on the DB path */
static int perform_db_access_checks()
{
	struct stat db_path_stat;

	/* Does db_path exists ? */
	if (stat(APM_G(db_path), &db_path_stat) != 0) {
		zend_error(E_CORE_WARNING, "APM cannot be loaded, an error occured while accessing %s", APM_G(db_path));
		return FAILURE;
	}

	/* Is this a directory ? */
	if (! S_ISDIR(db_path_stat.st_mode)) {
		zend_error(E_CORE_WARNING, "APM cannot be loaded, %s should be a directory", APM_G(db_path));
		return FAILURE;
	}

	/* Does it have the correct permissions ? */
	if (access(APM_G(db_path), R_OK | W_OK | X_OK) != 0) {
		zend_error(E_CORE_WARNING, "APM cannot be loaded, %s should be readable, writable and executable", APM_G(db_path));
		return FAILURE;
	}
	return SUCCESS;
}
