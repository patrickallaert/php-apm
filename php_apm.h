/*
  +----------------------------------------------------------------------+
  |  APM stands for Alternative PHP Monitor                              |
  +----------------------------------------------------------------------+
  | Copyright (c) 2008-2009  Davide Mendolia, Patrick Allaert            |
  +----------------------------------------------------------------------+
  | This source file is subject to version 3.01 of the PHP license,      |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.php.net/license/3_01.txt                                  |
  | If you did not receive a copy of the PHP license and are unable to   |
  | obtain it through the world-wide-web, please send a note to          |
  | license@php.net so we can mail you a copy immediately.               |
  +----------------------------------------------------------------------+
*/

#ifndef PHP_APM_H
#define PHP_APM_H

#include "php.h"

extern zend_module_entry apm_module_entry;
#define phpext_apm_ptr &apm_module_entry

#ifdef PHP_WIN32
#define PHP_APM_API __declspec(dllexport)
#else
#define PHP_APM_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

#define APM_ORDER_ID 1
#define APM_ORDER_TIMESTAMP 2
#define APM_ORDER_TYPE 3
#define APM_ORDER_DURATION 3
#define APM_ORDER_FILE 4

PHP_MINIT_FUNCTION(apm);
PHP_MSHUTDOWN_FUNCTION(apm);
PHP_RINIT_FUNCTION(apm);
PHP_RSHUTDOWN_FUNCTION(apm);
PHP_MINFO_FUNCTION(apm);

PHP_FUNCTION(apm_get_events);
PHP_FUNCTION(apm_get_slow_requests);
PHP_FUNCTION(apm_get_events_count);
PHP_FUNCTION(apm_get_slow_requests_count);
PHP_FUNCTION(apm_get_event_info);

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
	/* Path to the SQLite database file */
	char     *db_path;
	
	/* The actual db file */
	char     db_file[MAXPATHLEN];
	
	/* DB handle */
	sqlite3 *event_db;
	
	/* Max timeout to wait for storing the event in the DB */
	long      timeout;
	/* Time (in ms) before a request is considered 'slow' */
	long      slow_request_duration;
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
} apm_event_info;

#endif

