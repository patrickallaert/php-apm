/*
 +----------------------------------------------------------------------+
 |  APM stands for Alternative PHP Monitor                              |
 +----------------------------------------------------------------------+
 | Copyright (c) 2008-2013  Davide Mendolia, Patrick Allaert            |
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
	/* store silenced events? */
	STD_PHP_INI_BOOLEAN("apm.mysql_store_silenced_events", "1",         PHP_INI_PERDIR, OnUpdateBool,   store_silenced_events, zend_apm_mysql_globals, apm_mysql_globals)  
PHP_INI_END()

/* Returns the MYSQL instance (singleton) */
MYSQL * mysql_get_instance() {
	if (APM_MY_G(event_db) == NULL) {
		APM_MY_G(event_db) = malloc(sizeof(MYSQL));
		mysql_init(APM_MY_G(event_db));
		APM_DEBUG("[MySQL driver] Connecting to server...");
		if (mysql_real_connect(APM_MY_G(event_db), APM_MY_G(db_host), APM_MY_G(db_user), APM_MY_G(db_pass), APM_MY_G(db_name), APM_MY_G(db_port), NULL, 0) == NULL) {
			APM_DEBUG("FAILED! Message: %s\n", mysql_error(APM_MY_G(event_db)));

			free(APM_MY_G(event_db));
			APM_MY_G(event_db) = NULL;
			return NULL;
		}
		APM_DEBUG("OK\n");
		mysql_set_character_set(APM_MY_G(event_db), "utf8");
	}

	return APM_MY_G(event_db);
}

/* Insert a request in the backend */
void apm_driver_mysql_insert_request(char * uri, char * host, char * ip, char * cookies, char * post_vars, char * referer TSRMLS_DC)
{
	char *script = NULL, *script_esc = NULL, *uri_esc = NULL, *host_esc = NULL, *cookies_esc = NULL, *post_vars_esc = NULL, *referer_esc = NULL, *sql = NULL;
	int script_len = 0, uri_len = 0, host_len = 0, ip_int = 0, cookies_len = 0, post_vars_len = 0, referer_len = 0;
	struct in_addr ip_addr;
	MYSQL *connection;

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

	if (uri) {
		uri_len = strlen(uri);
		uri_esc = emalloc(uri_len * 2 + 1);
		uri_len = mysql_real_escape_string(connection, uri_esc, uri, uri_len);
	}

	if (host) {
		host_len = strlen(host);
		host_esc = emalloc(host_len * 2 + 1);
		host_len = mysql_real_escape_string(connection, host_esc, host, host_len);
	}

	if (ip && (inet_pton(AF_INET, ip, &ip_addr) == 1)) {
		ip_int = ntohl(ip_addr.s_addr);
	}
	
	if (cookies) {
		cookies_len = strlen(cookies);
		cookies_esc = emalloc(cookies_len * 2 + 1);
		cookies_len = mysql_real_escape_string(connection, cookies_esc, cookies, cookies_len);
	}

	if (post_vars) {
		post_vars_len = strlen(post_vars);
		post_vars_esc = emalloc(post_vars_len * 2 + 1);
		post_vars_len = mysql_real_escape_string(connection, post_vars_esc, post_vars, post_vars_len);
	}

	if (referer) {
		referer_len = strlen(referer);
		referer_esc = emalloc(referer_len * 2 + 1);
		referer_len = mysql_real_escape_string(connection, referer_esc, referer, referer_len);
	}

	sql = emalloc(134 + script_len + uri_len + host_len + cookies_len + post_vars_len + referer_len);
	sprintf(
		sql,
		"INSERT INTO request (script, uri, host, ip, cookies, post_vars, referer) VALUES ('%s', '%s', '%s', %u, '%s', '%s', '%s')",
		script ? script_esc : "", uri ? uri_esc : "", host ? host_esc : "", ip_int, cookies ? cookies_esc : "", post_vars ? post_vars_esc : "", referer ? referer_esc : "");

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
	return SUCCESS;
}

int apm_driver_mysql_rshutdown()
{
	if (APM_MY_G(event_db) != NULL) {
		APM_DEBUG("[MySQL driver] Closing connection\n");
		mysql_close(APM_MY_G(event_db));
		free(APM_MY_G(event_db));
		APM_MY_G(event_db) = NULL;
	}
	return SUCCESS;
}

void apm_driver_mysql_insert_slow_request(float duration TSRMLS_DC)
{
	char *sql = NULL;
	MYSQL *connection;

	MYSQL_INSTANCE_INIT

	sql = emalloc(90);
	sprintf(
		sql,
		"INSERT INTO slow_request (request_id, duration) VALUES (@request_id, %f)",
		USEC_TO_SEC(duration));

	APM_DEBUG("[MySQL driver] Sending: %s\n", sql);
	if (mysql_query(connection, sql) != 0)
		APM_DEBUG("[MySQL driver] Error: %s\n", mysql_error(APM_MY_G(event_db)));

	efree(sql);
}

zend_bool apm_driver_mysql_wants_silenced_events(TSRMLS_DC)
{
	return APM_MY_G(store_silenced_events);
}

