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
   | Author: Rasmus Lerdorf <rasmus@lerdorf.on.ca>                        |
   +----------------------------------------------------------------------+
*/

/* $Id: html.h,v 1.15.2.1.8.3 2007/12/31 07:22:52 sebastian Exp $ */

#ifndef HTML_H
#define HTML_H

#define ENT_HTML_QUOTE_NONE		0
#define ENT_HTML_QUOTE_SINGLE	1
#define ENT_HTML_QUOTE_DOUBLE	2

#define ENT_COMPAT    ENT_HTML_QUOTE_DOUBLE
#define ENT_QUOTES    (ENT_HTML_QUOTE_DOUBLE | ENT_HTML_QUOTE_SINGLE)
#define ENT_NOQUOTES  ENT_HTML_QUOTE_NONE

void register_html_constants(INIT_FUNC_ARGS);

PHP_FUNCTION(htmlspecialchars);
PHP_FUNCTION(htmlentities);
PHP_FUNCTION(html_entity_decode);
PHP_FUNCTION(get_html_translation_table);

PHPAPI char *php_escape_html_entities(unsigned char *old, int oldlen, int *newlen, int all, int quote_style, char *hint_charset TSRMLS_DC);

#endif /* HTML_H */
