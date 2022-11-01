/* $XFree86: xc/lib/GL/mesa/src/drv/radeon/radeon_tex.c,v 1.9 2002/12/16 16:18:59 dawes Exp $ */
/*
 * Copyright 2000, 2001 ATI Technologies Inc., Ontario, Canada, and
 *                      VA Linux Systems Inc., Fremont, California.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * on the rights to use, copy, modify, merge, publish, distribute, sub
 * license, and/or sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * ATI, VA LINUX SYSTEMS AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
 * USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Gareth Hughes <gareth@valinux.com>
 *    Brian Paul <brianp@valinux.com>
 */

#include "radeon_context.h"
#include "radeon_state.h"
#include "radeon_ioctl.h"
#include "radeon_swtcl.h"
#include "radeon_tex.h"

#include "colormac.h"
#include "context.h"
#include "enums.h"
#include "image.h"
#include "mem.h"
#include "simple_list.h"
#include "texformat.h"
#include "texstore.h"


/* =============================================================
 * Utility functions:
 */

static void radeonSetTexWrap( radeonTexObjPtr t, GLenum swrap, GLenum twrap )
{
   t->pp_txfilter &= ~(RADEON_CLAMP_S_MASK | RADEON_CLAMP_T_MASK);

   switch ( swrap ) {
   case GL_REPEAT:
      t->pp_txfilter |= RADEON_CLAMP_S_WRAP;
      break;
   case GL_CLAMP:
      t->pp_txfilter |= RADEON_CLAMP_S_CLAMP_LAST;
      break;
   case GL_CLAMP_TO_EDGE:
      t->pp_txfilter |= RADEON_CLAMP_S_CLAMP_LAST;
      break;
   case GL_CLAMP_TO_BORDER:
      t->pp_txfilter |= RADEON_CLAMP_S_CLAMP_BORDER;
      break;
   case GL_MIRRORED_REPEAT:
      t->pp_txfilter |= RADEON_CLAMP_S_MIRROR;
      break;
   case GL_MIRROR_CLAMP_ATI:
      t->pp_txfilter |= RADEON_CLAMP_S_MIRROR_CLAMP_BORDER;
      break;
   case GL_MIRROR_CLAMP_TO_EDGE_ATI:
      t->pp_txfilter |= RADEON_CLAMP_S_MIRROR_CLAMP_LAST;
      break;
   }

   switch ( twrap ) {
   case GL_REPEAT:
      t->pp_txfilter |= RADEON_CLAMP_T_WRAP;
      break;
   case GL_CLAMP:
      t->pp_txfilter |= RADEON_CLAMP_T_CLAMP_LAST;
      break;
   case GL_CLAMP_TO_EDGE:
      t->pp_txfilter |= RADEON_CLAMP_T_CLAMP_LAST;
      break;
   case GL_CLAMP_TO_BORDER:
      t->pp_txfilter |= RADEON_CLAMP_T_CLAMP_BORDER;
      break;
   case GL_MIRRORED_REPEAT:
      t->pp_txfilter |= RADEON_CLAMP_T_MIRROR;
      break;
   case GL_MIRROR_CLAMP_ATI:
      t->pp_txfilter |= RADEON_CLAMP_T_MIRROR_CLAMP_BORDER;
      break;
   case GL_MIRROR_CLAMP_TO_EDGE_ATI:
      t->pp_txfilter |= RADEON_CLAMP_T_MIRROR_CLAMP_LAST;
      break;
   }
}

static void radeonSetTexMaxAnisotropy( radeonTexObjPtr t, GLfloat max )
{
   t->pp_txfilter &= ~RADEON_MAX_ANISO_MASK;

   if ( max == 1.0 ) {
      t->pp_txfilter |= RADEON_MAX_ANISO_1_TO_1;
   } else if ( max <= 2.0 ) {
      t->pp_txfilter |= RADEON_MAX_ANISO_2_TO_1;
   } else if ( max <= 4.0 ) {
      t->pp_txfilter |= RADEON_MAX_ANISO_4_TO_1;
   } else if ( max <= 8.0 ) {
      t->pp_txfilter |= RADEON_MAX_ANISO_8_TO_1;
   } else {
      t->pp_txfilter |= RADEON_MAX_ANISO_16_TO_1;
   }
}

static void radeonSetTexFilter( radeonTexObjPtr t, GLenum minf, GLenum magf )
{
   GLuint anisotropy = (t->pp_txfilter & RADEON_MAX_ANISO_MASK);

   t->pp_txfilter &= ~(RADEON_MIN_FILTER_MASK | RADEON_MAG_FILTER_MASK);

   if ( anisotropy == RADEON_MAX_ANISO_1_TO_1 ) {
      switch ( minf ) {
      case GL_NEAREST:
	 t->pp_txfilter |= RADEON_MIN_FILTER_NEAREST;
	 break;
      case GL_LINEAR:
	 t->pp_txfilter |= RADEON_MIN_FILTER_LINEAR;
	 break;
      case GL_NEAREST_MIPMAP_NEAREST:
	 t->pp_txfilter |= RADEON_MIN_FILTER_NEAREST_MIP_NEAREST;
	 break;
      case GL_NEAREST_MIPMAP_LINEAR:
	 t->pp_txfilter |= RADEON_MIN_FILTER_LINEAR_MIP_NEAREST;
	 break;
      case GL_LINEAR_MIPMAP_NEAREST:
	 t->pp_txfilter |= RADEON_MIN_FILTER_NEAREST_MIP_LINEAR;
	 break;
      case GL_LINEAR_MIPMAP_LINEAR:
	 t->pp_txfilter |= RADEON_MIN_FILTER_LINEAR_MIP_LINEAR;
	 break;
      }
   } else {
      switch ( minf ) {
      case GL_NEAREST:
	 t->pp_txfilter |= RADEON_MIN_FILTER_ANISO_NEAREST;
	 break;
      case GL_LINEAR:
	 t->pp_txfilter |= RADEON_MIN_FILTER_ANISO_LINEAR;
	 break;
      case GL_NEAREST_MIPMAP_NEAREST:
      case GL_LINEAR_MIPMAP_NEAREST:
	 t->pp_txfilter |= RADEON_MIN_FILTER_ANISO_NEAREST_MIP_NEAREST;
	 break;
      case GL_NEAREST_MIPMAP_LINEAR:
      case GL_LINEAR_MIPMAP_LINEAR:
	 t->pp_txfilter |= RADEON_MIN_FILTER_ANISO_NEAREST_MIP_LINEAR;
	 break;
      }
   }

   switch ( magf ) {
   case GL_NEAREST:
      t->pp_txfilter |= RADEON_MAG_FILTER_NEAREST;
      break;
   case GL_LINEAR:
      t->pp_txfilter |= RADEON_MAG_FILTER_LINEAR;
      break;
   }
}

static void radeonSetTexBorderColor( radeonTexObjPtr t, GLubyte c[4] )
{
   t->pp_border_color = radeonPackColor( 4, c[0], c[1], c[2], c[3] );
}


static radeonTexObjPtr radeonAllocTexObj( struct gl_texture_object *texObj )
{
   radeonTexObjPtr t;

   t = CALLOC_STRUCT( radeon_tex_obj );
   if (!t)
      return NULL;

   if ( RADEON_DEBUG & DEBUG_TEXTURE ) {
      fprintf( stderr, "%s( %p, %p )\n", __FUNCTION__, texObj, t );
   }

   t->tObj = texObj;
   make_empty_list( t );

   /* Initialize non-image-dependent parts of the state:
    */
   radeonSetTexWrap( t, texObj->WrapS, texObj->WrapT );
   radeonSetTexMaxAnisotropy( t, texObj->MaxAnisotropy );
   radeonSetTexFilter( t, texObj->MinFilter, texObj->MagFilter );
   radeonSetTexBorderColor( t, texObj->BorderColor );
   return t;
}


static const struct gl_texture_format *
radeonChooseTextureFormat( GLcontext *ctx, GLint internalFormat,
                           GLenum format, GLenum type )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   const GLboolean do32bpt = ( rmesa->radeonScreen->cpp == 4 );

   switch ( internalFormat ) {
   case 4:
   case GL_RGBA:
   case GL_COMPRESSED_RGBA:
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
      return do32bpt ? &_mesa_texformat_rgba8888 : &_mesa_texformat_argb4444;

   case 3:
   case GL_RGB:
   case GL_COMPRESSED_RGB:
      if ( format == GL_RGB && type == GL_UNSIGNED_SHORT_5_6_5 ) {
	 return &_mesa_texformat_rgb565;
      }
      return do32bpt ? &_mesa_texformat_rgba8888 : &_mesa_texformat_rgb565;

   case GL_RGBA8:
   case GL_RGB10_A2:
   case GL_RGBA12:
   case GL_RGBA16:
      return do32bpt ? &_mesa_texformat_rgba8888 : &_mesa_texformat_argb4444;

   case GL_RGBA4:
   case GL_RGBA2:
      return &_mesa_texformat_argb4444;

   case GL_RGB5_A1:
      return &_mesa_texformat_argb1555;

   case GL_RGB8:
   case GL_RGB10:
   case GL_RGB12:
   case GL_RGB16:
      return do32bpt ? &_mesa_texformat_rgba8888 : &_mesa_texformat_rgb565;

   case GL_RGB5:
   case GL_RGB4:
   case GL_R3_G3_B2:
      return &_mesa_texformat_rgb565;

   case GL_ALPHA:
   case GL_ALPHA4:
   case GL_ALPHA8:
   case GL_ALPHA12:
   case GL_ALPHA16:
   case GL_COMPRESSED_ALPHA:
      return &_mesa_texformat_al88;

   case 1:
   case GL_LUMINANCE:
   case GL_LUMINANCE4:
   case GL_LUMINANCE8:
   case GL_LUMINANCE12:
   case GL_LUMINANCE16:
   case GL_COMPRESSED_LUMINANCE:
      return &_mesa_texformat_al88;

   case 2:
   case GL_LUMINANCE_ALPHA:
   case GL_LUMINANCE4_ALPHA4:
   case GL_LUMINANCE6_ALPHA2:
   case GL_LUMINANCE8_ALPHA8:
   case GL_LUMINANCE12_ALPHA4:
   case GL_LUMINANCE12_ALPHA12:
   case GL_LUMINANCE16_ALPHA16:
   case GL_COMPRESSED_LUMINANCE_ALPHA:
      return &_mesa_texformat_al88;

   case GL_INTENSITY:
   case GL_INTENSITY4:
   case GL_INTENSITY8:
   case GL_INTENSITY12:
   case GL_INTENSITY16:
   case GL_COMPRESSED_INTENSITY:
      return &_mesa_texformat_i8;

   default:
      _mesa_problem(ctx, "unexpected texture format in radeonChoosTexFormat");
      return NULL;
   }

   return NULL; /* never get here */
}


static void radeonTexImage1D( GLcontext *ctx, GLenum target, GLint level,
                              GLint internalFormat,
                              GLint width, GLint border,
                              GLenum format, GLenum type, const GLvoid *pixels,
                              const struct gl_pixelstore_attrib *packing,
                              struct gl_texture_object *texObj,
                              struct gl_texture_image *texImage )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   radeonTexObjPtr t = (radeonTexObjPtr) texObj->DriverData;

   if ( t ) {
      radeonSwapOutTexObj( rmesa, t );
   }
   else {
      t = radeonAllocTexObj( texObj );
      if (!t) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage1D");
         return;
      }
      texObj->DriverData = t;
   }

   /* Note, this will call radeonChooseTextureFormat */
   _mesa_store_teximage1d(ctx, target, level, internalFormat,
                          width, border, format, type, pixels,
                          &ctx->Unpack, texObj, texImage);

   t->dirty_images |= (1 << level);
}


static void radeonTexSubImage1D( GLcontext *ctx, GLenum target, GLint level,
                                 GLint xoffset,
                                 GLsizei width,
                                 GLenum format, GLenum type,
                                 const GLvoid *pixels,
                                 const struct gl_pixelstore_attrib *packing,
                                 struct gl_texture_object *texObj,
                                 struct gl_texture_image *texImage )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   radeonTexObjPtr t = (radeonTexObjPtr)texObj->DriverData;

   assert( t ); /* this _should_ be true */
   if ( t ) {
      radeonSwapOutTexObj( rmesa, t );
      t->dirty_images |= (1 << level);
   }
   else {
      t = radeonAllocTexObj(texObj);
      if (!t) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexSubImage1D");
         return;
      }
      texObj->DriverData = t;
   }

   _mesa_store_texsubimage1d(ctx, target, level, xoffset, width,
			     format, type, pixels, packing, texObj,
			     texImage);

   t->dirty_images |= (1 << level);
}


static void radeonTexImage2D( GLcontext *ctx, GLenum target, GLint level,
                              GLint internalFormat,
                              GLint width, GLint height, GLint border,
                              GLenum format, GLenum type, const GLvoid *pixels,
                              const struct gl_pixelstore_attrib *packing,
                              struct gl_texture_object *texObj,
                              struct gl_texture_image *texImage )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   radeonTexObjPtr t = (radeonTexObjPtr)texObj->DriverData;

/*     fprintf(stderr, "%s\n", __FUNCTION__); */

   if ( t ) {
      radeonSwapOutTexObj( rmesa, t );
   }
   else {
      t = radeonAllocTexObj( texObj );
      if (!t) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexImage2D");
         return;
      }
      texObj->DriverData = t;
   }

   /* Note, this will call radeonChooseTextureFormat */
   _mesa_store_teximage2d(ctx, target, level, internalFormat,
                          width, height, border, format, type, pixels,
                          &ctx->Unpack, texObj, texImage);

   t->dirty_images |= (1 << level);
}


static void radeonTexSubImage2D( GLcontext *ctx, GLenum target, GLint level,
                                 GLint xoffset, GLint yoffset,
                                 GLsizei width, GLsizei height,
                                 GLenum format, GLenum type,
                                 const GLvoid *pixels,
                                 const struct gl_pixelstore_attrib *packing,
                                 struct gl_texture_object *texObj,
                                 struct gl_texture_image *texImage )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   radeonTexObjPtr t = (radeonTexObjPtr) texObj->DriverData;

/*     fprintf(stderr, "%s\n", __FUNCTION__); */

   assert( t ); /* this _should_ be true */
   if ( t ) {
      radeonSwapOutTexObj( rmesa, t );
   }
   else {
      t = radeonAllocTexObj(texObj);
      if (!t) {
         _mesa_error(ctx, GL_OUT_OF_MEMORY, "glTexSubImage2D");
         return;
      }
      texObj->DriverData = t;
   }

   _mesa_store_texsubimage2d(ctx, target, level, xoffset, yoffset, width,
			     height, format, type, pixels, packing, texObj,
			     texImage);

   t->dirty_images |= (1 << level);
}



#define SCALED_FLOAT_TO_BYTE( x, scale ) \
		(((GLuint)((255.0F / scale) * (x))) / 2)

static void radeonTexEnv( GLcontext *ctx, GLenum target,
			  GLenum pname, const GLfloat *param )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   GLuint unit = ctx->Texture.CurrentUnit;
   struct gl_texture_unit *texUnit = &ctx->Texture.Unit[unit];

   if ( RADEON_DEBUG & DEBUG_STATE ) {
      fprintf( stderr, "%s( %s )\n",
	       __FUNCTION__, _mesa_lookup_enum_by_nr( pname ) );
   }

   switch ( pname ) {
   case GL_TEXTURE_ENV_COLOR: {
      GLubyte c[4];
      GLuint envColor;
      UNCLAMPED_FLOAT_TO_RGBA_CHAN( c, texUnit->EnvColor );
      envColor = radeonPackColor( 4, c[0], c[1], c[2], c[3] );
      if ( rmesa->hw.tex[unit].cmd[TEX_PP_TFACTOR] != envColor ) {
	 RADEON_STATECHANGE( rmesa, tex[unit] );
	 rmesa->hw.tex[unit].cmd[TEX_PP_TFACTOR] = envColor;
      }
      break;
   }

   case GL_TEXTURE_LOD_BIAS_EXT: {
      GLfloat bias;
      GLuint b;

      /* The Radeon's LOD bias is a signed 2's complement value with a
       * range of -1.0 <= bias < 4.0.  We break this into two linear
       * functions, one mapping [-1.0,0.0] to [-128,0] and one mapping
       * [0.0,4.0] to [0,127].
       */
      bias = CLAMP( *param, -1.0, 4.0 );
      if ( bias == 0 ) {
	 b = 0;
      } else if ( bias > 0 ) {
	 b = ((GLuint)SCALED_FLOAT_TO_BYTE( bias, 4.0 )) << RADEON_LOD_BIAS_SHIFT;
      } else {
	 b = ((GLuint)SCALED_FLOAT_TO_BYTE( bias, 1.0 )) << RADEON_LOD_BIAS_SHIFT;
      }
      if ( (rmesa->hw.tex[unit].cmd[TEX_PP_TXFILTER] & RADEON_LOD_BIAS_MASK) != b ) {
	 RADEON_STATECHANGE( rmesa, tex[unit] );
	 rmesa->hw.tex[unit].cmd[TEX_PP_TXFILTER] &= ~RADEON_LOD_BIAS_MASK;
	 rmesa->hw.tex[unit].cmd[TEX_PP_TXFILTER] |= (b & RADEON_LOD_BIAS_MASK);
      }
      break;
   }

   default:
      return;
   }
}

static void radeonTexParameter( GLcontext *ctx, GLenum target,
				struct gl_texture_object *texObj,
				GLenum pname, const GLfloat *params )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   radeonTexObjPtr t = (radeonTexObjPtr) texObj->DriverData;

   if ( RADEON_DEBUG & (DEBUG_STATE|DEBUG_TEXTURE) ) {
      fprintf( stderr, "%s( %s )\n", __FUNCTION__,
	       _mesa_lookup_enum_by_nr( pname ) );
   }

   if ( ( target != GL_TEXTURE_2D ) &&
	( target != GL_TEXTURE_1D ) )
      return;

   switch ( pname ) {
   case GL_TEXTURE_MIN_FILTER:
   case GL_TEXTURE_MAG_FILTER:
   case GL_TEXTURE_MAX_ANISOTROPY_EXT:
      radeonSetTexMaxAnisotropy( t, texObj->MaxAnisotropy );
      radeonSetTexFilter( t, texObj->MinFilter, texObj->MagFilter );
      break;

   case GL_TEXTURE_WRAP_S:
   case GL_TEXTURE_WRAP_T:
      radeonSetTexWrap( t, texObj->WrapS, texObj->WrapT );
      break;

   case GL_TEXTURE_BORDER_COLOR:
      radeonSetTexBorderColor( t, texObj->BorderColor );
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
      radeonSwapOutTexObj( rmesa, t );
      break;

   default:
      return;
   }

   /* Mark this texobj as dirty (one bit per tex unit)
    */
   t->dirty_state = TEX_ALL;
}



static void radeonBindTexture( GLcontext *ctx, GLenum target,
			       struct gl_texture_object *texObj )
{
   radeonTexObjPtr t = (radeonTexObjPtr) texObj->DriverData;
   GLuint unit = ctx->Texture.CurrentUnit;

   if ( RADEON_DEBUG & (DEBUG_STATE|DEBUG_TEXTURE) ) {
      fprintf( stderr, "%s( %p ) unit=%d\n", __FUNCTION__, texObj, unit );
   }

   if ( target == GL_TEXTURE_2D || target == GL_TEXTURE_1D ) {
      if ( !t ) {
	 t = radeonAllocTexObj( texObj );
	 texObj->DriverData = t;
      }
   }
}

static void radeonDeleteTexture( GLcontext *ctx,
				 struct gl_texture_object *texObj )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   radeonTexObjPtr t = (radeonTexObjPtr) texObj->DriverData;

   if ( RADEON_DEBUG & (DEBUG_STATE|DEBUG_TEXTURE) ) {
      fprintf( stderr, "%s( %p )\n", __FUNCTION__, texObj );
   }

   if ( t ) {
      if ( rmesa ) {
         RADEON_FIREVERTICES( rmesa );
      }
      radeonDestroyTexObj( rmesa, t );
      texObj->DriverData = NULL;
   }
}

static GLboolean radeonIsTextureResident( GLcontext *ctx,
					  struct gl_texture_object *texObj )
{
   radeonTexObjPtr t = (radeonTexObjPtr) texObj->DriverData;

   return ( t && t->memBlock );
}


static void radeonInitTextureObjects( GLcontext *ctx )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   struct gl_texture_object *texObj;
   GLuint tmp = ctx->Texture.CurrentUnit;

   ctx->Texture.CurrentUnit = 0;

   texObj = ctx->Texture.Unit[0].Current1D;
   radeonBindTexture( ctx, GL_TEXTURE_1D, texObj );
   move_to_tail( &rmesa->texture.swapped,
		 (radeonTexObjPtr)texObj->DriverData );

   texObj = ctx->Texture.Unit[0].Current2D;
   radeonBindTexture( ctx, GL_TEXTURE_2D, texObj );
   move_to_tail( &rmesa->texture.swapped,
		 (radeonTexObjPtr)texObj->DriverData );

   ctx->Texture.CurrentUnit = 1;

   texObj = ctx->Texture.Unit[1].Current1D;
   radeonBindTexture( ctx, GL_TEXTURE_1D, texObj );
   move_to_tail( &rmesa->texture.swapped,
		 (radeonTexObjPtr)texObj->DriverData );

   texObj = ctx->Texture.Unit[1].Current2D;
   radeonBindTexture( ctx, GL_TEXTURE_2D, texObj );
   move_to_tail( &rmesa->texture.swapped,
		 (radeonTexObjPtr)texObj->DriverData );

   ctx->Texture.CurrentUnit = tmp;
}

/* Need:  
 *  - Same GEN_MODE for all active bits
 *  - Same EyePlane/ObjPlane for all active bits when using Eye/Obj
 *  - STRQ presumably all supported (matrix means incoming R values
 *    can end up in STQ, this has implications for vertex support,
 *    presumably ok if maos is used, though?)
 *  
 * Basically impossible to do this on the fly - just collect some
 * basic info & do the checks from ValidateState().
 */
static void radeonTexGen( GLcontext *ctx,
			  GLenum coord,
			  GLenum pname,
			  const GLfloat *params )
{
   radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
   GLuint unit = ctx->Texture.CurrentUnit;
   rmesa->recheck_texgen[unit] = GL_TRUE;
}


void radeonInitTextureFuncs( GLcontext *ctx )
{
   ctx->Driver.ChooseTextureFormat	= radeonChooseTextureFormat;
   ctx->Driver.TexImage1D		= radeonTexImage1D;
   ctx->Driver.TexImage2D		= radeonTexImage2D;
   ctx->Driver.TexImage3D		= _mesa_store_teximage3d;
   ctx->Driver.TexSubImage1D		= radeonTexSubImage1D;
   ctx->Driver.TexSubImage2D		= radeonTexSubImage2D;
   ctx->Driver.TexSubImage3D		= _mesa_store_texsubimage3d;
   ctx->Driver.CopyTexImage1D		= _swrast_copy_teximage1d;
   ctx->Driver.CopyTexImage2D		= _swrast_copy_teximage2d;
   ctx->Driver.CopyTexSubImage1D	= _swrast_copy_texsubimage1d;
   ctx->Driver.CopyTexSubImage2D	= _swrast_copy_texsubimage2d;
   ctx->Driver.CopyTexSubImage3D 	= _swrast_copy_texsubimage3d;
   ctx->Driver.TestProxyTexImage	= _mesa_test_proxy_teximage;

   ctx->Driver.BindTexture		= radeonBindTexture;
   ctx->Driver.CreateTexture		= NULL; /* FIXME: Is this used??? */
   ctx->Driver.DeleteTexture		= radeonDeleteTexture;
   ctx->Driver.IsTextureResident	= radeonIsTextureResident;
   ctx->Driver.PrioritizeTexture	= NULL;
   ctx->Driver.ActiveTexture		= NULL;
   ctx->Driver.UpdateTexturePalette	= NULL;

   ctx->Driver.TexEnv			= radeonTexEnv;
   ctx->Driver.TexParameter		= radeonTexParameter;
   ctx->Driver.TexGen                   = radeonTexGen;

   radeonInitTextureObjects( ctx );
}
