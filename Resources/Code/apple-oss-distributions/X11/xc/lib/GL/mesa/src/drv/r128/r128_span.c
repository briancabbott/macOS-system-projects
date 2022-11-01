/* $XFree86: xc/lib/GL/mesa/src/drv/r128/r128_span.c,v 1.8 2002/10/30 12:51:39 alanh Exp $ */
/**************************************************************************

Copyright 1999, 2000 ATI Technologies Inc. and Precision Insight, Inc.,
                                               Cedar Park, Texas.
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
ATI, PRECISION INSIGHT AND/OR THEIR SUPPLIERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Gareth Hughes <gareth@valinux.com>
 *   Keith Whitwell <keith@tungstengraphics.com>
 *   Kevin E. Martin <martin@valinux.com>
 *
 */

#include "r128_context.h"
#include "r128_ioctl.h"
#include "r128_state.h"
#include "r128_span.h"
#include "r128_tex.h"

#include "swrast/s_pb.h"	/* for PB_SIZE */

#define DBG 0

#define HAVE_HW_DEPTH_SPANS	1
#define HAVE_HW_DEPTH_PIXELS	1

#define LOCAL_VARS							\
   r128ContextPtr rmesa = R128_CONTEXT(ctx);				\
   r128ScreenPtr r128scrn = rmesa->r128Screen;				\
   __DRIscreenPrivate *sPriv = rmesa->driScreen;			\
   __DRIdrawablePrivate *dPriv = rmesa->driDrawable;			\
   GLuint pitch = r128scrn->frontPitch * r128scrn->cpp;			\
   GLuint height = dPriv->h;						\
   char *buf = (char *)(sPriv->pFB +					\
			rmesa->drawOffset +				\
			(dPriv->x * r128scrn->cpp) +			\
			(dPriv->y * pitch));				\
   char *read_buf = (char *)(sPriv->pFB +				\
			     rmesa->readOffset +			\
			     (dPriv->x * r128scrn->cpp) +		\
			     (dPriv->y * pitch));			\
   GLuint p;								\
   (void) read_buf; (void) buf; (void) p

#define LOCAL_DEPTH_VARS						\
   r128ContextPtr rmesa = R128_CONTEXT(ctx);				\
   r128ScreenPtr r128scrn = rmesa->r128Screen;				\
   __DRIscreenPrivate *sPriv = rmesa->driScreen;			\
   __DRIdrawablePrivate *dPriv = rmesa->driDrawable;			\
   GLuint height = dPriv->h;						\
   (void) r128scrn; (void) sPriv; (void) height

#define LOCAL_STENCIL_VARS	LOCAL_DEPTH_VARS


#define CLIPPIXEL( _x, _y )						\
   ((_x >= minx) && (_x < maxx) && (_y >= miny) && (_y < maxy))


#define CLIPSPAN( _x, _y, _n, _x1, _n1, _i )				\
   if ( _y < miny || _y >= maxy ) {					\
      _n1 = 0, _x1 = x;							\
   } else {								\
      _n1 = _n;								\
      _x1 = _x;								\
      if ( _x1 < minx ) _i += (minx-_x1), n1 -= (minx-_x1), _x1 = minx; \
      if ( _x1 + _n1 >= maxx ) n1 -= (_x1 + n1 - maxx);		        \
   }

#define Y_FLIP( _y )		(height - _y - 1)


#define HW_LOCK()							\
   r128ContextPtr rmesa = R128_CONTEXT(ctx);				\
   FLUSH_BATCH( rmesa );						\
   LOCK_HARDWARE( rmesa );						\
   r128WaitForIdleLocked( rmesa );

#define HW_CLIPLOOP()							\
   do {									\
      __DRIdrawablePrivate *dPriv = rmesa->driDrawable;			\
      int _nc = dPriv->numClipRects;					\
									\
      while ( _nc-- ) {							\
	 int minx = dPriv->pClipRects[_nc].x1 - dPriv->x;		\
	 int miny = dPriv->pClipRects[_nc].y1 - dPriv->y;		\
	 int maxx = dPriv->pClipRects[_nc].x2 - dPriv->x;		\
	 int maxy = dPriv->pClipRects[_nc].y2 - dPriv->y;

#define HW_ENDCLIPLOOP()						\
      }									\
   } while (0)

#define HW_UNLOCK()							\
   UNLOCK_HARDWARE( rmesa )



/* ================================================================
 * Color buffer
 */

/* 16 bit, RGB565 color spanline and pixel functions
 */
#undef INIT_MONO_PIXEL
#define INIT_MONO_PIXEL(p, color) \
  p = R128PACKCOLOR565( color[0], color[1], color[2] )

#define WRITE_RGBA( _x, _y, r, g, b, a )				\
   *(GLushort *)(buf + _x*2 + _y*pitch) = ((((int)r & 0xf8) << 8) |	\
					   (((int)g & 0xfc) << 3) |	\
					   (((int)b & 0xf8) >> 3))

#define WRITE_PIXEL( _x, _y, p )					\
   *(GLushort *)(buf + _x*2 + _y*pitch) = p

#define READ_RGBA( rgba, _x, _y )					\
   do {									\
      GLushort p = *(GLushort *)(read_buf + _x*2 + _y*pitch);		\
      rgba[0] = (p >> 8) & 0xf8;					\
      rgba[1] = (p >> 3) & 0xfc;					\
      rgba[2] = (p << 3) & 0xf8;					\
      rgba[3] = 0xff;							\
      if ( rgba[0] & 0x08 ) rgba[0] |= 0x07;				\
      if ( rgba[1] & 0x04 ) rgba[1] |= 0x03;				\
      if ( rgba[2] & 0x08 ) rgba[2] |= 0x07;				\
   } while (0)

#define TAG(x) r128##x##_RGB565
#include "spantmp.h"

#define READ_DEPTH(d, _x, _y)                                                 \
    d = *(GLushort *)(buf + _x*2 + _y*pitch)

/* 32 bit, ARGB8888 color spanline and pixel functions
 */
#undef INIT_MONO_PIXEL
#define INIT_MONO_PIXEL(p, color) \
  p = R128PACKCOLOR8888( color[0], color[1], color[2], color[3] )

#define WRITE_RGBA( _x, _y, r, g, b, a )				\
   *(GLuint *)(buf + _x*4 + _y*pitch) = ((b <<  0) |			\
					 (g <<  8) |			\
					 (r << 16) |			\
					 (a << 24) )

#define WRITE_PIXEL( _x, _y, p )					\
   *(GLuint *)(buf + _x*4 + _y*pitch) = p

#define READ_RGBA( rgba, _x, _y )					\
do {									\
   GLuint p = *(GLuint *)(read_buf + _x*4 + _y*pitch);			\
   rgba[0] = (p >> 16) & 0xff;						\
   rgba[1] = (p >>  8) & 0xff;						\
   rgba[2] = (p >>  0) & 0xff;						\
   rgba[3] = 0xff;/*(p >> 24) & 0xff;*/						\
} while (0)

#define TAG(x) r128##x##_ARGB8888
#include "spantmp.h"


/* 24 bit, RGB888 color spanline and pixel functions */
#undef INIT_MONO_PIXEL
#define INIT_MONO_PIXEL(p, color) \
  p = R128PACKCOLOR888( color[0], color[1], color[2] )

#define WRITE_RGBA(_x, _y, r, g, b, a)                                        \
    *(GLuint *)(buf + _x*3 + _y*pitch) = ((r << 16) |                         \
					  (g << 8)  |                         \
					  (b << 0))

#define WRITE_PIXEL(_x, _y, p)                                                \
    *(GLuint *)(buf + _x*3 + _y*pitch) = p

#define READ_RGBA(rgba, _x, _y)                                               \
    do {                                                                      \
	GLuint p = *(GLuint *)(read_buf + _x*3 + _y*pitch);                   \
	rgba[0] = (p >> 16) & 0xff;                                           \
	rgba[1] = (p >> 8)  & 0xff;                                           \
	rgba[2] = (p >> 0)  & 0xff;                                           \
	rgba[3] = 0xff;                                                       \
    } while (0)

/* ================================================================
 * Depth buffer
 */

/* 16-bit depth buffer functions
 */
#define WRITE_DEPTH_SPAN()						\
   r128WriteDepthSpanLocked( rmesa, n,					\
			     x + dPriv->x,				\
			     y + dPriv->y,				\
			     depth, mask );

#define WRITE_DEPTH_PIXELS()						\
do {									\
   GLint ox[PB_SIZE];							\
   GLint oy[PB_SIZE];							\
   for ( i = 0 ; i < n ; i++ ) {					\
      ox[i] = x[i] + dPriv->x;						\
   }									\
   for ( i = 0 ; i < n ; i++ ) {					\
      oy[i] = Y_FLIP( y[i] ) + dPriv->y;				\
   }									\
   r128WriteDepthPixelsLocked( rmesa, n, ox, oy, depth, mask );		\
} while (0)

#define READ_DEPTH_SPAN()						\
do {									\
   GLushort *buf = (GLushort *)((GLubyte *)sPriv->pFB +			\
				r128scrn->spanOffset);			\
   GLint i;								\
									\
   r128ReadDepthSpanLocked( rmesa, n,					\
			    x + dPriv->x,				\
			    y + dPriv->y );				\
   r128WaitForIdleLocked( rmesa );					\
									\
   for ( i = 0 ; i < n ; i++ ) {					\
      depth[i] = buf[i];						\
   }									\
} while (0)

#define READ_DEPTH_PIXELS()						\
do {									\
   GLushort *buf = (GLushort *)((GLubyte *)sPriv->pFB +			\
				r128scrn->spanOffset);			\
   GLint i, remaining = n;						\
									\
   while ( remaining > 0 ) {						\
      GLint ox[PB_SIZE];						\
      GLint oy[PB_SIZE];						\
      GLint count;							\
									\
      if ( remaining <= 128 ) {						\
	 count = remaining;						\
      } else {								\
	 count = 128;							\
      }									\
      for ( i = 0 ; i < count ; i++ ) {					\
	 ox[i] = x[i] + dPriv->x;					\
      }									\
      for ( i = 0 ; i < count ; i++ ) {					\
	 oy[i] = Y_FLIP( y[i] ) + dPriv->y;				\
      }									\
									\
      r128ReadDepthPixelsLocked( rmesa, count, ox, oy );		\
      r128WaitForIdleLocked( rmesa );					\
									\
      for ( i = 0 ; i < count ; i++ ) {					\
	 depth[i] = buf[i];						\
      }									\
      depth += count;							\
      x += count;							\
      y += count;							\
      remaining -= count;						\
   }									\
} while (0)

#define TAG(x) r128##x##_16
#include "depthtmp.h"


/* 24-bit depth, 8-bit stencil buffer functions
 */
#define WRITE_DEPTH_SPAN()						\
   r128WriteDepthSpanLocked( rmesa, n,					\
			     x + dPriv->x,				\
			     y + dPriv->y,				\
			     depth, mask );

#define WRITE_DEPTH_PIXELS()						\
do {									\
   GLint ox[PB_SIZE];							\
   GLint oy[PB_SIZE];							\
   for ( i = 0 ; i < n ; i++ ) {					\
      ox[i] = x[i] + dPriv->x;						\
   }									\
   for ( i = 0 ; i < n ; i++ ) {					\
      oy[i] = Y_FLIP( y[i] ) + dPriv->y;				\
   }									\
   r128WriteDepthPixelsLocked( rmesa, n, ox, oy, depth, mask );		\
} while (0)

#define READ_DEPTH_SPAN()						\
do {									\
   GLuint *buf = (GLuint *)((GLubyte *)sPriv->pFB +			\
			    r128scrn->spanOffset);			\
   GLint i;								\
									\
   r128ReadDepthSpanLocked( rmesa, n,					\
			    x + dPriv->x,				\
			    y + dPriv->y );				\
   r128WaitForIdleLocked( rmesa );					\
									\
   for ( i = 0 ; i < n ; i++ ) {					\
      depth[i] = buf[i] & 0x00ffffff;					\
   }									\
} while (0)

#define READ_DEPTH_PIXELS()						\
do {									\
   GLuint *buf = (GLuint *)((GLubyte *)sPriv->pFB +			\
			    r128scrn->spanOffset);			\
   GLint i, remaining = n;						\
									\
   while ( remaining > 0 ) {						\
      GLint ox[PB_SIZE];						\
      GLint oy[PB_SIZE];						\
      GLint count;							\
									\
      if ( remaining <= 128 ) {						\
	 count = remaining;						\
      } else {								\
	 count = 128;							\
      }									\
      for ( i = 0 ; i < count ; i++ ) {					\
	 ox[i] = x[i] + dPriv->x;					\
      }									\
      for ( i = 0 ; i < count ; i++ ) {					\
	 oy[i] = Y_FLIP( y[i] ) + dPriv->y;				\
      }									\
									\
      r128ReadDepthPixelsLocked( rmesa, count, ox, oy );		\
      r128WaitForIdleLocked( rmesa );					\
									\
      for ( i = 0 ; i < count ; i++ ) {					\
	 depth[i] = buf[i] & 0x00ffffff;				\
      }									\
      depth += count;							\
      x += count;							\
      y += count;							\
      remaining -= count;						\
   }									\
} while (0)

#define TAG(x) r128##x##_24_8
#include "depthtmp.h"



/* ================================================================
 * Stencil buffer
 */

/* FIXME: Add support for hardware stencil buffers.
 */


/* 32 bit depthbuffer functions */
#define WRITE_DEPTH(_x, _y, d)                                                \
    *(GLuint *)(buf + _x*4 + _y*pitch) = d



static void r128DDSetReadBuffer( GLcontext *ctx,
				 GLframebuffer *colorBuffer,
				 GLenum mode )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);

   switch ( mode ) {
   case GL_FRONT_LEFT:
      rmesa->readOffset = rmesa->r128Screen->frontOffset;
      rmesa->readPitch  = rmesa->r128Screen->frontPitch;
      break;
   case GL_BACK_LEFT:
      rmesa->readOffset = rmesa->r128Screen->backOffset;
      rmesa->readPitch  = rmesa->r128Screen->backPitch;
      break;
   default:
      break;
   }
}


void r128DDInitSpanFuncs( GLcontext *ctx )
{
   r128ContextPtr rmesa = R128_CONTEXT(ctx);
   struct swrast_device_driver *swdd = _swrast_GetDeviceDriverReference(ctx);

   swdd->SetReadBuffer = r128DDSetReadBuffer;

   switch ( rmesa->r128Screen->cpp ) {
   case 2:
      swdd->WriteRGBASpan	= r128WriteRGBASpan_RGB565;
      swdd->WriteRGBSpan	= r128WriteRGBSpan_RGB565;
      swdd->WriteMonoRGBASpan	= r128WriteMonoRGBASpan_RGB565;
      swdd->WriteRGBAPixels	= r128WriteRGBAPixels_RGB565;
      swdd->WriteMonoRGBAPixels	= r128WriteMonoRGBAPixels_RGB565;
      swdd->ReadRGBASpan	= r128ReadRGBASpan_RGB565;
      swdd->ReadRGBAPixels	= r128ReadRGBAPixels_RGB565;
      break;

   case 4:
      swdd->WriteRGBASpan	= r128WriteRGBASpan_ARGB8888;
      swdd->WriteRGBSpan	= r128WriteRGBSpan_ARGB8888;
      swdd->WriteMonoRGBASpan	= r128WriteMonoRGBASpan_ARGB8888;
      swdd->WriteRGBAPixels	= r128WriteRGBAPixels_ARGB8888;
      swdd->WriteMonoRGBAPixels	= r128WriteMonoRGBAPixels_ARGB8888;
      swdd->ReadRGBASpan	= r128ReadRGBASpan_ARGB8888;
      swdd->ReadRGBAPixels	= r128ReadRGBAPixels_ARGB8888;
      break;

   default:
      break;
   }

   switch ( rmesa->glCtx->Visual.depthBits ) {
   case 16:
      swdd->ReadDepthSpan	= r128ReadDepthSpan_16;
      swdd->WriteDepthSpan	= r128WriteDepthSpan_16;
      swdd->ReadDepthPixels	= r128ReadDepthPixels_16;
      swdd->WriteDepthPixels	= r128WriteDepthPixels_16;
      break;

   case 24:
      swdd->ReadDepthSpan	= r128ReadDepthSpan_24_8;
      swdd->WriteDepthSpan	= r128WriteDepthSpan_24_8;
      swdd->ReadDepthPixels	= r128ReadDepthPixels_24_8;
      swdd->WriteDepthPixels	= r128WriteDepthPixels_24_8;
      break;

   default:
      break;
   }

   swdd->WriteCI8Span		= NULL;
   swdd->WriteCI32Span		= NULL;
   swdd->WriteMonoCISpan	= NULL;
   swdd->WriteCI32Pixels	= NULL;
   swdd->WriteMonoCIPixels	= NULL;
   swdd->ReadCI32Span		= NULL;
   swdd->ReadCI32Pixels		= NULL;
}
