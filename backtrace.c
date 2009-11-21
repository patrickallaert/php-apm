/*
 +----------------------------------------------------------------------+
 |  APM stands for Alternative PHP Monitor                              |
 +----------------------------------------------------------------------+
 | Copyright (c) 2008-2009  Davide Mendolia, Patrick Allaert            |
 +----------------------------------------------------------------------+
 | This source file is subject to version 3.01 of the PHP license,      |
 | that is bundled with this package in the file LICENSE, and is        |
 | available through the world-wide-web at the following url:           |
 | http://www.php.net/license/3_01.txt                                  |
 | If you did not receive a copy of the PHP license and are unable to   |
 | obtain it through the world-wide-web, please send a note to          |
 | license@php.net so we can mail you a copy immediately.               |
 +----------------------------------------------------------------------+
 */

#include "php.h"
#include "ext/standard/php_smart_str.h"

void debug_print_backtrace_args(zval *arg_array TSRMLS_DC, smart_str *trace_str);
void append_flat_zval_r(zval *expr TSRMLS_DC, smart_str *trace_str);
static void append_flat_hash(HashTable *ht TSRMLS_DC, smart_str *trace_str);
static zval *debug_backtrace_get_args(void ***curpos TSRMLS_DC);


extern void append_backtrace(smart_str *trace_str)
{
    /* backtrace variables */
        zend_execute_data *ptr, *skip;
	int lineno;
	char *function_name;
	char *filename;
	char *class_name = NULL;
	char *call_type;
	char *include_filename = NULL;
        zval *arg_array = NULL;
#if PHP_API_VERSION < 20090626
	void **cur_arg_pos = EG(argument_stack).top_element;
	void **args = cur_arg_pos;
	int arg_stack_consistent = 0;
	int frames_on_stack = 0;
#endif
        int indent = 0;


#if PHP_API_VERSION < 20090626
	while (--args > EG(argument_stack).elements) {
		if (*args--) {
			break;
		}
		args -= *(ulong*)args;
		frames_on_stack++;

		/* skip args from incomplete frames */
		while (((args-1) > EG(argument_stack).elements) && *(args-1)) {
			args--;
		}

		if ((args-1) == EG(argument_stack).elements) {
			arg_stack_consistent = 1;
			break;
		}
	}
#endif
	ptr = EG(current_execute_data);

#if PHP_API_VERSION < 20090626
	cur_arg_pos -= 2;
	frames_on_stack--;
#endif

	while (ptr) {
		char *free_class_name = NULL;

		class_name = call_type = NULL;
		arg_array = NULL;

		skip = ptr;
		/* skip internal handler */
		if (!skip->op_array &&
		    skip->prev_execute_data &&
		    skip->prev_execute_data->opline &&
		    skip->prev_execute_data->opline->opcode != ZEND_DO_FCALL &&
		    skip->prev_execute_data->opline->opcode != ZEND_DO_FCALL_BY_NAME &&
		    skip->prev_execute_data->opline->opcode != ZEND_INCLUDE_OR_EVAL) {
		  skip = skip->prev_execute_data;
		}

		if (skip->op_array) {
			filename = skip->op_array->filename;
			lineno = skip->opline->lineno;
		} else {
			filename = NULL;
			lineno = 0;
		}

		function_name = ptr->function_state.function->common.function_name;

		if (function_name) {
			if (ptr->object) {
				if (ptr->function_state.function->common.scope) {
					class_name = ptr->function_state.function->common.scope->name;
				} else {
					zend_uint class_name_len;
					int dup;

					dup = zend_get_object_classname(ptr->object, &class_name, &class_name_len TSRMLS_CC);
					if(!dup) {
						free_class_name = class_name;
					}
				}

				call_type = "->";
			} else if (ptr->function_state.function->common.scope) {
				class_name = ptr->function_state.function->common.scope->name;
				call_type = "::";
			} else {
				class_name = NULL;
				call_type = NULL;
			}
			if ((! ptr->opline) || ((ptr->opline->opcode == ZEND_DO_FCALL_BY_NAME) || (ptr->opline->opcode == ZEND_DO_FCALL))) {
#if PHP_API_VERSION >= 20090626
				if (ptr->function_state.arguments) {
					arg_array = debug_backtrace_get_args(&ptr->function_state.arguments TSRMLS_CC);
				}
#else
                            if (arg_stack_consistent && (frames_on_stack > 0)) {
					arg_array = debug_backtrace_get_args(&cur_arg_pos TSRMLS_CC);
					frames_on_stack--;
				}
#endif
			}
		} else {
			/* i know this is kinda ugly, but i'm trying to avoid extra cycles in the main execution loop */
			zend_bool build_filename_arg = 1;

			if (!ptr->opline || ptr->opline->opcode != ZEND_INCLUDE_OR_EVAL) {
				/* can happen when calling eval from a custom sapi */
				function_name = "unknown";
				build_filename_arg = 0;
			} else
			switch (Z_LVAL(ptr->opline->op2.u.constant)) {
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
				MAKE_STD_ZVAL(arg_array);
				array_init(arg_array);
				add_next_index_string(arg_array, include_filename, 1);
			}
			call_type = NULL;
		}
		smart_str_appendc(trace_str, '#');
		smart_str_append_long(trace_str, indent);
		smart_str_appendc(trace_str, ' ');
		if (class_name) {
			smart_str_appends(trace_str, class_name);
			smart_str_appends(trace_str, call_type);
		}
		smart_str_appends(trace_str, function_name?function_name:"main");
		smart_str_appendc(trace_str, '(');
		if (arg_array) {
			debug_print_backtrace_args(arg_array TSRMLS_CC, trace_str);
			zval_ptr_dtor(&arg_array);
		}
		if (filename) {
			smart_str_appends(trace_str, ") called at [");
			smart_str_appends(trace_str, filename);
			smart_str_appendc(trace_str, ':');
			smart_str_append_long(trace_str, lineno);
			smart_str_appends(trace_str, "]\n");
		} else {
			zend_execute_data *prev = skip->prev_execute_data;

			while (prev) {
				if (prev->function_state.function &&
					prev->function_state.function->common.type != ZEND_USER_FUNCTION) {
					prev = NULL;
					break;
				}
				if (prev->op_array) {
					smart_str_appends(trace_str, ") called at [");
					smart_str_appends(trace_str, prev->op_array->filename);
					smart_str_appendc(trace_str, ':');
					smart_str_append_long(trace_str, prev->opline->lineno);
					smart_str_appends(trace_str, "]\n");
					break;
				}
				prev = prev->prev_execute_data;
			}
			if (!prev) {
				smart_str_appends(trace_str, ")\n");
			}
		}
		include_filename = filename;
		ptr = skip->prev_execute_data;
		++indent;
		if (free_class_name) {
			efree(free_class_name);
		}
	}
}

void debug_print_backtrace_args(zval *arg_array TSRMLS_DC, smart_str *trace_str)
{
	zval **tmp;
	HashPosition iterator;
	int i = 0;

	zend_hash_internal_pointer_reset_ex(arg_array->value.ht, &iterator);
	while (zend_hash_get_current_data_ex(arg_array->value.ht, (void **) &tmp, &iterator) == SUCCESS) {
		if (i++) {
                    smart_str_appends(trace_str, ", ");
		}
		append_flat_zval_r(*tmp TSRMLS_CC, trace_str);
		zend_hash_move_forward_ex(arg_array->value.ht, &iterator);
	}
}


void append_flat_zval_r(zval *expr TSRMLS_DC, smart_str *trace_str) /* {{{ */
{
	switch (Z_TYPE_P(expr)) {
		case IS_ARRAY:
                        smart_str_appends(trace_str, "Array (");
			if (++Z_ARRVAL_P(expr)->nApplyCount>1) {
				smart_str_appends(trace_str, " *RECURSION*");
				Z_ARRVAL_P(expr)->nApplyCount--;
				return;
			}
			append_flat_hash(Z_ARRVAL_P(expr) TSRMLS_CC, trace_str);
			smart_str_appends(trace_str, ")");
			Z_ARRVAL_P(expr)->nApplyCount--;
			break;
		case IS_OBJECT:
		{
			HashTable *properties = NULL;
			char *class_name = NULL;
			zend_uint clen;

			if (Z_OBJ_HANDLER_P(expr, get_class_name)) {
				Z_OBJ_HANDLER_P(expr, get_class_name)(expr, &class_name, &clen, 0 TSRMLS_CC);
			}
			if (class_name) {
                                smart_str_appends(trace_str, class_name);
				smart_str_appends(trace_str, " Object (");
			} else {
				smart_str_appends(trace_str, "Unknown Class Object (");
			}
			if (class_name) {
				efree(class_name);
			}
			if (Z_OBJ_HANDLER_P(expr, get_properties)) {
				properties = Z_OBJPROP_P(expr);
			}
			if (properties) {
				if (++properties->nApplyCount>1) {
					smart_str_appends(trace_str, " *RECURSION*");
					properties->nApplyCount--;
					return;
				}
				append_flat_hash(properties TSRMLS_CC, trace_str);
				properties->nApplyCount--;
			}
			smart_str_appends(trace_str, ")");
			break;
		}
		default:
			append_variable(expr, trace_str);
			break;
	}
}

static void append_flat_hash(HashTable *ht TSRMLS_DC, smart_str *trace_str) /* {{{ */
{
	zval **tmp;
	char *string_key;
	HashPosition iterator;
	ulong num_key;
	uint str_len;
	int i = 0;

	zend_hash_internal_pointer_reset_ex(ht, &iterator);
	while (zend_hash_get_current_data_ex(ht, (void **) &tmp, &iterator) == SUCCESS) {
		if (i++ > 0) {
			smart_str_appends(trace_str, ",");
		}
		smart_str_appends(trace_str, "[");
		switch (zend_hash_get_current_key_ex(ht, &string_key, &str_len, &num_key, 0, &iterator)) {
			case HASH_KEY_IS_STRING:
				smart_str_appends(trace_str, string_key);
				break;
			case HASH_KEY_IS_LONG:
				smart_str_append_long(trace_str, num_key);
				break;
		}
		smart_str_appends(trace_str, "] => ");
		append_flat_zval_r(*tmp TSRMLS_CC, trace_str);
		zend_hash_move_forward_ex(ht, &iterator);
	}
}

int append_variable(zval *expr, smart_str *trace_str) /* {{{ */
{
	zval expr_copy;
	int use_copy;

	zend_make_printable_zval(expr, &expr_copy, &use_copy);
	if (use_copy) {
		expr = &expr_copy;
	}
	if (Z_STRLEN_P(expr) == 0) { /* optimize away empty strings */
		if (use_copy) {
			zval_dtor(expr);
		}
		return 0;
	}
	smart_str_appends(trace_str, (Z_STRVAL_P(expr)));
	if (use_copy) {
		zval_dtor(expr);
	}
	return Z_STRLEN_P(expr);
}

static zval *debug_backtrace_get_args(void ***curpos TSRMLS_DC)
{
#if PHP_API_VERSION >= 20090626
	void **p = *curpos;
#else
        void **p = *curpos - 2;
#endif
        zval *arg_array, **arg;
	int arg_count = (int)(zend_uintptr_t) *p;
#if PHP_API_VERSION < 20090626
 	*curpos -= (arg_count+2);

#endif
	MAKE_STD_ZVAL(arg_array);
#if PHP_API_VERSION >= 20090626
	array_init_size(arg_array, arg_count);
#else
	array_init(arg_array);
#endif
	p -= arg_count;

	while (--arg_count >= 0) {
		arg = (zval **) p++;
		if (*arg) {
			if (Z_TYPE_PP(arg) != IS_OBJECT) {
				SEPARATE_ZVAL_TO_MAKE_IS_REF(arg);
			}
#if PHP_API_VERSION >= 20090626
			Z_ADDREF_PP(arg);
#else
			(*arg)->refcount++;
#endif
			add_next_index_zval(arg_array, *arg);
		} else {
			add_next_index_null(arg_array);
		}
	}

#if PHP_API_VERSION < 20090626
	/* skip args from incomplete frames */
	while ((((*curpos)-1) > EG(argument_stack).elements) && *((*curpos)-1)) {
		(*curpos)--;
	}

#endif
	return arg_array;
}