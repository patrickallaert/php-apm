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

#ifndef DRIVER_STATSD_H
#define DRIVER_STATSD_H

#include "zend_API.h"

#define APM_E_statsd APM_E_ALL

apm_driver_entry * apm_driver_statsd_create();
void apm_driver_statsd_process_event(PROCESS_EVENT_ARGS);
void apm_driver_statsd_process_stats(TSRMLS_D);
int apm_driver_statsd_minit(int TSRMLS_DC);
int apm_driver_statsd_rinit(TSRMLS_D);
int apm_driver_statsd_mshutdown();
int apm_driver_statsd_rshutdown(TSRMLS_D);

PHP_INI_MH(OnUpdateAPMstatsdErrorReporting);

#endif
