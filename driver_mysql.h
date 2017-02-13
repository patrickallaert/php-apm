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

#define TABLE_REQUEST "\
  CREATE TABLE IF NOT EXISTS request (\
    id INTEGER UNSIGNED PRIMARY KEY auto_increment,\
    application VARCHAR(255) NOT NULL,\
    ts TIMESTAMP NOT NULL,\
    script TEXT NOT NULL,\
    uri TEXT NOT NULL,\
    host TEXT NOT NULL,\
    ip INTEGER UNSIGNED NOT NULL,\
    cookies TEXT NOT NULL,\
    post_vars TEXT NOT NULL,\
    referer TEXT NOT NULL,\
    method TEXT NOT NULL\
)"

#define TABLE_EVENT "\
  CREATE TABLE IF NOT EXISTS event (\
    id INTEGER UNSIGNED PRIMARY KEY auto_increment,\
    request_id INTEGER UNSIGNED,\
    ts TIMESTAMP NOT NULL,\
    type SMALLINT UNSIGNED NOT NULL,\
    file TEXT NOT NULL,\
    line MEDIUMINT UNSIGNED NOT NULL,\
    message TEXT NOT NULL,\
    backtrace BLOB NOT NULL,\
    KEY request (request_id)\
)"

#define TABLE_STATS "\
  CREATE TABLE IF NOT EXISTS stats (\
    id INTEGER UNSIGNED PRIMARY KEY auto_increment,\
    request_id INTEGER UNSIGNED,\
    duration FLOAT UNSIGNED NOT NULL,\
    user_cpu FLOAT UNSIGNED NOT NULL,\
    sys_cpu FLOAT UNSIGNED NOT NULL,\
    mem_peak_usage INTEGER UNSIGNED NOT NULL,\
    KEY request (request_id)\
)"

#define QUERY_REQUEST_ID "SET @request_id = LAST_INSERT_ID()"

#endif
