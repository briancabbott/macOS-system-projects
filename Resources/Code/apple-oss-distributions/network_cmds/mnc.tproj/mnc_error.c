/*
 * $Id: mnc_error.c,v 1.2 2004/09/22 16:02:26 colmmacc Exp $
 *
 * mnc_multicast.c -- Multicast NetCat
 *
 * Colm MacCarthaigh, <colm@apache.org>
 *
 * Copyright (c) 2007, Colm MacCarthaigh.
 * Copyright (c) 2004 - 2006, HEAnet Ltd. 
 *
 * This software is an open source.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * Neither the name of the HEAnet Ltd. nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "mnc.h"

void mnc_warning(char * string, ...)
{
	va_list ap;

	/* Do the vararg stuff */
	va_start(ap, string);
		
	/* Output our name */
	if (fprintf(stderr, "mnc: ") < 0)
	{
		exit(2);
	}

	/* Output our error */
	if (vfprintf(stderr, string, ap) < 0)
	{
		exit(2);
	}

	/* End the vararg stuff */
	va_end(ap);
}
	
void mnc_error(char * string, ...)
{
	va_list ap;

	/* Do the vararg stuff */
	va_start(ap, string);
	
	/* Output our name */
	if (fprintf(stderr, "mnc: ") < 0)
	{
		exit(2);
	}

	/* Output our error */
	if (vfprintf(stderr, string, ap) < 0)
	{
		exit(2);
	}

	/* End the vararg stuff */
	va_end(ap);

	/* Die! */
	exit(1);
}
