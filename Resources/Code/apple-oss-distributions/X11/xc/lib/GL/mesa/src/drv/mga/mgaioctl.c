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
/* $XFree86: xc/lib/GL/mesa/src/drv/mga/mgaioctl.c,v 1.16 2002/12/16 16:18:52 dawes Exp $ */

#include <stdio.h>

#include "mtypes.h"
#include "macros.h"
#include "dd.h"
#include "swrast/swrast.h"

#include "mm.h"
#include "mgacontext.h"
#include "mgadd.h"
#include "mgastate.h"
#include "mgatex.h"
#include "mgavb.h"
#include "mgaioctl.h"
#include "mgatris.h"
#include "mgabuffers.h"


#include "xf86drm.h"
#include "mga_common.h"

static void mga_iload_dma_ioctl(mgaContextPtr mmesa,
				unsigned long dest,
				int length)
{
   drmBufPtr buf = mmesa->iload_buffer;
   drmMGAIload iload;
   int ret, i;

   if (MGA_DEBUG&DEBUG_VERBOSE_IOCTL)
      fprintf(stderr, "DRM_IOCTL_MGA_ILOAD idx %d dst %x length %d\n",
	      buf->idx, (int) dest, length);

   iload.idx = buf->idx;
   iload.dstorg = dest;
   iload.length = length;

   i = 0;
   do {
      ret = drmCommandWrite( mmesa->driFd, DRM_MGA_ILOAD, 
                             &iload, sizeof(drmMGAIload) );
   } while ( ret == -EBUSY && i++ < DRM_MGA_IDLE_RETRY );

   if ( ret < 0 ) {
      printf("send iload retcode = %d\n", ret);
      exit(1);
   }

   mmesa->iload_buffer = 0;

   if (MGA_DEBUG&DEBUG_VERBOSE_IOCTL)
      fprintf(stderr, "finished iload dma put\n");

}

drmBufPtr mga_get_buffer_ioctl( mgaContextPtr mmesa )
{
   int idx = 0;
   int size = 0;
   drmDMAReq dma;
   int retcode;
   drmBufPtr buf;

   if (MGA_DEBUG&DEBUG_VERBOSE_IOCTL)
      fprintf(stderr,  "Getting dma buffer\n");

   dma.context = mmesa->hHWContext;
   dma.send_count = 0;
   dma.send_list = NULL;
   dma.send_sizes = NULL;
   dma.flags = 0;
   dma.request_count = 1;
   dma.request_size = MGA_BUFFER_SIZE;
   dma.request_list = &idx;
   dma.request_sizes = &size;
   dma.granted_count = 0;


   if (MGA_DEBUG&DEBUG_VERBOSE_IOCTL)
      fprintf(stderr, "drmDMA (get) ctx %d count %d size 0x%x\n",
	   dma.context, dma.request_count,
	   dma.request_size);

   while (1) {
      retcode = drmDMA(mmesa->driFd, &dma);

      if (MGA_DEBUG&DEBUG_VERBOSE_IOCTL)
	 fprintf(stderr, "retcode %d sz %d idx %d count %d\n",
		 retcode,
		 dma.request_sizes[0],
		 dma.request_list[0],
		 dma.granted_count);

      if (retcode == 0 &&
	  dma.request_sizes[0] &&
	  dma.granted_count)
	 break;

      if (MGA_DEBUG&DEBUG_VERBOSE_IOCTL)
	 fprintf(stderr, "\n\nflush");

      UPDATE_LOCK( mmesa, DRM_LOCK_FLUSH | DRM_LOCK_QUIESCENT );
   }

   buf = &(mmesa->mgaScreen->bufs->list[idx]);
   buf->used = 0;

   if (MGA_DEBUG&DEBUG_VERBOSE_IOCTL)
      fprintf(stderr,
	   "drmDMA (get) returns size[0] 0x%x idx[0] %d\n"
	   "dma_buffer now: buf idx: %d size: %d used: %d addr %p\n",
	   dma.request_sizes[0], dma.request_list[0],
	   buf->idx, buf->total,
	   buf->used, buf->address);

   if (MGA_DEBUG&DEBUG_VERBOSE_IOCTL)
      fprintf(stderr, "finished getbuffer\n");

   return buf;
}




static void
mgaDDClear( GLcontext *ctx, GLbitfield mask, GLboolean all,
            GLint cx, GLint cy, GLint cw, GLint ch )
{
   mgaContextPtr mmesa = MGA_CONTEXT(ctx);
   __DRIdrawablePrivate *dPriv = mmesa->driDrawable;
   GLuint flags = 0;
   GLuint clear_color = mmesa->ClearColor;
   GLuint clear_depth = 0;
   GLuint color_mask = 0;
   GLuint depth_mask = 0;
   int ret;
   int i;
   static int nrclears;
   drmMGAClearRec clear;

   FLUSH_BATCH( mmesa );

   if ( mask & DD_FRONT_LEFT_BIT ) {
      flags |= MGA_FRONT;
      color_mask = mmesa->setup.plnwt;
      mask &= ~DD_FRONT_LEFT_BIT;
   }

   if ( mask & DD_BACK_LEFT_BIT ) {
      flags |= MGA_BACK;
      color_mask = mmesa->setup.plnwt;
      mask &= ~DD_BACK_LEFT_BIT;
   }

   if ( (mask & DD_DEPTH_BIT) && ctx->Depth.Mask ) {
      flags |= MGA_DEPTH;
      clear_depth = (mmesa->ClearDepth & mmesa->depth_clear_mask);
      depth_mask |= mmesa->depth_clear_mask;
      mask &= ~DD_DEPTH_BIT;
   }

   if ( (mask & DD_STENCIL_BIT) && mmesa->hw_stencil ) {
      flags |= MGA_DEPTH;
      clear_depth |= (ctx->Stencil.Clear & mmesa->stencil_clear_mask);
      depth_mask |= mmesa->stencil_clear_mask;
      mask &= ~DD_STENCIL_BIT;
   }

   if ( flags ) {
      LOCK_HARDWARE( mmesa );

      if ( mmesa->dirty_cliprects )
	 mgaUpdateRects( mmesa, (MGA_FRONT | MGA_BACK) );

      /* flip top to bottom */
      cy = dPriv->h-cy-ch;
      cx += mmesa->drawX;
      cy += mmesa->drawY;

      if ( MGA_DEBUG & DEBUG_VERBOSE_IOCTL )
	 fprintf( stderr, "Clear, bufs %x nbox %d\n",
		  (int)flags, (int)mmesa->numClipRects );

      for (i = 0 ; i < mmesa->numClipRects ; )
      {
	 int nr = MIN2(i + MGA_NR_SAREA_CLIPRECTS, mmesa->numClipRects);
	 XF86DRIClipRectPtr box = mmesa->pClipRects;
	 XF86DRIClipRectPtr b = mmesa->sarea->boxes;
	 int n = 0;

	 if (!all) {
	    for ( ; i < nr ; i++) {
	       GLint x = box[i].x1;
	       GLint y = box[i].y1;
	       GLint w = box[i].x2 - x;
	       GLint h = box[i].y2 - y;

	       if (x < cx) w -= cx - x, x = cx;
	       if (y < cy) h -= cy - y, y = cy;
	       if (x + w > cx + cw) w = cx + cw - x;
	       if (y + h > cy + ch) h = cy + ch - y;
	       if (w <= 0) continue;
	       if (h <= 0) continue;

	       b->x1 = x;
	       b->y1 = y;
	       b->x2 = x + w;
	       b->y2 = y + h;
	       b++;
	       n++;
	    }
	 } else {
	    for ( ; i < nr ; i++) {
	       *b++ = *(XF86DRIClipRectPtr)&box[i];
	       n++;
	    }
	 }


	 if ( MGA_DEBUG & DEBUG_VERBOSE_IOCTL )
	    fprintf( stderr,
		     "DRM_IOCTL_MGA_CLEAR flag 0x%x color %x depth %x nbox %d\n",
		     flags, clear_color, clear_depth, mmesa->sarea->nbox );

	 mmesa->sarea->nbox = n;

         clear.flags = flags;
         clear.clear_color = clear_color;
         clear.clear_depth = clear_depth;
         clear.color_mask = color_mask;
         clear.depth_mask = depth_mask;
         ret = drmCommandWrite( mmesa->driFd, DRM_MGA_CLEAR,
                                 &clear, sizeof(drmMGAClearRec));
	 if ( ret ) {
	    fprintf( stderr, "send clear retcode = %d\n", ret );
	    exit( 1 );
	 }
	 if ( MGA_DEBUG & DEBUG_VERBOSE_IOCTL )
	    fprintf( stderr, "finished clear %d\n", ++nrclears );
      }

      UNLOCK_HARDWARE( mmesa );
      mmesa->dirty |= MGA_UPLOAD_CLIPRECTS|MGA_UPLOAD_CONTEXT;
   }

   if (mask) 
      _swrast_Clear( ctx, mask, all, cx, cy, cw, ch );
}


int nrswaps;


void mgaWaitForVBlank( mgaContextPtr mmesa )
{
    drmVBlank vbl;
    int ret;

    if ( !mmesa->mgaScreen->irq )
	return;

    if ( getenv("LIBGL_SYNC_REFRESH") ) {
	/* Wait for until the next vertical blank */
	vbl.request.type = DRM_VBLANK_RELATIVE;
	vbl.request.sequence = 1;
    } else if ( getenv("LIBGL_THROTTLE_REFRESH") ) {
	/* Wait for at least one vertical blank since the last call */
	vbl.request.type = DRM_VBLANK_ABSOLUTE;
	vbl.request.sequence = mmesa->vbl_seq + 1;
    } else {
	return;
    }

    if ((ret = drmWaitVBlank( mmesa->driFd, &vbl ))) {
	fprintf(stderr, "%s: drmWaitVBlank returned %d, IRQs don't seem to be"
		" working correctly.\nTry running with LIBGL_THROTTLE_REFRESH"
		" and LIBL_SYNC_REFRESH unset.\n", __FUNCTION__, ret);
	exit(1);
    }

    mmesa->vbl_seq = vbl.reply.sequence;
}


/*
 * Copy the back buffer to the front buffer.
 */
void mgaSwapBuffers(Display *dpy, void *drawablePrivate)
{
   __DRIdrawablePrivate *dPriv = (__DRIdrawablePrivate *) drawablePrivate;
   mgaContextPtr mmesa;
   XF86DRIClipRectPtr pbox;
   GLint nbox;
   GLint ret, wait = 0;
   GLint i;
   GLuint last_frame, last_wrap;

   assert(dPriv);
   assert(dPriv->driContextPriv);
   assert(dPriv->driContextPriv->driverPrivate);

   mmesa = (mgaContextPtr) dPriv->driContextPriv->driverPrivate;

   FLUSH_BATCH( mmesa );

   mgaWaitForVBlank( mmesa );

   LOCK_HARDWARE( mmesa );

   last_frame = mmesa->sarea->last_frame.head;
   last_wrap = mmesa->sarea->last_frame.wrap;

   /* FIXME: Add a timeout to this loop...
    */
   while ( 1 ) {
      if ( last_wrap < mmesa->sarea->last_wrap ||
	   ( last_wrap == mmesa->sarea->last_wrap &&
	     last_frame <= (MGA_READ( MGAREG_PRIMADDRESS ) -
			    mmesa->primary_offset) ) ) {
	 break;
      }
      if ( 0 ) {
	 wait++;
	 fprintf( stderr, "   last: head=0x%06x wrap=%d\n",
		  last_frame, last_wrap );
	 fprintf( stderr, "   head: head=0x%06lx wrap=%d\n",
		  (long)(MGA_READ( MGAREG_PRIMADDRESS ) - mmesa->primary_offset),
		  mmesa->sarea->last_wrap );
      }
      UPDATE_LOCK( mmesa, DRM_LOCK_FLUSH );

      for ( i = 0 ; i < 1024 ; i++ ) {
	 /* Don't just hammer the register... */
      }
   }
   if ( wait )
      fprintf( stderr, "\n" );

   /* Use the frontbuffer cliprects
    */
   if (mmesa->dirty_cliprects & MGA_FRONT)
      mgaUpdateRects( mmesa, MGA_FRONT );


   pbox = dPriv->pClipRects;
   nbox = dPriv->numClipRects;

   for (i = 0 ; i < nbox ; )
   {
      int nr = MIN2(i + MGA_NR_SAREA_CLIPRECTS, dPriv->numClipRects);
      XF86DRIClipRectPtr b = mmesa->sarea->boxes;

      mmesa->sarea->nbox = nr - i;

      for ( ; i < nr ; i++)
	 *b++ = pbox[i];

      if (0)
	 fprintf(stderr, "DRM_IOCTL_MGA_SWAP\n");

      ret = drmCommandNone( mmesa->driFd, DRM_MGA_SWAP );
      if ( ret ) {
	 printf("send swap retcode = %d\n", ret);
	 exit(1);
      }
   }

   UNLOCK_HARDWARE( mmesa );

   mmesa->dirty |= MGA_UPLOAD_CLIPRECTS;
}


/* This is overkill
 */
void mgaDDFinish( GLcontext *ctx  )
{
   mgaContextPtr mmesa = MGA_CONTEXT(ctx);

   FLUSH_BATCH( mmesa );

   if (1/*mmesa->sarea->last_quiescent != mmesa->sarea->last_enqueue*/) {
      if (MGA_DEBUG&DEBUG_VERBOSE_IOCTL)
	 fprintf(stderr, "mgaRegetLockQuiescent\n");

      LOCK_HARDWARE( mmesa );
      UPDATE_LOCK( mmesa, DRM_LOCK_QUIESCENT | DRM_LOCK_FLUSH );
      UNLOCK_HARDWARE( mmesa );

      mmesa->sarea->last_quiescent = mmesa->sarea->last_enqueue;
   }
}

void mgaWaitAgeLocked( mgaContextPtr mmesa, int age  )
{
   if (GET_DISPATCH_AGE(mmesa) < age) {
      UPDATE_LOCK( mmesa, DRM_LOCK_FLUSH );
   }
}


void mgaWaitAge( mgaContextPtr mmesa, int age  )
{
   if (GET_DISPATCH_AGE(mmesa) < age) {
      LOCK_HARDWARE(mmesa);
      if (GET_DISPATCH_AGE(mmesa) < age) {
	 UPDATE_LOCK( mmesa, DRM_LOCK_FLUSH );
      }
      UNLOCK_HARDWARE(mmesa);
   }
}


static int intersect_rect( XF86DRIClipRectPtr out,
			   XF86DRIClipRectPtr a,
			   XF86DRIClipRectPtr b )
{
   *out = *a;
   if (b->x1 > out->x1) out->x1 = b->x1;
   if (b->y1 > out->y1) out->y1 = b->y1;
   if (b->x2 < out->x2) out->x2 = b->x2;
   if (b->y2 < out->y2) out->y2 = b->y2;
   if (out->x1 > out->x2) return 0;
   if (out->y1 > out->y2) return 0;
   return 1;
}




static void age_mmesa( mgaContextPtr mmesa, int age )
{
   if (mmesa->CurrentTexObj[0]) mmesa->CurrentTexObj[0]->age = age;
   if (mmesa->CurrentTexObj[1]) mmesa->CurrentTexObj[1]->age = age;
}

#ifdef __i386__
static int __break_vertex = 0;
#endif

void mgaFlushVerticesLocked( mgaContextPtr mmesa )
{
   XF86DRIClipRectPtr pbox = mmesa->pClipRects;
   int nbox = mmesa->numClipRects;
   drmBufPtr buffer = mmesa->vertex_dma_buffer;
   drmMGAVertex vertex;
   int i;

   mmesa->vertex_dma_buffer = 0;

   if (!buffer)
      return;

   if (mmesa->dirty_cliprects & mmesa->draw_buffer)
      mgaUpdateRects( mmesa, mmesa->draw_buffer );

   if (mmesa->dirty & ~MGA_UPLOAD_CLIPRECTS)
      mgaEmitHwStateLocked( mmesa );

   /* FIXME: Workaround bug in kernel module.
    */
   mmesa->sarea->dirty |= MGA_UPLOAD_CONTEXT;

   if (!nbox)
      buffer->used = 0;

   if (nbox >= MGA_NR_SAREA_CLIPRECTS)
      mmesa->dirty |= MGA_UPLOAD_CLIPRECTS;

#if 0
   if (!buffer->used || !(mmesa->dirty & MGA_UPLOAD_CLIPRECTS))
   {
      if (nbox == 1)
	 mmesa->sarea->nbox = 0;
      else
	 mmesa->sarea->nbox = nbox;

      if (MGA_DEBUG&DEBUG_VERBOSE_IOCTL)
	 fprintf(stderr, "Firing vertex -- case a nbox %d\n", nbox);

      vertex.idx = buffer->idx;
      vertex.used = buffer->used;
      vertex.discard = 1;
      drmCommandWrite( mmesa->driFd, DRM_MGA_VERTEX, 
                       &vertex, sizeof(drmMGAVertex) );

      age_mmesa(mmesa, mmesa->sarea->last_enqueue);
   }
   else
#endif
   {
      for (i = 0 ; i < nbox ; )
      {
	 int nr = MIN2(i + MGA_NR_SAREA_CLIPRECTS, nbox);
	 XF86DRIClipRectPtr b = mmesa->sarea->boxes;
	 int discard = 0;

	 if (mmesa->scissor) {
	    mmesa->sarea->nbox = 0;

	    for ( ; i < nr ; i++) {
	       *b = pbox[i];
	       if (intersect_rect(b, b, &mmesa->scissor_rect)) {
		  mmesa->sarea->nbox++;
		  b++;
	       }
	    }

	    /* Culled?
	     */
	    if (!mmesa->sarea->nbox) {
	       if (nr < nbox) continue;
	       buffer->used = 0;
	    }
	 } else {
	    mmesa->sarea->nbox = nr - i;
	    for ( ; i < nr ; i++)
	       *b++ = pbox[i];
	 }

	 /* Finished with the buffer?
	  */
	 if (nr == nbox)
	    discard = 1;

	 mmesa->sarea->dirty |= MGA_UPLOAD_CLIPRECTS;

         vertex.idx = buffer->idx;
         vertex.used = buffer->used;
         vertex.discard = discard;
         drmCommandWrite( mmesa->driFd, DRM_MGA_VERTEX, 
                          &vertex, sizeof(drmMGAVertex) );

	 age_mmesa(mmesa, mmesa->sarea->last_enqueue);
      }
   }

   /* Do we really need to do this ? */
#ifdef __i386__
   if ( __break_vertex ) {
      __asm__ __volatile__ ( "int $3" );
   }
#endif

   mmesa->dirty &= ~MGA_UPLOAD_CLIPRECTS;
}

void mgaFlushVertices( mgaContextPtr mmesa )
{
   LOCK_HARDWARE( mmesa );
   mgaFlushVerticesLocked( mmesa );
   UNLOCK_HARDWARE( mmesa );
}


void mgaFireILoadLocked( mgaContextPtr mmesa,
			 GLuint offset, GLuint length )
{
   if (!mmesa->iload_buffer) {
      fprintf(stderr, "mgaFireILoad: no buffer\n");
      return;
   }

   if (MGA_DEBUG&DEBUG_VERBOSE_IOCTL)
      fprintf(stderr, "mgaFireILoad idx %d ofs 0x%x length %d\n",
	      mmesa->iload_buffer->idx, (int)offset, (int)length );

   mga_iload_dma_ioctl( mmesa, offset, length );
}

void mgaGetILoadBufferLocked( mgaContextPtr mmesa )
{
   if (MGA_DEBUG&DEBUG_VERBOSE_IOCTL)
      fprintf(stderr, "mgaGetIloadBuffer (buffer now %p)\n",
	   mmesa->iload_buffer);

   mmesa->iload_buffer = mga_get_buffer_ioctl( mmesa );
}

drmBufPtr mgaGetBufferLocked( mgaContextPtr mmesa )
{
   return mga_get_buffer_ioctl( mmesa );
}



void mgaDDFlush( GLcontext *ctx )
{
   mgaContextPtr mmesa = MGA_CONTEXT( ctx );


   FLUSH_BATCH( mmesa );

   /* This may be called redundantly - dispatch_age may trail what
    * has actually been sent and processed by the hardware.
    */
   if (1 || GET_DISPATCH_AGE( mmesa ) < mmesa->sarea->last_enqueue) {
      LOCK_HARDWARE( mmesa );
      UPDATE_LOCK( mmesa, DRM_LOCK_FLUSH );
      UNLOCK_HARDWARE( mmesa );
   }
}




void mgaReleaseBufLocked( mgaContextPtr mmesa, drmBufPtr buffer )
{
   drmMGAVertex vertex;

   if (!buffer) return;

   vertex.idx = buffer->idx;
   vertex.used = 0;
   vertex.discard = 1;
   drmCommandWrite( mmesa->driFd, DRM_MGA_VERTEX, 
                    &vertex, sizeof(drmMGAVertex) );
}

int mgaFlushDMA( int fd, drmLockFlags flags )
{
   drmMGALock lock;
   int ret, i = 0;

   memset( &lock, 0, sizeof(drmMGALock) );

   if ( flags & DRM_LOCK_QUIESCENT )    lock.flags |= DRM_LOCK_QUIESCENT;
   if ( flags & DRM_LOCK_FLUSH )        lock.flags |= DRM_LOCK_FLUSH;
   if ( flags & DRM_LOCK_FLUSH_ALL )    lock.flags |= DRM_LOCK_FLUSH_ALL;

   do {
      ret = drmCommandWrite( fd, DRM_MGA_FLUSH, &lock, sizeof(drmMGALock) );
   } while ( ret && errno == EBUSY && i++ < DRM_MGA_IDLE_RETRY );

   if ( ret == 0 )
      return 0;
   if ( errno != EBUSY )
      return -errno;

   if ( lock.flags & DRM_LOCK_QUIESCENT ) {
      /* Only keep trying if we need quiescence.
       */
      lock.flags &= ~(DRM_LOCK_FLUSH | DRM_LOCK_FLUSH_ALL);

      do {
         ret = drmCommandWrite( fd, DRM_MGA_FLUSH, &lock, sizeof(drmMGALock) );
      } while ( ret && errno == EBUSY && i++ < DRM_MGA_IDLE_RETRY );
   }

   if ( ret == 0 ) {
      return 0;
   } else {
      return -errno;
   }
}

void mgaDDInitIoctlFuncs( GLcontext *ctx )
{
   ctx->Driver.Clear = mgaDDClear;
   ctx->Driver.Flush = mgaDDFlush;
   ctx->Driver.Finish = mgaDDFinish;
}
