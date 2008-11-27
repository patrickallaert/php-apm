
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sqlite3.h>
#include "php.h"
#include "php_ini.h"
#include "php_apm.h"

ZEND_API void (*old_error_cb)(int type, const char *error_filename,
                              const uint error_lineno, const char *format,
                              va_list args);
void apm_error_cb(int type, const char *error_filename, 
                  const uint error_lineno, const char *format,
                  va_list args);
sqlite3 *eventDb;

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
	STD_PHP_INI_BOOLEAN("apm.enabled",                "1",                      PHP_INI_ALL, OnUpdateBool,   enabled,  zend_apm_globals, apm_globals)
	STD_PHP_INI_ENTRY("apm.max_event_insert_timeout", "100",                    PHP_INI_ALL, OnUpdateLong,   timeout,  zend_apm_globals, apm_globals)
	STD_PHP_INI_ENTRY("apm.db_path",                  "/var/php/apm/events.db", PHP_INI_ALL, OnUpdateString, db_path,  zend_apm_globals, apm_globals)
PHP_INI_END()
 
static void apm_init_globals(zend_apm_globals *apm_globals)
{
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

	zend_error_cb = old_error_cb;

	return SUCCESS;
}

PHP_RINIT_FUNCTION(apm)
{
	old_error_cb = zend_error_cb;

	if (APM_G(enabled)) {
		int rc;
		/* opening the sqlite database file */
		rc = sqlite3_open(APM_G(db_path), &eventDb);
		sqlite3_busy_timeout(eventDb, APM_G(timeout));
		if (rc) {
			sqlite3_close(eventDb);
			return FAILURE;
		}

		zend_error_cb = apm_error_cb;
	}
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(apm)
{
	zend_error_cb = old_error_cb;

	return SUCCESS;
}

PHP_MINFO_FUNCTION(apm)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "APM support", "enabled");
	php_info_print_table_end();

	DISPLAY_INI_ENTRIES();
}

/* {{{ void apm_error(int type, const char *format, ...)
 *    This function provides a hook for error */
void apm_error_cb(int type, const char *error_filename, const uint error_lineno, const char *format, va_list args)
{
	char *msg, *sql;
	va_list args_copy;

	/* a copy of args is needed to be used for the old_error_cb */
	va_copy(args_copy, args);
	vspprintf(&msg, 0, format, args_copy);

	sql = sqlite3_mprintf("INSERT INTO event (ts, type, file, line, message) VALUES (datetime(), %d, %Q, %d, %Q);",
	                      type, error_filename, error_lineno, msg);
	sqlite3_exec(eventDb, sql, NULL, NULL, NULL);
	efree(msg);

	/* calling saved callback function for error handling */
	old_error_cb(type, error_filename, error_lineno, format, args);
}
/* }}} */

