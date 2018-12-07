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

#include "php_apm.h"
#include "php.h"
#include "zend_types.h"
#include "ext/standard/php_string.h"

#if PHP_VERSION_ID >= 70000
#include "Zend/zend_generators.h"
#endif

#include "backtrace.h"

ZEND_DECLARE_MODULE_GLOBALS(apm);

static void debug_print_backtrace_args(zval *arg_array TSRMLS_DC, smart_str *trace_str);
static void append_flat_zval_r(zval *expr TSRMLS_DC, smart_str *trace_str, char depth);
static void append_flat_hash(HashTable *ht TSRMLS_DC, smart_str *trace_str, char is_object, char depth);

#if PHP_VERSION_ID >= 70000
static void debug_backtrace_get_args(zend_execute_data *call, zval *arg_array);
#else
static zval *debug_backtrace_get_args(void ***curpos TSRMLS_DC);
#endif

static int append_variable(zval *expr, smart_str *trace_str);
static char *apm_addslashes(char *str, uint length, int *new_length);

void append_backtrace(smart_str *trace_str TSRMLS_DC)
{
	/* backtrace variables */
	zend_execute_data *ptr, *skip;
	int lineno;
	const char *function_name;
	const char *filename;
	char *call_type;
	const char *include_filename = NULL;
# if PHP_VERSION_ID >= 70000
	zval arg_array;
	zend_execute_data *call;
	zend_string *class_name = NULL;
	zend_object *object;
	zend_function *func;
# else
	const char *class_name = NULL;
	zval *arg_array = NULL;
	const char *free_class_name = NULL;
	zend_uint class_name_len = 0;
# endif
	int indent = 0;


#if PHP_VERSION_ID >= 70000
	ZVAL_UNDEF(&arg_array);
	//FIXME? ptr = EX(prev_execute_data);
	ptr = EG(current_execute_data);
	call = ptr;
#else
	ptr = EG(current_execute_data);
#endif

	while (ptr) {
		class_name = NULL;
		call_type = NULL;

#if PHP_VERSION_ID >= 70000
		ZVAL_UNDEF(&arg_array);

		ptr = zend_generator_check_placeholder_frame(ptr);
#else
		arg_array = NULL;
#endif

		skip = ptr;
		/* skip internal handler */
#if PHP_VERSION_ID >= 70000
		if ((!skip->func || !ZEND_USER_CODE(skip->func->common.type)) &&
#else
		if (!skip->op_array &&
#endif
			skip->prev_execute_data &&
#if PHP_VERSION_ID >= 70000
			skip->prev_execute_data->func &&
			ZEND_USER_CODE(skip->prev_execute_data->func->common.type) &&
			skip->prev_execute_data->opline->opcode != ZEND_DO_ICALL &&
			skip->prev_execute_data->opline->opcode != ZEND_DO_UCALL &&
#else
			skip->prev_execute_data->opline &&
#endif
			skip->prev_execute_data->opline->opcode != ZEND_DO_FCALL &&
			skip->prev_execute_data->opline->opcode != ZEND_DO_FCALL_BY_NAME &&
			skip->prev_execute_data->opline->opcode != ZEND_INCLUDE_OR_EVAL) {
			skip = skip->prev_execute_data;
		}

#if PHP_VERSION_ID >= 70000
		if (skip->func && ZEND_USER_CODE(skip->func->common.type)) {
			filename = skip->func->op_array.filename->val;
			if (skip->opline->opcode == ZEND_HANDLE_EXCEPTION) {
				if (EG(opline_before_exception)) {
					lineno = EG(opline_before_exception)->lineno;
				} else {
					lineno = skip->func->op_array.line_end;
				}
			} else {
				lineno = skip->opline->lineno;
			}
		}
#else
		if (skip->op_array) {
			filename = skip->op_array->filename;
			lineno = skip->opline->lineno;
		}
#endif
		else {
			filename = NULL;
			lineno = 0;
		}

#if PHP_VERSION_ID >= 70000
		/* $this may be passed into regular internal functions */
		object = Z_OBJ(call->This);

		if (call->func) {
			func = call->func;
			function_name = (func->common.scope && func->common.scope->trait_aliases) ?
				ZSTR_VAL(zend_resolve_method_name(
					(object ? object->ce : func->common.scope), func)) :
				(func->common.function_name ?
					ZSTR_VAL(func->common.function_name) : NULL);
		} else {
			func = NULL;
			function_name = NULL;
		}

		if (function_name) {
			if (object) {
				if (func->common.scope) {
					class_name = func->common.scope->name;
				} else if (object->handlers->get_class_name == std_object_handlers.get_class_name) {
					class_name = object->ce->name;
				} else {
					class_name = object->handlers->get_class_name(object);
				}

				call_type = "->";
			} else if (func->common.scope) {
				class_name = func->common.scope->name;
				call_type = "::";
			} else {
				class_name = NULL;
				call_type = NULL;
			}
			if (func->type != ZEND_EVAL_CODE) {
				debug_backtrace_get_args(call, &arg_array);
			}
		}
#else
		function_name = ptr->function_state.function->common.function_name;

		if (function_name) {
			if (ptr->object) {
				if (ptr->function_state.function->common.scope) {
					class_name = ptr->function_state.function->common.scope->name;
					class_name_len = strlen(class_name);
				} else {
					int dup = zend_get_object_classname(ptr->object, &class_name, &class_name_len TSRMLS_CC);
					if(!dup) {
						free_class_name = class_name;
					}
				}

				call_type = "->";
			} else if (ptr->function_state.function->common.scope) {
				class_name = ptr->function_state.function->common.scope->name;
				class_name_len = strlen(class_name);
				call_type = "::";
			} else {
				class_name = NULL;
				call_type = NULL;
			}
			if ((! ptr->opline) || ((ptr->opline->opcode == ZEND_DO_FCALL_BY_NAME) || (ptr->opline->opcode == ZEND_DO_FCALL))) {
				if (ptr->function_state.arguments) {
					arg_array = debug_backtrace_get_args(&ptr->function_state.arguments TSRMLS_CC);
				}
			}
		}
#endif
		else {
			/* i know this is kinda ugly, but i'm trying to avoid extra cycles in the main execution loop */
			zend_bool build_filename_arg = 1;

#if PHP_VERSION_ID >= 70000
			if (!ptr->func || !ZEND_USER_CODE(ptr->func->common.type) || ptr->opline->opcode != ZEND_INCLUDE_OR_EVAL) {
#else
			if (!ptr->opline || ptr->opline->opcode != ZEND_INCLUDE_OR_EVAL) {
#endif
				/* can happen when calling eval from a custom sapi */
				function_name = "unknown";
				build_filename_arg = 0;
			} else
			switch (ptr->opline->op2.constant) {
				case ZEND_EVAL:
					function_name = "eval";
					build_filename_arg = 0;
					break;
				case ZEND_INCLUDE:
					function_name = "include";
					break;
				case ZEND_REQUIRE:
					function_name = "require";
					break;
				case ZEND_INCLUDE_ONCE:
					function_name = "include_once";
					break;
				case ZEND_REQUIRE_ONCE:
					function_name = "require_once";
					break;
				default:
					/* this can actually happen if you use debug_backtrace() in your error_handler and
					 * you're in the top-scope */
					function_name = "unknown";
					build_filename_arg = 0;
					break;
			}

			if (build_filename_arg && include_filename) {
#if PHP_VERSION_ID >= 70000
				array_init(&arg_array);
				add_next_index_string(&arg_array, include_filename);
#else
				MAKE_STD_ZVAL(arg_array);
				array_init(arg_array);
				add_next_index_string(arg_array, include_filename, 1);
#endif
			}
			call_type = NULL;
		}
		smart_str_appendc(trace_str, '#');
		smart_str_append_long(trace_str, indent);
		smart_str_appendc(trace_str, ' ');
		if (class_name) {
#if PHP_VERSION_ID >= 70000
			smart_str_appends(trace_str, ZSTR_VAL(class_name));
#else
			smart_str_appends(trace_str, class_name);
#endif
			/* here, call_type is either "::" or "->" */
			smart_str_appendl(trace_str, call_type, 2);
		}
		if (function_name) {
			smart_str_appends(trace_str, function_name);
		} else {
			smart_str_appendl(trace_str, "main", 4);
		}
		smart_str_appendc(trace_str, '(');
#if PHP_VERSION_ID >= 70000
		if (Z_TYPE(arg_array) != IS_UNDEF) {
			debug_print_backtrace_args(&arg_array, trace_str);
#else
		if (arg_array) {
			debug_print_backtrace_args(arg_array TSRMLS_CC, trace_str);
#endif
			zval_ptr_dtor(&arg_array);
		}
		if (filename) {
			smart_str_appendl(trace_str, ") called at [", sizeof(") called at [") - 1);
			smart_str_appends(trace_str, filename);
			smart_str_appendc(trace_str, ':');
			smart_str_append_long(trace_str, lineno);
			smart_str_appendl(trace_str, "]\n", 2);
		} else {
#if PHP_VERSION_ID >= 70000
			zend_execute_data *prev_call = skip;
#endif
			zend_execute_data *prev = skip->prev_execute_data;

			while (prev) {
#if PHP_VERSION_ID >= 70000
				if (prev_call &&
					prev_call->func &&
					!ZEND_USER_CODE(prev_call->func->common.type)) {
					prev = NULL;
					break;
				}
				if (prev->func && ZEND_USER_CODE(prev->func->common.type)) {
					zend_printf(") called at [%s:%d]\n", prev->func->op_array.filename->val, prev->opline->lineno);
					break;
				}
				prev_call = prev;
#else
				if (prev->function_state.function &&
					prev->function_state.function->common.type != ZEND_USER_FUNCTION) {
					prev = NULL;
					break;
				}
				if (prev->op_array) {
					smart_str_appendl(trace_str, ") called at [", sizeof(") called at [") - 1);
					smart_str_appends(trace_str, prev->op_array->filename);
					smart_str_appendc(trace_str, ':');
					smart_str_append_long(trace_str, (long) prev->opline->lineno);
					smart_str_appendl(trace_str, "]\n", 2);
					break;
				}
#endif
				prev = prev->prev_execute_data;
			}
			if (!prev) {
				smart_str_appendl(trace_str, ")\n", 2);
			}
		}
		include_filename = filename;
		ptr = skip->prev_execute_data;
		++indent;
#if PHP_VERSION_ID >= 70000
		call = skip;
#else
		if (free_class_name) {
			efree((char *) free_class_name);
			free_class_name = NULL;
		}
#endif
	}
}

#if PHP_VERSION_ID >= 70000
static void debug_print_backtrace_args(zval *arg_array, smart_str *trace_str)
{
	zval *tmp;
	int i = 0;

	ZEND_HASH_FOREACH_VAL(Z_ARRVAL_P(arg_array), tmp) {
		if (i++) {
			smart_str_appendl(trace_str, ", ", 2);
		}
		append_flat_zval_r(tmp, trace_str, 0);
	} ZEND_HASH_FOREACH_END();
}
#else
static void debug_print_backtrace_args(zval *arg_array TSRMLS_DC, smart_str *trace_str)
{
	zval **tmp;
	HashPosition iterator;
	int i = 0;

	zend_hash_internal_pointer_reset_ex(arg_array->value.ht, &iterator);
	while (zend_hash_get_current_data_ex(arg_array->value.ht, (void **) &tmp, &iterator) == SUCCESS) {
		if (i++) {
			smart_str_appendl(trace_str, ", ", 2);
		}
		append_flat_zval_r(*tmp TSRMLS_CC, trace_str, 0);
		zend_hash_move_forward_ex(arg_array->value.ht, &iterator);
	}
}
#endif

// See void zend_print_flat_zval_r in php/Zend/zend.c for template when adapting to newer php versions
#if PHP_VERSION_ID >= 70300
static void append_flat_zval_r(zval *expr TSRMLS_DC, smart_str *trace_str, char depth)
{
	if (depth >= APM_G(dump_max_depth)) {
		smart_str_appendl(trace_str, "/* [...] */", sizeof("/* [...] */") - 1);
		return;
	}

	switch (Z_TYPE_P(expr)) {
		case IS_REFERENCE:
			ZVAL_DEREF(expr);
			smart_str_appendc(trace_str, '&');
			append_flat_zval_r(expr, trace_str, depth);
			break;
		case IS_ARRAY:
			smart_str_appendc(trace_str, '[');
			if (Z_REFCOUNTED_P(expr)) {
				if (Z_IS_RECURSIVE_P(expr)) {
					smart_str_appendl(trace_str, " *RECURSION*", sizeof(" *RECURSION*") - 1);
					return;
				}
				Z_PROTECT_RECURSION_P(expr);
			}
			append_flat_hash(Z_ARRVAL_P(expr) TSRMLS_CC, trace_str, 0, depth + 1);
			smart_str_appendc(trace_str, ']');
			if (Z_REFCOUNTED_P(expr)) {
				Z_UNPROTECT_RECURSION_P(expr);
			}
			break;
		case IS_OBJECT:
		{
			HashTable *properties = NULL;
			zend_string *class_name = Z_OBJ_HANDLER_P(expr, get_class_name)(Z_OBJ_P(expr));
			smart_str_appends(trace_str, ZSTR_VAL(class_name));
			smart_str_appendl(trace_str, " Object (", sizeof(" Object (") - 1);
			zend_string_release_ex(class_name, 0);

			if (Z_IS_RECURSIVE_P(expr)) {
				smart_str_appendl(trace_str, " *RECURSION*", sizeof(" *RECURSION*") - 1);
				return;
			}

			if (Z_OBJ_HANDLER_P(expr, get_properties)) {
				properties = Z_OBJPROP_P(expr);
			}
			if (properties) {
				Z_PROTECT_RECURSION_P(expr);
				append_flat_hash(properties TSRMLS_CC, trace_str, 1, depth + 1);
				Z_UNPROTECT_RECURSION_P(expr);
			}
			smart_str_appendc(trace_str, ')');
			break;
		}
		default:
			append_variable(expr, trace_str);
			break;
	}
}
#elif PHP_VERSION_ID >= 70000
static void append_flat_zval_r(zval *expr TSRMLS_DC, smart_str *trace_str, char depth)
{
	if (depth >= APM_G(dump_max_depth)) {
		smart_str_appendl(trace_str, "/* [...] */", sizeof("/* [...] */") - 1);
		return;
	}

	switch (Z_TYPE_P(expr)) {
		case IS_REFERENCE:
			ZVAL_DEREF(expr);
			smart_str_appendc(trace_str, '&');
			append_flat_zval_r(expr, trace_str, depth);
			break;

		case IS_ARRAY:
			smart_str_appendc(trace_str, '[');
			if (ZEND_HASH_APPLY_PROTECTION(Z_ARRVAL_P(expr)) && ++Z_ARRVAL_P(expr)->u.v.nApplyCount>1) {
				smart_str_appendl(trace_str, " *RECURSION*", sizeof(" *RECURSION*") - 1);
				Z_ARRVAL_P(expr)->u.v.nApplyCount--;
				return;
			}
			append_flat_hash(Z_ARRVAL_P(expr) TSRMLS_CC, trace_str, 0, depth + 1);
			smart_str_appendc(trace_str, ']');
			if (ZEND_HASH_APPLY_PROTECTION(Z_ARRVAL_P(expr))) {
				Z_ARRVAL_P(expr)->u.v.nApplyCount--;
			}
			break;
		case IS_OBJECT:
		{
			HashTable *properties = NULL;
			zend_string *class_name = Z_OBJ_HANDLER_P(expr, get_class_name)(Z_OBJ_P(expr));
			smart_str_appends(trace_str, ZSTR_VAL(class_name));
			smart_str_appendl(trace_str, " Object (", sizeof(" Object (") - 1);
			zend_string_release(class_name);

			if (Z_OBJ_APPLY_COUNT_P(expr) > 0) {
				smart_str_appendl(trace_str, " *RECURSION*", sizeof(" *RECURSION*") - 1);
				return;
			}
			if (Z_OBJ_HANDLER_P(expr, get_properties)) {
				properties = Z_OBJPROP_P(expr);
			}
			if (properties) {
				Z_OBJ_INC_APPLY_COUNT_P(expr);
				append_flat_hash(properties TSRMLS_CC, trace_str, 1, depth + 1);
				Z_OBJ_DEC_APPLY_COUNT_P(expr);
			}
			smart_str_appendc(trace_str, ')');
			break;
		}
		default:
			append_variable(expr, trace_str);
			break;
	}
}
#else
static void append_flat_zval_r(zval *expr TSRMLS_DC, smart_str *trace_str, char depth)
{
	if (depth >= APM_G(dump_max_depth)) {
		smart_str_appendl(trace_str, "/* [...] */", sizeof("/* [...] */") - 1);
		return;
	}

	switch (Z_TYPE_P(expr)) {
		case IS_ARRAY:
			smart_str_appendc(trace_str, '[');
			if (++Z_ARRVAL_P(expr)->nApplyCount>1) {
				smart_str_appendl(trace_str, " *RECURSION*", sizeof(" *RECURSION*") - 1);
				Z_ARRVAL_P(expr)->nApplyCount--;
				return;
			}
			append_flat_hash(Z_ARRVAL_P(expr) TSRMLS_CC, trace_str, 0, depth + 1);
			smart_str_appendc(trace_str, ']');
			Z_ARRVAL_P(expr)->nApplyCount--;
			break;
		case IS_OBJECT:
		{
			HashTable *properties = NULL;
			char *class_name = NULL;
			zend_uint clen;
			if (Z_OBJ_HANDLER_P(expr, get_class_name)) {
				Z_OBJ_HANDLER_P(expr, get_class_name)(expr, (const char **) &class_name, &clen, 0 TSRMLS_CC);
			}
			if (class_name) {
				smart_str_appendl(trace_str, class_name, clen);
				smart_str_appendl(trace_str, " Object (", sizeof(" Object (") - 1);
			} else {
				smart_str_appendl(trace_str, "Unknown Class Object (", sizeof("Unknown Class Object (") - 1);
			}
			if (class_name) {
				efree(class_name);
			}
			if (Z_OBJ_HANDLER_P(expr, get_properties)) {
				properties = Z_OBJPROP_P(expr);
			}
			if (properties) {
				if (++properties->nApplyCount>1) {
					smart_str_appendl(trace_str, " *RECURSION*", sizeof(" *RECURSION*") - 1);
					properties->nApplyCount--;
					return;
				}
				append_flat_hash(properties TSRMLS_CC, trace_str, 1, depth + 1);
				properties->nApplyCount--;
			}
			smart_str_appendc(trace_str, ')');
			break;
		}
		default:
			append_variable(expr, trace_str);
			break;
	}
}
#endif

static void append_flat_hash(HashTable *ht TSRMLS_DC, smart_str *trace_str, char is_object, char depth)
{
	int i = 0;

#if PHP_VERSION_ID >= 70000
	zval *tmp;
	zend_string *string_key;
	zend_ulong num_key;

	ZEND_HASH_FOREACH_KEY_VAL_IND(ht, num_key, string_key, tmp) {
#else
	zval **tmp;
	char *string_key, *temp;
	ulong num_key;
	int new_len;
	uint str_len;
	HashPosition iterator;

	zend_hash_internal_pointer_reset_ex(ht, &iterator);
	while (zend_hash_get_current_data_ex(ht, (void **) &tmp, &iterator) == SUCCESS) {
#endif
		if (depth >= APM_G(dump_max_depth)) {
			smart_str_appendl(trace_str, "/* [...] */", sizeof("/* [...] */") - 1);
			return;
		}

		if (i++ > 0) {
			smart_str_appendl(trace_str, ", ", 2);
		}
		smart_str_appendc(trace_str, '[');
#if PHP_VERSION_ID >= 70000
		if (string_key) {
			smart_str_appendl(trace_str, ZSTR_VAL(string_key), ZSTR_LEN(string_key));
		} else {
			smart_str_append_long(trace_str, num_key);
		}

		smart_str_appendl(trace_str, "] => ", 5);
		append_flat_zval_r(tmp, trace_str, depth);
	} ZEND_HASH_FOREACH_END();
#else
		switch (zend_hash_get_current_key_ex(ht, &string_key, &str_len, &num_key, 0, &iterator)) {
			case HASH_KEY_IS_STRING:
				if (is_object) {
					if (*string_key == '\0') {
						do {
							++string_key;
							--str_len;
						} while (*(string_key) != '\0');
						++string_key;
						--str_len;
					}
				}
				smart_str_appendc(trace_str, '"');

				if (str_len > 0) {
					temp = apm_addslashes(string_key, str_len - 1, &new_len);
					smart_str_appendl(trace_str, temp, new_len);
					if (temp) {
						efree(temp);
					}
				}
				else
				{
					smart_str_appendl(trace_str, "*unknown key*", sizeof("*unknown key*") - 1);
				}

				smart_str_appendc(trace_str, '"');
				break;
			case HASH_KEY_IS_LONG:
				smart_str_append_long(trace_str, (long) num_key);
				break;
		}

		smart_str_appendl(trace_str, "] => ", 5);
		append_flat_zval_r(*tmp TSRMLS_CC, trace_str, depth);
		zend_hash_move_forward_ex(ht, &iterator);
	}
#endif
}

static int append_variable(zval *expr, smart_str *trace_str)
{
	zval expr_copy;
	int use_copy;
	char is_string = 0;
	char * temp;
	int new_len;

	if (Z_TYPE_P(expr) == IS_STRING) {
		smart_str_appendc(trace_str, '"');
		is_string = 1;
	}

#if PHP_VERSION_ID >= 70000
	use_copy = zend_make_printable_zval(expr, &expr_copy);
#else
	zend_make_printable_zval(expr, &expr_copy, &use_copy);
#endif
	if (use_copy) {
		expr = &expr_copy;
	}
	if (Z_STRLEN_P(expr) == 0) { /* optimize away empty strings */
		if (is_string) {
			smart_str_appendc(trace_str, '"');
		}
		if (use_copy) {
			zval_dtor(expr);
		}
		return 0;
	}

	if (is_string) {
		temp = apm_addslashes(Z_STRVAL_P(expr), Z_STRLEN_P(expr), &new_len);
		smart_str_appendl(trace_str, temp, new_len);
		smart_str_appendc(trace_str, '"');
		if (temp) {
			efree(temp);
		}
	} else {
		smart_str_appendl(trace_str, Z_STRVAL_P(expr), Z_STRLEN_P(expr));
	}

	if (use_copy) {
		zval_dtor(expr);
	}
	return Z_STRLEN_P(expr);
}

#if PHP_VERSION_ID >= 70000
static void debug_backtrace_get_args(zend_execute_data *call, zval *arg_array)
{
	uint32_t num_args = ZEND_CALL_NUM_ARGS(call);

	array_init_size(arg_array, num_args);
	if (num_args) {
		uint32_t i = 0;
		zval *p = ZEND_CALL_ARG(call, 1);

		zend_hash_real_init(Z_ARRVAL_P(arg_array), 1);
		ZEND_HASH_FILL_PACKED(Z_ARRVAL_P(arg_array)) {
			if (call->func->type == ZEND_USER_FUNCTION) {
				uint32_t first_extra_arg = call->func->op_array.num_args;

				if (ZEND_CALL_NUM_ARGS(call) > first_extra_arg) {
					while (i < first_extra_arg) {
						if (Z_OPT_REFCOUNTED_P(p)) Z_ADDREF_P(p);
						ZEND_HASH_FILL_ADD(p);
						zend_hash_next_index_insert_new(Z_ARRVAL_P(arg_array), p);
						p++;
						i++;
					}
					p = ZEND_CALL_VAR_NUM(call, call->func->op_array.last_var + call->func->op_array.T);
				}
			}

			while (i < num_args) {
				if (Z_OPT_REFCOUNTED_P(p)) Z_ADDREF_P(p);
				ZEND_HASH_FILL_ADD(p);
				p++;
				i++;
			}
		} ZEND_HASH_FILL_END();
	}
}
#else
static zval *debug_backtrace_get_args(void ***curpos TSRMLS_DC)
{
	void **p = *curpos;
	zval *arg_array, **arg;
	int arg_count =
	(int)(zend_uintptr_t) *p;

	MAKE_STD_ZVAL(arg_array);
	array_init_size(arg_array, arg_count);
	p -= arg_count;

	while (--arg_count >= 0) {
		arg = (zval **) p++;
		if (*arg) {
			if (Z_TYPE_PP(arg) != IS_OBJECT) {
				SEPARATE_ZVAL_TO_MAKE_IS_REF(arg);
			}
			Z_ADDREF_PP(arg);
			add_next_index_zval(arg_array, *arg);
		} else {
			add_next_index_null(arg_array);
		}
	}

	return arg_array;
}
#endif

static char *apm_addslashes(char *str, uint length, int *new_length)
{
	/* maximum string length, worst case situation */
	char *new_str;
	char *source, *target;
	char *end;
	int local_new_length;

	if (!new_length) {
		new_length = &local_new_length;
	}

	if (!str) {
		*new_length = 0;
		return str;
	}
	new_str = (char *) safe_emalloc(2, (length ? length : (length = strlen(str))), 1);
	source = str;
	end = source + length;
	target = new_str;

	while (source < end) {
		switch (*source) {
			case '\0':
				*target++ = '\\';
				*target++ = '0';
				break;
			case '\"':
			case '\\':
				*target++ = '\\';
				/* break is missing *intentionally* */
			default:
				*target++ = *source;
				break;
		}

		source++;
	}

	*target = 0;
	*new_length = target - new_str;
	return (char *) erealloc(new_str, *new_length + 1);
}
