/*
 * Copyright (c) 1988 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

/* based on @(#)vfprintf.c	5.2 (Berkeley) 6/27/88 */

#include <stdio.h>
#if defined(HAVE_STDARG_H) || defined(_WIN32)
#include <stdarg.h>
#else
#include <varargs.h>
#define VARARGS
#endif

int
vfprintf(iop, fmt, ap)
	FILE *iop;
	char *fmt;
	va_list ap;
{
	int len;
	char localbuf[BUFSIZ];

	if (iop->_flag & _IONBF) {
		iop->_flag &= ~_IONBF;
		iop->_ptr = iop->_base = localbuf;
		len = _doprnt(fmt, ap, iop);
		(void) fflush(iop);
		iop->_flag |= _IONBF;
		iop->_base = NULL;
		iop->_bufsiz = 0;
		iop->_cnt = 0;
	} else
		len = _doprnt(fmt, ap, iop);

	return (ferror(iop) ? EOF : len);
}
