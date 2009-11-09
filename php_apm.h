/*
    APM stands for Alternative PHP Monitor
    Copyright (C) 2008-2009  Davide Mendolia, Patrick Allaert

    This file is part of APM.

    APM is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    APM is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with APM.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef PHP_APM_H
#define PHP_APM_H

#include "php.h"

extern zend_module_entry apm_module_entry;
#define phpext_apm_ptr &apm_module_entry

#ifdef PHP_WIN32
#define PHP_APM_API __declspec(dllexport)
#else
#define PHP_APM_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

PHP_MINIT_FUNCTION(apm);
PHP_MSHUTDOWN_FUNCTION(apm);
PHP_RINIT_FUNCTION(apm);
PHP_RSHUTDOWN_FUNCTION(apm);
PHP_MINFO_FUNCTION(apm);

PHP_FUNCTION(apm_get_events);
PHP_FUNCTION(apm_get_slow_requests);

/* Extension globals */
ZEND_BEGIN_MODULE_GLOBALS(apm)
	/* Boolean controlling whether the extension is globally active or not */
	zend_bool enabled;
	/* Boolean controlling whether the event monitoring is active or not */
	zend_bool event_enabled;
	/* Boolean controlling whether the slow request monitoring is active or not */
	zend_bool slow_request_enabled;
	/* Path to the SQLite database file */
	char     *db_path;
	/* Max timeout to wait for storing the event in the DB */
	long      timeout;
	/* Time (in ms) before a request is considered 'slow' */
	long      slow_request_duration;
ZEND_END_MODULE_GLOBALS(apm) 

#ifdef ZTS
#define APM_G(v) TSRMG(apm_globals_id, zend_apm_globals *, v)
#else
#define APM_G(v) (apm_globals.v)
#endif

#endif

