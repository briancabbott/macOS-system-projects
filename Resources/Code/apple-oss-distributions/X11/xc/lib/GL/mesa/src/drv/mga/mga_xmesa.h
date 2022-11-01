/*
 * Copyright 2000-2001 VA Linux Systems, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.  IN NO EVENT SHALL
 * VA LINUX SYSTEMS AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Keith Whitwell <keith@tungstengraphics.com>
 */
/* $XFree86: xc/lib/GL/mesa/src/drv/mga/mga_xmesa.h,v 1.12 2002/12/16 16:18:52 dawes Exp $ */

#ifndef _MGA_INIT_H_
#define _MGA_INIT_H_

#ifdef GLX_DIRECT_RENDERING

#include <sys/time.h>
#include "dri_util.h"
#include "mtypes.h"
#include "mgaregs.h"
#include "mga_common.h"

typedef struct mga_screen_private_s {

   int chipset;
   int width;
   int height;
   int mem;

   int cpp;			/* for front and back buffers */
   GLint agpMode;
   unsigned int irq;		/* IRQ number (0 means none) */

   unsigned int mAccess;

   unsigned int frontOffset;
   unsigned int frontPitch;
   unsigned int backOffset;
   unsigned int backPitch;

   unsigned int depthOffset;
   unsigned int depthPitch;
   int depthCpp;

   unsigned int dmaOffset;

   unsigned int textureOffset[DRM_MGA_NR_TEX_HEAPS];
   unsigned int textureSize[DRM_MGA_NR_TEX_HEAPS];
   int logTextureGranularity[DRM_MGA_NR_TEX_HEAPS];
   char *texVirtual[DRM_MGA_NR_TEX_HEAPS];


   __DRIscreenPrivate *sPriv;
   drmBufMapPtr  bufs;

   drmRegion mmio;
   drmRegion status;
   drmRegion primary;
   drmRegion buffers;
   unsigned int sarea_priv_offset;
} mgaScreenPrivate;


#include "mgacontext.h"

extern void mgaGetLock( mgaContextPtr mmesa, GLuint flags );
extern void mgaEmitHwStateLocked( mgaContextPtr mmesa );
extern void mgaEmitScissorValues( mgaContextPtr mmesa, int box_nr, int emit );

#define GET_DISPATCH_AGE( mmesa ) mmesa->sarea->last_dispatch
#define GET_ENQUEUE_AGE( mmesa ) mmesa->sarea->last_enqueue



/* Lock the hardware and validate our state.
 */
#define LOCK_HARDWARE( mmesa )					\
  do {								\
    char __ret=0;						\
    DRM_CAS(mmesa->driHwLock, mmesa->hHWContext,		\
	    (DRM_LOCK_HELD|mmesa->hHWContext), __ret);		\
    if (__ret)							\
        mgaGetLock( mmesa, 0 );					\
  } while (0)


/*
 */
#define LOCK_HARDWARE_QUIESCENT( mmesa ) do {	                        \
	LOCK_HARDWARE( mmesa );			                        \
	UPDATE_LOCK( mmesa, DRM_LOCK_QUIESCENT | DRM_LOCK_FLUSH );	\
} while (0)


/* Unlock the hardware using the global current context
 */
#define UNLOCK_HARDWARE(mmesa) 				\
    DRM_UNLOCK(mmesa->driFd, mmesa->driHwLock, mmesa->hHWContext);


/* Freshen our snapshot of the drawables
 */
#define REFRESH_DRAWABLE_INFO( mmesa )		\
do {						\
   LOCK_HARDWARE( mmesa );			\
   mmesa->lastX = mmesa->drawX; 		\
   mmesa->lastY = mmesa->drawY; 		\
   UNLOCK_HARDWARE( mmesa );			\
} while (0)


#define GET_DRAWABLE_LOCK( mmesa ) while(0)
#define RELEASE_DRAWABLE_LOCK( mmesa ) while(0)


/* The 2D driver macros are busted -- we can't use them here as they
 * rely on the 2D driver data structures rather than taking an explicit
 * base address.
 */
#define MGA_BASE( reg )		((unsigned long)(mmesa->mgaScreen->mmio.map))
#define MGA_ADDR( reg )		(MGA_BASE(reg) + reg)

#define MGA_DEREF( reg )	*(volatile CARD32 *)MGA_ADDR( reg )
#define MGA_READ( reg )		MGA_DEREF( reg )
#define MGA_WRITE( reg, val )	do { MGA_DEREF( reg ) = val; } while (0)

#endif
#endif
