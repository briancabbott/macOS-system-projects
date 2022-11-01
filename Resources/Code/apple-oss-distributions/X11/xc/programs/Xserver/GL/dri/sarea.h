/* $XFree86: xc/programs/Xserver/GL/dri/sarea.h,v 1.11 2002/10/30 12:52:03 alanh Exp $ */
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
Copyright 2000 VA Linux Systems, Inc.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Kevin E. Martin <kevin@precisioninsight.com>
 *   Jens Owen <jens@tungstengraphics.com>
 *   Rickard E. (Rik) Faith <faith@valinux.com>
 *
 */

#ifndef _SAREA_H_
#define _SAREA_H_

#include "xf86drm.h"

/* SAREA area needs to be at least a page */
#if defined(__alpha__)
#define SAREA_MAX 			0x2000
#elif defined(__ia64__)
#define SAREA_MAX			0x10000		/* 64kB */
#else
/* Intel 830M driver needs at least 8k SAREA */
#define SAREA_MAX			0x2000
#endif

#define SAREA_MAX_DRAWABLES 		256

#define SAREA_DRAWABLE_CLAIMED_ENTRY	0x80000000

typedef struct _XF86DRISAREADrawable {
    unsigned int	stamp;
    unsigned int	flags;
} XF86DRISAREADrawableRec, *XF86DRISAREADrawablePtr;

typedef struct _XF86DRISAREAFrame {
    unsigned int        x;
    unsigned int        y;
    unsigned int        width;
    unsigned int        height;
    unsigned int        fullscreen;
} XF86DRISAREAFrameRec, *XF86DRISAREAFramePtr;

typedef struct _XF86DRISAREA {
    /* first thing is always the drm locking structure */
    drmLock			lock;
		/* NOT_DONE: Use readers/writer lock for drawable_lock */
    drmLock			drawable_lock;
    XF86DRISAREADrawableRec	drawableTable[SAREA_MAX_DRAWABLES];
    XF86DRISAREAFrameRec        frame;
    drmContext			dummy_context;
} XF86DRISAREARec, *XF86DRISAREAPtr;

#endif
