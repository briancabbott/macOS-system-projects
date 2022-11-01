/* $XFree86: xc/lib/GL/mesa/src/drv/r200/r200_cmdbuf.c,v 1.1 2002/10/30 12:51:51 alanh Exp $ */
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
#include "simple_list.h"

static void print_state_atom( struct r200_state_atom *state )
{
   int i;

   fprintf(stderr, "emit %s/%d\n", state->name, state->cmd_size);

   if (0 & R200_DEBUG & DEBUG_VERBOSE) 
      for (i = 0 ; i < state->cmd_size ; i++) 
	 fprintf(stderr, "\t%s[%d]: %x\n", state->name, i, state->cmd[i]);

}

static void r200_emit_state_list( r200ContextPtr rmesa, 
				    struct r200_state_atom *list )
{
   struct r200_state_atom *state, *tmp;
   char *dest;

   foreach_s( state, tmp, list ) {
      if (state->check( rmesa->glCtx, state->idx )) {
	 dest = r200AllocCmdBuf( rmesa, state->cmd_size * 4, __FUNCTION__);
	 memcpy( dest, state->cmd, state->cmd_size * 4);
	 move_to_head( &(rmesa->hw.clean), state );
	 if (R200_DEBUG & DEBUG_STATE) 
	    print_state_atom( state );
      }
      else if (R200_DEBUG & DEBUG_STATE)
	 fprintf(stderr, "skip state %s\n", state->name);
   }
}


void r200EmitState( r200ContextPtr rmesa )
{
   struct r200_state_atom *state, *tmp;

   if (R200_DEBUG & (DEBUG_STATE|DEBUG_PRIMS))
      fprintf(stderr, "%s\n", __FUNCTION__);

   /* Somewhat overkill:
    */
   if ( rmesa->lost_context) {
      if (R200_DEBUG & (DEBUG_STATE|DEBUG_PRIMS|DEBUG_IOCTL))
	 fprintf(stderr, "%s - lost context\n", __FUNCTION__); 

      foreach_s( state, tmp, &(rmesa->hw.clean) ) 
	 move_to_tail(&(rmesa->hw.dirty), state );

      rmesa->lost_context = 0;
   }
   else {
      move_to_tail( &rmesa->hw.dirty, &rmesa->hw.mtl[0] ); 
      /* odd bug? -- isosurf, cycle between reflect & lit */
   }

   r200_emit_state_list( rmesa, &rmesa->hw.dirty );
}



/* Fire a section of the retained (indexed_verts) buffer as a regular
 * primtive.  
 */
extern void r200EmitVbufPrim( r200ContextPtr rmesa,
				GLuint primitive,
				GLuint vertex_nr )
{
   drmRadeonCmdHeader *cmd;

   assert(!(primitive & R200_VF_PRIM_WALK_IND));
   
   r200EmitState( rmesa );
   
   if (R200_DEBUG & (DEBUG_IOCTL|DEBUG_PRIMS))
      fprintf(stderr, "%s cmd_used/4: %d prim %x nr %d\n", __FUNCTION__,
	      rmesa->store.cmd_used/4, primitive, vertex_nr);
   
   cmd = (drmRadeonCmdHeader *)r200AllocCmdBuf( rmesa, 3 * sizeof(*cmd),
						  __FUNCTION__ );
   cmd[0].i = 0;
   cmd[0].header.cmd_type = RADEON_CMD_PACKET3_CLIP;
   cmd[1].i = R200_CP_CMD_3D_DRAW_VBUF_2;
   cmd[2].i = (primitive | 
	       R200_VF_PRIM_WALK_LIST |
	       R200_VF_COLOR_ORDER_RGBA |
	       (vertex_nr << R200_VF_VERTEX_NUMBER_SHIFT));


   if (R200_DEBUG & DEBUG_SYNC) {
      fprintf(stderr, "\nSyncing\n\n");
      R200_FIREVERTICES( rmesa );
      r200Finish( rmesa->glCtx );
   }
}


void r200FlushElts( r200ContextPtr rmesa )
{
   int *cmd = (int *)(rmesa->store.cmd_buf + rmesa->store.elts_start);
   int dwords;
   int nr = (rmesa->store.cmd_used - (rmesa->store.elts_start + 12)) / 2;

   if (R200_DEBUG & (DEBUG_IOCTL|DEBUG_PRIMS))
      fprintf(stderr, "%s\n", __FUNCTION__);

   assert( rmesa->dma.flush == r200FlushElts );
   rmesa->dma.flush = 0;

   /* Cope with odd number of elts:
    */
   rmesa->store.cmd_used = (rmesa->store.cmd_used + 2) & ~2;
   dwords = (rmesa->store.cmd_used - rmesa->store.elts_start) / 4;

   cmd[1] |= (dwords - 3) << 16;
   cmd[2] |= nr << R200_VF_VERTEX_NUMBER_SHIFT;

   if (R200_DEBUG & DEBUG_SYNC) {
      fprintf(stderr, "\nSyncing in %s\n\n", __FUNCTION__);
      R200_FIREVERTICES( rmesa );
      r200Finish( rmesa->glCtx );
   }
}


GLushort *r200AllocEltsOpenEnded( r200ContextPtr rmesa,
				    GLuint primitive,
				    GLuint min_nr )
{
   drmRadeonCmdHeader *cmd;
   GLushort *retval;

   if (R200_DEBUG & DEBUG_IOCTL)
      fprintf(stderr, "%s %d prim %x\n", __FUNCTION__, min_nr, primitive);

   assert((primitive & R200_VF_PRIM_WALK_IND));
   
   r200EmitState( rmesa );
   
   cmd = (drmRadeonCmdHeader *)r200AllocCmdBuf( rmesa, 
						12 + min_nr*2,
						__FUNCTION__ );
   cmd[0].i = 0;
   cmd[0].header.cmd_type = RADEON_CMD_PACKET3_CLIP;
   cmd[1].i = R200_CP_CMD_3D_DRAW_INDX_2;
   cmd[2].i = (primitive | 
	       R200_VF_PRIM_WALK_IND |
	       R200_VF_COLOR_ORDER_RGBA);

   
   retval = (GLushort *)(cmd+3);

   if (R200_DEBUG & DEBUG_PRIMS)
      fprintf(stderr, "%s: header 0x%x prim %x \n",
	      __FUNCTION__,
	      cmd[1].i, primitive);

   assert(!rmesa->dma.flush);
   rmesa->dma.flush = r200FlushElts;

   rmesa->store.elts_start = ((char *)cmd) - rmesa->store.cmd_buf;

   return retval;
}



void r200EmitVertexAOS( r200ContextPtr rmesa,
			  GLuint vertex_size,
			  GLuint offset )
{
   drmRadeonCmdHeader *cmd;

   if (R200_DEBUG & (DEBUG_PRIMS|DEBUG_IOCTL))
      fprintf(stderr, "%s:  vertex_size 0x%x offset 0x%x \n",
	      __FUNCTION__, vertex_size, offset);

   cmd = (drmRadeonCmdHeader *)r200AllocCmdBuf( rmesa, 5 * sizeof(int),
						  __FUNCTION__ );

   cmd[0].header.cmd_type = RADEON_CMD_PACKET3;
   cmd[1].i = R200_CP_CMD_3D_LOAD_VBPNTR | (2 << 16);
   cmd[2].i = 1;
   cmd[3].i = vertex_size | (vertex_size << 8);
   cmd[4].i = offset;
}
		       

void r200EmitAOS( r200ContextPtr rmesa,
		    struct r200_dma_region **component,
		    GLuint nr,
		    GLuint offset )
{
   drmRadeonCmdHeader *cmd;
   int sz = 3 + ((nr/2)*3) + ((nr&1)*2);
   int i;
   int *tmp;

   if (R200_DEBUG & DEBUG_IOCTL)
      fprintf(stderr, "%s nr arrays: %d\n", __FUNCTION__, nr);

   cmd = (drmRadeonCmdHeader *)r200AllocCmdBuf( rmesa, sz * sizeof(int),
						  __FUNCTION__ );
   cmd[0].i = 0;
   cmd[0].header.cmd_type = RADEON_CMD_PACKET3;
   cmd[1].i = R200_CP_CMD_3D_LOAD_VBPNTR | ((sz-3) << 16);
   cmd[2].i = nr;
   tmp = &cmd[0].i;
   cmd += 3;

   for (i = 0 ; i < nr ; i++) {
      if (i & 1) {
	 cmd[0].i |= ((component[i]->aos_stride << 24) | 
		      (component[i]->aos_size << 16));
	 cmd[2].i = (component[i]->aos_start + 
		     offset * component[i]->aos_stride * 4);
	 cmd += 3;
      }
      else {
	 cmd[0].i = ((component[i]->aos_stride << 8) | 
		     (component[i]->aos_size << 0));
	 cmd[1].i = (component[i]->aos_start + 
		     offset * component[i]->aos_stride * 4);
      }
   }

   if (R200_DEBUG & DEBUG_VERTS) {
      fprintf(stderr, "%s:\n", __FUNCTION__);
      for (i = 0 ; i < sz ; i++)
	 fprintf(stderr, "   %d: %x\n", i, tmp[i]);
   }
}

void r200EmitBlit( r200ContextPtr rmesa,
		   GLuint color_fmt,
		   GLuint src_pitch,
		   GLuint src_offset,
		   GLuint dst_pitch,
		   GLuint dst_offset,
		   GLint srcx, GLint srcy,
		   GLint dstx, GLint dsty,
		   GLuint w, GLuint h )
{
   drmRadeonCmdHeader *cmd;

   if (R200_DEBUG & DEBUG_IOCTL)
      fprintf(stderr, "%s src %x/%x %d,%d dst: %x/%x %d,%d sz: %dx%d\n",
	      __FUNCTION__, 
	      src_pitch, src_offset, srcx, srcy,
	      dst_pitch, dst_offset, dstx, dsty,
	      w, h);

   assert( (src_pitch & 63) == 0 );
   assert( (dst_pitch & 63) == 0 );
   assert( (src_offset & 1023) == 0 );
   assert( (dst_offset & 1023) == 0 );
   assert( w < (1<<16) );
   assert( h < (1<<16) );

   cmd = (drmRadeonCmdHeader *)r200AllocCmdBuf( rmesa, 8 * sizeof(int),
						  __FUNCTION__ );


   cmd[0].header.cmd_type = RADEON_CMD_PACKET3;
   cmd[1].i = R200_CP_CMD_BITBLT_MULTI | (5 << 16);
   cmd[2].i = (RADEON_GMC_SRC_PITCH_OFFSET_CNTL |
	       RADEON_GMC_DST_PITCH_OFFSET_CNTL |
	       RADEON_GMC_BRUSH_NONE |
	       (color_fmt << 8) |
	       RADEON_GMC_SRC_DATATYPE_COLOR |
	       RADEON_ROP3_S |
	       RADEON_DP_SRC_SOURCE_MEMORY |
	       RADEON_GMC_CLR_CMP_CNTL_DIS |
	       RADEON_GMC_WR_MSK_DIS );

   cmd[3].i = ((src_pitch/64)<<22) | (src_offset >> 10);
   cmd[4].i = ((dst_pitch/64)<<22) | (dst_offset >> 10);
   cmd[5].i = (srcx << 16) | srcy;
   cmd[6].i = (dstx << 16) | dsty; /* dst */
   cmd[7].i = (w << 16) | h;
}


void r200EmitWait( r200ContextPtr rmesa, GLuint flags )
{
   if (rmesa->dri.drmMinor >= 6) {
      drmRadeonCmdHeader *cmd;

      assert( !(flags & ~(RADEON_WAIT_2D|RADEON_WAIT_3D)) );
      
      cmd = (drmRadeonCmdHeader *)r200AllocCmdBuf( rmesa, 1 * sizeof(int),
						   __FUNCTION__ );
      cmd[0].i = 0;
      cmd[0].wait.cmd_type = RADEON_CMD_WAIT;
      cmd[0].wait.flags = flags;
   }
}
