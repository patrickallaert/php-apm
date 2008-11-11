
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

ZEND_BEGIN_MODULE_GLOBALS(apm)
	zend_bool enabled;
ZEND_END_MODULE_GLOBALS(apm) 

int apm_printf(FILE *stream, const char* fmt, ...);

#ifdef ZTS
#define APM_G(v) TSRMG(apm_globals_id, zend_apm_globals *, v)
#else
#define APM_G(v) (apm_globals.v)
#endif
#define APM_PRINT(v,args...) if (apm_G(verbosity) >= (v)) { apm_printf(stderr, args); }

#if PHP_VERSION_ID >= 60000
#define ZSTRCP(str)  ((str).v)
#define ZSTRKEY(key) ((key).s)
#define ZSTRFMT      "%v"
#else
#define ZSTRCP(str)  (str)
#define ZSTRKEY(key) (key)
#define ZSTRFMT      "%s"
#endif

#endif

