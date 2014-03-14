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

#include <mysql/mysql.h>
#include "php_apm.h"
#include "php_ini.h"
#include "driver_mysql.h"
#include "ext/standard/php_smart_str.h"
#include "ext/json/php_json.h"
#ifdef NETWARE
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

static long get_table_count(char * table);

ZEND_EXTERN_MODULE_GLOBALS(apm)

ZEND_DECLARE_MODULE_GLOBALS(apm_mysql)

APM_DRIVER_CREATE(mysql)

PHP_INI_BEGIN()
 	/* Boolean controlling whether the driver is active or not */
 	STD_PHP_INI_BOOLEAN("apm.mysql_enabled",       "1",               PHP_INI_PERDIR, OnUpdateBool,   enabled,             zend_apm_mysql_globals, apm_mysql_globals)
	/* Boolean controlling the collection of stats */
	STD_PHP_INI_BOOLEAN("apm.mysql_stats_enabled", "0",               PHP_INI_ALL, OnUpdateBool,   stats_enabled,       zend_apm_mysql_globals, apm_mysql_globals)
	/* Control which exceptions to collect (0: none exceptions collected, 1: collect uncaught exceptions (default), 2: collect ALL exceptions) */
	STD_PHP_INI_ENTRY("apm.mysql_exception_mode","1",               PHP_INI_PERDIR, OnUpdateLongGEZero,   exception_mode,      zend_apm_mysql_globals, apm_mysql_globals)
	/* error_reporting of the driver */
	STD_PHP_INI_ENTRY("apm.mysql_error_reporting", NULL,              PHP_INI_ALL,    OnUpdateAPMmysqlErrorReporting,   error_reporting,     zend_apm_mysql_globals, apm_mysql_globals)
	/* mysql host */
	STD_PHP_INI_ENTRY("apm.mysql_host",            "localhost",       PHP_INI_PERDIR, OnUpdateString, db_host,             zend_apm_mysql_globals, apm_mysql_globals)
	/* mysql port */
	STD_PHP_INI_ENTRY("apm.mysql_port",            "0",               PHP_INI_PERDIR, OnUpdateLong,   db_port,             zend_apm_mysql_globals, apm_mysql_globals)
	/* mysql user */
	STD_PHP_INI_ENTRY("apm.mysql_user",            "root",            PHP_INI_PERDIR, OnUpdateString, db_user,             zend_apm_mysql_globals, apm_mysql_globals)
	/* mysql password */
	STD_PHP_INI_ENTRY("apm.mysql_pass",            "",                PHP_INI_PERDIR, OnUpdateString, db_pass,             zend_apm_mysql_globals, apm_mysql_globals)
	/* mysql database */
	STD_PHP_INI_ENTRY("apm.mysql_db",              "apm",             PHP_INI_PERDIR, OnUpdateString, db_name,             zend_apm_mysql_globals, apm_mysql_globals)
PHP_INI_END()

/* Returns the MYSQL instance (singleton) */
MYSQL * mysql_get_instance() {
	my_bool reconnect = 1;
	if (APM_MY_G(event_db) == NULL) {
		APM_MY_G(event_db) = malloc(sizeof(MYSQL));
		mysql_init(APM_MY_G(event_db));
		mysql_options(APM_MY_G(event_db), MYSQL_OPT_RECONNECT, &reconnect);
		APM_DEBUG("[MySQL driver] Connecting to server...");
		if (mysql_real_connect(APM_MY_G(event_db), APM_MY_G(db_host), APM_MY_G(db_user), APM_MY_G(db_pass), APM_MY_G(db_name), APM_MY_G(db_port), NULL, 0) == NULL) {
			APM_DEBUG("FAILED! Message: %s\n", mysql_error(APM_MY_G(event_db)));

			free(APM_MY_G(event_db));
			APM_MY_G(event_db) = NULL;
			return NULL;
		}
		APM_DEBUG("OK\n");
		mysql_set_character_set(APM_MY_G(event_db), "utf8");

		mysql_query(
			APM_MY_G(event_db),
			"\
CREATE TABLE IF NOT EXISTS request (\
    id INTEGER UNSIGNED PRIMARY KEY auto_increment,\
    ts TIMESTAMP NOT NULL,\
    script TEXT NOT NULL,\
    uri TEXT NOT NULL,\
    host TEXT NOT NULL,\
    ip INTEGER UNSIGNED NOT NULL,\
    cookies TEXT NOT NULL,\
    post_vars TEXT NOT NULL,\
    referer TEXT NOT NULL\
)"
	);
		mysql_query(
			APM_MY_G(event_db),
			"\
CREATE TABLE IF NOT EXISTS event (\
    id INTEGER UNSIGNED PRIMARY KEY auto_increment,\
    request_id INTEGER UNSIGNED,\
    ts TIMESTAMP NOT NULL,\
    type TINYINT UNSIGNED NOT NULL,\
    file TEXT NOT NULL,\
    line MEDIUMINT UNSIGNED NOT NULL,\
    message TEXT NOT NULL,\
    backtrace BLOB NOT NULL,\
    KEY request (request_id)\
)"
	);

		mysql_query(
			APM_MY_G(event_db),
			"\
CREATE TABLE IF NOT EXISTS stats (\
    id INTEGER UNSIGNED PRIMARY KEY auto_increment,\
    request_id INTEGER UNSIGNED,\
    duration FLOAT UNSIGNED NOT NULL,\
    user_cpu FLOAT UNSIGNED NOT NULL,\
    sys_cpu FLOAT UNSIGNED NOT NULL,\
    mem_peak_usage INTEGER UNSIGNED NOT NULL,\
    KEY request (request_id)\
)"
		);
	}

	return APM_MY_G(event_db);
}

/* Insert a request in the backend */
void apm_driver_mysql_insert_request(TSRMLS_D)
{
	char *script = NULL, *script_esc = NULL, *uri_esc = NULL, *host_esc = NULL, *cookies_esc = NULL, *post_vars_esc = NULL, *referer_esc = NULL, *sql = NULL;
	int script_len = 0, uri_len = 0, host_len = 0, ip_int = 0, cookies_len = 0, post_vars_len = 0, referer_len = 0;
	struct in_addr ip_addr;
	MYSQL *connection;
	zval *tmp;

	EXTRACT_DATA();

	APM_DEBUG("[MySQL driver] Begin insert request\n", sql);
	if (APM_MY_G(is_request_created)) {
		APM_DEBUG("[MySQL driver] SKIPPED, request already created.\n", sql);
		return;
	}

	MYSQL_INSTANCE_INIT

	get_script(&script);

	if (script) {
		script_len = strlen(script);
		script_esc = emalloc(script_len * 2 + 1);
		script_len = mysql_real_escape_string(connection, script_esc, script, script_len);
	}

	if (APM_RD(uri_found)) {
		uri_len = strlen(Z_STRVAL_PP(APM_RD(uri)));
		uri_esc = emalloc(uri_len * 2 + 1);
		uri_len = mysql_real_escape_string(connection, uri_esc, Z_STRVAL_PP(APM_RD(uri)), uri_len);
	}

	if (APM_RD(host_found)) {
		host_len = strlen(Z_STRVAL_PP(APM_RD(host)));
		host_esc = emalloc(host_len * 2 + 1);
		host_len = mysql_real_escape_string(connection, host_esc, Z_STRVAL_PP(APM_RD(host)), host_len);
	}

	if (APM_RD(ip_found) && (inet_pton(AF_INET, Z_STRVAL_PP(APM_RD(ip)), &ip_addr) == 1)) {
		ip_int = ntohl(ip_addr.s_addr);
	}
	
	if (APM_RD(cookies_found)) {
		cookies_len = strlen(APM_RD(cookies).c);
		cookies_esc = emalloc(cookies_len * 2 + 1);
		cookies_len = mysql_real_escape_string(connection, cookies_esc, APM_RD(cookies).c, cookies_len);
	}

	if (APM_RD(post_vars_found)) {
		post_vars_len = strlen(APM_RD(post_vars).c);
		post_vars_esc = emalloc(post_vars_len * 2 + 1);
		post_vars_len = mysql_real_escape_string(connection, post_vars_esc, APM_RD(post_vars).c, post_vars_len);
	}

	if (APM_RD(referer_found)) {
		referer_len = strlen(Z_STRVAL_PP(APM_RD(referer)));
		referer_esc = emalloc(referer_len * 2 + 1);
		referer_len = mysql_real_escape_string(connection, referer_esc, Z_STRVAL_PP(APM_RD(referer)), referer_len);
	}

	sql = emalloc(134 + script_len + uri_len + host_len + cookies_len + post_vars_len + referer_len);
	sprintf(
		sql,
		"INSERT INTO request (script, uri, host, ip, cookies, post_vars, referer) VALUES ('%s', '%s', '%s', %u, '%s', '%s', '%s')",
		script ? script_esc : "", APM_RD(uri_found) ? uri_esc : "", APM_RD(host_found) ? host_esc : "", ip_int, APM_RD(cookies_found) ? cookies_esc : "", APM_RD(post_vars_found) ? post_vars_esc : "", APM_RD(referer_found) ? referer_esc : "");

	APM_DEBUG("[MySQL driver] Sending: %s\n", sql);
	if (mysql_query(connection, sql) != 0)
		APM_DEBUG("[MySQL driver] Error: %s\n", mysql_error(APM_MY_G(event_db)));

	mysql_query(connection, "SET @request_id = LAST_INSERT_ID()");

	efree(sql);
	efree(script_esc);
	efree(uri_esc);
	efree(host_esc);
	efree(cookies_esc);
	efree(post_vars_esc);
	efree(referer_esc);

	APM_MY_G(is_request_created) = 1;
	APM_DEBUG("[MySQL driver] End insert request\n", sql);
}

/* Insert an event in the backend */
void apm_driver_mysql_insert_event(int type, char * error_filename, uint error_lineno, char * msg, char * trace  TSRMLS_DC)
{
	char *filename_esc = NULL, *msg_esc = NULL, *trace_esc = NULL, *sql = NULL;
	int filename_len = 0, msg_len = 0, trace_len = 0;
	MYSQL *connection;

	MYSQL_INSTANCE_INIT

	if (error_filename) {
		filename_len = strlen(error_filename);
		filename_esc = emalloc(filename_len * 2 + 1);
		filename_len = mysql_real_escape_string(connection, filename_esc, error_filename, filename_len);
	}

	if (msg) {
		msg_len = strlen(msg);
		msg_esc = emalloc(msg_len * 2 + 1);
		msg_len = mysql_real_escape_string(connection, msg_esc, msg, msg_len);
	}

	if (trace) {
		trace_len = strlen(trace);
		trace_esc = emalloc(trace_len * 2 + 1);
		trace_len = mysql_real_escape_string(connection, trace_esc, trace, trace_len);
	}

	sql = emalloc(135 + filename_len + msg_len + trace_len);
	sprintf(
		sql,
		"INSERT INTO event (request_id, type, file, line, message, backtrace) VALUES (@request_id, %d, '%s', %u, '%s', '%s')",
		type, error_filename ? filename_esc : "", error_lineno, msg ? msg_esc : "", trace ? trace_esc : "");

	APM_DEBUG("[MySQL driver] Sending: %s\n", sql);
	if (mysql_query(connection, sql) != 0)
		APM_DEBUG("[MySQL driver] Error: %s\n", mysql_error(APM_MY_G(event_db)));

	efree(sql);
	efree(filename_esc);
	efree(msg_esc);
	efree(trace_esc);
}

int apm_driver_mysql_minit(int module_number)
{
	REGISTER_INI_ENTRIES();
	return SUCCESS;
}

int apm_driver_mysql_rinit()
{
	APM_MY_G(is_request_created) = 0;
	return SUCCESS;
}

int apm_driver_mysql_mshutdown()
{
	if (APM_MY_G(event_db) != NULL) {
		APM_DEBUG("[MySQL driver] Closing connection\n");
		mysql_close(APM_MY_G(event_db));
		free(APM_MY_G(event_db));
		APM_MY_G(event_db) = NULL;
	}
	return SUCCESS;
}

int apm_driver_mysql_rshutdown()
{
	return SUCCESS;
}

void apm_driver_mysql_insert_stats(float duration, float user_cpu, float sys_cpu, long mem_peak_usage TSRMLS_DC)
{
	char *sql = NULL;
	MYSQL *connection;

	MYSQL_INSTANCE_INIT

	sql = emalloc(170);
	sprintf(
		sql,
		"INSERT INTO stats (request_id, duration, user_cpu, sys_cpu, mem_peak_usage) VALUES (@request_id, %f, %f, %f, %ld)",
		USEC_TO_SEC(duration),
		USEC_TO_SEC(user_cpu),
		USEC_TO_SEC(sys_cpu),
		mem_peak_usage
	);

	APM_DEBUG("[MySQL driver] Sending: %s\n", sql);
	if (mysql_query(connection, sql) != 0)
		APM_DEBUG("[MySQL driver] Error: %s\n", mysql_error(APM_MY_G(event_db)));

	efree(sql);
}
