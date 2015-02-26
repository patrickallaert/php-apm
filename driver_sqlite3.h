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

#ifndef DRIVER_SQLITE3_H
#define DRIVER_SQLITE3_H

#include "zend_API.h"
#include "php_apm.h"

#define APM_E_sqlite3 APM_E_ALL

#define SQLITE_INSTANCE_INIT_EX(ret) connection = sqlite_get_instance(TSRMLS_C); \
	if (connection == NULL) { \
		return ret; \
	}
#define SQLITE_INSTANCE_INIT SQLITE_INSTANCE_INIT_EX()

#define DB_FILE "events"

apm_driver_entry * apm_driver_sqlite3_create();

PHP_INI_MH(OnUpdateDBFile);
PHP_INI_MH(OnUpdateAPMsqlite3ErrorReporting);

#endif
