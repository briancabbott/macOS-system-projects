/* 
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2008 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Andi Gutmans <andi@zend.com>                                |
   |          Zeev Suraski <zeev@zend.com>                                |
   |          Rasmus Lerdorf <rasmus@php.net>                             |
   |          Andrei Zmievski <andrei@php.net>                            |
   |          Stig Venaas <venaas@php.net>                                |
   |          Jason Greene <jason@php.net>                                |
   +----------------------------------------------------------------------+
*/


/* $Id: array.c,v 1.199.2.44.2.17 2007/12/31 07:22:51 sebastian Exp $ */

#include "php.h"
#include "php_ini.h"
#include <stdarg.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdio.h>
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#ifdef PHP_WIN32
#include "win32/unistd.h"
#endif
#include "zend_globals.h"
#include "php_globals.h"
#include "php_array.h"
#include "basic_functions.h"
#include "php_string.h"
#include "php_rand.h"
#include "php_smart_str.h"

#ifdef ZTS
int array_globals_id;
#else
php_array_globals array_globals;
#endif

#define EXTR_OVERWRITE			0
#define EXTR_SKIP				1
#define EXTR_PREFIX_SAME		2
#define	EXTR_PREFIX_ALL			3
#define	EXTR_PREFIX_INVALID		4
#define	EXTR_PREFIX_IF_EXISTS	5
#define	EXTR_IF_EXISTS			6

#define EXTR_REFS				0x100

#define SORT_REGULAR			0
#define SORT_NUMERIC			1
#define	SORT_STRING				2
#define	SORT_LOCALE_STRING      5

#define SORT_DESC				3
#define SORT_ASC				4

#define CASE_LOWER				0
#define CASE_UPPER				1

#define COUNT_NORMAL			0
#define COUNT_RECURSIVE			1

#define DIFF_NORMAL			0
#define DIFF_ASSOC			1

#define INTERSECT_NORMAL		0
#define INTERSECT_ASSOC			1

PHP_MINIT_FUNCTION(array)
{
#ifdef ZTS
	ts_allocate_id(&array_globals_id, sizeof(php_array_globals), NULL, NULL);
#endif

	REGISTER_LONG_CONSTANT("EXTR_OVERWRITE", EXTR_OVERWRITE, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("EXTR_SKIP", EXTR_SKIP, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("EXTR_PREFIX_SAME", EXTR_PREFIX_SAME, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("EXTR_PREFIX_ALL", EXTR_PREFIX_ALL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("EXTR_PREFIX_INVALID", EXTR_PREFIX_INVALID, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("EXTR_PREFIX_IF_EXISTS", EXTR_PREFIX_IF_EXISTS, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("EXTR_IF_EXISTS", EXTR_IF_EXISTS, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("EXTR_REFS", EXTR_REFS, CONST_CS | CONST_PERSISTENT);
	
	REGISTER_LONG_CONSTANT("SORT_ASC", SORT_ASC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SORT_DESC", SORT_DESC, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("SORT_REGULAR", SORT_REGULAR, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SORT_NUMERIC", SORT_NUMERIC, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SORT_STRING", SORT_STRING, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("SORT_LOCALE_STRING", SORT_LOCALE_STRING, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("CASE_LOWER", CASE_LOWER, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("CASE_UPPER", CASE_UPPER, CONST_CS | CONST_PERSISTENT);

	REGISTER_LONG_CONSTANT("COUNT_NORMAL", COUNT_NORMAL, CONST_CS | CONST_PERSISTENT);
	REGISTER_LONG_CONSTANT("COUNT_RECURSIVE", COUNT_RECURSIVE, CONST_CS | CONST_PERSISTENT);
	
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(array)
{
#ifdef ZTS
	ts_free_id(array_globals_id);
#endif

	return SUCCESS;
}

static void set_compare_func(int sort_type TSRMLS_DC)
{
	switch (sort_type) {
		case SORT_NUMERIC:
			ARRAYG(compare_func) = numeric_compare_function;
			break;

		case SORT_STRING:
			ARRAYG(compare_func) = string_compare_function;
			break;

#if HAVE_STRCOLL
		case SORT_LOCALE_STRING:
			ARRAYG(compare_func) = string_locale_compare_function;
			break;
#endif

		case SORT_REGULAR:
		default:
			ARRAYG(compare_func) = compare_function;
			break;
	}
}

static int array_key_compare(const void *a, const void *b TSRMLS_DC)
{
	Bucket *f;
	Bucket *s;
	zval result;
	zval first;
	zval second;
 
	f = *((Bucket **) a);
	s = *((Bucket **) b);

	if (f->nKeyLength == 0) {
		Z_TYPE(first) = IS_LONG;
		Z_LVAL(first) = f->h;
	} else {
		Z_TYPE(first) = IS_STRING;
		Z_STRVAL(first) = f->arKey;
		Z_STRLEN(first) = f->nKeyLength-1;
	}

	if (s->nKeyLength == 0) {
		Z_TYPE(second) = IS_LONG;
		Z_LVAL(second) = s->h;
	} else {
		Z_TYPE(second) = IS_STRING;
		Z_STRVAL(second) = s->arKey;
		Z_STRLEN(second) = s->nKeyLength-1;
	}
 
	if (ARRAYG(compare_func)(&result, &first, &second TSRMLS_CC) == FAILURE) {
		return 0;
	} 

	if (Z_TYPE(result) == IS_DOUBLE) {
		if (Z_DVAL(result) < 0) {
			return -1;
		} else if (Z_DVAL(result) > 0) {
			return 1;
		} else {
			return 0;
		}
	}

	convert_to_long(&result);

	if (Z_LVAL(result) < 0) {
		return -1;
	} else if (Z_LVAL(result) > 0) {
		return 1;
	} 

	return 0;
}

static int array_reverse_key_compare(const void *a, const void *b TSRMLS_DC)
{
	return array_key_compare(a, b TSRMLS_CC) * -1;
}

/* {{{ proto bool krsort(array array_arg [, int sort_flags])
   Sort an array by key value in reverse order */
PHP_FUNCTION(krsort)
{
	zval *array;
	long sort_type = SORT_REGULAR;
	HashTable *target_hash;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|l", &array, &sort_type) == FAILURE) {
		RETURN_FALSE;
	}
	
	target_hash = HASH_OF(array);
	set_compare_func(sort_type TSRMLS_CC);
	
	if (zend_hash_sort(target_hash, zend_qsort, array_reverse_key_compare, 0 TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool ksort(array array_arg [, int sort_flags])
   Sort an array by key */
PHP_FUNCTION(ksort)
{
	zval *array;
	long sort_type = SORT_REGULAR;
	HashTable *target_hash;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|l", &array, &sort_type) == FAILURE) {
		RETURN_FALSE;
	}

	target_hash = HASH_OF(array);
	set_compare_func(sort_type TSRMLS_CC);
	
	if (zend_hash_sort(target_hash, zend_qsort, array_key_compare, 0 TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */


static int php_count_recursive(zval *array, long mode TSRMLS_DC)
{
	long cnt = 0;
	zval **element;

	if (Z_TYPE_P(array) == IS_ARRAY) {
		cnt = zend_hash_num_elements(Z_ARRVAL_P(array));
		if (mode == COUNT_RECURSIVE) {
			HashPosition pos;

			for (zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(array), &pos);
				 zend_hash_get_current_data_ex(Z_ARRVAL_P(array), (void **) &element, &pos) == SUCCESS;
				 zend_hash_move_forward_ex(Z_ARRVAL_P(array), &pos)) {
				cnt += php_count_recursive(*element, COUNT_RECURSIVE TSRMLS_CC);
			}
		}
	}

	return cnt;
}

/* {{{ proto int count(mixed var [, int mode])
   Count the number of elements in a variable (usually an array) */
PHP_FUNCTION(count)
{
	zval *array;
	long mode = COUNT_NORMAL;
	
	if (zend_parse_parameters (ZEND_NUM_ARGS() TSRMLS_CC, "z|l", &array, &mode) == FAILURE)
		return;
	
	switch (Z_TYPE_P(array)) {
		case IS_NULL:
			RETURN_LONG(0);
			break;
		case IS_ARRAY:
			RETURN_LONG (php_count_recursive (array, mode TSRMLS_CC));
			break;
		default:
			RETURN_LONG(1);
			break;
	}
}
/* }}} */

/* Numbers are always smaller than strings int this function as it
 * anyway doesn't make much sense to compare two different data types.
 * This keeps it consistant and simple.
 *
 * This is not correct any more, depends on what compare_func is set to.
 */
static int array_data_compare(const void *a, const void *b TSRMLS_DC)
{
	Bucket *f;
	Bucket *s;
	pval result;
	pval *first;
	pval *second;
 
	f = *((Bucket **) a);
	s = *((Bucket **) b);
 
	first = *((pval **) f->pData);
	second = *((pval **) s->pData);

	if (ARRAYG(compare_func)(&result, first, second TSRMLS_CC) == FAILURE) {
		return 0;
	} 

	if (Z_TYPE(result) == IS_DOUBLE) {
		if (Z_DVAL(result) < 0) {
			return -1;
		} else if (Z_DVAL(result) > 0) {
			return 1;
		} else {
			return 0;
		}
	}

	convert_to_long(&result);

	if (Z_LVAL(result) < 0) {
		return -1;
	} else if (Z_LVAL(result) > 0) {
		return 1;
	} 

	return 0;
}

static int array_reverse_data_compare(const void *a, const void *b TSRMLS_DC)
{
	return array_data_compare(a, b TSRMLS_CC)*-1;
}

static int array_natural_general_compare(const void *a, const void *b, int fold_case)
{
	Bucket *f, *s;
	zval *fval, *sval;
	zval first, second;
	int result;
 
	f = *((Bucket **) a);
	s = *((Bucket **) b);
 
	fval = *((pval **) f->pData);
	sval = *((pval **) s->pData);
	first = *fval;
	second = *sval;
	if (Z_TYPE_P(fval) != IS_STRING) {
		zval_copy_ctor(&first);
		convert_to_string(&first);
	}
	if (Z_TYPE_P(sval) != IS_STRING) {
		zval_copy_ctor(&second);
		convert_to_string(&second);
	}

	result = strnatcmp_ex(Z_STRVAL(first), Z_STRLEN(first),
						  Z_STRVAL(second), Z_STRLEN(second), fold_case);

	if (Z_TYPE_P(fval) != IS_STRING)
		zval_dtor(&first);
	if (Z_TYPE_P(sval) != IS_STRING)
		zval_dtor(&second);
	
	return result;
}

static int array_natural_compare(const void *a, const void *b TSRMLS_DC)
{
	return array_natural_general_compare(a, b, 0);
}

static int array_natural_case_compare(const void *a, const void *b TSRMLS_DC)
{
	return array_natural_general_compare(a, b, 1);
}

static void php_natsort(INTERNAL_FUNCTION_PARAMETERS, int fold_case)
{
	zval **array;
	HashTable *target_hash;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &array) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	target_hash = HASH_OF(*array);
	if (!target_hash) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The argument should be an array");
		return;
	}

	if (fold_case) {
		if (zend_hash_sort(target_hash, zend_qsort, array_natural_case_compare, 0 TSRMLS_CC) == FAILURE) {
			return;
		}
	} else {
		if (zend_hash_sort(target_hash, zend_qsort, array_natural_compare, 0 TSRMLS_CC) == FAILURE) {
			return;
		}
	}

	RETURN_TRUE;
}


/* {{{ proto void natsort(array array_arg)
   Sort an array using natural sort */
PHP_FUNCTION(natsort)
{
	php_natsort(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */


/* {{{ proto void natcasesort(array array_arg)
   Sort an array using case-insensitive natural sort */
PHP_FUNCTION(natcasesort)
{
	php_natsort(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */


/* {{{ proto bool asort(array array_arg [, int sort_flags])
   Sort an array and maintain index association */
PHP_FUNCTION(asort)
{
	zval *array;
	long sort_type = SORT_REGULAR;
	HashTable *target_hash;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|l", &array, &sort_type) == FAILURE) {
		RETURN_FALSE;
	}
	
	target_hash = HASH_OF(array);
	set_compare_func(sort_type TSRMLS_CC);
	
	if (zend_hash_sort(target_hash, zend_qsort, array_data_compare, 0 TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool arsort(array array_arg [, int sort_flags])
   Sort an array in reverse order and maintain index association */
PHP_FUNCTION(arsort)
{
	zval *array;
	long sort_type = SORT_REGULAR;
	HashTable *target_hash;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|l", &array, &sort_type) == FAILURE) {
		RETURN_FALSE;
	}
	
	target_hash = HASH_OF(array);
	set_compare_func(sort_type TSRMLS_CC);
	
	if (zend_hash_sort(target_hash, zend_qsort, array_reverse_data_compare, 0 TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool sort(array array_arg [, int sort_flags])
   Sort an array */
PHP_FUNCTION(sort)
{
	zval *array;
	long sort_type = SORT_REGULAR;
	HashTable *target_hash;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|l", &array, &sort_type) == FAILURE) {
		RETURN_FALSE;
	}
	
	target_hash = HASH_OF(array);
	set_compare_func(sort_type TSRMLS_CC);
	
	if (zend_hash_sort(target_hash, zend_qsort, array_data_compare, 1 TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool rsort(array array_arg [, int sort_flags])
   Sort an array in reverse order */
PHP_FUNCTION(rsort)
{
	zval *array;
	long sort_type = SORT_REGULAR;
	HashTable *target_hash;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "a|l", &array, &sort_type) == FAILURE) {
		RETURN_FALSE;
	}
	
	target_hash = HASH_OF(array);
	set_compare_func(sort_type TSRMLS_CC);
	
	if (zend_hash_sort(target_hash, zend_qsort, array_reverse_data_compare, 1 TSRMLS_CC) == FAILURE) {
		RETURN_FALSE;
	}
	RETURN_TRUE;
}
/* }}} */

static int array_user_compare(const void *a, const void *b TSRMLS_DC)
{
	Bucket *f;
	Bucket *s;
	zval **args[2];
	zval *retval_ptr;

	f = *((Bucket **) a);
	s = *((Bucket **) b);

	args[0] = (zval **) f->pData;
	args[1] = (zval **) s->pData;

	if (call_user_function_ex(EG(function_table), NULL, *BG(user_compare_func_name), &retval_ptr, 2, args, 0, NULL TSRMLS_CC)==SUCCESS
		&& retval_ptr) {
		long retval;

		convert_to_long_ex(&retval_ptr);
		retval = Z_LVAL_P(retval_ptr);
		zval_ptr_dtor(&retval_ptr);
		return retval;
	} else {
		return 0;
	}
}

/* check is comparison function is valid */
#define PHP_ARRAY_CMP_FUNC_CHECK(func_name)	\
	if (!zend_is_callable(*func_name, 0, NULL)) {	\
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Invalid comparison function.");	\
		BG(user_compare_func_name) = old_compare_func;	\
		RETURN_FALSE;	\
	}	\

/* {{{ proto bool usort(array array_arg, string cmp_function)
   Sort an array by values using a user-defined comparison function */
PHP_FUNCTION(usort)
{
	zval **array;
	zval **old_compare_func;
	HashTable *target_hash;

	old_compare_func = BG(user_compare_func_name);
	if (ZEND_NUM_ARGS() != 2 || zend_get_parameters_ex(2, &array, &BG(user_compare_func_name)) == FAILURE) {
		BG(user_compare_func_name) = old_compare_func;
		WRONG_PARAM_COUNT;
	}
	target_hash = HASH_OF(*array);
	if (!target_hash) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The argument should be an array");
		BG(user_compare_func_name) = old_compare_func;
		RETURN_FALSE;
	}

	PHP_ARRAY_CMP_FUNC_CHECK(BG(user_compare_func_name))
	
	if (zend_hash_sort(target_hash, zend_qsort, array_user_compare, 1 TSRMLS_CC) == FAILURE) {
		BG(user_compare_func_name) = old_compare_func;
		RETURN_FALSE;
	}
	BG(user_compare_func_name) = old_compare_func;
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto bool uasort(array array_arg, string cmp_function)
   Sort an array with a user-defined comparison function and maintain index association */
PHP_FUNCTION(uasort)
{
	zval **array;
	zval **old_compare_func;
	HashTable *target_hash;

	old_compare_func = BG(user_compare_func_name);
	if (ZEND_NUM_ARGS() != 2 || zend_get_parameters_ex(2, &array, &BG(user_compare_func_name)) == FAILURE) {
		BG(user_compare_func_name) = old_compare_func;
		WRONG_PARAM_COUNT;
	}
	target_hash = HASH_OF(*array);
	if (!target_hash) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The argument should be an array");
		BG(user_compare_func_name) = old_compare_func;
		RETURN_FALSE;
	}

	PHP_ARRAY_CMP_FUNC_CHECK(BG(user_compare_func_name))

	if (zend_hash_sort(target_hash, zend_qsort, array_user_compare, 0 TSRMLS_CC) == FAILURE) {
		BG(user_compare_func_name) = old_compare_func;
		RETURN_FALSE;
	}
	BG(user_compare_func_name) = old_compare_func;
	RETURN_TRUE;
}
/* }}} */

static int array_user_key_compare(const void *a, const void *b TSRMLS_DC)
{
	Bucket *f;
	Bucket *s;
	zval *key1, *key2;
	zval *args[2];
	zval retval;
	int status;

	ALLOC_INIT_ZVAL(key1);
	ALLOC_INIT_ZVAL(key2);
	args[0] = key1;
	args[1] = key2;
	
	f = *((Bucket **) a);
	s = *((Bucket **) b);

	if (f->nKeyLength) {
		Z_STRVAL_P(key1) = estrndup(f->arKey, f->nKeyLength-1);
		Z_STRLEN_P(key1) = f->nKeyLength-1;
		Z_TYPE_P(key1) = IS_STRING;
	} else {
		Z_LVAL_P(key1) = f->h;
		Z_TYPE_P(key1) = IS_LONG;
	}
	if (s->nKeyLength) {
		Z_STRVAL_P(key2) = estrndup(s->arKey, s->nKeyLength-1);
		Z_STRLEN_P(key2) = s->nKeyLength-1;
		Z_TYPE_P(key2) = IS_STRING;
	} else {
		Z_LVAL_P(key2) = s->h;
		Z_TYPE_P(key2) = IS_LONG;
	}

	status = call_user_function(EG(function_table), NULL, *BG(user_compare_func_name), &retval, 2, args TSRMLS_CC);
	
	zval_ptr_dtor(&key1);
	zval_ptr_dtor(&key2);
	
	if (status == SUCCESS) {
		convert_to_long(&retval);
		return Z_LVAL(retval);
	} else {
		return 0;
	}
}

/* {{{ proto bool uksort(array array_arg, string cmp_function)
   Sort an array by keys using a user-defined comparison function */
PHP_FUNCTION(uksort)
{
	zval **array;
	zval **old_compare_func;
	HashTable *target_hash;

	old_compare_func = BG(user_compare_func_name);
	if (ZEND_NUM_ARGS() != 2 || zend_get_parameters_ex(2, &array, &BG(user_compare_func_name)) == FAILURE) {
		BG(user_compare_func_name) = old_compare_func;
		WRONG_PARAM_COUNT;
	}
	target_hash = HASH_OF(*array);
	if (!target_hash) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The argument should be an array");
		BG(user_compare_func_name) = old_compare_func;
		RETURN_FALSE;
	}

	PHP_ARRAY_CMP_FUNC_CHECK(BG(user_compare_func_name))

	if (zend_hash_sort(target_hash, zend_qsort, array_user_key_compare, 0 TSRMLS_CC) == FAILURE) {
		BG(user_compare_func_name) = old_compare_func;
		RETURN_FALSE;
	}
	BG(user_compare_func_name) = old_compare_func;
	RETURN_TRUE;
}
/* }}} */

/* {{{ proto mixed end(array array_arg)
   Advances array argument's internal pointer to the last element and return it */
PHP_FUNCTION(end)
{
	pval **array, **entry;
	HashTable *target_hash;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &array) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	target_hash = HASH_OF(*array);
	if (!target_hash) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Passed variable is not an array or object");
		RETURN_FALSE;
	}
	zend_hash_internal_pointer_end(target_hash);

	if (return_value_used) {	
		if (zend_hash_get_current_data(target_hash, (void **) &entry) == FAILURE) {
			RETURN_FALSE;
		}

		*return_value = **entry;
		zval_copy_ctor(return_value);
	}
}
/* }}} */

/* {{{ proto mixed prev(array array_arg)
   Move array argument's internal pointer to the previous element and return it */
PHP_FUNCTION(prev)
{
	pval **array, **entry;
	HashTable *target_hash;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &array) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	target_hash = HASH_OF(*array);
	if (!target_hash) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Passed variable is not an array or object");
		RETURN_FALSE;
	}
	zend_hash_move_backwards(target_hash);

	if (return_value_used) {	
		if (zend_hash_get_current_data(target_hash, (void **) &entry) == FAILURE) {
			RETURN_FALSE;
		}
	
		*return_value = **entry;
		zval_copy_ctor(return_value);
	}
}
/* }}} */

/* {{{ proto mixed next(array array_arg)
   Move array argument's internal pointer to the next element and return it */
PHP_FUNCTION(next)
{
	pval **array, **entry;
	HashTable *target_hash;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &array) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	target_hash = HASH_OF(*array);
	if (!target_hash) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Passed variable is not an array or object");
		RETURN_FALSE;
	}
	zend_hash_move_forward(target_hash);

	if (return_value_used) {
		if (zend_hash_get_current_data(target_hash, (void **) &entry) == FAILURE) {
			RETURN_FALSE;
		}
	  
		*return_value = **entry;
		zval_copy_ctor(return_value);
	}
}
/* }}} */

/* {{{ proto mixed reset(array array_arg)
   Set array argument's internal pointer to the first element and return it */	
PHP_FUNCTION(reset)
{
	pval **array, **entry;
	HashTable *target_hash;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &array) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	target_hash = HASH_OF(*array);
	if (!target_hash) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Passed variable is not an array or object");
		RETURN_FALSE;
	}
	zend_hash_internal_pointer_reset(target_hash);

	if (return_value_used) {	
		if (zend_hash_get_current_data(target_hash, (void **) &entry) == FAILURE) {
			RETURN_FALSE;
		}

		*return_value = **entry;
		zval_copy_ctor(return_value);
	}
}
/* }}} */

/* {{{ proto mixed current(array array_arg)
   Return the element currently pointed to by the internal array pointer */
PHP_FUNCTION(current)
{
	pval **array, **entry;
	HashTable *target_hash;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &array) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	target_hash = HASH_OF(*array);
	if (!target_hash) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Passed variable is not an array or object");
		RETURN_FALSE;
	}
	if (zend_hash_get_current_data(target_hash, (void **) &entry) == FAILURE) {
		RETURN_FALSE;
	}
	*return_value = **entry;
	zval_copy_ctor(return_value);
}
/* }}} */

/* {{{ proto mixed key(array array_arg)
   Return the key of the element currently pointed to by the internal array pointer */
PHP_FUNCTION(key)
{
	pval **array;
	char *string_key;
	uint string_length;
	ulong num_key;
	HashTable *target_hash;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &array) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	target_hash = HASH_OF(*array);
	if (!target_hash) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Passed variable is not an array or object");
		RETURN_FALSE;
	}
	switch (zend_hash_get_current_key_ex(target_hash, &string_key, &string_length, &num_key, 0, NULL)) {
		case HASH_KEY_IS_STRING:
			RETVAL_STRINGL(string_key, string_length - 1, 1);
			break;
		case HASH_KEY_IS_LONG:
			RETVAL_LONG(num_key);
			break;
		case HASH_KEY_NON_EXISTANT:
			return;
	}
}
/* }}} */

/* {{{ proto mixed min(mixed arg1 [, mixed arg2 [, mixed ...]])
   Return the lowest value in an array or a series of arguments */
PHP_FUNCTION(min)
{
	int argc=ZEND_NUM_ARGS();
	pval **result;

	if (argc<=0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Atleast one value should be passed");
		RETURN_NULL();
	}
	set_compare_func(SORT_REGULAR TSRMLS_CC);
	if (argc == 1) {
		pval **arr;

		if (zend_get_parameters_ex(1, &arr) == FAILURE || Z_TYPE_PP(arr) != IS_ARRAY) {
			WRONG_PARAM_COUNT;
		}
		if (zend_hash_minmax(Z_ARRVAL_PP(arr), array_data_compare, 0, (void **) &result TSRMLS_CC) == SUCCESS) {
			*return_value = **result;
			zval_copy_ctor(return_value);
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Array must contain atleast one element");
			RETURN_FALSE;
		}
	} else {
		pval ***args = (pval ***) safe_emalloc(sizeof(pval **), ZEND_NUM_ARGS(), 0);
		pval **min, result;
		int i;

		if (zend_get_parameters_array_ex(ZEND_NUM_ARGS(), args)==FAILURE) {
			efree(args);
			WRONG_PARAM_COUNT;
		}

		min = args[0];

		for (i=1; i<ZEND_NUM_ARGS(); i++) {
			is_smaller_function(&result, *args[i], *min TSRMLS_CC);
			if (Z_LVAL(result) == 1) {
				min = args[i];
			}
		}

		*return_value = **min;
		zval_copy_ctor(return_value);

		efree(args);
	}
}
/* }}} */

/* {{{ proto mixed max(mixed arg1 [, mixed arg2 [, mixed ...]])
   Return the highest value in an array or a series of arguments */
PHP_FUNCTION(max)
{
	int argc=ZEND_NUM_ARGS();
	pval **result;

	if (argc<=0) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Atleast one value should be passed");
		RETURN_NULL();
	}
	set_compare_func(SORT_REGULAR TSRMLS_CC);
	if (argc == 1) {
		pval **arr;

		if (zend_get_parameters_ex(1, &arr) == FAILURE || Z_TYPE_PP(arr) != IS_ARRAY) {
			WRONG_PARAM_COUNT;
		}
		if (zend_hash_minmax(Z_ARRVAL_PP(arr), array_data_compare, 1, (void **) &result TSRMLS_CC) == SUCCESS) {
			*return_value = **result;
			zval_copy_ctor(return_value);
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Array must contain atleast one element");
			RETURN_FALSE;
		}
	} else {
		pval ***args = (pval ***) safe_emalloc(sizeof(pval **), ZEND_NUM_ARGS(), 0);
		pval **max, result;
		int i;

		if (zend_get_parameters_array_ex(ZEND_NUM_ARGS(), args) == FAILURE) {
			efree(args);
			WRONG_PARAM_COUNT;
		}

		max = args[0];

		for (i=1; i<ZEND_NUM_ARGS(); i++) {
			is_smaller_or_equal_function(&result, *args[i], *max TSRMLS_CC);
			if (Z_LVAL(result) == 0) {
				max = args[i];
			}
		}

		*return_value = **max;
		zval_copy_ctor(return_value);

		efree(args);
	}
}
/* }}} */

static int php_array_walk(HashTable *target_hash, zval **userdata TSRMLS_DC)
{
	zval **args[3],			/* Arguments to userland function */
		  *retval_ptr,			/* Return value - unused */
		  *key;				/* Entry key */
	char  *string_key;
	uint  string_key_len;
	ulong  num_key;
	HashPosition pos;

	/* Set up known arguments */
	args[1] = &key;
	args[2] = userdata;

	zend_hash_internal_pointer_reset_ex(target_hash, &pos);

	/* Iterate through hash */
	while (zend_hash_get_current_data_ex(target_hash, (void **)&args[0], &pos) == SUCCESS) {
		/* Allocate space for key */
		MAKE_STD_ZVAL(key);
	
		/* Set up the key */
		if (zend_hash_get_current_key_ex(target_hash, &string_key, &string_key_len, &num_key, 0, &pos) == HASH_KEY_IS_LONG) {
			Z_TYPE_P(key) = IS_LONG;
			Z_LVAL_P(key) = num_key;
		} else {
			ZVAL_STRINGL(key, string_key, string_key_len-1, 1);
		}
		
		/* Call the userland function */
		if (call_user_function_ex(EG(function_table), NULL, *BG(array_walk_func_name),
						   &retval_ptr, userdata ? 3 : 2, args, 0, NULL TSRMLS_CC) == SUCCESS) {
		
			zval_ptr_dtor(&retval_ptr);
		} else {
			char *func_name;

			if (zend_is_callable(*BG(array_walk_func_name), 0, &func_name)) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to call %s()", func_name);
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unable to call %s() - function does not exist", func_name);
			}

			efree(func_name);
			break;
		}

		zval_ptr_dtor(&key);
		zend_hash_move_forward_ex(target_hash, &pos);
	}
	
	return 0;
}

/* {{{ proto bool array_walk(array input, string funcname [, mixed userdata])
   Apply a user function to every member of an array */
PHP_FUNCTION(array_walk)
{
	zval *array,
		 *userdata = NULL,
		 *tmp,
		 **old_walk_func_name;
	HashTable *target_hash;

	old_walk_func_name = BG(array_walk_func_name);
	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "zz|z", &array, &tmp, &userdata) == FAILURE) {
		return;
	}
	target_hash = HASH_OF(array);
	if (!target_hash) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The argument should be an array");
		RETURN_FALSE;
	}
	if (Z_TYPE_P(tmp) != IS_ARRAY && Z_TYPE_P(tmp) != IS_STRING) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Wrong syntax for function name");
		RETURN_FALSE;
	} else {
		BG(array_walk_func_name) = &tmp;
	}
	php_array_walk(target_hash, userdata ? &userdata : NULL TSRMLS_CC);
	BG(array_walk_func_name) = old_walk_func_name;
	RETURN_TRUE;
}
/* }}} */

/* void php_search_array(INTERNAL_FUNCTION_PARAMETERS, int behavior)
 *	  0 = return boolean
 *	  1 = return key
 */
static void php_search_array(INTERNAL_FUNCTION_PARAMETERS, int behavior)
{
 	zval **value,				/* value to check for */
		 **array,				/* array to check in */
		 **strict,				/* strict comparison or not */
		 **entry,				/* pointer to array entry */
		  res;					/* comparison result */
	HashTable *target_hash;		/* array hashtable */
	HashPosition pos;			/* hash iterator */
   	ulong num_key;
	uint str_key_len;
   	char *string_key;
	int (*is_equal_func)(zval *, zval *, zval * TSRMLS_DC) = is_equal_function;
	  	
	if (ZEND_NUM_ARGS() < 2 || ZEND_NUM_ARGS() > 3 ||
		zend_get_parameters_ex(ZEND_NUM_ARGS(), &value, &array, &strict) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	if (Z_TYPE_PP(value) == IS_OBJECT) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Wrong datatype for first argument");
		RETURN_FALSE;
	}
	
	if (Z_TYPE_PP(array) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Wrong datatype for second argument");
		RETURN_FALSE;
	}

	if (ZEND_NUM_ARGS() == 3) {
		convert_to_boolean_ex(strict);
		if (Z_LVAL_PP(strict)) {
			is_equal_func = is_identical_function;
		}
	}

	target_hash = HASH_OF(*array);
	zend_hash_internal_pointer_reset_ex(target_hash, &pos);
	while (zend_hash_get_current_data_ex(target_hash, (void **)&entry, &pos) == SUCCESS) {
	 	is_equal_func(&res, *value, *entry TSRMLS_CC);
		if (Z_LVAL(res)) {
			if (behavior == 0) {		 
				RETURN_TRUE;
			} else {
				/* Return current key */
				switch (zend_hash_get_current_key_ex(target_hash, &string_key, &str_key_len, &num_key, 0,  &pos)) {
					case HASH_KEY_IS_STRING:
						RETURN_STRINGL(string_key, str_key_len-1, 1);
						break;
					case HASH_KEY_IS_LONG:
						RETURN_LONG(num_key);
						break;
				}
			}
		}
		
		zend_hash_move_forward_ex(target_hash, &pos);
	}
   
	RETURN_FALSE;
}


/* {{{ proto bool in_array(mixed needle, array haystack [, bool strict])
   Checks if the given value exists in the array */
PHP_FUNCTION(in_array)
{
	php_search_array(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */

/* {{{ proto mixed array_search(mixed needle, array haystack [, bool strict])
   Searches the array for a given value and returns the corresponding key if successful */
PHP_FUNCTION(array_search)
{
	php_search_array(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */


static int php_valid_var_name(char *var_name)
{
	int len, i;
	
	if (!var_name)
		return 0;
	
	len = strlen(var_name);
	
	if (!isalpha((int)((unsigned char *)var_name)[0]) && var_name[0] != '_')
		return 0;
	
	if (len > 1) {
		for (i=1; i<len; i++) {
			if (!isalnum((int)((unsigned char *)var_name)[i]) && var_name[i] != '_') {
				return 0;
			}
		}
	}
	
	return 1;
}


/* {{{ proto int extract(array var_array [, int extract_type [, string prefix]])
   Imports variables into symbol table from an array */
PHP_FUNCTION(extract)
{
	zval **var_array, **z_extract_type, **prefix;
	zval **entry, *data;
	char *var_name;
	smart_str final_name = {0};
	ulong num_key;
	uint var_name_len;
	int var_exists, extract_type, key_type, count = 0;
	int extract_refs = 0;
	HashPosition pos;

	switch (ZEND_NUM_ARGS()) {
		case 1:
			if (zend_get_parameters_ex(1, &var_array) == FAILURE) {
				WRONG_PARAM_COUNT;
			}
			extract_type = EXTR_OVERWRITE;
			break;

		case 2:
			if (zend_get_parameters_ex(2, &var_array, &z_extract_type) == FAILURE) {
				WRONG_PARAM_COUNT;
			}
			convert_to_long_ex(z_extract_type);
			extract_type = Z_LVAL_PP(z_extract_type);
			extract_refs = (extract_type & EXTR_REFS);
			extract_type &= 0xff;
			if (extract_type > EXTR_SKIP && extract_type <= EXTR_PREFIX_IF_EXISTS) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "Prefix expected to be specified");
				return;
			}
			break;
			
		case 3:
			if (zend_get_parameters_ex(3, &var_array, &z_extract_type, &prefix) == FAILURE) {
				WRONG_PARAM_COUNT;
			}
			convert_to_long_ex(z_extract_type);
			extract_type = Z_LVAL_PP(z_extract_type);
			extract_refs = (extract_type & EXTR_REFS);
			extract_type &= 0xff;
			convert_to_string_ex(prefix);
			break;

		default:
			WRONG_PARAM_COUNT;
			break;
	}
	
	if (extract_type < EXTR_OVERWRITE || extract_type > EXTR_IF_EXISTS) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Unknown extract type");
		return;
	}
	
	if (Z_TYPE_PP(var_array) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "First argument should be an array");
		return;
	}
		
	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(var_array), &pos);
	while (zend_hash_get_current_data_ex(Z_ARRVAL_PP(var_array), (void **)&entry, &pos) == SUCCESS) {
		key_type = zend_hash_get_current_key_ex(Z_ARRVAL_PP(var_array), &var_name, &var_name_len, &num_key, 0, &pos);
		var_exists = 0;

		if (key_type == HASH_KEY_IS_STRING) {
			var_name_len--;
			var_exists = zend_hash_exists(EG(active_symbol_table), var_name, var_name_len + 1);
		} else if (extract_type == EXTR_PREFIX_ALL || extract_type == EXTR_PREFIX_INVALID) {
			smart_str_appendl(&final_name, Z_STRVAL_PP(prefix), Z_STRLEN_PP(prefix));
			smart_str_appendc(&final_name, '_');
			smart_str_append_long(&final_name, num_key);
		} else {
			zend_hash_move_forward_ex(Z_ARRVAL_PP(var_array), &pos);
			continue;
		}
			
		switch (extract_type) {
			case EXTR_IF_EXISTS:
				if (!var_exists) break;
				/* break omitted intentionally */

			case EXTR_OVERWRITE:
				/* GLOBALS protection */
				if (var_exists && !strcmp(var_name, "GLOBALS")) {
					break;
				}
				smart_str_appendl(&final_name, var_name, var_name_len);
				break;

			case EXTR_PREFIX_IF_EXISTS:
				if (var_exists) {
					smart_str_appendl(&final_name, Z_STRVAL_PP(prefix), Z_STRLEN_PP(prefix));
					smart_str_appendc(&final_name, '_');
					smart_str_appendl(&final_name, var_name, var_name_len);
				}
				break;

			case EXTR_PREFIX_SAME:
				if (!var_exists)
					smart_str_appendl(&final_name, var_name, var_name_len);
				/* break omitted intentionally */

			case EXTR_PREFIX_ALL:
				if (final_name.len == 0 && var_name_len != 0) {
					smart_str_appendl(&final_name, Z_STRVAL_PP(prefix), Z_STRLEN_PP(prefix));
					smart_str_appendc(&final_name, '_');
					smart_str_appendl(&final_name, var_name, var_name_len);
				}
				break;

			case EXTR_PREFIX_INVALID:
				if (final_name.len == 0) {
					if (!php_valid_var_name(var_name)) {
						smart_str_appendl(&final_name, Z_STRVAL_PP(prefix), Z_STRLEN_PP(prefix));
						smart_str_appendc(&final_name, '_');
						smart_str_appendl(&final_name, var_name, var_name_len);
					} else
						smart_str_appendl(&final_name, var_name, var_name_len);
				}
				break;

			default:
				if (!var_exists)
					smart_str_appendl(&final_name, var_name, var_name_len);
				break;
		}

		if (final_name.len) {
			smart_str_0(&final_name);
			if (php_valid_var_name(final_name.c)) {
				if (extract_refs) {
					zval **orig_var;

					if (zend_hash_find(EG(active_symbol_table), final_name.c, final_name.len+1, (void **) &orig_var) == SUCCESS) {
						SEPARATE_ZVAL_TO_MAKE_IS_REF(entry);
						zval_add_ref(entry);
						
						zval_ptr_dtor(orig_var);

						*orig_var = *entry;
					} else {
						if ((*var_array)->refcount > 1) {
							SEPARATE_ZVAL_TO_MAKE_IS_REF(entry);
						} else {
							(*entry)->is_ref = 1;
						}
						zval_add_ref(entry);
						zend_hash_update(EG(active_symbol_table), final_name.c, final_name.len+1, (void **) entry, sizeof(zval *), NULL);
					}
				} else {
					MAKE_STD_ZVAL(data);
					*data = **entry;
					zval_copy_ctor(data);

					ZEND_SET_SYMBOL(EG(active_symbol_table), final_name.c, data);
				}

				count++;
			}
			final_name.len = 0;
		}

		zend_hash_move_forward_ex(Z_ARRVAL_PP(var_array), &pos);
	}

	smart_str_free(&final_name);

	RETURN_LONG(count);
}
/* }}} */


static void php_compact_var(HashTable *eg_active_symbol_table, zval *return_value, zval *entry)
{
	zval **value_ptr, *value, *data;
	
	if (Z_TYPE_P(entry) == IS_STRING) {
		if (zend_hash_find(eg_active_symbol_table, Z_STRVAL_P(entry),
						   Z_STRLEN_P(entry)+1, (void **)&value_ptr) != FAILURE) {
			value = *value_ptr;
			ALLOC_ZVAL(data);
			*data = *value;
			zval_copy_ctor(data);
			INIT_PZVAL(data);
			
			zend_hash_update(Z_ARRVAL_P(return_value), Z_STRVAL_P(entry),
							 Z_STRLEN_P(entry)+1, &data, sizeof(zval *), NULL);
		}
	}
	else if (Z_TYPE_P(entry) == IS_ARRAY) {
		HashPosition pos;

		zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(entry), &pos);
		while (zend_hash_get_current_data_ex(Z_ARRVAL_P(entry), (void**)&value_ptr, &pos) == SUCCESS) {
			value = *value_ptr;

			php_compact_var(eg_active_symbol_table, return_value, value);
			zend_hash_move_forward_ex(Z_ARRVAL_P(entry), &pos);
		}
	}
}


/* {{{ proto array compact(mixed var_names [, mixed ...])
   Creates a hash containing variables and their values */
PHP_FUNCTION(compact)
{
	zval ***args;			/* function arguments array */
	int i;
	
	args = (zval ***)safe_emalloc(sizeof(zval **), ZEND_NUM_ARGS(), 0);
	
	if (zend_get_parameters_array_ex(ZEND_NUM_ARGS(), args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	array_init(return_value);
	
	for (i=0; i<ZEND_NUM_ARGS(); i++) {
		php_compact_var(EG(active_symbol_table), return_value, *args[i]);
	}
	
	efree(args);
}
/* }}} */

/* {{{ proto array array_fill(int start_key, int num, mixed val)
   Create an array containing num elements starting with index start_key each initialized to val */
PHP_FUNCTION(array_fill)
{
	zval **start_key, **num, **val, *newval;
	long i;

	if (ZEND_NUM_ARGS() != 3 || zend_get_parameters_ex(3, &start_key, &num, &val) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	switch (Z_TYPE_PP(start_key)) {
		case IS_STRING:
		case IS_LONG:
		case IS_DOUBLE:
			/* allocate an array for return */
			array_init(return_value);
	
			if (PZVAL_IS_REF(*val)) {
				SEPARATE_ZVAL(val);
			}
			convert_to_long_ex(start_key);
			zval_add_ref(val);
			zend_hash_index_update(Z_ARRVAL_P(return_value), Z_LVAL_PP(start_key), val, sizeof(val), NULL);
			break;
		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Wrong data type for start key");
			RETURN_FALSE;
			break;
	}	

	convert_to_long_ex(num);
	i = Z_LVAL_PP(num) - 1;	
	if (i < 0) {
		zend_hash_destroy(Z_ARRVAL_P(return_value));
		efree(Z_ARRVAL_P(return_value));
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Number of elements must be positive");
		RETURN_FALSE;
	}
	newval = *val;
	while (i--) {
#ifndef ZEND_ENGINE_2
		if (newval->refcount >= 62000) {
			MAKE_STD_ZVAL(newval);
			*newval = **val;
			zval_copy_ctor(newval);
			newval->refcount = 0;
		}
#endif		
		zval_add_ref(&newval);
		zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &newval, sizeof(zval *), NULL);
	}
}
/* }}} */

/* {{{ proto array range(mixed low, mixed high)
   Create an array containing the range of integers or characters from low to high (inclusive) */
PHP_FUNCTION(range)
{
	zval **zlow, **zhigh;
	
	if (ZEND_NUM_ARGS() != 2 || zend_get_parameters_ex(2, &zlow, &zhigh) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	/* allocate an array for return */
	array_init(return_value);

	if (Z_TYPE_PP(zlow) == IS_STRING && Z_TYPE_PP(zhigh) == IS_STRING && Z_STRLEN_PP(zlow) == 1 && Z_STRLEN_PP(zhigh) == 1) {
		unsigned char low, high;
		low = *((unsigned char *)Z_STRVAL_PP(zlow));
		high = *((unsigned char *)Z_STRVAL_PP(zhigh));
		
		if (low>high) {
			for (; low >= high; (low)--) {
				add_next_index_stringl(return_value, (char *)&low, 1, 1);
				if ((int)low == 0) {
					break;
				}
			}	
		} else {
			for (; low <= high; (low)++) {
				add_next_index_stringl(return_value, (char *)&low, 1, 1);
				if ((int)low == 255) {
					break;
				}
			}	
		}
	} else {
		int low, high;
		convert_to_long_ex(zlow);
		convert_to_long_ex(zhigh);
		low = Z_LVAL_PP(zlow);
		high = Z_LVAL_PP(zhigh);
		if (low > high) { 
			for (; low >= high; low--) {
				add_next_index_long(return_value, low);
			}	
		} else {
			for (; low <= high; low++) {
				add_next_index_long(return_value, low);
			}	
		}
	}
}
/* }}} */


static void array_data_shuffle(zval *array TSRMLS_DC)
{
	Bucket **elems, *temp;
	HashTable *hash;
	int j, n_elems, rnd_idx, n_left;

	n_elems = zend_hash_num_elements(Z_ARRVAL_P(array));
	
	if (n_elems < 1) {
		return;
	}

	elems = (Bucket **)safe_emalloc(sizeof(Bucket *), n_elems, 0);
	hash = Z_ARRVAL_P(array);
	n_left = n_elems;

	for (j = 0, temp = hash->pListHead; temp; temp = temp->pListNext)
		elems[j++] = temp;
	while (--n_left) {
		rnd_idx = php_rand(TSRMLS_C);
		RAND_RANGE(rnd_idx, 0, n_left, PHP_RAND_MAX);
		if (rnd_idx != n_left) {
			temp = elems[n_left];
			elems[n_left] = elems[rnd_idx];
			elems[rnd_idx] = temp;
		}
	}

	HANDLE_BLOCK_INTERRUPTIONS();
	hash->pListHead = elems[0];
	hash->pListTail = NULL;
	hash->pInternalPointer = hash->pListHead;

	for (j = 0; j < n_elems; j++) {
		if (hash->pListTail) {
			hash->pListTail->pListNext = elems[j];
		}
		elems[j]->pListLast = hash->pListTail;
		elems[j]->pListNext = NULL;
		hash->pListTail = elems[j];
	}
	temp = hash->pListHead;
	j = 0;
	while (temp != NULL) {
		temp->nKeyLength = 0;
		temp->h = j++;
		temp = temp->pListNext;
	}
	hash->nNextFreeElement = n_elems;
	zend_hash_rehash(hash);
	HANDLE_UNBLOCK_INTERRUPTIONS();

	efree(elems);
}

/* {{{ proto bool shuffle(array array_arg)
   Randomly shuffle the contents of an array */
PHP_FUNCTION(shuffle)
{
	zval *array;

	if (zend_parse_parameters(1 TSRMLS_CC, "a", &array) == FAILURE) {
		RETURN_FALSE;
	}

	array_data_shuffle(array TSRMLS_CC);

	RETURN_TRUE;
}
/* }}} */


/* HashTable* php_splice(HashTable *in_hash, int offset, int length,
						 zval ***list, int list_count, HashTable **removed) */
HashTable* php_splice(HashTable *in_hash, int offset, int length,
					  zval ***list, int list_count, HashTable **removed)
{
	HashTable 	*out_hash = NULL;	/* Output hashtable */
	int			 num_in,			/* Number of entries in the input hashtable */
				 pos,				/* Current position in the hashtable */
				 i;					/* Loop counter */
	Bucket		*p;					/* Pointer to hash bucket */
	zval		*entry;				/* Hash entry */
	
	/* If input hash doesn't exist, we have nothing to do */
	if (!in_hash)
		return NULL;
	
	/* Get number of entries in the input hash */
	num_in = zend_hash_num_elements(in_hash);
	
	/* Clamp the offset.. */
	if (offset > num_in)
		offset = num_in;
	else if (offset < 0 && (offset=num_in+offset) < 0)
		offset = 0;
	
	/* ..and the length */
	if (length < 0) {
		length = num_in-offset+length;
	} else if (((unsigned) offset + (unsigned) length) > num_in) {
		length = num_in-offset;
	}

	/* Create and initialize output hash */
	ALLOC_HASHTABLE(out_hash);
	zend_hash_init(out_hash, 0, NULL, ZVAL_PTR_DTOR, 0);
	
	/* Start at the beginning of the input hash and copy
	   entries to output hash until offset is reached */
	for (pos=0, p=in_hash->pListHead; pos<offset && p ; pos++, p=p->pListNext) {
		/* Get entry and increase reference count */
		entry = *((zval **)p->pData);
		entry->refcount++;
		
		/* Update output hash depending on key type */
		if (p->nKeyLength)
			zend_hash_update(out_hash, p->arKey, p->nKeyLength, &entry, sizeof(zval *), NULL);
		else
			zend_hash_next_index_insert(out_hash, &entry, sizeof(zval *), NULL);
	}
	
	/* If hash for removed entries exists, go until offset+length
	   and copy the entries to it */
	if (removed != NULL) {
		for ( ; pos<offset+length && p; pos++, p=p->pListNext) {
			entry = *((zval **)p->pData);
			entry->refcount++;
			if (p->nKeyLength)
				zend_hash_update(*removed, p->arKey, p->nKeyLength, &entry, sizeof(zval *), NULL);
			else
				zend_hash_next_index_insert(*removed, &entry, sizeof(zval *), NULL);
		}
	} else /* otherwise just skip those entries */
		for ( ; pos<offset+length && p; pos++, p=p->pListNext);
	
	/* If there are entries to insert.. */
	if (list != NULL) {
		/* ..for each one, create a new zval, copy entry into it
		   and copy it into the output hash */
		for (i=0; i<list_count; i++) {
			entry = *list[i];
			if (entry->refcount>=1000) {
				zval *tmp = (zval *) emalloc(sizeof(zval));

				*tmp = *entry;
				zval_copy_ctor(tmp);
				tmp->refcount = 1;
				tmp->is_ref = 0;
				entry = tmp;
			} else {
				entry->refcount++;
			}
			zend_hash_next_index_insert(out_hash, &entry, sizeof(zval *), NULL);
		}
	}
	
	/* Copy the remaining input hash entries to the output hash */
	for ( ; p ; p=p->pListNext) {
		entry = *((zval **)p->pData);
		entry->refcount++;
		if (p->nKeyLength)
			zend_hash_update(out_hash, p->arKey, p->nKeyLength, &entry, sizeof(zval *), NULL);
		else
			zend_hash_next_index_insert(out_hash, &entry, sizeof(zval *), NULL);
	}

	zend_hash_internal_pointer_reset(out_hash);
	return out_hash;
}
/* }}} */


/* {{{ proto int array_push(array stack, mixed var [, mixed ...])
   Pushes elements onto the end of the array */
PHP_FUNCTION(array_push)
{
	zval	  ***args,		/* Function arguments array */
				*stack,		/* Input array */
				*new_var;	/* Variable to be pushed */
	int			 i,			/* Loop counter */
				 argc;		/* Number of function arguments */

	/* Get the argument count and check it */
	argc = ZEND_NUM_ARGS();
	if (argc < 2) {
		WRONG_PARAM_COUNT;
	}
	
	/* Allocate arguments array and get the arguments, checking for errors. */
	args = (zval ***)safe_emalloc(sizeof(zval **), argc, 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	/* Get first argument and check that it's an array */	
	stack = *args[0];
	if (Z_TYPE_P(stack) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "First argument should be an array");
		efree(args);
		RETURN_FALSE;
	}

	/* For each subsequent argument, make it a reference, increase refcount,
	   and add it to the end of the array */
	for (i=1; i<argc; i++) {
		new_var = *args[i];
		new_var->refcount++;
	
		zend_hash_next_index_insert(Z_ARRVAL_P(stack), &new_var, sizeof(zval *), NULL);
	}
	
	/* Clean up and return the number of values in the stack */
	efree(args);
	RETVAL_LONG(zend_hash_num_elements(Z_ARRVAL_P(stack)));
}
/* }}} */


/* {{{ void _phpi_pop(INTERNAL_FUNCTION_PARAMETERS, int which_end) */
static void _phpi_pop(INTERNAL_FUNCTION_PARAMETERS, int off_the_end)
{
	zval	   **stack,			/* Input stack */
			   **val;			/* Value to be popped */
	char *key = NULL;
	uint key_len = 0;
	ulong index;
	
	/* Get the arguments and do error-checking */
	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &stack) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	if (Z_TYPE_PP(stack) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The argument should be an array");
		return;
	}

	if (zend_hash_num_elements(Z_ARRVAL_PP(stack)) == 0) {
		return;
	}
		
	/* Get the first or last value and copy it into the return value */
	if (off_the_end)
		zend_hash_internal_pointer_end(Z_ARRVAL_PP(stack));
	else
		zend_hash_internal_pointer_reset(Z_ARRVAL_PP(stack));
	zend_hash_get_current_data(Z_ARRVAL_PP(stack), (void **)&val);
	*return_value = **val;
	zval_copy_ctor(return_value);
	INIT_PZVAL(return_value);
	
	/* Delete the first or last value */
	zend_hash_get_current_key_ex(Z_ARRVAL_PP(stack), &key, &key_len, &index, 0, NULL);
	zend_hash_del_key_or_index(Z_ARRVAL_PP(stack), key, key_len, index, (key) ? HASH_DEL_KEY : HASH_DEL_INDEX);
	
	/* If we did a shift... re-index like it did before */
	if (!off_the_end) {
		int k = 0;
		Bucket *p = Z_ARRVAL_PP(stack)->pListHead;
		while (p != NULL) {
			if (p->nKeyLength == 0) {
				p->h = k++;
			}
			p = p->pListNext;
		}
		Z_ARRVAL_PP(stack)->nNextFreeElement = k;
		zend_hash_rehash(Z_ARRVAL_PP(stack));
	} else if (!key_len && index >= Z_ARRVAL_PP(stack)->nNextFreeElement-1) {
		Z_ARRVAL_PP(stack)->nNextFreeElement = Z_ARRVAL_PP(stack)->nNextFreeElement - 1;
	}

	zend_hash_internal_pointer_reset(Z_ARRVAL_PP(stack));
}
/* }}} */

/* {{{ proto mixed array_pop(array stack)
   Pops an element off the end of the array */
PHP_FUNCTION(array_pop)
{
	_phpi_pop(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */


/* {{{ proto mixed array_shift(array stack)
   Pops an element off the beginning of the array */
PHP_FUNCTION(array_shift)
{
	_phpi_pop(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */


/* {{{ proto int array_unshift(array stack, mixed var [, mixed ...])
   Pushes elements onto the beginning of the array */
PHP_FUNCTION(array_unshift)
{
	zval	  ***args,		/* Function arguments array */
				*stack;		/* Input stack */
	HashTable	*new_hash;	/* New hashtable for the stack */
	int			 argc;		/* Number of function arguments */
	

	/* Get the argument count and check it */	
	argc = ZEND_NUM_ARGS();
	if (argc < 2) {
		WRONG_PARAM_COUNT;
	}
	
	/* Allocate arguments array and get the arguments, checking for errors. */
	args = (zval ***)safe_emalloc(sizeof(zval **), argc, 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}
	
	/* Get first argument and check that it's an array */
	stack = *args[0];
	if (Z_TYPE_P(stack) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The first argument should be an array");
		efree(args);
		RETURN_FALSE;
	}

	/* Use splice to insert the elements at the beginning.  Destroy old
	   hashtable and replace it with new one */
	new_hash = php_splice(Z_ARRVAL_P(stack), 0, 0, &args[1], argc-1, NULL);
	zend_hash_destroy(Z_ARRVAL_P(stack));
	*Z_ARRVAL_P(stack) = *new_hash;
	FREE_HASHTABLE(new_hash);

	/* Clean up and return the number of elements in the stack */
	efree(args);
	RETVAL_LONG(zend_hash_num_elements(Z_ARRVAL_P(stack)));
}
/* }}} */


/* {{{ proto array array_splice(array input, int offset [, int length [, array replacement]])
   Removes the elements designated by offset and length and replace them with supplied array */
PHP_FUNCTION(array_splice)
{
	zval	  ***args,				/* Function arguments array */
				*array,				/* Input array */
			  ***repl = NULL;		/* Replacement elements */
	HashTable	*new_hash = NULL;	/* Output array's hash */
	Bucket		*p;					/* Bucket used for traversing hash */
	int			 argc,				/* Number of function arguments */
				 i,
				 offset,
				 length,
				 repl_num = 0;		/* Number of replacement elements */

	/* Get the argument count and check it */
	argc = ZEND_NUM_ARGS();
	if (argc < 2 || argc > 4) {
		WRONG_PARAM_COUNT;
	}
	
	/* Allocate arguments array and get the arguments, checking for errors. */
	args = (zval ***)safe_emalloc(sizeof(zval **), argc, 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}	

	/* Get first argument and check that it's an array */
	array = *args[0];
	if (Z_TYPE_P(array) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The first argument should be an array");
		efree(args);
		return;
	}
	
	/* Get the next two arguments.  If length is omitted,
	   it's assumed to be until the end of the array */
	convert_to_long_ex(args[1]);
	offset = Z_LVAL_PP(args[1]);
	if (argc > 2) {
		convert_to_long_ex(args[2]);
		length = Z_LVAL_PP(args[2]);
	} else
		length = zend_hash_num_elements(Z_ARRVAL_P(array));

	if (argc == 4) {
		/* Make sure the last argument, if passed, is an array */
		convert_to_array_ex(args[3]);
		
		/* Create the array of replacement elements */
		repl_num = zend_hash_num_elements(Z_ARRVAL_PP(args[3]));
		repl = (zval ***)safe_emalloc(sizeof(zval **), repl_num, 0);
		for (p=Z_ARRVAL_PP(args[3])->pListHead, i=0; p; p=p->pListNext, i++) {
			repl[i] = ((zval **)p->pData);
		}
	}
	
	/* Initialize return value */
	array_init(return_value);
	
	/* Perform splice */
	new_hash = php_splice(Z_ARRVAL_P(array), offset, length,
							repl, repl_num,
							&Z_ARRVAL_P(return_value));
	
	/* Replace input array's hashtable with the new one */
	zend_hash_destroy(Z_ARRVAL_P(array));
	*Z_ARRVAL_P(array) = *new_hash;
	FREE_HASHTABLE(new_hash);
	
	/* Clean up */
	if (argc == 4)
		efree(repl);
	efree(args);
}
/* }}} */


/* {{{ proto array array_slice(array input, int offset [, int length])
   Returns elements specified by offset and length */
PHP_FUNCTION(array_slice)
{
	zval	   **input,			/* Input array */
			   **offset,		/* Offset to get elements from */
			   **length,		/* How many elements to get */
			   **entry;			/* An array entry */
	int			 offset_val,	/* Value of the offset argument */
				 length_val,	/* Value of the length argument */
				 num_in,		/* Number of elements in the input array */
				 pos,			/* Current position in the array */
				 argc;			/* Number of function arguments */
				 
	char		*string_key;
	uint		 string_key_len;
	ulong		 num_key;
	HashPosition hpos;
	

	/* Get the arguments and do error-checking */	
	argc = ZEND_NUM_ARGS();
	if (argc < 2 || argc > 3 || zend_get_parameters_ex(argc, &input, &offset, &length)) {
		WRONG_PARAM_COUNT;
	}
	
	if (Z_TYPE_PP(input) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The first argument should be an array");
		return;
	}
	
	/* Make sure offset and length are integers and assume
	   we want all entries from offset to the end if length
	   is not passed */
	convert_to_long_ex(offset);
	offset_val = Z_LVAL_PP(offset);
	if (argc == 3) {
		convert_to_long_ex(length);
		length_val = Z_LVAL_PP(length);
	} else {
		length_val = zend_hash_num_elements(Z_ARRVAL_PP(input));
	}
	
	/* Initialize returned array */
	array_init(return_value);
	
	/* Get number of entries in the input hash */
	num_in = zend_hash_num_elements(Z_ARRVAL_PP(input));
	
	/* Clamp the offset.. */
	if (offset_val > num_in)
		return;
	else if (offset_val < 0 && (offset_val=num_in+offset_val) < 0)
		offset_val = 0;
	
	/* ..and the length */
	if (length_val < 0) {
		length_val = num_in-offset_val+length_val;
	} else if (((unsigned) offset_val + (unsigned)length_val) > num_in) {
		length_val = num_in-offset_val;
	}
	
	if (length_val == 0)
		return;
	
	/* Start at the beginning and go until we hit offset */
	pos = 0;
	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(input), &hpos);
	while (pos < offset_val &&
		  zend_hash_get_current_data_ex(Z_ARRVAL_PP(input), (void **)&entry, &hpos) == SUCCESS) {
		pos++;
		zend_hash_move_forward_ex(Z_ARRVAL_PP(input), &hpos);
	}
	
	/* Copy elements from input array to the one that's returned */
	while (pos < offset_val+length_val &&
		  zend_hash_get_current_data_ex(Z_ARRVAL_PP(input), (void **)&entry, &hpos) == SUCCESS) {
		
		(*entry)->refcount++;

		switch (zend_hash_get_current_key_ex(Z_ARRVAL_PP(input), &string_key, &string_key_len, &num_key, 0, &hpos)) {
			case HASH_KEY_IS_STRING:
				zend_hash_update(Z_ARRVAL_P(return_value), string_key, string_key_len,
								 entry, sizeof(zval *), NULL);
				break;
	
			case HASH_KEY_IS_LONG:
				zend_hash_next_index_insert(Z_ARRVAL_P(return_value),
											entry, sizeof(zval *), NULL);
				break;
		}
		pos++;
		zend_hash_move_forward_ex(Z_ARRVAL_PP(input), &hpos);
	}
}
/* }}} */


PHPAPI int php_array_merge(HashTable *dest, HashTable *src, int recursive TSRMLS_DC)
{
	zval	  **src_entry,
			  **dest_entry;
	char	   *string_key;
	uint		string_key_len;
	ulong		num_key;
	HashPosition pos;

	zend_hash_internal_pointer_reset_ex(src, &pos);
	while (zend_hash_get_current_data_ex(src, (void **)&src_entry, &pos) == SUCCESS) {
		switch (zend_hash_get_current_key_ex(src, &string_key, &string_key_len, &num_key, 0, &pos)) {
			case HASH_KEY_IS_STRING:
				if (recursive &&
					zend_hash_find(dest, string_key, string_key_len,
								   (void **)&dest_entry) == SUCCESS) {
					if (*src_entry == *dest_entry && ((*dest_entry)->refcount % 2)) {
						zend_error(E_WARNING, "%s(): recursion detected",
								   get_active_function_name(TSRMLS_C));
						return 0;
					}
					SEPARATE_ZVAL(dest_entry);
					SEPARATE_ZVAL(src_entry);
					
					convert_to_array_ex(dest_entry);
					convert_to_array_ex(src_entry);
					if (!php_array_merge(Z_ARRVAL_PP(dest_entry),
									Z_ARRVAL_PP(src_entry), recursive TSRMLS_CC))
						return 0;
				} else {
					(*src_entry)->refcount++;

					zend_hash_update(dest, string_key, strlen(string_key)+1,
									 src_entry, sizeof(zval *), NULL);
				}
				break;

			case HASH_KEY_IS_LONG:
				(*src_entry)->refcount++;
				zend_hash_next_index_insert(dest, src_entry, sizeof(zval *), NULL);
				break;
		}

		zend_hash_move_forward_ex(src, &pos);
	}

	return 1;
}

static void php_array_merge_wrapper(INTERNAL_FUNCTION_PARAMETERS, int recursive)
{
	zval	  ***args = NULL;
	int			 argc,
				 i;

	/* Get the argument count and check it */	
	argc = ZEND_NUM_ARGS();
	if (argc < 1) {
		WRONG_PARAM_COUNT;
	}
	
	/* Allocate arguments array and get the arguments, checking for errors. */
	args = (zval ***)safe_emalloc(sizeof(zval **), argc, 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}
	
	array_init(return_value);
	
	for (i=0; i<argc; i++) {
		SEPARATE_ZVAL(args[i]);
		convert_to_array_ex(args[i]);
		php_array_merge(Z_ARRVAL_P(return_value), Z_ARRVAL_PP(args[i]), recursive TSRMLS_CC);
	}
	
	efree(args);
}


/* {{{ proto array array_merge(array arr1, array arr2 [, array ...])
   Merges elements from passed arrays into one array */
PHP_FUNCTION(array_merge)
{
	php_array_merge_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, 0);
}
/* }}} */


/* {{{ proto array array_merge_recursive(array arr1, array arr2 [, array ...])
   Recursively merges elements from passed arrays into one array */
PHP_FUNCTION(array_merge_recursive)
{
	php_array_merge_wrapper(INTERNAL_FUNCTION_PARAM_PASSTHRU, 1);
}
/* }}} */


/* {{{ proto array array_keys(array input [, mixed search_value])
   Return just the keys from the input array, optionally only for the specified search_value */
PHP_FUNCTION(array_keys)
{
	zval	   **input,			/* Input array */
			   **search_value,	/* Value to search for */
			   **entry,			/* An entry in the input array */
				 res,			/* Result of comparison */
				*new_val;		/* New value */
	int			 add_key;		/* Flag to indicate whether a key should be added */
	char		*string_key;	/* String key */
	uint		 string_key_len;
	ulong		 num_key;		/* Numeric key */
	HashPosition pos;

	search_value = NULL;
	
	/* Get arguments and do error-checking */
	if (ZEND_NUM_ARGS() < 1 || ZEND_NUM_ARGS() > 2 ||
		zend_get_parameters_ex(ZEND_NUM_ARGS(), &input, &search_value) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	if (Z_TYPE_PP(input) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The first argument should be an array");
		return;
	}
	
	/* Initialize return array */
	array_init(return_value);
	add_key = 1;
	
	/* Go through input array and add keys to the return array */
	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(input), &pos);
	while (zend_hash_get_current_data_ex(Z_ARRVAL_PP(input), (void **)&entry, &pos) == SUCCESS) {
		if (search_value != NULL) {
	 		is_equal_function(&res, *search_value, *entry TSRMLS_CC);
			add_key = zval_is_true(&res);
		}
	
		if (add_key) {	
			MAKE_STD_ZVAL(new_val);

			switch (zend_hash_get_current_key_ex(Z_ARRVAL_PP(input), &string_key, &string_key_len, &num_key, 1, &pos)) {
				case HASH_KEY_IS_STRING:
					Z_TYPE_P(new_val) = IS_STRING;
					Z_STRVAL_P(new_val) = string_key;
					Z_STRLEN_P(new_val) = string_key_len-1;
					zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &new_val,
												sizeof(zval *), NULL);
					break;

				case HASH_KEY_IS_LONG:
					Z_TYPE_P(new_val) = IS_LONG;
					Z_LVAL_P(new_val) = num_key;
					zend_hash_next_index_insert(Z_ARRVAL_P(return_value), &new_val,
												sizeof(zval *), NULL);
					break;
			}
		}

		zend_hash_move_forward_ex(Z_ARRVAL_PP(input), &pos);
	}
}
/* }}} */


/* {{{ proto array array_values(array input)
   Return just the values from the input array */
PHP_FUNCTION(array_values)
{
	zval	   **input,		/* Input array */
			   **entry;		/* An entry in the input array */
	HashPosition pos;
	
	/* Get arguments and do error-checking */
	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(ZEND_NUM_ARGS(), &input) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	if (Z_TYPE_PP(input) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The argument should be an array");
		return;
	}
	
	/* Initialize return array */
	array_init(return_value);

	/* Go through input array and add values to the return array */	
	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(input), &pos);
	while (zend_hash_get_current_data_ex(Z_ARRVAL_PP(input), (void **)&entry, &pos) == SUCCESS) {
		
		(*entry)->refcount++;
		zend_hash_next_index_insert(Z_ARRVAL_P(return_value), entry,
											sizeof(zval *), NULL);

		zend_hash_move_forward_ex(Z_ARRVAL_PP(input), &pos);
	}
}
/* }}} */


/* {{{ proto array array_count_values(array input)
   Return the value as key and the frequency of that value in input as value */
PHP_FUNCTION(array_count_values)
{
	zval	   **input,		/* Input array */
			   **entry;		/* An entry in the input array */
	zval	   **tmp;
	HashTable   *myht;
	HashPosition pos;
	
	/* Get arguments and do error-checking */
	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &input) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	if (Z_TYPE_PP(input) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The argument should be an array");
		return;
	}
	
	/* Initialize return array */
	array_init(return_value);

	/* Go through input array and add values to the return array */	
	myht = Z_ARRVAL_PP(input);
	zend_hash_internal_pointer_reset_ex(myht, &pos);
	while (zend_hash_get_current_data_ex(myht, (void **)&entry, &pos) == SUCCESS) {
		if (Z_TYPE_PP(entry) == IS_LONG) {
			if (zend_hash_index_find(Z_ARRVAL_P(return_value), 
									 Z_LVAL_PP(entry), 
									 (void**)&tmp) == FAILURE) {
				zval *data;
				MAKE_STD_ZVAL(data);
				Z_TYPE_P(data) = IS_LONG;
				Z_LVAL_P(data) = 1;
				zend_hash_index_update(Z_ARRVAL_P(return_value), Z_LVAL_PP(entry), &data, sizeof(data), NULL);
			} else {
				Z_LVAL_PP(tmp)++;
			}
		} else if (Z_TYPE_PP(entry) == IS_STRING) {
			if (zend_hash_find(Z_ARRVAL_P(return_value), 
							   Z_STRVAL_PP(entry), 
							   Z_STRLEN_PP(entry)+1, 
							   (void**)&tmp) == FAILURE) {
				zval *data;
				MAKE_STD_ZVAL(data);
				Z_TYPE_P(data) = IS_LONG;
				Z_LVAL_P(data) = 1;
				zend_hash_update(Z_ARRVAL_P(return_value), Z_STRVAL_PP(entry), Z_STRLEN_PP(entry) + 1, &data, sizeof(data), NULL);
			} else {
				Z_LVAL_PP(tmp)++;
			}
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Can only count STRING and INTEGER values!");
		}

		zend_hash_move_forward_ex(myht, &pos);
	}
}
/* }}} */


/* {{{ proto array array_reverse(array input [, bool preserve keys])
   Return input as a new array with the order of the entries reversed */
PHP_FUNCTION(array_reverse)
{
	zval	   **input,			  /* Input array */
			   **z_preserve_keys, /* Flag: whether to preserve keys */
			   **entry;			  /* An entry in the input array */
	char		*string_key;
	uint		 string_key_len;
	ulong		 num_key;
	zend_bool	 preserve_keys = 0;
	HashPosition pos;
	
	/* Get arguments and do error-checking */
	if (ZEND_NUM_ARGS() < 1 || ZEND_NUM_ARGS() > 2 ||
		zend_get_parameters_ex(ZEND_NUM_ARGS(), &input, &z_preserve_keys) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	if (Z_TYPE_PP(input) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The argument should be an array");
		return;
	}

	if (ZEND_NUM_ARGS() > 1) {
		convert_to_boolean_ex(z_preserve_keys);
		preserve_keys = Z_BVAL_PP(z_preserve_keys);
	}
	
	/* Initialize return array */
	array_init(return_value);
	
	zend_hash_internal_pointer_end_ex(Z_ARRVAL_PP(input), &pos);
	while (zend_hash_get_current_data_ex(Z_ARRVAL_PP(input), (void **)&entry, &pos) == SUCCESS) {
		(*entry)->refcount++;
		
		switch (zend_hash_get_current_key_ex(Z_ARRVAL_PP(input), &string_key, &string_key_len, &num_key, 0, &pos)) {
			case HASH_KEY_IS_STRING:
				zend_hash_update(Z_ARRVAL_P(return_value), string_key, string_key_len,
								 entry, sizeof(zval *), NULL);
				break;

			case HASH_KEY_IS_LONG:
				if (preserve_keys)
					zend_hash_index_update(Z_ARRVAL_P(return_value), num_key,
										   entry, sizeof(zval *), NULL);
				else
					zend_hash_next_index_insert(Z_ARRVAL_P(return_value),
												entry, sizeof(zval *), NULL);
				break;
		}
		
		zend_hash_move_backwards_ex(Z_ARRVAL_PP(input), &pos);
	}
}
/* }}} */


/* {{{ proto array array_pad(array input, int pad_size, mixed pad_value)
   Returns a copy of input array padded with pad_value to size pad_size */
PHP_FUNCTION(array_pad)
{
	zval		**input;		/* Input array */
	zval		**pad_size;		/* Size to pad to */
	zval		**pad_value;	/* Padding value obviously */
	zval	   ***pads;			/* Array to pass to splice */
	HashTable	 *new_hash;		/* Return value from splice */
	int			  input_size;	/* Size of the input array */
	int			  pad_size_abs; /* Absolute value of pad_size */
	int			  num_pads;		/* How many pads do we need */
	int			  do_pad;		/* Whether we should do padding at all */
	int			  i;
	
	/* Get arguments and do error-checking */
	if (ZEND_NUM_ARGS() != 3 || zend_get_parameters_ex(3, &input, &pad_size, &pad_value) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	
	/* Make sure arguments are of the proper type */
	if (Z_TYPE_PP(input) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The argument should be an array");
		return;
	}
	convert_to_long_ex(pad_size);
	
	/* Do some initial calculations */
	input_size = zend_hash_num_elements(Z_ARRVAL_PP(input));
	pad_size_abs = abs(Z_LVAL_PP(pad_size));
	do_pad = (input_size >= pad_size_abs) ? 0 : 1;
	
	/* Copy the original array */
	*return_value = **input;
	zval_copy_ctor(return_value);
	
	/* If no need to pad, no need to continue */
	if (!do_pad)
		return;
	
	/* Populate the pads array */
	num_pads = pad_size_abs - input_size;
	if(num_pads > 1048576) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "You may only pad up to 1048576 elements at a time");
		RETURN_FALSE;
	}
	pads = (zval ***)safe_emalloc(sizeof(zval **), num_pads, 0);
	for (i = 0; i < num_pads; i++)
		pads[i] = pad_value;
	
	/* Pad on the right or on the left */
	if (Z_LVAL_PP(pad_size) > 0)
		new_hash = php_splice(Z_ARRVAL_P(return_value), input_size, 0, pads, num_pads, NULL);
	else
		new_hash = php_splice(Z_ARRVAL_P(return_value), 0, 0, pads, num_pads, NULL);

	
	/* Copy the result hash into return value */
	zend_hash_destroy(Z_ARRVAL_P(return_value));
	*Z_ARRVAL_P(return_value) = *new_hash;
	FREE_HASHTABLE(new_hash);
	
	/* Clean up */
	efree(pads);
}
/* }}} */

/* {{{ proto array array_flip(array input)
   Return array with key <-> value flipped */
PHP_FUNCTION(array_flip)
{
	zval **array, **entry, *data;
	HashTable *target_hash;
	char *string_key;
	uint str_key_len;
	ulong num_key;
	HashPosition pos;
					
	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &array) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	target_hash = HASH_OF(*array);
	if (!target_hash) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The argument should be an array");
		RETURN_FALSE;
	}
	
	array_init(return_value);

	zend_hash_internal_pointer_reset_ex(target_hash, &pos);
	while (zend_hash_get_current_data_ex(target_hash, (void **)&entry, &pos) == SUCCESS) {
		MAKE_STD_ZVAL(data);
		switch (zend_hash_get_current_key_ex(target_hash, &string_key, &str_key_len, &num_key, 1, &pos)) {
			case HASH_KEY_IS_STRING:
				Z_STRVAL_P(data) = string_key;
				Z_STRLEN_P(data) = str_key_len-1;
				Z_TYPE_P(data) = IS_STRING;
				break;
			case HASH_KEY_IS_LONG:
				Z_TYPE_P(data) = IS_LONG;
				Z_LVAL_P(data) = num_key;
				break;
		}

		if (Z_TYPE_PP(entry) == IS_LONG) {
			zend_hash_index_update(Z_ARRVAL_P(return_value), Z_LVAL_PP(entry), &data, sizeof(data), NULL);
		} else if (Z_TYPE_PP(entry) == IS_STRING) {
			zend_hash_update(Z_ARRVAL_P(return_value), Z_STRVAL_PP(entry), Z_STRLEN_PP(entry) + 1, &data, sizeof(data), NULL);
		} else {
			zval_ptr_dtor(&data); /* will free also zval structure */
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Can only flip STRING and INTEGER values!");
		}
	
		zend_hash_move_forward_ex(target_hash, &pos);
	}
}
/* }}} */

/* {{{ proto array array_change_key_case(array input [, int case=CASE_LOWER])
   Retuns an array with all string keys lowercased [or uppercased] */
PHP_FUNCTION(array_change_key_case)
{
	zval **array, **entry, **to_upper;
	char *string_key;
	char *new_key;
	uint str_key_len;
	ulong num_key;
	ulong change_to_upper=0;

	HashPosition pos;
					
	if (ZEND_NUM_ARGS() < 1 || ZEND_NUM_ARGS() > 2 || 
		zend_get_parameters_ex(ZEND_NUM_ARGS(), &array, &to_upper) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	if (ZEND_NUM_ARGS() > 1) {
		convert_to_long_ex(to_upper);
		change_to_upper = Z_LVAL_PP(to_upper);
	}

	if (Z_TYPE_PP(array) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The argument should be an array");
		RETURN_FALSE;
	}

	array_init(return_value);

	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(array), &pos);
	while (zend_hash_get_current_data_ex(Z_ARRVAL_PP(array), (void **)&entry, &pos) == SUCCESS) {
		(*entry)->refcount++; 

		switch (zend_hash_get_current_key_ex(Z_ARRVAL_PP(array), &string_key, &str_key_len, &num_key, 0, &pos)) {
			case HASH_KEY_IS_LONG:
				zend_hash_index_update(Z_ARRVAL_P(return_value), num_key, entry, sizeof(entry), NULL);
				break;
			case HASH_KEY_IS_STRING:
				new_key=estrndup(string_key,str_key_len - 1);
				if (change_to_upper)
					php_strtoupper(new_key, str_key_len - 1);
				else
					php_strtolower(new_key, str_key_len - 1);
				zend_hash_update(Z_ARRVAL_P(return_value), new_key, str_key_len, entry, sizeof(entry), NULL);
				efree(new_key);
				break;
		}

		zend_hash_move_forward_ex(Z_ARRVAL_PP(array), &pos);
	}
}
/* }}} */

/* {{{ proto array array_unique(array input)
   Removes duplicate values from array */
PHP_FUNCTION(array_unique)
{
	zval **array, *tmp;
	HashTable *target_hash;
	Bucket *p;
	struct bucketindex {
		Bucket *b;
		unsigned int i;
	};
	struct bucketindex *arTmp, *cmpdata, *lastkept;
	unsigned int i;

	if (ZEND_NUM_ARGS() != 1 || zend_get_parameters_ex(1, &array) == FAILURE) {
		WRONG_PARAM_COUNT;
	}
	target_hash = HASH_OF(*array);
	if (!target_hash) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The argument should be an array");
		RETURN_FALSE;
	}

	array_init(return_value);
	zend_hash_copy(Z_ARRVAL_P(return_value), target_hash, (copy_ctor_func_t) zval_add_ref, (void *)&tmp, sizeof(zval*));

	if (target_hash->nNumOfElements <= 1) /* nothing to do */
			return;

	/* create and sort array with pointers to the target_hash buckets */
	arTmp = (struct bucketindex *) pemalloc((target_hash->nNumOfElements + 1) * sizeof(struct bucketindex), target_hash->persistent);
	if (!arTmp)
		RETURN_FALSE;
	for (i = 0, p = target_hash->pListHead; p; i++, p = p->pListNext) {
		arTmp[i].b = p;
		arTmp[i].i = i;
	}
	arTmp[i].b = NULL;
	set_compare_func(SORT_STRING TSRMLS_CC);
	zend_qsort((void *) arTmp, i, sizeof(struct bucketindex), array_data_compare TSRMLS_CC);

	/* go through the sorted array and delete duplicates from the copy */
	lastkept = arTmp;
	for (cmpdata = arTmp + 1; cmpdata->b; cmpdata++) {
		if (array_data_compare(lastkept, cmpdata TSRMLS_CC)) {
				lastkept = cmpdata;
		} else {
			if (lastkept->i > cmpdata->i) {
				p = lastkept->b;
				lastkept = cmpdata;
			} else {
				p = cmpdata->b;
			}
			if (p->nKeyLength)
					zend_hash_del(Z_ARRVAL_P(return_value), p->arKey, p->nKeyLength);  
			else
					zend_hash_index_del(Z_ARRVAL_P(return_value), p->h);  
		}
	}
	pefree(arTmp, target_hash->persistent);
}
/* }}} */

static void php_array_intersect(INTERNAL_FUNCTION_PARAMETERS, int behavior)
{
	zval ***args = NULL;
	HashTable *hash;
	int argc, i, c = 0;
	Bucket ***lists, **list, ***ptrs, *p;

	/* Get the argument count and check it */	
	argc = ARG_COUNT(ht);
	if (argc < 2) {
		WRONG_PARAM_COUNT;
	}
	/* Allocate arguments array and get the arguments, checking for errors. */
	args = (zval ***)safe_emalloc(sizeof(zval **), argc, 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}
	/* for each argument, create and sort list with pointers to the hash buckets */
	lists = (Bucket ***)safe_emalloc(sizeof(Bucket **), argc, 0);
	ptrs = (Bucket ***)safe_emalloc(sizeof(Bucket **), argc, 0);
	set_compare_func(SORT_STRING TSRMLS_CC);
	for (i=0; i<argc; i++) {
		if (Z_TYPE_PP(args[i]) != IS_ARRAY) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument #%d is not an array", i+1);
			argc = i; /* only free up to i-1 */
			goto out;
		}
		hash = HASH_OF(*args[i]);
		list = (Bucket **) pemalloc((hash->nNumOfElements + 1) * sizeof(Bucket *), hash->persistent);
		if (!list)
			RETURN_FALSE;
		lists[i] = list;
		ptrs[i] = list;
		for (p = hash->pListHead; p; p = p->pListNext)
			*list++ = p;
		*list = NULL;
		if (behavior == INTERSECT_NORMAL) {
			zend_qsort((void *) lists[i], hash->nNumOfElements, sizeof(Bucket *), array_data_compare TSRMLS_CC);
		} else if (behavior == INTERSECT_ASSOC) {
			zend_qsort((void *) lists[i], hash->nNumOfElements, sizeof(Bucket *), array_key_compare TSRMLS_CC);
		}
	}

	/* copy the argument array */
	*return_value = **args[0];
	zval_copy_ctor(return_value);
	if (return_value->value.ht == &EG(symbol_table)) {
		HashTable *ht;
		zval *tmp;

		ALLOC_HASHTABLE(ht);
		zend_hash_init(ht, 0, NULL, ZVAL_PTR_DTOR, 0);
		zend_hash_copy(ht, return_value->value.ht, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));
		return_value->value.ht = ht;		
	}

	/* go through the lists and look for common values */
	while (*ptrs[0]) {
		for (i=1; i<argc; i++) {
			if (behavior == INTERSECT_NORMAL) {
				while (*ptrs[i] && (0 < (c = array_data_compare(ptrs[0], ptrs[i] TSRMLS_CC))))
					ptrs[i]++;
			} else if (behavior == INTERSECT_ASSOC) {
				while (*ptrs[i] && (0 < (c = array_key_compare(ptrs[0], ptrs[i] TSRMLS_CC))))
					ptrs[i]++;
				if (!c && *ptrs[i]) { /* this means that ptrs[i] is not NULL so we can compare */
						/* and "c==0" is from last operation */
				 	if (array_data_compare(ptrs[0], ptrs[i] TSRMLS_CC) != 0) {
				 		c = 1;
				 		/* we are going to the break */
				 	} else {
				 		/* continue looping */
				 	}
				}
			}
			if (!*ptrs[i]) {
				/* delete any values corresponding to remains of ptrs[0] */
				/* and exit because they do not present in at least one of */
				/* the other arguments */
				for (;;) {
					p = *ptrs[0]++;
					if (!p)
						goto out;
					if (p->nKeyLength)
						zend_hash_del(Z_ARRVAL_P(return_value), p->arKey, p->nKeyLength);  
					else
						zend_hash_index_del(Z_ARRVAL_P(return_value), p->h);  
				}
			}
			if (c) /* here we get if not all are equal */
				break;
			ptrs[i]++;
		}
		if (c) {
			/* Value of ptrs[0] not in all arguments, delete all entries */
			/* with value < value of ptrs[i] */
			for (;;) {
				p = *ptrs[0];
				if (p->nKeyLength)
					zend_hash_del(Z_ARRVAL_P(return_value), p->arKey, p->nKeyLength);  
				else
					zend_hash_index_del(Z_ARRVAL_P(return_value), p->h);  
				if (!*++ptrs[0])
					goto out;
				if (behavior == INTERSECT_NORMAL) {
					if (0 <= array_data_compare(ptrs[0], ptrs[i] TSRMLS_CC))
						break;
				} else if (behavior == INTERSECT_ASSOC) {
					/* no need of looping because indexes are unique */
					break;
				}
			}
		} else {
			/* ptrs[0] is present in all the arguments */
			/* Skip all entries with same value as ptrs[0] */
			for (;;) {
				if (!*++ptrs[0])
					goto out;
				if (behavior == INTERSECT_NORMAL) {
					if (array_data_compare(ptrs[0]-1, ptrs[0] TSRMLS_CC))
						break;
				} else if (behavior == INTERSECT_ASSOC) {
					/* no need of looping because indexes are unique */
					break;
				}
			}
		}
	}

out:
	for (i=0; i<argc; i++) {
		hash = HASH_OF(*args[i]);
		pefree(lists[i], hash->persistent);
	}
	efree(ptrs);
	efree(lists);
	efree(args);
}

/* {{{ proto array array_intersect(array arr1, array arr2 [, array ...])
   Returns the entries of arr1 that have values which are present in all the other arguments */
PHP_FUNCTION(array_intersect)
{
	php_array_intersect(INTERNAL_FUNCTION_PARAM_PASSTHRU, INTERSECT_NORMAL);
}
/* }}} */

/* {{{ proto array array_intersect_assoc(array arr1, array arr2 [, array ...])
   Returns the entries of arr1 that have values which are present in all the other arguments. Keys are used to do more restrctive check */
PHP_FUNCTION(array_intersect_assoc)
{
	php_array_intersect(INTERNAL_FUNCTION_PARAM_PASSTHRU, INTERSECT_ASSOC);
}
/* }}} */


static void php_array_diff(INTERNAL_FUNCTION_PARAMETERS, int behavior)
{
	zval ***args = NULL;
	HashTable *hash;
	int argc, i, c;
	Bucket ***lists, **list, ***ptrs, *p;

	/* Get the argument count and check it */
	argc = ARG_COUNT(ht);
	if (argc < 2) {
		WRONG_PARAM_COUNT;
	}
	/* Allocate arguments array and get the arguments, checking for errors. */
	args = (zval ***)safe_emalloc(sizeof(zval **), argc, 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}
	/* for each argument, create and sort list with pointers to the hash buckets */
	lists = (Bucket ***)safe_emalloc(sizeof(Bucket **), argc, 0);
	ptrs = (Bucket ***)safe_emalloc(sizeof(Bucket **), argc, 0);
	set_compare_func(SORT_STRING TSRMLS_CC);
	for (i = 0; i < argc; i++) {
		if (Z_TYPE_PP(args[i]) != IS_ARRAY) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument #%d is not an array", i + 1);
			argc = i; /* only free up to i-1 */
			goto out;
		}
		hash = HASH_OF(*args[i]);
		list = (Bucket **) pemalloc((hash->nNumOfElements + 1) * sizeof(Bucket *), hash->persistent);
		if (!list) {
			RETURN_FALSE;
		}
		lists[i] = list;
		ptrs[i] = list;
		for (p = hash->pListHead; p; p = p->pListNext) {
			*list++ = p;
		}
		*list = NULL;
		if (behavior == DIFF_NORMAL) {
			zend_qsort((void *) lists[i], hash->nNumOfElements, sizeof(Bucket *), array_data_compare TSRMLS_CC);
		} else if (behavior == DIFF_ASSOC) {
			zend_qsort((void *) lists[i], hash->nNumOfElements, sizeof(Bucket *), array_key_compare TSRMLS_CC);
		}
	}

	/* copy the argument array */
	*return_value = **args[0];
	zval_copy_ctor(return_value);
	if (return_value->value.ht == &EG(symbol_table)) {
		HashTable *ht;
		zval *tmp;

		ALLOC_HASHTABLE(ht);
		zend_hash_init(ht, 0, NULL, ZVAL_PTR_DTOR, 0);
		zend_hash_copy(ht, return_value->value.ht, (copy_ctor_func_t) zval_add_ref, (void *) &tmp, sizeof(zval *));
		return_value->value.ht = ht;		
	}

	/* go through the lists and look for values of ptr[0]
		   that are not in the others */
	while (*ptrs[0]) {
		c = 1;
		for (i = 1; i < argc; i++) {
			if (behavior == DIFF_NORMAL) {
				while (*ptrs[i] && (0 < (c = array_data_compare(ptrs[0], ptrs[i] TSRMLS_CC)))) {
					ptrs[i]++;
				}
			} else if (behavior == DIFF_ASSOC) {
				while (*ptrs[i] && (0 < (c = array_key_compare(ptrs[0], ptrs[i] TSRMLS_CC)))) {
					ptrs[i]++;
				}
			}
			if (!c) {
				if (behavior == DIFF_NORMAL) {
					if (*ptrs[i]) {
						ptrs[i]++;
					}
					break;
				} else if (behavior == DIFF_ASSOC) {
					if (*ptrs[i]) {
						if (array_data_compare(ptrs[0], ptrs[i] TSRMLS_CC) != 0) {
							c = -1;
						} else {
							break;
						}
					}
				}
			}
		}
		if (!c) {
			/* ptrs[0] in one of the other arguments */
			/* delete all entries with value as ptrs[0] */
			for (;;) {
				p = *ptrs[0];
				if (p->nKeyLength) {
					zend_hash_del(Z_ARRVAL_P(return_value), p->arKey, p->nKeyLength);  
				} else {
					zend_hash_index_del(Z_ARRVAL_P(return_value), p->h);
				}
				if (!*++ptrs[0]) {
					goto out;
				}
				if (behavior == DIFF_NORMAL) {
					if (array_data_compare(ptrs[0] - 1, ptrs[0] TSRMLS_CC)) {
						break;
					}
				} else if (behavior == DIFF_ASSOC) {
					/* in this case no array_key_compare is needed */
					break;
				}
			}
		} else {
			/* ptrs[0] in none of the other arguments */
			/* skip all entries with value as ptrs[0] */
			for (;;) {
				if (!*++ptrs[0]) {
					goto out;
				}
				if (behavior == DIFF_NORMAL) {
					if (array_data_compare(ptrs[0]-1, ptrs[0] TSRMLS_CC)) {
						break;
					}
				} else if (behavior == DIFF_ASSOC) {
					/* in this case no array_key_compare is needed */
					break;
				}
			}
		}
	}
out:
	for (i = 0; i < argc; i++) {
		hash = HASH_OF(*args[i]);
		pefree(lists[i], hash->persistent);
	}
	efree(ptrs);
	efree(lists);
	efree(args);
}

/* {{{ proto array array_diff(array arr1, array arr2 [, array ...])
   Returns the entries of arr1 that have values which are not present in any of the others arguments */
PHP_FUNCTION(array_diff)
{
	php_array_diff(INTERNAL_FUNCTION_PARAM_PASSTHRU, DIFF_NORMAL);
}
/* }}} */

/* {{{ proto array array_diff_assoc(array arr1, array arr2 [, array ...])
   Returns the entries of arr1 that have values which are not present in any of the others arguments but do additional checks whether the keys are equal */
PHP_FUNCTION(array_diff_assoc)
{
	php_array_diff(INTERNAL_FUNCTION_PARAM_PASSTHRU, DIFF_ASSOC);
}
/* }}} */



#define MULTISORT_ORDER	0
#define MULTISORT_TYPE	1
#define MULTISORT_LAST	2

int multisort_compare(const void *a, const void *b TSRMLS_DC)
{
	Bucket**	  ab = *(Bucket ***)a;
	Bucket**	  bb = *(Bucket ***)b;
	int			  r;
	int			  result = 0;
	zval		  temp;
	
	r = 0;
	do {
		set_compare_func(ARRAYG(multisort_flags)[MULTISORT_TYPE][r] TSRMLS_CC);

		ARRAYG(compare_func)(&temp, *((zval **)ab[r]->pData), *((zval **)bb[r]->pData) TSRMLS_CC);
		result = ARRAYG(multisort_flags)[MULTISORT_ORDER][r] * Z_LVAL(temp);
		if (result != 0)
			return result;
		r++;
	} while (ab[r] != NULL);
	return result;
}

#define MULTISORT_ABORT						\
	for (k = 0; k < MULTISORT_LAST; k++)	\
		efree(ARRAYG(multisort_flags)[k]);	\
	efree(arrays);							\
	efree(args);							\
	RETURN_FALSE;

/* {{{ proto bool array_multisort(array ar1 [, SORT_ASC|SORT_DESC [, SORT_REGULAR|SORT_NUMERIC|SORT_STRING]] [, array ar2 [, SORT_ASC|SORT_DESC [, SORT_REGULAR|SORT_NUMERIC|SORT_STRING]], ...])
   Sort multiple arrays at once similar to how ORDER BY clause works in SQL */
PHP_FUNCTION(array_multisort)
{
	zval***			args;
	zval***			arrays;
	Bucket***		indirect;
	Bucket*			p;
	HashTable*		hash;
	int				argc;
	int				array_size;
	int				num_arrays = 0;
	int				parse_state[MULTISORT_LAST];   /* 0 - flag not allowed
											 		  1 - flag allowed	 */
	int				sort_order = SORT_ASC;
	int				sort_type  = SORT_REGULAR;
	int				i, k;
	
	/* Get the argument count and check it */
	argc = ZEND_NUM_ARGS();
	if (argc < 1) {
		WRONG_PARAM_COUNT;
	}
	
	/* Allocate arguments array and get the arguments, checking for errors. */
	args = (zval ***)safe_emalloc(sizeof(zval **), argc, 0);
	if (zend_get_parameters_array_ex(argc, args) == FAILURE) {
		efree(args);
		WRONG_PARAM_COUNT;
	}

	/* Allocate space for storing pointers to input arrays and sort flags. */
	arrays = (zval ***)ecalloc(argc, sizeof(zval **)); 
	for (i = 0; i < MULTISORT_LAST; i++) {
		parse_state[i] = 0;
		ARRAYG(multisort_flags)[i] = (int *)ecalloc(argc, sizeof(int));
	}

	/* Here we go through the input arguments and parse them. Each one can
	   be either an array or a sort flag which follows an array. If not
	   specified, the sort flags defaults to SORT_ASC and SORT_REGULAR
	   accordingly. There can't be two sort flags of the same type after an
	   array, and the very first argument has to be an array.
	 */
	for (i = 0; i < argc; i++) {
		if (Z_TYPE_PP(args[i]) == IS_ARRAY) {
			/* We see the next array, so we update the sort flags of
			   the previous array and reset the sort flags. */
			if (i > 0) {
				ARRAYG(multisort_flags)[MULTISORT_ORDER][num_arrays-1] = sort_order;
				ARRAYG(multisort_flags)[MULTISORT_TYPE][num_arrays-1] = sort_type;
				sort_order = SORT_ASC;
				sort_type = SORT_REGULAR;
			}
			arrays[num_arrays++] = args[i];

			/* Next one may be an array or a list of sort flags. */
			for (k = 0; k < MULTISORT_LAST; k++)
				parse_state[k] = 1;
		} else if (Z_TYPE_PP(args[i]) == IS_LONG) {
			switch (Z_LVAL_PP(args[i])) {
				case SORT_ASC:
				case SORT_DESC:
					/* flag allowed here */
					if (parse_state[MULTISORT_ORDER] == 1) {
						/* Save the flag and make sure then next arg is not the current flag. */
						sort_order = Z_LVAL_PP(args[i]) == SORT_DESC ? -1 : 1;
						parse_state[MULTISORT_ORDER] = 0;
					} else {
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument #%d is expected to be an array or sorting flag that has not already been specified", i+1);
						MULTISORT_ABORT;
					}
					break;

				case SORT_REGULAR:
				case SORT_NUMERIC:
				case SORT_STRING:
					/* flag allowed here */
					if (parse_state[MULTISORT_TYPE] == 1) {
						/* Save the flag and make sure then next arg is not the current flag. */
						sort_type = Z_LVAL_PP(args[i]);
						parse_state[MULTISORT_TYPE] = 0;
					} else {
						php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument #%d is expected to be an array or sorting flag that has not already been specified", i + 1);
						MULTISORT_ABORT;
					}
					break;

				default:
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument #%d is an unknown sort flag", i + 1);
					MULTISORT_ABORT;
					break;

			}
		} else {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument #%d is expected to be an array or a sort flag", i + 1);
			MULTISORT_ABORT;
		}
	}
	/* Take care of the last array sort flags. */
	ARRAYG(multisort_flags)[MULTISORT_ORDER][num_arrays-1] = sort_order;
	ARRAYG(multisort_flags)[MULTISORT_TYPE][num_arrays-1] = sort_type;
	
	/* Make sure the arrays are of the same size. */
	array_size = zend_hash_num_elements(Z_ARRVAL_PP(arrays[0]));
	for (i = 0; i < num_arrays; i++) {
		if (zend_hash_num_elements(Z_ARRVAL_PP(arrays[i])) != array_size) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Array sizes are inconsistent");
			MULTISORT_ABORT;
		}
	}

	/* If all arrays are empty or have only one entry,
	   we don't need to do anything. */
	if (array_size < 1) {
		for (k = 0; k < MULTISORT_LAST; k++)
			efree(ARRAYG(multisort_flags)[k]);
		efree(arrays);
		efree(args);
		RETURN_TRUE;
	}

	/* Create the indirection array. This array is of size MxN, where 
	   M is the number of entries in each input array and N is the number
	   of the input arrays + 1. The last column is NULL to indicate the end
	   of the row.
	 */
	indirect = (Bucket ***)safe_emalloc(sizeof(Bucket **), array_size, 0);
	for (i = 0; i < array_size; i++)
		indirect[i] = (Bucket **)safe_emalloc(sizeof(Bucket *), (num_arrays+1), 0);
	
	for (i = 0; i < num_arrays; i++) {
		k = 0;
		for (p = Z_ARRVAL_PP(arrays[i])->pListHead; p; p = p->pListNext, k++) {
			indirect[k][i] = p;
		}
	}
	for (k = 0; k < array_size; k++)
		indirect[k][num_arrays] = NULL;

	/* Do the actual sort magic - bada-bim, bada-boom. */
	zend_qsort(indirect, array_size, sizeof(Bucket **), multisort_compare TSRMLS_CC);
	
	/* Restructure the arrays based on sorted indirect - this is mostly
	   taken from zend_hash_sort() function. */
	HANDLE_BLOCK_INTERRUPTIONS();
	for (i = 0; i < num_arrays; i++) {
		hash = Z_ARRVAL_PP(arrays[i]);
		hash->pListHead = indirect[0][i];;
		hash->pListTail = NULL;
		hash->pInternalPointer = hash->pListHead;

		for (k = 0; k < array_size; k++) {
			if (hash->pListTail) {
				hash->pListTail->pListNext = indirect[k][i];
			}
			indirect[k][i]->pListLast = hash->pListTail;
			indirect[k][i]->pListNext = NULL;
			hash->pListTail = indirect[k][i];
		}
		
		p = hash->pListHead;
		k = 0;
		while (p != NULL) {
			if (p->nKeyLength == 0)
				p->h = k++;
			p = p->pListNext;
		}
		hash->nNextFreeElement = array_size;
		zend_hash_rehash(hash);
	}
	HANDLE_UNBLOCK_INTERRUPTIONS();
		
	/* Clean up. */	
	for (i = 0; i < array_size; i++)
		efree(indirect[i]);
	efree(indirect);
	for (k = 0; k < MULTISORT_LAST; k++)
		efree(ARRAYG(multisort_flags)[k]);
	efree(arrays);
	efree(args);
	RETURN_TRUE;
}
/* }}} */


/* {{{ proto mixed array_rand(array input [, int num_req])
   Return key/keys for random entry/entries in the array */
PHP_FUNCTION(array_rand)
{
	zval **input, **num_req;
	long randval;
	int num_req_val, num_avail, key_type;
	char *string_key;
	uint string_key_len;
	ulong num_key;
	HashPosition pos;

	if (ZEND_NUM_ARGS() < 1 || ZEND_NUM_ARGS() > 2 ||
		zend_get_parameters_ex(ZEND_NUM_ARGS(), &input, &num_req) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	if (Z_TYPE_PP(input) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "First argument has to be an array");
		return;
	}

	num_avail = zend_hash_num_elements(Z_ARRVAL_PP(input));

	if (ZEND_NUM_ARGS() > 1) {
		convert_to_long_ex(num_req);
		num_req_val = Z_LVAL_PP(num_req);
		if (num_req_val <= 0 || num_req_val > num_avail) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Second argument has to be between 1 and the number of elements in the array");
			return;
		}
	} else
		num_req_val = 1;

	/* Make the return value an array only if we need to pass back more than one
	   result. */
	if (num_req_val > 1)
		array_init(return_value);

	/* We can't use zend_hash_index_find() because the array may have string keys or gaps. */
	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(input), &pos);
	while (num_req_val && (key_type = zend_hash_get_current_key_ex(Z_ARRVAL_PP(input), &string_key, &string_key_len, &num_key, 0, &pos)) != HASH_KEY_NON_EXISTANT) {

		randval = php_rand(TSRMLS_C);

		if ((double)(randval/(PHP_RAND_MAX+1.0)) < (double)num_req_val/(double)num_avail) {
			/* If we are returning a single result, just do it. */
			if (Z_TYPE_P(return_value) != IS_ARRAY) {
				if (key_type == HASH_KEY_IS_STRING) {
					RETURN_STRINGL(string_key, string_key_len-1, 1);
				} else {
					RETURN_LONG(num_key);
				}
			} else {
				/* Append the result to the return value. */
				if (key_type == HASH_KEY_IS_STRING)
					add_next_index_stringl(return_value, string_key, string_key_len-1, 1);
				else
					add_next_index_long(return_value, num_key);
			}
			num_req_val--;
		}
		num_avail--;
		zend_hash_move_forward_ex(Z_ARRVAL_PP(input), &pos);
	}  

	if (num_req_val == num_avail) {
		array_data_shuffle(return_value TSRMLS_CC);
	}
}
/* }}} */

/* {{{ proto mixed array_sum(array input)
   Returns the sum of the array entries */
 
PHP_FUNCTION(array_sum)
{
	zval **input,
		 **entry,
		 entry_n;
	int argc = ZEND_NUM_ARGS();
	HashPosition pos;
	double dval;
	
	if (argc != 1 || zend_get_parameters_ex(argc, &input) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	if (Z_TYPE_PP(input) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The argument should be an array");
		return;
	}

	ZVAL_LONG(return_value, 0);

	for (zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(input), &pos);
		 zend_hash_get_current_data_ex(Z_ARRVAL_PP(input), (void **)&entry, &pos) == SUCCESS;
		 zend_hash_move_forward_ex(Z_ARRVAL_PP(input), &pos)) {
		
		if (Z_TYPE_PP(entry) == IS_ARRAY || Z_TYPE_PP(entry) == IS_OBJECT)
			continue;

		entry_n = **entry;
		zval_copy_ctor(&entry_n);
		convert_scalar_to_number(&entry_n TSRMLS_CC);

		if (Z_TYPE(entry_n) == IS_LONG && Z_TYPE_P(return_value) == IS_LONG) {
			dval = (double)Z_LVAL_P(return_value) + (double)Z_LVAL(entry_n);
			if ( (double)LONG_MIN <= dval && dval <= (double)LONG_MAX ) {
				Z_LVAL_P(return_value) += Z_LVAL(entry_n);
				continue;
			}
		}
		convert_to_double(return_value);
		convert_to_double(&entry_n);
		Z_DVAL_P(return_value) += Z_DVAL(entry_n);
	}
}

/* }}} */

/* {{{ proto mixed array_reduce(array input, mixed callback [, int initial])
   Iteratively reduce the array to a single value via the callback. */
PHP_FUNCTION(array_reduce)
{
	zval **input, **callback, **initial;
	zval **args[2];
	zval **operand;
	zval *result = NULL;
	zval *retval;
	char *callback_name;
	HashPosition pos;
	HashTable *htbl;
	
	if (ZEND_NUM_ARGS() < 2 || ZEND_NUM_ARGS() > 3 ||
		zend_get_parameters_ex(ZEND_NUM_ARGS(), &input, &callback, &initial) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	if (Z_TYPE_PP(input) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The first argument should be an array");
		return;
	}

	if (!zend_is_callable(*callback, 0, &callback_name)) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The second argument, '%s', should be a valid callback", callback_name);
		efree(callback_name);
		return;
	}
	efree(callback_name);

	if (ZEND_NUM_ARGS() > 2) {
		ALLOC_ZVAL(result);
		*result = **initial;
		zval_copy_ctor(result);
		convert_to_long(result);
		INIT_PZVAL(result);
	} else {
		MAKE_STD_ZVAL(result);
		ZVAL_NULL(result);
	}

	/* (zval **)input points to an element of argument stack
	 * the base pointer of which is subject to change.
	 * thus we need to keep the pointer to the hashtable for safety */

	htbl = Z_ARRVAL_PP(input);
	
	if (zend_hash_num_elements(htbl) == 0) {
		if (result) {
			*return_value = *result;
			zval_copy_ctor(return_value);
			zval_ptr_dtor(&result);
		}
		return;
	}

	zend_hash_internal_pointer_reset_ex(htbl, &pos);
	while (zend_hash_get_current_data_ex(htbl, (void **)&operand, &pos) == SUCCESS) {
		if (result) {
			args[0] = &result;
			args[1] = operand;
			if (call_user_function_ex(EG(function_table), NULL, *callback, &retval, 2, args, 0, NULL TSRMLS_CC) == SUCCESS && retval) {
				zval_ptr_dtor(&result);
				result = retval;
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "An error occurred while invoking the reduction callback");
				return;
			}
		} else {
			result = *operand;
			zval_add_ref(&result);
		}

		zend_hash_move_forward_ex(htbl, &pos);
	}
	
	*return_value = *result;
	zval_copy_ctor(return_value);
	zval_ptr_dtor(&result);
}
/* }}} */


/* {{{ proto array array_filter(array input [, mixed callback])
   Filters elements from the array via the callback. */
PHP_FUNCTION(array_filter)
{
	zval **input, **callback = NULL;
	zval *array, *func = NULL;
	zval **operand;
	zval **args[1];
	zval *retval = NULL;
	char *callback_name;
	char *string_key;
	uint string_key_len;
	ulong num_key;
	HashPosition pos;
	
	if (ZEND_NUM_ARGS() < 1 || ZEND_NUM_ARGS() > 2 ||
		zend_get_parameters_ex(ZEND_NUM_ARGS(), &input, &callback) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	if (Z_TYPE_PP(input) != IS_ARRAY) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The first argument should be an array");
		return;
	}
	if (callback) {
		func = *callback;
	}
	array = *input;

	if (ZEND_NUM_ARGS() > 1) {
		if (!zend_is_callable(func, 0, &callback_name)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "The second argument, '%s', should be a valid callback", callback_name);
			efree(callback_name);
			return;
		}
		efree(callback_name);
	}

	array_init(return_value);
	if (zend_hash_num_elements(Z_ARRVAL_P(array)) == 0)
		return;

	for (zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(array), &pos);
		 zend_hash_get_current_data_ex(Z_ARRVAL_P(array), (void **)&operand, &pos) == SUCCESS;
		 zend_hash_move_forward_ex(Z_ARRVAL_P(array), &pos)) {

		if (func) {
			args[0] = operand;
			if (call_user_function_ex(EG(function_table), NULL, func, &retval, 1, args, 0, NULL TSRMLS_CC) == SUCCESS && retval) {
				if (!zend_is_true(retval)) {
					zval_ptr_dtor(&retval);
					continue;
				} else
					zval_ptr_dtor(&retval);
			} else {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "An error occurred while invoking the filter callback");
				return;
			}
		} else if (!zend_is_true(*operand))
			continue;

		zval_add_ref(operand);
		switch (zend_hash_get_current_key_ex(Z_ARRVAL_P(array), &string_key, &string_key_len, &num_key, 0, &pos)) {
			case HASH_KEY_IS_STRING:
				zend_hash_update(Z_ARRVAL_P(return_value), string_key,
								 string_key_len, operand, sizeof(zval *), NULL);
				break;

			case HASH_KEY_IS_LONG:
				zend_hash_index_update(Z_ARRVAL_P(return_value), num_key,
									   operand, sizeof(zval *), NULL);
				break;
		}
	}
}
/* }}} */


/* {{{ proto array array_map(mixed callback, array input1 [, array input2 ,...])
   Applies the callback to the elements in given arrays. */
PHP_FUNCTION(array_map)
{
	zval ***pargs = NULL;
	zval ***params;
	zval *callback;
	zval *result, *null;
	HashPosition *array_pos;
	zval **args;
	char *callback_name;
	int i, k, maxlen = 0;
	int *array_len;

	if (ZEND_NUM_ARGS() < 2) {
		WRONG_PARAM_COUNT;
	}

	RETVAL_NULL();

	pargs = (zval ***)safe_emalloc(ZEND_NUM_ARGS(), sizeof(zval **), 0);
	if (zend_get_parameters_array_ex(ZEND_NUM_ARGS(), pargs) == FAILURE) {
		efree(pargs);
		WRONG_PARAM_COUNT;
	}

	callback = *pargs[0];

	if (Z_TYPE_P(callback) != IS_NULL) {
		if (!zend_is_callable(callback, 0, &callback_name)) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "The first argument, '%s', should be either NULL or a valid callback", callback_name);
			efree(callback_name);
			efree(pargs);
			return;
		}
		efree(callback_name);
	}

	args = (zval **)safe_emalloc(ZEND_NUM_ARGS(), sizeof(zval *), 0);
	array_len = (int *)safe_emalloc(ZEND_NUM_ARGS(), sizeof(int), 0);
	array_pos = (HashPosition *)safe_emalloc(ZEND_NUM_ARGS(), sizeof(HashPosition), 0);

	for (i = 1; i < ZEND_NUM_ARGS(); i++) {
		if (Z_TYPE_PP(pargs[i]) != IS_ARRAY) {
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "Argument #%d should be an array", i + 1);
			efree(pargs);
			efree(args);
			efree(array_len);
			efree(array_pos);
			return;
		}
		SEPARATE_ZVAL_IF_NOT_REF(pargs[i]);
		args[i] = *pargs[i];
		array_len[i] = zend_hash_num_elements(Z_ARRVAL_PP(pargs[i]));
		if (array_len[i] > maxlen) {
			maxlen = array_len[i];
		}
		zend_hash_internal_pointer_reset_ex(Z_ARRVAL_PP(pargs[i]), &array_pos[i]);
	}

	efree(pargs);

	/* Short-circuit: if no callback and only one array, just return it. */
	if (Z_TYPE_P(callback) == IS_NULL && ZEND_NUM_ARGS() == 2) {
		*return_value = *args[1];
		zval_copy_ctor(return_value);
		efree(array_len);
		efree(array_pos);
		efree(args);
		return;
	}

	array_init(return_value);
	params = (zval ***)safe_emalloc(ZEND_NUM_ARGS(), sizeof(zval **), 0);
	MAKE_STD_ZVAL(null);
	ZVAL_NULL(null);

	/* We iterate through all the arrays at once. */
	for (k = 0; k < maxlen; k++) {
		uint str_key_len;
		ulong num_key;
		char *str_key;
		int key_type = 0;

		/*
		 * If no callback, the result will be an array, consisting of current
		 * entries from all arrays.
		 */
		if (Z_TYPE_P(callback) == IS_NULL) {
			MAKE_STD_ZVAL(result);
			array_init(result);
		}

		for (i = 1; i < ZEND_NUM_ARGS(); i++) {
			/*
			 * If this array still hash elements, add the current one to the
			 * parameter list, otherwise use null value.
			 */
			if (k < array_len[i]) {
				zend_hash_get_current_data_ex(Z_ARRVAL_P(args[i]), (void **)&params[i], &array_pos[i]);

				/*
				 * It is safe to store only last value of key type, because
				 * this loop will run just once if there is only 1 array.
				 */
				if (ZEND_NUM_ARGS() == 2) {
					key_type = zend_hash_get_current_key_ex(Z_ARRVAL_P(args[1]), &str_key, &str_key_len, &num_key, 0, &array_pos[i]);
				}
															
				zend_hash_move_forward_ex(Z_ARRVAL_P(args[i]), &array_pos[i]);
			} else {
				params[i] = &null;
			}

			if (Z_TYPE_P(callback) == IS_NULL) {
				zval_add_ref(params[i]);
				add_next_index_zval(result, *params[i]);
			}
		}

		if (Z_TYPE_P(callback) != IS_NULL) {
			if (!call_user_function_ex(EG(function_table), NULL, callback, &result, ZEND_NUM_ARGS() - 1, &params[1], 0, NULL TSRMLS_CC) == SUCCESS && result) {
				php_error_docref(NULL TSRMLS_CC, E_WARNING, "An error occurred while invoking the map callback");
				efree(array_len);
				efree(args);
				efree(array_pos);
				zval_dtor(return_value);
				RETURN_NULL();
			}
		}

		if (ZEND_NUM_ARGS() > 2) {
			add_next_index_zval(return_value, result);
		} else {
			if (key_type == HASH_KEY_IS_STRING) {
				add_assoc_zval_ex(return_value, str_key, str_key_len, result);
			} else {
				add_index_zval(return_value, num_key, result);
			}
		}
	}
	
	zval_ptr_dtor(&null);
	efree(params);
	efree(array_len);
	efree(array_pos);
	efree(args);
}
/* }}} */


/* {{{ proto bool array_key_exists(mixed key, array search)
   Checks if the given key or index exists in the array */
PHP_FUNCTION(array_key_exists)
{
	zval **key,					/* key to check for */
		 **array;				/* array to check in */

	if (ZEND_NUM_ARGS() != 2 ||
		zend_get_parameters_ex(ZEND_NUM_ARGS(), &key, &array) == FAILURE) {
		WRONG_PARAM_COUNT;
	}

	if (Z_TYPE_PP(array) != IS_ARRAY && Z_TYPE_PP(array) != IS_OBJECT) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "The second argument should be either an array or an object");
		RETURN_FALSE;
	}

	switch (Z_TYPE_PP(key)) {
		case IS_STRING:
			if (zend_hash_exists(HASH_OF(*array), Z_STRVAL_PP(key), Z_STRLEN_PP(key)+1)) {
				RETURN_TRUE;
			}
			RETURN_FALSE;

		case IS_LONG:
			if (zend_hash_index_exists(HASH_OF(*array), Z_LVAL_PP(key))) {
				RETURN_TRUE;
			}
			RETURN_FALSE;
		case IS_NULL:
			if (zend_hash_exists(HASH_OF(*array), "", 1)) {
				RETURN_TRUE;
			}
			RETURN_FALSE;

		default:
			php_error_docref(NULL TSRMLS_CC, E_WARNING, "The first argument should be either a string or an integer");
			RETURN_FALSE;
	}

}
/* }}} */


/* {{{ proto array array_chunk(array input, int size [, bool preserve_keys])
   Split array into chunks */
PHP_FUNCTION(array_chunk)
{
	int argc = ZEND_NUM_ARGS(), key_type;
	long size, current = 0;
	char *str_key;
	uint str_key_len;
	ulong num_key;
	zend_bool preserve_keys = 0;
	zval *input = NULL;
	zval *chunk = NULL;
	zval **entry;
	HashPosition pos;

	if (zend_parse_parameters(argc TSRMLS_CC, "al|b", &input, &size,
							  &preserve_keys) == FAILURE) 
		return;

	/* Do bounds checking for size parameter. */
	if (size < 1) {
		php_error_docref(NULL TSRMLS_CC, E_WARNING, "Size parameter expected to be greater than 0");
		return;
	}

	array_init(return_value);

	zend_hash_internal_pointer_reset_ex(Z_ARRVAL_P(input), &pos);
	while (zend_hash_get_current_data_ex(Z_ARRVAL_P(input), (void**)&entry, &pos) == SUCCESS) {
		/* If new chunk, create and initialize it. */
		if (!chunk) {
			MAKE_STD_ZVAL(chunk);
			array_init(chunk);
		}

		/* Add entry to the chunk, preserving keys if necessary. */
		zval_add_ref(entry);

		if (preserve_keys) {
			key_type = zend_hash_get_current_key_ex(Z_ARRVAL_P(input), &str_key,
													&str_key_len, &num_key, 0, &pos);
			if (key_type == HASH_KEY_IS_STRING) {
				add_assoc_zval_ex(chunk, str_key, str_key_len, *entry);
			} else {
				add_index_zval(chunk, num_key, *entry);
			}
		} else {
		   	add_next_index_zval(chunk, *entry);
		}

		/*
		 * If reached the chunk size, add it to the result array, and reset the
		 * pointer.
		 */
		if (!(++current % size)) {
			add_next_index_zval(return_value, chunk);
			chunk = NULL;
		}

		zend_hash_move_forward_ex(Z_ARRVAL_P(input), &pos);
	}

	/* Add the final chunk if there is one. */
	if (chunk) {
		add_next_index_zval(return_value, chunk);
	}
}
/* }}} */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */
