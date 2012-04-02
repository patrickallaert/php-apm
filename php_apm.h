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

#ifndef PHP_APM_H
#define PHP_APM_H

#include "php.h"
#include "zend_errors.h"
#include "ext/standard/php_smart_str.h"

extern zend_module_entry apm_module_entry;
#define phpext_apm_ptr &apm_module_entry

#ifdef PHP_WIN32
#define PHP_APM_API __declspec(dllexport)
#else
#define PHP_APM_API
#endif

#include "TSRM.h"

#define APM_ORDER_ID 1
#define APM_ORDER_TIMESTAMP 2
#define APM_ORDER_TYPE 3
#define APM_ORDER_DURATION 3
#define APM_ORDER_FILE 4
#define APM_ORDER_IP 5
#define APM_ORDER_URL 6

#define APM_E_ALL (E_ALL | E_STRICT)

typedef struct apm_event {
	int type;
	char * error_filename;
	uint error_lineno;
	char * msg;
	char * trace;
} apm_event;

typedef struct apm_event_entry {
	apm_event event;
	struct apm_event_entry *next;
} apm_event_entry;

typedef struct apm_driver {
	void (* insert_event)(int, char *, uint, char *, char *, char *, char *, char *, char * TSRMLS_DC);
	int (* minit)(int);
	int (* rinit)();
	int (* mshutdown)();
	int (* rshutdown)();
	void (* insert_slow_request)(float, char *);
	zend_bool (* is_enabled)();
	int (* error_reporting)();
} apm_driver;

typedef struct apm_driver_entry {
	apm_driver driver;
	struct apm_driver_entry *next;
} apm_driver_entry;

#ifdef ZTS
#define APM_GLOBAL(driver, v) TSRMG(apm_##driver##_globals_id, zend_apm_##driver##_globals *, v)
#else
#define APM_GLOBAL(driver, v) (apm_##driver##_globals.v)
#endif

#define APM_DRIVER_CREATE(name) \
static PHP_INI_MH(OnUpdateAPM##name##ErrorReporting) \
{ \
	APM_GLOBAL(name, error_reporting) = (new_value ? atoi(new_value) : APM_E_##name ); \
	return SUCCESS; \
} \
zend_bool apm_driver_##name##_is_enabled() { \
	return APM_GLOBAL(name, enabled); \
} \
int apm_driver_##name##_error_reporting() { \
	return APM_GLOBAL(name, error_reporting); \
} \
apm_driver_entry * apm_driver_##name##_create() \
{ \
	apm_driver_entry * driver_entry; \
	driver_entry = (apm_driver_entry *) malloc(sizeof(apm_driver_entry)); \
	driver_entry->driver.insert_event = apm_driver_##name##_insert_event; \
	driver_entry->driver.minit = apm_driver_##name##_minit; \
	driver_entry->driver.rinit = apm_driver_##name##_rinit; \
	driver_entry->driver.mshutdown = apm_driver_##name##_mshutdown; \
	driver_entry->driver.rshutdown = apm_driver_##name##_rshutdown; \
	driver_entry->driver.insert_slow_request = apm_driver_##name##_insert_slow_request; \
	driver_entry->driver.is_enabled = apm_driver_##name##_is_enabled; \
	driver_entry->driver.error_reporting = apm_driver_##name##_error_reporting; \
	driver_entry->next = NULL; \
	return driver_entry; \
}

PHP_MINIT_FUNCTION(apm);
PHP_MSHUTDOWN_FUNCTION(apm);
PHP_RINIT_FUNCTION(apm);
PHP_RSHUTDOWN_FUNCTION(apm);
PHP_MINFO_FUNCTION(apm);

#ifdef APM_DRIVER_SQLITE3
PHP_FUNCTION(apm_get_sqlite_events);
PHP_FUNCTION(apm_get_sqlite_slow_requests);
PHP_FUNCTION(apm_get_sqlite_events_count);
PHP_FUNCTION(apm_get_sqlite_slow_requests_count);
PHP_FUNCTION(apm_get_sqlite_event_info);
#endif
#ifdef APM_DRIVER_MYSQL
PHP_FUNCTION(apm_get_mysql_events);
PHP_FUNCTION(apm_get_mysql_slow_requests);
PHP_FUNCTION(apm_get_mysql_events_count);
PHP_FUNCTION(apm_get_mysql_slow_requests_count);
PHP_FUNCTION(apm_get_mysql_event_info);
#endif

/* Extension globals */
ZEND_BEGIN_MODULE_GLOBALS(apm)
	/* Boolean controlling whether the extension is globally active or not */
	zend_bool enabled;
	/* Boolean controlling whether the event monitoring is active or not */
	zend_bool event_enabled;
	/* Boolean controlling whether the slow request monitoring is active or not */
	zend_bool slow_request_enabled;
	/* Boolean controlling whether the the stacktrace should be generated or not */
	zend_bool stacktrace_enabled;
	/* Boolean controlling whether the processing of events by drivers should be deffered at the end of the request */
	zend_bool deffered_processing;
	/* Time (in ms) before a request is considered 'slow' */
	long      slow_request_duration;
	apm_driver_entry *drivers;
	apm_event_entry *events;
	apm_event_entry **last_event;
	smart_str *buffer;
ZEND_END_MODULE_GLOBALS(apm)

#ifdef ZTS
#define APM_G(v) TSRMG(apm_globals_id, zend_apm_globals *, v)
#else
#define APM_G(v) (apm_globals.v)
#endif

typedef struct {
	char *file;
	long line;
	long type;
	long ts;
	char *message;
	char *stacktrace;
	long ip;
	char *cookies;
	char *host;
	char *uri;
} apm_event_info;

#define SEC_TO_USEC(sec) ((sec) * 1000000.00)
#define USEC_TO_SEC(usec) ((usec) / 1000000.00)

#endif

