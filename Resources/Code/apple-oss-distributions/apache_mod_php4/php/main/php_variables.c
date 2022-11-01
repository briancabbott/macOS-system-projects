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
   | Authors: Rasmus Lerdorf <rasmus@lerdorf.on.ca>                       |
   |          Zeev Suraski <zeev@zend.com>                                |
   +----------------------------------------------------------------------+
 */
/* $Id: php_variables.c,v 1.45.2.13.2.14 2007/12/31 07:22:55 sebastian Exp $ */

#include <stdio.h>
#include "php.h"
#include "ext/standard/php_standard.h"
#include "php_variables.h"
#include "php_globals.h"
#include "php_content_types.h"
#include "SAPI.h"

#include "zend_globals.h"

/* for systems that need to override reading of environment variables */
void _php_import_environment_variables(zval *array_ptr TSRMLS_DC);
PHPAPI void (*php_import_environment_variables)(zval *array_ptr TSRMLS_DC) = _php_import_environment_variables;

PHPAPI void php_register_variable(char *var, char *strval, zval *track_vars_array TSRMLS_DC)
{
	php_register_variable_safe(var, strval, strlen(strval), track_vars_array TSRMLS_CC);
}


/* binary-safe version */
PHPAPI void php_register_variable_safe(char *var, char *strval, int str_len, zval *track_vars_array TSRMLS_DC)
{
	zval new_entry;
	assert(strval != NULL);
	
	/* Prepare value */
	Z_STRLEN(new_entry) = str_len;
	if (PG(magic_quotes_gpc)) {
		Z_STRVAL(new_entry) = php_addslashes(strval, Z_STRLEN(new_entry), &Z_STRLEN(new_entry), 0 TSRMLS_CC);
	} else {
		Z_STRVAL(new_entry) = estrndup(strval, Z_STRLEN(new_entry));
	}
	Z_TYPE(new_entry) = IS_STRING;

	php_register_variable_ex(var, &new_entry, track_vars_array TSRMLS_CC);
}


PHPAPI void php_register_variable_ex(char *var, zval *val, pval *track_vars_array TSRMLS_DC)
{
	char *p = NULL;
	char *ip;		/* index pointer */
	char *index;
	int var_len, index_len;
	zval *gpc_element, **gpc_element_p;
	zend_bool is_array;
	HashTable *symtable1=NULL;
	int nest_level = 0;

	assert(var != NULL);
	
	if (track_vars_array) {
		symtable1 = Z_ARRVAL_P(track_vars_array);
	} else if (PG(register_globals)) {
		symtable1 = EG(active_symbol_table);
		/* GLOBALS hijack attempt, reject parameter */
		if (!strncmp("GLOBALS", var, sizeof("GLOBALS")) || !strncmp("GLOBALS", var, sizeof("GLOBALS[")-1)) {
			return;
		}
	}
	if (!symtable1) {
		/* Nothing to do */
		zval_dtor(val);
		return;
	}

	/*
	 * Prepare variable name
	 */
	ip = strchr(var, '[');
	if (ip) {
		is_array = 1;
		*ip = 0;
	} else {
		is_array = 0;
	}
	/* ignore leading spaces in the variable name */
	while (*var && *var==' ') {
		var++;
	}
	var_len = strlen(var);
	if (var_len==0) { /* empty variable name, or variable name with a space in it */
		zval_dtor(val);
		return;
	}

	/* GLOBALS hijack attempt, reject parameter */
	if (symtable1 == EG(active_symbol_table) && !strcmp("GLOBALS", var)) {
		zval_dtor(val);
		return;
	}

	/* ensure that we don't have spaces or dots in the variable name (not binary safe) */
	for (p=var; *p; p++) {
		switch(*p) {
			case ' ':
			case '.':
				*p='_';
				break;
		}
	}

	index = var;
	index_len = var_len;

	while (1) {
		if (is_array) {
			char *escaped_index = NULL, *index_s;
			int new_idx_len = 0;

			if(++nest_level > PG(max_input_nesting_level)) {
				HashTable *ht;
  				/* too many levels of nesting */

				if (track_vars_array) {
					ht = Z_ARRVAL_P(track_vars_array);
				} else if (PG(register_globals)) {
					ht = EG(active_symbol_table);
				}

				zend_hash_del(ht, var, var_len + 1);
				zval_dtor(val);

				if (!PG(display_errors)) {
					php_error_docref(NULL TSRMLS_CC, E_WARNING, "Input variable nesting level exceeded %ld. To increase the limit change max_input_nesting_level in php.ini.", PG(max_input_nesting_level));
				}
				return;
			}
			ip++;
			index_s = ip;
			if (isspace(*ip)) {
				ip++;
			}
			if (*ip==']') {
				index_s = NULL;
			} else {
				ip = strchr(ip, ']');
				if (!ip) {
					/* PHP variables cannot contain '[' in their names, so we replace the character with a '_' */
					*(index_s - 1) = '_';
					
					index_len = 0;
					if (index) {
						index_len = strlen(index);
					}
					goto plain_var;
					return;
				}
				*ip = 0;
				new_idx_len = strlen(index_s);	
			}

			if (!index) {
				MAKE_STD_ZVAL(gpc_element);
				array_init(gpc_element);
				zend_hash_next_index_insert(symtable1, &gpc_element, sizeof(zval *), (void **) &gpc_element_p);
			} else {
				if (PG(magic_quotes_gpc) && (index!=var)) {
					/* no need to addslashes() the index if it's the main variable name */
					escaped_index = php_addslashes(index, index_len, &index_len, 0 TSRMLS_CC);
				} else {
					escaped_index = index;
				}
				if (zend_hash_find(symtable1, escaped_index, index_len+1, (void **) &gpc_element_p)==FAILURE
					|| Z_TYPE_PP(gpc_element_p) != IS_ARRAY) {
					MAKE_STD_ZVAL(gpc_element);
					array_init(gpc_element);
					zend_hash_update(symtable1, escaped_index, index_len+1, &gpc_element, sizeof(zval *), (void **) &gpc_element_p);
				}
				if (index!=escaped_index) {
					efree(escaped_index);
				}
			}
			symtable1 = Z_ARRVAL_PP(gpc_element_p);
			/* ip pointed to the '[' character, now obtain the key */
			index = index_s;
			index_len = new_idx_len;

			ip++;
			if (*ip=='[') {
				is_array = 1;
				*ip = 0;
			} else {
				is_array = 0;
			}
		} else {
plain_var:
			MAKE_STD_ZVAL(gpc_element);
			gpc_element->value = val->value;
			Z_TYPE_P(gpc_element) = Z_TYPE_P(val);
			if (!index) {
				zend_hash_next_index_insert(symtable1, &gpc_element, sizeof(zval *), (void **) &gpc_element_p);
			} else {
				zval **tmp;
			
				if (PG(magic_quotes_gpc) && (index!=var)) {
					char *escaped_index = php_addslashes(index, index_len, &index_len, 0 TSRMLS_CC);
					
					if (PG(http_globals)[TRACK_VARS_COOKIE] && symtable1 == Z_ARRVAL_P(PG(http_globals)[TRACK_VARS_COOKIE]) && 
							zend_hash_find(symtable1, escaped_index, index_len+1, (void **) &tmp) != FAILURE) {
						efree(escaped_index);
						zval_ptr_dtor(&gpc_element);
						break;
					}
					
					zend_hash_update(symtable1, escaped_index, index_len+1, &gpc_element, sizeof(zval *), (void **) &gpc_element_p);
					efree(escaped_index);
				} else {
					if (PG(http_globals)[TRACK_VARS_COOKIE] && symtable1 == Z_ARRVAL_P(PG(http_globals)[TRACK_VARS_COOKIE]) && 
							zend_hash_find(symtable1, index, index_len+1, (void **) &tmp) != FAILURE) {
						zval_ptr_dtor(&gpc_element);
						break;
					}
				
					zend_hash_update(symtable1, index, index_len+1, &gpc_element, sizeof(zval *), (void **) &gpc_element_p);
				}
			}
			break;
		}
	}
}


SAPI_API SAPI_POST_HANDLER_FUNC(php_std_post_handler)
{
	char *var, *val, *e, *s, *p;
	zval *array_ptr = (zval *) arg;

	if (SG(request_info).post_data==NULL) {
		return;
	}	

	s = SG(request_info).post_data;
	e = s + SG(request_info).post_data_length;

	while (s < e && (p = memchr(s, '&', (e - s)))) {
last_value:
		if ((val = memchr(s, '=', (p - s)))) { /* have a value */
			int val_len;

			var = s;

			php_url_decode(var, (val - s));
			val++;
			val_len = php_url_decode(val, (p - val));
			php_register_variable_safe(var, val, val_len, array_ptr TSRMLS_CC);
		}
		s = p + 1;
	}
	if (s < e) {
		p = e;
		goto last_value;
	}
}

SAPI_API SAPI_TREAT_DATA_FUNC(php_default_treat_data)
{
	char *res = NULL, *var, *val, *separator=NULL;
	const char *c_var;
	pval *array_ptr;
	int free_buffer=0;
	char *strtok_buf = NULL;
	
	switch (arg) {
		case PARSE_POST:
		case PARSE_GET:
		case PARSE_COOKIE:
			ALLOC_ZVAL(array_ptr);
			array_init(array_ptr);
			INIT_PZVAL(array_ptr);
			switch (arg) {
				case PARSE_POST:
					PG(http_globals)[TRACK_VARS_POST] = array_ptr;
					break;
				case PARSE_GET:
					PG(http_globals)[TRACK_VARS_GET] = array_ptr;
					break;
				case PARSE_COOKIE:
					PG(http_globals)[TRACK_VARS_COOKIE] = array_ptr;
					break;
			}
			break;
		default:
			array_ptr=destArray;
			break;
	}

	if (arg==PARSE_POST) {
		sapi_handle_post(array_ptr TSRMLS_CC);
		return;
	}

	if (arg == PARSE_GET) {		/* GET data */
		c_var = SG(request_info).query_string;
		if (c_var && *c_var) {
			res = (char *) estrdup(c_var);
			free_buffer = 1;
		} else {
			free_buffer = 0;
		}
	} else if (arg == PARSE_COOKIE) {		/* Cookie data */
		c_var = SG(request_info).cookie_data;
		if (c_var && *c_var) {
			res = (char *) estrdup(c_var);
			free_buffer = 1;
		} else {
			free_buffer = 0;
		}
	} else if (arg == PARSE_STRING) {		/* String data */
		res = str;
		free_buffer = 1;
	}

	if (!res) {
		return;
	}

	switch (arg) {
		case PARSE_GET:
		case PARSE_STRING:
			separator = (char *) estrdup(PG(arg_separator).input);
			break;
		case PARSE_COOKIE:
			separator = ";\0";
			break;
	}
	
	var = php_strtok_r(res, separator, &strtok_buf);
	
	while (var) {
		val = strchr(var, '=');
		if (val) { /* have a value */
			int val_len;

			*val++ = '\0';
			php_url_decode(var, strlen(var));
			val_len = php_url_decode(val, strlen(val));
			php_register_variable_safe(var, val, val_len, array_ptr TSRMLS_CC);
		} else {
			php_url_decode(var, strlen(var));
			php_register_variable_safe(var, "", 0, array_ptr TSRMLS_CC);
		}
		var = php_strtok_r(NULL, separator, &strtok_buf);
	}

	if(arg != PARSE_COOKIE) {
		efree(separator);
	}

	if (free_buffer) {
		efree(res);
	}
}

void _php_import_environment_variables(zval *array_ptr TSRMLS_DC)
{
	char **env, *p, *t;

	for (env = environ; env != NULL && *env != NULL; env++) {
		p = strchr(*env, '=');
		if (!p) {				/* malformed entry? */
			continue;
		}
		t = estrndup(*env, p - *env);
		php_register_variable(t, p+1, array_ptr TSRMLS_CC);
		efree(t);
	}
}


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: sw=4 ts=4 fdm=marker
 * vim<600: sw=4 ts=4
 */
