/*
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997-2008 The PHP Group                                |
   +----------------------------------------------------------------------+
   | dbx module version 1.0                                               |
   +----------------------------------------------------------------------+
   | Copyright (c) 2001 Guidance Rotterdam BV                             |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author : Marc Boeren         <marc@guidance.nl>                      |
   +----------------------------------------------------------------------+
*/

/* $Id: dbx_oci8.h,v 1.5.2.1.8.3 2007/12/31 07:22:46 sebastian Exp $ */

#ifndef ZEND_DBX_OCI8_H
#define ZEND_DBX_OCI8_H

#ifndef INIT_FUNC_ARGS
#include "zend_modules.h"
#endif

#include "php.h"

int dbx_oci8_connect(zval **rv, zval **host, zval **db, zval **username, zval **password, INTERNAL_FUNCTION_PARAMETERS);
	/* returns connection handle as resource on success or 0 as long on failure */
int dbx_oci8_pconnect(zval **rv, zval **host, zval **db, zval **username, zval **password, INTERNAL_FUNCTION_PARAMETERS);
	/* returns persistent connection handle as resource on success or 0 as long on failure */
int dbx_oci8_close(zval **rv, zval **dbx_handle, INTERNAL_FUNCTION_PARAMETERS);
	/* returns 1 as long on success or 0 as long on failure */
int dbx_oci8_query(zval **rv, zval **dbx_handle, zval **db_name, zval **sql_statement, INTERNAL_FUNCTION_PARAMETERS);
	/* returns 1 as long or a result identifier as resource on success  or 0 as long on failure */
int dbx_oci8_getcolumncount(zval **rv, zval **result_handle, INTERNAL_FUNCTION_PARAMETERS);
	/* returns column-count as long on success or 0 as long on failure */
int dbx_oci8_getcolumnname(zval **rv, zval **result_handle, long column_index, INTERNAL_FUNCTION_PARAMETERS);
	/* returns column-name as string on success or 0 as long on failure */
int dbx_oci8_getcolumntype(zval **rv, zval **result_handle, long column_index, INTERNAL_FUNCTION_PARAMETERS);
	/* returns column-type as string on success or 0 as long on failure */
int dbx_oci8_getrow(zval **rv, zval **result_handle, long row_number, INTERNAL_FUNCTION_PARAMETERS);
	/* returns array[0..columncount-1] as strings on success or 0 as long on failure */
int dbx_oci8_error(zval **rv, zval **dbx_handle, INTERNAL_FUNCTION_PARAMETERS);
	/* returns string */
int dbx_oci8_esc(zval **rv, zval **dbx_handle, zval **string, INTERNAL_FUNCTION_PARAMETERS);
	/* returns escaped string */

#endif	/* ZEND_DBX_OCI8_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
