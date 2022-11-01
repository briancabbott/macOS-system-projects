/*
 * GLX Hardware Device Driver for Intel i810
 * Copyright (C) 1999 Keith Whitwell
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
 * KEITH WHITWELL, OR ANY OTHER CONTRIBUTORS BE LIABLE FOR ANY CLAIM, 
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR 
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE 
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */
/* $XFree86: xc/lib/GL/mesa/src/drv/i810/i810tex.c,v 1.9 2002/10/30 12:51:33 alanh Exp $ */

#include <stdlib.h>
#include <stdio.h>

#include "glheader.h"
#include "mtypes.h"
#include "mem.h"
#include "simple_list.h"
#include "enums.h"
#include "texstore.h"
#include "texformat.h"
#include "swrast/swrast.h"

#include "mm.h"

#include "i810screen.h"
#include "i810_dri.h"

#include "i810context.h"
#include "i810tex.h"
#include "i810state.h"
#include "i810ioctl.h"


/*
 * Compute the 'S2.4' lod bias factor from the floating point OpenGL bias.
 */
static GLuint i810ComputeLodBias(GLfloat bias)
{
   int b = (int) (bias * 16.0) + 12;
   if (b > 63)
      b = 63;
   else if (b < -64)
      b = -64;
   return (GLuint) (b & MLC_LOD_BIAS_MASK);
}


static void i810SetTexWrapping(i810TextureObjectPtr t, 
			       GLenum wraps, GLenum wrapt)
{
   t->Setup[I810_TEXREG_MCS] &= ~(MCS_U_STATE_MASK| MCS_V_STATE_MASK);
   t->Setup[I810_TEXREG_MCS] |= (MCS_U_WRAP|MCS_V_WRAP);

   if (wraps != GL_REPEAT) 
      t->Setup[I810_TEXREG_MCS] ^= (MCS_U_WRAP^MCS_U_CLAMP);

   if (wrapt != GL_REPEAT) 
      t->Setup[I810_TEXREG_MCS] ^= (MCS_V_WRAP^MCS_V_CLAMP);

}


static void i810SetTexFilter(i810ContextPtr imesa, 
			     i810TextureObjectPtr t, 
			     GLenum minf, GLenum magf,
                             GLfloat bias)
{
   t->Setup[I810_TEXREG_MF] &= ~(MF_MIN_MASK|
				 MF_MAG_MASK|
				 MF_MIP_MASK);
   t->Setup[I810_TEXREG_MLC] &= ~(MLC_LOD_BIAS_MASK);

   switch (minf) {
   case GL_NEAREST:
      t->Setup[I810_TEXREG_MF] |= MF_MIN_NEAREST | MF_MIP_NONE;
      break;
   case GL_LINEAR:
      t->Setup[I810_TEXREG_MF] |= MF_MIN_LINEAR | MF_MIP_NONE;
      break;
   case GL_NEAREST_MIPMAP_NEAREST:
      t->Setup[I810_TEXREG_MF] |= MF_MIN_NEAREST | MF_MIP_NEAREST;
      if (magf == GL_LINEAR) {
         /*bias -= 0.5;*/  /* this doesn't work too good */
      }
      break;
   case GL_LINEAR_MIPMAP_NEAREST:
      t->Setup[I810_TEXREG_MF] |= MF_MIN_LINEAR | MF_MIP_NEAREST;
      break;
   case GL_NEAREST_MIPMAP_LINEAR:
      if (IS_I815(imesa)) 
	 t->Setup[I810_TEXREG_MF] |= MF_MIN_NEAREST | MF_MIP_LINEAR;
      else 
	 t->Setup[I810_TEXREG_MF] |= MF_MIN_NEAREST | MF_MIP_DITHER;
      /*
      if (magf == GL_LINEAR) {
         bias -= 0.5;
      }
      */
      bias -= 0.5; /* always biasing here looks better */
      break;
   case GL_LINEAR_MIPMAP_LINEAR:
      if (IS_I815(imesa))
	 t->Setup[I810_TEXREG_MF] |= MF_MIN_LINEAR | MF_MIP_LINEAR;
      else 
	 t->Setup[I810_TEXREG_MF] |= MF_MIN_LINEAR | MF_MIP_DITHER;
      break;
   default:
      return;
   }

   switch (magf) {
   case GL_NEAREST: 
      t->Setup[I810_TEXREG_MF] |= MF_MAG_NEAREST; 
      break;
   case GL_LINEAR: 
      t->Setup[I810_TEXREG_MF] |= MF_MAG_LINEAR; 
      break;
   default: 
      return;
   }

   t->Setup[I810_TEXREG_MLC] |= i810ComputeLodBias(bias);
}


static void i810SetTexBorderColor(i810TextureObjectPtr t, 
				  GLubyte color[4])
{
   /* Need a fallback.
    */
}


static void i810TexParameter( GLcontext *ctx, GLenum target,
			      struct gl_texture_object *tObj,
			      GLenum pname, const GLfloat *params )
{
   i810ContextPtr imesa = I810_CONTEXT(ctx);
   i810TextureObjectPtr t = (i810TextureObjectPtr) tObj->DriverData;
   if (!t)
      return;

   if ( target != GL_TEXTURE_2D )
      return;

   /* Can't do the update now as we don't know whether to flush
    * vertices or not.  Setting imesa->new_state means that
    * i810UpdateTextureState() will be called before any triangles are
    * rendered.  If a statechange has occurred, it will be detected at
    * that point, and buffered vertices flushed.  
    */
   switch (pname) {
   case GL_TEXTURE_MIN_FILTER:
   case GL_TEXTURE_MAG_FILTER:
      {
         GLfloat bias = ctx->Texture.Unit[ctx->Texture.CurrentUnit].LodBias;
         i810SetTexFilter( imesa, t, tObj->MinFilter, tObj->MagFilter, bias );
      }
      break;

   case GL_TEXTURE_WRAP_S:
   case GL_TEXTURE_WRAP_T:
      i810SetTexWrapping( t, tObj->WrapS, tObj->WrapT );
      break;
  
   case GL_TEXTURE_BORDER_COLOR:
      i810SetTexBorderColor( t, tObj->BorderColor );
      break;

   case GL_TEXTURE_BASE_LEVEL:
   case GL_TEXTURE_MAX_LEVEL:
   case GL_TEXTURE_MIN_LOD:
   case GL_TEXTURE_MAX_LOD:
      /* This isn't the most efficient solution but there doesn't appear to
       * be a nice alternative for Radeon.  Since there's no LOD clamping,
       * we just have to rely on loading the right subset of mipmap levels
       * to simulate a clamped LOD.
       */
      i810SwapOutTexObj( imesa, t );
      break;

   default:
      return;
   }

   if (t == imesa->CurrentTexObj[0]) {
      I810_STATECHANGE( imesa, I810_UPLOAD_TEX0 );
   }

   if (t == imesa->CurrentTexObj[1]) {
      I810_STATECHANGE( imesa, I810_UPLOAD_TEX1 );
   }
}


static void i810TexEnv( GLcontext *ctx, GLenum target, 
			GLenum pname, const GLfloat *param )
{
   i810ContextPtr imesa = I810_CONTEXT( ctx );
   GLuint unit = ctx->Texture.CurrentUnit;

   /* Only one env color.  Need a fallback if env colors are different
    * and texture setup references env color in both units.  
    */
   switch (pname) {
   case GL_TEXTURE_ENV_COLOR: {
      struct gl_texture_unit *texUnit = &ctx->Texture.Unit[unit];
      GLfloat *fc = texUnit->EnvColor;
      GLuint r, g, b, a, col;
      CLAMPED_FLOAT_TO_UBYTE(r, fc[0]);
      CLAMPED_FLOAT_TO_UBYTE(g, fc[1]);
      CLAMPED_FLOAT_TO_UBYTE(b, fc[2]);
      CLAMPED_FLOAT_TO_UBYTE(a, fc[3]);

      col = ((a << 24) | 
	     (r << 16) | 
	     (g <<  8) | 
	     (b <<  0));
 
      if (imesa->Setup[I810_CTXREG_CF1] != col) {
	 I810_STATECHANGE(imesa, I810_UPLOAD_CTX);	
	 imesa->Setup[I810_CTXREG_CF1] = col;      
      }
      break;
   }
   case GL_TEXTURE_ENV_MODE:
      imesa->TexEnvImageFmt[unit] = 0; /* force recalc of env state */
      break;

   case GL_TEXTURE_LOD_BIAS_EXT:
      {
         struct gl_texture_object *tObj = ctx->Texture.Unit[unit]._Current;
         i810TextureObjectPtr t = (i810TextureObjectPtr) tObj->DriverData;
         t->Setup[I810_TEXREG_MLC] &= ~(MLC_LOD_BIAS_MASK);
         t->Setup[I810_TEXREG_MLC] |= i810ComputeLodBias(*param);
      }
      break;

   default:
      break;
   }
} 

#if 0
static void i810TexImage1D( GLcontext *ctx, GLenum target, GLint level,
			    GLint internalFormat,
			    GLint width, GLint border,
			    GLenum format, GLenum type, 
			    const GLvoid *pixels,
			    const struct gl_pixelstore_attrib *pack,
			    struct gl_texture_object *texObj,
			    struct gl_texture_image *texImage )
{
   i810TextureObjectPtr t = (i810TextureObjectPtr) texObj->DriverData;
   if (t) {
      i810SwapOutTexObj( imesa, t );
   }
}

static void i810TexSubImage1D( GLcontext *ctx, 
			       GLenum target,
			       GLint level,	
			       GLint xoffset,
			       GLsizei width,
			       GLenum format, GLenum type,
			       const GLvoid *pixels,
			       const struct gl_pixelstore_attrib *pack,
			       struct gl_texture_object *texObj,
			       struct gl_texture_image *texImage )
{
}
#endif


static void i810TexImage2D( GLcontext *ctx, GLenum target, GLint level,
			    GLint internalFormat,
			    GLint width, GLint height, GLint border,
			    GLenum format, GLenum type, const GLvoid *pixels,
			    const struct gl_pixelstore_attrib *packing,
			    struct gl_texture_object *texObj,
			    struct gl_texture_image *texImage )
{
   i810TextureObjectPtr t = (i810TextureObjectPtr) texObj->DriverData;
   if (t) {
      i810SwapOutTexObj( I810_CONTEXT(ctx), t );
   }
   _mesa_store_teximage2d( ctx, target, level, internalFormat,
			   width, height, border, format, type,
			   pixels, packing, texObj, texImage );
}

static void i810TexSubImage2D( GLcontext *ctx, 
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
   i810TextureObjectPtr t = (i810TextureObjectPtr) texObj->DriverData;
   if (t) {
      i810SwapOutTexObj( I810_CONTEXT(ctx), t );
   }
   _mesa_store_texsubimage2d(ctx, target, level, xoffset, yoffset, width, 
			     height, format, type, pixels, packing, texObj,
			     texImage);

}


static void i810BindTexture( GLcontext *ctx, GLenum target,
			     struct gl_texture_object *tObj )
{
   if (target == GL_TEXTURE_2D) {
      i810ContextPtr imesa = I810_CONTEXT( ctx );
      i810TextureObjectPtr t = (i810TextureObjectPtr) tObj->DriverData;

      if (!t) {
         GLfloat bias = ctx->Texture.Unit[ctx->Texture.CurrentUnit].LodBias;
	 t = CALLOC_STRUCT(i810_texture_object_t);

	 /* Initialize non-image-dependent parts of the state:
	  */
	 t->globj = tObj;
	 t->Setup[I810_TEXREG_MI0] = GFX_OP_MAP_INFO;
	 t->Setup[I810_TEXREG_MI1] = MI1_MAP_0; 
	 t->Setup[I810_TEXREG_MI2] = MI2_DIMENSIONS_ARE_LOG2;
	 t->Setup[I810_TEXREG_MLC] = (GFX_OP_MAP_LOD_CTL | 
				      MLC_MAP_0 |
				      /*MLC_DITHER_WEIGHT_FULL |*/
				      MLC_DITHER_WEIGHT_12 |
				      MLC_UPDATE_LOD_BIAS |
				      0x0);
	 t->Setup[I810_TEXREG_MCS] = (GFX_OP_MAP_COORD_SETS |
				      MCS_COORD_0 |
				      MCS_UPDATE_NORMALIZED |
				      MCS_NORMALIZED_COORDS |
				      MCS_UPDATE_V_STATE |
				      MCS_V_WRAP |
				      MCS_UPDATE_U_STATE |
				      MCS_U_WRAP);
	 t->Setup[I810_TEXREG_MF] = (GFX_OP_MAP_FILTER |
				     MF_MAP_0 |
				     MF_UPDATE_ANISOTROPIC |
				     MF_UPDATE_MIP_FILTER |
				     MF_UPDATE_MAG_FILTER |
				     MF_UPDATE_MIN_FILTER);

	 t->dirty_images = ~0;

	 tObj->DriverData = t;
	 make_empty_list( t );

	 i810SetTexWrapping( t, tObj->WrapS, tObj->WrapT );
	 i810SetTexFilter( imesa, t, tObj->MinFilter, tObj->MagFilter, bias );
	 i810SetTexBorderColor( t, tObj->BorderColor );
      }
   }
}


static void i810DeleteTexture( GLcontext *ctx, struct gl_texture_object *tObj )
{
   i810TextureObjectPtr t = (i810TextureObjectPtr)tObj->DriverData;

   if (t) {
      i810ContextPtr imesa = I810_CONTEXT( ctx );
      if (imesa)
         I810_FIREVERTICES( imesa );
      i810DestroyTexObj( imesa, t );
      tObj->DriverData = 0;
   }
}

static GLboolean i810IsTextureResident( GLcontext *ctx, 
					struct gl_texture_object *tObj )
{
   i810TextureObjectPtr t = (i810TextureObjectPtr)tObj->DriverData;
   return t && t->MemBlock;
}

void i810InitTextureFuncs( GLcontext *ctx )
{
   ctx->Driver.TexEnv = i810TexEnv;
   ctx->Driver.ChooseTextureFormat = _mesa_choose_tex_format;
   ctx->Driver.TexImage1D = _mesa_store_teximage1d;
   ctx->Driver.TexImage2D = i810TexImage2D;
   ctx->Driver.TexImage3D = _mesa_store_teximage3d;
   ctx->Driver.TexSubImage1D = _mesa_store_texsubimage1d;
   ctx->Driver.TexSubImage2D = i810TexSubImage2D;
   ctx->Driver.TexSubImage3D = _mesa_store_texsubimage3d;
   ctx->Driver.CopyTexImage1D = _swrast_copy_teximage1d;
   ctx->Driver.CopyTexImage2D = _swrast_copy_teximage2d;
   ctx->Driver.CopyTexSubImage1D = _swrast_copy_texsubimage1d;
   ctx->Driver.CopyTexSubImage2D = _swrast_copy_texsubimage2d;
   ctx->Driver.CopyTexSubImage3D = _swrast_copy_texsubimage3d;
   ctx->Driver.BindTexture = i810BindTexture;
   ctx->Driver.DeleteTexture = i810DeleteTexture;
   ctx->Driver.TexParameter = i810TexParameter;
   ctx->Driver.UpdateTexturePalette = 0;
   ctx->Driver.IsTextureResident = i810IsTextureResident;
   ctx->Driver.TestProxyTexImage = _mesa_test_proxy_teximage;

   {
      GLuint tmp = ctx->Texture.CurrentUnit;
      ctx->Texture.CurrentUnit = 0;
      i810BindTexture( ctx, GL_TEXTURE_2D, ctx->Texture.Unit[0].Current2D);
      ctx->Texture.CurrentUnit = 1;
      i810BindTexture( ctx, GL_TEXTURE_2D, ctx->Texture.Unit[1].Current2D);
      ctx->Texture.CurrentUnit = tmp;
   }
}
