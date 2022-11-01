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
   | If you did not receive a copy of the PHP license and are unable to	  |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
   | Author: Kristian Koehntopp <kris@koehntopp.de>                       |
   +----------------------------------------------------------------------+
*/

/* $Id: php_recode.h,v 1.13.4.1.8.3 2007/12/31 07:22:51 sebastian Exp $ */

#ifndef PHP_RECODE_H
#define PHP_RECODE_H

#if HAVE_LIBRECODE

extern zend_module_entry recode_module_entry;
#define phpext_recode_ptr &recode_module_entry

PHP_MINIT_FUNCTION(recode);
PHP_MSHUTDOWN_FUNCTION(recode);
PHP_MINFO_FUNCTION(recode);
PHP_FUNCTION(recode_string);
PHP_FUNCTION(recode_file);

#else
#define phpext_recode_ptr NULL
#endif
	
#endif /* PHP_RECODE_H */
