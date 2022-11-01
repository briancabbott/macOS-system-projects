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
   | Author: Edin Kadribasic <edink@php.net>                              |
   +----------------------------------------------------------------------+
*/
/* $Id: php_embed.h,v 1.1.2.2.8.3 2007/12/31 07:22:55 sebastian Exp $ */

#ifndef _PHP_EMBED_H_
#define _PHP_EMBED_H_

#include <main/php.h>
#include <main/SAPI.h>
#include <main/php_main.h>
#include <main/php_variables.h>
#include <main/php_ini.h>
#include <zend_ini.h>

#ifdef ZTS
#define PTSRMLS_D        void ****ptsrm_ls
#define PTSRMLS_DC       , PTSRMLS_D
#define PTSRMLS_C        &tsrm_ls
#define PTSRMLS_CC       , PTSRMLS_C
#else
#define PTSRMLS_D
#define PTSRMLS_DC
#define PTSRMLS_C
#define PTSRMLS_CC
#endif

#define PHP_EMBED_START_BLOCK(x,y) { \
    void ***tsrm_ls; \
    php_embed_init(x, y PTSRMLS_CC); \
    zend_first_try {

#define PHP_EMBED_END_BLOCK() \
  } zend_catch { \
    /* int exit_status = EG(exit_status); */ \
  } zend_end_try(); \
  php_embed_shutdown(TSRMLS_C); \
}

BEGIN_EXTERN_C() 
int php_embed_init(int argc, char **argv PTSRMLS_DC);
void php_embed_shutdown(TSRMLS_D);
extern sapi_module_struct php_embed_module;
END_EXTERN_C()


#endif /* _PHP_EMBED_H_ */
