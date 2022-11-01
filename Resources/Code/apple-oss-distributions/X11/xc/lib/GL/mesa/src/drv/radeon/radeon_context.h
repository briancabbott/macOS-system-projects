/* $XFree86: xc/lib/GL/mesa/src/drv/radeon/radeon_context.h,v 1.6 2002/12/16 16:18:58 dawes Exp $ */
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
 *   Keith Whitwell <keith@tungstengraphics.com>
 */

#ifndef __RADEON_CONTEXT_H__
#define __RADEON_CONTEXT_H__

#ifdef GLX_DIRECT_RENDERING

struct radeon_context;
typedef struct radeon_context radeonContextRec;
typedef struct radeon_context *radeonContextPtr;

#include "mtypes.h"
#include "radeon_lock.h"
#include "radeon_screen.h"
#include "mm.h"

/* Flags for software fallback cases */
/* See correponding strings in radeon_swtcl.c */
#define RADEON_FALLBACK_TEXTURE		0x0001
#define RADEON_FALLBACK_DRAW_BUFFER	0x0002
#define RADEON_FALLBACK_STENCIL		0x0004
#define RADEON_FALLBACK_RENDER_MODE	0x0008
#define RADEON_FALLBACK_BLEND_EQ	0x0010
#define RADEON_FALLBACK_BLEND_FUNC	0x0020
#define RADEON_FALLBACK_DISABLE 	0x0040

/* Use the templated vertex format:
 */
#define COLOR_IS_RGBA
#define TAG(x) radeon##x
#include "tnl_dd/t_dd_vertex.h"
#undef TAG

typedef void (*radeon_tri_func)( radeonContextPtr,
				 radeonVertex *,
				 radeonVertex *,
				 radeonVertex * );

typedef void (*radeon_line_func)( radeonContextPtr,
				  radeonVertex *,
				  radeonVertex * );

typedef void (*radeon_point_func)( radeonContextPtr,
				   radeonVertex * );


struct radeon_colorbuffer_state {
   GLuint clear;
   GLint drawOffset, drawPitch;
};


struct radeon_depthbuffer_state {
   GLuint clear;
   GLfloat scale;
};

struct radeon_pixel_state {
   GLint readOffset, readPitch;
};

struct radeon_scissor_state {
   XF86DRIClipRectRec rect;
   GLboolean enabled;

   GLuint numClipRects;			/* Cliprects active */
   GLuint numAllocedClipRects;		/* Cliprects available */
   XF86DRIClipRectPtr pClipRects;
};

struct radeon_stencilbuffer_state {
   GLboolean hwBuffer;
   GLuint clear;			/* rb3d_stencilrefmask value */
};

struct radeon_stipple_state {
   GLuint mask[32];
};



#define TEX_0   0x1
#define TEX_1   0x2
#define TEX_ALL 0x3

typedef struct radeon_tex_obj radeonTexObj, *radeonTexObjPtr;

/* Texture object in locally shared texture space.
 */
struct radeon_tex_obj {
   radeonTexObjPtr next, prev;

   struct gl_texture_object *tObj;	/* Mesa texture object */

   PMemBlock memBlock;			/* Memory block containing texture */
   GLuint bufAddr;			/* Offset to start of locally
					   shared texture block */

   GLuint dirty_images;			/* Flags for whether or not
					   images need to be uploaded to
					   local or AGP texture space */

   GLuint dirty_state;		        /* Flags (1 per texunit) for
					   whether or not this texobj
					   has dirty hardware state
					   (pp_*) that needs to be
					   brought into the
					   texunit. */

   GLint heap;				/* Texture heap currently stored in */

   drmRadeonTexImage image[RADEON_MAX_TEXTURE_LEVELS];

   GLint totalSize;			/* Total size of the texture
					   including all mipmap levels */

   GLuint pp_txfilter;		        /* hardware register values */
   GLuint pp_txformat;
   GLuint pp_txoffset;
   GLuint pp_border_color;

   /* texObj->Image[firstLevel] through texObj->Image[lastLevel] are the
    * images to upload.
    */
   GLint firstLevel;     
   GLint lastLevel;      
};


struct radeon_texture_env_state {
   radeonTexObjPtr texobj;
   GLenum format;
   GLenum envMode;
};

struct radeon_texture_state {
   struct radeon_texture_env_state unit[RADEON_MAX_TEXTURE_UNITS];
};


struct radeon_state_atom {
   struct radeon_state_atom *next, *prev;
   const char *name;		         /* for debug */
   int cmd_size;		         /* size in bytes */
   GLuint is_tcl;
   int *cmd;			         /* one or more cmd's */
   int *lastcmd;			 /* one or more cmd's */
   GLboolean (*check)( GLcontext * );    /* is this state active? */
};
   


/* Trying to keep these relatively short as the variables are becoming
 * extravagently long.  Drop the RADEON_ off the front of everything -
 * I think we know we're in the radeon driver by now, and keep the
 * prefix to 3 letters unless absolutely impossible.  
 */

#define CTX_CMD_0             0
#define CTX_PP_MISC           1
#define CTX_PP_FOG_COLOR      2
#define CTX_RE_SOLID_COLOR    3
#define CTX_RB3D_BLENDCNTL    4
#define CTX_RB3D_DEPTHOFFSET  5
#define CTX_RB3D_DEPTHPITCH   6
#define CTX_RB3D_ZSTENCILCNTL 7
#define CTX_CMD_1             8
#define CTX_PP_CNTL           9
#define CTX_RB3D_CNTL         10
#define CTX_RB3D_COLOROFFSET  11
#define CTX_CMD_2             12
#define CTX_RB3D_COLORPITCH   13
#define CTX_STATE_SIZE        14

#define SET_CMD_0               0
#define SET_SE_CNTL             1
#define SET_SE_COORDFMT         2
#define SET_CMD_1               3
#define SET_SE_CNTL_STATUS      4
#define SET_STATE_SIZE          5

#define LIN_CMD_0               0
#define LIN_RE_LINE_PATTERN     1
#define LIN_RE_LINE_STATE       2
#define LIN_CMD_1               3
#define LIN_SE_LINE_WIDTH       4
#define LIN_STATE_SIZE          5

#define MSK_CMD_0               0
#define MSK_RB3D_STENCILREFMASK 1
#define MSK_RB3D_ROPCNTL        2
#define MSK_RB3D_PLANEMASK      3
#define MSK_STATE_SIZE          4

#define VPT_CMD_0           0
#define VPT_SE_VPORT_XSCALE          1
#define VPT_SE_VPORT_XOFFSET         2
#define VPT_SE_VPORT_YSCALE          3
#define VPT_SE_VPORT_YOFFSET         4
#define VPT_SE_VPORT_ZSCALE          5
#define VPT_SE_VPORT_ZOFFSET         6
#define VPT_STATE_SIZE      7

#define MSC_CMD_0               0
#define MSC_RE_MISC             1
#define MSC_STATE_SIZE          2

#define TEX_CMD_0                   0
#define TEX_PP_TXFILTER             1
#define TEX_PP_TXFORMAT             2
#define TEX_PP_TXOFFSET             3
#define TEX_PP_TXCBLEND             4
#define TEX_PP_TXABLEND             5
#define TEX_PP_TFACTOR              6
#define TEX_CMD_1                   7
#define TEX_PP_BORDER_COLOR         8
#define TEX_STATE_SIZE              9

#define ZBS_CMD_0              0
#define ZBS_SE_ZBIAS_FACTOR             1
#define ZBS_SE_ZBIAS_CONSTANT           2
#define ZBS_STATE_SIZE         3

#define TCL_CMD_0                        0
#define TCL_OUTPUT_VTXFMT         1
#define TCL_OUTPUT_VTXSEL         2
#define TCL_MATRIX_SELECT_0       3
#define TCL_MATRIX_SELECT_1       4
#define TCL_UCP_VERT_BLEND_CTL    5
#define TCL_TEXTURE_PROC_CTL      6
#define TCL_LIGHT_MODEL_CTL       7
#define TCL_PER_LIGHT_CTL_0       8
#define TCL_PER_LIGHT_CTL_1       9
#define TCL_PER_LIGHT_CTL_2       10
#define TCL_PER_LIGHT_CTL_3       11
#define TCL_STATE_SIZE                   12

#define MTL_CMD_0            0	
#define MTL_EMMISSIVE_RED    1	
#define MTL_EMMISSIVE_GREEN  2	
#define MTL_EMMISSIVE_BLUE   3	
#define MTL_EMMISSIVE_ALPHA  4	
#define MTL_AMBIENT_RED      5
#define MTL_AMBIENT_GREEN    6
#define MTL_AMBIENT_BLUE     7
#define MTL_AMBIENT_ALPHA    8
#define MTL_DIFFUSE_RED      9
#define MTL_DIFFUSE_GREEN    10
#define MTL_DIFFUSE_BLUE     11
#define MTL_DIFFUSE_ALPHA    12
#define MTL_SPECULAR_RED     13
#define MTL_SPECULAR_GREEN   14
#define MTL_SPECULAR_BLUE    15
#define MTL_SPECULAR_ALPHA   16
#define MTL_SHININESS        17
#define MTL_STATE_SIZE       18

#define VTX_CMD_0              0
#define VTX_SE_COORD_FMT       1
#define VTX_STATE_SIZE         2

#define MAT_CMD_0              0
#define MAT_ELT_0              1
#define MAT_STATE_SIZE         17

#define GRD_CMD_0                  0
#define GRD_VERT_GUARD_CLIP_ADJ    1
#define GRD_VERT_GUARD_DISCARD_ADJ 2
#define GRD_HORZ_GUARD_CLIP_ADJ    3
#define GRD_HORZ_GUARD_DISCARD_ADJ 4
#define GRD_STATE_SIZE             5

/* position changes frequently when lighting in modelpos - separate
 * out to new state item?  
 */
#define LIT_CMD_0                  0
#define LIT_AMBIENT_RED            1
#define LIT_AMBIENT_GREEN          2
#define LIT_AMBIENT_BLUE           3
#define LIT_AMBIENT_ALPHA          4
#define LIT_DIFFUSE_RED            5
#define LIT_DIFFUSE_GREEN          6
#define LIT_DIFFUSE_BLUE           7
#define LIT_DIFFUSE_ALPHA          8
#define LIT_SPECULAR_RED           9
#define LIT_SPECULAR_GREEN         10
#define LIT_SPECULAR_BLUE          11
#define LIT_SPECULAR_ALPHA         12
#define LIT_POSITION_X             13
#define LIT_POSITION_Y             14
#define LIT_POSITION_Z             15
#define LIT_POSITION_W             16
#define LIT_DIRECTION_X            17
#define LIT_DIRECTION_Y            18
#define LIT_DIRECTION_Z            19
#define LIT_DIRECTION_W            20
#define LIT_ATTEN_CONST            21
#define LIT_ATTEN_LINEAR           22
#define LIT_ATTEN_QUADRATIC        23
#define LIT_ATTEN_XXX              24
#define LIT_CMD_1                  25
#define LIT_SPOT_DCD               26
#define LIT_SPOT_EXPONENT          27
#define LIT_SPOT_CUTOFF            28
#define LIT_SPECULAR_THRESH        29
#define LIT_RANGE_CUTOFF           30 /* ? */
#define LIT_RANGE_ATTEN            31 /* ? */
#define LIT_STATE_SIZE             32

/* Fog
 */
#define FOG_CMD_0      0
#define FOG_R          1
#define FOG_C          2
#define FOG_D          3
#define FOG_PAD        4
#define FOG_STATE_SIZE 5

/* UCP
 */
#define UCP_CMD_0      0
#define UCP_X          1
#define UCP_Y          2
#define UCP_Z          3
#define UCP_W          4
#define UCP_STATE_SIZE 5

/* GLT - Global ambient
 */
#define GLT_CMD_0      0
#define GLT_RED        1
#define GLT_GREEN      2
#define GLT_BLUE       3
#define GLT_ALPHA      4
#define GLT_STATE_SIZE 5

/* EYE
 */
#define EYE_CMD_0          0
#define EYE_X              1
#define EYE_Y              2
#define EYE_Z              3
#define EYE_RESCALE_FACTOR 4
#define EYE_STATE_SIZE     5

#define SHN_CMD_0          0
#define SHN_SHININESS      1
#define SHN_STATE_SIZE     2





struct radeon_hw_state {
   /* All state should be on one of these lists:
    */
   struct radeon_state_atom dirty; /* dirty list head placeholder */
   struct radeon_state_atom clean; /* clean list head placeholder */

   /* Hardware state, stored as cmdbuf commands:  
    *   -- Need to doublebuffer for
    *           - reviving state after loss of context
    *           - eliding noop statechange loops? (except line stipple count)
    */
   struct radeon_state_atom ctx;
   struct radeon_state_atom set;
   struct radeon_state_atom lin;
   struct radeon_state_atom msk;
   struct radeon_state_atom vpt;
   struct radeon_state_atom tcl;
   struct radeon_state_atom msc;
   struct radeon_state_atom tex[2];
   struct radeon_state_atom zbs;
   struct radeon_state_atom mtl; 
   struct radeon_state_atom mat[5]; 
   struct radeon_state_atom lit[8]; /* includes vec, scl commands */
   struct radeon_state_atom ucp[6];
   struct radeon_state_atom eye; /* eye pos */
   struct radeon_state_atom grd; /* guard band clipping */
   struct radeon_state_atom fog; 
   struct radeon_state_atom glt; 
};

struct radeon_state {
   /* Derived state for internal purposes:
    */
   struct radeon_colorbuffer_state color;
   struct radeon_depthbuffer_state depth;
   struct radeon_pixel_state pixel;
   struct radeon_scissor_state scissor;
   struct radeon_stencilbuffer_state stencil;
   struct radeon_stipple_state stipple;
   struct radeon_texture_state texture;
};

struct radeon_texture {
   radeonTexObj objects[RADEON_NR_TEX_HEAPS];
   radeonTexObj swapped;

   memHeap_t *heap[RADEON_NR_TEX_HEAPS];
   GLint age[RADEON_NR_TEX_HEAPS];

   GLint numHeaps;
};

/* Need refcounting on dma buffers:
 */
struct radeon_dma_buffer {
   int refcount;		/* the number of retained regions in buf */
   drmBufPtr buf;
};

#define GET_START(rvb) (rmesa->radeonScreen->agp_buffer_offset +			\
			(rvb)->address - rmesa->dma.buf0_address +	\
			(rvb)->start)

/* A retained region, eg vertices for indexed vertices.
 */
struct radeon_dma_region {
   struct radeon_dma_buffer *buf;
   char *address;		/* == buf->address */
   int start, end, ptr;		/* offsets from start of buf */
   int aos_start;
   int aos_stride;
   int aos_size;
};


struct radeon_dma {
   /* Active dma region.  Allocations for vertices and retained
    * regions come from here.  Also used for emitting random vertices,
    * these may be flushed by calling flush_current();
    */
   struct radeon_dma_region current;
   
   void (*flush)( radeonContextPtr );

   char *buf0_address;		/* start of buf[0], for index calcs */
   GLuint nr_released_bufs;	/* flush after so many buffers released */
};

struct radeon_dri_mirror {
   Display *display;			/* X server display */

   __DRIcontextPrivate	*context;	/* DRI context */
   __DRIscreenPrivate	*screen;	/* DRI screen */
   __DRIdrawablePrivate	*drawable;	/* DRI drawable bound to this ctx */

   drmContext hwContext;
   drmLock *hwLock;
   int fd;
   int drmMinor;
};


#define RADEON_CMD_BUF_SZ  (8*1024) 

struct radeon_store {
   GLuint statenr;
   GLuint primnr;
   char cmd_buf[RADEON_CMD_BUF_SZ];
   int cmd_used;   
   int elts_start;
};


/* radeon_tcl.c
 */
struct radeon_tcl_info {
   GLuint vertex_format;
   GLint last_offset;
   GLuint hw_primitive;

   struct radeon_dma_region *aos_components[8];
   GLuint nr_aos_components;

   GLuint *Elts;

   struct radeon_dma_region indexed_verts;
   struct radeon_dma_region obj;
   struct radeon_dma_region rgba;
   struct radeon_dma_region spec;
   struct radeon_dma_region fog;
   struct radeon_dma_region tex[RADEON_MAX_TEXTURE_UNITS];
   struct radeon_dma_region norm;
};


/* radeon_swtcl.c
 */
struct radeon_swtcl_info {
   GLuint SetupIndex;
   GLuint SetupNewInputs;
   GLuint RenderIndex;
   GLuint vertex_size;
   GLuint vertex_stride_shift;
   GLuint vertex_format;
   char *verts;

   /* Fallback rasterization functions
    */
   radeon_point_func draw_point;
   radeon_line_func draw_line;
   radeon_tri_func draw_tri;

   GLuint hw_primitive;
   GLenum render_primitive;
   GLuint numverts;

   struct radeon_dma_region indexed_verts;
};


struct radeon_ioctl {
   GLuint vertex_offset;
   GLuint vertex_size;
};



#define RADEON_MAX_PRIMS 64


/* Want to keep a cache of these around.  Each is parameterized by
 * only a single value which has only a small range.  Only expect a
 * few, so just rescan the list each time?
 */
struct dynfn {
   struct dynfn *next, *prev;
   int key;
   char *code;
};

struct dfn_lists {
   struct dynfn Vertex2f;
   struct dynfn Vertex2fv;
   struct dynfn Vertex3f;
   struct dynfn Vertex3fv;
   struct dynfn Color4ub;
   struct dynfn Color4ubv;
   struct dynfn Color3ub;
   struct dynfn Color3ubv;
   struct dynfn Color4f;
   struct dynfn Color4fv;
   struct dynfn Color3f;
   struct dynfn Color3fv;
   struct dynfn SecondaryColor3ubEXT;
   struct dynfn SecondaryColor3ubvEXT;
   struct dynfn SecondaryColor3fEXT;
   struct dynfn SecondaryColor3fvEXT;
   struct dynfn Normal3f;
   struct dynfn Normal3fv;
   struct dynfn TexCoord2f;
   struct dynfn TexCoord2fv;
   struct dynfn TexCoord1f;
   struct dynfn TexCoord1fv;
   struct dynfn MultiTexCoord2fARB;
   struct dynfn MultiTexCoord2fvARB;
   struct dynfn MultiTexCoord1fARB;
   struct dynfn MultiTexCoord1fvARB;
};

struct _vb;

struct dfn_generators {
   struct dynfn *(*Vertex2f)( GLcontext *, int );
   struct dynfn *(*Vertex2fv)( GLcontext *, int );
   struct dynfn *(*Vertex3f)( GLcontext *, int );
   struct dynfn *(*Vertex3fv)( GLcontext *, int );
   struct dynfn *(*Color4ub)( GLcontext *, int );
   struct dynfn *(*Color4ubv)( GLcontext *, int );
   struct dynfn *(*Color3ub)( GLcontext *, int );
   struct dynfn *(*Color3ubv)( GLcontext *, int );
   struct dynfn *(*Color4f)( GLcontext *, int );
   struct dynfn *(*Color4fv)( GLcontext *, int );
   struct dynfn *(*Color3f)( GLcontext *, int );
   struct dynfn *(*Color3fv)( GLcontext *, int );
   struct dynfn *(*SecondaryColor3ubEXT)( GLcontext *, int );
   struct dynfn *(*SecondaryColor3ubvEXT)( GLcontext *, int );
   struct dynfn *(*SecondaryColor3fEXT)( GLcontext *, int );
   struct dynfn *(*SecondaryColor3fvEXT)( GLcontext *, int );
   struct dynfn *(*Normal3f)( GLcontext *, int );
   struct dynfn *(*Normal3fv)( GLcontext *, int );
   struct dynfn *(*TexCoord2f)( GLcontext *, int );
   struct dynfn *(*TexCoord2fv)( GLcontext *, int );
   struct dynfn *(*TexCoord1f)( GLcontext *, int );
   struct dynfn *(*TexCoord1fv)( GLcontext *, int );
   struct dynfn *(*MultiTexCoord2fARB)( GLcontext *, int );
   struct dynfn *(*MultiTexCoord2fvARB)( GLcontext *, int );
   struct dynfn *(*MultiTexCoord1fARB)( GLcontext *, int );
   struct dynfn *(*MultiTexCoord1fvARB)( GLcontext *, int );
};


struct radeon_vb {
   /* Keep these first: referenced from codegen templates:
    */
   GLint counter, initial_counter;
   GLint *dmaptr;
   void (*notify)( void );
   GLint vertex_size;

   /* A maximum total of 15 elements per vertex:  3 floats for position, 3
    * floats for normal, 4 floats for color, 4 bytes for secondary color,
    * 2 floats for each texture unit (4 floats total).
    * 
    * As soon as the 3rd TMU is supported or cube maps (or 3D textures) are
    * supported, this value will grow.
    * 
    * The position data is never actually stored here, so 3 elements could be
    * trimmed out of the buffer.
    */
   union { float f; int i; radeon_color_t color; } vertex[15];

   GLfloat *normalptr;
   GLfloat *floatcolorptr;
   radeon_color_t *colorptr;
   GLfloat *floatspecptr;
   radeon_color_t *specptr;
   GLfloat *texcoordptr[2];

   GLcontext *context;		/* current context : Single thread only! */
};

struct radeon_prim {
   GLuint start;
   GLuint end;
   GLuint prim;
};

struct radeon_vbinfo {
   GLenum *prim;		/* &ctx->Driver.CurrentExecPrimitive */
   GLuint primflags;
   GLboolean enabled;		/* RADEON_NO_VTXFMT//RADEON_NO_TCL env vars */
   GLboolean installed;
   GLboolean fell_back;
   GLboolean recheck;
   GLint initial_counter;
   GLint nrverts;
   GLuint vertex_format;

   GLuint installed_vertex_format;
   GLuint installed_color_3f_sz;

   struct radeon_prim primlist[RADEON_MAX_PRIMS];
   int nrprims;

   struct dfn_lists dfn_cache;
   struct dfn_generators codegen;
   GLvertexformat vtxfmt;
};




struct radeon_context {
   GLcontext *glCtx;			/* Mesa context */

   /* Driver and hardware state management
    */
   struct radeon_hw_state hw;
   struct radeon_state state;

   /* Texture object bookkeeping
    */
   struct radeon_texture texture;


   /* Rasterization and vertex state:
    */
   GLuint TclFallback;
   GLuint Fallback;
   GLuint NewGLState;

   
   /* Temporaries for translating away float colors:
    */
   struct gl_client_array UbyteColor;
   struct gl_client_array UbyteSecondaryColor;

   /* Vertex buffers
    */
   struct radeon_ioctl ioctl;
   struct radeon_dma dma;
   struct radeon_store store;

   /* Page flipping
    */
   GLuint doPageFlip;

   /* Busy waiting
    */
   GLuint do_usleeps;
   GLuint do_irqs;
   GLuint irqsEmitted;
   drmRadeonIrqWait iw;

   /* Drawable, cliprect and scissor information
    */
   GLuint numClipRects;			/* Cliprects for the draw buffer */
   XF86DRIClipRectPtr pClipRects;
   unsigned int lastStamp;
   GLboolean lost_context;
   radeonScreenPtr radeonScreen;	/* Screen private DRI data */
   RADEONSAREAPrivPtr sarea;		/* Private SAREA data */

   /* TCL stuff
    */
   GLmatrix TexGenMatrix[RADEON_MAX_TEXTURE_UNITS];
   GLboolean recheck_texgen[RADEON_MAX_TEXTURE_UNITS];
   GLboolean TexGenNeedNormals[RADEON_MAX_TEXTURE_UNITS];
   GLuint TexMatEnabled;
   GLuint TexGenEnabled;
   GLmatrix tmpmat;
   GLuint last_ReallyEnabled;

   /* VBI
    */
   GLuint vbl_seq;

   /* radeon_tcl.c
    */
   struct radeon_tcl_info tcl;

   /* radeon_swtcl.c
    */
   struct radeon_swtcl_info swtcl;

   /* radeon_vtxfmt.c
    */
   struct radeon_vbinfo vb;

   /* Mirrors of some DRI state
    */
   struct radeon_dri_mirror dri;

 
   /* Performance counters
    */
   GLuint boxes;			/* Draw performance boxes */
   GLuint hardwareWentIdle;
   GLuint c_clears;
   GLuint c_drawWaits;
   GLuint c_textureSwaps;
   GLuint c_textureBytes;
   GLuint c_vertexBuffers;
};

#define RADEON_CONTEXT(ctx)		((radeonContextPtr)(ctx->DriverCtx))


static __inline GLuint radeonPackColor( GLuint cpp,
					GLubyte r, GLubyte g,
					GLubyte b, GLubyte a )
{
   switch ( cpp ) {
   case 2:
      return PACK_COLOR_565( r, g, b );
   case 4:
      return PACK_COLOR_8888( a, r, g, b );
   default:
      return 0;
   }
}

#define RADEON_OLD_PACKETS 1

/* ================================================================
 * Debugging:
 */
#define DO_DEBUG		1

#if DO_DEBUG
extern int RADEON_DEBUG;
#else
#define RADEON_DEBUG		0
#endif

#define DEBUG_TEXTURE	0x001
#define DEBUG_STATE	0x002
#define DEBUG_IOCTL	0x004
#define DEBUG_PRIMS	0x008
#define DEBUG_VERTS	0x010
#define DEBUG_FALLBACKS	0x020
#define DEBUG_VFMT	0x040
#define DEBUG_CODEGEN	0x080
#define DEBUG_VERBOSE	0x100
#define DEBUG_DRI       0x200
#define DEBUG_DMA       0x400
#define DEBUG_SANITY    0x800

#endif
#endif /* __RADEON_CONTEXT_H__ */
