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
   | Author:                                                              |
   +----------------------------------------------------------------------+
 */

/* $Id: crypt_win32.h,v 1.3.6.1.8.3 2007/12/31 07:22:56 sebastian Exp $ */

/* This code is distributed under the PHP license with permission from
   the author Jochen Obalek <jochen.obalek@bigfoot.de> */

/* encrypt.h - API to 56 bit DES encryption via  calls
               encrypt(3), setkey(3) and crypt(3)
   Copyright (C) 1991 Jochen Obalek

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef _ENCRYPT_H_
#define _ENCRYPT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <_ansi.h>

void _EXFUN(encrypt, (char *block, int edflag));
void _EXFUN(setkey, (char *key));
char * _EXFUN(crypt, (const char *key, const char *salt));

#ifdef __cplusplus
}
#endif

#endif /* _ENCRYPT_H_ */
