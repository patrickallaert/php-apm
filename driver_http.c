/*
 +----------------------------------------------------------------------+
 |	APM stands for Alternative PHP Monitor															|
 +----------------------------------------------------------------------+
 | Copyright (c) 2011-2016	David Strauss															 |
 +----------------------------------------------------------------------+
 | This source file is subject to version 3.01 of the PHP license,			|
 | that is bundled with this package in the file LICENSE, and is				|
 | available through the world-wide-web at the following url:					 |
 | http://www.php.net/license/3_01.txt																	|
 | If you did not receive a copy of the PHP license and are unable to	 |
 | obtain it through the world-wide-web, please send a note to					|
 | license@php.net so we can mail you a copy immediately.							 |
 +----------------------------------------------------------------------+
 | Authors: David Strauss <david@davidstrauss.net>											|
 +----------------------------------------------------------------------+
*/

#include <stdio.h>
#include <curl/curl.h>
#include "php_apm.h"
#include "php_ini.h"

#include "driver_http.h"

ZEND_EXTERN_MODULE_GLOBALS(apm)

APM_DRIVER_CREATE(http)

char *truncate_data(char *input_str, size_t max_len)
{
	char *truncated;
	input_str = input_str ? input_str : NULL;
	if (max_len == 0)
		return strdup(input_str);
	truncated = strndup(input_str, max_len);
	return truncated;
}

/* Insert an event in the backend */
void apm_driver_http_process_event(PROCESS_EVENT_ARGS)
{
	CURL *curl;
	CURLcode res;

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();
	if(curl) {
	struct curl_httppost *formpost = NULL;
	struct curl_httppost *lastptr = NULL;
	struct curl_slist *headerlist = NULL;
	static const char buf[] = "Expect:";
	char int2string[64];
	char *trace_to_send;
	size_t max_len = 0;

	if (APM_G(http_max_backtrace_length) >= 0)
		max_len = APM_G(http_max_backtrace_length);

	trace_to_send = truncate_data(trace, max_len);

	sprintf(int2string, "%d", type);
	curl_formadd(&formpost,
			 &lastptr,
			 CURLFORM_COPYNAME, "type",
			 CURLFORM_COPYCONTENTS, int2string,
			 CURLFORM_END);

	curl_formadd(&formpost,
			 &lastptr,
			 CURLFORM_COPYNAME, "file",
			 CURLFORM_COPYCONTENTS, error_filename ? error_filename : "",
			 CURLFORM_END);

	sprintf(int2string, "%d", error_lineno);
	curl_formadd(&formpost,
			 &lastptr,
			 CURLFORM_COPYNAME, "line",
			 CURLFORM_COPYCONTENTS, int2string,
			 CURLFORM_END);

	curl_formadd(&formpost,
			 &lastptr,
			 CURLFORM_COPYNAME, "message",
			 CURLFORM_COPYCONTENTS, msg ? msg : "",
			 CURLFORM_END);

	curl_formadd(&formpost,
			 &lastptr,
			 CURLFORM_COPYNAME, "backtrace",
			 CURLFORM_COPYCONTENTS, trace_to_send,
			 CURLFORM_END);
	
	headerlist = curl_slist_append(headerlist, buf);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerlist);
	curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);

	curl_easy_setopt(curl, CURLOPT_URL, APM_G(http_server));
	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, APM_G(http_request_timeout));
	if (APM_G(http_client_certificate) != NULL) {
		curl_easy_setopt(curl, CURLOPT_SSLCERT, APM_G(http_client_certificate));
	}
	if (APM_G(http_client_key) != NULL) {
		curl_easy_setopt(curl, CURLOPT_SSLKEY, APM_G(http_client_key));
	}
	if (APM_G(http_certificate_authorities) != NULL) {
		curl_easy_setopt(curl, CURLOPT_CAINFO, APM_G(http_certificate_authorities));
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
	}
	
	res = curl_easy_perform(curl);
 
	APM_DEBUG("[HTTP driver] Result: %s\n", curl_easy_strerror(res));

	/* Always clean up. */
	curl_easy_cleanup(curl);
	free(trace_to_send);
	}
}

int apm_driver_http_minit(int module_number)
{
	return SUCCESS;
}

int apm_driver_http_rinit()
{
	return SUCCESS;
}

int apm_driver_http_mshutdown()
{
	return SUCCESS;
}

int apm_driver_http_rshutdown()
{
	return SUCCESS;
}

void apm_driver_http_process_stats(TSRMLS_D)
{
}
