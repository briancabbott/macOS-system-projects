/**************************************************************************

Copyright 2001 2d3d Inc., Delray Beach, FL

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

/* $XFree86: xc/lib/GL/mesa/src/drv/i830/i830_tex.c,v 1.4 2002/12/10 01:26:53 dawes Exp $ */

/*
 * Author:
 *   Jeff Hartmann <jhartmann@2d3d.com>
 *
 * Heavily based on the I810 driver, which was written by:
 *   Keith Whitwell <keithw@tungstengraphics.com>
 */

#include <stdlib.h>
#include <stdio.h>

#include <GL/gl.h>
#include "glheader.h"
#include "mtypes.h"
#include "mem.h"
#include "simple_list.h"
#include "enums.h"
#include "texstore.h"
#include "texformat.h"
#include "swrast/swrast.h"

#include "mm.h"

#include "i830_screen.h"
#include "i830_dri.h"
#include "i830_context.h"
#include "i830_tex.h"
#include "i830_state.h"
#include "i830_ioctl.h"

/*
 * Compute the 'S2.4' lod bias factor from the floating point OpenGL bias.
 */
static void i830ComputeLodBias(i830ContextPtr imesa,
			       i830TextureObjectPtr t,
			       GLfloat bias)
{
   int b;

   b = (int) (bias * 16.0);
   if(b > 63) b = 63;
   else if (b < -64) b = -64;
   t->Setup[I830_TEXREG_TM0S3] &= ~TM0S3_LOD_BIAS_MASK;
   t->Setup[I830_TEXREG_TM0S3] |= ((b << TM0S3_LOD_BIAS_SHIFT) & 
				   TM0S3_LOD_BIAS_MASK);
}

static void i830SetTexWrapping(i830TextureObjectPtr tex,
			       GLenum swrap, GLenum twrap)
{
   if(I830_DEBUG&DEBUG_DRI)
      fprintf(stderr, "%s\n", __FUNCTION__);

   tex->Setup[I830_TEXREG_MCS] &= ~(TEXCOORD_ADDR_U_MASK|TEXCOORD_ADDR_V_MASK);

   switch( swrap ) {
   case GL_REPEAT:
      tex->Setup[I830_TEXREG_MCS] |= TEXCOORD_ADDR_U_MODE(TEXCOORDMODE_WRAP);
      break;
   case GL_CLAMP:
      tex->Setup[I830_TEXREG_MCS] |= TEXCOORD_ADDR_U_MODE(TEXCOORDMODE_CLAMP);
      break;
   case GL_CLAMP_TO_EDGE:
      tex->Setup[I830_TEXREG_MCS] |= 
			TEXCOORD_ADDR_U_MODE(TEXCOORDMODE_CLAMP_BORDER);
      break;
   default: break;
   }

   switch( twrap ) {
   case GL_REPEAT:
      tex->Setup[I830_TEXREG_MCS] |= TEXCOORD_ADDR_V_MODE(TEXCOORDMODE_WRAP);
      break;
   case GL_CLAMP:
      tex->Setup[I830_TEXREG_MCS] |= TEXCOORD_ADDR_V_MODE(TEXCOORDMODE_CLAMP);
      break;
   case GL_CLAMP_TO_EDGE:
      tex->Setup[I830_TEXREG_MCS] |= 
			TEXCOORD_ADDR_V_MODE(TEXCOORDMODE_CLAMP_BORDER);
      break;
   default: break;
   }
}

static void i830SetTexFilter(i830ContextPtr imesa, 
			     i830TextureObjectPtr t, 
			     GLenum minf, GLenum magf,
			     GLfloat bias)
{
   int minFilt = 0, mipFilt = 0, magFilt = 0;

   if(I830_DEBUG&DEBUG_DRI)
      fprintf(stderr, "%s\n", __FUNCTION__);

   switch (minf) {
   case GL_NEAREST:
      minFilt = FILTER_NEAREST;
      mipFilt = MIPFILTER_NONE;
      break;
   case GL_LINEAR:
      minFilt = FILTER_LINEAR;
      mipFilt = MIPFILTER_NONE;
      break;
   case GL_NEAREST_MIPMAP_NEAREST:
      minFilt = FILTER_NEAREST;
      mipFilt = MIPFILTER_NEAREST;

/*       if(magf == GL_LINEAR && 0) { */
/* 	 bias -= 0.5; */
/*       } */

      break;
   case GL_LINEAR_MIPMAP_NEAREST:
      minFilt = FILTER_LINEAR;
      mipFilt = MIPFILTER_NEAREST;
      break;
   case GL_NEAREST_MIPMAP_LINEAR:
      minFilt = FILTER_NEAREST;
      mipFilt = MIPFILTER_LINEAR;

/*       if(magf == GL_LINEAR && 0) { */
/* 	 bias -= 0.5; */
/*       } */

      break;
   case GL_LINEAR_MIPMAP_LINEAR:
      minFilt = FILTER_LINEAR;
      mipFilt = MIPFILTER_LINEAR;
      break;
   default:
      fprintf(stderr, "i830SetTexFilter(): not supported min. filter %d\n",
	      (int)minf);
      break;
   }

   switch (magf) {
   case GL_NEAREST:
      magFilt = FILTER_NEAREST;
      break;
   case GL_LINEAR:
      magFilt = FILTER_LINEAR;
      break;
   default:
      fprintf(stderr, "i830SetTexFilter(): not supported mag. filter %d\n",
	      (int)magf);
      break;
   }  

   t->Setup[I830_TEXREG_TM0S3] &= ~TM0S3_MIN_FILTER_MASK;
   t->Setup[I830_TEXREG_TM0S3] &= ~TM0S3_MIP_FILTER_MASK;
   t->Setup[I830_TEXREG_TM0S3] &= ~TM0S3_MAG_FILTER_MASK;
   t->Setup[I830_TEXREG_TM0S3] |= ((minFilt << TM0S3_MIN_FILTER_SHIFT) |
				   (mipFilt << TM0S3_MIP_FILTER_SHIFT) |
				   (magFilt << TM0S3_MAG_FILTER_SHIFT));

   i830ComputeLodBias(imesa, t, bias); 
}

static void i830SetTexBorderColor(i830TextureObjectPtr t, GLubyte color[4])
{
   if(I830_DEBUG&DEBUG_DRI)
      fprintf(stderr, "%s\n", __FUNCTION__);

    t->Setup[I830_TEXREG_TM0S4] = 
        I830PACKCOLOR8888(color[0],color[1],color[2],color[3]);
}


static void i830TexParameter( GLcontext *ctx, GLenum target,
			      struct gl_texture_object *tObj,
			      GLenum pname, const GLfloat *params )
{
   i830ContextPtr imesa = I830_CONTEXT(ctx);
   i830TextureObjectPtr t = (i830TextureObjectPtr) tObj->DriverData;
   GLuint unit = ctx->Texture.CurrentUnit;
   if (!t)
      return;

   if ( target != GL_TEXTURE_2D )
      return;

   /* Can't do the update now as we don't know whether to flush
    * vertices or not.  Setting imesa->new_state means that
    * i830UpdateTextureState() will be called before any triangles are
    * rendered.  If a statechange has occurred, it will be detected at
    * that point, and buffered vertices flushed.  
    */
   switch (pname) {
   case GL_TEXTURE_MIN_FILTER:
   case GL_TEXTURE_MAG_FILTER:
      {
         GLfloat bias = ctx->Texture.Unit[unit].LodBias;
         i830SetTexFilter( imesa, t, tObj->MinFilter, tObj->MagFilter, bias );
      }
      break;

   case GL_TEXTURE_WRAP_S:
   case GL_TEXTURE_WRAP_T:
      i830SetTexWrapping( t, tObj->WrapS, tObj->WrapT );
      break;
  
   case GL_TEXTURE_BORDER_COLOR:
      i830SetTexBorderColor( t, tObj->BorderColor );
      break;

   case GL_TEXTURE_BASE_LEVEL:
   case GL_TEXTURE_MAX_LEVEL:
   case GL_TEXTURE_MIN_LOD:
   case GL_TEXTURE_MAX_LOD:
      /* The i830 and its successors can do a lot of this without
       * reloading the textures.  A project for someone?
       */
      I830_FIREVERTICES( I830_CONTEXT(ctx) );
      i830SwapOutTexObj( imesa, t );
      break;

   default:
      return;
   }

   if (t == imesa->CurrentTexObj[unit]) {
      I830_STATECHANGE( imesa, I830_UPLOAD_TEX0 );
   }
}


static void i830TexEnv( GLcontext *ctx, GLenum target, 
			GLenum pname, const GLfloat *param )
{
   i830ContextPtr imesa = I830_CONTEXT( ctx );
   GLuint unit = ctx->Texture.CurrentUnit;

   /* Only one env color.  Need a fallback if env colors are different
    * and texture setup references env color in both units.  
    */
   switch (pname) {
   case GL_TEXTURE_ENV_COLOR:
   case GL_TEXTURE_ENV_MODE:
   case GL_COMBINE_RGB_EXT:
   case GL_COMBINE_ALPHA_EXT:
   case GL_SOURCE0_RGB_EXT:
   case GL_SOURCE1_RGB_EXT:
   case GL_SOURCE2_RGB_EXT:
   case GL_SOURCE0_ALPHA_EXT:
   case GL_SOURCE1_ALPHA_EXT:
   case GL_SOURCE2_ALPHA_EXT:
   case GL_OPERAND0_RGB_EXT:
   case GL_OPERAND1_RGB_EXT:
   case GL_OPERAND2_RGB_EXT:
   case GL_OPERAND0_ALPHA_EXT:
   case GL_OPERAND1_ALPHA_EXT:
   case GL_OPERAND2_ALPHA_EXT:
   case GL_RGB_SCALE_EXT:
   case GL_ALPHA_SCALE:
      imesa->TexEnvImageFmt[unit] = 0; /* force recalc of env state */
      break;

   case GL_TEXTURE_LOD_BIAS_EXT:
      {
         struct gl_texture_object *tObj = ctx->Texture.Unit[unit]._Current;
         i830TextureObjectPtr t = (i830TextureObjectPtr) tObj->DriverData;
	 i830ComputeLodBias(imesa, t, *param);
	 /* Do a state change */
	 if (t == imesa->CurrentTexObj[unit]) {
	    I830_STATECHANGE( imesa, I830_UPLOAD_TEX_N(unit) );
	 }
      }
      break;

   default:
      break;
   }
} 

static void i830TexImage2D( GLcontext *ctx, GLenum target, GLint level,
			    GLint internalFormat,
			    GLint width, GLint height, GLint border,
			    GLenum format, GLenum type, const GLvoid *pixels,
			    const struct gl_pixelstore_attrib *packing,
			    struct gl_texture_object *texObj,
			    struct gl_texture_image *texImage )
{
   i830TextureObjectPtr t = (i830TextureObjectPtr) texObj->DriverData;
   if (t) {
      I830_FIREVERTICES( I830_CONTEXT(ctx) );
      i830SwapOutTexObj( I830_CONTEXT(ctx), t );
   }
   _mesa_store_teximage2d( ctx, target, level, internalFormat,
			   width, height, border, format, type,
			   pixels, packing, texObj, texImage );
}

static void i830TexSubImage2D( GLcontext *ctx, 
			       GLenum target,
			       GLint level,	
			       GLint xoffset, GLint yoffset,
			       GLsizei width, GLsizei height,
			       GLenum format, GLenum type,
			       const GLvoid *pixels,
			       const struct gl_pixelstore_attrib *packing,
			       struct gl_texture_object *texObj,
			       struct gl_texture_image *texImage )
{
   i830TextureObjectPtr t = (i830TextureObjectPtr) texObj->DriverData;
   if (t) {
      I830_FIREVERTICES( I830_CONTEXT(ctx) );
      i830SwapOutTexObj( I830_CONTEXT(ctx), t );
   }
   _mesa_store_texsubimage2d(ctx, target, level, xoffset, yoffset, width, 
			     height, format, type, pixels, packing, texObj,
			     texImage);

}

static void i830BindTexture( GLcontext *ctx, GLenum target,
			     struct gl_texture_object *tObj )
{
   if (target == GL_TEXTURE_2D) {
      i830ContextPtr imesa = I830_CONTEXT( ctx );
      i830TextureObjectPtr t = (i830TextureObjectPtr) tObj->DriverData;

      if (!t) {
         GLfloat bias = ctx->Texture.Unit[ctx->Texture.CurrentUnit].LodBias;
	 t = CALLOC_STRUCT(i830_texture_object_t);

	 /* Initialize non-image-dependent parts of the state:
	  */
	 t->globj = tObj;
	 t->Setup[I830_TEXREG_TM0LI] = STATE3D_LOAD_STATE_IMMEDIATE_2;
	 t->Setup[I830_TEXREG_TM0S0] = TM0S0_USE_FENCE;
	 t->Setup[I830_TEXREG_TM0S1] = 0;
	 t->Setup[I830_TEXREG_TM0S2] = 0;
	 t->Setup[I830_TEXREG_TM0S3] = 0;

	 t->Setup[I830_TEXREG_NOP0] = 0;
	 t->Setup[I830_TEXREG_NOP1] = 0;
	 t->Setup[I830_TEXREG_NOP2] = 0;

	 t->Setup[I830_TEXREG_MCS] = (STATE3D_MAP_COORD_SET_CMD |
				      MAP_UNIT(0) |
				      ENABLE_TEXCOORD_PARAMS |
				      TEXCOORDS_ARE_NORMAL |
				      TEXCOORDTYPE_CARTESIAN |
				      ENABLE_ADDR_V_CNTL |
				      TEXCOORD_ADDR_V_MODE(TEXCOORDMODE_WRAP) |
				      ENABLE_ADDR_U_CNTL |
				      TEXCOORD_ADDR_U_MODE(TEXCOORDMODE_WRAP));


	 t->dirty_images = ~0;

	 tObj->DriverData = t;
	 make_empty_list( t );

	 i830SetTexWrapping( t, tObj->WrapS, tObj->WrapT );
	 i830SetTexFilter( imesa, t, tObj->MinFilter, tObj->MagFilter, bias );
	 i830SetTexBorderColor( t, tObj->BorderColor );
      }
   }
}

static void i830DeleteTexture( GLcontext *ctx, struct gl_texture_object *tObj )
{
   i830TextureObjectPtr t = (i830TextureObjectPtr)tObj->DriverData;

   if (t) {
      i830ContextPtr imesa = I830_CONTEXT( ctx );
      if (imesa)
         I830_FIREVERTICES( imesa );
      i830DestroyTexObj( imesa, t );
      tObj->DriverData = 0;
   }
}

static GLboolean i830IsTextureResident( GLcontext *ctx, 
					struct gl_texture_object *tObj )
{
   i830TextureObjectPtr t = (i830TextureObjectPtr)tObj->DriverData;
   return t && t->MemBlock;
}

static const struct gl_texture_format *
i830ChooseTextureFormat( GLcontext *ctx, GLint internalFormat,
			 GLenum format, GLenum type )
{
   i830ContextPtr imesa = I830_CONTEXT( ctx );
   const GLboolean do32bpt = ( imesa->i830Screen->cpp == 4 &&
			       imesa->i830Screen->textureSize > 4*1024*1024);

   switch ( internalFormat ) {
   case 4:
   case GL_RGBA:
      if ( format == GL_BGRA ) {
	 if ( type == GL_UNSIGNED_INT_8_8_8_8_REV ) {
	    return &_mesa_texformat_argb8888;
	 }
         else if ( type == GL_UNSIGNED_SHORT_4_4_4_4_REV ) {
            return &_mesa_texformat_argb4444;
	 }
         else if ( type == GL_UNSIGNED_SHORT_1_5_5_5_REV ) {
	    return &_mesa_texformat_argb1555;
	 }
      }
      return do32bpt ? &_mesa_texformat_argb8888 : &_mesa_texformat_argb4444;

   case 3:
   case GL_RGB:
      if ( format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5 ) {
	 return &_mesa_texformat_rgb565;
      }
      return do32bpt ? &_mesa_texformat_argb8888 : &_mesa_texformat_rgb565;

   case GL_RGBA8:
   case GL_RGB10_A2:
   case GL_RGBA12:
   case GL_RGBA16:
      return do32bpt ? &_mesa_texformat_argb8888 : &_mesa_texformat_argb4444;

   case GL_RGBA4:
   case GL_RGBA2:
      return &_mesa_texformat_argb4444;

   case GL_RGB5_A1:
      return &_mesa_texformat_argb1555;

   case GL_RGB8:
   case GL_RGB10:
   case GL_RGB12:
   case GL_RGB16:
      return do32bpt ? &_mesa_texformat_argb8888 : &_mesa_texformat_rgb565;

   case GL_RGB5:
   case GL_RGB4:
   case GL_R3_G3_B2:
      return &_mesa_texformat_rgb565;

   case GL_ALPHA:
   case GL_ALPHA4:
   case GL_ALPHA8:
   case GL_ALPHA12:
   case GL_ALPHA16:
      return &_mesa_texformat_al88;

   case 1:
   case GL_LUMINANCE:
   case GL_LUMINANCE4:
   case GL_LUMINANCE8:
   case GL_LUMINANCE12:
   case GL_LUMINANCE16:
      return &_mesa_texformat_l8;

   case 2:
   case GL_LUMINANCE_ALPHA:
   case GL_LUMINANCE4_ALPHA4:
   case GL_LUMINANCE6_ALPHA2:
   case GL_LUMINANCE8_ALPHA8:
   case GL_LUMINANCE12_ALPHA4:
   case GL_LUMINANCE12_ALPHA12:
   case GL_LUMINANCE16_ALPHA16:
      return &_mesa_texformat_al88;

   case GL_INTENSITY:
   case GL_INTENSITY4:
   case GL_INTENSITY8:
   case GL_INTENSITY12:
   case GL_INTENSITY16:
      return &_mesa_texformat_i8;

   default:
      fprintf(stderr, "unexpected texture format in %s", __FUNCTION__);
      return NULL;
   }

   return NULL; /* never get here */
}

void i830DDInitTextureFuncs( GLcontext *ctx )
{
   ctx->Driver.TexEnv = i830TexEnv;
   ctx->Driver.ChooseTextureFormat = i830ChooseTextureFormat;
   ctx->Driver.TexImage1D = _mesa_store_teximage1d;
   ctx->Driver.TexImage2D = i830TexImage2D;
   ctx->Driver.TexImage3D = _mesa_store_teximage3d;
   ctx->Driver.TexSubImage1D = _mesa_store_texsubimage1d;
   ctx->Driver.TexSubImage2D = i830TexSubImage2D;
   ctx->Driver.TexSubImage3D = _mesa_store_texsubimage3d;
   ctx->Driver.CopyTexImage1D = _swrast_copy_teximage1d;
   ctx->Driver.CopyTexImage2D = _swrast_copy_teximage2d;
   ctx->Driver.CopyTexSubImage1D = _swrast_copy_texsubimage1d;
   ctx->Driver.CopyTexSubImage2D = _swrast_copy_texsubimage2d;
   ctx->Driver.CopyTexSubImage3D = _swrast_copy_texsubimage3d;
   ctx->Driver.BindTexture = i830BindTexture;
   ctx->Driver.DeleteTexture = i830DeleteTexture;
   ctx->Driver.TexParameter = i830TexParameter;
   ctx->Driver.UpdateTexturePalette = 0;
   ctx->Driver.IsTextureResident = i830IsTextureResident;
   ctx->Driver.TestProxyTexImage = _mesa_test_proxy_teximage;

   {
      GLuint tmp = ctx->Texture.CurrentUnit;
      ctx->Texture.CurrentUnit = 0;
      i830BindTexture( ctx, GL_TEXTURE_2D, ctx->Texture.Unit[0].Current2D);
      ctx->Texture.CurrentUnit = 1;
      i830BindTexture( ctx, GL_TEXTURE_2D, ctx->Texture.Unit[1].Current2D);
      ctx->Texture.CurrentUnit = tmp;
   }
}
