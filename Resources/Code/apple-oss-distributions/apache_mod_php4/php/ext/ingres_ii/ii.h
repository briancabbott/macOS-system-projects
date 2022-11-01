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
   | Contributed by ECL IP'S Software & Services                          |
   |                http://www.eclips-software.com                        |
   |                mailto://idev@eclips-software.com                     |
   | Author: David H�not <henot@php.net>                                  |
   +----------------------------------------------------------------------+
 */

/* $Id: ii.h,v 1.11.8.2.8.3 2007/12/31 07:22:48 sebastian Exp $ */

#ifndef II_H
#define II_H

#if HAVE_II
#include "php_ii.h"
#include "iiapi.h"

typedef struct _II_LINK {
	int autocommit;
	II_PTR connHandle;
	II_PTR tranHandle;
	II_PTR stmtHandle;
	II_LONG fieldCount;
	IIAPI_DESCRIPTOR *descriptor;
} II_LINK;

static int ii_sync(IIAPI_GENPARM *genParm);
static int ii_success(IIAPI_GENPARM *genParm);
#define II_FAIL 0
#define II_OK 1
#define II_NO_DATA 2
static int _close_statement(II_LINK *link);
static int _rollback_transaction(II_LINK *link TSRMLS_DC);
static void _close_ii_link(II_LINK *link TSRMLS_DC);
static void _close_ii_plink(zend_rsrc_list_entry *link TSRMLS_DC);
static int php_ii_get_default_link(INTERNAL_FUNCTION_PARAMETERS);
static void php_ii_set_default_link(int id TSRMLS_DC);
static void php_ii_do_connect(INTERNAL_FUNCTION_PARAMETERS, int persistent);
static char *php_ii_field_name(II_LINK *ii_link, int index TSRMLS_DC);
static void php_ii_field_info(INTERNAL_FUNCTION_PARAMETERS, int info_type);
static void php_ii_fetch(INTERNAL_FUNCTION_PARAMETERS, II_LINK *ii_link, int result_type);

#endif  /* HAVE_II */
#endif	/* II_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
