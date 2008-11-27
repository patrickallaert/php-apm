
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

/* Extension globals */
ZEND_BEGIN_MODULE_GLOBALS(apm)
	/* Boolean controlling whether the monitoring is active or not */
	zend_bool enabled;
	/* Path to the SQLite database file */
	char     *db_path;
	/* max timeout to wait for storing the event in the DB */
	long      timeout;
ZEND_END_MODULE_GLOBALS(apm) 

#ifdef ZTS
#define APM_G(v) TSRMG(apm_globals_id, zend_apm_globals *, v)
#else
#define APM_G(v) (apm_globals.v)
#endif

#endif

