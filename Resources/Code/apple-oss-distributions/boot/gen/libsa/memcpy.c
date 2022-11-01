/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
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
/*
 * more string operations
 *
 */
 
#import "libsa.h"

#if defined(NX_CURRENT_COMPILER_RELEASE) && (NX_CURRENT_COMPILER_RELEASE < 320)
#define SIREG "e"
#else
#define SIREG "S"
#endif

#if i386 && !defined(DONT_USE_ASM)
/* Simple forward character copy */
void *memcpy(void *dst, const void *src, size_t len)
{
    asm("rep; movsb"
	: /* no outputs */
	: "&c" (len), "D" (dst), SIREG (src)
	: "ecx", "esi", "edi");
    return (void *)src;
}
#else
void *memcpy(
    void *dst,
    const void *src,
    size_t len
)
{
	register char *src_c, *dst_c;
	
	src_c = (char *)src;
	dst_c = (char *)dst;
	
	while (len-- > 0)
		*dst_c++ = *src_c++;
	return src;
}
#endif

/*
 * copy n bytes from src to dst. Both src and dst are virtual addresses.
 */
char *bcopy(char *src, char *dst, int n)
{
	memcpy(dst, src, n);
	return 0;
}


