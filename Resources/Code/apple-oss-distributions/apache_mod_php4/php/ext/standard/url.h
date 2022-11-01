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
/* $Id: url.h,v 1.13.8.2.2.3 2007/12/31 07:22:53 sebastian Exp $ */

#ifndef URL_H
#define URL_H

typedef struct php_url {
	char *scheme;
	char *user;
	char *pass;
	char *host;
	unsigned short port;
	char *path;
	char *query;
	char *fragment;
} php_url;

PHPAPI void php_url_free(php_url *theurl);
PHPAPI php_url *php_url_parse(char const *str);
PHPAPI php_url *php_url_parse_ex(char const *str, int length);
PHPAPI int php_url_decode(char *str, int len); /* return value: length of decoded string */
PHPAPI int php_raw_url_decode(char *str, int len); /* return value: length of decoded string */
PHPAPI char *php_url_encode(char *s, int len, int *new_length);
PHPAPI char *php_raw_url_encode(char *s, int len, int *new_length);

PHP_FUNCTION(parse_url);
PHP_FUNCTION(urlencode);
PHP_FUNCTION(urldecode);
PHP_FUNCTION(rawurlencode);
PHP_FUNCTION(rawurldecode);

#endif /* URL_H */

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 */
