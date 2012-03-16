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

#include "php.h"
#include "php_ini.h"
#include "zend_exceptions.h"
#include "php_apm.h"
#include "backtrace.h"
#include "ext/standard/info.h"
#ifdef APM_DRIVER_SQLITE3
  #include "driver_sqlite3.h"
#endif
#ifdef APM_DRIVER_MYSQL
  #include "driver_mysql.h"
#endif

ZEND_DECLARE_MODULE_GLOBALS(apm);
static PHP_GINIT_FUNCTION(apm);

#define APM_DRIVER_BEGIN_LOOP driver_entry = APM_G(drivers); \
		while ((driver_entry = driver_entry->next) != NULL) {

#define EXTRACT_DATA() zval **uri = NULL, **host = NULL, **ip = NULL, *tmp; \
zend_bool uri_found = 0, host_found = 0, ip_found = 0, cookies_found = 0; \
smart_str cookies = {0}; \
 \
zend_is_auto_global("_SERVER", sizeof("_SERVER")-1 TSRMLS_CC); \
if ((tmp = PG(http_globals)[TRACK_VARS_SERVER])) { \
	if ((zend_hash_find(Z_ARRVAL_P(tmp), "REQUEST_URI", sizeof("REQUEST_URI"), (void**)&uri) == SUCCESS) && \
		(Z_TYPE_PP(uri) == IS_STRING)) { \
		uri_found = 1; \
	} \
	if ((zend_hash_find(Z_ARRVAL_P(tmp), "HTTP_HOST", sizeof("HTTP_HOST"), (void**)&host) == SUCCESS) && \
		(Z_TYPE_PP(host) == IS_STRING)) { \
		host_found = 1; \
	} \
	if ((zend_hash_find(Z_ARRVAL_P(tmp), "REMOTE_ADDR", sizeof("REMOTE_ADDR"), (void**)&ip) == SUCCESS) && \
		(Z_TYPE_PP(ip) == IS_STRING)) { \
		ip_found = 1; \
	} \
} \
zend_is_auto_global("_COOKIE", sizeof("_COOKIE")-1 TSRMLS_CC); \
if ((tmp = PG(http_globals)[TRACK_VARS_COOKIE])) { \
	if (Z_ARRVAL_P(tmp)->nNumOfElements > 0) { \
		APM_G(buffer) = &cookies; \
		zend_print_zval_r_ex(apm_write, tmp, 0 TSRMLS_CC); \
		cookies_found = 1; \
	} \
}

static int apm_write(const char *str, uint length) {
	TSRMLS_FETCH();
	smart_str_appendl(APM_G(buffer), str, length);
	smart_str_0(APM_G(buffer));
	return length;
}

void (*old_error_cb)(int type, const char *error_filename,
                     const uint error_lineno, const char *format,
                     va_list args);

void apm_error_cb(int type, const char *error_filename,
                  const uint error_lineno, const char *format,
                  va_list args);

void apm_throw_exception_hook(zval *exception TSRMLS_DC);

static void insert_event(int, char *, uint, char * TSRMLS_DC);
static void deffered_insert_events(TSRMLS_D);

/* recorded timestamp for the request */
struct timeval begin_tp;

zend_function_entry apm_functions[] = {
#ifdef APM_DRIVER_SQLITE3
		PHP_FE(apm_get_sqlite_events, NULL)
		PHP_FE(apm_get_sqlite_slow_requests, NULL)
		PHP_FE(apm_get_sqlite_events_count, NULL)
		PHP_FE(apm_get_sqlite_slow_requests_count, NULL)
		PHP_FE(apm_get_sqlite_event_info, NULL)
#endif
#ifdef APM_DRIVER_MYSQL
		PHP_FE(apm_get_mysql_events, NULL)
		PHP_FE(apm_get_mysql_slow_requests, NULL)
		PHP_FE(apm_get_mysql_events_count, NULL)
		PHP_FE(apm_get_mysql_slow_requests_count, NULL)
		PHP_FE(apm_get_mysql_event_info, NULL)
#endif
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
	PHP_MODULE_GLOBALS(apm),
	PHP_GINIT(apm),
	NULL,
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_APM
ZEND_GET_MODULE(apm)
#endif

PHP_INI_BEGIN()
	/* Boolean controlling whether the extension is globally active or not */
	STD_PHP_INI_BOOLEAN("apm.enabled",              "1",   PHP_INI_ALL, OnUpdateBool, enabled,               zend_apm_globals, apm_globals)
	/* Boolean controlling whether the event monitoring is active or not */
	STD_PHP_INI_BOOLEAN("apm.event_enabled",        "1",   PHP_INI_ALL, OnUpdateBool, event_enabled,         zend_apm_globals, apm_globals)
	/* Boolean controlling whether the slow request monitoring is active or not */
	STD_PHP_INI_BOOLEAN("apm.slow_request_enabled", "1",   PHP_INI_ALL, OnUpdateBool, slow_request_enabled,  zend_apm_globals, apm_globals)
	/* Boolean controlling whether the the stacktrace should be generated or not */
	STD_PHP_INI_BOOLEAN("apm.stacktrace_enabled",   "1",   PHP_INI_ALL, OnUpdateBool, stacktrace_enabled,    zend_apm_globals, apm_globals)
	/* Boolean controlling whether the processing of events by drivers should be deffered at the end of the request */
	STD_PHP_INI_BOOLEAN("apm.deffered_processing",  "1",   PHP_INI_PERDIR, OnUpdateBool, deffered_processing,zend_apm_globals, apm_globals)
	/* Time (in ms) before a request is considered 'slow' */
	STD_PHP_INI_ENTRY("apm.slow_request_duration",  "100", PHP_INI_ALL, OnUpdateLong, slow_request_duration, zend_apm_globals, apm_globals)
PHP_INI_END()

static PHP_GINIT_FUNCTION(apm)
{
	apm_driver_entry **next;
	apm_globals->buffer = NULL;
	apm_globals->drivers = (apm_driver_entry *) malloc(sizeof(apm_driver_entry));
	apm_globals->drivers->driver.insert_event = (void (*)(int, char *, uint, char *, char *, char *, char *, char *, char * TSRMLS_DC)) NULL;
	apm_globals->drivers->driver.minit = (int (*)(int)) NULL;
	apm_globals->drivers->driver.rinit = (int (*)()) NULL;
	apm_globals->drivers->driver.mshutdown = (int (*)()) NULL;
	apm_globals->drivers->driver.rshutdown = (int (*)()) NULL;
	apm_globals->drivers->driver.insert_slow_request = (void (*)(float, char *)) NULL;

	next = &apm_globals->drivers->next;
	*next = (apm_driver_entry *) NULL;
#ifdef APM_DRIVER_SQLITE3
	*next = apm_driver_sqlite3_create();
	next = &(*next)->next;
#endif
#ifdef APM_DRIVER_MYSQL
	*next = apm_driver_mysql_create();
	next = &(*next)->next;
#endif

	apm_globals->events = (apm_event_entry *) malloc(sizeof(apm_event_entry));
	apm_globals->events->event.type = 0;
	apm_globals->events->event.error_filename = NULL;
	apm_globals->events->event.error_lineno = 0;
	apm_globals->events->event.msg = NULL;
	apm_globals->events->event.trace = NULL;
	apm_globals->events->next = NULL;
	apm_globals->last_event = &apm_globals->events;
}

PHP_MINIT_FUNCTION(apm)
{
	REGISTER_INI_ENTRIES();

	REGISTER_LONG_CONSTANT("APM_ORDER_ID", APM_ORDER_ID, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("APM_ORDER_TIMESTAMP", APM_ORDER_TIMESTAMP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("APM_ORDER_TYPE", APM_ORDER_TYPE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("APM_ORDER_DURATION", APM_ORDER_DURATION, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("APM_ORDER_FILE", APM_ORDER_FILE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("APM_ORDER_IP", APM_ORDER_IP, CONST_CS | CONST_PERSISTENT);

	/* Storing actual error callback function for later restore */
	old_error_cb = zend_error_cb;
	
	if (APM_G(enabled)) {
		apm_driver_entry * driver_entry;

		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL) {
			if (driver_entry->driver.minit(module_number) == FAILURE) {
				return FAILURE;
			}
		}
	}

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(apm)
{
	UNREGISTER_INI_ENTRIES();

	if (APM_G(enabled)) {
		apm_driver_entry * driver_entry;

		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL) {
			if (driver_entry->driver.mshutdown() == FAILURE) {
				return FAILURE;
			}
		}
	}

	/* Restoring saved error callback function */
	zend_error_cb = old_error_cb;

	return SUCCESS;
}

PHP_RINIT_FUNCTION(apm)
{
	if (APM_G(enabled)) {
		apm_driver_entry * driver_entry;

		if (APM_G(event_enabled)) {
			/* storing timestamp of request */
			gettimeofday(&begin_tp, NULL);
		}

		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL) {
			if (driver_entry->driver.is_enabled()) {
				if (driver_entry->driver.rinit()) {
					return FAILURE;
				}
			}
		}

		/* Replacing current error callback function with apm's one */
		zend_error_cb = apm_error_cb;
		zend_throw_exception_hook = apm_throw_exception_hook;
	}
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(apm)
{
	if (APM_G(enabled)) {
		apm_driver_entry * driver_entry;
		if (APM_G(slow_request_enabled)) {
			float duration;
			struct timeval end_tp;

			gettimeofday(&end_tp, NULL);

			/* Request longer than accepted thresold ? */
			duration = (float) (SEC_TO_USEC(end_tp.tv_sec - begin_tp.tv_sec) + end_tp.tv_usec - begin_tp.tv_usec);
			if (duration > 1000.0 * APM_G(slow_request_duration)) {
				zval **array;
				zval **token;
				char *script_filename = NULL;

				zend_is_auto_global("_SERVER", sizeof("_SERVER")-1 TSRMLS_CC);
				if (zend_hash_find(&EG(symbol_table), "_SERVER", sizeof("_SERVER"), (void **) &array) == SUCCESS &&
					Z_TYPE_PP(array) == IS_ARRAY &&
					zend_hash_find(Z_ARRVAL_PP(array), "SCRIPT_FILENAME", sizeof("SCRIPT_FILENAME"), (void **) &token) == SUCCESS) {
					script_filename = Z_STRVAL_PP(token);
				}

				driver_entry = APM_G(drivers);
				while ((driver_entry = driver_entry->next) != NULL) {
					if (driver_entry->driver.is_enabled()) {
						driver_entry->driver.insert_slow_request(duration, script_filename);
					}
				}
			}
		}

		if (APM_G(deffered_processing) && APM_G(events) != *APM_G(last_event)) {
			deffered_insert_events(TSRMLS_C);

			apm_event_entry * event_entry_cursor = APM_G(events);
			apm_event_entry * event_entry_cursor_next = event_entry_cursor->next;
			while ((event_entry_cursor = event_entry_cursor_next) != NULL) {
				free(event_entry_cursor->event.error_filename);
				free(event_entry_cursor->event.msg);
				free(event_entry_cursor->event.trace);
				event_entry_cursor_next = event_entry_cursor->next;
				free(event_entry_cursor);
			}
			APM_G(last_event) = &APM_G(events);
		}
		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL) {
			if (driver_entry->driver.is_enabled()) {
				if (driver_entry->driver.rshutdown() == FAILURE) {
					return FAILURE;
				}
			}
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

/* Insert an event in the backend */
static void insert_event(int type, char * error_filename, uint error_lineno, char * msg TSRMLS_DC)
{
	smart_str trace_str = {0};
	apm_driver_entry * driver_entry;

	if (APM_G(stacktrace_enabled)) {
		append_backtrace(&trace_str TSRMLS_CC);
		smart_str_0(&trace_str);
	}

	if (APM_G(deffered_processing)) {
		(*APM_G(last_event))->next = (apm_event_entry *) malloc(sizeof(apm_event_entry));
		(*APM_G(last_event))->next->event.type = type;

		if (((*APM_G(last_event))->next->event.error_filename = malloc(strlen(error_filename) + 1)) != NULL) {
			strcpy((*APM_G(last_event))->next->event.error_filename, error_filename);
		} else {
			(*APM_G(last_event))->next->event.error_filename = NULL;
		}
		
		(*APM_G(last_event))->next->event.error_lineno = error_lineno;


		if (((*APM_G(last_event))->next->event.msg = malloc(strlen(msg) + 1)) != NULL) {
			strcpy((*APM_G(last_event))->next->event.msg, msg);
		} else {
			(*APM_G(last_event))->next->event.msg = NULL;
		}

		if (APM_G(stacktrace_enabled) && trace_str.c && (((*APM_G(last_event))->next->event.trace = malloc(strlen(trace_str.c) + 1)) != NULL)) {
			strcpy((*APM_G(last_event))->next->event.trace, trace_str.c);
		} else {
			(*APM_G(last_event))->next->event.trace = NULL;
		}

		(*APM_G(last_event))->next->next = NULL;
		APM_G(last_event) = &(*APM_G(last_event))->next;
	} else {
		EXTRACT_DATA();

		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL) {
			if (driver_entry->driver.is_enabled() && (type & driver_entry->driver.error_reporting())) {
				driver_entry->driver.insert_event(
					type,
					error_filename,
					error_lineno,
					msg,
					(APM_G(stacktrace_enabled) && trace_str.c) ? trace_str.c : "",
					uri_found ? Z_STRVAL_PP(uri) : "",
					host_found ? Z_STRVAL_PP(host) : "",
					ip_found ? Z_STRVAL_PP(ip) : "",
					cookies_found ? cookies.c : "" TSRMLS_CC
				);
			}
		}

		smart_str_free(&cookies);
	}

	smart_str_free(&trace_str);
}

static void deffered_insert_events(TSRMLS_D)
{
	apm_driver_entry * driver_entry = APM_G(drivers);
	apm_event_entry * event_entry_cursor;

	EXTRACT_DATA();

	while ((driver_entry = driver_entry->next) != NULL) {
		if (driver_entry->driver.is_enabled()) {
			event_entry_cursor = APM_G(events);
			while ((event_entry_cursor = event_entry_cursor->next) != NULL) {
				if (event_entry_cursor->event.type & driver_entry->driver.error_reporting()) {
					driver_entry->driver.insert_event(
						event_entry_cursor->event.type,
						event_entry_cursor->event.error_filename,
						event_entry_cursor->event.error_lineno,
						event_entry_cursor->event.msg,
						event_entry_cursor->event.trace,
						uri_found ? Z_STRVAL_PP(uri) : "",
						host_found ? Z_STRVAL_PP(host) : "",
						ip_found ? Z_STRVAL_PP(ip) : "",
						cookies_found ? cookies.c : "" TSRMLS_CC
					);
				}
			}
		}
	}

	smart_str_free(&cookies);
}
