/* $XFree86: xc/lib/GL/mesa/src/drv/radeon/radeon_tex.h,v 1.3 2002/02/22 21:45:01 dawes Exp $ */
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
 *
 */

#ifndef __RADEON_TEX_H__
#define __RADEON_TEX_H__

#ifdef GLX_DIRECT_RENDERING

extern void radeonUpdateTextureState( GLcontext *ctx );

extern int radeonUploadTexImages( radeonContextPtr rmesa, radeonTexObjPtr t );

extern void radeonAgeTextures( radeonContextPtr rmesa, int heap );
extern void radeonDestroyTexObj( radeonContextPtr rmesa, radeonTexObjPtr t );
extern void radeonSwapOutTexObj( radeonContextPtr rmesa, radeonTexObjPtr t );

extern void radeonPrintLocalLRU( radeonContextPtr rmesa, int heap );
extern void radeonPrintGlobalLRU( radeonContextPtr rmesa, int heap );
extern void radeonUpdateTexLRU( radeonContextPtr rmesa, radeonTexObjPtr t );

extern void radeonInitTextureFuncs( GLcontext *ctx );

#endif
#endif /* __RADEON_TEX_H__ */
