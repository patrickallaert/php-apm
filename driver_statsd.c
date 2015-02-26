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
#include <locale.h>
#include "php_apm.h"
#include "php_ini.h"
#include "driver_statsd.h"
#include "SAPI.h"
#ifdef NETWARE
#include <netinet/in.h>
#endif
#if HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

APM_DRIVER_CREATE(statsd)

/* Insert an event in the backend */
void apm_driver_statsd_process_event(PROCESS_EVENT_ARGS)
{
	int socketDescriptor;
	char data[1024], type_string[20];

	if (
		(socketDescriptor = socket(APM_G(statsd_servinfo)->ai_family, APM_G(statsd_servinfo)->ai_socktype, APM_G(statsd_servinfo)->ai_protocol)) != -1
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

		sprintf(data, "%s.%s:1|ms", APM_G(statsd_prefix), type_string);
		if (sendto(socketDescriptor, data, strlen(data), 0, APM_G(statsd_servinfo)->ai_addr, APM_G(statsd_servinfo)->ai_addrlen) == -1) {/* cannot send */ }

		close(socketDescriptor);
	}
}

int apm_driver_statsd_minit(int module_number TSRMLS_DC)
{
	struct addrinfo hints;
	char port[8];

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	sprintf(port, "%u", APM_G(statsd_port));

	if (getaddrinfo(APM_G(statsd_host), port, &hints, &APM_G(statsd_servinfo)) != 0) {
		APM_G(statsd_enabled) = 0;
	}
	return SUCCESS;
}

int apm_driver_statsd_rinit(TSRMLS_D)
{
	return SUCCESS;
}

int apm_driver_statsd_mshutdown(SHUTDOWN_FUNC_ARGS)
{
	freeaddrinfo(APM_G(statsd_servinfo));

	return SUCCESS;
}

int apm_driver_statsd_rshutdown(TSRMLS_D)
{
	return SUCCESS;
}

void apm_driver_statsd_process_stats(TSRMLS_D)
{
	int socketDescriptor;
	char data[1024];

	setlocale(LC_ALL, "C");

	if (
		(socketDescriptor = socket(APM_G(statsd_servinfo)->ai_family, APM_G(statsd_servinfo)->ai_socktype, APM_G(statsd_servinfo)->ai_protocol)) != -1
	) {
		APM_DEBUG("Sending data to StatsD");
		sprintf(data, "%1$s.duration:%2$f|ms\n%1$s.user_cpu:%3$f|ms\n%1$s.sys_cpu:%4$f|ms\n%1$s.mem_peak_usage:%5$ld|g\n%1$s.response.code.%6$d:1|c", APM_G(statsd_prefix), APM_G(duration) / 1000, APM_G(user_cpu) / 1000, APM_G(sys_cpu) / 1000, APM_G(mem_peak_usage), SG(sapi_headers).http_response_code);
		if (sendto(socketDescriptor, data, strlen(data), 0, APM_G(statsd_servinfo)->ai_addr, APM_G(statsd_servinfo)->ai_addrlen) == -1) {/* cannot send */ }

		close(socketDescriptor);
	}
}
