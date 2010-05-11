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

#ifndef DRIVER_SQLITE3_H
#define DRIVER_SQLITE3_H

#include <sqlite3.h>
#include "zend_API.h"

#define APM_E_sqlite3 APM_E_ALL

#define DB_FILE "events"

apm_driver_entry * apm_driver_sqlite3_create();
void apm_driver_sqlite3_insert_event(int type, char * error_filename, uint error_lineno, char * msg, char * trace TSRMLS_DC);
void apm_driver_sqlite3_insert_events(apm_event_entry * event_entry TSRMLS_DC);
int apm_driver_sqlite3_minit(int);
int apm_driver_sqlite3_rinit();
int apm_driver_sqlite3_mshutdown();
int apm_driver_sqlite3_rshutdown();
void apm_driver_sqlite3_insert_slow_request(float duration, char * script_filename);

/* Extension globals */
ZEND_BEGIN_MODULE_GLOBALS(apm_sqlite3)
	/* Boolean controlling whether the driver is active or not */
	zend_bool enabled;

	/* driver error reporting */
	int     error_reporting;

	/* Path to the SQLite database file */
	char    *db_path;

	/* The actual db file */
	char     db_file[MAXPATHLEN];

	/* DB handle */
	sqlite3 *event_db;

	/* Max timeout to wait for storing the event in the DB */
	long      timeout;
ZEND_END_MODULE_GLOBALS(apm_sqlite3)

#ifdef ZTS
#define APM_S3_G(v) TSRMG(apm_sqlite3_globals_id, zend_apm_sqlite3_globals *, v)
#else
#define APM_S3_G(v) (apm_sqlite3_globals.v)
#endif

#endif
