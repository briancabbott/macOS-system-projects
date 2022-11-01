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
   | Author: Sterling Hughes <sterling@php.net>                           |
   +----------------------------------------------------------------------+
 */

/* $Id: php_cyrus.h,v 1.3.8.1.8.3 2007/12/31 07:22:45 sebastian Exp $ */

#ifndef PHP_CYRUS_H
#define PHP_CYRUS_H

#include "php.h"

#if HAVE_CYRUS

#include <cyrus/imclient.h>

extern zend_module_entry cyrus_module_entry;
#define phpext_cyrus_ptr &cyrus_module_entry

#ifdef PHP_WIN32
#define PHP_CYRUS_API __declspec(dllexport)
#else
#define PHP_CYRUS_API
#endif

PHP_MINIT_FUNCTION(cyrus);
PHP_MINFO_FUNCTION(cyrus);

PHP_FUNCTION(cyrus_connect);
PHP_FUNCTION(cyrus_authenticate);
PHP_FUNCTION(cyrus_bind);
PHP_FUNCTION(cyrus_unbind);
PHP_FUNCTION(cyrus_query);
PHP_FUNCTION(cyrus_close);

typedef struct {
	struct imclient *client;
	char            *host;
	char            *port;
	int              flags;
	int              id;
}
php_cyrus;

typedef struct {
	zval *function;
	char *trigger;
	long  le;
	int   flags;
}
php_cyrus_callback;

#endif


#endif


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 */
