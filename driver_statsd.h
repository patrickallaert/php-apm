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
void apm_driver_statsd_insert_event(int type, char * error_filename, uint error_lineno, char * msg, char * trace TSRMLS_DC);
int apm_driver_statsd_minit(int);
int apm_driver_statsd_rinit();
int apm_driver_statsd_mshutdown();
int apm_driver_statsd_rshutdown();
void apm_driver_statsd_insert_stats(float duration, float user_cpu, float sys_cpu, long mem_peak_usage TSRMLS_DC);

/* Extension globals */
ZEND_BEGIN_MODULE_GLOBALS(apm_statsd)
	/* Boolean controlling whether the driver is active or not */
	zend_bool enabled;

	/* Boolean controlling the collection of stats */
	zend_bool stats_enabled;

	/* (unused for StatsD) */
	long exception_mode;

	/* (unused for StatsD) */
	int error_reporting;

	/* StatsD host */
	char *host;

	/* StatsD port */
	unsigned int port;

	/* StatsD key prefix */
	char *prefix;
	
	/* addinfo for StatsD server */
	struct addrinfo *servinfo;
ZEND_END_MODULE_GLOBALS(apm_statsd)

#ifdef ZTS
#define APM_SD_G(v) TSRMG(apm_statsd_globals_id, zend_apm_statsd_globals *, v)
#else
#define APM_SD_G(v) (apm_statsd_globals.v)
#endif

#endif
