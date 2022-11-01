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
   | Author: Adam Dickmeiss <adam@indexdata.dk>                           |
   +----------------------------------------------------------------------+
 */

/* $Id: php_yaz.h,v 1.13.4.2.8.3 2007/12/31 07:22:54 sebastian Exp $ */

#ifndef PHP_YAZ_H
#define PHP_YAZ_H

#if HAVE_YAZ

#ifdef ZTS
#include "TSRM.h"
#endif

extern zend_module_entry yaz_module_entry;
#define yaz_module_ptr &yaz_module_entry

PHP_FUNCTION(yaz_connect);
PHP_FUNCTION(yaz_close);
PHP_FUNCTION(yaz_search);
PHP_FUNCTION(yaz_wait);
PHP_FUNCTION(yaz_errno);
PHP_FUNCTION(yaz_error);
PHP_FUNCTION(yaz_addinfo);
PHP_FUNCTION(yaz_hits);
PHP_FUNCTION(yaz_record);
PHP_FUNCTION(yaz_syntax);
PHP_FUNCTION(yaz_element);
PHP_FUNCTION(yaz_range);
PHP_FUNCTION(yaz_itemorder);
PHP_FUNCTION(yaz_scan);
PHP_FUNCTION(yaz_scan_result);
PHP_FUNCTION(yaz_es_result);
PHP_FUNCTION(yaz_present);
PHP_FUNCTION(yaz_ccl_conf);
PHP_FUNCTION(yaz_ccl_parse);
PHP_FUNCTION(yaz_database);
PHP_FUNCTION(yaz_sort);
PHP_FUNCTION(yaz_schema);

ZEND_BEGIN_MODULE_GLOBALS(yaz)
    int assoc_seq;
    long max_links;
    char *log_file;
ZEND_END_MODULE_GLOBALS(yaz)
    
#ifdef ZTS
#define YAZSG(v) TSRMG(yaz_globals_id, zend_yaz_globals *, v)
#else
#define YAZSG(v) (yaz_globals.v)
#endif

#else

#define yaz_module_ptr NULL
#endif

#define phpext_yaz_ptr yaz_module_ptr
#endif
