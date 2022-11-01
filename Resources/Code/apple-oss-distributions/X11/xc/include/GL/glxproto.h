#ifndef _GLX_glxproto_h_
#define _GLX_glxproto_h_

/* $XFree86: xc/include/GL/glxproto.h,v 1.5 2001/08/01 00:44:34 tsi Exp $ */
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
*/

#include <GL/glxmd.h>

/*****************************************************************************/

/*
** Errrors.
*/
#define GLXBadContext		0
#define GLXBadContextState	1
#define GLXBadDrawable		2
#define GLXBadPixmap		3
#define GLXBadContextTag	4
#define GLXBadCurrentWindow	5
#define GLXBadRenderRequest	6
#define GLXBadLargeRequest	7
#define GLXUnsupportedPrivateRequest	8
#define GLXBadFBConfig		9
#define GLXBadPbuffer		10
#define GLXBadCurrentDrawable	11
#define GLXBadWindow		12

#define __GLX_NUMBER_ERRORS 12

/*
** Events.
** __GLX_NUMBER_EVENTS is set to 17 to account for the BufferClobberSGIX
**  event - this helps initialization if the server supports the pbuffer
**  extension and the client doesn't.
*/
#define GLX_PbufferClobber	0

#define __GLX_NUMBER_EVENTS 17

#define GLX_EXTENSION_NAME	"GLX"
#define GLX_EXTENSION_ALIAS	"SGI-GLX"

#define __GLX_MAX_CONTEXT_PROPS 3

#define GLX_VENDOR		0x1
#define GLX_VERSION		0x2
#define GLX_EXTENSIONS		0x3

/*****************************************************************************/

/*
** For the structure definitions in this file, we must redefine these types in
** terms of Xmd.h types, which may include bitfields.  All of these are
** undef'ed at the end of this file, restoring the definitions in glx.h.
*/
#define GLXContextID CARD32
#define GLXPixmap CARD32
#define GLXDrawable CARD32
#define GLXPbuffer CARD32
#define GLXWindow CARD32
#define GLXFBConfigID CARD32
#define GLXFBConfigIDSGIX CARD32
#define GLXPbufferSGIX CARD32

/*
** ContextTag is not exposed to the API.
*/
typedef CARD32 GLXContextTag;

/*****************************************************************************/

/*
** Sizes of basic wire types.
*/
#define __GLX_SIZE_INT8		1
#define __GLX_SIZE_INT16	2
#define __GLX_SIZE_INT32	4
#define __GLX_SIZE_CARD8	1
#define __GLX_SIZE_CARD16	2
#define __GLX_SIZE_CARD32	4
#define __GLX_SIZE_FLOAT32	4
#define __GLX_SIZE_FLOAT64	8

/*****************************************************************************/

/* Requests */

/*
** Render command request.  A bunch of rendering commands are packed into
** a single X extension request.
*/
typedef struct GLXRender {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXContextTag contextTag B32;
} xGLXRenderReq;
#define sz_xGLXRenderReq 8

/*
** The maximum size that a GLXRender command can be.  The value must fit
** in 16 bits and should be a multiple of 4.
*/
#define __GLX_MAX_RENDER_CMD_SIZE	64000

/*
** Large render command request.  A single large rendering command
** is output in multiple X extension requests.	The first packet
** contains an opcode dependent header (see below) that describes
** the data that follows.
*/
typedef struct GLXRenderLarge {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXContextTag contextTag B32;
    CARD16	requestNumber B16;
    CARD16	requestTotal B16;
    CARD32	dataBytes B32;
} xGLXRenderLargeReq;
#define sz_xGLXRenderLargeReq 16

/*
** GLX single request.	Commands that go over as single GLX protocol
** requests use this structure.  The glxCode will be one of the X_GLsop
** opcodes.
*/
typedef struct GLXSingle {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXContextTag contextTag B32;
} xGLXSingleReq;
#define sz_xGLXSingleReq 8

/*
** glXQueryVersion request
*/
typedef struct GLXQueryVersion {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    CARD32	majorVersion B32;
    CARD32	minorVersion B32;
} xGLXQueryVersionReq;
#define sz_xGLXQueryVersionReq 12

/*
** glXIsDirect request
*/
typedef struct GLXIsDirect {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXContextID context B32;
} xGLXIsDirectReq;
#define sz_xGLXIsDirectReq 8

/*
** glXCreateContext request
*/
typedef struct GLXCreateContext {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXContextID context B32;
    CARD32	visual B32;
    CARD32	screen B32;
    GLXContextID shareList B32;
    BOOL	isDirect;
    CARD8	reserved1;
    CARD16	reserved2 B16;
} xGLXCreateContextReq;
#define sz_xGLXCreateContextReq 24

/*
** glXDestroyContext request
*/
typedef struct GLXDestroyContext {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXContextID context B32;
} xGLXDestroyContextReq;
#define sz_xGLXDestroyContextReq 8

/*
** glXMakeCurrent request
*/
typedef struct GLXMakeCurrent {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXDrawable drawable B32;
    GLXContextID context B32;
    GLXContextTag oldContextTag B32;
} xGLXMakeCurrentReq;
#define sz_xGLXMakeCurrentReq 16

/*
** glXWaitGL request
*/
typedef struct GLXWaitGL {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXContextTag contextTag B32;
} xGLXWaitGLReq;
#define sz_xGLXWaitGLReq 8

/*
** glXWaitX request
*/
typedef struct GLXWaitX {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXContextTag contextTag B32;
} xGLXWaitXReq;
#define sz_xGLXWaitXReq 8

/*
** glXCopyContext request
*/
typedef struct GLXCopyContext {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXContextID source B32;
    GLXContextID dest B32;
    CARD32	mask B32;
    GLXContextTag contextTag B32;
} xGLXCopyContextReq;
#define sz_xGLXCopyContextReq 20

/*
** glXSwapBuffers request
*/
typedef struct GLXSwapBuffers {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXContextTag contextTag B32;
    GLXDrawable drawable B32;
} xGLXSwapBuffersReq;
#define sz_xGLXSwapBuffersReq 12

/*
** glXUseXFont request
*/
typedef struct GLXUseXFont {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXContextTag contextTag B32;
    CARD32	font B32;
    CARD32	first B32;
    CARD32	count B32;
    CARD32	listBase B32;
} xGLXUseXFontReq;
#define sz_xGLXUseXFontReq 24

/*
** glXCreateGLXPixmap request
*/
typedef struct GLXCreateGLXPixmap {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    CARD32	screen B32;
    CARD32	visual B32;
    CARD32	pixmap B32;
    GLXPixmap	glxpixmap B32;
} xGLXCreateGLXPixmapReq;
#define sz_xGLXCreateGLXPixmapReq 20

/*
** glXDestroyGLXPixmap request
*/
typedef struct GLXDestroyGLXPixmap {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXPixmap	glxpixmap B32;
} xGLXDestroyGLXPixmapReq;
#define sz_xGLXDestroyGLXPixmapReq 8

/*
** glXGetVisualConfigs request
*/
typedef struct GLXGetVisualConfigs {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    CARD32	screen B32;
} xGLXGetVisualConfigsReq;
#define sz_xGLXGetVisualConfigsReq 8

/*
** glXVendorPrivate request.
*/
typedef struct GLXVendorPrivate {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    CARD32	vendorCode B32;		/* vendor-specific opcode */
    GLXContextTag contextTag B32;
    /*
    ** More data may follow; this is just the header.
    */
} xGLXVendorPrivateReq;
#define sz_xGLXVendorPrivateReq 12

/*
** glXVendorPrivateWithReply request
*/
typedef struct GLXVendorPrivateWithReply {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    CARD32	vendorCode B32;		/* vendor-specific opcode */
    GLXContextTag contextTag B32;
    /*
    ** More data may follow; this is just the header.
    */
} xGLXVendorPrivateWithReplyReq;
#define sz_xGLXVendorPrivateWithReplyReq 12

/*
** glXQueryExtensionsString request
*/
typedef struct GLXQueryExtensionsString {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    CARD32	screen B32;
} xGLXQueryExtensionsStringReq;
#define sz_xGLXQueryExtensionsStringReq 8

/*
** glXQueryServerString request
*/
typedef struct GLXQueryServerString {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    CARD32	screen	B32;
    CARD32	name B32;
} xGLXQueryServerStringReq;
#define sz_xGLXQueryServerStringReq 12

/*
** glXClientInfo request
*/
typedef struct GLXClientInfo {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    CARD32	major B32;
    CARD32	minor B32;
    CARD32	numbytes B32;
} xGLXClientInfoReq;
#define sz_xGLXClientInfoReq 16

/*** Start of GLX 1.3 requests */

/*
** glXGetFBConfigs request
*/
typedef struct GLXGetFBConfigs {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    CARD32	screen B32;
} xGLXGetFBConfigsReq;
#define sz_xGLXGetFBConfigsReq 8

/*
** glXCreatePixmap request
*/
typedef struct GLXCreatePixmap {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    CARD32	screen B32;
    GLXFBConfigID fbconfig B32;
    CARD32	pixmap B32;
    GLXPixmap	glxpixmap B32;
    CARD32	numAttribs B32;
    /* followed by attribute list */
} xGLXCreatePixmapReq;
#define sz_xGLXCreatePixmapReq 24

/*
** glXDestroyPixmap request
*/
typedef struct GLXDestroyPixmap {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXPixmap	glxpixmap B32;
} xGLXDestroyPixmapReq;
#define sz_xGLXDestroyPixmapReq 8

/*
** glXCreateNewContext request
*/
typedef struct GLXCreateNewContext {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXContextID context B32;
    GLXFBConfigID fbconfig B32;
    CARD32	screen B32;
    CARD32	renderType;
    GLXContextID shareList B32;
    BOOL	isDirect;
    CARD8	reserved1;
    CARD16	reserved2 B16;
} xGLXCreateNewContextReq;
#define sz_xGLXCreateNewContextReq 28

/*
** glXQueryContext request
*/
typedef struct GLXQueryContext {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXContextID context B32;
} xGLXQueryContextReq;
#define sz_xGLXQueryContextReq 8

/*
** glXMakeContextCurrent request
*/
typedef struct GLXMakeContextCurrent {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXContextTag oldContextTag B32;
    GLXDrawable drawable B32;
    GLXDrawable readdrawable B32;
    GLXContextID context B32;
} xGLXMakeContextCurrentReq;
#define sz_xGLXMakeContextCurrentReq 20

/*
** glXCreatePbuffer request
*/
typedef struct GLXCreatePbuffer {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    CARD32	screen B32;
    GLXFBConfigID fbconfig B32;
    GLXPbuffer	pbuffer B32;
    CARD32	numAttribs B32;
    /* followed by attribute list */
} xGLXCreatePbufferReq;
#define sz_xGLXCreatePbufferReq 20

/*
** glXDestroyPbuffer request
*/
typedef struct GLXDestroyPbuffer {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXPbuffer	pbuffer B32;
} xGLXDestroyPbufferReq;
#define sz_xGLXDestroyPbufferReq 8

/*
** glXGetDrawableAttributes request
*/
typedef struct GLXGetDrawableAttributes {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXDrawable drawable B32;
} xGLXGetDrawableAttributesReq;
#define sz_xGLXGetDrawableAttributesReq 8

/*
** glXChangeDrawableAttributes request
*/
typedef struct GLXChangeDrawableAttributes {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXDrawable drawable B32;
    CARD32	numAttribs B32;
    /* followed by attribute list */
} xGLXChangeDrawableAttributesReq;
#define sz_xGLXChangeDrawableAttributesReq 12

/*
** glXCreateWindow request
*/
typedef struct GLXCreateWindow {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    CARD32	screen B32;
    GLXFBConfigID fbconfig B32;
    CARD32	window B32;
    GLXWindow	glxwindow B32;
    CARD32	numAttribs B32;
    /* followed by attribute list */
} xGLXCreateWindowReq;
#define sz_xGLXCreateWindowReq 24

/*
** glXDestroyWindow request
*/
typedef struct GLXDestroyWindow {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    GLXWindow	glxwindow B32;
} xGLXDestroyWindowReq;
#define sz_xGLXDestroyWindowReq 8

/* Replies */

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	error B32;
    CARD32	pad2 B32;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXGetErrorReply;
#define sz_xGLXGetErrorReply 32

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    GLXContextTag contextTag B32;
    CARD32	pad2 B32;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXMakeCurrentReply;
#define sz_xGLXMakeCurrentReply 32

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	pad1 B32;
    CARD32	pad2 B32;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXReadPixelsReply;
#define sz_xGLXReadPixelsReply 32

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	pad1 B32;
    CARD32	pad2 B32;
    CARD32	width B32;
    CARD32	height B32;
    CARD32	depth B32;
    CARD32	pad6 B32;
} xGLXGetTexImageReply;
#define sz_xGLXGetTexImageReply 32

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	pad1 B32;
    CARD32	pad2 B32;
    CARD32	width B32;
    CARD32	height B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXGetSeparableFilterReply;
#define sz_xGLXGetSeparableFilterReply 32

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	pad1 B32;
    CARD32	pad2 B32;
    CARD32	width B32;
    CARD32	height B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXGetConvolutionFilterReply;
#define sz_xGLXGetConvolutionFilterReply 32

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	pad1 B32;
    CARD32	pad2 B32;
    CARD32	width B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXGetHistogramReply;
#define sz_xGLXGetHistogramReply 32

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	pad1 B32;
    CARD32	pad2 B32;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXGetMinmaxReply;
#define sz_xGLXGetMinmaxReply 32

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	retval B32;
    CARD32	size B32;
    CARD32	newMode B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXRenderModeReply;
#define sz_xGLXRenderModeReply 32

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	majorVersion B32;
    CARD32	minorVersion B32;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXQueryVersionReply;
#define sz_xGLXQueryVersionReply 32

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	numVisuals B32;
    CARD32	numProps B32;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXGetVisualConfigsReply;
#define sz_xGLXGetVisualConfigsReply 32

typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    BOOL	isDirect;
    CARD8	pad1;
    CARD16	pad2 B16;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
    CARD32	pad7 B32;
} xGLXIsDirectReply;
#define sz_xGLXIsDirectReply	32

/*
** This reply structure is used for all single replies.  Single replies
** ship either 1 piece of data or N pieces of data.  In these cases
** size indicates how much data is to be returned.
*/
typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	retval B32;
    CARD32	size B32;
    CARD32	pad3 B32;		/* NOTE: may hold a single value */
    CARD32	pad4 B32;		/* NOTE: may hold half a double */
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXSingleReply;
#define sz_xGLXSingleReply 32

/*
** This reply structure is used for all Vendor Private replies. Vendor
** Private replies can ship up to 24 bytes within the header or can
** be variable sized, in which case, the reply length field indicates
** the number of words of data which follow the header.
*/
typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	retval B32;
    CARD32	size B32;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXVendorPrivReply;
#define sz_xGLXVendorPrivReply 32

/*
**  QueryExtensionsStringReply
**  n indicates the number of bytes to be returned.
*/
typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	pad1 B32;
    CARD32	n B32;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXQueryExtensionsStringReply;
#define sz_xGLXQueryExtensionsStringReply 32

/*
** QueryServerString Reply struct
** n indicates the number of bytes to be returned.
*/
typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	pad1 B32;
    CARD32	n B32;
    CARD32	pad3 B32;		/* NOTE: may hold a single value */
    CARD32	pad4 B32;		/* NOTE: may hold half a double */
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXQueryServerStringReply;
#define sz_xGLXQueryServerStringReply 32

/*** Start of GLX 1.3 replies */

/*
** glXGetFBConfigs reply
*/
typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	numFBConfigs B32;
    CARD32	numAttribs B32;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXGetFBConfigsReply;
#define sz_xGLXGetFBConfigsReply 32

/*
** glXQueryContext reply
*/
typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	n B32;			/* number of attribute/value pairs */
    CARD32	pad2 B32;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXQueryContextReply;
#define sz_xGLXQueryContextReply 32

/*
** glXMakeContextCurrent reply
*/
typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    GLXContextTag contextTag B32;
    CARD32	pad2 B32;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXMakeContextCurrentReply;
#define sz_xGLXMakeContextCurrentReply 32

/*
** glXCreateGLXPbuffer reply
** This is used only in the direct rendering case on SGIs - otherwise
**  CreateGLXPbuffer has no reply. It is not part of GLX 1.3.
*/
typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	success;
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	pad1 B32;
    CARD32	pad2 B32;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXCreateGLXPbufferReply;
#define sz_xGLXCreateGLXPbufferReply 32

/*
** glXGetDrawableAttributes reply
*/
typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	numAttribs B32;
    CARD32	pad2 B32;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXGetDrawableAttributesReply;
#define sz_xGLXGetDrawableAttributesReply 32

/*
** glXGetColorTable reply
*/
typedef struct {
    BYTE	type;		       /* X_Reply */
    CARD8	unused;		       /* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	pad1 B32;
    CARD32	pad2 B32;
    CARD32	width B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXGetColorTableReply;
#define sz_xGLXGetColorTableReply 32

/************************************************************************/

/* GLX extension requests and replies */

/*
** glXQueryContextInfoEXT request
*/
typedef struct GLXQueryContextInfoEXT {
    CARD8	reqType;
    CARD8	glxCode;
    CARD16	length B16;
    CARD32	vendorCode B32;		/* vendor-specific opcode */
    CARD32	pad1 B32;   /* unused; corresponds to contextTag in header */
    GLXContextID context B32;
} xGLXQueryContextInfoEXTReq;
#define sz_xGLXQueryContextInfoEXTReq 16

/*
** glXQueryContextInfoEXT reply
*/
typedef struct {
    BYTE	type;			/* X_Reply */
    CARD8	unused;			/* not used */
    CARD16	sequenceNumber B16;
    CARD32	length B32;
    CARD32	n B32;			/* number of attribute/value pairs */
    CARD32	pad2 B32;
    CARD32	pad3 B32;
    CARD32	pad4 B32;
    CARD32	pad5 B32;
    CARD32	pad6 B32;
} xGLXQueryContextInfoEXTReply;
#define sz_xGLXQueryContextInfoEXTReply 32

/************************************************************************/

/*
** Events
*/

typedef struct {
    BYTE type;
    BYTE pad;
    CARD16 sequenceNumber B16;
    CARD16 event_type B16;  /*** was clobber_class */
    CARD16 draw_type B16;
    CARD32 drawable B32;
    CARD32 buffer_mask B32; /*** was mask */
    CARD16 aux_buffer B16;
    CARD16 x B16;
    CARD16 y B16;
    CARD16 width B16;
    CARD16 height B16;
    CARD16 count B16;
    CARD32 unused2 B32;
} xGLXPbufferClobberEvent;

/************************************************************************/

/*
** Size of the standard X request header.
*/
#define __GLX_SINGLE_HDR_SIZE sz_xGLXSingleReq
#define __GLX_VENDPRIV_HDR_SIZE sz_xGLXVendorPrivateReq

#define __GLX_RENDER_HDR    \
    CARD16	length B16; \
    CARD16	opcode B16

#define __GLX_RENDER_HDR_SIZE 4

typedef struct {
    __GLX_RENDER_HDR;
} __GLXrenderHeader;

#define __GLX_RENDER_LARGE_HDR \
    CARD32	length B32;    \
    CARD32	opcode B32

#define __GLX_RENDER_LARGE_HDR_SIZE 8

typedef struct {
    __GLX_RENDER_LARGE_HDR;
} __GLXrenderLargeHeader;

/*
** The glBitmap, glPolygonStipple, glTexImage[12]D, glTexSubImage[12]D
** and glDrawPixels calls all have a pixel header transmitted after the
** Render or RenderLarge header and before their own opcode specific
** headers.
*/
#define __GLX_PIXEL_HDR		\
    BOOL	swapBytes;	\
    BOOL	lsbFirst;	\
    CARD8	reserved0;	\
    CARD8	reserved1;	\
    CARD32	rowLength B32;	\
    CARD32	skipRows B32;	\
    CARD32	skipPixels B32; \
    CARD32	alignment B32

#define __GLX_PIXEL_HDR_SIZE 20

typedef struct {
    __GLX_PIXEL_HDR;
} __GLXpixelHeader;

/*
** glTexImage[34]D and glTexSubImage[34]D calls
** all have a pixel header transmitted after the Render or RenderLarge
** header and before their own opcode specific headers.
*/
#define __GLX_PIXEL_3D_HDR		\
    BOOL	swapBytes;		\
    BOOL	lsbFirst;		\
    CARD8	reserved0;		\
    CARD8	reserved1;		\
    CARD32	rowLength B32;		\
    CARD32	imageHeight B32;	\
    CARD32	imageDepth B32;		\
    CARD32	skipRows B32;		\
    CARD32	skipImages B32;		\
    CARD32	skipVolumes B32;	\
    CARD32	skipPixels B32;		\
    CARD32	alignment B32

#define __GLX_PIXEL_3D_HDR_SIZE 36

/*
** Data that is specific to a glBitmap call.  The data is sent in the
** following order:
**	Render or RenderLarge header
**	Pixel header
**	Bitmap header
*/
#define __GLX_BITMAP_HDR    \
    CARD32	width B32;  \
    CARD32	height B32; \
    FLOAT32	xorig F32;  \
    FLOAT32	yorig F32;  \
    FLOAT32	xmove F32;  \
    FLOAT32	ymove F32

typedef struct {
    __GLX_RENDER_HDR;
    __GLX_PIXEL_HDR;
    __GLX_BITMAP_HDR;
} __GLXbitmapHeader;

typedef struct {
    __GLX_RENDER_LARGE_HDR;
    __GLX_PIXEL_HDR;
    __GLX_BITMAP_HDR;
} __GLXbitmapLargeHeader;

typedef struct {
    __GLX_PIXEL_HDR;
    __GLX_BITMAP_HDR;
} __GLXdispatchBitmapHeader;

#define __GLX_BITMAP_HDR_SIZE 24

#define __GLX_BITMAP_CMD_HDR_SIZE \
    (__GLX_RENDER_HDR_SIZE + __GLX_PIXEL_HDR_SIZE + __GLX_BITMAP_HDR_SIZE)

#define __GLX_BITMAP_CMD_DISPATCH_HDR_SIZE \
    (__GLX_PIXEL_HDR_SIZE + __GLX_BITMAP_HDR_SIZE)

typedef struct {
    __GLX_RENDER_HDR;
    __GLX_PIXEL_HDR;
} __GLXpolygonStippleHeader;

#define __GLX_POLYGONSTIPPLE_CMD_HDR_SIZE \
    (__GLX_RENDER_HDR_SIZE + __GLX_PIXEL_HDR_SIZE)

/*
** Data that is specific to a glTexImage1D or glTexImage2D call.  The
** data is sent in the following order:
**	Render or RenderLarge header
**	Pixel header
**	TexImage header
** When a glTexImage1D call the height field is unexamined by the server.
*/
#define __GLX_TEXIMAGE_HDR	\
    CARD32	target B32;	\
    CARD32	level B32;	\
    CARD32	components B32; \
    CARD32	width B32;	\
    CARD32	height B32;	\
    CARD32	border B32;	\
    CARD32	format B32;	\
    CARD32	type B32

#define __GLX_TEXIMAGE_HDR_SIZE 32

#define __GLX_TEXIMAGE_CMD_HDR_SIZE \
    (__GLX_RENDER_HDR_SIZE + __GLX_PIXEL_HDR_SIZE + __GLX_TEXIMAGE_HDR_SIZE)

#define __GLX_TEXIMAGE_CMD_DISPATCH_HDR_SIZE \
    (__GLX_PIXEL_HDR_SIZE + __GLX_TEXIMAGE_HDR_SIZE)

typedef struct {
    __GLX_RENDER_HDR;
    __GLX_PIXEL_HDR;
    __GLX_TEXIMAGE_HDR;
} __GLXtexImageHeader;

typedef struct {
    __GLX_RENDER_LARGE_HDR;
    __GLX_PIXEL_HDR;
    __GLX_TEXIMAGE_HDR;
} __GLXtexImageLargeHeader;

typedef struct {
    __GLX_PIXEL_HDR;
    __GLX_TEXIMAGE_HDR;
} __GLXdispatchTexImageHeader;

/*
** Data that is specific to a glTexImage3D or glTexImage4D call.  The
** data is sent in the following order:
**	Render or RenderLarge header
**	Pixel 3D header
**	TexImage 3D header
** When a glTexImage3D call the size4d and woffset fields are unexamined
** by the server.
** Could be used by all TexImage commands and perhaps should be in the
** future.
*/
#define __GLX_TEXIMAGE_3D_HDR \
    CARD32	target B32;	\
    CARD32	level B32;	\
    CARD32	internalformat B32;	\
    CARD32	width B32;	\
    CARD32	height B32;	\
    CARD32	depth B32;	\
    CARD32	size4d B32;	\
    CARD32	border B32;	\
    CARD32	format B32;	\
    CARD32	type B32;	\
    CARD32	nullimage B32

#define __GLX_TEXIMAGE_3D_HDR_SIZE 44

#define __GLX_TEXIMAGE_3D_CMD_HDR_SIZE \
    (__GLX_RENDER_HDR_SIZE + __GLX_PIXEL_3D_HDR_SIZE + \
		__GLX_TEXIMAGE_3D_HDR_SIZE)

#define __GLX_TEXIMAGE_3D_CMD_DISPATCH_HDR_SIZE \
    (__GLX_PIXEL_3D_HDR_SIZE + __GLX_TEXIMAGE_3D_HDR_SIZE)

typedef struct {
    __GLX_RENDER_HDR;
    __GLX_PIXEL_3D_HDR;
    __GLX_TEXIMAGE_3D_HDR;
} __GLXtexImage3DHeader;

typedef struct {
    __GLX_RENDER_LARGE_HDR;
    __GLX_PIXEL_3D_HDR;
    __GLX_TEXIMAGE_3D_HDR;
} __GLXtexImage3DLargeHeader;

typedef struct {
    __GLX_PIXEL_3D_HDR;
    __GLX_TEXIMAGE_3D_HDR;
} __GLXdispatchTexImage3DHeader;

/*
** Data that is specific to a glTexSubImage1D or glTexSubImage2D call.	The
** data is sent in the following order:
**	Render or RenderLarge header
**	Pixel header
**	TexSubImage header
** When a glTexSubImage1D call is made, the yoffset and height fields
** are unexamined by the server and are  considered to be padding.
*/
#define __GLX_TEXSUBIMAGE_HDR	\
    CARD32	target B32;	\
    CARD32	level B32;	\
    CARD32	xoffset B32;	\
    CARD32	yoffset B32;	\
    CARD32	width B32;	\
    CARD32	height B32;	\
    CARD32	format B32;	\
    CARD32	type B32;	\
    CARD32	nullImage	\

#define __GLX_TEXSUBIMAGE_HDR_SIZE 36

#define __GLX_TEXSUBIMAGE_CMD_HDR_SIZE \
    (__GLX_RENDER_HDR_SIZE + __GLX_PIXEL_HDR_SIZE + __GLX_TEXSUBIMAGE_HDR_SIZE)

#define __GLX_TEXSUBIMAGE_CMD_DISPATCH_HDR_SIZE \
    (__GLX_PIXEL_HDR_SIZE + __GLX_TEXSUBIMAGE_HDR_SIZE)

typedef struct {
    __GLX_RENDER_HDR;
    __GLX_PIXEL_HDR;
    __GLX_TEXSUBIMAGE_HDR;
} __GLXtexSubImageHeader;

typedef struct {
    __GLX_RENDER_LARGE_HDR;
    __GLX_PIXEL_HDR;
    __GLX_TEXSUBIMAGE_HDR;
} __GLXtexSubImageLargeHeader;

typedef struct {
    __GLX_PIXEL_HDR;
    __GLX_TEXSUBIMAGE_HDR;
} __GLXdispatchTexSubImageHeader;

/*
** Data that is specific to a glTexSubImage3D and 4D calls.  The
** data is sent in the following order:
**	Render or RenderLarge header
**	Pixel 3D header
**	TexSubImage 3D header
** When a glTexSubImage3D call is made, the woffset and size4d fields
** are unexamined by the server and are considered to be padding.
*/
#define __GLX_TEXSUBIMAGE_3D_HDR	\
    CARD32	target B32;	\
    CARD32	level B32;	\
    CARD32	xoffset B32;	\
    CARD32	yoffset B32;	\
    CARD32	zoffset B32;	\
    CARD32	woffset B32;	\
    CARD32	width B32;	\
    CARD32	height B32;	\
    CARD32	depth B32;	\
    CARD32	size4d B32;	\
    CARD32	format B32;	\
    CARD32	type B32;	\
    CARD32	nullImage	\

#define __GLX_TEXSUBIMAGE_3D_HDR_SIZE 52

#define __GLX_TEXSUBIMAGE_3D_CMD_HDR_SIZE \
    (__GLX_RENDER_HDR_SIZE + __GLX_PIXEL_3D_HDR_SIZE + \
		__GLX_TEXSUBIMAGE_3D_HDR_SIZE)

#define __GLX_TEXSUBIMAGE_3D_CMD_DISPATCH_HDR_SIZE \
    (__GLX_PIXEL_3D_HDR_SIZE + __GLX_TEXSUBIMAGE_3D_HDR_SIZE)

typedef struct {
    __GLX_RENDER_HDR;
    __GLX_PIXEL_3D_HDR;
    __GLX_TEXSUBIMAGE_3D_HDR;
} __GLXtexSubImage3DHeader;

typedef struct {
    __GLX_RENDER_LARGE_HDR;
    __GLX_PIXEL_3D_HDR;
    __GLX_TEXSUBIMAGE_3D_HDR;
} __GLXtexSubImage3DLargeHeader;

typedef struct {
    __GLX_PIXEL_3D_HDR;
    __GLX_TEXSUBIMAGE_3D_HDR;
} __GLXdispatchTexSubImage3DHeader;

/*
** Data that is specific to a glDrawPixels call.  The data is sent in the
** following order:
**	Render or RenderLarge header
**	Pixel header
**	DrawPixels header
*/
#define __GLX_DRAWPIXELS_HDR \
    CARD32	width B32;   \
    CARD32	height B32;  \
    CARD32	format B32;  \
    CARD32	type B32

#define __GLX_DRAWPIXELS_HDR_SIZE 16

#define __GLX_DRAWPIXELS_CMD_HDR_SIZE \
    (__GLX_RENDER_HDR_SIZE + __GLX_PIXEL_HDR_SIZE + __GLX_DRAWPIXELS_HDR_SIZE)

#define __GLX_DRAWPIXELS_CMD_DISPATCH_HDR_SIZE \
    (__GLX_PIXEL_HDR_SIZE + __GLX_DRAWPIXELS_HDR_SIZE)

typedef struct {
    __GLX_RENDER_HDR;
    __GLX_PIXEL_HDR;
    __GLX_DRAWPIXELS_HDR;
} __GLXdrawPixelsHeader;

typedef struct {
    __GLX_RENDER_LARGE_HDR;
    __GLX_PIXEL_HDR;
    __GLX_DRAWPIXELS_HDR;
} __GLXdrawPixelsLargeHeader;

typedef struct {
    __GLX_PIXEL_HDR;
    __GLX_DRAWPIXELS_HDR;
} __GLXdispatchDrawPixelsHeader;

/*
** Data that is specific to a glConvolutionFilter1D or glConvolutionFilter2D
** call.  The data is sent in the following order:
**	Render or RenderLarge header
**	Pixel header
**	ConvolutionFilter header
** When a glConvolutionFilter1D call the height field is unexamined by the server.
*/
#define __GLX_CONV_FILT_HDR	\
    CARD32	target B32;	\
    CARD32	internalformat B32;	\
    CARD32	width B32;	\
    CARD32	height B32;	\
    CARD32	format B32;	\
    CARD32	type B32

#define __GLX_CONV_FILT_HDR_SIZE 24

#define __GLX_CONV_FILT_CMD_HDR_SIZE \
    (__GLX_RENDER_HDR_SIZE + __GLX_PIXEL_HDR_SIZE + __GLX_CONV_FILT_HDR_SIZE)

#define __GLX_CONV_FILT_CMD_DISPATCH_HDR_SIZE \
    (__GLX_PIXEL_HDR_SIZE + __GLX_CONV_FILT_HDR_SIZE)
typedef struct {
    __GLX_RENDER_HDR;
    __GLX_PIXEL_HDR;
    __GLX_CONV_FILT_HDR;
} __GLXConvolutionFilterHeader;

typedef struct {
    __GLX_RENDER_LARGE_HDR;
    __GLX_PIXEL_HDR;
    __GLX_CONV_FILT_HDR;
} __GLXConvolutionFilterLargeHeader;

typedef struct {
    __GLX_PIXEL_HDR;
    __GLX_CONV_FILT_HDR;
} __GLXdispatchConvolutionFilterHeader;

/*
** Data that is specific to a glDrawArraysEXT call.  The data is sent in the
** following order:
**	Render or RenderLarge header
**	Draw Arrays header
**	a variable number of Component headers
**	vertex data for each component type
*/

#define __GLX_DRAWARRAYS_HDR \
    CARD32	numVertexes B32; \
    CARD32	numComponents B32; \
    CARD32	primType B32

#define __GLX_DRAWARRAYS_HDR_SIZE 12

#define __GLX_DRAWARRAYS_CMD_HDR_SIZE \
    (__GLX_RENDER_HDR_SIZE + __GLX_DRAWARRAYS_HDR_SIZE)

typedef struct {
    __GLX_RENDER_HDR;
    __GLX_DRAWARRAYS_HDR;
} __GLXdrawArraysHeader;

typedef struct {
    __GLX_RENDER_LARGE_HDR;
    __GLX_DRAWARRAYS_HDR;
} __GLXdrawArraysLargeHeader;

typedef struct {
    __GLX_DRAWARRAYS_HDR;
} __GLXdispatchDrawArraysHeader;

#define __GLX_COMPONENT_HDR \
    CARD32	datatype B32; \
    INT32	numVals B32; \
    CARD32	component B32

typedef struct {
    __GLX_COMPONENT_HDR;
} __GLXdispatchDrawArraysComponentHeader;

#define __GLX_COMPONENT_HDR_SIZE 12

/*
** Data that is specific to a glColorTable call
**	The data is sent in the following order:
**	Render or RenderLarge header
**	Pixel header
**	ColorTable header
*/

#define __GLX_COLOR_TABLE_HDR	     \
    CARD32	target B32;	    \
    CARD32	internalformat B32; \
    CARD32	width B32;	    \
    CARD32	format B32;	    \
    CARD32	type   B32

#define __GLX_COLOR_TABLE_HDR_SIZE 20

#define __GLX_COLOR_TABLE_CMD_HDR_SIZE \
    (__GLX_RENDER_HDR_SIZE + __GLX_PIXEL_HDR_SIZE + __GLX_COLOR_TABLE_HDR_SIZE)

typedef struct {
    __GLX_RENDER_HDR;
    __GLX_PIXEL_HDR;
    __GLX_COLOR_TABLE_HDR;
} __GLXColorTableHeader;

typedef struct {
    __GLX_RENDER_LARGE_HDR;
    __GLX_PIXEL_HDR;
    __GLX_COLOR_TABLE_HDR;
} __GLXColorTableLargeHeader;

typedef struct {
    __GLX_PIXEL_HDR;
    __GLX_COLOR_TABLE_HDR;
} __GLXdispatchColorTableHeader;

/*
** Data that is specific to a glColorSubTable call
**	The data is sent in the following order:
**	Render or RenderLarge header
**	Pixel header
**	ColorTable header
*/

#define __GLX_COLOR_SUBTABLE_HDR    \
    CARD32	target B32;	    \
    CARD32	start B32; 	    \
    CARD32	count B32;	    \
    CARD32	format B32;	    \
    CARD32	type   B32

#define __GLX_COLOR_SUBTABLE_HDR_SIZE 20

#define __GLX_COLOR_SUBTABLE_CMD_HDR_SIZE \
    (__GLX_RENDER_HDR_SIZE + __GLX_PIXEL_HDR_SIZE + \
     __GLX_COLOR_SUBTABLE_HDR_SIZE)

typedef struct {
    __GLX_RENDER_HDR;
    __GLX_PIXEL_HDR;
    __GLX_COLOR_SUBTABLE_HDR;
} __GLXColorSubTableHeader;

typedef struct {
    __GLX_RENDER_LARGE_HDR;
    __GLX_PIXEL_HDR;
    __GLX_COLOR_SUBTABLE_HDR;
} __GLXColorSubTableLargeHeader;

typedef struct {
    __GLX_PIXEL_HDR;
    __GLX_COLOR_SUBTABLE_HDR;
} __GLXdispatchColorSubTableHeader;


/*****************************************************************************/

/*
** Restore these definitions back to the typedefs in glx.h
*/
#undef GLXContextID
#undef GLXPixmap
#undef GLXDrawable
#undef GLXPbuffer
#undef GLXWindow
#undef GLXFBConfigID
#undef GLXFBConfigIDSGIX
#undef GLXPbufferSGIX


/* Opcodes for GLX commands */

#define X_GLXRender                       1
#define X_GLXRenderLarge                  2
#define X_GLXCreateContext                3
#define X_GLXDestroyContext               4
#define X_GLXMakeCurrent                  5
#define X_GLXIsDirect                     6
#define X_GLXQueryVersion                 7
#define X_GLXWaitGL                       8
#define X_GLXWaitX                        9
#define X_GLXCopyContext                 10
#define X_GLXSwapBuffers                 11
#define X_GLXUseXFont                    12
#define X_GLXCreateGLXPixmap             13
#define X_GLXGetVisualConfigs            14
#define X_GLXDestroyGLXPixmap            15
#define X_GLXVendorPrivate               16
#define X_GLXVendorPrivateWithReply      17
#define X_GLXQueryExtensionsString       18
#define X_GLXQueryServerString           19
#define X_GLXClientInfo                  20


/* Opcodes for single commands (part of GLX command space) */

#define X_GLsop_NewList                    101
#define X_GLsop_EndList                    102
#define X_GLsop_DeleteLists                103
#define X_GLsop_GenLists                   104
#define X_GLsop_FeedbackBuffer             105
#define X_GLsop_SelectBuffer               106
#define X_GLsop_RenderMode                 107
#define X_GLsop_Finish                     108
#define X_GLsop_Flush                      142
#define X_GLsop_PixelStoref                109
#define X_GLsop_PixelStorei                110
#define X_GLsop_ReadPixels                 111
#define X_GLsop_GetBooleanv                112
#define X_GLsop_GetClipPlane               113
#define X_GLsop_GetDoublev                 114
#define X_GLsop_GetError                   115
#define X_GLsop_GetFloatv                  116
#define X_GLsop_GetIntegerv                117
#define X_GLsop_GetLightfv                 118
#define X_GLsop_GetLightiv                 119
#define X_GLsop_GetMapdv                   120
#define X_GLsop_GetMapfv                   121
#define X_GLsop_GetMapiv                   122
#define X_GLsop_GetMaterialfv              123
#define X_GLsop_GetMaterialiv              124
#define X_GLsop_GetPixelMapfv              125
#define X_GLsop_GetPixelMapuiv             126
#define X_GLsop_GetPixelMapusv             127
#define X_GLsop_GetPolygonStipple          128
#define X_GLsop_GetString                  129
#define X_GLsop_GetTexEnvfv                130
#define X_GLsop_GetTexEnviv                131
#define X_GLsop_GetTexGendv                132
#define X_GLsop_GetTexGenfv                133
#define X_GLsop_GetTexGeniv                134
#define X_GLsop_GetTexImage                135
#define X_GLsop_GetTexParameterfv          136
#define X_GLsop_GetTexParameteriv          137
#define X_GLsop_GetTexLevelParameterfv     138
#define X_GLsop_GetTexLevelParameteriv     139
#define X_GLsop_IsEnabled                  140
#define X_GLsop_IsList                     141
#define X_GLsop_AreTexturesResident        143
#define X_GLsop_DeleteTextures             144
#define X_GLsop_GenTextures                145
#define X_GLsop_IsTexture                  146
#define X_GLsop_GetColorTable              147
#define X_GLsop_GetColorTableParameterfv   148
#define X_GLsop_GetColorTableParameteriv   149
#define X_GLsop_GetConvolutionFilter       150
#define X_GLsop_GetConvolutionParameterfv  151
#define X_GLsop_GetConvolutionParameteriv  152
#define X_GLsop_GetSeparableFilter         153
#define X_GLsop_GetHistogram               154
#define X_GLsop_GetHistogramParameterfv    155
#define X_GLsop_GetHistogramParameteriv    156
#define X_GLsop_GetMinmax                  157
#define X_GLsop_GetMinmaxParameterfv       158
#define X_GLsop_GetMinmaxParameteriv       159


/* Opcodes for rendering commands */

#define X_GLrop_CallList                     1
#define X_GLrop_CallLists                    2
#define X_GLrop_ListBase                     3
#define X_GLrop_Begin                        4
#define X_GLrop_Bitmap                       5
#define X_GLrop_Color3bv                     6
#define X_GLrop_Color3dv                     7
#define X_GLrop_Color3fv                     8
#define X_GLrop_Color3iv                     9
#define X_GLrop_Color3sv                    10
#define X_GLrop_Color3ubv                   11
#define X_GLrop_Color3uiv                   12
#define X_GLrop_Color3usv                   13
#define X_GLrop_Color4bv                    14
#define X_GLrop_Color4dv                    15
#define X_GLrop_Color4fv                    16
#define X_GLrop_Color4iv                    17
#define X_GLrop_Color4sv                    18
#define X_GLrop_Color4ubv                   19
#define X_GLrop_Color4uiv                   20
#define X_GLrop_Color4usv                   21
#define X_GLrop_EdgeFlagv                   22
#define X_GLrop_End                         23
#define X_GLrop_Indexdv                     24
#define X_GLrop_Indexfv                     25
#define X_GLrop_Indexiv                     26
#define X_GLrop_Indexsv                     27
#define X_GLrop_Normal3bv                   28
#define X_GLrop_Normal3dv                   29
#define X_GLrop_Normal3fv                   30
#define X_GLrop_Normal3iv                   31
#define X_GLrop_Normal3sv                   32
#define X_GLrop_RasterPos2dv                33
#define X_GLrop_RasterPos2fv                34
#define X_GLrop_RasterPos2iv                35
#define X_GLrop_RasterPos2sv                36
#define X_GLrop_RasterPos3dv                37
#define X_GLrop_RasterPos3fv                38
#define X_GLrop_RasterPos3iv                39
#define X_GLrop_RasterPos3sv                40
#define X_GLrop_RasterPos4dv                41
#define X_GLrop_RasterPos4fv                42
#define X_GLrop_RasterPos4iv                43
#define X_GLrop_RasterPos4sv                44
#define X_GLrop_Rectdv                      45
#define X_GLrop_Rectfv                      46
#define X_GLrop_Rectiv                      47
#define X_GLrop_Rectsv                      48
#define X_GLrop_TexCoord1dv                 49
#define X_GLrop_TexCoord1fv                 50
#define X_GLrop_TexCoord1iv                 51
#define X_GLrop_TexCoord1sv                 52
#define X_GLrop_TexCoord2dv                 53
#define X_GLrop_TexCoord2fv                 54
#define X_GLrop_TexCoord2iv                 55
#define X_GLrop_TexCoord2sv                 56
#define X_GLrop_TexCoord3dv                 57
#define X_GLrop_TexCoord3fv                 58
#define X_GLrop_TexCoord3iv                 59
#define X_GLrop_TexCoord3sv                 60
#define X_GLrop_TexCoord4dv                 61
#define X_GLrop_TexCoord4fv                 62
#define X_GLrop_TexCoord4iv                 63
#define X_GLrop_TexCoord4sv                 64
#define X_GLrop_Vertex2dv                   65
#define X_GLrop_Vertex2fv                   66
#define X_GLrop_Vertex2iv                   67
#define X_GLrop_Vertex2sv                   68
#define X_GLrop_Vertex3dv                   69
#define X_GLrop_Vertex3fv                   70
#define X_GLrop_Vertex3iv                   71
#define X_GLrop_Vertex3sv                   72
#define X_GLrop_Vertex4dv                   73
#define X_GLrop_Vertex4fv                   74
#define X_GLrop_Vertex4iv                   75
#define X_GLrop_Vertex4sv                   76
#define X_GLrop_ClipPlane                   77
#define X_GLrop_ColorMaterial               78
#define X_GLrop_CullFace                    79
#define X_GLrop_Fogf                        80
#define X_GLrop_Fogfv                       81
#define X_GLrop_Fogi                        82
#define X_GLrop_Fogiv                       83
#define X_GLrop_FrontFace                   84
#define X_GLrop_Hint                        85
#define X_GLrop_Lightf                      86
#define X_GLrop_Lightfv                     87
#define X_GLrop_Lighti                      88
#define X_GLrop_Lightiv                     89
#define X_GLrop_LightModelf                 90
#define X_GLrop_LightModelfv                91
#define X_GLrop_LightModeli                 92
#define X_GLrop_LightModeliv                93
#define X_GLrop_LineStipple                 94
#define X_GLrop_LineWidth                   95
#define X_GLrop_Materialf                   96
#define X_GLrop_Materialfv                  97
#define X_GLrop_Materiali                   98
#define X_GLrop_Materialiv                  99
#define X_GLrop_PointSize                  100
#define X_GLrop_PolygonMode                101
#define X_GLrop_PolygonStipple             102
#define X_GLrop_Scissor                    103
#define X_GLrop_ShadeModel                 104
#define X_GLrop_TexParameterf              105
#define X_GLrop_TexParameterfv             106
#define X_GLrop_TexParameteri              107
#define X_GLrop_TexParameteriv             108
#define X_GLrop_TexImage1D                 109
#define X_GLrop_TexImage2D                 110
#define X_GLrop_TexEnvf                    111
#define X_GLrop_TexEnvfv                   112
#define X_GLrop_TexEnvi                    113
#define X_GLrop_TexEnviv                   114
#define X_GLrop_TexGend                    115
#define X_GLrop_TexGendv                   116
#define X_GLrop_TexGenf                    117
#define X_GLrop_TexGenfv                   118
#define X_GLrop_TexGeni                    119
#define X_GLrop_TexGeniv                   120
#define X_GLrop_InitNames                  121
#define X_GLrop_LoadName                   122
#define X_GLrop_PassThrough                123
#define X_GLrop_PopName                    124
#define X_GLrop_PushName                   125
#define X_GLrop_DrawBuffer                 126
#define X_GLrop_Clear                      127
#define X_GLrop_ClearAccum                 128
#define X_GLrop_ClearIndex                 129
#define X_GLrop_ClearColor                 130
#define X_GLrop_ClearStencil               131
#define X_GLrop_ClearDepth                 132
#define X_GLrop_StencilMask                133
#define X_GLrop_ColorMask                  134
#define X_GLrop_DepthMask                  135
#define X_GLrop_IndexMask                  136
#define X_GLrop_Accum                      137
#define X_GLrop_Disable                    138
#define X_GLrop_Enable                     139
#define X_GLrop_PopAttrib                  141
#define X_GLrop_PushAttrib                 142
#define X_GLrop_Map1d                      143
#define X_GLrop_Map1f                      144
#define X_GLrop_Map2d                      145
#define X_GLrop_Map2f                      146
#define X_GLrop_MapGrid1d                  147
#define X_GLrop_MapGrid1f                  148
#define X_GLrop_MapGrid2d                  149
#define X_GLrop_MapGrid2f                  150
#define X_GLrop_EvalCoord1dv               151
#define X_GLrop_EvalCoord1fv               152
#define X_GLrop_EvalCoord2dv               153
#define X_GLrop_EvalCoord2fv               154
#define X_GLrop_EvalMesh1                  155
#define X_GLrop_EvalPoint1                 156
#define X_GLrop_EvalMesh2                  157
#define X_GLrop_EvalPoint2                 158
#define X_GLrop_AlphaFunc                  159
#define X_GLrop_BlendFunc                  160
#define X_GLrop_LogicOp                    161
#define X_GLrop_StencilFunc                162
#define X_GLrop_StencilOp                  163
#define X_GLrop_DepthFunc                  164
#define X_GLrop_PixelZoom                  165
#define X_GLrop_PixelTransferf             166
#define X_GLrop_PixelTransferi             167
#define X_GLrop_PixelMapfv                 168
#define X_GLrop_PixelMapuiv                169
#define X_GLrop_PixelMapusv                170
#define X_GLrop_ReadBuffer                 171
#define X_GLrop_CopyPixels                 172
#define X_GLrop_DrawPixels                 173
#define X_GLrop_DepthRange                 174
#define X_GLrop_Frustum                    175
#define X_GLrop_LoadIdentity               176
#define X_GLrop_LoadMatrixf                177
#define X_GLrop_LoadMatrixd                178
#define X_GLrop_MatrixMode                 179
#define X_GLrop_MultMatrixf                180
#define X_GLrop_MultMatrixd                181
#define X_GLrop_Ortho                      182
#define X_GLrop_PopMatrix                  183
#define X_GLrop_PushMatrix                 184
#define X_GLrop_Rotated                    185
#define X_GLrop_Rotatef                    186
#define X_GLrop_Scaled                     187
#define X_GLrop_Scalef                     188
#define X_GLrop_Translated                 189
#define X_GLrop_Translatef                 190
#define X_GLrop_Viewport                   191
#define X_GLrop_DrawArrays                 193
#define X_GLrop_PolygonOffset              192
#define X_GLrop_CopyTexImage1D             4119
#define X_GLrop_CopyTexImage2D             4120
#define X_GLrop_CopyTexSubImage1D          4121
#define X_GLrop_CopyTexSubImage2D          4122
#define X_GLrop_TexSubImage1D              4099
#define X_GLrop_TexSubImage2D              4100
#define X_GLrop_BindTexture                4117
#define X_GLrop_PrioritizeTextures         4118
#define X_GLrop_Indexubv                   194
#define X_GLrop_BlendColor                 4096
#define X_GLrop_BlendEquation              4097
#define X_GLrop_ColorTable                 2053
#define X_GLrop_ColorTableParameterfv      2054
#define X_GLrop_ColorTableParameteriv      2055
#define X_GLrop_CopyColorTable             2056
#define X_GLrop_ColorSubTable              195
#define X_GLrop_CopyColorSubTable          196
#define X_GLrop_ConvolutionFilter1D        4101
#define X_GLrop_ConvolutionFilter2D        4102
#define X_GLrop_ConvolutionParameterf      4103
#define X_GLrop_ConvolutionParameterfv     4104
#define X_GLrop_ConvolutionParameteri      4105
#define X_GLrop_ConvolutionParameteriv     4106
#define X_GLrop_CopyConvolutionFilter1D    4107
#define X_GLrop_CopyConvolutionFilter2D    4108
#define X_GLrop_SeparableFilter2D          4109
#define X_GLrop_Histogram                  4110
#define X_GLrop_Minmax                     4111
#define X_GLrop_ResetHistogram             4112
#define X_GLrop_ResetMinmax                4113
#define X_GLrop_TexImage3D                 4114
#define X_GLrop_TexSubImage3D              4115
#define X_GLrop_CopyTexSubImage3D          4123
#define X_GLrop_ActiveTextureARB           197
#define X_GLrop_MultiTexCoord1dvARB        198
#define X_GLrop_MultiTexCoord1fvARB        199
#define X_GLrop_MultiTexCoord1ivARB        200
#define X_GLrop_MultiTexCoord1svARB        201
#define X_GLrop_MultiTexCoord2dvARB        202
#define X_GLrop_MultiTexCoord2fvARB        203
#define X_GLrop_MultiTexCoord2ivARB        204
#define X_GLrop_MultiTexCoord2svARB        205
#define X_GLrop_MultiTexCoord3dvARB        206
#define X_GLrop_MultiTexCoord3fvARB        207
#define X_GLrop_MultiTexCoord3ivARB        208
#define X_GLrop_MultiTexCoord3svARB        209
#define X_GLrop_MultiTexCoord4dvARB        210
#define X_GLrop_MultiTexCoord4fvARB        211
#define X_GLrop_MultiTexCoord4ivARB        212
#define X_GLrop_MultiTexCoord4svARB        213
#define X_GLrop_DrawArraysEXT              4116


/* Opcodes for Vendor Private commands */

#define X_GLvop_AreTexturesResidentEXT      11
#define X_GLvop_DeleteTexturesEXT           12
#define X_GLvop_GenTexturesEXT              13
#define X_GLvop_IsTextureEXT                14


/* Opcodes for GLX vendor private commands */

#define X_GLXvop_QueryContextInfoEXT        1024


#endif /* _GLX_glxproto_h_ */
