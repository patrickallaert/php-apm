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

#ifndef PHP_APM_H
#define PHP_APM_H

#include "config.h"
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

#define APM_E_ALL (E_ALL | E_STRICT)

#define APM_EVENT_ERROR 1
#define APM_EVENT_EXCEPTION 2

#define PROCESS_EVENT_ARGS int type, char * error_filename, uint error_lineno, char * msg, char * trace  TSRMLS_DC
#define PROCESS_STATS_ARGS float duration, float user_cpu, float sys_cpu, long mem_peak_usage TSRMLS_DC

typedef struct apm_event {
	int event_type;
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
	void (* process_event)(PROCESS_EVENT_ARGS);
	void (* process_stats)(PROCESS_STATS_ARGS);
	int (* minit)(int);
	int (* rinit)();
	int (* mshutdown)();
	int (* rshutdown)();
	zend_bool (* is_enabled)();
	zend_bool (* want_event)(int, int, char *);
	zend_bool (* want_stats)();
	int (* error_reporting)();
	zend_bool is_request_created;
} apm_driver;

typedef struct apm_driver_entry {
	apm_driver driver;
	struct apm_driver_entry *next;
} apm_driver_entry;

typedef struct apm_request_data {
	zval **uri, **host, **ip, **referer;
	zend_bool uri_found, host_found, ip_found, cookies_found, post_vars_found, referer_found;
	smart_str cookies, post_vars;
} apm_request_data;


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
zend_bool apm_driver_##name##_want_event(int event_type, int error_level, char *msg) { \
	return \
		APM_GLOBAL(name, enabled) \
		&& ( \
			(event_type == APM_EVENT_EXCEPTION && APM_GLOBAL(name, exception_mode) == 2) \
			|| \
			(event_type == APM_EVENT_ERROR && ((APM_GLOBAL(name, exception_mode) == 1) || (strncmp(msg, "Uncaught exception", 18) != 0)) && (error_level & APM_GLOBAL(name, error_reporting))) \
		) \
		&& ( \
			!APM_G(currently_silenced) || APM_GLOBAL(name, process_silenced_events) \
		) \
	; \
} \
zend_bool apm_driver_##name##_want_stats() { \
	return \
		APM_GLOBAL(name, enabled) \
		&& ( \
			APM_GLOBAL(name, stats_enabled)\
		) \
	; \
} \
apm_driver_entry * apm_driver_##name##_create() \
{ \
	apm_driver_entry * driver_entry; \
	driver_entry = (apm_driver_entry *) malloc(sizeof(apm_driver_entry)); \
	driver_entry->driver.process_event = apm_driver_##name##_process_event; \
	driver_entry->driver.minit = apm_driver_##name##_minit; \
	driver_entry->driver.rinit = apm_driver_##name##_rinit; \
	driver_entry->driver.mshutdown = apm_driver_##name##_mshutdown; \
	driver_entry->driver.rshutdown = apm_driver_##name##_rshutdown; \
	driver_entry->driver.process_stats = apm_driver_##name##_process_stats; \
	driver_entry->driver.is_enabled = apm_driver_##name##_is_enabled; \
	driver_entry->driver.error_reporting = apm_driver_##name##_error_reporting; \
	driver_entry->driver.want_event = apm_driver_##name##_want_event; \
	driver_entry->driver.want_stats = apm_driver_##name##_want_stats; \
	driver_entry->driver.is_request_created = 0; \
	driver_entry->next = NULL; \
	return driver_entry; \
}

PHP_MINIT_FUNCTION(apm);
PHP_MSHUTDOWN_FUNCTION(apm);
PHP_RINIT_FUNCTION(apm);
PHP_RSHUTDOWN_FUNCTION(apm);
PHP_MINFO_FUNCTION(apm);

#ifdef APM_DEBUGFILE
#define APM_INIT_DEBUG APM_G(debugfile) = fopen(APM_DEBUGFILE, "a+");
#define APM_DEBUG(...) if (APM_G(debugfile)) { fprintf(APM_G(debugfile), __VA_ARGS__); fflush(APM_G(debugfile)); }
#define APM_SHUTDOWN_DEBUG if (APM_G(debugfile)) { fclose(APM_G(debugfile)); APM_G(debugfile) = NULL; }
#else
#define APM_INIT_DEBUG
#define APM_DEBUG(...)
#define APM_SHUTDOWN_DEBUG
#endif

/* Extension globals */
ZEND_BEGIN_MODULE_GLOBALS(apm)
	/* Boolean controlling whether the extension is globally active or not */
	zend_bool enabled;
	/* Application identifier, helps identifying which application is being monitored */
	char      *application_id;
	/* Boolean controlling whether the event monitoring is active or not */
	zend_bool event_enabled;
	/* Boolean controlling whether the stacktrace should be generated or not */
	zend_bool store_stacktrace;
	/* Boolean controlling whether the ip should be generated or not */
	zend_bool store_ip;
	/* Boolean controlling whether the cookies should be generated or not */
	zend_bool store_cookies;
	/* Boolean controlling whether the POST variables should be generated or not */
	zend_bool store_post;
	/* Time (in ms) before a request is considered for stats */
	long      stats_duration_threshold;
	/* User CPU time usage (in ms) before a request is considered for stats */
	long      stats_user_cpu_threshold;
	/* System CPU time usage (in ms) before a request is considered for stats */
	long      stats_sys_cpu_threshold;
	/* Maximum recursion depth used when dumping a variable */
	long      dump_max_depth;
	/* Determines whether we're currently silenced */
	zend_bool currently_silenced;

	apm_driver_entry *drivers;
	smart_str *buffer;

	/* Structure used to store request data */
	apm_request_data request_data;
#ifdef APM_DEBUGFILE
	FILE * debugfile;
#endif
ZEND_END_MODULE_GLOBALS(apm)

#ifdef ZTS
#define APM_G(v) TSRMG(apm_globals_id, zend_apm_globals *, v)
#else
#define APM_G(v) (apm_globals.v)
#endif

#define APM_RD(data) APM_G(request_data).data

#define SEC_TO_USEC(sec) ((sec) * 1000000.00)
#define USEC_TO_SEC(usec) ((usec) / 1000000.00)

#if ((PHP_MAJOR_VERSION == 5) && (PHP_MINOR_VERSION < 3))
#define apm_json_encode(buf, pzval) php_json_encode(buf, pzval TSRMLS_CC);
#else
#define apm_json_encode(buf, pzval) php_json_encode(buf, pzval, 0 TSRMLS_CC);
#endif

void * get_script(char ** script_filename);

#define EXTRACT_DATA() \
zend_is_auto_global("_SERVER", sizeof("_SERVER")-1 TSRMLS_CC); \
if ((tmp = PG(http_globals)[TRACK_VARS_SERVER])) { \
	if ((zend_hash_find(Z_ARRVAL_P(tmp), "REQUEST_URI", sizeof("REQUEST_URI"), (void**)&APM_RD(uri)) == SUCCESS) && \
		(Z_TYPE_PP(APM_RD(uri)) == IS_STRING)) { \
		APM_RD(uri_found) = 1; \
	} \
	if ((zend_hash_find(Z_ARRVAL_P(tmp), "HTTP_HOST", sizeof("HTTP_HOST"), (void**)&APM_RD(host)) == SUCCESS) && \
		(Z_TYPE_PP(APM_RD(host)) == IS_STRING)) { \
		APM_RD(host_found) = 1; \
	} \
	if (APM_G(store_ip) && (zend_hash_find(Z_ARRVAL_P(tmp), "REMOTE_ADDR", sizeof("REMOTE_ADDR"), (void**)&APM_RD(ip)) == SUCCESS) && \
		(Z_TYPE_PP(APM_RD(ip)) == IS_STRING)) { \
		APM_RD(ip_found) = 1; \
	} \
	if ((zend_hash_find(Z_ARRVAL_P(tmp), "HTTP_REFERER", sizeof("HTTP_REFERER"), (void**)&APM_RD(referer)) == SUCCESS) && \
		(Z_TYPE_PP(APM_RD(referer)) == IS_STRING)) { \
		APM_RD(referer_found) = 1; \
	} \
} \
if (APM_G(store_cookies)) { \
	zend_is_auto_global("_COOKIE", sizeof("_COOKIE")-1 TSRMLS_CC); \
	if ((tmp = PG(http_globals)[TRACK_VARS_COOKIE])) { \
		if (Z_ARRVAL_P(tmp)->nNumOfElements > 0) { \
			APM_G(buffer) = &APM_RD(cookies); \
			zend_print_zval_r_ex(apm_write, tmp, 0 TSRMLS_CC); \
			APM_RD(cookies_found) = 1; \
		} \
	} \
} \
if (APM_G(store_post)) { \
	zend_is_auto_global("_POST", sizeof("_POST")-1 TSRMLS_CC); \
	if ((tmp = PG(http_globals)[TRACK_VARS_POST])) { \
		if (Z_ARRVAL_P(tmp)->nNumOfElements > 0) { \
			APM_G(buffer) = &APM_RD(post_vars); \
			zend_print_zval_r_ex(apm_write, tmp, 0 TSRMLS_CC); \
			APM_RD(post_vars_found) = 1; \
		} \
	} \
}

int apm_write(const char *str, uint length);

#endif

