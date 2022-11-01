/* $XFree86: xc/lib/GL/mesa/src/drv/r200/r200_maos_arrays.c,v 1.3 2003/02/23 23:59:01 dawes Exp $ */
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

#include "glheader.h"
#include "mtypes.h"
#include "colormac.h"
#include "mem.h"
#include "mmath.h"
#include "macros.h"

#include "swrast_setup/swrast_setup.h"
#include "math/m_translate.h"
#include "tnl/tnl.h"
#include "tnl/t_context.h"
#include "tnl/t_imm_debug.h"

#include "r200_context.h"
#include "r200_ioctl.h"
#include "r200_state.h"
#include "r200_swtcl.h"
#include "r200_maos.h"

/* Usage:
 *   - from r200_tcl_render
 *   - call r200EmitArrays to ensure uptodate arrays in dma
 *   - emit primitives (new type?) which reference the data
 *       -- need to use elts for lineloop, quads, quadstrip/flat
 *       -- other primitives are all well-formed (need tristrip-1,fake-poly)
 *
 */
static void emit_ubyte_rgba3( GLcontext *ctx,
		       struct r200_dma_region *rvb,
		       char *data,
		       int stride,
		       int count )
{
   int i;
   r200_color_t *out = (r200_color_t *)(rvb->start + rvb->address);

   if (R200_DEBUG & DEBUG_VERTS)
      fprintf(stderr, "%s count %d stride %d out %p\n",
	      __FUNCTION__, count, stride, out);

   for (i = 0; i < count; i++) {
      out->red   = *data;
      out->green = *(data+1);
      out->blue  = *(data+2);
      out->alpha = 0xFF;
      out++;
      data += stride;
   }
}


#if defined(USE_X86_ASM)
#define COPY_DWORDS( dst, src, nr )					\
do {									\
	int __tmp;							\
	__asm__ __volatile__( "rep ; movsl"				\
			      : "=%c" (__tmp), "=D" (dst), "=S" (__tmp)	\
			      : "0" (nr),				\
			        "D" ((long)dst),			\
			        "S" ((long)src) );			\
} while (0)
#else
#define COPY_DWORDS( dst, src, nr )		\
do {						\
   int j;					\
   for ( j = 0 ; j < nr ; j++ )			\
      dst[j] = ((int *)src)[j];			\
   dst += nr;					\
} while (0)
#endif



static void emit_ubyte_rgba4( GLcontext *ctx,
			      struct r200_dma_region *rvb,
			      char *data,
			      int stride,
			      int count )
{
   int i;
   int *out = (int *)(rvb->address + rvb->start);

   if (R200_DEBUG & DEBUG_VERTS)
      fprintf(stderr, "%s count %d stride %d\n",
	      __FUNCTION__, count, stride);

   if (stride == 4) {
      for (i = 0; i < count; i++)
	 ((int *)out)[i] = LE32_TO_CPU(((int *)data)[i]);
   } else {
      for (i = 0; i < count; i++) {
	 *(int *)out++ = LE32_TO_CPU(*(int *)data);
	 data += stride;
      }
   }
}


static void emit_ubyte_rgba( GLcontext *ctx,
			     struct r200_dma_region *rvb,
			     char *data,
			     int size,
			     int stride,
			     int count )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);

   if (R200_DEBUG & DEBUG_VERTS)
      fprintf(stderr, "%s %d/%d\n", __FUNCTION__, count, size);

   assert (!rvb->buf);

   if (stride == 0) {
      r200AllocDmaRegion( rmesa, rvb, 4, 4 );
      count = 1;
      rvb->aos_start = GET_START(rvb);
      rvb->aos_stride = 0;
      rvb->aos_size = 1;
   }
   else {
      r200AllocDmaRegion( rmesa, rvb, 4 * count, 4 );	/* alignment? */
      rvb->aos_start = GET_START(rvb);
      rvb->aos_stride = 1;
      rvb->aos_size = 1;
   }

   /* Emit the data
    */
   switch (size) {
   case 3:
      emit_ubyte_rgba3( ctx, rvb, data, stride, count );
      break;
   case 4:
      emit_ubyte_rgba4( ctx, rvb, data, stride, count );
      break;
   default:
      assert(0);
      exit(1);
      break;
   }
}




static void emit_vec8( GLcontext *ctx,
		       struct r200_dma_region *rvb,
		       char *data,
		       int stride,
		       int count )
{
   int i;
   int *out = (int *)(rvb->address + rvb->start);

   if (R200_DEBUG & DEBUG_VERTS)
      fprintf(stderr, "%s count %d stride %d\n",
	      __FUNCTION__, count, stride);

   if (stride == 8)
      COPY_DWORDS( out, data, count*2 );
   else
      for (i = 0; i < count; i++) {
	 out[0] = *(int *)data;
	 out[1] = *(int *)(data+4);
	 out += 2;
	 data += stride;
      }
}

static void emit_vec12( GLcontext *ctx,
		       struct r200_dma_region *rvb,
		       char *data,
		       int stride,
		       int count )
{
   int i;
   int *out = (int *)(rvb->address + rvb->start);

   if (R200_DEBUG & DEBUG_VERTS)
      fprintf(stderr, "%s count %d stride %d out %p data %p\n",
	      __FUNCTION__, count, stride, out, data);

   if (stride == 12)
      COPY_DWORDS( out, data, count*3 );
   else
      for (i = 0; i < count; i++) {
	 out[0] = *(int *)data;
	 out[1] = *(int *)(data+4);
	 out[2] = *(int *)(data+8);
	 out += 3;
	 data += stride;
      }
}

static void emit_vec16( GLcontext *ctx,
			struct r200_dma_region *rvb,
			char *data,
			int stride,
			int count )
{
   int i;
   int *out = (int *)(rvb->address + rvb->start);

   if (R200_DEBUG & DEBUG_VERTS)
      fprintf(stderr, "%s count %d stride %d\n",
	      __FUNCTION__, count, stride);

   if (stride == 16)
      COPY_DWORDS( out, data, count*4 );
   else
      for (i = 0; i < count; i++) {
	 out[0] = *(int *)data;
	 out[1] = *(int *)(data+4);
	 out[2] = *(int *)(data+8);
	 out[3] = *(int *)(data+12);
	 out += 4;
	 data += stride;
      }
}


static void emit_vector( GLcontext *ctx,
			 struct r200_dma_region *rvb,
			 char *data,
			 int size,
			 int stride,
			 int count )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);

   if (R200_DEBUG & DEBUG_VERTS)
      fprintf(stderr, "%s count %d size %d stride %d\n",
	      __FUNCTION__, count, size, stride);

   assert (!rvb->buf);

   if (stride == 0) {
      r200AllocDmaRegion( rmesa, rvb, size * 4, 4 );
      count = 1;
      rvb->aos_start = GET_START(rvb);
      rvb->aos_stride = 0;
      rvb->aos_size = size;
   }
   else {
      r200AllocDmaRegion( rmesa, rvb, size * count * 4, 4 );	/* alignment? */
      rvb->aos_start = GET_START(rvb);
      rvb->aos_stride = size;
      rvb->aos_size = size;
   }

   /* Emit the data
    */
   switch (size) {
   case 2:
      emit_vec8( ctx, rvb, data, stride, count );
      break;
   case 3:
      emit_vec12( ctx, rvb, data, stride, count );
      break;
   case 4:
      emit_vec16( ctx, rvb, data, stride, count );
      break;
   default:
      assert(0);
      exit(1);
      break;
   }

}



/* Emit any changed arrays to new agp memory, re-emit a packet to
 * update the arrays.  
 */
void r200EmitArrays( GLcontext *ctx, GLuint inputs )
{
   r200ContextPtr rmesa = R200_CONTEXT( ctx );
   struct vertex_buffer *VB = &TNL_CONTEXT( ctx )->vb;
   struct r200_dma_region **component = rmesa->tcl.aos_components;
   GLuint nr = 0;
   GLuint vfmt0 = 0, vfmt1 = 0;
   GLuint count = VB->Count;
   
   if (R200_DEBUG & DEBUG_VERTS) 
      _tnl_print_vert_flags( __FUNCTION__, inputs );

   if (1) {
      if (!rmesa->tcl.obj.buf) 
	 emit_vector( ctx, 
		      &rmesa->tcl.obj, 
		      (char *)VB->ObjPtr->data,
		      VB->ObjPtr->size,
		      VB->ObjPtr->stride,
		      count);

      switch( VB->ObjPtr->size ) {
      case 4: vfmt0 |= R200_VTX_W0;
      case 3: vfmt0 |= R200_VTX_Z0;
      case 2: 
      default:
	break;
      }
      component[nr++] = &rmesa->tcl.obj;
   }
   

   if (inputs & VERT_NORM) {
      if (!rmesa->tcl.norm.buf)
	 emit_vector( ctx, 
		      &(rmesa->tcl.norm), 
		      (char *)VB->NormalPtr->data,
		      3,
		      VB->NormalPtr->stride,
		      count);

      vfmt0 |= R200_VTX_N0;
      component[nr++] = &rmesa->tcl.norm;
   }

   if (inputs & VERT_RGBA) {
      if (VB->ColorPtr[0]->Type == GL_UNSIGNED_BYTE) {
	 if (!rmesa->tcl.rgba.buf)
	    emit_ubyte_rgba( ctx, 
			     &rmesa->tcl.rgba, 
			     (char *)VB->ColorPtr[0]->Ptr,
			     VB->ColorPtr[0]->Size,
			     VB->ColorPtr[0]->StrideB,
			     count);

	 vfmt0 |= R200_VTX_PK_RGBA << R200_VTX_COLOR_0_SHIFT; 
      }
      else {
	 int emitsize;

	 if (VB->ColorPtr[0]->Size == 4 &&
	     (VB->ColorPtr[0]->StrideB != 0 ||
	      ((GLfloat *)VB->ColorPtr[0]->Ptr)[3] != 1.0)) { 
	    vfmt0 |= R200_VTX_FP_RGBA << R200_VTX_COLOR_0_SHIFT; 
	    emitsize = 4;
	 }
	 else { 
	    vfmt0 |= R200_VTX_FP_RGB << R200_VTX_COLOR_0_SHIFT; 
	    emitsize = 3;
	 }

	 if (!rmesa->tcl.rgba.buf)
	    emit_vector( ctx, 
			 &(rmesa->tcl.rgba), 
			 (char *)VB->ColorPtr[0]->Ptr,
			 emitsize,
			 VB->ColorPtr[0]->StrideB,
			 count);
      }

      component[nr++] = &rmesa->tcl.rgba;
   }


   if (inputs & VERT_SPEC_RGB) {
      if (!rmesa->tcl.spec.buf) {
	 if (VB->SecondaryColorPtr[0]->Type != GL_UNSIGNED_BYTE)
	    r200_import_float_spec_colors( ctx );

	 emit_ubyte_rgba( ctx, 
			  &rmesa->tcl.spec, 
			  (char *)VB->SecondaryColorPtr[0]->Ptr,
			  3,
			  VB->SecondaryColorPtr[0]->StrideB,
			  count);
      }

      /* How does this work?
       */
      vfmt0 |= R200_VTX_PK_RGBA << R200_VTX_COLOR_1_SHIFT; 
      component[nr++] = &rmesa->tcl.spec;
   }

/*    vtx = (rmesa->hw.tcl.cmd[TCL_OUTPUT_VTXFMT] & */
/* 	  ~(R200_TCL_VTX_Q0|R200_TCL_VTX_Q1)); */
      
   if (inputs & VERT_TEX0) {
      if (!rmesa->tcl.tex[0].buf)
	 emit_vector( ctx, 
		      &(rmesa->tcl.tex[0]), 
		      (char *)VB->TexCoordPtr[0]->data,
		      VB->TexCoordPtr[0]->size,
		      VB->TexCoordPtr[0]->stride,
		      count );

      vfmt1 |= VB->TexCoordPtr[0]->size << R200_VTX_TEX0_COMP_CNT_SHIFT;
      component[nr++] = &rmesa->tcl.tex[0];
   }

   if (inputs & VERT_TEX1) {
      if (!rmesa->tcl.tex[1].buf)
	 emit_vector( ctx, 
		      &(rmesa->tcl.tex[1]), 
		      (char *)VB->TexCoordPtr[1]->data,
		      VB->TexCoordPtr[1]->size,
		      VB->TexCoordPtr[1]->stride,
		      count );
	 
      vfmt1 |= VB->TexCoordPtr[1]->size << R200_VTX_TEX1_COMP_CNT_SHIFT;
      component[nr++] = &rmesa->tcl.tex[1];
   }

   if (vfmt0 != rmesa->hw.vtx.cmd[VTX_VTXFMT_0] ||
       vfmt1 != rmesa->hw.vtx.cmd[VTX_VTXFMT_1]) { 
      R200_STATECHANGE( rmesa, vtx ); 
      rmesa->hw.vtx.cmd[VTX_VTXFMT_0] = vfmt0;
      rmesa->hw.vtx.cmd[VTX_VTXFMT_1] = vfmt1;
   } 


/*    fprintf(stderr, "VTXFMT_0: %x VTXFMT_1: %x\n", vfmt0, vfmt1); */
    
   rmesa->tcl.nr_aos_components = nr;
   rmesa->tcl.vertex_format = vfmt0;
}


void r200ReleaseArrays( GLcontext *ctx, GLuint newinputs )
{
   r200ContextPtr rmesa = R200_CONTEXT( ctx );

   if (R200_DEBUG & DEBUG_VERTS) 
      _tnl_print_vert_flags( __FUNCTION__, newinputs );

   if (newinputs & VERT_OBJ) 
     r200ReleaseDmaRegion( rmesa, &rmesa->tcl.obj, __FUNCTION__ );

   if (newinputs & VERT_NORM) 
      r200ReleaseDmaRegion( rmesa, &rmesa->tcl.norm, __FUNCTION__ );

   if (newinputs & VERT_RGBA) 
      r200ReleaseDmaRegion( rmesa, &rmesa->tcl.rgba, __FUNCTION__ );

   if (newinputs & VERT_SPEC_RGB) 
      r200ReleaseDmaRegion( rmesa, &rmesa->tcl.spec, __FUNCTION__ );

   if (newinputs & VERT_TEX0)
      r200ReleaseDmaRegion( rmesa, &rmesa->tcl.tex[0], __FUNCTION__ );

   if (newinputs & VERT_TEX1)
      r200ReleaseDmaRegion( rmesa, &rmesa->tcl.tex[1], __FUNCTION__ );
}
