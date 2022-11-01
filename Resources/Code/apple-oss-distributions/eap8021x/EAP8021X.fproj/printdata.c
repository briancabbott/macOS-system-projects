/*
 * Copyright (c) 2001-2013 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/* 
 * Modification History
 *
 * November 8, 2001	Dieter Siegmund
 * - created
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include "printdata.h"
#include "myCFUtil.h"
#include <SystemConfiguration/SCPrivate.h>

void
print_bytes_cfstr(CFMutableStringRef str, const uint8_t * data_p,
		  int n_bytes)
{
    int 		i;

    for (i = 0; i < n_bytes; i++) {
	char * space;

	if (i == 0) {
	    space = "";
	}
	else if ((i % 8) == 0) {
	    space = "  ";
	}
	else {
	    space = " ";
	}
	STRING_APPEND(str, "%s%02x", space, data_p[i]);
    }
    return;
}

void
print_data_cfstr(CFMutableStringRef str, const uint8_t * data_p,
		 int n_bytes)
{
#define CHARS_PER_LINE 	16
    char		line_buf[CHARS_PER_LINE + 1];
    int			line_pos;
    int			offset;

    for (line_pos = 0, offset = 0; offset < n_bytes; offset++, data_p++) {
	if (line_pos == 0)
	    STRING_APPEND(str, "%04x ", offset);

	line_buf[line_pos] = isprint(*data_p) ? *data_p : '.';
	STRING_APPEND(str, " %02x", *data_p);
	line_pos++;
	if (line_pos == CHARS_PER_LINE) {
	    line_buf[CHARS_PER_LINE] = '\0';
	    STRING_APPEND(str, "  %s\n", line_buf);
	    line_pos = 0;
	}
	else if (line_pos == (CHARS_PER_LINE / 2))
	    STRING_APPEND(str, " ");
    }
    if (line_pos) { /* need to finish up the line */
	char * extra_space = "";
	if (line_pos < (CHARS_PER_LINE / 2)) {
	    extra_space = " ";
	}
	for (; line_pos < CHARS_PER_LINE; line_pos++) {
	    STRING_APPEND(str, "   ");
	    line_buf[line_pos] = ' ';
	}
	line_buf[CHARS_PER_LINE] = '\0';
	STRING_APPEND(str, "  %s%s\n", extra_space, line_buf);
    }
    return;
}

void
fprint_bytes(FILE * out_f, const uint8_t * data_p, int n_bytes)
{
    CFMutableStringRef	str;

    str = CFStringCreateMutable(NULL, 0);
    if (out_f == NULL) {
	out_f = stdout;
    }
    print_bytes_cfstr(str, data_p, n_bytes);
    SCPrint(TRUE, out_f, CFSTR("%@"), str);
    CFRelease(str);
    fflush(out_f);
    return;
}

void
fprint_data(FILE * out_f, const uint8_t * data_p, int n_bytes)
{
    CFMutableStringRef	str;

    str = CFStringCreateMutable(NULL, 0);
    print_data_cfstr(str, data_p, n_bytes);
    if (out_f == NULL) {
	out_f = stdout;
    }
    SCPrint(TRUE, out_f, CFSTR("%@"), str);
    CFRelease(str);
    fflush(out_f);
    return;
}

void
print_bytes(const uint8_t * data, int len)
{
    fprint_bytes(NULL, data, len);
}

void
print_data(const uint8_t * data, int len)
{
    fprint_data(NULL, data, len);
}
