/*
 +----------------------------------------------------------------------+
 |  APM stands for Alternative PHP Monitor                              |
 +----------------------------------------------------------------------+
 | Copyright (c) 2008-2014  Davide Mendolia, Patrick Allaert            |
 +----------------------------------------------------------------------+
 | This source file is subject to version 3.01 of the PHP license,      |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.php.net/license/3_01.txt                                  |
 | If you did not receive a copy of the PHP license and are unable to   |
 | obtain it through the world-wide-web, please send a note to          |
 | license@php.net so we can mail you a copy immediately.               |
 +----------------------------------------------------------------------+
 | Authors: Patrick Allaert <patrickallaert@php.net>                    |
 +----------------------------------------------------------------------+
*/

#include <sqlite3.h>
#include <time.h>
#include "php_apm.h"
#include "php_ini.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/php_filestat.h"
#include "ext/json/php_json.h"
#include "driver_sqlite3.h"
#ifdef NETWARE
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

ZEND_EXTERN_MODULE_GLOBALS(apm);

APM_DRIVER_CREATE(sqlite3);

static void disconnect(TSRMLS_D)
{
	if (APM_G(sqlite3_event_db) != NULL) {
		sqlite3_close(APM_G(sqlite3_event_db));
		APM_G(sqlite3_event_db) = NULL;
	}
}

static int perform_db_access_checks(const char *path TSRMLS_DC)
{
// php_stat() crashes with ZTS, see later
#ifndef ZTS
	zend_bool is_dir;
	zval *stat;

	MAKE_STD_ZVAL(stat);
	php_stat(path, strlen(path), FS_IS_DIR, stat TSRMLS_CC);

	is_dir = Z_BVAL_P(stat);
	zval_dtor(stat);
	FREE_ZVAL(stat);

	/* Does db_path exists ? */
	if (!is_dir && !php_stream_mkdir((char *)path, 0777, PHP_STREAM_MKDIR_RECURSIVE, NULL)) {
		zend_error(E_CORE_WARNING, "APM cannot be enabled, '%s' is not a directory or it cannot be created", path);
		return FAILURE;
	}

	if (VCWD_ACCESS(path, W_OK | R_OK | X_OK) != SUCCESS) {
		zend_error(E_CORE_WARNING, "APM cannot be enabled, %s needs to be readable, writable and executable", path);
		return FAILURE;
	}
#endif

	return SUCCESS;
}

PHP_INI_MH(OnUpdateDBFile)
{
	if (new_value && new_value_length > 0) {
		snprintf(APM_G(sqlite3_db_file), MAXPATHLEN, "%s/%s", new_value, DB_FILE);
		disconnect(TSRMLS_C);

		if (perform_db_access_checks(new_value TSRMLS_CC) == FAILURE) {
			APM_G(sqlite3_enabled) = 0;
		}
	} else {
		APM_G(sqlite3_enabled) = 0;
	}
	return OnUpdateString(entry, new_value, new_value_length, mh_arg1, mh_arg2, mh_arg3, stage TSRMLS_CC);
}

/* Returns the SQLite instance (singleton) */
sqlite3 * sqlite_get_instance(TSRMLS_D) {
	int code;

	if (APM_G(sqlite3_event_db) == NULL) {
		/* Opening the sqlite database file */
		APM_DEBUG("[SQLite driver] Connecting to db...");
		if (sqlite3_open(APM_G(sqlite3_db_file), &APM_G(sqlite3_event_db))) {
			APM_DEBUG("FAILED!\n");
			/*
			 Closing DB file and stop loading the extension
			 in case of error while opening the database file
			 */
			disconnect(TSRMLS_C);
			return NULL;
		}
		APM_DEBUG("OK\n");

		sqlite3_busy_timeout(APM_G(sqlite3_event_db), APM_G(sqlite3_timeout));

		/* Making the connection asynchronous, not waiting for data being really written to the disk */
		sqlite3_exec(APM_G(sqlite3_event_db), "PRAGMA synchronous = OFF", NULL, NULL, NULL);

		APM_DEBUG("[SQLite driver] Setting up database\n");

		if ((code = sqlite3_exec(
			APM_G(sqlite3_event_db),
			"\n\
CREATE TABLE IF NOT EXISTS request (\n\
    id INTEGER PRIMARY KEY AUTOINCREMENT,\n\
    application TEXT NOT NULL,\n\
    ts INTEGER NOT NULL,\n\
    script TEXT NOT NULL,\n\
    uri TEXT NOT NULL,\n\
    host TEXT NOT NULL,\n\
    ip INTEGER UNSIGNED NOT NULL,\n\
    cookies TEXT NOT NULL,\n\
    post_vars TEXT NOT NULL,\n\
    referer TEXT NOT NULL\n\
);\n\
CREATE TABLE IF NOT EXISTS event (\n\
    id INTEGER PRIMARY KEY AUTOINCREMENT,\n\
    request_id INTEGER,\n\
    ts INTEGER NOT NULL,\n\
    type INTEGER NOT NULL,\n\
    file TEXT NOT NULL,\n\
    line INTEGER NOT NULL,\n\
    message TEXT NOT NULL,\n\
    backtrace BLOB NOT NULL\n\
);\n\
CREATE INDEX IF NOT EXISTS event_request ON event (request_id);\n\
CREATE TABLE IF NOT EXISTS stats (\n\
    id INTEGER PRIMARY KEY AUTOINCREMENT,\n\
    request_id INTEGER,\n\
    duration FLOAT UNSIGNED NOT NULL,\n\
    user_cpu FLOAT UNSIGNED NOT NULL,\n\
    sys_cpu FLOAT UNSIGNED NOT NULL,\n\
    mem_peak_usage INTEGER UNSIGNED NOT NULL\n\
);\n\
CREATE INDEX IF NOT EXISTS stats_request ON stats (request_id);",
			NULL, NULL, NULL)) != SQLITE_OK) {
			zend_error(E_CORE_WARNING, "APM's schema cannot be created, error code: %d", code);
		}
	}

	return APM_G(sqlite3_event_db);
}

/* Insert a request in the backend */
void apm_driver_sqlite3_insert_request(TSRMLS_D)
{
	char *sql, *script;
	int ip_int = 0, code;
	struct in_addr ip_addr;
	sqlite3 *connection;
	zval *tmp;

	EXTRACT_DATA();

	APM_DEBUG("[SQLite driver] Begin insert request\n");
	if (APM_G(sqlite3_is_request_created)) {
		APM_DEBUG("[SQLite driver] SKIPPED, request already created.\n");
		return;
	}

	SQLITE_INSTANCE_INIT

	get_script(&script TSRMLS_CC);

	if (APM_RD(ip_found) && (inet_pton(AF_INET, Z_STRVAL_PP(APM_RD(ip)), &ip_addr) == 1)) {
		ip_int = ntohl(ip_addr.s_addr);
	}

	/* Builing SQL insert query */
	sql = sqlite3_mprintf(
		"INSERT INTO request (application, ts, script, uri, host, ip, cookies, post_vars, referer) VALUES (%Q, %d, %Q, %Q, %Q, %d, %Q, %Q, %Q)",
		APM_G(application_id) ? APM_G(application_id) : "", (long)time(NULL), script ? script : "", APM_RD(uri_found) ? Z_STRVAL_PP(APM_RD(uri)) : "", APM_RD(host_found) ? Z_STRVAL_PP(APM_RD(host)) : "", ip_int, APM_RD(cookies_found) ? APM_RD(cookies).c : "", APM_RD(post_vars_found) ? APM_RD(post_vars).c : "", APM_RD(referer_found) ? Z_STRVAL_PP(APM_RD(referer)) : "");
	/* Executing SQL insert query */
	APM_DEBUG("[SQLite driver] Sending: %s\n", sql);
	if ((code = sqlite3_exec(connection, sql, NULL, NULL, NULL)) != SQLITE_OK)
		APM_DEBUG("[SQLite driver] Error occured with previous query. Error code: %d\n", code);

	sqlite3_free(sql);
	APM_G(sqlite3_request_id) = sqlite3_last_insert_rowid(connection);
	APM_G(sqlite3_is_request_created) = 1;
	APM_DEBUG("[SQLite driver] End insert request\n");
}

/* Insert an event in the backend */
void apm_driver_sqlite3_process_event(PROCESS_EVENT_ARGS)
{
	char *sql;
	sqlite3 *connection;

	apm_driver_sqlite3_insert_request(TSRMLS_C);

	SQLITE_INSTANCE_INIT

	/* Builing SQL insert query */
	sql = sqlite3_mprintf(
		"INSERT INTO event (request_id, ts, type, file, line, message, backtrace) VALUES (%d, %d, %d, %Q, %d, %Q, %Q)",
		APM_G(sqlite3_request_id), (long)time(NULL), type, error_filename ? error_filename : "", error_lineno, msg ? msg : "", trace ? trace : ""
	);
	/* Executing SQL insert query */
	APM_DEBUG("[SQLite driver] Sending: %s\n", sql);
	if (sqlite3_exec(connection, sql, NULL, NULL, NULL) != SQLITE_OK)
		APM_DEBUG("[SQLite driver] Error occured with previous query\n");

	sqlite3_free(sql);
}

int apm_driver_sqlite3_minit(int module_number TSRMLS_DC)
{
	return SUCCESS;
}

int apm_driver_sqlite3_rinit(TSRMLS_D)
{
	APM_G(sqlite3_is_request_created) = 0;
	return SUCCESS;
}

int apm_driver_sqlite3_mshutdown(SHUTDOWN_FUNC_ARGS)
{
	return SUCCESS;
}

int apm_driver_sqlite3_rshutdown(TSRMLS_D)
{
	disconnect(TSRMLS_C);
	return SUCCESS;
}

void apm_driver_sqlite3_process_stats(TSRMLS_D)
{
	char *sql;
	sqlite3 *connection;

	apm_driver_sqlite3_insert_request(TSRMLS_C);

	SQLITE_INSTANCE_INIT

	/* Building SQL insert query */
	sql = sqlite3_mprintf(
		"INSERT INTO stats (request_id, duration, user_cpu, sys_cpu, mem_peak_usage) VALUES (%d, %f, %f, %f, %d)",
		APM_G(sqlite3_request_id), USEC_TO_SEC(APM_G(duration)), USEC_TO_SEC(APM_G(user_cpu)), USEC_TO_SEC(APM_G(sys_cpu)), APM_G(mem_peak_usage)
	);

	/* Executing SQL insert query */
	APM_DEBUG("[SQLite driver] Sending: %s\n", sql);
	if (sqlite3_exec(connection, sql, NULL, NULL, NULL) != SQLITE_OK)
		APM_DEBUG("[SQLite driver] Error occured with previous query\n");

	sqlite3_free(sql);
}
