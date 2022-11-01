/*
 * Copyright (c) 2002-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 2002-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "libsa.h"

extern void putc(int c);

/*
 * write one character to console
 */
void putchar(int c)
{
        if ( c == '\t' )
        {
                for (c = 0; c < 8; c++) putc(' ');
                return;
        }

        if ( c == '\n' )
    {
                putc('\r');
    }

        putc(c);
}

int printf(const char * fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    prf(fmt, ap, putchar, 0);
    va_end(ap);
    return 0;
}

int verbose(const char * fmt, ...)
{  
#if DEBUG
    va_list ap;

    va_start(ap, fmt);
    prf(fmt, ap, putchar, 0);
    va_end(ap);
#endif
    return(0);
}

