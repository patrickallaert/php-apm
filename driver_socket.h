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

#ifndef DRIVER_SOCKET_H
#define DRIVER_SOCKET_H

#include "zend_API.h"

#define APM_E_socket APM_E_ALL

#define MAX_SOCKETS 10

apm_driver_entry * apm_driver_socket_create();
void apm_driver_socket_process_event(PROCESS_EVENT_ARGS);
void apm_driver_socket_process_stats();
int apm_driver_socket_minit(int);
int apm_driver_socket_rinit();
int apm_driver_socket_mshutdown();
int apm_driver_socket_rshutdown();

/* Extension globals */
ZEND_BEGIN_MODULE_GLOBALS(apm_socket)
	/* Boolean controlling whether the driver is active or not */
	zend_bool enabled;

	/* Boolean controlling the collection of stats */
	zend_bool stats_enabled;

	/* (unused for StatsD) */
	long exception_mode;

	/* (unused for StatsD) */
	int error_reporting;

	/* Option to process silenced events */
	zend_bool process_silenced_events;

	/* socket path */
	char *path;

	apm_event_entry *events;
	apm_event_entry **last_event;
ZEND_END_MODULE_GLOBALS(apm_socket)

#ifdef ZTS
#define APM_SOCK_G(v) TSRMG(apm_socket_globals_id, zend_apm_socket_globals *, v)
#else
#define APM_SOCK_G(v) (apm_socket_globals.v)
#endif

#endif
