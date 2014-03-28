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

#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <time.h>
#include "php_apm.h"
#include "php_ini.h"
#include "ext/standard/php_smart_str.h"
#include "ext/standard/php_filestat.h"
#include "driver_statsd.h"
#ifdef NETWARE
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

ZEND_EXTERN_MODULE_GLOBALS(apm)

ZEND_DECLARE_MODULE_GLOBALS(apm_statsd)

APM_DRIVER_CREATE(statsd)

PHP_INI_BEGIN()
	/* Boolean controlling whether the driver is active or not */
	STD_PHP_INI_BOOLEAN("apm.statsd_enabled", "1",         PHP_INI_PERDIR, OnUpdateBool,   enabled, zend_apm_statsd_globals, apm_statsd_globals)
	/* Boolean controlling the collection of stats */
	STD_PHP_INI_BOOLEAN("apm.statsd_stats_enabled", "1",   PHP_INI_ALL, OnUpdateBool,   stats_enabled,       zend_apm_statsd_globals, apm_statsd_globals)
	/* error_reporting of the driver */
	STD_PHP_INI_ENTRY("apm.statsd_error_reporting", NULL,  PHP_INI_ALL,    OnUpdateAPMstatsdErrorReporting,   error_reporting, zend_apm_statsd_globals, apm_statsd_globals)
	/* StatsD host */
	STD_PHP_INI_ENTRY("apm.statsd_host",      "localhost", PHP_INI_PERDIR,    OnUpdateString, host,    zend_apm_statsd_globals, apm_statsd_globals)
	/* StatsD port */
	STD_PHP_INI_ENTRY("apm.statsd_port",      "8125",      PHP_INI_PERDIR,    OnUpdateLong,   port,    zend_apm_statsd_globals, apm_statsd_globals)
	/* StatsD port */
	STD_PHP_INI_ENTRY("apm.statsd_prefix",    "apm",       PHP_INI_ALL,    OnUpdateString, prefix,  zend_apm_statsd_globals, apm_statsd_globals)
PHP_INI_END()

/* Insert an event in the backend */
void apm_driver_statsd_process_event(PROCESS_EVENT_ARGS)
{
	int socketDescriptor;
	char data[1024], type_string[20];

	if (
		(socketDescriptor = socket(APM_SD_G(servinfo)->ai_family, APM_SD_G(servinfo)->ai_socktype, APM_SD_G(servinfo)->ai_protocol)) != -1
	) {
		switch(type) {
			case E_ERROR:
				strcpy(type_string, "error");
				break;
			case E_WARNING:
				strcpy(type_string, "warning");
				break;
			case E_PARSE:
				strcpy(type_string, "parse_error");
				break;
			case E_NOTICE:
				strcpy(type_string, "notice");
				break;
			case E_CORE_ERROR:
				strcpy(type_string, "core_error");
				break;
			case E_CORE_WARNING:
				strcpy(type_string, "core_warning");
				break;
			case E_COMPILE_ERROR:
				strcpy(type_string, "compile_error");
				break;
			case E_COMPILE_WARNING:
				strcpy(type_string, "compile_warning");
				break;
			case E_USER_ERROR:
				strcpy(type_string, "user_error");
				break;
			case E_USER_WARNING:
				strcpy(type_string, "user_warning");
				break;
			case E_USER_NOTICE:
				strcpy(type_string, "user_notice");
				break;
			case E_STRICT:
				strcpy(type_string, "strict");
				break;
			case E_RECOVERABLE_ERROR:
				strcpy(type_string, "recoverable_error");
				break;
			case E_DEPRECATED:
				strcpy(type_string, "deprecated");
				break;
			case E_USER_DEPRECATED:
				strcpy(type_string, "user_deprecated");
				break;
			default:
				strcpy(type_string, "unknown");
		}

		sprintf(data, "%s.%s:1|ms", APM_SD_G(prefix), type_string);
		if (sendto(socketDescriptor, data, strlen(data), 0, APM_SD_G(servinfo)->ai_addr, APM_SD_G(servinfo)->ai_addrlen) == -1) {/* cannot send */ }

		close(socketDescriptor);
	}
}

int apm_driver_statsd_minit(int module_number)
{
	struct addrinfo hints;
	char port[8];

	REGISTER_INI_ENTRIES();

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	sprintf(port, "%u", APM_SD_G(port));

	if (getaddrinfo(APM_SD_G(host), port, &hints, &APM_SD_G(servinfo)) != 0) {
		APM_SD_G(enabled) = 0;
	}
	return SUCCESS;
}

int apm_driver_statsd_rinit()
{
	return SUCCESS;
}

int apm_driver_statsd_mshutdown()
{
	freeaddrinfo(APM_SD_G(servinfo));

	return SUCCESS;
}

int apm_driver_statsd_rshutdown()
{
	return SUCCESS;
}

void apm_driver_statsd_process_stats(PROCESS_STATS_ARGS)
{
	int socketDescriptor;
	char data[1024];

	if (
		(socketDescriptor = socket(APM_SD_G(servinfo)->ai_family, APM_SD_G(servinfo)->ai_socktype, APM_SD_G(servinfo)->ai_protocol)) != -1
	) {

		sprintf(data, "%1$s.duration:%2$f|ms\n%1$s.user_cpu:%3$f|ms\n%1$s.sys_cpu:%4$f|ms\n%1$s.mem_peak_usage:%5$ld|g", APM_SD_G(prefix), duration / 1000, user_cpu / 1000, sys_cpu / 1000, mem_peak_usage);
		if (sendto(socketDescriptor, data, strlen(data), 0, APM_SD_G(servinfo)->ai_addr, APM_SD_G(servinfo)->ai_addrlen) == -1) {/* cannot send */ }

		close(socketDescriptor);
	}
}
