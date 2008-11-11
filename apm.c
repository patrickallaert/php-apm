
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "ext/standard/url.h"
#include "php_apm.h"
#include "php_globals.h"

ZEND_API void (*old_error_cb)(int type, const char *error_filename,
                              const uint error_lineno, const char *format,
                              va_list args);
void apm_error_cb(int type, const char *error_filename, 
				  const uint error_lineno, const char *format,
                  va_list args);

function_entry apm_functions[] = {
	{NULL, NULL, NULL}
};


zend_module_entry apm_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"apm",
	apm_functions,
	PHP_MINIT(apm),
	PHP_MSHUTDOWN(apm),
	PHP_RINIT(apm),	
	PHP_RSHUTDOWN(apm),
	PHP_MINFO(apm),
#if ZEND_MODULE_API_NO >= 20010901
	"0.1.0",
#endif
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_APM
ZEND_GET_MODULE(apm)
#endif

ZEND_DECLARE_MODULE_GLOBALS(apm)

PHP_INI_BEGIN()
    STD_PHP_INI_ENTRY("apm.enabled",       "1", PHP_INI_ALL, OnUpdateBool, enabled,       zend_apm_globals, apm_globals)
PHP_INI_END()
 
static void apm_init_globals(zend_apm_globals *apm_globals)
{
	apm_globals->enabled       = 0;
}


PHP_MINIT_FUNCTION(apm)
{
	ZEND_INIT_MODULE_GLOBALS(apm, apm_init_globals, NULL);
	REGISTER_INI_ENTRIES();

	return SUCCESS;
}


PHP_MSHUTDOWN_FUNCTION(apm)
{
	UNREGISTER_INI_ENTRIES();

	zend_error_cb        = old_error_cb;

	return SUCCESS;
}



PHP_RINIT_FUNCTION(apm)
{
	old_error_cb = zend_error_cb;

	if (APM_G(enabled)) {
		zend_error_cb = apm_error_cb;
	}
	return SUCCESS;
}



PHP_RSHUTDOWN_FUNCTION(apm)
{
	zend_error_cb        = old_error_cb;

	return SUCCESS;
}


PHP_MINFO_FUNCTION(apm)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "apm support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();

}

/** To investigate */
int apm_printf(FILE *stream, const char* fmt, ...)
{
	char *message;
	int len;
	va_list args;
	
	va_start(args, fmt);
	len = vspprintf(&message, 0, fmt, args);
	va_end(args);

	fprintf(stream, "%s", message);
	efree(message);
	
	return len;
}

/* {{{ void apm_error(int type, const char *format, ...)
 *    This function provides a hook for error */
void apm_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
    old_error_cb(type, error_filename, error_lineno, format, args);
}
/* }}} */

