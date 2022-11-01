/**
 * StartupDisplay.h - Show Boot Status
 * Wilfredo Sanchez | wsanchez@opensource.apple.com
 * $Apple$
 **
 * Copyright (c) 1999-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 **
 * Display plug-ins are expected to provide this API.
 * Draws the startup screen, progress bar, and status text.
 **/

#ifndef _StartupDisplay_H_
#define _StartupDisplay_H_

#include <CoreFoundation/CFString.h>

/*
 * DisplayContext is an opaque data type outside of the actual
 * implementation code. It is a pointer to implementation-specific
 * state storage.
 */
#ifndef _StartupDisplay_C_
typedef void* DisplayContext;
#endif

/* Initialize the context store. */
DisplayContext initDisplayContext();

/* Free the context store. */
void freeDisplayContext (DisplayContext aContext);

/* Update the user-visible progress. */
int displayStatus (DisplayContext aContext, CFStringRef aMessage);
int displayProgress (DisplayContext aContext, float aPercentage);
int displaySafeBootMsg (DisplayContext aDisplayContext, CFStringRef aMessage);

#endif /* _StartupDisplay_H_ */
