/* $XFree86: xc/programs/Xserver/GL/mesa/src/X/xf86glxint.h,v 1.4 2002/02/22 21:45:08 dawes Exp $ */
/**************************************************************************

Copyright 1998-1999 Precision Insight, Inc., Cedar Park, Texas.
All Rights Reserved.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sub license, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice (including the
next paragraph) shall be included in all copies or substantial portions
of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
IN NO EVENT SHALL PRECISION INSIGHT AND/OR ITS SUPPLIERS BE LIABLE FOR
ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

**************************************************************************/

/*
 * Authors:
 *   Kevin E. Martin <kevin@precisioninsight.com>
 *
 */

#ifndef _XF86GLXINT_H_
#define _XF86GLXINT_H_

#include <miscstruct.h>
#include <GL/gl.h>
#include <GL/xmesa.h>

/*  struct __GLcontextRec { */
/*      __GLinterface iface; */
/*      XMesaContext xm_ctx; */
/*  }; */

typedef struct __MESA_screenRec __MESA_screen;
struct __MESA_screenRec {
    int num_vis;
    __GLXvisualConfig *glx_vis;
    XMesaVisual *xm_vis;
    void **private;
};

typedef struct __MESA_bufferRec *__MESA_buffer;
struct __MESA_bufferRec {
    XMesaBuffer xm_buf;
    GLboolean (*fbresize)(__GLdrawableBuffer *buf,
			  GLint x, GLint y, GLuint width, GLuint height, 
			  __GLdrawablePrivate *glPriv, GLuint bufferMask);
    GLboolean (*fbswap)(__GLXdrawablePrivate *glxPriv);
};

extern void __MESA_setVisualConfigs(int nconfigs, __GLXvisualConfig *configs,
				    void **privates);
extern Bool __MESA_initVisuals(VisualPtr *visualp, DepthPtr *depthp,
			       int *nvisualp, int *ndepthp, int *rootDepthp,
			       VisualID *defaultVisp, unsigned long sizes,
			       int bitsPerRGB);
extern Bool __MESA_screenProbe(int screen);

extern void __MESA_resetExtension(void);

extern void __MESA_createBuffer(__GLXdrawablePrivate *glxPriv);
extern GLboolean __MESA_resizeBuffers(__GLdrawableBuffer *buf,
				      GLint x, GLint y,
				      GLuint width, GLuint height, 
				      __GLdrawablePrivate *glPriv,
				      GLuint bufferMask);
extern GLboolean __MESA_swapBuffers(__GLXdrawablePrivate *glxPriv);
extern void __MESA_destroyBuffer(__GLdrawablePrivate *glPriv);

extern __GLinterface *__MESA_createContext(__GLimports *imports,
					   __GLcontextModes *modes,
					   __GLinterface *shareGC);
extern GLboolean __MESA_destroyContext(__GLcontext *gc);
extern GLboolean __MESA_loseCurrent(__GLcontext *gc);
extern GLboolean __MESA_makeCurrent(__GLcontext *gc, __GLdrawablePrivate *oldglPriv);
extern GLboolean __MESA_shareContext(__GLcontext *gc, __GLcontext *gcShare);
extern GLboolean __MESA_copyContext(__GLcontext *dst, const __GLcontext *src,
				GLuint mask);
extern GLboolean __MESA_forceCurrent(__GLcontext *gc);

extern GLboolean __MESA_notifyResize(__GLcontext *gc);
extern void __MESA_notifyDestroy(__GLcontext *gc);
extern void __MESA_notifySwapBuffers(__GLcontext *gc);
extern struct __GLdispatchStateRec *__MESA_dispatchExec(__GLcontext *gc);
extern void __MESA_beginDispatchOverride(__GLcontext *gc);
extern void __MESA_endDispatchOverride(__GLcontext *gc);

extern GLint __glCallLists_size(GLsizei n, GLenum type);
extern GLint __glEvalComputeK(GLenum target);
extern GLuint __glFloorLog2(GLuint val);
extern GLint __glFogfv_size(GLenum pname);
extern GLint __glFogiv_size(GLenum pname);
extern GLint __glLightModelfv_size(GLenum pname);
extern GLint __glLightModeliv_size(GLenum pname);
extern GLint __glLightfv_size(GLenum pname);
extern GLint __glLightiv_size(GLenum pname);
extern GLint __glMaterialfv_size(GLenum pname);
extern GLint __glMaterialiv_size(GLenum pname);
extern GLint __glTexEnvfv_size(GLenum pname);
extern GLint __glTexEnviv_size(GLenum pname);
extern GLint __glTexGendv_size(GLenum pname);
extern GLint __glTexGenfv_size(GLenum pname);
extern GLint __glTexGeniv_size(GLenum pname);
extern GLint __glTexParameterfv_size(GLenum pname);
extern GLint __glTexParameteriv_size(GLenum pname);

#endif /* _XF86GLXINT_H_ */
