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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/* gettimeofday */
#ifdef PHP_WIN32
# include "win32/time.h"
#else
# include "main/php_config.h"
#endif

#ifdef HAVE_GETRUSAGE
# include <sys/resource.h>
#endif

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#include "php.h"
#include "php_ini.h"
#include "zend_exceptions.h"
#include "php_apm.h"
#include "backtrace.h"
#include "ext/standard/info.h"
#ifdef APM_DRIVER_SQLITE3
# include "driver_sqlite3.h"
#endif
#ifdef APM_DRIVER_MYSQL
# include "driver_mysql.h"
#endif
#ifdef APM_DRIVER_STATSD
# include "driver_statsd.h"
#endif
#ifdef APM_DRIVER_SOCKET
# include "driver_socket.h"
#endif
#ifdef APM_DRIVER_HTTP
  #include "driver_http.h"
#endif

ZEND_DECLARE_MODULE_GLOBALS(apm);
static PHP_GINIT_FUNCTION(apm);
static PHP_GSHUTDOWN_FUNCTION(apm);

#define APM_DRIVER_BEGIN_LOOP driver_entry = APM_G(drivers); \
		while ((driver_entry = driver_entry->next) != NULL) {

static user_opcode_handler_t _orig_begin_silence_opcode_handler = NULL;
static user_opcode_handler_t _orig_end_silence_opcode_handler = NULL;

#if PHP_VERSION_ID >= 70000
# define ZEND_USER_OPCODE_HANDLER_ARGS zend_execute_data *execute_data
# define ZEND_USER_OPCODE_HANDLER_ARGS_PASSTHRU execute_data
#else
# define ZEND_USER_OPCODE_HANDLER_ARGS ZEND_OPCODE_HANDLER_ARGS
# define ZEND_USER_OPCODE_HANDLER_ARGS_PASSTHRU ZEND_OPCODE_HANDLER_ARGS_PASSTHRU
#endif

static int apm_begin_silence_opcode_handler(ZEND_USER_OPCODE_HANDLER_ARGS)
{
	APM_G(currently_silenced) = 1;

	if (_orig_begin_silence_opcode_handler) {
		_orig_begin_silence_opcode_handler(ZEND_USER_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	return ZEND_USER_OPCODE_DISPATCH;
}

static int apm_end_silence_opcode_handler(ZEND_USER_OPCODE_HANDLER_ARGS)
{
	APM_G(currently_silenced) = 0;

	if (_orig_end_silence_opcode_handler) {
		_orig_end_silence_opcode_handler(ZEND_USER_OPCODE_HANDLER_ARGS_PASSTHRU);
	}

	return ZEND_USER_OPCODE_DISPATCH;
}

int apm_write(const char *str,
#if PHP_VERSION_ID >= 70000
size_t
#else
uint
#endif
length)
{
	TSRMLS_FETCH();
	smart_str_appendl(APM_G(buffer), str, length);
	smart_str_0(APM_G(buffer));
	return length;
}

void (*old_error_cb)(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);

void apm_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args);

void apm_throw_exception_hook(zval *exception TSRMLS_DC);

static void process_event(int, int, char *, uint, char * TSRMLS_DC);

/* recorded timestamp for the request */
struct timeval begin_tp;
#ifdef HAVE_GETRUSAGE
struct rusage begin_usg;
#endif

zend_module_entry apm_module_entry = {
	STANDARD_MODULE_HEADER,
	"apm",
	NULL,
	PHP_MINIT(apm),
	PHP_MSHUTDOWN(apm),
	PHP_RINIT(apm),
	PHP_RSHUTDOWN(apm),
	PHP_MINFO(apm),
	PHP_APM_VERSION,
	PHP_MODULE_GLOBALS(apm),
	PHP_GINIT(apm),
	PHP_GSHUTDOWN(apm),
	NULL,
	STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_APM
ZEND_GET_MODULE(apm)
#endif

PHP_INI_BEGIN()
	/* Boolean controlling whether the extension is globally active or not */
	STD_PHP_INI_BOOLEAN("apm.enabled", "1", PHP_INI_SYSTEM, OnUpdateBool, enabled, zend_apm_globals, apm_globals)
	/* Application identifier, helps identifying which application is being monitored */
	STD_PHP_INI_ENTRY("apm.application_id", "My application", PHP_INI_ALL, OnUpdateString, application_id, zend_apm_globals, apm_globals)
	/* Boolean controlling whether the event monitoring is active or not */
	STD_PHP_INI_BOOLEAN("apm.event_enabled", "1", PHP_INI_ALL, OnUpdateBool, event_enabled, zend_apm_globals, apm_globals)
	/* Boolean controlling whether the stacktrace should be stored or not */
	STD_PHP_INI_BOOLEAN("apm.store_stacktrace", "1", PHP_INI_ALL, OnUpdateBool, store_stacktrace, zend_apm_globals, apm_globals)
	/* Boolean controlling whether the ip should be stored or not */
	STD_PHP_INI_BOOLEAN("apm.store_ip", "1", PHP_INI_ALL, OnUpdateBool, store_ip, zend_apm_globals, apm_globals)
	/* Boolean controlling whether the cookies should be stored or not */
	STD_PHP_INI_BOOLEAN("apm.store_cookies", "1", PHP_INI_ALL, OnUpdateBool, store_cookies, zend_apm_globals, apm_globals)
	/* Boolean controlling whether the POST variables should be stored or not */
	STD_PHP_INI_BOOLEAN("apm.store_post", "1", PHP_INI_ALL, OnUpdateBool, store_post, zend_apm_globals, apm_globals)
	/* Time (in ms) before a request is considered for stats */
	STD_PHP_INI_ENTRY("apm.stats_duration_threshold", "100", PHP_INI_ALL, OnUpdateLong, stats_duration_threshold, zend_apm_globals, apm_globals)
#ifdef HAVE_GETRUSAGE
	/* User CPU time usage (in ms) before a request is considered for stats */
	STD_PHP_INI_ENTRY("apm.stats_user_cpu_threshold", "100", PHP_INI_ALL, OnUpdateLong, stats_user_cpu_threshold, zend_apm_globals, apm_globals)
	/* System CPU time usage (in ms) before a request is considered for stats */
	STD_PHP_INI_ENTRY("apm.stats_sys_cpu_threshold", "10", PHP_INI_ALL, OnUpdateLong, stats_sys_cpu_threshold, zend_apm_globals, apm_globals)
#endif
	/* Maximum recursion depth used when dumping a variable */
	STD_PHP_INI_ENTRY("apm.dump_max_depth", "1", PHP_INI_ALL, OnUpdateLong, dump_max_depth, zend_apm_globals, apm_globals)

#ifdef APM_DRIVER_SQLITE3
	/* Boolean controlling whether the driver is active or not */
	STD_PHP_INI_BOOLEAN("apm.sqlite_enabled", "1", PHP_INI_PERDIR, OnUpdateBool, sqlite3_enabled, zend_apm_globals, apm_globals)
	/* Boolean controlling the collection of stats */
	STD_PHP_INI_BOOLEAN("apm.sqlite_stats_enabled", "0", PHP_INI_ALL, OnUpdateBool, sqlite3_stats_enabled, zend_apm_globals, apm_globals)
	/* Control which exceptions to collect (0: none exceptions collected, 1: collect uncaught exceptions (default), 2: collect ALL exceptions) */
	STD_PHP_INI_ENTRY("apm.sqlite_exception_mode", "1", PHP_INI_PERDIR, OnUpdateLongGEZero, sqlite3_exception_mode, zend_apm_globals, apm_globals)
	/* error_reporting of the driver */
	STD_PHP_INI_ENTRY("apm.sqlite_error_reporting", NULL, PHP_INI_ALL, OnUpdateAPMsqlite3ErrorReporting, sqlite3_error_reporting, zend_apm_globals, apm_globals)
	/* Path to the SQLite database file */
	STD_PHP_INI_ENTRY("apm.sqlite_max_event_insert_timeout", "100", PHP_INI_ALL, OnUpdateLong, sqlite3_timeout, zend_apm_globals, apm_globals)
	/* Max timeout to wait for storing the event in the DB */
	STD_PHP_INI_ENTRY("apm.sqlite_db_path", SQLITE3_DEFAULTDB, PHP_INI_ALL, OnUpdateDBFile, sqlite3_db_path, zend_apm_globals, apm_globals)
	/* Store silenced events? */
	STD_PHP_INI_BOOLEAN("apm.sqlite_process_silenced_events", "1", PHP_INI_PERDIR, OnUpdateBool, sqlite3_process_silenced_events, zend_apm_globals, apm_globals)
#endif

#ifdef APM_DRIVER_MYSQL
	/* Boolean controlling whether the driver is active or not */
	STD_PHP_INI_BOOLEAN("apm.mysql_enabled", "1", PHP_INI_PERDIR, OnUpdateBool, mysql_enabled, zend_apm_globals, apm_globals)
	/* Boolean controlling the collection of stats */
	STD_PHP_INI_BOOLEAN("apm.mysql_stats_enabled", "0", PHP_INI_ALL, OnUpdateBool, mysql_stats_enabled, zend_apm_globals, apm_globals)
	/* Control which exceptions to collect (0: none exceptions collected, 1: collect uncaught exceptions (default), 2: collect ALL exceptions) */
	STD_PHP_INI_ENTRY("apm.mysql_exception_mode","1", PHP_INI_PERDIR, OnUpdateLongGEZero, mysql_exception_mode, zend_apm_globals, apm_globals)
	/* error_reporting of the driver */
	STD_PHP_INI_ENTRY("apm.mysql_error_reporting", NULL, PHP_INI_ALL, OnUpdateAPMmysqlErrorReporting, mysql_error_reporting, zend_apm_globals, apm_globals)
	/* mysql host */
	STD_PHP_INI_ENTRY("apm.mysql_host", "localhost", PHP_INI_PERDIR, OnUpdateString, mysql_db_host, zend_apm_globals, apm_globals)
	/* mysql port */
	STD_PHP_INI_ENTRY("apm.mysql_port", "0", PHP_INI_PERDIR, OnUpdateLong, mysql_db_port, zend_apm_globals, apm_globals)
	/* mysql user */
	STD_PHP_INI_ENTRY("apm.mysql_user", "root", PHP_INI_PERDIR, OnUpdateString, mysql_db_user, zend_apm_globals, apm_globals)
	/* mysql password */
	STD_PHP_INI_ENTRY("apm.mysql_pass", "", PHP_INI_PERDIR, OnUpdateString, mysql_db_pass, zend_apm_globals, apm_globals)
	/* mysql database */
	STD_PHP_INI_ENTRY("apm.mysql_db", "apm", PHP_INI_PERDIR, OnUpdateString, mysql_db_name, zend_apm_globals, apm_globals)
	/* process silenced events? */
	STD_PHP_INI_BOOLEAN("apm.mysql_process_silenced_events", "1", PHP_INI_PERDIR, OnUpdateBool, mysql_process_silenced_events, zend_apm_globals, apm_globals)
#endif

#ifdef APM_DRIVER_STATSD
	/* Boolean controlling whether the driver is active or not */
	STD_PHP_INI_BOOLEAN("apm.statsd_enabled", "1", PHP_INI_PERDIR, OnUpdateBool, statsd_enabled, zend_apm_globals, apm_globals)
	/* Boolean controlling the collection of stats */
	STD_PHP_INI_BOOLEAN("apm.statsd_stats_enabled", "1", PHP_INI_ALL, OnUpdateBool, statsd_stats_enabled, zend_apm_globals, apm_globals)
	/* Control which exceptions to collect (0: none exceptions collected, 1: collect uncaught exceptions (default), 2: collect ALL exceptions) */
	STD_PHP_INI_ENTRY("apm.statsd_exception_mode","1", PHP_INI_PERDIR, OnUpdateLongGEZero, statsd_exception_mode, zend_apm_globals, apm_globals)
	/* error_reporting of the driver */
	STD_PHP_INI_ENTRY("apm.statsd_error_reporting", NULL, PHP_INI_ALL, OnUpdateAPMstatsdErrorReporting, statsd_error_reporting, zend_apm_globals, apm_globals)
	/* StatsD host */
	STD_PHP_INI_ENTRY("apm.statsd_host", "localhost", PHP_INI_PERDIR, OnUpdateString, statsd_host, zend_apm_globals, apm_globals)
	/* StatsD port */
	STD_PHP_INI_ENTRY("apm.statsd_port", "8125", PHP_INI_PERDIR, OnUpdateLong, statsd_port, zend_apm_globals, apm_globals)
	/* StatsD port */
	STD_PHP_INI_ENTRY("apm.statsd_prefix", "apm", PHP_INI_ALL, OnUpdateString, statsd_prefix, zend_apm_globals, apm_globals)
#endif

#ifdef APM_DRIVER_SOCKET
	/* Boolean controlling whether the driver is active or not */
	STD_PHP_INI_BOOLEAN("apm.socket_enabled", "1", PHP_INI_ALL, OnUpdateBool, socket_enabled, zend_apm_globals, apm_globals)
	/* Boolean controlling the collection of stats */
	STD_PHP_INI_BOOLEAN("apm.socket_stats_enabled", "1", PHP_INI_ALL, OnUpdateBool, socket_stats_enabled, zend_apm_globals, apm_globals)
	/* Control which exceptions to collect (0: none exceptions collected, 1: collect uncaught exceptions (default), 2: collect ALL exceptions) */
	STD_PHP_INI_ENTRY("apm.socket_exception_mode","1", PHP_INI_PERDIR, OnUpdateLongGEZero, socket_exception_mode, zend_apm_globals, apm_globals)
	/* error_reporting of the driver */
	STD_PHP_INI_ENTRY("apm.socket_error_reporting", NULL, PHP_INI_ALL, OnUpdateAPMsocketErrorReporting, socket_error_reporting, zend_apm_globals, apm_globals)
	/* Socket path */
	STD_PHP_INI_ENTRY("apm.socket_path", "file:/tmp/apm.sock|tcp:127.0.0.1:8264", PHP_INI_ALL, OnUpdateString, socket_path, zend_apm_globals, apm_globals)
	/* process silenced events? */
	STD_PHP_INI_BOOLEAN("apm.socket_process_silenced_events", "1", PHP_INI_PERDIR, OnUpdateBool, socket_process_silenced_events, zend_apm_globals, apm_globals)
#endif

#ifdef APM_DRIVER_HTTP
	/* Boolean controlling whether the driver is active or not */
	STD_PHP_INI_BOOLEAN("apm.http_enabled", "1", PHP_INI_ALL, OnUpdateBool, http_enabled, zend_apm_globals, apm_globals)
	/* Boolean controlling the collection of stats */
	STD_PHP_INI_BOOLEAN("apm.http_stats_enabled", "1", PHP_INI_ALL, OnUpdateBool, http_stats_enabled, zend_apm_globals, apm_globals)
	/* Control which exceptions to collect (0: none exceptions collected, 1: collect uncaught exceptions (default), 2: collect ALL exceptions) */
	STD_PHP_INI_ENTRY("apm.http_exception_mode","1", PHP_INI_PERDIR, OnUpdateLongGEZero, http_exception_mode, zend_apm_globals, apm_globals)
	/* error_reporting of the driver */
	STD_PHP_INI_ENTRY("apm.http_error_reporting", NULL, PHP_INI_ALL, OnUpdateAPMhttpErrorReporting, http_error_reporting, zend_apm_globals, apm_globals)
	/* process silenced events? */
	STD_PHP_INI_BOOLEAN("apm.http_process_silenced_events", "1", PHP_INI_PERDIR, OnUpdateBool, http_process_silenced_events, zend_apm_globals, apm_globals)
	STD_PHP_INI_ENTRY("apm.http_request_timeout", "1000", PHP_INI_ALL, OnUpdateLong, http_request_timeout, zend_apm_globals, apm_globals)
	STD_PHP_INI_ENTRY("apm.http_server", "http://localhost", PHP_INI_ALL, OnUpdateString, http_server, zend_apm_globals, apm_globals)
	STD_PHP_INI_ENTRY("apm.http_client_certificate", NULL, PHP_INI_ALL, OnUpdateString, http_client_certificate, zend_apm_globals, apm_globals)
	STD_PHP_INI_ENTRY("apm.http_client_key", NULL, PHP_INI_ALL, OnUpdateString, http_client_key, zend_apm_globals, apm_globals)
	STD_PHP_INI_ENTRY("apm.http_certificate_authorities", NULL, PHP_INI_ALL, OnUpdateString, http_certificate_authorities, zend_apm_globals, apm_globals)
	STD_PHP_INI_ENTRY("apm.http_max_backtrace_length", "0", PHP_INI_ALL, OnUpdateLong, http_max_backtrace_length, zend_apm_globals, apm_globals)
#endif
PHP_INI_END()

static PHP_GINIT_FUNCTION(apm)
{
	apm_driver_entry **next;
	apm_globals->buffer = NULL;
	apm_globals->drivers = (apm_driver_entry *) malloc(sizeof(apm_driver_entry));
	apm_globals->drivers->driver.process_event = (void (*)(PROCESS_EVENT_ARGS)) NULL;
	apm_globals->drivers->driver.process_stats = (void (*)(TSRMLS_D)) NULL;
	apm_globals->drivers->driver.minit = (int (*)(int TSRMLS_DC)) NULL;
	apm_globals->drivers->driver.rinit = (int (*)(TSRMLS_D)) NULL;
	apm_globals->drivers->driver.mshutdown = (int (*)()) NULL;
	apm_globals->drivers->driver.rshutdown = (int (*)(TSRMLS_D)) NULL;

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
#ifdef APM_DRIVER_STATSD
	*next = apm_driver_statsd_create();
	next = &(*next)->next;
#endif
#ifdef APM_DRIVER_SOCKET
	*next = apm_driver_socket_create();
	next = &(*next)->next;
#endif
#ifdef APM_DRIVER_HTTP
	*next = apm_driver_http_create();
#endif
}

static void recursive_free_driver(apm_driver_entry **driver)
{
	if ((*driver)->next) {
		recursive_free_driver(&(*driver)->next);
	}
	free(*driver);
}

static PHP_GSHUTDOWN_FUNCTION(apm)
{
	recursive_free_driver(&apm_globals->drivers);
}

PHP_MINIT_FUNCTION(apm)
{
	apm_driver_entry * driver_entry;

	REGISTER_INI_ENTRIES();

	/* Storing actual error callback function for later restore */
	old_error_cb = zend_error_cb;

	/* Initialize the storage drivers */
	if (APM_G(enabled)) {

		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL) {
			if (driver_entry->driver.minit(module_number TSRMLS_CC) == FAILURE) {
				return FAILURE;
			}
		}

		/* Since xdebug looks for zend_error_cb in his MINIT, we change it once more so he can get our address */
		zend_error_cb = apm_error_cb;
		zend_throw_exception_hook = apm_throw_exception_hook;

	}

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(apm)
{
	apm_driver_entry * driver_entry;

	UNREGISTER_INI_ENTRIES();

	if (APM_G(enabled)) {
		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL) {
			if (driver_entry->driver.mshutdown(SHUTDOWN_FUNC_ARGS_PASSTHRU) == FAILURE) {
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
	apm_driver_entry * driver_entry;

	APM_INIT_DEBUG;
	if (APM_G(enabled)) {
		/* Overload the ZEND_BEGIN_SILENCE / ZEND_END_SILENCE opcodes */
		_orig_begin_silence_opcode_handler = zend_get_user_opcode_handler(ZEND_BEGIN_SILENCE);
		zend_set_user_opcode_handler(ZEND_BEGIN_SILENCE, apm_begin_silence_opcode_handler);

		_orig_end_silence_opcode_handler = zend_get_user_opcode_handler(ZEND_END_SILENCE);
		zend_set_user_opcode_handler(ZEND_END_SILENCE, apm_end_silence_opcode_handler);

		memset(&APM_G(request_data), 0, sizeof(struct apm_request_data));
		if (APM_G(event_enabled)) {
			/* storing timestamp of request */
			gettimeofday(&begin_tp, NULL);
#ifdef HAVE_GETRUSAGE
			memset(&begin_usg, 0, sizeof(struct rusage));
			getrusage(RUSAGE_SELF, &begin_usg);
#endif
		}

		APM_DEBUG("Registering drivers\n");
		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL) {
			if (driver_entry->driver.is_enabled(TSRMLS_C)) {
				if (driver_entry->driver.rinit(TSRMLS_C)) {
					return FAILURE;
				}
			}
		}

		APM_DEBUG("Replacing handlers\n");
		/* Replacing current error callback function with apm's one */
		zend_error_cb = apm_error_cb;
		zend_throw_exception_hook = apm_throw_exception_hook;
	}
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(apm)
{
	apm_driver_entry * driver_entry;
#ifdef HAVE_GETRUSAGE
	struct rusage usg;
#endif
	struct timeval end_tp;
	zend_bool stats_enabled = 0;
	int code = SUCCESS;

	if (APM_G(enabled)) {
		zend_set_user_opcode_handler(ZEND_BEGIN_SILENCE, _orig_begin_silence_opcode_handler);
		zend_set_user_opcode_handler(ZEND_END_SILENCE, _orig_end_silence_opcode_handler);

		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL && stats_enabled == 0) {
			stats_enabled = driver_entry->driver.want_stats(TSRMLS_C);
		}

		if (stats_enabled) {
			gettimeofday(&end_tp, NULL);

			/* Request longer than accepted threshold ? */
			APM_G(duration) = (float) (SEC_TO_USEC(end_tp.tv_sec - begin_tp.tv_sec) + end_tp.tv_usec - begin_tp.tv_usec);
			APM_G(mem_peak_usage) = zend_memory_peak_usage(1 TSRMLS_CC);
#ifdef HAVE_GETRUSAGE
			memset(&usg, 0, sizeof(struct rusage));

			if (getrusage(RUSAGE_SELF, &usg) == 0) {
				APM_G(user_cpu) = (float) (SEC_TO_USEC(usg.ru_utime.tv_sec - begin_usg.ru_utime.tv_sec) + usg.ru_utime.tv_usec - begin_usg.ru_utime.tv_usec);
				APM_G(sys_cpu) = (float) (SEC_TO_USEC(usg.ru_stime.tv_sec - begin_usg.ru_stime.tv_sec) + usg.ru_stime.tv_usec - begin_usg.ru_stime.tv_usec);
			}
#endif
			if (
				APM_G(duration) > 1000.0 * APM_G(stats_duration_threshold)
#ifdef HAVE_GETRUSAGE
				|| APM_G(user_cpu) > 1000.0 * APM_G(stats_user_cpu_threshold)
				|| APM_G(sys_cpu) > 1000.0 * APM_G(stats_sys_cpu_threshold)
#endif
				) {

				driver_entry = APM_G(drivers);
				APM_DEBUG("Stats loop begin\n");
				while ((driver_entry = driver_entry->next) != NULL) {
					if (driver_entry->driver.want_stats(TSRMLS_C)) {
						driver_entry->driver.process_stats(TSRMLS_C);
					}
				}
				APM_DEBUG("Stats loop end\n");
			}
		}

		driver_entry = APM_G(drivers);
		while ((driver_entry = driver_entry->next) != NULL) {
			if (driver_entry->driver.is_enabled(TSRMLS_C)) {
				if (driver_entry->driver.rshutdown(TSRMLS_C) == FAILURE) {
					code = FAILURE;
				}
			}
		}

		/* Restoring saved error callback function */
		APM_DEBUG("Restoring handlers\n");
		zend_error_cb = old_error_cb;
		zend_throw_exception_hook = NULL;

		smart_str_free(&APM_RD(cookies));
		smart_str_free(&APM_RD(post_vars));
	}

	APM_SHUTDOWN_DEBUG;

	return code;
}

PHP_MINFO_FUNCTION(apm)
{
	php_info_print_table_start();
	php_info_print_table_row(2, "APM support", "enabled");
	php_info_print_table_row(2, "Version", PHP_APM_VERSION);
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

/* {{{ void apm_error(int type, const char *format, ...)
	This function provides a hook for error */
void apm_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
	char *msg;
	va_list args_copy;
	TSRMLS_FETCH();

	/* A copy of args is needed to be used for the old_error_cb */
	va_copy(args_copy, args);
	vspprintf(&msg, 0, format, args_copy);
	va_end(args_copy);

	if (APM_G(event_enabled)) {
		process_event(APM_EVENT_ERROR, type, (char *) error_filename, error_lineno, msg TSRMLS_CC);
	}
	efree(msg);

	old_error_cb(type, error_filename, error_lineno, format, args);
}
/* }}} */


void apm_throw_exception_hook(zval *exception TSRMLS_DC)
{
	zval *message, *file, *line;
#if PHP_VERSION_ID >= 70000
	zval rv;
#endif
	zend_class_entry *default_ce;

	if (APM_G(event_enabled)) {
		if (!exception) {
			return;
		}

		default_ce = zend_exception_get_default(TSRMLS_C);

#if PHP_VERSION_ID >= 70000
		message = zend_read_property(default_ce, exception, "message", sizeof("message")-1, 0, &rv);
		file = zend_read_property(default_ce, exception, "file", sizeof("file")-1, 0, &rv);
		line = zend_read_property(default_ce, exception, "line", sizeof("line")-1, 0, &rv);
#else
		message = zend_read_property(default_ce, exception, "message", sizeof("message")-1, 0 TSRMLS_CC);
		file = zend_read_property(default_ce, exception, "file", sizeof("file")-1, 0 TSRMLS_CC);
		line = zend_read_property(default_ce, exception, "line", sizeof("line")-1, 0 TSRMLS_CC);
#endif

		process_event(APM_EVENT_EXCEPTION, E_EXCEPTION, Z_STRVAL_P(file), Z_LVAL_P(line), Z_STRVAL_P(message) TSRMLS_CC);
	}
}

/* Insert an event in the backend */
static void process_event(int event_type, int type, char * error_filename, uint error_lineno, char * msg TSRMLS_DC)
{
	smart_str trace_str = {0};
	apm_driver_entry * driver_entry;

	if (APM_G(store_stacktrace)) {
		append_backtrace(&trace_str TSRMLS_CC);
		smart_str_0(&trace_str);
	}

	driver_entry = APM_G(drivers);
	APM_DEBUG("Direct processing process_event loop begin\n");
	while ((driver_entry = driver_entry->next) != NULL) {
		if (driver_entry->driver.want_event(event_type, type, msg TSRMLS_CC)) {
			driver_entry->driver.process_event(
				type,
				error_filename,
				error_lineno,
				msg,
#if PHP_VERSION_ID >= 70000
				(APM_G(store_stacktrace) && trace_str.s && trace_str.s->val) ? trace_str.s->val : ""
#else
				(APM_G(store_stacktrace) && trace_str.c) ? trace_str.c : ""
#endif
				TSRMLS_CC
			);
		}
	}
	APM_DEBUG("Direct processing process_event loop end\n");

	smart_str_free(&trace_str);
}

#if PHP_VERSION_ID >= 70000
#define REGISTER_INFO(name, dest, type) \
	if ((APM_RD(dest) = zend_hash_str_find(Z_ARRVAL_P(tmp), name, sizeof(name) - 1)) && (Z_TYPE_P(APM_RD(dest)) == (type))) { \
		APM_RD(dest##_found) = 1; \
	}
#else
#define REGISTER_INFO(name, dest, type) \
	if ((zend_hash_find(Z_ARRVAL_P(tmp), name, sizeof(name), (void**)&APM_RD(dest)) == SUCCESS) && (Z_TYPE_PP(APM_RD(dest)) == (type))) { \
		APM_RD(dest##_found) = 1; \
	}
#endif

#if PHP_VERSION_ID >= 70000
#define FETCH_HTTP_GLOBALS(name) (tmp = &PG(http_globals)[TRACK_VARS_##name])
#else
#define FETCH_HTTP_GLOBALS(name) (tmp = PG(http_globals)[TRACK_VARS_##name])
#endif

void extract_data(TSRMLS_D)
{
	zval *tmp;

	APM_DEBUG("Extracting data\n");
	
	if (APM_RD(initialized)) {
		APM_DEBUG("Data already initialized\n");
		return;
	}

	APM_RD(initialized) = 1;
	
	zend_is_auto_global_compat("_SERVER");
	if (FETCH_HTTP_GLOBALS(SERVER)) {
		REGISTER_INFO("REQUEST_URI", uri, IS_STRING);
		REGISTER_INFO("HTTP_HOST", host, IS_STRING);
		REGISTER_INFO("HTTP_REFERER", referer, IS_STRING);
		REGISTER_INFO("REQUEST_TIME", ts, IS_LONG);
		REGISTER_INFO("SCRIPT_FILENAME", script, IS_STRING);
		REGISTER_INFO("REQUEST_METHOD", method, IS_STRING);
		
		if (APM_G(store_ip)) {
			REGISTER_INFO("REMOTE_ADDR", ip, IS_STRING);
		}
	}
	if (APM_G(store_cookies)) {
		zend_is_auto_global_compat("_COOKIE");
		if (FETCH_HTTP_GLOBALS(COOKIE)) {
			if (Z_ARRVAL_P(tmp)->nNumOfElements > 0) {
				APM_G(buffer) = &APM_RD(cookies);
				zend_print_zval_r_ex(apm_write, tmp, 0 TSRMLS_CC);
				APM_RD(cookies_found) = 1;
			}
		}
	}
	if (APM_G(store_post)) {
		zend_is_auto_global_compat("_POST");
		if (FETCH_HTTP_GLOBALS(POST)) {
			if (Z_ARRVAL_P(tmp)->nNumOfElements > 0) {
				APM_G(buffer) = &APM_RD(post_vars);
				zend_print_zval_r_ex(apm_write, tmp, 0 TSRMLS_CC);
				APM_RD(post_vars_found) = 1;
			}
		}
	}
}
