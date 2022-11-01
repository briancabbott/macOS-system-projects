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
   | Author: Jim Winstead <jimw@php.net>                                  |
   +----------------------------------------------------------------------+
 */
/* $Id: fopen_wrappers.h,v 1.38.4.2.4.3 2007/12/31 07:22:54 sebastian Exp $ */

#ifndef FOPEN_WRAPPERS_H
#define FOPEN_WRAPPERS_H

BEGIN_EXTERN_C()
#include "php_globals.h"

PHPAPI int php_fopen_primary_script(zend_file_handle *file_handle TSRMLS_DC);
PHPAPI char *expand_filepath(const char *filepath, char *real_path TSRMLS_DC);

PHPAPI int php_check_open_basedir(const char *path TSRMLS_DC);
PHPAPI int php_check_open_basedir_ex(const char *path, int warn TSRMLS_DC);
PHPAPI int php_check_specific_open_basedir(const char *basedir, const char *path TSRMLS_DC);

PHPAPI int php_check_safe_mode_include_dir(char *path TSRMLS_DC);

PHPAPI FILE *php_fopen_with_path(char *filename, char *mode, char *path, char **opened_path TSRMLS_DC);

PHPAPI int php_is_url(char *path);
PHPAPI char *php_strip_url_passwd(char *path);
END_EXTERN_C()

#endif
/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
