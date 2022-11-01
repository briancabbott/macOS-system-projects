
/*
 * Mesa 3-D graphics library
 * Version:  3.5
 *
 * Copyright (C) 1999-2001  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Brian Paul <brian@valinux.com>
 */

#include "mtypes.h"
#include "mem.h"
#include "t_context.h"
#include "t_pipeline.h"


struct point_stage_data {
   GLvector1f PointSize;
};

#define POINT_STAGE_DATA(stage) ((struct point_stage_data *)stage->privatePtr)


/*
 * Compute attenuated point sizes
 */
static GLboolean run_point_stage( GLcontext *ctx,
				  struct gl_pipeline_stage *stage )
{
   struct point_stage_data *store = POINT_STAGE_DATA(stage);
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   const GLfloat (*eye)[4] = (const GLfloat (*)[4]) VB->EyePtr->data;
   const GLfloat p0 = ctx->Point.Params[0];
   const GLfloat p1 = ctx->Point.Params[1];
   const GLfloat p2 = ctx->Point.Params[2];
   const GLfloat pointSize = ctx->Point._Size;
   GLfloat *size = store->PointSize.data;
   GLuint i;

   if (stage->changed_inputs) {
      /* XXX do threshold and min/max clamping here? */
      for (i = 0; i < VB->Count; i++) {
	 const GLfloat dist = -eye[i][2];
	 /* GLfloat dist = GL_SQRT(pos[0]*pos[0]+pos[1]*pos[1]+pos[2]*pos[2]);*/
	 size[i] = pointSize / (p0 + dist * (p1 + dist * p2));
      }
   }

   VB->PointSizePtr = &store->PointSize;

   return GL_TRUE;
}


/* If point size attenuation is on we'll compute the point size for
 * each vertex in a special pipeline stage.
 */
static void check_point_size( GLcontext *ctx, struct gl_pipeline_stage *d )
{
   d->active = ctx->Point._Attenuated;
}

static GLboolean alloc_point_data( GLcontext *ctx,
				   struct gl_pipeline_stage *stage )
{
   struct vertex_buffer *VB = &TNL_CONTEXT(ctx)->vb;
   struct point_stage_data *store;
   stage->privatePtr = MALLOC(sizeof(*store));
   store = POINT_STAGE_DATA(stage);
   if (!store)
      return GL_FALSE;

   _mesa_vector1f_alloc( &store->PointSize, 0, VB->Size, 32 );

   /* Now run the stage.
    */
   stage->run = run_point_stage;
   return stage->run( ctx, stage );
}


static void free_point_data( struct gl_pipeline_stage *stage )
{
   struct point_stage_data *store = POINT_STAGE_DATA(stage);
   if (store) {
      _mesa_vector1f_free( &store->PointSize );
      FREE( store );
      stage->privatePtr = 0;
   }
}

const struct gl_pipeline_stage _tnl_point_attenuation_stage =
{
   "point size attenuation",	/* name */
   _NEW_POINT,			/* build_state_change */
   _NEW_POINT,			/* run_state_change */
   0,				/* active */
   VERT_EYE,			/* inputs */
   VERT_POINT_SIZE,		/* outputs */
   0,				/* changed_inputs (temporary value) */
   0,				/* stage private data */
   free_point_data,		/* destructor */
   check_point_size,		/* check */
   alloc_point_data		/* run -- initially set to alloc data */
};
