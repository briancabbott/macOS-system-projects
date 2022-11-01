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
   | Author: Marcus Boerger <helly@php.net>                               |
   +----------------------------------------------------------------------+
*/

/* $Id: php_getopt.h,v 1.1.8.3.6.4 2007/12/31 07:22:55 sebastian Exp $ */

#include "php.h"

#ifdef NETWARE
/*
As NetWare LibC has optind and optarg macros defined in unistd.h our local variables were getting mistakenly preprocessed so undeffing optind and optarg
*/
#undef optarg
#undef optind
#endif
/* Define structure for one recognized option (both single char and long name).
 * If short_open is '-' this is the last option.
 */
typedef struct _opt_struct {
	const char opt_char;
	const int  need_param;
	const char * opt_name;
} opt_struct;

int php_getopt(int argc, char* const *argv, const opt_struct opts[], char **optarg, int *optind, int show_err);
