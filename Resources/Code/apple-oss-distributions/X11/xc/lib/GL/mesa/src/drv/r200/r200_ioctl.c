/* $XFree86: xc/lib/GL/mesa/src/drv/r200/r200_ioctl.c,v 1.4 2002/12/17 00:32:56 dawes Exp $ */
/*
Copyright (C) The Weather Channel, Inc.  2002.  All Rights Reserved.

The Weather Channel (TM) funded Tungsten Graphics to develop the
initial release of the Radeon 8500 driver under the XFree86 license.
This notice must be preserved.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial
portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 */

#include "r200_context.h"
#include "r200_state.h"
#include "r200_ioctl.h"
#include "r200_tcl.h"
#include "r200_sanity.h"
#include "radeon_reg.h"

#include "mem.h"
#include "macros.h"
#include "context.h"
#include "swrast/swrast.h"

#include <unistd.h>	/* for usleep */

#define R200_TIMEOUT             512
#define R200_IDLE_RETRY           16


static void do_usleep( int nr, const char *caller )
{
   if (0) fprintf(stderr, "usleep %d in %s\n", nr, caller );
   if (1) usleep( nr );
}

static void r200WaitForIdle( r200ContextPtr rmesa );


int r200FlushCmdBufLocked( r200ContextPtr rmesa, const char * caller )
{
   int ret, i;
   drmRadeonCmdBuffer cmd;

   if (R200_DEBUG & DEBUG_IOCTL) {
      fprintf(stderr, "%s from %s\n", __FUNCTION__, caller); 

      if (0 & R200_DEBUG & DEBUG_VERBOSE) 
	 for (i = 0 ; i < rmesa->store.cmd_used ; i += 4 )
	    fprintf(stderr, "%d: %x\n", i/4, 
		    *(int *)(&rmesa->store.cmd_buf[i]));
   }

   if (R200_DEBUG & DEBUG_DMA)
      fprintf(stderr, "%s: Releasing %d buffers\n", __FUNCTION__,
	      rmesa->dma.nr_released_bufs);


   if (R200_DEBUG & DEBUG_SANITY) {
      if (rmesa->state.scissor.enabled) 
	 ret = r200SanityCmdBuffer( rmesa, 
				    rmesa->state.scissor.numClipRects,
				    rmesa->state.scissor.pClipRects);
      else
	 ret = r200SanityCmdBuffer( rmesa, 
				    rmesa->numClipRects,
				    rmesa->pClipRects);
      if (ret) {
	 fprintf(stderr, "drmSanityCommandWrite: %d\n", ret);	 
	 goto out;
      }
   }


   cmd.bufsz = rmesa->store.cmd_used;
   cmd.buf = rmesa->store.cmd_buf;

   if (rmesa->state.scissor.enabled) {
      cmd.nbox = rmesa->state.scissor.numClipRects;
      cmd.boxes = (drmClipRect *)rmesa->state.scissor.pClipRects;
   } else {
      cmd.nbox = rmesa->numClipRects;
      cmd.boxes = (drmClipRect *)rmesa->pClipRects;
   }

   ret = drmCommandWrite( rmesa->dri.fd,
			  DRM_RADEON_CMDBUF,
			  &cmd, sizeof(cmd) );

   if (ret)
      fprintf(stderr, "drmCommandWrite: %d\n", ret);

   if (R200_DEBUG & DEBUG_SYNC) {
      fprintf(stderr, "\nSyncing in %s\n\n", __FUNCTION__);
      r200WaitForIdleLocked( rmesa );
   }


 out:
   rmesa->store.primnr = 0;
   rmesa->store.statenr = 0;
   rmesa->store.cmd_used = 0;
   rmesa->dma.nr_released_bufs = 0;
/*    rmesa->lost_context = 0; */
   rmesa->lost_context = 1;	
   return ret;
}


/* Note: does not emit any commands to avoid recursion on
 * r200AllocCmdBuf.
 */
void r200FlushCmdBuf( r200ContextPtr rmesa, const char *caller )
{
   int ret;

   LOCK_HARDWARE( rmesa );

   ret = r200FlushCmdBufLocked( rmesa, caller );

   UNLOCK_HARDWARE( rmesa );

   if (ret) {
      fprintf(stderr, "drmRadeonCmdBuffer: %d\n", ret);
      exit(ret);
   }
}


/* =============================================================
 * Hardware vertex buffer handling
 */


void r200RefillCurrentDmaRegion( r200ContextPtr rmesa )
{
   struct r200_dma_buffer *dmabuf;
   int fd = rmesa->dri.fd;
   int index = 0;
   int size = 0;
   drmDMAReq dma;
   int ret;

   if (R200_DEBUG & (DEBUG_IOCTL|DEBUG_DMA))
      fprintf(stderr, "%s\n", __FUNCTION__);  

   if (rmesa->dma.flush) {
      rmesa->dma.flush( rmesa );
   }

   if (rmesa->dma.current.buf)
      r200ReleaseDmaRegion( rmesa, &rmesa->dma.current, __FUNCTION__ );

   if (rmesa->dma.nr_released_bufs > 4)
      r200FlushCmdBuf( rmesa, __FUNCTION__ );

   dma.context = rmesa->dri.hwContext;
   dma.send_count = 0;
   dma.send_list = NULL;
   dma.send_sizes = NULL;
   dma.flags = 0;
   dma.request_count = 1;
   dma.request_size = RADEON_BUFFER_SIZE;
   dma.request_list = &index;
   dma.request_sizes = &size;
   dma.granted_count = 0;

   LOCK_HARDWARE(rmesa);	/* no need to validate */

   while (1) {
      ret = drmDMA( fd, &dma );
      if (ret == 0)
	 break;
   
      if (rmesa->dma.nr_released_bufs) {
	 r200FlushCmdBufLocked( rmesa, __FUNCTION__ );
      }
      
      if (rmesa->do_usleeps) {
	 UNLOCK_HARDWARE( rmesa );
	 do_usleep(1, __FUNCTION__);
	 LOCK_HARDWARE( rmesa );
      }
   }

   UNLOCK_HARDWARE(rmesa);

   if (R200_DEBUG & DEBUG_DMA)
      fprintf(stderr, "Allocated buffer %d\n", index);

   dmabuf = CALLOC_STRUCT( r200_dma_buffer );
   dmabuf->buf = &rmesa->r200Screen->buffers->list[index];
   dmabuf->refcount = 1;

   rmesa->dma.current.buf = dmabuf;
   rmesa->dma.current.address = dmabuf->buf->address;
   rmesa->dma.current.end = dmabuf->buf->total;
   rmesa->dma.current.start = 0;
   rmesa->dma.current.ptr = 0;
}

void r200ReleaseDmaRegion( r200ContextPtr rmesa,
			     struct r200_dma_region *region,
			     const char *caller )
{
   if (R200_DEBUG & DEBUG_IOCTL)
      fprintf(stderr, "%s from %s\n", __FUNCTION__, caller); 
   
   if (!region->buf)
      return;

   if (rmesa->dma.flush)
      rmesa->dma.flush( rmesa );

   if (--region->buf->refcount == 0) {
      drmRadeonCmdHeader *cmd;

      if (R200_DEBUG & (DEBUG_IOCTL|DEBUG_DMA))
	 fprintf(stderr, "%s -- DISCARD BUF %d\n", __FUNCTION__,
		 region->buf->buf->idx);  
      
      cmd = (drmRadeonCmdHeader *)r200AllocCmdBuf( rmesa, sizeof(*cmd), 
						     __FUNCTION__ );
      cmd->dma.cmd_type = RADEON_CMD_DMA_DISCARD;
      cmd->dma.buf_idx = region->buf->buf->idx;
      FREE(region->buf);
      rmesa->dma.nr_released_bufs++;
   }

   region->buf = 0;
   region->start = 0;
}

/* Allocates a region from rmesa->dma.current.  If there isn't enough
 * space in current, grab a new buffer (and discard what was left of current)
 */
void r200AllocDmaRegion( r200ContextPtr rmesa, 
			   struct r200_dma_region *region,
			   int bytes,
			   int alignment )
{
   if (R200_DEBUG & DEBUG_IOCTL)
      fprintf(stderr, "%s %d\n", __FUNCTION__, bytes);

   if (rmesa->dma.flush)
      rmesa->dma.flush( rmesa );

   if (region->buf)
      r200ReleaseDmaRegion( rmesa, region, __FUNCTION__ );

   alignment--;
   rmesa->dma.current.start = rmesa->dma.current.ptr = 
      (rmesa->dma.current.ptr + alignment) & ~alignment;

   if ( rmesa->dma.current.ptr + bytes > rmesa->dma.current.end ) 
      r200RefillCurrentDmaRegion( rmesa );

   region->start = rmesa->dma.current.start;
   region->ptr = rmesa->dma.current.start;
   region->end = rmesa->dma.current.start + bytes;
   region->address = rmesa->dma.current.address;
   region->buf = rmesa->dma.current.buf;
   region->buf->refcount++;

   rmesa->dma.current.ptr += bytes; /* bug - if alignment > 7 */
   rmesa->dma.current.start = 
      rmesa->dma.current.ptr = (rmesa->dma.current.ptr + 0x7) & ~0x7;  
}

void r200AllocDmaRegionVerts( r200ContextPtr rmesa, 
				struct r200_dma_region *region,
				int numverts,
				int vertsize,
				int alignment )
{
   r200AllocDmaRegion( rmesa, region, vertsize * numverts, alignment );
}

/* ================================================================
 * SwapBuffers with client-side throttling
 */

static CARD32 r200GetLastFrame(r200ContextPtr rmesa)
{
   drmRadeonGetParam gp;
   int ret;
   CARD32 frame;

   gp.param = RADEON_PARAM_LAST_FRAME;
   gp.value = (int *)&frame;
   ret = drmCommandWriteRead( rmesa->dri.fd, DRM_RADEON_GETPARAM,
			      &gp, sizeof(gp) );
   if ( ret ) {
      fprintf( stderr, "%s: drmRadeonGetParam: %d\n", __FUNCTION__, ret );
      exit(1);
   }

   return frame;
}

static void r200EmitIrqLocked( r200ContextPtr rmesa )
{
   drmRadeonIrqEmit ie;
   int ret;

   ie.irq_seq = &rmesa->iw.irq_seq;
   ret = drmCommandWriteRead( rmesa->dri.fd, DRM_RADEON_IRQ_EMIT, 
			      &ie, sizeof(ie) );
   if ( ret ) {
      fprintf( stderr, "%s: drmRadeonIrqEmit: %d\n", __FUNCTION__, ret );
      exit(1);
   }
}


static void r200WaitIrq( r200ContextPtr rmesa )
{
   int ret;

   do {
      ret = drmCommandWrite( rmesa->dri.fd, DRM_RADEON_IRQ_WAIT,
			     &rmesa->iw, sizeof(rmesa->iw) );
   } while (ret && (errno == EINTR || errno == EAGAIN));

   if ( ret ) {
      fprintf( stderr, "%s: drmRadeonIrqWait: %d\n", __FUNCTION__, ret );
      exit(1);
   }
}


static void r200WaitForFrameCompletion( r200ContextPtr rmesa )
{
   RADEONSAREAPrivPtr sarea = rmesa->sarea;

   if (rmesa->do_irqs) {
      if (r200GetLastFrame(rmesa) < sarea->last_frame) {
	 if (!rmesa->irqsEmitted) {
	    while (r200GetLastFrame (rmesa) < sarea->last_frame)
	       ;
	 }
	 else {
	    UNLOCK_HARDWARE( rmesa ); 
	    r200WaitIrq( rmesa );	
	    LOCK_HARDWARE( rmesa ); 
	 }
	 rmesa->irqsEmitted = 10;
      }

      if (rmesa->irqsEmitted) {
	 r200EmitIrqLocked( rmesa );
	 rmesa->irqsEmitted--;
      }
   } 
   else {
      while (r200GetLastFrame (rmesa) < sarea->last_frame) {
	 UNLOCK_HARDWARE( rmesa ); 
	 if (rmesa->do_usleeps) 
	    do_usleep(1, __FUNCTION__); 
	 LOCK_HARDWARE( rmesa ); 
      }
   }
}



/* Copy the back color buffer to the front color buffer.
 */
void r200CopyBuffer( const __DRIdrawablePrivate *dPriv )
{
   r200ContextPtr rmesa;
   GLint nbox, i, ret;

   assert(dPriv);
   assert(dPriv->driContextPriv);
   assert(dPriv->driContextPriv->driverPrivate);

   rmesa = (r200ContextPtr) dPriv->driContextPriv->driverPrivate;

   if ( R200_DEBUG & DEBUG_IOCTL ) {
      fprintf( stderr, "\n%s( %p )\n\n", __FUNCTION__, rmesa->glCtx );
   }

   R200_FIREVERTICES( rmesa );

   LOCK_HARDWARE( rmesa );


   /* Throttle the frame rate -- only allow one pending swap buffers
    * request at a time.
    */
   r200WaitForFrameCompletion( rmesa );

   r200WaitForVBlank( rmesa );

   nbox = rmesa->dri.drawable->numClipRects; /* must be in locked region */

   for ( i = 0 ; i < nbox ; ) {
      GLint nr = MIN2( i + RADEON_NR_SAREA_CLIPRECTS , nbox );
      XF86DRIClipRectPtr box = rmesa->dri.drawable->pClipRects;
      XF86DRIClipRectPtr b = rmesa->sarea->boxes;
      GLint n = 0;

      for ( ; i < nr ; i++ ) {
	 *b++ = box[i];
	 n++;
      }
      rmesa->sarea->nbox = n;

      ret = drmCommandNone( rmesa->dri.fd, DRM_RADEON_SWAP );

      if ( ret ) {
	 fprintf( stderr, "DRM_R200_SWAP_BUFFERS: return = %d\n", ret );
	 UNLOCK_HARDWARE( rmesa );
	 exit( 1 );
      }
   }

   UNLOCK_HARDWARE( rmesa );
   rmesa->lost_context = 1;
}

void r200PageFlip( const __DRIdrawablePrivate *dPriv )
{
   r200ContextPtr rmesa;
   GLint ret;

   assert(dPriv);
   assert(dPriv->driContextPriv);
   assert(dPriv->driContextPriv->driverPrivate);

   rmesa = (r200ContextPtr) dPriv->driContextPriv->driverPrivate;

   if ( R200_DEBUG & DEBUG_IOCTL ) {
      fprintf(stderr, "%s: pfCurrentPage: %d\n", __FUNCTION__,
	      rmesa->sarea->pfCurrentPage);
   }

   R200_FIREVERTICES( rmesa );
   LOCK_HARDWARE( rmesa );

   if (!rmesa->dri.drawable->numClipRects) {
      UNLOCK_HARDWARE( rmesa );
      usleep( 10000 );		/* throttle invisible client 10ms */
      return;
   }

   /* Need to do this for the perf box placement:
    */
   {
      XF86DRIClipRectPtr box = rmesa->dri.drawable->pClipRects;
      XF86DRIClipRectPtr b = rmesa->sarea->boxes;
      b[0] = box[0];
      rmesa->sarea->nbox = 1;
   }

   /* Throttle the frame rate -- only allow a few pending swap buffers
    * request at a time.
    */
   r200WaitForFrameCompletion( rmesa );

   r200WaitForVBlank( rmesa );

   ret = drmCommandNone( rmesa->dri.fd, DRM_RADEON_FLIP );

   UNLOCK_HARDWARE( rmesa );

   if ( ret ) {
      fprintf( stderr, "DRM_R200_FLIP: return = %d\n", ret );
      exit( 1 );
   }

   if ( rmesa->sarea->pfCurrentPage == 1 ) {
	 rmesa->state.color.drawOffset = rmesa->r200Screen->frontOffset;
	 rmesa->state.color.drawPitch  = rmesa->r200Screen->frontPitch;
   } else {
	 rmesa->state.color.drawOffset = rmesa->r200Screen->backOffset;
	 rmesa->state.color.drawPitch  = rmesa->r200Screen->backPitch;
   }

   R200_STATECHANGE( rmesa, ctx );
   rmesa->hw.ctx.cmd[CTX_RB3D_COLOROFFSET] = rmesa->state.color.drawOffset;
   rmesa->hw.ctx.cmd[CTX_RB3D_COLORPITCH]  = rmesa->state.color.drawPitch;
}


/* ================================================================
 * Buffer clear
 */
static void r200Clear( GLcontext *ctx, GLbitfield mask, GLboolean all,
			 GLint cx, GLint cy, GLint cw, GLint ch )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   __DRIdrawablePrivate *dPriv = rmesa->dri.drawable;
   GLuint flags = 0;
   GLuint color_mask = 0;
   GLint ret, i;

   if ( R200_DEBUG & DEBUG_IOCTL ) {
      fprintf( stderr, "%s:  all=%d cx=%d cy=%d cw=%d ch=%d\n",
	       __FUNCTION__, all, cx, cy, cw, ch );
   }

   {
      LOCK_HARDWARE( rmesa );
      UNLOCK_HARDWARE( rmesa );
      if ( dPriv->numClipRects == 0 ) 
	 return;
   }

   r200EmitState( rmesa );

   /* Need to cope with lostcontext here as kernel relies on
    * some residual state:
    */
   R200_FIREVERTICES( rmesa ); 

   if ( mask & DD_FRONT_LEFT_BIT ) {
      flags |= RADEON_FRONT;
      color_mask = rmesa->hw.msk.cmd[MSK_RB3D_PLANEMASK];
      mask &= ~DD_FRONT_LEFT_BIT;
   }

   if ( mask & DD_BACK_LEFT_BIT ) {
      flags |= RADEON_BACK;
      color_mask = rmesa->hw.msk.cmd[MSK_RB3D_PLANEMASK];
      mask &= ~DD_BACK_LEFT_BIT;
   }

   if ( mask & DD_DEPTH_BIT ) {
      if ( ctx->Depth.Mask ) flags |= RADEON_DEPTH; /* FIXME: ??? */
      mask &= ~DD_DEPTH_BIT;
   }

   if ( (mask & DD_STENCIL_BIT) && rmesa->state.stencil.hwBuffer ) {
      flags |= RADEON_STENCIL;
      mask &= ~DD_STENCIL_BIT;
   }

   if ( mask )
      _swrast_Clear( ctx, mask, all, cx, cy, cw, ch );

   if ( !flags ) 
      return;

   /* Flip top to bottom */
   cx += dPriv->x;
   cy  = dPriv->y + dPriv->h - cy - ch;

   LOCK_HARDWARE( rmesa );

   /* Throttle the number of clear ioctls we do.
    */
   while ( 1 ) {
      drmRadeonGetParam gp;
      int ret;
      int clear;

      gp.param = RADEON_PARAM_LAST_CLEAR;
      gp.value = (int *)&clear;
      ret = drmCommandWriteRead( rmesa->dri.fd,
		      DRM_RADEON_GETPARAM, &gp, sizeof(gp) );

      if ( ret ) {
	 fprintf( stderr, "%s: drmRadeonGetParam: %d\n", __FUNCTION__, ret );
	 exit(1);
      }

      /* Clear throttling needs more thought.
       */
      if ( rmesa->sarea->last_clear - clear <= 8 ) {
	 break;
      }
      
      if (rmesa->do_usleeps) {
	 UNLOCK_HARDWARE( rmesa );
	 do_usleep(1, __FUNCTION__);
	 LOCK_HARDWARE( rmesa );
      }
   }


   for ( i = 0 ; i < dPriv->numClipRects ; ) {
      GLint nr = MIN2( i + RADEON_NR_SAREA_CLIPRECTS, dPriv->numClipRects );
      XF86DRIClipRectPtr box = dPriv->pClipRects;
      XF86DRIClipRectPtr b = rmesa->sarea->boxes;
      drmRadeonClearType clear;
      drmRadeonClearRect depth_boxes[RADEON_NR_SAREA_CLIPRECTS];
      GLint n = 0;

      if ( !all ) {
	 for ( ; i < nr ; i++ ) {
	    GLint x = box[i].x1;
	    GLint y = box[i].y1;
	    GLint w = box[i].x2 - x;
	    GLint h = box[i].y2 - y;

	    if ( x < cx ) w -= cx - x, x = cx;
	    if ( y < cy ) h -= cy - y, y = cy;
	    if ( x + w > cx + cw ) w = cx + cw - x;
	    if ( y + h > cy + ch ) h = cy + ch - y;
	    if ( w <= 0 ) continue;
	    if ( h <= 0 ) continue;

	    b->x1 = x;
	    b->y1 = y;
	    b->x2 = x + w;
	    b->y2 = y + h;
	    b++;
	    n++;
	 }
      } else {
	 for ( ; i < nr ; i++ ) {
	    *b++ = box[i];
	    n++;
	 }
      }

      rmesa->sarea->nbox = n;

      clear.flags       = flags;
      clear.clear_color = rmesa->state.color.clear;
      clear.clear_depth = 0;	/* not used */
      clear.color_mask  = rmesa->hw.msk.cmd[MSK_RB3D_PLANEMASK];
      clear.depth_mask  = rmesa->state.stencil.clear;
      clear.depth_boxes = depth_boxes;

      n--;
      b = rmesa->sarea->boxes;
      for ( ; n >= 0 ; n-- ) {
	 depth_boxes[n].f[RADEON_CLEAR_X1] = (float)b[n].x1;
	 depth_boxes[n].f[RADEON_CLEAR_Y1] = (float)b[n].y1;
	 depth_boxes[n].f[RADEON_CLEAR_X2] = (float)b[n].x2;
	 depth_boxes[n].f[RADEON_CLEAR_Y2] = (float)b[n].y2;
	 depth_boxes[n].f[RADEON_CLEAR_DEPTH] = ctx->Depth.Clear;
      }

      ret = drmCommandWrite( rmesa->dri.fd, DRM_RADEON_CLEAR,
			     &clear, sizeof(drmRadeonClearType));


      if ( ret ) {
	 UNLOCK_HARDWARE( rmesa );
	 fprintf( stderr, "DRM_RADEON_CLEAR: return = %d\n", ret );
	 exit( 1 );
      }
   }

   UNLOCK_HARDWARE( rmesa );
   rmesa->lost_context = 1;
}


void r200WaitForIdleLocked( r200ContextPtr rmesa )
{
    int ret;
    int i = 0;
    
    do {
       ret = drmCommandNone( rmesa->dri.fd, DRM_RADEON_CP_IDLE);
       if (ret) 
	  do_usleep( 1, __FUNCTION__ );
    } while (ret && ++i < 100);
    
    if ( ret < 0 ) {
       UNLOCK_HARDWARE( rmesa );
       fprintf( stderr, "Error: R200 timed out... exiting\n" );
       exit( -1 );
    }
}

static void r200WaitForIdle( r200ContextPtr rmesa )
{
   LOCK_HARDWARE(rmesa);
   r200WaitForIdleLocked( rmesa );
   UNLOCK_HARDWARE(rmesa);
}


void r200WaitForVBlank( r200ContextPtr rmesa )
{
   drmVBlank vbl;
   int ret;

   if ( !rmesa->r200Screen->irq )
      return;

   if ( getenv("LIBGL_SYNC_REFRESH") ) {
      /* Wait for until the next vertical blank */
      vbl.request.type = DRM_VBLANK_RELATIVE;
      vbl.request.sequence = 1;
   } else if ( getenv("LIBGL_THROTTLE_REFRESH") ) {
      /* Wait for at least one vertical blank since the last call */
      vbl.request.type = DRM_VBLANK_ABSOLUTE;
      vbl.request.sequence = rmesa->vbl_seq + 1;
   } else {
      return;
   }

   UNLOCK_HARDWARE( rmesa );

   if ((ret = drmWaitVBlank( rmesa->dri.fd, &vbl ))) {
      fprintf(stderr, "%s: drmWaitVBlank returned %d, IRQs don't seem to be"
	      " working correctly.\nTry running with LIBGL_THROTTLE_REFRESH"
	      " and LIBL_SYNC_REFRESH unset.\n", __FUNCTION__, ret);
      exit(1);
   } else if (R200_DEBUG & DEBUG_IOCTL)
      fprintf(stderr, "%s: drmWaitVBlank returned %d\n", __FUNCTION__, ret);
   rmesa->vbl_seq = vbl.reply.sequence;

   LOCK_HARDWARE( rmesa );
}


void r200Flush( GLcontext *ctx )
{
   r200ContextPtr rmesa = R200_CONTEXT( ctx );

   if (R200_DEBUG & DEBUG_IOCTL)
      fprintf(stderr, "%s\n", __FUNCTION__);

   if (rmesa->dma.flush)
      rmesa->dma.flush( rmesa );

   if (!is_empty_list(&rmesa->hw.dirty)) 
      r200EmitState( rmesa );
   
   if (rmesa->store.cmd_used)
      r200FlushCmdBuf( rmesa, __FUNCTION__ );
}

/* Make sure all commands have been sent to the hardware and have
 * completed processing.
 */
void r200Finish( GLcontext *ctx )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   r200Flush( ctx );

   if (rmesa->do_irqs) {
      LOCK_HARDWARE( rmesa );
      r200EmitIrqLocked( rmesa );
      UNLOCK_HARDWARE( rmesa );
      r200WaitIrq( rmesa );
   }
   else 
      r200WaitForIdle( rmesa );
}


/* This version of AllocateMemoryNV allocates only agp memory, and
 * only does so after the point at which the driver has been
 * initialized.
 *
 * Theoretically a valid context isn't required.  However, in this
 * implementation, it is, as I'm using the hardware lock to protect
 * the kernel data structures, and the current context to get the
 * device fd.
 */
void *r200AllocateMemoryNV(GLsizei size, GLfloat readfreq,
			    GLfloat writefreq, GLfloat priority)
{
   GET_CURRENT_CONTEXT(ctx);
   r200ContextPtr rmesa;
   int region_offset;
   drmRadeonMemAlloc alloc;
   int ret;

   if (R200_DEBUG & DEBUG_IOCTL)
      fprintf(stderr, "%s sz %d %f/%f/%f\n", __FUNCTION__, size, readfreq, 
	      writefreq, priority);

   if (!ctx || !(rmesa = R200_CONTEXT(ctx)) || rmesa->r200Screen->IsPCI ) 
      return NULL;

   if (getenv("R200_NO_ALLOC"))
      return NULL;
   
   if (rmesa->dri.drmMinor < 6) 
      return NULL;
      
   alloc.region = RADEON_MEM_REGION_AGP;
   alloc.alignment = 0;
   alloc.size = size;
   alloc.region_offset = &region_offset;

   ret = drmCommandWriteRead( rmesa->r200Screen->driScreen->fd,
			      DRM_RADEON_ALLOC,
			      &alloc, sizeof(alloc));
   
   if (ret) {
      fprintf(stderr, "%s: DRM_RADEON_ALLOC ret %d\n", __FUNCTION__, ret);
      return NULL;
   }
   
   {
      char *region_start = (char *)rmesa->r200Screen->agpTextures.map;
      return (void *)(region_start + region_offset);
   }
}


/* Called via glXFreeMemoryNV() */
void r200FreeMemoryNV(GLvoid *pointer)
{
   GET_CURRENT_CONTEXT(ctx);
   r200ContextPtr rmesa;
   int region_offset;
   drmRadeonMemFree memfree;
   int ret;

   if (R200_DEBUG & DEBUG_IOCTL)
      fprintf(stderr, "%s %p\n", __FUNCTION__, pointer);

   if (!ctx || !(rmesa = R200_CONTEXT(ctx)) || rmesa->r200Screen->IsPCI ) {
      fprintf(stderr, "%s: no context\n", __FUNCTION__);
      return;
   }

   if (rmesa->dri.drmMinor < 6) 
      return;

   region_offset = (char *)pointer - (char *)rmesa->r200Screen->agpTextures.map;

   if (region_offset < 0 || 
       region_offset > rmesa->r200Screen->agpTextures.size) {
      fprintf(stderr, "offset %d outside range 0..%d\n", region_offset,
	      rmesa->r200Screen->agpTextures.size);
      return;
   }

   memfree.region = RADEON_MEM_REGION_AGP;
   memfree.region_offset = region_offset;
   
   ret = drmCommandWrite( rmesa->r200Screen->driScreen->fd,
			  DRM_RADEON_FREE,
			  &memfree, sizeof(memfree));
   
   if (ret) 
      fprintf(stderr, "%s: DRM_RADEON_FREE ret %d\n", __FUNCTION__, ret);
}

/* Called via glXGetAGPOffsetMESA() */
GLuint r200GetAGPOffset(const GLvoid *pointer)
{
   GET_CURRENT_CONTEXT(ctx);
   r200ContextPtr rmesa;
   GLuint card_offset;

   if (!ctx || !(rmesa = R200_CONTEXT(ctx)) ) {
      fprintf(stderr, "%s: no context\n", __FUNCTION__);
      return ~0;
   }

   if (!r200IsAgpMemory( rmesa, pointer, 0 ))
      return ~0;

   if (rmesa->dri.drmMinor < 6) 
      return ~0;

   card_offset = r200AgpOffsetFromVirtual( rmesa, pointer );

   return card_offset - rmesa->r200Screen->agp_base;
}


GLboolean r200IsAgpMemory( r200ContextPtr rmesa, const GLvoid *pointer,
			   GLint size )
{
   int offset = (char *)pointer - (char *)rmesa->r200Screen->agpTextures.map;
   int valid = (size >= 0 &&
		offset >= 0 &&
		offset + size < rmesa->r200Screen->agpTextures.size);

   if (R200_DEBUG & DEBUG_IOCTL)
      fprintf(stderr, "r200IsAgpMemory( %p ) : %d\n", pointer, valid );
   
   return valid;
}


GLuint r200AgpOffsetFromVirtual( r200ContextPtr rmesa, const GLvoid *pointer )
{
   int offset = (char *)pointer - (char *)rmesa->r200Screen->agpTextures.map;

   if (offset < 0 || offset > rmesa->r200Screen->agpTextures.size)
      return ~0;
   else
      return rmesa->r200Screen->agp_texture_offset + offset;
}



void r200InitIoctlFuncs( GLcontext *ctx )
{
    ctx->Driver.Clear = r200Clear;
    ctx->Driver.Finish = r200Finish;
    ctx->Driver.Flush = r200Flush;
}

