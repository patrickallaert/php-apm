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

#include "php_apm.h"
#include "php_ini.h"
#include "driver_mysql.h"
#include <ext/mysqlnd/mysqlnd.h>

#ifdef NETWARE
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

ZEND_EXTERN_MODULE_GLOBALS(apm);

APM_DRIVER_CREATE(mysql)


static void mysql_destroy(TSRMLS_D) {
	APM_DEBUG("[MySQL driver] Closing connection\n");
	mysqlnd_close(APM_G(mysql_event_db), 0);
	APM_G(mysql_event_db) = NULL;
}

/* Returns the MYSQL instance (singleton) */
MYSQLND * mysql_get_instance(TSRMLS_D) {
	
	if (APM_G(mysql_event_db) == NULL) {
		APM_G(mysql_event_db) = mysqlnd_init(0, 1);

		APM_DEBUG("[MySQL driver] Connecting to server...");
		if (mysqlnd_connect(APM_G(mysql_event_db), APM_G(mysql_db_host), APM_G(mysql_db_user), 
                            APM_G(mysql_db_pass), strlen(APM_G(mysql_db_pass)),
                            APM_G(mysql_db_name), strlen(APM_G(mysql_db_name)),
                            APM_G(mysql_db_port), NULL, 0, 0) == NULL) {
			APM_DEBUG("FAILED! Message: %s\n", mysqlnd_error(APM_G(mysql_event_db)));

			mysql_destroy(TSRMLS_C);
			return NULL;
		}
		APM_DEBUG("OK\n");

		mysqlnd_set_character_set(APM_G(mysql_event_db), "utf8");

		mysqlnd_query(APM_G(mysql_event_db), TABLE_REQUEST, sizeof(TABLE_REQUEST)-1);
		mysqlnd_query(APM_G(mysql_event_db), TABLE_EVENT,   sizeof(TABLE_EVENT)-1);
		mysqlnd_query(APM_G(mysql_event_db), TABLE_STATS,   sizeof(TABLE_STATS)-1);
	}

	return APM_G(mysql_event_db);
}

/* Escape string request data */
#define APM_MYSQL_ESCAPE_STR(data) \
{ \
	if (APM_RD(data##_found)) { \
		data##_len = strlen(APM_RD_STRVAL(data)); \
		data##_esc = emalloc(data##_len * 2 + 1); \
		data##_len = mysqlnd_real_escape_string(connection, data##_esc, APM_RD_STRVAL(data), data##_len); \
	} \
}

/* Escape smart_str request data */
#define APM_MYSQL_ESCAPE_SMART_STR(data) \
{ \
	if (APM_RD(data##_found)) { \
		data##_len = strlen(APM_RD_SMART_STRVAL(data)); \
		data##_esc = emalloc(data##_len * 2 + 1); \
		data##_len = mysqlnd_real_escape_string(connection, data##_esc, APM_RD_SMART_STRVAL(data), data##_len); \
	} \
}

/* Insert a request in the backend */
static void apm_driver_mysql_insert_request(TSRMLS_D)
{
	char *application_esc = NULL, *script_esc = NULL, *uri_esc = NULL, *host_esc = NULL, *cookies_esc = NULL, *post_vars_esc = NULL, *referer_esc = NULL, *method_esc = NULL, *sql = NULL;
	unsigned int application_len = 0, script_len = 0, uri_len = 0, host_len = 0, ip_int = 0, cookies_len = 0, post_vars_len = 0, referer_len = 0, method_len = 0;
	struct in_addr ip_addr;
	MYSQLND *connection;

	extract_data(TSRMLS_C);

	APM_DEBUG("[MySQL driver] Begin insert request\n");
	if (APM_G(mysql_is_request_created)) {
		APM_DEBUG("[MySQL driver] SKIPPED, request already created.\n");
		return;
	}

	MYSQL_INSTANCE_INIT

	if (APM_G(application_id)) {
		application_len = strlen(APM_G(application_id));
		application_esc = emalloc(application_len * 2 + 1);
		application_len = mysqlnd_real_escape_string(connection, application_esc, APM_G(application_id), application_len);
	}
	
	APM_MYSQL_ESCAPE_STR(script);
	APM_MYSQL_ESCAPE_STR(uri);
	APM_MYSQL_ESCAPE_STR(host);
	APM_MYSQL_ESCAPE_STR(referer);
	APM_MYSQL_ESCAPE_STR(method);
	APM_MYSQL_ESCAPE_SMART_STR(cookies);
	APM_MYSQL_ESCAPE_SMART_STR(post_vars);

	if (APM_RD(ip_found) && (inet_pton(AF_INET, APM_RD_STRVAL(ip), &ip_addr) == 1)) {
		ip_int = ntohl(ip_addr.s_addr);
	}

	sql = emalloc(166 + application_len + script_len + uri_len + host_len + cookies_len + post_vars_len + referer_len + method_len);
	sprintf(
		sql,
		"INSERT INTO request (application, script, uri, host, ip, cookies, post_vars, referer, method) VALUES ('%s', '%s', '%s', '%s', %u, '%s', '%s', '%s', '%s')",
		application_esc ? application_esc : "",
		APM_RD(script_found) ? script_esc : "",
		APM_RD(uri_found) ? uri_esc : "",
		APM_RD(host_found) ? host_esc : "",
		ip_int, APM_RD(cookies_found) ? cookies_esc : "",
		APM_RD(post_vars_found) ? post_vars_esc : "",
		APM_RD(referer_found) ? referer_esc : "",
		APM_RD(method_found) ? method_esc : "");

	APM_DEBUG("[MySQL driver] Sending: %s\n", sql);
	if (mysqlnd_query(connection, sql, strlen(sql)) != 0)
		APM_DEBUG("[MySQL driver] Error: %s\n", mysqlnd_error(APM_G(mysql_event_db)));

	mysqlnd_query(connection, QUERY_REQUEST_ID, sizeof(QUERY_REQUEST_ID)-1);

	efree(sql);
	if (application_esc)
		efree(application_esc);
	if (script_esc)
		efree(script_esc);
	if (uri_esc)
		efree(uri_esc);
	if (host_esc)
		efree(host_esc);
	if (cookies_esc)
		efree(cookies_esc);
	if (post_vars_esc)
		efree(post_vars_esc);
	if (referer_esc)
		efree(referer_esc);
	if (method_esc)
		efree(method_esc);

	APM_G(mysql_is_request_created) = 1;
	APM_DEBUG("[MySQL driver] End insert request\n");
}

/* Insert an event in the backend */
void apm_driver_mysql_process_event(PROCESS_EVENT_ARGS)
{
	char *filename_esc = NULL, *msg_esc = NULL, *trace_esc = NULL, *sql = NULL;
	int filename_len = 0, msg_len = 0, trace_len = 0;
	MYSQLND *connection;

	apm_driver_mysql_insert_request(TSRMLS_C);

	MYSQL_INSTANCE_INIT

	if (error_filename) {
		filename_len = strlen(error_filename);
		filename_esc = emalloc(filename_len * 2 + 1);
		filename_len = mysqlnd_real_escape_string(connection, filename_esc, error_filename, filename_len);
	}

	if (msg) {
		msg_len = strlen(msg);
		msg_esc = emalloc(msg_len * 2 + 1);
		msg_len = mysqlnd_real_escape_string(connection, msg_esc, msg, msg_len);
	}

	if (trace) {
		trace_len = strlen(trace);
		trace_esc = emalloc(trace_len * 2 + 1);
		trace_len = mysqlnd_real_escape_string(connection, trace_esc, trace, trace_len);
	}

	sql = emalloc(135 + filename_len + msg_len + trace_len);
	sprintf(
		sql,
		"INSERT INTO event (request_id, type, file, line, message, backtrace) VALUES (@request_id, %d, '%s', %u, '%s', '%s')",
		type, error_filename ? filename_esc : "", error_lineno, msg ? msg_esc : "", trace ? trace_esc : "");

	APM_DEBUG("[MySQL driver] Sending: %s\n", sql);
	if (mysqlnd_query(connection, sql, strlen(sql)) != 0)
		APM_DEBUG("[MySQL driver] Error: %s\n", mysqlnd_error(APM_G(mysql_event_db)));

	efree(sql);
	efree(filename_esc);
	efree(msg_esc);
	efree(trace_esc);
}

int apm_driver_mysql_minit(int module_number TSRMLS_DC)
{
	return SUCCESS;
}

int apm_driver_mysql_rinit(TSRMLS_D)
{
	APM_G(mysql_is_request_created) = 0;
	return SUCCESS;
}

int apm_driver_mysql_mshutdown(SHUTDOWN_FUNC_ARGS)
{
	if (APM_G(mysql_event_db) != NULL) {
		mysql_destroy(TSRMLS_C);
	}

	return SUCCESS;
}

int apm_driver_mysql_rshutdown(TSRMLS_D)
{
	return SUCCESS;
}

void apm_driver_mysql_process_stats(TSRMLS_D)
{
	char *sql = NULL;
	MYSQLND *connection;

	apm_driver_mysql_insert_request(TSRMLS_C);

	MYSQL_INSTANCE_INIT

	sql = emalloc(170);
	sprintf(
		sql,
		"INSERT INTO stats (request_id, duration, user_cpu, sys_cpu, mem_peak_usage) VALUES (@request_id, %f, %f, %f, %ld)",
		USEC_TO_SEC(APM_G(duration)),
		USEC_TO_SEC(APM_G(user_cpu)),
		USEC_TO_SEC(APM_G(sys_cpu)),
		APM_G(mem_peak_usage)
	);

	APM_DEBUG("[MySQL driver] Sending: %s\n", sql);
	if (mysqlnd_query(connection, sql, strlen(sql)) != 0)
		APM_DEBUG("[MySQL driver] Error: %s\n", mysqlnd_error(APM_G(mysql_event_db)));

	efree(sql);
}
