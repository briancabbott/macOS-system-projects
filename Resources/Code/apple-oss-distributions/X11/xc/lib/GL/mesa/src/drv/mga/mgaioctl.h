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
 *    Gareth Hughes <gareth@valinux.com>
 */
/* $XFree86: xc/lib/GL/mesa/src/drv/mga/mgaioctl.h,v 1.11 2002/10/30 12:51:36 alanh Exp $ */

#ifndef MGA_IOCTL_H
#define MGA_IOCTL_H

#include "mgacontext.h"
#include "mga_xmesa.h"

void mgaSwapBuffers( Display *dpy, void *drawablePrivate );

GLuint *mgaAllocVertexDwords( mgaContextPtr mmesa, int dwords );


void mgaGetILoadBufferLocked( mgaContextPtr mmesa );
drmBufPtr mgaGetBufferLocked( mgaContextPtr mmesa );


void mgaFireILoadLocked( mgaContextPtr mmesa,
			 GLuint offset, GLuint length );

void mgaWaitAgeLocked( mgaContextPtr mmesa, int age );
void mgaWaitAge( mgaContextPtr mmesa, int age );

void mgaFlushVertices( mgaContextPtr mmesa );
void mgaFlushVerticesLocked( mgaContextPtr mmesa );
void mgaReleaseBufLocked( mgaContextPtr mmesa, drmBufPtr buffer );
int mgaFlushDMA( int fd, drmLockFlags flags );

void mgaDDFlush( GLcontext *ctx );
void mgaDDFinish( GLcontext *ctx );

void mgaDDInitIoctlFuncs( GLcontext *ctx );

#define FLUSH_BATCH(mmesa) do {						\
        if (MGA_DEBUG&DEBUG_VERBOSE_IOCTL)  				\
              fprintf(stderr, "FLUSH_BATCH in %s\n", __FUNCTION__);	\
	if (mmesa->vertex_dma_buffer) mgaFlushVertices(mmesa);		\
} while (0)

#define MGA_STATECHANGE(mmesa, flag) do {	\
   FLUSH_BATCH(mmesa);				\
   mmesa->dirty |= flag;			\
} while (0)


extern drmBufPtr mga_get_buffer_ioctl( mgaContextPtr mmesa );

static __inline
GLuint *mgaAllocDmaLow( mgaContextPtr mmesa, int bytes )
{
   GLuint *head;

   if (!mmesa->vertex_dma_buffer) {
      LOCK_HARDWARE( mmesa );
      mmesa->vertex_dma_buffer = mga_get_buffer_ioctl( mmesa );
      UNLOCK_HARDWARE( mmesa );
   } else if (mmesa->vertex_dma_buffer->used + bytes >
	      mmesa->vertex_dma_buffer->total) {
      LOCK_HARDWARE( mmesa );
      mgaFlushVerticesLocked( mmesa );
      mmesa->vertex_dma_buffer = mga_get_buffer_ioctl( mmesa );
      UNLOCK_HARDWARE( mmesa );
   }

   head = (GLuint *)((char *)mmesa->vertex_dma_buffer->address +
		      mmesa->vertex_dma_buffer->used);

   mmesa->vertex_dma_buffer->used += bytes;
   return head;
}


#define UPDATE_LOCK( mmesa, flags )					\
do {									\
   GLint ret = mgaFlushDMA( mmesa->driFd, flags );			\
   if ( ret < 0 ) {							\
      drmCommandNone( mmesa->driFd, DRM_MGA_RESET );			\
      UNLOCK_HARDWARE( mmesa );						\
      fprintf( stderr, "%s: flush ret=%d\n", __FUNCTION__, ret );	\
      /*fprintf( stderr, "drmMGAFlushDMA: return = %d\n", ret );*/	\
      exit( 1 );							\
   }									\
} while (0)

#endif
