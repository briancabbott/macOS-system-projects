/* $XFree86: xc/lib/GL/glx/pixelstore.c,v 1.3 2001/03/21 16:04:39 dawes Exp $ */
/*
** License Applicability. Except to the extent portions of this file are
** made subject to an alternative license as permitted in the SGI Free
** Software License B, Version 1.1 (the "License"), the contents of this
** file are subject only to the provisions of the License. You may not use
** this file except in compliance with the License. You may obtain a copy
** of the License at Silicon Graphics, Inc., attn: Legal Services, 1600
** Amphitheatre Parkway, Mountain View, CA 94043-1351, or at:
** 
** http://oss.sgi.com/projects/FreeB
** 
** Note that, as provided in the License, the Software is distributed on an
** "AS IS" basis, with ALL EXPRESS AND IMPLIED WARRANTIES AND CONDITIONS
** DISCLAIMED, INCLUDING, WITHOUT LIMITATION, ANY IMPLIED WARRANTIES AND
** CONDITIONS OF MERCHANTABILITY, SATISFACTORY QUALITY, FITNESS FOR A
** PARTICULAR PURPOSE, AND NON-INFRINGEMENT.
** 
** Original Code. The Original Code is: OpenGL Sample Implementation,
** Version 1.2.1, released January 26, 2000, developed by Silicon Graphics,
** Inc. The Original Code is Copyright (c) 1991-2000 Silicon Graphics, Inc.
** Copyright in any portions created by third parties is as indicated
** elsewhere herein. All Rights Reserved.
** 
** Additional Notice Provisions: The application programming interfaces
** established by SGI in conjunction with the Original Code are The
** OpenGL(R) Graphics System: A Specification (Version 1.2.1), released
** April 1, 1999; The OpenGL(R) Graphics System Utility Library (Version
** 1.3), released November 4, 1998; and OpenGL(R) Graphics with the X
** Window System(R) (Version 1.3), released October 19, 1998. This software
** was created using the OpenGL(R) version 1.2.1 Sample Implementation
** published by SGI, but has not been independently verified as being
** compliant with the OpenGL(R) version 1.2.1 Specification.
**
*/

#include "packsingle.h"

/*
** Specify parameters that control the storage format of pixel arrays.
*/
void glPixelStoref(GLenum pname, GLfloat param)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    Display *dpy = gc->currentDpy;
    GLuint a;

    if (!dpy) return;

    switch (pname) {
      case GL_PACK_ROW_LENGTH:
	a = (GLuint) (param + 0.5);
	if (((GLint) a) < 0) {
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	gc->state.storePack.rowLength = a;
	break;
      case GL_PACK_IMAGE_HEIGHT:
        a = (GLuint) (param + 0.5);
        if (((GLint) a) < 0) {
            __glXSetError(gc, GL_INVALID_VALUE);
            return;
        }
        gc->state.storePack.imageHeight = a;
        break;
      case GL_PACK_SKIP_ROWS:
	a = (GLuint) (param + 0.5);
	if (((GLint) a) < 0) {
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	gc->state.storePack.skipRows = a;
	break;
      case GL_PACK_SKIP_PIXELS:
	a = (GLuint) (param + 0.5);
	if (((GLint) a) < 0) {
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	gc->state.storePack.skipPixels = a;
	break;
      case GL_PACK_SKIP_IMAGES:
        a = (GLuint) (param + 0.5);
        if (((GLint) a) < 0) {
            __glXSetError(gc, GL_INVALID_VALUE);
            return;
        }
        gc->state.storePack.skipImages = a;
        break;
      case GL_PACK_ALIGNMENT:
	a = (GLint) (param + 0.5);
	switch (a) {
	  case 1: case 2: case 4: case 8:
	    gc->state.storePack.alignment = a;
	    break;
	  default:
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	break;
      case GL_PACK_SWAP_BYTES:
	gc->state.storePack.swapEndian = (param != 0);
	break;
      case GL_PACK_LSB_FIRST:
	gc->state.storePack.lsbFirst = (param != 0);
	break;

      case GL_UNPACK_ROW_LENGTH:
	a = (GLuint) (param + 0.5);
	if (((GLint) a) < 0) {
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	gc->state.storeUnpack.rowLength = a;
	break;
      case GL_UNPACK_IMAGE_HEIGHT:
        a = (GLuint) (param + 0.5);
        if (((GLint) a) < 0) {
            __glXSetError(gc, GL_INVALID_VALUE);
            return;
        }
        gc->state.storeUnpack.imageHeight = a;
        break;
      case GL_UNPACK_SKIP_ROWS:
	a = (GLuint) (param + 0.5);
	if (((GLint) a) < 0) {
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	gc->state.storeUnpack.skipRows = a;
	break;
      case GL_UNPACK_SKIP_PIXELS:
	a = (GLuint) (param + 0.5);
	if (((GLint) a) < 0) {
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	gc->state.storeUnpack.skipPixels = a;
	break;
      case GL_UNPACK_SKIP_IMAGES:
        a = (GLuint) (param + 0.5);
        if (((GLint) a) < 0) {
            __glXSetError(gc, GL_INVALID_VALUE);
            return;
        }
        gc->state.storeUnpack.skipImages = a;
        break;
      case GL_UNPACK_ALIGNMENT:
	a = (GLint) (param + 0.5);
	switch (a) {
	  case 1: case 2: case 4: case 8:
	    gc->state.storeUnpack.alignment = a;
	    break;
	  default:
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	break;
      case GL_UNPACK_SWAP_BYTES:
	gc->state.storeUnpack.swapEndian = (param != 0);
	break;
      case GL_UNPACK_LSB_FIRST:
	gc->state.storeUnpack.lsbFirst = (param != 0);
	break;
      default:
	/*
	** NOTE: there are currently no pixel storage commands that need to
	** be sent to the server.  This may change in future versions
	** of the API, however.
	*/
	__glXSetError(gc, GL_INVALID_ENUM);
	break;
    }
}

void glPixelStorei(GLenum pname, GLint param)
{
    __GLXcontext *gc = __glXGetCurrentContext();
    Display *dpy = gc->currentDpy;

    if (!dpy) return;

    switch (pname) {
      case GL_PACK_ROW_LENGTH:
	if (param < 0) {
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	gc->state.storePack.rowLength = param;
	break;
      case GL_PACK_IMAGE_HEIGHT:
        if (param < 0) {
            __glXSetError(gc, GL_INVALID_VALUE);
            return;
        }
        gc->state.storePack.imageHeight = param;
        break;
      case GL_PACK_SKIP_ROWS:
	if (param < 0) {
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	gc->state.storePack.skipRows = param;
	break;
      case GL_PACK_SKIP_PIXELS:
	if (param < 0) {
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	gc->state.storePack.skipPixels = param;
	break;
      case GL_PACK_SKIP_IMAGES:
        if (param < 0) {
            __glXSetError(gc, GL_INVALID_VALUE);
            return;
        }
        gc->state.storePack.skipImages = param;
        break;
      case GL_PACK_ALIGNMENT:
	switch (param) {
	  case 1: case 2: case 4: case 8:
	    gc->state.storePack.alignment = param;
	    break;
	  default:
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	break;
      case GL_PACK_SWAP_BYTES:
	gc->state.storePack.swapEndian = (param != 0);
	break;
      case GL_PACK_LSB_FIRST:
	gc->state.storePack.lsbFirst = (param != 0);
	break;

      case GL_UNPACK_ROW_LENGTH:
	if (param < 0) {
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	gc->state.storeUnpack.rowLength = param;
	break;
      case GL_UNPACK_IMAGE_HEIGHT:
        if (param < 0) {
            __glXSetError(gc, GL_INVALID_VALUE);
            return;
        }
        gc->state.storeUnpack.imageHeight = param;
        break;
      case GL_UNPACK_SKIP_ROWS:
	if (param < 0) {
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	gc->state.storeUnpack.skipRows = param;
	break;
      case GL_UNPACK_SKIP_PIXELS:
	if (param < 0) {
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	gc->state.storeUnpack.skipPixels = param;
	break;
      case GL_UNPACK_SKIP_IMAGES:
        if (param < 0) {
            __glXSetError(gc, GL_INVALID_VALUE);
            return;
        }
        gc->state.storeUnpack.skipImages = param;
        break;
      case GL_UNPACK_ALIGNMENT:
	switch (param) {
	  case 1: case 2: case 4: case 8:
	    gc->state.storeUnpack.alignment = param;
	    break;
	  default:
	    __glXSetError(gc, GL_INVALID_VALUE);
	    return;
	}
	break;
      case GL_UNPACK_SWAP_BYTES:
	gc->state.storeUnpack.swapEndian = (param != 0);
	break;
      case GL_UNPACK_LSB_FIRST:
	gc->state.storeUnpack.lsbFirst = (param != 0);
	break;
      default:
	/*
	** NOTE: there are currently no pixel storage commands that need to
	** be sent to the server.  This may change in future versions
	** of the API, however.
	*/
	__glXSetError(gc, GL_INVALID_ENUM);
	break;
    }
}
