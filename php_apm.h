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

#define PHP_APM_VERSION "2.1.1"

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "php.h"
#include "zend_errors.h"

#if PHP_VERSION_ID >= 70000
# include "zend_smart_str.h"
#else
# include "ext/standard/php_smart_str.h"
#endif

#ifndef E_EXCEPTION
# define E_EXCEPTION (1<<15L)
#endif

#ifdef APM_DRIVER_SQLITE3
	#include <sqlite3.h>
#endif
#ifdef APM_DRIVER_MYSQL
	#include <mysql/mysql.h>
#endif

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
	void (* process_stats)(TSRMLS_D);
	int (* minit)(int TSRMLS_DC);
	int (* rinit)(TSRMLS_D);
	int (* mshutdown)(SHUTDOWN_FUNC_ARGS);
	int (* rshutdown)(TSRMLS_D);
	zend_bool (* is_enabled)(TSRMLS_D);
	zend_bool (* want_event)(int, int, char * TSRMLS_DC);
	zend_bool (* want_stats)(TSRMLS_D);
	int (* error_reporting)(TSRMLS_D);
	zend_bool is_request_created;
} apm_driver;

typedef struct apm_driver_entry {
	apm_driver driver;
	struct apm_driver_entry *next;
} apm_driver_entry;

#if PHP_VERSION_ID >= 70000
# define RD_DEF(var) zval *var; zend_bool var##_found;
#else
# define RD_DEF(var) zval **var; zend_bool var##_found;
#endif

typedef struct apm_request_data {
	RD_DEF(uri);
	RD_DEF(host);
	RD_DEF(ip);
	RD_DEF(referer);
	RD_DEF(ts);
	RD_DEF(script);
	RD_DEF(method);

	zend_bool initialized, cookies_found, post_vars_found;
	smart_str cookies, post_vars;
} apm_request_data;


#ifdef ZTS
#define APM_GLOBAL(driver, v) TSRMG(apm_globals_id, zend_apm_globals *, driver##_##v)
#else
#define APM_GLOBAL(driver, v) (apm_globals.driver##_##v)
#endif

#if PHP_VERSION_ID >= 70000
# define apm_error_reporting_new_value (new_value && new_value->val) ? atoi(new_value->val)
#else
# define apm_error_reporting_new_value new_value ? atoi(new_value)
#endif

#define APM_DRIVER_CREATE(name) \
void apm_driver_##name##_process_event(PROCESS_EVENT_ARGS); \
void apm_driver_##name##_process_stats(TSRMLS_D); \
int apm_driver_##name##_minit(int TSRMLS_DC); \
int apm_driver_##name##_rinit(TSRMLS_D); \
int apm_driver_##name##_mshutdown(); \
int apm_driver_##name##_rshutdown(TSRMLS_D); \
PHP_INI_MH(OnUpdateAPM##name##ErrorReporting) \
{ \
	APM_GLOBAL(name, error_reporting) = (apm_error_reporting_new_value : APM_E_##name); \
	return SUCCESS; \
} \
zend_bool apm_driver_##name##_is_enabled(TSRMLS_D) \
{ \
	return APM_GLOBAL(name, enabled); \
} \
int apm_driver_##name##_error_reporting(TSRMLS_D) \
{ \
	return APM_GLOBAL(name, error_reporting); \
} \
zend_bool apm_driver_##name##_want_event(int event_type, int error_level, char *msg TSRMLS_DC) \
{ \
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
zend_bool apm_driver_##name##_want_stats(TSRMLS_D) \
{ \
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

	float duration;

	long mem_peak_usage;
#ifdef HAVE_GETRUSAGE
	float user_cpu;

	float sys_cpu;
#endif

#ifdef APM_DEBUGFILE
	FILE * debugfile;
#endif

#ifdef APM_DRIVER_SQLITE3
	/* Boolean controlling whether the driver is active or not */
	zend_bool sqlite3_enabled;
	/* Boolean controlling the collection of stats */
	zend_bool sqlite3_stats_enabled;
	/* Control which exceptions to collect (0: none exceptions collected, 1: collect uncaught exceptions (default), 2: collect ALL exceptions) */
	long sqlite3_exception_mode;
	/* driver error reporting */
	int sqlite3_error_reporting;
	/* Path to the SQLite database file */
	char *sqlite3_db_path;
	/* The actual db file */
	char sqlite3_db_file[MAXPATHLEN];
	/* DB handle */
	sqlite3 *sqlite3_event_db;
	/* Max timeout to wait for storing the event in the DB */
	long sqlite3_timeout;
	/* Request ID */
	sqlite3_int64 sqlite3_request_id;
	/* Boolean to ensure request content is only inserted once */
	zend_bool sqlite3_is_request_created;
	/* Option to process silenced events */
	zend_bool sqlite3_process_silenced_events;
#endif

#ifdef APM_DRIVER_MYSQL
	/* Boolean controlling whether the driver is active or not */
	zend_bool mysql_enabled;
	/* Boolean controlling the collection of stats */
	zend_bool mysql_stats_enabled;
	/* Control which exceptions to collect (0: none exceptions collected, 1: collect uncaught exceptions (default), 2: collect ALL exceptions) */
	long mysql_exception_mode;
	/* driver error reporting */
	int mysql_error_reporting;
	/* MySQL host */
	char *mysql_db_host;
	/* MySQL port */
	unsigned int mysql_db_port;
	/* MySQL user */
	char *mysql_db_user;
	/* MySQL password */
	char *mysql_db_pass;
	/* MySQL database */
	char *mysql_db_name;
	/* DB handle */
	MYSQL *mysql_event_db;
	/* Option to process silenced events */
	zend_bool mysql_process_silenced_events;

	/* Boolean to ensure request content is only inserted once */
	zend_bool mysql_is_request_created;
#endif

#ifdef APM_DRIVER_STATSD
	/* Boolean controlling whether the driver is active or not */
	zend_bool statsd_enabled;
	/* Boolean controlling the collection of stats */
	zend_bool statsd_stats_enabled;
	/* (unused for StatsD) */
	long statsd_exception_mode;
	/* (unused for StatsD) */
	int statsd_error_reporting;
	/* StatsD host */
	char *statsd_host;
	/* StatsD port */
	unsigned int statsd_port;
	/* StatsD key prefix */
	char *statsd_prefix;
	/* addinfo for StatsD server */
	struct addrinfo *statsd_servinfo;
	/* Option to process silenced events */
	zend_bool statsd_process_silenced_events;
#endif

#ifdef APM_DRIVER_SOCKET
	/* Boolean controlling whether the driver is active or not */
	zend_bool socket_enabled;
	/* Boolean controlling the collection of stats */
	zend_bool socket_stats_enabled;
	/* (unused for socket) */
	long socket_exception_mode;
	/* (unused for socket) */
	int socket_error_reporting;
	/* Option to process silenced events */
	zend_bool socket_process_silenced_events;
	/* socket path */
	char *socket_path;
	apm_event_entry *socket_events;
	apm_event_entry **socket_last_event;
#endif

#ifdef APM_DRIVER_HTTP
	/* Boolean controlling whether the driver is active or not */
	zend_bool http_enabled;
	/* Boolean controlling the collection of stats */
	zend_bool http_stats_enabled;
	/* (unused for HTTP) */
	long http_exception_mode;
	/* (unused for HTTP) */
	int http_error_reporting;
	/* Option to process silenced events */
	zend_bool http_process_silenced_events;

	long http_request_timeout;
	char *http_server;
	char *http_client_certificate;
	char *http_client_key;
	char *http_certificate_authorities;
	long http_max_backtrace_length;
#endif

ZEND_END_MODULE_GLOBALS(apm)

#ifdef ZTS
#define APM_G(v) TSRMG(apm_globals_id, zend_apm_globals *, v)
#else
#define APM_G(v) (apm_globals.v)
#endif

#define APM_RD(data) APM_G(request_data).data

#if PHP_VERSION_ID >= 70000
# define APM_RD_STRVAL(var) Z_STRVAL_P(APM_RD(var))
# define APM_RD_SMART_STRVAL(var) APM_RD(var).s->val
#else
# define APM_RD_STRVAL(var) Z_STRVAL_PP(APM_RD(var))
# define APM_RD_SMART_STRVAL(var) APM_RD(var).c
#endif

#define SEC_TO_USEC(sec) ((sec) * 1000000.00)
#define USEC_TO_SEC(usec) ((usec) / 1000000.00)

#if PHP_VERSION_ID >= 70000
# define zend_is_auto_global_compat(name) (zend_is_auto_global_str(ZEND_STRL((name))))
# define add_assoc_long_compat(array, key, value) add_assoc_long_ex((array), (key), (sizeof(key) - 1), (value));
#else
# define zend_is_auto_global_compat(name) (zend_is_auto_global(ZEND_STRL((name)) TSRMLS_CC))
# define add_assoc_long_compat(array, key, value) add_assoc_long_ex((array), (key), (sizeof(key)), (value));
#endif

int apm_write(const char *str,
#if PHP_VERSION_ID >= 70000
size_t
#else
uint
#endif
length);

#endif

void extract_data(TSRMLS_D);
