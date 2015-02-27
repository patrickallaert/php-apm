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

#ifndef DRIVER_MYSQL_H
#define DRIVER_MYSQL_H

#define DB_HOST "localhost"
#define DB_USER "root"

#include "php_apm.h"

#define APM_E_mysql APM_E_ALL

#define MYSQL_INSTANCE_INIT_EX(ret) connection = mysql_get_instance(TSRMLS_C); \
	if (connection == NULL) { \
		return ret; \
	}
#define MYSQL_INSTANCE_INIT MYSQL_INSTANCE_INIT_EX()

apm_driver_entry * apm_driver_mysql_create();

PHP_INI_MH(OnUpdateAPMmysqlErrorReporting);

#endif
