/* $XFree86: xc/lib/GL/mesa/src/drv/radeon/radeon_ioctl.h,v 1.6 2002/12/16 16:18:58 dawes Exp $ */
/**************************************************************************

Copyright 2000, 2001 ATI Technologies Inc., Ontario, Canada, and
                     VA Linux Systems Inc., Fremont, California.

All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
on the rights to use, copy, modify, merge, publish, distribute, sub
license, and/or sell copies of the Software, and to permit persons to whom
the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice (including the next
paragraph) shall be included in all copies or substantial portions of the
Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
ATI, VA LINUX SYSTEMS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Kevin E. Martin <martin@valinux.com>
 *   Gareth Hughes <gareth@valinux.com>
 *
 */

#ifndef __RADEON_IOCTL_H__
#define __RADEON_IOCTL_H__

#ifdef GLX_DIRECT_RENDERING

#include "simple_list.h"
#include "radeon_lock.h"


extern void radeonEmitState( radeonContextPtr rmesa );
extern void radeonEmitVertexAOS( radeonContextPtr rmesa,
				 GLuint vertex_size,
				 GLuint offset );

extern void radeonEmitVbufPrim( radeonContextPtr rmesa,
				GLuint vertex_format,
				GLuint primitive,
				GLuint vertex_nr );

extern void radeonFlushElts( radeonContextPtr rmesa );

extern GLushort *radeonAllocEltsOpenEnded( radeonContextPtr rmesa,
					   GLuint vertex_format,
					   GLuint primitive,
					   GLuint min_nr );

extern void radeonEmitAOS( radeonContextPtr rmesa,
			   struct radeon_dma_region **regions,
			   GLuint n,
			   GLuint offset );



extern void radeonFlushCmdBuf( radeonContextPtr rmesa, const char * );
extern void radeonRefillCurrentDmaRegion( radeonContextPtr rmesa );

extern void radeonAllocDmaRegion( radeonContextPtr rmesa,
				  struct radeon_dma_region *region,
				  int bytes, 
				  int alignment );

extern void radeonAllocDmaRegionVerts( radeonContextPtr rmesa,
				       struct radeon_dma_region *region,
				       int numverts,
				       int vertsize, 
				       int alignment );

extern void radeonReleaseDmaRegion( radeonContextPtr rmesa,
				    struct radeon_dma_region *region,
				    const char *caller );

extern void radeonCopyBuffer( const __DRIdrawablePrivate *drawable );
extern void radeonPageFlip( const __DRIdrawablePrivate *drawable );
extern void radeonFlush( GLcontext *ctx );
extern void radeonFinish( GLcontext *ctx );
extern void radeonWaitForIdleLocked( radeonContextPtr rmesa );
extern void radeonWaitForVBlank( radeonContextPtr rmesa );
extern void radeonInitIoctlFuncs( GLcontext *ctx );
extern void radeonGetAllParams( radeonContextPtr rmesa );

/* radeon_compat.c:
 */
extern void radeonCompatEmitPrimitive( radeonContextPtr rmesa,
				       GLuint vertex_format,
				       GLuint hw_primitive,
				       GLuint nrverts );


/* ================================================================
 * Helper macros:
 */

/* Close off the last primitive, if it exists.
 */
#define RADEON_NEWPRIM( rmesa )			\
do {						\
   if ( rmesa->dma.flush )			\
      rmesa->dma.flush( rmesa );	\
} while (0)

/* Can accomodate several state changes and primitive changes without
 * actually firing the buffer.
 */
#define RADEON_STATECHANGE( rmesa, ATOM )			\
do {								\
   RADEON_NEWPRIM( rmesa );					\
   move_to_head( &(rmesa->hw.dirty), &(rmesa->hw.ATOM));	\
} while (0)

#define RADEON_DB_STATE( ATOM )			        \
   memcpy( rmesa->hw.ATOM.lastcmd, rmesa->hw.ATOM.cmd,	\
	   rmesa->hw.ATOM.cmd_size * 4)

static __inline int RADEON_DB_STATECHANGE( 
   radeonContextPtr rmesa,
   struct radeon_state_atom *atom )
{
   if (memcmp(atom->cmd, atom->lastcmd, atom->cmd_size*4)) {
      int *tmp;
      RADEON_NEWPRIM( rmesa );
      move_to_head( &(rmesa->hw.dirty), atom );
      tmp = atom->cmd; 
      atom->cmd = atom->lastcmd;
      atom->lastcmd = tmp;
      return 1;
   }
   else
      return 0;
}


/* Fire the buffered vertices no matter what.
 */
#define RADEON_FIREVERTICES( rmesa )			\
do {							\
   if ( rmesa->store.cmd_used || rmesa->dma.flush ) {	\
      radeonFlush( rmesa->glCtx );			\
   }							\
} while (0)

/* Alloc space in the command buffer
 */
static __inline char *radeonAllocCmdBuf( radeonContextPtr rmesa,
					 int bytes, const char *where )
{
   if (rmesa->store.cmd_used + bytes > RADEON_CMD_BUF_SZ)
      radeonFlushCmdBuf( rmesa, __FUNCTION__ );
   
   assert(rmesa->dri.drmMinor >= 3);

   {
      char *head = rmesa->store.cmd_buf + rmesa->store.cmd_used;
      rmesa->store.cmd_used += bytes;
      return head;
   }
}




#endif
#endif /* __RADEON_IOCTL_H__ */
