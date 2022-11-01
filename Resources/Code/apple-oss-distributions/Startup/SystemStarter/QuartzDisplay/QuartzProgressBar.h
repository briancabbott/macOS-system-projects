/**
 * QuartzProgressBar.h - Show Boot Status
 * Wilfredo Sanchez | wsanchez@opensource.apple.com
 * $Apple$
 **
 * Copyright (c) 1999-2001 Apple Computer, Inc. All rights reserved.
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
 * Draws the progress bar using CoreGraphics (Quartz).
 **/

#ifndef _QuartzProgressBar_H_
#define _QuartzProgressBar_H_

#include <CoreGraphics/CGContext.h>

#ifndef _QuartzProgressBar_C_
typedef void* ProgressBarRef;
#endif

ProgressBarRef ProgressBarCreate (CGContextRef aContext, float x, float y, float w);

void  ProgressBarDisplay    (ProgressBarRef aProgressBar);
void  ProgressBarSetPercent (ProgressBarRef aProgressBar, float aPercent);

void ProgressBarFree (ProgressBarRef aProgressBar);

#endif /* _QuartzProgressBar_H_ */
