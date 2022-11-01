/* $XFree86: xc/lib/GL/mesa/src/drv/r200/r200_tcl.c,v 1.2 2002/12/16 16:18:55 dawes Exp $ */
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
#include "r200_tex.h"
#include "r200_tcl.h"
#include "r200_swtcl.h"
#include "r200_maos.h"

#include "mmath.h"
#include "mtypes.h"
#include "enums.h"
#include "colormac.h"
#include "light.h"

#include "array_cache/acache.h"
#include "tnl/tnl.h"
#include "tnl/t_pipeline.h"



#define HAVE_POINTS      1
#define HAVE_LINES       1
#define HAVE_LINE_LOOP   0
#define HAVE_LINE_STRIPS 1
#define HAVE_TRIANGLES   1
#define HAVE_TRI_STRIPS  1
#define HAVE_TRI_STRIP_1 0
#define HAVE_TRI_FANS    1
#define HAVE_QUADS       0	/* hw quad verts in wrong order??? */
#define HAVE_QUAD_STRIPS 1
#define HAVE_POLYGONS    1
#define HAVE_ELTS        1


#define HW_POINTS           R200_VF_PRIM_POINTS
#define HW_LINES            R200_VF_PRIM_LINES
#define HW_LINE_LOOP        0
#define HW_LINE_STRIP       R200_VF_PRIM_LINE_STRIP
#define HW_TRIANGLES        R200_VF_PRIM_TRIANGLES
#define HW_TRIANGLE_STRIP_0 R200_VF_PRIM_TRIANGLE_STRIP
#define HW_TRIANGLE_STRIP_1 0
#define HW_TRIANGLE_FAN     R200_VF_PRIM_TRIANGLE_FAN
#define HW_QUADS            R200_VF_PRIM_QUADS
#define HW_QUAD_STRIP       R200_VF_PRIM_QUAD_STRIP
#define HW_POLYGON          R200_VF_PRIM_POLYGON


static GLboolean discrete_prim[0x10] = {
   0,				/* 0 none */
   1,				/* 1 points */
   1,				/* 2 lines */
   0,				/* 3 line_strip */
   1,				/* 4 tri_list */
   0,				/* 5 tri_fan */
   0,				/* 6 tri_strip */
   0,				/* 7 tri_w_flags */
   1,				/* 8 rect list (unused) */
   1,				/* 9 3vert point */
   1,				/* a 3vert line */
   0,				/* b point sprite */
   0,				/* c line loop */
   1,				/* d quads */
   0,				/* e quad strip */
   0,				/* f polygon */
};
   

#define LOCAL_VARS r200ContextPtr rmesa = R200_CONTEXT(ctx)
#define ELTS_VARS  GLushort *dest

#define ELT_INIT(prim, hw_prim) \
   r200TclPrimitive( ctx, prim, hw_prim | R200_VF_PRIM_WALK_IND )

#define GET_ELTS() rmesa->tcl.Elts


#define NEW_PRIMITIVE()  R200_NEWPRIM( rmesa )
#define NEW_BUFFER()  r200RefillCurrentDmaRegion( rmesa )

/* Don't really know how many elts will fit in what's left of cmdbuf,
 * as there is state to emit, etc:
 */

#if 0
#define GET_CURRENT_VB_MAX_ELTS() \
   ((R200_CMD_BUF_SZ - (rmesa->store.cmd_used + 16)) / 2) 
#define GET_SUBSEQUENT_VB_MAX_ELTS() ((R200_CMD_BUF_SZ - 16) / 2) 
#else
/* Testing on isosurf shows a maximum around here.  Don't know if it's
 * the card or driver or kernel module that is causing the behaviour.
 */
#define GET_CURRENT_VB_MAX_ELTS() 300
#define GET_SUBSEQUENT_VB_MAX_ELTS() 300
#endif

#define RESET_STIPPLE() do {			\
   R200_STATECHANGE( rmesa, lin );		\
   r200EmitState( rmesa );			\
} while (0)

#define AUTO_STIPPLE( mode )  do {		\
   R200_STATECHANGE( rmesa, lin );		\
   if (mode)					\
      rmesa->hw.lin.cmd[LIN_RE_LINE_PATTERN] |=	\
	 R200_LINE_PATTERN_AUTO_RESET;	\
   else						\
      rmesa->hw.lin.cmd[LIN_RE_LINE_PATTERN] &=	\
	 ~R200_LINE_PATTERN_AUTO_RESET;	\
   r200EmitState( rmesa );			\
} while (0)


/* How do you extend an existing primitive?
 */
#define ALLOC_ELTS(nr)							\
do {									\
   if (rmesa->dma.flush == r200FlushElts &&				\
       rmesa->store.cmd_used + nr*2 < R200_CMD_BUF_SZ) {		\
									\
      dest = (GLushort *)(rmesa->store.cmd_buf + 			\
			  rmesa->store.cmd_used);			\
      rmesa->store.cmd_used += nr*2;					\
   }									\
   else {								\
      if (rmesa->dma.flush)						\
	 rmesa->dma.flush( rmesa );					\
									\
      r200EmitAOS( rmesa,						\
	  	     rmesa->tcl.aos_components,				\
		     rmesa->tcl.nr_aos_components,			\
		     0 );						\
									\
      dest = r200AllocEltsOpenEnded( rmesa,				\
				       rmesa->tcl.hw_primitive,		\
				       nr );				\
   }									\
} while (0) 



/* TODO: Try to extend existing primitive if both are identical,
 * discrete and there are no intervening state changes.  (Somewhat
 * duplicates changes to DrawArrays code)
 */
static void EMIT_PRIM( GLcontext *ctx, 
		       GLenum prim, 
		       GLuint hwprim, 
		       GLuint start, 
		       GLuint count)	
{
   r200ContextPtr rmesa = R200_CONTEXT( ctx );
   r200TclPrimitive( ctx, prim, hwprim );
   
   r200EmitAOS( rmesa,
		  rmesa->tcl.aos_components,
		  rmesa->tcl.nr_aos_components,
		  start );
   
   /* Why couldn't this packet have taken an offset param?
    */
   r200EmitVbufPrim( rmesa,
		     rmesa->tcl.hw_primitive,
		     count - start );
}



/* Try & join small primitives
 */
#if 0
#define PREFER_DISCRETE_ELT_PRIM( NR, PRIM ) 0
#else
#define PREFER_DISCRETE_ELT_PRIM( NR, PRIM )			\
  ((NR) < 20 ||							\
   ((NR) < 40 &&						\
    rmesa->tcl.hw_primitive == (PRIM|				\
			    R200_VF_TCL_OUTPUT_VTX_ENABLE|	\
			        R200_VF_PRIM_WALK_IND)))
#endif

#ifdef MESA_BIG_ENDIAN
/* We could do without (most of) this ugliness if dest was always 32 bit word aligned... */
#define EMIT_ELT(offset, x) do {                                \
        int off = offset + ( ( (GLuint)dest & 0x2 ) >> 1 );     \
        GLushort *des = (GLushort *)( (GLuint)dest & ~0x2 );    \
        (des)[ off + 1 - 2 * ( off & 1 ) ] = (GLushort)(x); } while (0)
#else
#define EMIT_ELT(offset, x) (dest)[offset] = (GLushort) (x)
#endif
#define EMIT_TWO_ELTS(offset, x, y)  *(GLuint *)(dest+offset) = ((y)<<16)|(x);
#define INCR_ELTS( nr ) dest += nr
#define RELEASE_ELT_VERTS() \
   r200ReleaseArrays( ctx, ~0 )



#define TAG(x) tcl_##x
#include "tnl_dd/t_dd_dmatmp2.h"

/**********************************************************************/
/*                          External entrypoints                     */
/**********************************************************************/

void r200EmitPrimitive( GLcontext *ctx, 
			  GLuint first,
			  GLuint last,
			  GLuint flags )
{
   tcl_render_tab_verts[flags&PRIM_MODE_MASK]( ctx, first, last, flags );
}

void r200EmitEltPrimitive( GLcontext *ctx, 
			     GLuint first,
			     GLuint last,
			     GLuint flags )
{
   tcl_render_tab_elts[flags&PRIM_MODE_MASK]( ctx, first, last, flags );
}

void r200TclPrimitive( GLcontext *ctx, 
			 GLenum prim,
			 int hw_prim )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   GLuint newprim = hw_prim | R200_VF_TCL_OUTPUT_VTX_ENABLE;

   if (newprim != rmesa->tcl.hw_primitive ||
       !discrete_prim[hw_prim&0xf]) {
      R200_NEWPRIM( rmesa );
      rmesa->tcl.hw_primitive = newprim;
   }
}


/**********************************************************************/
/*                          Render pipeline stage                     */
/**********************************************************************/


/* TCL render.
 */
static GLboolean r200_run_tcl_render( GLcontext *ctx,
					struct gl_pipeline_stage *stage )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   GLuint i,flags = 0,length;

   /* TODO: separate this from the swtnl pipeline 
    */
   if (rmesa->TclFallback)
      return GL_TRUE;	/* fallback to software t&l */

   if (R200_DEBUG & DEBUG_PRIMS)
      fprintf(stderr, "%s\n", __FUNCTION__);

   if (VB->Count == 0)
      return GL_FALSE;

   r200ReleaseArrays( ctx, stage->changed_inputs );
   r200EmitArrays( ctx, stage->inputs );

   rmesa->tcl.Elts = VB->Elts;

   for (i = VB->FirstPrimitive ; !(flags & PRIM_LAST) ; i += length)
   {
      flags = VB->Primitive[i];
      length = VB->PrimitiveLength[i];

      if (R200_DEBUG & DEBUG_PRIMS)
	 fprintf(stderr, "%s: prim %s %d..%d\n", 
		 __FUNCTION__,
		 _mesa_lookup_enum_by_nr(flags & PRIM_MODE_MASK), 
		 i, i+length);

      if (!length)
	 continue;

      if (rmesa->tcl.Elts)
	 r200EmitEltPrimitive( ctx, i, i+length, flags );
      else
	 r200EmitPrimitive( ctx, i, i+length, flags );
   }

   return GL_FALSE;		/* finished the pipe */
}



static void r200_check_tcl_render( GLcontext *ctx,
				     struct gl_pipeline_stage *stage )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   GLuint inputs = VERT_OBJ;

   /* Validate state:
    */
   if (rmesa->NewGLState)
      r200ValidateState( ctx );

   if (0)
      fprintf(stderr, "%s: RE %d TGE %d NN %d\n",
	   __FUNCTION__,
	   ctx->Texture.Unit[0]._ReallyEnabled,
	   ctx->Texture.Unit[0].TexGenEnabled,
	   rmesa->TexGenNeedNormals[0]);

   if (ctx->RenderMode == GL_RENDER) {
      /* Make all this event-driven:
       */
      if (ctx->Light.Enabled) {
	 inputs |= VERT_NORM;

	 if (ctx->Light.ColorMaterialEnabled) {
	    inputs |= VERT_RGBA;
	 }
      }
      else {
	 inputs |= VERT_RGBA;
	 
	 if (ctx->_TriangleCaps & DD_SEPARATE_SPECULAR) {
	    inputs |= VERT_SPEC_RGB;
	 }
      }

      if (ctx->Texture.Unit[0]._ReallyEnabled) {
	 if (ctx->Texture.Unit[0].TexGenEnabled) {
	    if (rmesa->TexGenNeedNormals[0]) {
	       inputs |= VERT_NORM;
	    }
	 } else {
	    inputs |= VERT_TEX(0);
	 }
      }

      if (ctx->Texture.Unit[1]._ReallyEnabled) {
	 if (ctx->Texture.Unit[1].TexGenEnabled) {
	    if (rmesa->TexGenNeedNormals[1]) {
	       inputs |= VERT_NORM;
	    }
	 } else {
	    inputs |= VERT_TEX(1);
	 }
      }

      stage->inputs = inputs;
      stage->active = 1;
   }
   else
      stage->active = 0;
}

static void r200_init_tcl_render( GLcontext *ctx,
				    struct gl_pipeline_stage *stage )
{
   stage->check = r200_check_tcl_render;
   stage->check( ctx, stage );
}

static void dtr( struct gl_pipeline_stage *stage )
{
   (void)stage;
}


/* Initial state for tcl stage.  
 */
const struct gl_pipeline_stage _r200_tcl_stage =
{
   "r200 render",
   (_DD_NEW_SEPARATE_SPECULAR |
    _NEW_LIGHT|
    _NEW_TEXTURE|
    _NEW_FOG|
    _NEW_RENDERMODE),		/* re-check (new inputs) */
   0,				/* re-run (always runs) */
   GL_TRUE,			/* active */
   0, 0,			/* inputs (set in check_render), outputs */
   0, 0,			/* changed_inputs, private */
   dtr,				/* destructor */
   r200_init_tcl_render,	/* check - initially set to alloc data */
   r200_run_tcl_render	/* run */
};



/**********************************************************************/
/*                 Validate state at pipeline start                   */
/**********************************************************************/


/*-----------------------------------------------------------------------
 * Manage TCL fallbacks
 */


static void transition_to_swtnl( GLcontext *ctx )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   TNLcontext *tnl = TNL_CONTEXT(ctx);

   R200_NEWPRIM( rmesa );
   rmesa->swtcl.vertex_format = 0;

   r200ChooseVertexState( ctx );
   r200ChooseRenderState( ctx );

   _mesa_validate_all_lighting_tables( ctx ); 

   tnl->Driver.NotifyMaterialChange = 
      _mesa_validate_all_lighting_tables;

   r200ReleaseArrays( ctx, ~0 );

   /* Still using the D3D based hardware-rasterizer from the radeon;
    * need to put the card into D3D mode to make it work:
    */
   R200_STATECHANGE( rmesa, vap );
   rmesa->hw.vap.cmd[VAP_SE_VAP_CNTL] &= ~R200_VAP_TCL_ENABLE;
   rmesa->hw.vap.cmd[VAP_SE_VAP_CNTL] |= R200_VAP_D3D_TEX_DEFAULT;

   R200_STATECHANGE( rmesa, vte );
   rmesa->hw.vte.cmd[VTE_SE_VTE_CNTL] &= ~R200_VTX_W0_FMT;

   R200_STATECHANGE( rmesa, set );
   rmesa->hw.set.cmd[SET_RE_CNTL] |= (R200_VTX_STQ0_D3D |
				      R200_VTX_STQ1_D3D);
}


static void transition_to_hwtnl( GLcontext *ctx )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   TNLcontext *tnl = TNL_CONTEXT(ctx);

   _tnl_need_projected_coords( ctx, GL_FALSE );

   r200UpdateMaterial( ctx );

   tnl->Driver.NotifyMaterialChange = r200UpdateMaterial;

   if ( rmesa->dma.flush )			
      rmesa->dma.flush( rmesa );	

   rmesa->dma.flush = 0;
   rmesa->swtcl.vertex_format = 0;
   
   if (rmesa->swtcl.indexed_verts.buf) 
      r200ReleaseDmaRegion( rmesa, &rmesa->swtcl.indexed_verts, 
			      __FUNCTION__ );

   R200_STATECHANGE( rmesa, vap );
   rmesa->hw.vap.cmd[VAP_SE_VAP_CNTL] |= R200_VAP_TCL_ENABLE;
   rmesa->hw.vap.cmd[VAP_SE_VAP_CNTL] &= ~(R200_VAP_FORCE_W_TO_ONE |
					   R200_VAP_D3D_TEX_DEFAULT);

   R200_STATECHANGE( rmesa, vte );
   rmesa->hw.vte.cmd[VTE_SE_VTE_CNTL] &= ~(R200_VTX_XY_FMT|R200_VTX_Z_FMT);
   rmesa->hw.vte.cmd[VTE_SE_VTE_CNTL] |= R200_VTX_W0_FMT;

   R200_STATECHANGE( rmesa, set );
   rmesa->hw.set.cmd[SET_RE_CNTL] &= ~(R200_VTX_STQ0_D3D |
				       R200_VTX_STQ1_D3D);


   if (R200_DEBUG & DEBUG_FALLBACKS) 
      fprintf(stderr, "R200 end tcl fallback\n");
}


static char *fallbackStrings[] = {
   "Rasterization fallback",
   "Unfilled triangles",
   "Twosided lighting, differing materials",
   "Materials in VB (maybe between begin/end)",
   "Texgen unit 0",
   "Texgen unit 1",
   "Texgen unit 2",
   "User disable"
};


static char *getFallbackString(GLuint bit)
{
   int i = 0;
   while (bit > 1) {
      i++;
      bit >>= 1;
   }
   return fallbackStrings[i];
}



void r200TclFallback( GLcontext *ctx, GLuint bit, GLboolean mode )
{
   r200ContextPtr rmesa = R200_CONTEXT(ctx);
   GLuint oldfallback = rmesa->TclFallback;

   if (mode) {
      rmesa->TclFallback |= bit;
      if (oldfallback == 0) {
	 if (R200_DEBUG & DEBUG_FALLBACKS) 
	    fprintf(stderr, "R200 begin tcl fallback %s\n",
		    getFallbackString( bit ));
	 transition_to_swtnl( ctx );
      }
   }
   else {
      rmesa->TclFallback &= ~bit;
      if (oldfallback == bit) {
	 if (R200_DEBUG & DEBUG_FALLBACKS) 
	    fprintf(stderr, "R200 end tcl fallback %s\n",
		    getFallbackString( bit ));
	 transition_to_hwtnl( ctx );
      }
   }
}
