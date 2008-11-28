
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <sqlite3.h>
#include "php.h"
#include "php_ini.h"
#include "php_apm.h"

static const char *createSql =  "\
CREATE TABLE event ( \
	id INTEGER PRIMARY KEY AUTOINCREMENT, \
	ts TEXT, \
	type INTEGER, \
	file TEXT, \
	line INTEGER, \
	message TEXT)";

ZEND_API void (*old_error_cb)(int type, const char *error_filename,
                              const uint error_lineno, const char *format,
                              va_list args);
void apm_error_cb(int type, const char *error_filename, 
                  const uint error_lineno, const char *format,
                  va_list args);
sqlite3 *eventDb;

zend_module_entry apm_module_entry = {
#if ZEND_MODULE_API_NO >= 20010901
	STANDARD_MODULE_HEADER,
#endif
	"apm",
	NULL,
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
	/* Boolean controlling whether the monitoring is active or not */
	STD_PHP_INI_BOOLEAN("apm.enabled",                "1",                      PHP_INI_ALL, OnUpdateBool,   enabled,  zend_apm_globals, apm_globals)
	/* Path to the SQLite database file */
	STD_PHP_INI_ENTRY("apm.max_event_insert_timeout", "100",                    PHP_INI_ALL, OnUpdateLong,   timeout,  zend_apm_globals, apm_globals)
	/* Max timeout to wait for storing the event in the DB */
	STD_PHP_INI_ENTRY("apm.db_path",                  "/var/php/apm/events.db", PHP_INI_ALL, OnUpdateString, db_path,  zend_apm_globals, apm_globals)
PHP_INI_END()
 
static void apm_init_globals(zend_apm_globals *apm_globals)
{
}

PHP_MINIT_FUNCTION(apm)
{
	sqlite3 *db;
	int rc;

	ZEND_INIT_MODULE_GLOBALS(apm, apm_init_globals, NULL);
	REGISTER_INI_ENTRIES();

	/* Opening the sqlite database file */
	rc = sqlite3_open(APM_G(db_path), &db);
	if (rc) {
		/*
		 Closing DB file and stop loading the extension
		 in case of error while opening the database file
		 */
		sqlite3_close(db);
		return FAILURE;
	}
	/* Executing SQL creation table query */
	sqlite3_exec(
		db,
		"CREATE TABLE IF NOT EXISTS event ( \
		    id INTEGER PRIMARY KEY AUTOINCREMENT, \
		    ts TEXT, \
		    type INTEGER, \
		    file TEXT, \
		    line INTEGER, \
		    message TEXT)",
		NULL, NULL, NULL);
	sqlite3_close(db);

	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(apm)
{
	UNREGISTER_INI_ENTRIES();

	/* Restoring saved error callback function */
	zend_error_cb = old_error_cb;

	return SUCCESS;
}

PHP_RINIT_FUNCTION(apm)
{
	/* Storing actual error callback function for later restore */
	old_error_cb = zend_error_cb;

	if (APM_G(enabled)) {
		int rc;
		/* Opening the sqlite database file */
		rc = sqlite3_open(APM_G(db_path), &eventDb);
		sqlite3_busy_timeout(eventDb, APM_G(timeout));
		if (rc) {
			/*
			 Closing DB file and stop loading the extension
			 in case of error while opening the database file
			 */
			sqlite3_close(eventDb);
			return FAILURE;
		}

		/* Replacing current error callback function with apm's one */
		zend_error_cb = apm_error_cb;
	}
	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(apm)
{
	/* Restoring saved error callback function */
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

	/* A copy of args is needed to be used for the old_error_cb */
	va_copy(args_copy, args);
	vspprintf(&msg, 0, format, args_copy);

	/* Builing SQL insert query */
	sql = sqlite3_mprintf("INSERT INTO event (ts, type, file, line, message) VALUES (datetime(), %d, %Q, %d, %Q);",
	                      type, error_filename, error_lineno, msg);
	/* Executing SQL insert query */
	sqlite3_exec(eventDb, sql, NULL, NULL, NULL);
	efree(msg);

	/* Calling saved callback function for error handling */
	old_error_cb(type, error_filename, error_lineno, format, args);
}
/* }}} */

