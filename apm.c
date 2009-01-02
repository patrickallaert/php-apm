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
#include "php_apm.h"

#define DB_FILE "/events"
#define SEC_TO_USEC(sec) ((sec) * 1000000.00)
#define USEC_TO_SEC(usec) ((usec) / 1000000.00)

ZEND_API void (*old_error_cb)(int type, const char *error_filename,
                              const uint error_lineno, const char *format,
                              va_list args);
void apm_error_cb(int type, const char *error_filename, 
                  const uint error_lineno, const char *format,
                  va_list args);
int callback(void *, int, char **, char **);
int callback_slow_request(void *, int, char **, char **);

sqlite3 *event_db;
char *db_file;

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
	/* Boolean controlling whether the monitoring is active or not */
	STD_PHP_INI_BOOLEAN("apm.enabled",                "1",                      PHP_INI_ALL, OnUpdateBool,   enabled,  zend_apm_globals, apm_globals)
	/* Path to the SQLite database file */
	STD_PHP_INI_ENTRY("apm.max_event_insert_timeout", "100",                    PHP_INI_ALL, OnUpdateLong,   timeout,  zend_apm_globals, apm_globals)
	/* Max timeout to wait for storing the event in the DB */
	STD_PHP_INI_ENTRY("apm.db_path",                  "/var/php/apm/db",        PHP_INI_ALL, OnUpdateString, db_path,  zend_apm_globals, apm_globals)
PHP_INI_END()
 
static void apm_init_globals(zend_apm_globals *apm_globals)
{
}

PHP_MINIT_FUNCTION(apm)
{
	sqlite3 *db;
	int rc;

	ZEND_INIT_MODULE_GLOBALS(apm, apm_init_globals, NULL);
	REGISTER_INI_ENTRIES();

	if (APM_G(enabled)) {
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

		/* Defining full path to db file */
		db_file = (char *) malloc((strlen(APM_G(db_path)) + strlen(DB_FILE) + 1) * sizeof(char));

		strcpy(db_file, APM_G(db_path));
		strcat(db_file, DB_FILE);

		/* Opening the sqlite database file */
		rc = sqlite3_open(db_file, &db);
		if (rc) {
			/*
			 Closing DB file and stop loading the extension
			 in case of error while opening the database file
			 */
			sqlite3_close(db);
			zend_error(E_CORE_WARNING, "APM cannot be loaded, %s cannot be opened/created", db_file);
			return FAILURE;
		}

		/* Executing SQL creation table query */
		sqlite3_exec(
			db,
			"CREATE TABLE IF NOT EXISTS event ( \
			    id INTEGER PRIMARY KEY AUTOINCREMENT, \
			    ts TEXT NOT NULL, \
			    type INTEGER NOT NULL, \
			    file TEXT NOT NULL, \
			    line INTEGER NOT NULL, \
			    message TEXT NOT NULL)",
			NULL, NULL, NULL);
		sqlite3_exec(
			db,
			"CREATE TABLE IF NOT EXISTS slow_request ( \
			    id INTEGER PRIMARY KEY AUTOINCREMENT, \
			    ts TEXT NOT NULL, \
			    duration FLOAT NOT NULL, \
			    file TEXT NOT NULL)",
			NULL, NULL, NULL);
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
		int rc;
		struct timezone begin_tz;
		
		/* storing timestamp of request */
		gettimeofday(&begin_tp, &begin_tz);
		/* Opening the sqlite database file */
		rc = sqlite3_open(db_file, &event_db);
		sqlite3_busy_timeout(event_db, APM_G(timeout));
		if (rc) {
			/*
			 Closing DB file and stop loading the extension
			 in case of error while opening the database file
			 */
			sqlite3_close(event_db);
			return FAILURE;
		}

		/* Making the connection asynchronous, not waiting for data being really written to the disk */
		sqlite3_exec(event_db, "PRAGMA synchronous = OFF", NULL, NULL, NULL);

		/* Replacing current error callback function with apm's one */
		zend_error_cb = apm_error_cb;
	}
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(apm)
{
	float duration;
	struct timeval end_tp;
	struct timezone end_tz;

	gettimeofday(&end_tp, &end_tz);

	/* Request longer than accepted thresold ? */
	duration = SEC_TO_USEC(end_tp.tv_sec - begin_tp.tv_sec) + end_tp.tv_usec - begin_tp.tv_usec;
	if (duration > 1000000.00) {
		zval **array;
		zval **token;
		char *script_filename = NULL;
		char *sql;

		if (zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &array) == SUCCESS &&
			Z_TYPE_PP(array) == IS_ARRAY &&
			zend_hash_find(Z_ARRVAL_PP(array), "SCRIPT_FILENAME", sizeof("SCRIPT_FILENAME"), (void **) &token) == SUCCESS) {
			script_filename = Z_STRVAL_PP(token);
		}

		/* Building SQL insert query */
		sql = sqlite3_mprintf("INSERT INTO slow_request (ts, duration, file) VALUES (datetime(), %f, %Q);",
		                      USEC_TO_SEC(duration), script_filename);

		/* Executing SQL insert query */
		sqlite3_exec(event_db, sql, NULL, NULL, NULL);
	}
	/* Restoring saved error callback function */
	zend_error_cb = old_error_cb;

	return SUCCESS;
}

PHP_MINFO_FUNCTION(apm)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "APM support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

/* {{{ void apm_error(int type, const char *format, ...)
 *    This function provides a hook for error */
void apm_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
	char *msg, *sql;
	va_list args_copy;

	/* A copy of args is needed to be used for the old_error_cb */
	va_copy(args_copy, args);
	vspprintf(&msg, 0, format, args_copy);

	/* Builing SQL insert query */
	sql = sqlite3_mprintf("INSERT INTO event (ts, type, file, line, message) VALUES (datetime(), %d, %Q, %d, %Q);",
	                      type, error_filename, error_lineno, msg);
	/* Executing SQL insert query */
	sqlite3_exec(event_db, sql, NULL, NULL, NULL);
	efree(msg);

	/* Calling saved callback function for error handling */
	old_error_cb(type, error_filename, error_lineno, format, args);
}
/* }}} */

/* Return an HTML table with all events */
PHP_FUNCTION(apm_get_events)
{
	sqlite3 *db;
	int rc;
	/* Opening the sqlite database file */
	rc = sqlite3_open(db_file, &db);
	if (rc) {
		/* Closing DB file and returning false */
		sqlite3_close(db);
		RETURN_FALSE;
	}

	/* Results are printed in an HTML table */
	php_printf("<table id=\"event-list\"><tr><th>#</th><th>Time</th><th>Type</th><th>File</th><th>Line</th><th>Message</th></tr>\n");
	sqlite3_exec(db, "SELECT id, ts, type, file, line, message FROM event", callback, NULL, NULL);
	php_printf("</table>");

	RETURN_TRUE;
}

/* Return an HTML table with all events */
PHP_FUNCTION(apm_get_slow_requests)
{
	sqlite3 *db;
	int rc;
	/* Opening the sqlite database file */
	rc = sqlite3_open(db_file, &db);
	if (rc) {
		/* Closing DB file and returning false */
		sqlite3_close(db);
		RETURN_FALSE;
	}

	/* Results are printed in an HTML table */
	php_printf("<table id=\"slow-request-list\"><tr><th>#</th><th>Time</th><th>Duration</th><th>File</th></tr>\n");
	sqlite3_exec(db, "SELECT id, ts, duration, file FROM slow_request", callback_slow_request, NULL, NULL);
	php_printf("</table>");

	RETURN_TRUE;
}

/* Function called for every row returned by event query */
int callback(void *data, int num_fields, char **fields, char **col_name)
{
	php_printf("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
                   fields[0], fields[1], fields[2], fields[3], fields[4], fields[5]);

	return 0;
}

/* Function called for every row returned by slow request query */
int callback_slow_request(void *data, int num_fields, char **fields, char **col_name)
{
	php_printf("<tr><td>%s</td><td>%s</td><td>%s</td><td>%s</td></tr>\n",
                   fields[0], fields[1], fields[2], fields[3]);

	return 0;
}

