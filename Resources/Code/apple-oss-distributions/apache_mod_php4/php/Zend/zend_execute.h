/*
   +----------------------------------------------------------------------+
   | Zend Engine                                                          |
   +----------------------------------------------------------------------+
   | Copyright (c) 1998-2003 Zend Technologies Ltd. (http://www.zend.com) |
   +----------------------------------------------------------------------+
   | This source file is subject to version 2.00 of the Zend license,     |
   | that is bundled with this package in the file LICENSE, and is        | 
   | available through the world-wide-web at the following url:           |
   | http://www.zend.com/license/2_00.txt.                                |
   | If you did not receive a copy of the Zend license and are unable to  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@zend.com so we can mail you a copy immediately.              |
   +----------------------------------------------------------------------+
   | Authors: Andi Gutmans <andi@zend.com>                                |
   |          Zeev Suraski <zeev@zend.com>                                |
   +----------------------------------------------------------------------+
*/


#ifndef ZEND_EXECUTE_H
#define ZEND_EXECUTE_H

#include "zend_compile.h"
#include "zend_hash.h"
#include "zend_variables.h"
#include "zend_operators.h"
#include "zend_execute_globals.h"

BEGIN_EXTERN_C()
ZEND_API extern void (*zend_execute)(zend_op_array *op_array TSRMLS_DC);
ZEND_API extern void (*zend_execute_internal)(zend_execute_data *execute_data_ptr, int return_value_used TSRMLS_DC);

void init_executor(TSRMLS_D);
void shutdown_executor(TSRMLS_D);
ZEND_API void execute(zend_op_array *op_array TSRMLS_DC);
ZEND_API int zend_is_true(zval *op);
ZEND_API void execute_internal(zend_execute_data *execute_data_ptr, int return_value_used TSRMLS_DC);
static inline void safe_free_zval_ptr(zval *p)
{
	TSRMLS_FETCH();

	if (p!=EG(uninitialized_zval_ptr)) {
		FREE_ZVAL(p);
	}
}

ZEND_API int zend_eval_string(char *str, zval *retval_ptr, char *string_name TSRMLS_DC);
static inline int i_zend_is_true(zval *op)
{
	int result;

	switch (op->type) {
		case IS_NULL:
			result = 0;
			break;
		case IS_LONG:
		case IS_BOOL:
		case IS_RESOURCE:
			result = (op->value.lval?1:0);
			break;
		case IS_DOUBLE:
			result = (op->value.dval ? 1 : 0);
			break;
		case IS_STRING:
			if (op->value.str.len == 0
				|| (op->value.str.len==1 && op->value.str.val[0]=='0')) {
				result = 0;
			} else {
				result = 1;
			}
			break;
		case IS_ARRAY:
			result = (zend_hash_num_elements(op->value.ht)?1:0);
			break;
		case IS_OBJECT:
			result = (zend_hash_num_elements(Z_OBJPROP_P(op))?1:0);
			break;
		default:
			result = 0;
			break;
	}
	return result;
}

ZEND_API int zval_update_constant(zval **pp, void *arg TSRMLS_DC);

/* dedicated Zend executor functions - do not use! */
static inline void zend_ptr_stack_clear_multiple(TSRMLS_D)
{
	void **p = EG(argument_stack).top_element-2;
	int delete_count = (ulong) *p;

	EG(argument_stack).top -= (delete_count+2);
	while (--delete_count>=0) {
		zval_ptr_dtor((zval **) --p);
	}
	EG(argument_stack).top_element = p;
}

static inline int zend_ptr_stack_get_arg(int requested_arg, void **data TSRMLS_DC)
{
	void **p = EG(argument_stack).top_element-2;
	int arg_count = (ulong) *p;

	if (requested_arg>arg_count) {
		return FAILURE;
	}
	*data = (p-arg_count+requested_arg-1);
	return SUCCESS;
}

void execute_new_code(TSRMLS_D);


/* services */
ZEND_API char *get_active_function_name(TSRMLS_D);
ZEND_API char *zend_get_executed_filename(TSRMLS_D);
ZEND_API uint zend_get_executed_lineno(TSRMLS_D);
ZEND_API zend_bool zend_is_executing(TSRMLS_D);

ZEND_API void zend_set_timeout(long seconds);
ZEND_API void zend_unset_timeout(TSRMLS_D);
ZEND_API void zend_timeout(int dummy);

#ifdef ZEND_WIN32
void zend_init_timeout_thread();
void zend_shutdown_timeout_thread();
#define WM_REGISTER_ZEND_TIMEOUT		(WM_USER+1)
#define WM_UNREGISTER_ZEND_TIMEOUT		(WM_USER+2)
#endif

#define zendi_zval_copy_ctor(p) zval_copy_ctor(&(p))
#define zendi_zval_dtor(p) zval_dtor(&(p))

#define active_opline (*EG(opline_ptr))

void zend_assign_to_variable_reference(znode *result, zval **variable_ptr_ptr, zval **value_ptr_ptr, temp_variable *Ts TSRMLS_DC);

#define IS_OVERLOADED_OBJECT 1
#define IS_STRING_OFFSET 2

END_EXTERN_C()

#endif /* ZEND_EXECUTE_H */
