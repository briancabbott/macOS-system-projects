/* $XFree86: xc/lib/GL/mesa/src/drv/radeon/radeon_maos_verts.c,v 1.1 2002/10/30 12:51:55 alanh Exp $ */
/**************************************************************************

Copyright 2000, 2001 ATI Technologies Inc., Ontario, Canada, and
                     Tungsten Graphics Inc., Austin, Texas.

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
ATI, TUNGSTEN GRAPHICS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Keith Whitwell <keith@tungstengraphics.com>
 *
 */

#include "radeon_context.h"
#include "radeon_state.h"
#include "radeon_ioctl.h"
#include "radeon_tex.h"
#include "radeon_tcl.h"
#include "radeon_swtcl.h"
#include "radeon_maos.h"

#include "mmath.h"
#include "mtypes.h"
#include "enums.h"
#include "colormac.h"
#include "light.h"

#include "array_cache/acache.h"
#include "tnl/tnl.h"
#include "tnl/t_pipeline.h"
#include "tnl/t_imm_debug.h"

#define RADEON_TCL_MAX_SETUP 13

union emit_union { float f; GLuint ui; radeon_color_t specular; };

static struct {
   void   (*emit)( GLcontext *, GLuint, GLuint, void * );
   GLuint vertex_size;
   GLuint vertex_format;
} setup_tab[RADEON_TCL_MAX_SETUP];

#define DO_W    (IND & RADEON_CP_VC_FRMT_W0)
#define DO_RGBA (IND & RADEON_CP_VC_FRMT_PKCOLOR)
#define DO_SPEC (IND & RADEON_CP_VC_FRMT_PKSPEC)
#define DO_FOG  (IND & RADEON_CP_VC_FRMT_PKSPEC)
#define DO_TEX0 (IND & RADEON_CP_VC_FRMT_ST0)
#define DO_TEX1 (IND & RADEON_CP_VC_FRMT_ST1)
#define DO_PTEX (IND & RADEON_CP_VC_FRMT_Q0)
#define DO_NORM (IND & RADEON_CP_VC_FRMT_N0)

#define DO_TEX2 0
#define DO_TEX3 0

#define GET_TEXSOURCE(n)  n
#define GET_UBYTE_COLOR_STORE() &RADEON_CONTEXT(ctx)->UbyteColor
#define GET_UBYTE_SPEC_COLOR_STORE() &RADEON_CONTEXT(ctx)->UbyteSecondaryColor

#define IMPORT_FLOAT_COLORS radeon_import_float_colors
#define IMPORT_FLOAT_SPEC_COLORS radeon_import_float_spec_colors

/***********************************************************************
 *             Generate vertex emit functions               *
 ***********************************************************************/


/* Defined in order of increasing vertex size:
 */
#define IDX 0
#define IND (RADEON_CP_VC_FRMT_XY|		\
	     RADEON_CP_VC_FRMT_Z|		\
	     RADEON_CP_VC_FRMT_PKCOLOR)
#define TAG(x) x##_rgba
#include "radeon_maos_vbtmp.h"

#define IDX 1
#define IND (RADEON_CP_VC_FRMT_XY|		\
	     RADEON_CP_VC_FRMT_Z|		\
	     RADEON_CP_VC_FRMT_N0)
#define TAG(x) x##_n
#include "radeon_maos_vbtmp.h"

#define IDX 2
#define IND (RADEON_CP_VC_FRMT_XY|		\
	     RADEON_CP_VC_FRMT_Z|		\
	     RADEON_CP_VC_FRMT_PKCOLOR|		\
	     RADEON_CP_VC_FRMT_ST0)
#define TAG(x) x##_rgba_st
#include "radeon_maos_vbtmp.h"

#define IDX 3
#define IND (RADEON_CP_VC_FRMT_XY|		\
	     RADEON_CP_VC_FRMT_Z|		\
	     RADEON_CP_VC_FRMT_PKCOLOR|		\
	     RADEON_CP_VC_FRMT_N0)
#define TAG(x) x##_rgba_n
#include "radeon_maos_vbtmp.h"

#define IDX 4
#define IND (RADEON_CP_VC_FRMT_XY|		\
	     RADEON_CP_VC_FRMT_Z|		\
	     RADEON_CP_VC_FRMT_ST0|		\
	     RADEON_CP_VC_FRMT_N0)
#define TAG(x) x##_st_n
#include "radeon_maos_vbtmp.h"

#define IDX 5
#define IND (RADEON_CP_VC_FRMT_XY|		\
	     RADEON_CP_VC_FRMT_Z|		\
	     RADEON_CP_VC_FRMT_PKCOLOR|		\
	     RADEON_CP_VC_FRMT_ST0|		\
	     RADEON_CP_VC_FRMT_ST1)
#define TAG(x) x##_rgba_st_st
#include "radeon_maos_vbtmp.h"

#define IDX 6
#define IND (RADEON_CP_VC_FRMT_XY|		\
	     RADEON_CP_VC_FRMT_Z|		\
	     RADEON_CP_VC_FRMT_PKCOLOR|		\
	     RADEON_CP_VC_FRMT_ST0|		\
	     RADEON_CP_VC_FRMT_N0)
#define TAG(x) x##_rgba_st_n
#include "radeon_maos_vbtmp.h"

#define IDX 7
#define IND (RADEON_CP_VC_FRMT_XY|		\
	     RADEON_CP_VC_FRMT_Z|		\
	     RADEON_CP_VC_FRMT_PKCOLOR|		\
	     RADEON_CP_VC_FRMT_PKSPEC|		\
	     RADEON_CP_VC_FRMT_ST0|		\
	     RADEON_CP_VC_FRMT_ST1)
#define TAG(x) x##_rgba_spec_st_st
#include "radeon_maos_vbtmp.h"

#define IDX 8
#define IND (RADEON_CP_VC_FRMT_XY|		\
	     RADEON_CP_VC_FRMT_Z|		\
	     RADEON_CP_VC_FRMT_ST0|		\
	     RADEON_CP_VC_FRMT_ST1|		\
	     RADEON_CP_VC_FRMT_N0)
#define TAG(x) x##_st_st_n
#include "radeon_maos_vbtmp.h"

#define IDX 9
#define IND (RADEON_CP_VC_FRMT_XY|		\
	     RADEON_CP_VC_FRMT_Z|		\
	     RADEON_CP_VC_FRMT_PKCOLOR|		\
	     RADEON_CP_VC_FRMT_PKSPEC|		\
	     RADEON_CP_VC_FRMT_ST0|		\
	     RADEON_CP_VC_FRMT_ST1|		\
	     RADEON_CP_VC_FRMT_N0)
#define TAG(x) x##_rgpa_spec_st_st_n
#include "radeon_maos_vbtmp.h"

#define IDX 10
#define IND (RADEON_CP_VC_FRMT_XY|		\
	     RADEON_CP_VC_FRMT_Z|		\
	     RADEON_CP_VC_FRMT_PKCOLOR|		\
	     RADEON_CP_VC_FRMT_ST0|		\
	     RADEON_CP_VC_FRMT_Q0)
#define TAG(x) x##_rgba_stq
#include "radeon_maos_vbtmp.h"

#define IDX 11
#define IND (RADEON_CP_VC_FRMT_XY|		\
	     RADEON_CP_VC_FRMT_Z|		\
	     RADEON_CP_VC_FRMT_PKCOLOR|		\
	     RADEON_CP_VC_FRMT_ST1|		\
	     RADEON_CP_VC_FRMT_Q1|		\
	     RADEON_CP_VC_FRMT_ST0|		\
	     RADEON_CP_VC_FRMT_Q0)
#define TAG(x) x##_rgba_stq_stq
#include "radeon_maos_vbtmp.h"

#define IDX 12
#define IND (RADEON_CP_VC_FRMT_XY|		\
	     RADEON_CP_VC_FRMT_Z|		\
	     RADEON_CP_VC_FRMT_W0|		\
	     RADEON_CP_VC_FRMT_PKCOLOR|		\
	     RADEON_CP_VC_FRMT_PKSPEC|		\
	     RADEON_CP_VC_FRMT_ST0|		\
	     RADEON_CP_VC_FRMT_Q0|		\
	     RADEON_CP_VC_FRMT_ST1|		\
	     RADEON_CP_VC_FRMT_Q1|		\
	     RADEON_CP_VC_FRMT_N0)
#define TAG(x) x##_w_rgpa_spec_stq_stq_n
#include "radeon_maos_vbtmp.h"





/***********************************************************************
 *                         Initialization 
 ***********************************************************************/


static void init_tcl_verts( void )
{
   init_rgba();
   init_n();
   init_rgba_n();
   init_rgba_st();
   init_st_n();
   init_rgba_st_st();
   init_rgba_st_n();
   init_rgba_spec_st_st();
   init_st_st_n();
   init_rgpa_spec_st_st_n();
   init_rgba_stq();
   init_rgba_stq_stq();
   init_w_rgpa_spec_stq_stq_n();
}


void radeonEmitArrays( GLcontext *ctx, GLuint inputs )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   GLuint req = 0;
   GLuint vtx = (rmesa->hw.tcl.cmd[TCL_OUTPUT_VTXFMT] &
		 ~(RADEON_TCL_VTX_Q0|RADEON_TCL_VTX_Q1));
   int i;
   static int firsttime = 1;

   if (firsttime) {
      init_tcl_verts();
      firsttime = 0;
   }
		     
   if (1) {
      req |= RADEON_CP_VC_FRMT_Z;
      if (VB->ObjPtr->size == 4) {
	 req |= RADEON_CP_VC_FRMT_W0;
      }
   }

   if (inputs & VERT_NORM) {
      req |= RADEON_CP_VC_FRMT_N0;
   }
   
   if (inputs & VERT_RGBA) {
      req |= RADEON_CP_VC_FRMT_PKCOLOR;
   }

   if (inputs & VERT_SPEC_RGB) {
      req |= RADEON_CP_VC_FRMT_PKSPEC;
   }

   if (inputs & VERT_TEX0) {
      req |= RADEON_CP_VC_FRMT_ST0;

      if (VB->TexCoordPtr[0]->size == 4) {
	 req |= RADEON_CP_VC_FRMT_Q0;
	 vtx |= RADEON_TCL_VTX_Q0;
      }
   }

   if (inputs & VERT_TEX1) {
      req |= RADEON_CP_VC_FRMT_ST1;

      if (VB->TexCoordPtr[1]->size == 4) {
	 req |= RADEON_CP_VC_FRMT_Q1;
	 vtx |= RADEON_TCL_VTX_Q1;
      }
   }

   if (vtx != rmesa->hw.tcl.cmd[TCL_OUTPUT_VTXFMT]) {
      RADEON_STATECHANGE( rmesa, tcl );
      rmesa->hw.tcl.cmd[TCL_OUTPUT_VTXFMT] = vtx;
   }

   for (i = 0 ; i < RADEON_TCL_MAX_SETUP ; i++) 
      if ((setup_tab[i].vertex_format & req) == req) 
	 break;

   if (rmesa->tcl.vertex_format == setup_tab[i].vertex_format &&
       rmesa->tcl.indexed_verts.buf)
      return;

   if (rmesa->tcl.indexed_verts.buf)
      radeonReleaseArrays( ctx, ~0 );

   radeonAllocDmaRegionVerts( rmesa, 
			      &rmesa->tcl.indexed_verts, 
			      VB->Count,
			      setup_tab[i].vertex_size * 4, 
			      4);

   setup_tab[i].emit( ctx, 0, VB->Count, 
		      rmesa->tcl.indexed_verts.address + 
		      rmesa->tcl.indexed_verts.start );

   rmesa->tcl.vertex_format = setup_tab[i].vertex_format;
   rmesa->tcl.indexed_verts.aos_start = GET_START( &rmesa->tcl.indexed_verts );
   rmesa->tcl.indexed_verts.aos_size = setup_tab[i].vertex_size;
   rmesa->tcl.indexed_verts.aos_stride = setup_tab[i].vertex_size;

   rmesa->tcl.aos_components[0] = &rmesa->tcl.indexed_verts;
   rmesa->tcl.nr_aos_components = 1;
}



void radeonReleaseArrays( GLcontext *ctx, GLuint newinputs )
{
   radeonContextPtr rmesa = RADEON_CONTEXT( ctx );

   if (RADEON_DEBUG & DEBUG_VERTS) 
      _tnl_print_vert_flags( __FUNCTION__, newinputs );

   if (newinputs) 
     radeonReleaseDmaRegion( rmesa, &rmesa->tcl.indexed_verts, __FUNCTION__ );
}
