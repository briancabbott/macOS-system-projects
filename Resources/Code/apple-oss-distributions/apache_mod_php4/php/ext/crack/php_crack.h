/*
   +----------------------------------------------------------------------+
   | PHP Version 4                                                        |
   +----------------------------------------------------------------------+
   | Copyright (c) 1997, 1998, 1999, 2000, 2001 The PHP Group             |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Authors: Alexander Feldman                                           |
   +----------------------------------------------------------------------+
 */

/* $Id: php_crack.h,v 1.5.20.1 2006/01/01 13:46:50 sniper Exp $ */

#ifndef ZEND_CRACK_H
#define ZEND_CRACK_H

#if HAVE_CRACK

extern zend_module_entry crack_module_entry;
#define phpext_crack_ptr &crack_module_entry

#ifdef ZEND_WIN32
#define ZEND_CRACK_API __declspec(dllexport)
#else
#define ZEND_CRACK_API
#endif

ZEND_MINIT_FUNCTION(crack);
ZEND_MSHUTDOWN_FUNCTION(crack);
ZEND_RINIT_FUNCTION(crack);
ZEND_RSHUTDOWN_FUNCTION(crack);
PHP_MINFO_FUNCTION(crack);

ZEND_FUNCTION(crack_opendict);
ZEND_FUNCTION(crack_closedict);
ZEND_FUNCTION(crack_check);
ZEND_FUNCTION(crack_getlastmessage);

ZEND_BEGIN_MODULE_GLOBALS(crack)
	char *default_dictionary;
	char *last_message;
	long current_id;
ZEND_END_MODULE_GLOBALS(crack)

#ifdef ZTS
#define CRACKG(v) TSRMG(crack_globals_id, zend_crack_globals *, v)
#else
#define CRACKG(v) (crack_globals.v)
#endif

#else

#define phpext_crack_ptr NULL

#endif

#endif	/* ZEND_CRACK_H */


/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
