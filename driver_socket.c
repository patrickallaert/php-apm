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

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "php_apm.h"
#include "php_ini.h"
#include "ext/standard/php_smart_str.h"
#include "ext/json/php_json.h"
#include "driver_socket.h"
#include "SAPI.h"

ZEND_EXTERN_MODULE_GLOBALS(apm)

ZEND_DECLARE_MODULE_GLOBALS(apm_socket)

APM_DRIVER_CREATE(socket)

PHP_INI_BEGIN()
	/* Boolean controlling whether the driver is active or not */
	STD_PHP_INI_BOOLEAN("apm.socket_enabled", "1", PHP_INI_ALL, OnUpdateBool, enabled, zend_apm_socket_globals, apm_socket_globals)
	/* Boolean controlling the collection of stats */
	STD_PHP_INI_BOOLEAN("apm.socket_stats_enabled", "1", PHP_INI_ALL, OnUpdateBool, stats_enabled, zend_apm_socket_globals, apm_socket_globals)
	/* error_reporting of the driver */
	STD_PHP_INI_ENTRY("apm.socket_error_reporting", NULL, PHP_INI_ALL, OnUpdateAPMsocketErrorReporting, error_reporting, zend_apm_socket_globals, apm_socket_globals)
	/* Socket path */
	STD_PHP_INI_ENTRY("apm.socket_path", "file:/tmp/apm.sock|tcp:127.0.0.1:8264", PHP_INI_ALL, OnUpdateString, path, zend_apm_socket_globals, apm_socket_globals)
	/* process silenced events? */
	STD_PHP_INI_BOOLEAN("apm.socket_process_silenced_events", "1", PHP_INI_PERDIR, OnUpdateBool, process_silenced_events, zend_apm_socket_globals, apm_socket_globals)
PHP_INI_END()

/* Insert an event in the backend */
void apm_driver_socket_process_event(PROCESS_EVENT_ARGS)
{
	(*APM_SOCK_G(last_event))->next = (apm_event_entry *) calloc(1, sizeof(apm_event_entry));
	// Event type not provided yet
	// (*APM_SOCK_G(last_event))->next->event.event_type = ;
	(*APM_SOCK_G(last_event))->next->event.type = type;

	if (((*APM_SOCK_G(last_event))->next->event.error_filename = malloc(strlen(error_filename) + 1)) != NULL) {
		strcpy((*APM_SOCK_G(last_event))->next->event.error_filename, error_filename);
	}

	(*APM_SOCK_G(last_event))->next->event.error_lineno = error_lineno;

	if (((*APM_SOCK_G(last_event))->next->event.msg = malloc(strlen(msg) + 1)) != NULL) {
		strcpy((*APM_SOCK_G(last_event))->next->event.msg, msg);
	}

	if (APM_G(store_stacktrace) && trace && (((*APM_SOCK_G(last_event))->next->event.trace = malloc(strlen(trace) + 1)) != NULL)) {
		strcpy((*APM_SOCK_G(last_event))->next->event.trace, trace);
	}

	APM_SOCK_G(last_event) = &(*APM_SOCK_G(last_event))->next;
}

int apm_driver_socket_minit(int module_number)
{
	REGISTER_INI_ENTRIES();

	return SUCCESS;
}

int apm_driver_socket_rinit()
{
	APM_SOCK_G(events) = (apm_event_entry *) malloc(sizeof(apm_event_entry));
	APM_SOCK_G(events)->event.type = 0;
	APM_SOCK_G(events)->event.error_filename = NULL;
	APM_SOCK_G(events)->event.error_lineno = 0;
	APM_SOCK_G(events)->event.msg = NULL;
	APM_SOCK_G(events)->event.trace = NULL;
	APM_SOCK_G(events)->next = NULL;
	APM_SOCK_G(last_event) = &APM_SOCK_G(events);

	return SUCCESS;
}

int apm_driver_socket_mshutdown()
{
	return SUCCESS;
}

static void clear_events()
{
	apm_event_entry * event_entry_cursor = NULL;
	apm_event_entry * event_entry_cursor_next = NULL;

	if (APM_SOCK_G(events) != *APM_SOCK_G(last_event)) {
		event_entry_cursor = APM_SOCK_G(events);
		event_entry_cursor_next = event_entry_cursor->next;
		while ((event_entry_cursor = event_entry_cursor_next) != NULL) {
			free(event_entry_cursor->event.error_filename);
			free(event_entry_cursor->event.msg);
			free(event_entry_cursor->event.trace);
			event_entry_cursor_next = event_entry_cursor->next;
			free(event_entry_cursor);
		}
		APM_SOCK_G(last_event) = &APM_SOCK_G(events);
	}
}

int apm_driver_socket_rshutdown()
{
	struct sockaddr_un serveraddr;
	zval *data, *tmp, **val, *errors;
	smart_str buf = {0};
	apm_event_entry * event_entry_cursor = NULL;
	apm_event_entry * event_entry_cursor_next = NULL;
	int sd, sds[MAX_SOCKETS];
	unsigned char i, sd_it;
	char *socket_path, *path_copy;
	struct addrinfo hints, *servinfo;
	char host[1024], *port;

	if (!APM_SOCK_G(enabled)) {
		return SUCCESS;
	}

	sd_it = 0;

	// Path must be copied for strtok to work
	path_copy = (char*)malloc(strlen(APM_SOCK_G(path)));
	strcpy(path_copy, APM_SOCK_G(path));

	socket_path = strtok(path_copy, "|");

	while (socket_path != NULL && sd_it < MAX_SOCKETS) {
		if (strncmp(socket_path, "file:", 5) == 0) {
			socket_path += 5;
			sd = socket(AF_UNIX, SOCK_STREAM, 0);
			if (sd < 0) {
				break;
			}

			memset(&serveraddr, 0, sizeof(serveraddr));
			serveraddr.sun_family = AF_UNIX;
			strcpy(serveraddr.sun_path, socket_path);

			if (connect(sd, (struct sockaddr *)&serveraddr, SUN_LEN(&serveraddr)) < 0) {
				close(sd);
			} else {
				sds[sd_it++] = sd;
			}
		}
		if (strncmp(socket_path, "tcp:", 4) == 0) {
			socket_path += 4;

			memset(&hints, 0, sizeof hints);
			hints.ai_family = AF_INET;
			hints.ai_socktype = SOCK_STREAM;

			port = strchr(socket_path, ':');
			if (port == NULL) {
				break;
			} else {
				strncpy(host, socket_path, port - socket_path);
				host[port - socket_path] = '\0';
				// advance one char to be on port number
				++port;
			}

			if (getaddrinfo(host, port, &hints, &servinfo) != 0) {
				break;
			}

			sd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
			if (sd < 0) {
				break;
			}

			if (connect(sd, servinfo->ai_addr, servinfo->ai_addrlen) < 0) {
				close(sd);
			} else {
				sds[sd_it++] = sd;
			}
			freeaddrinfo(servinfo);
		}
		socket_path = strtok(NULL, "|");
	}

	ALLOC_INIT_ZVAL(data);
	array_init(data);
	add_assoc_string(data, "application_id", APM_G(application_id), 1);
	add_assoc_long(data, "response_code", SG(sapi_headers).http_response_code);

	zend_is_auto_global("_SERVER", sizeof("_SERVER")-1 TSRMLS_CC);
	if ((tmp = PG(http_globals)[TRACK_VARS_SERVER])) {
		if ((zend_hash_find(Z_ARRVAL_P(tmp), "REQUEST_TIME", sizeof("REQUEST_TIME"), (void**)&val) == SUCCESS) && (Z_TYPE_PP(val) == IS_LONG)) {
			add_assoc_long(data, "ts", Z_LVAL_PP(val));
		}
		if ((zend_hash_find(Z_ARRVAL_P(tmp), "SCRIPT_FILENAME", sizeof("SCRIPT_FILENAME"), (void**)&val) == SUCCESS) && (Z_TYPE_PP(val) == IS_STRING)) {
			zval_add_ref(val);
			add_assoc_zval(data, "script", *val);
		}
		if ((zend_hash_find(Z_ARRVAL_P(tmp), "REQUEST_URI", sizeof("REQUEST_URI"), (void**)&val) == SUCCESS) && (Z_TYPE_PP(val) == IS_STRING)) {
			zval_add_ref(val);
			add_assoc_zval(data, "uri", *val);
		}
		if ((zend_hash_find(Z_ARRVAL_P(tmp), "HTTP_HOST", sizeof("HTTP_HOST"), (void**)&val) == SUCCESS) && (Z_TYPE_PP(val) == IS_STRING)) {
			zval_add_ref(val);
			add_assoc_zval(data, "host", *val);
		}
		// Add ip, referer, ... if an error occured or if thresold is reached.
		if (
			APM_SOCK_G(events) != *APM_SOCK_G(last_event)
			|| APM_G(duration) > 1000.0 * APM_G(stats_duration_threshold)
#ifdef HAVE_GETRUSAGE
			|| APM_G(user_cpu) > 1000.0 * APM_G(stats_user_cpu_threshold)
			|| APM_G(sys_cpu) > 1000.0 * APM_G(stats_sys_cpu_threshold)
#endif
		) {
			if ((zend_hash_find(Z_ARRVAL_P(tmp), "REMOTE_ADDR", sizeof("REMOTE_ADDR"), (void**)&val) == SUCCESS) && (Z_TYPE_PP(val) == IS_STRING)) {
				zval_add_ref(val);
				add_assoc_zval(data, "ip", *val);
			}
			if ((zend_hash_find(Z_ARRVAL_P(tmp), "HTTP_REFERER", sizeof("HTTP_REFERER"), (void**)&val) == SUCCESS) && (Z_TYPE_PP(val) == IS_STRING)) {
				zval_add_ref(val);
				add_assoc_zval(data, "referer", *val);
			}
			if (APM_G(store_cookies)) {
				zend_is_auto_global("_COOKIE", sizeof("_COOKIE")-1 TSRMLS_CC);
				if ((tmp = PG(http_globals)[TRACK_VARS_COOKIE]) && (Z_ARRVAL_P(tmp)->nNumOfElements > 0)) {
					zval_add_ref(&tmp);
					add_assoc_zval(data, "cookies", tmp);
				}
			}
			/*
			This needs to be deactivated for now as it produces a segfault with error message: "zend_mm_heap corrupted".
			When using add_assoc_zval(), the refcount is not automatically incremented, the original code was missing a call to zval_add_ref().
			This remains commented as we don't want to dump _POST data to socket
			if (APM_G(store_post)) {
				zend_is_auto_global("_POST", sizeof("_POST")-1 TSRMLS_CC);
				if ((tmp = PG(http_globals)[TRACK_VARS_POST]) && (Z_ARRVAL_P(tmp)->nNumOfElements > 0)) {
					zval_add_ref(&tmp);
					add_assoc_zval(data, "post_vars", tmp);
				}
			}
			*/
		}
	}
	if (APM_SOCK_G(stats_enabled)) {
		add_assoc_double(data, "duration", APM_G(duration));
		add_assoc_long(data, "mem_peak_usage", APM_G(mem_peak_usage));
#ifdef HAVE_GETRUSAGE
		add_assoc_double(data, "user_cpu", APM_G(user_cpu));
		add_assoc_double(data, "sys_cpu", APM_G(sys_cpu));
#endif
	}

	if (APM_SOCK_G(events) != *APM_SOCK_G(last_event)) {
		event_entry_cursor = APM_SOCK_G(events);
		event_entry_cursor_next = event_entry_cursor->next;

		ALLOC_INIT_ZVAL(errors);
		array_init(errors);

		while ((event_entry_cursor = event_entry_cursor_next) != NULL) {
			ALLOC_INIT_ZVAL(tmp);
			array_init(tmp);

			add_assoc_long(tmp, "type", event_entry_cursor->event.type);
			add_assoc_long(tmp, "line", event_entry_cursor->event.error_lineno);
			add_assoc_string(tmp, "file", event_entry_cursor->event.error_filename, 1);
			add_assoc_string(tmp, "message", event_entry_cursor->event.msg, 1);
			add_assoc_string(tmp, "trace", event_entry_cursor->event.trace, 1);

			add_next_index_zval(errors, tmp);

			event_entry_cursor_next = event_entry_cursor->next;
		}
		add_assoc_zval(data, "errors", errors);
	}

	php_json_encode(&buf, data, 0 TSRMLS_CC);

	smart_str_0(&buf);

	zval_ptr_dtor(&data);

	for (i = 0; i < sd_it; ++i) {
		if (send(sds[i], buf.c, buf.len, 0) < 0) {
		}
	}

	smart_str_free(&buf);

	clear_events();

	for (i = 0; i < sd_it; ++i) {
		close(sds[i]);
	}

	return SUCCESS;
}

void apm_driver_socket_process_stats()
{
}
