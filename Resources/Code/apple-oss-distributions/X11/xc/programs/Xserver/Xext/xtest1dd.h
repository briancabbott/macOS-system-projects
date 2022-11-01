/* $XFree86: xc/programs/Xserver/Xext/xtest1dd.h,v 3.2 2001/08/01 00:44:44 tsi Exp $ */
/************************************************************

Copyright 1996 by Thomas E. Dickey <dickey@clark.net>

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of the above listed
copyright holder(s) not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.

THE ABOVE LISTED COPYRIGHT HOLDER(S) DISCLAIM ALL WARRANTIES WITH REGARD
TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS, IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE
LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

********************************************************/

#ifndef XTEST1DD_H
#define XTEST1DD_H 1

extern	short		xtest_mousex;
extern	short		xtest_mousey;
extern	int		playback_on;
extern	ClientPtr	current_xtest_client;
extern	ClientPtr	playback_client;
extern	KeyCode		xtest_command_key;

extern void stop_stealing_input(
#if NeedFunctionPrototypes
	void
#endif
);

extern void
steal_input(
#if NeedFunctionPrototypes
	ClientPtr              /* client */,
	CARD32                 /* mode */
#endif
);

extern void
flush_input_actions(
#if NeedFunctionPrototypes
	void
#endif
);

extern void
XTestStealJumpData(
#if NeedFunctionPrototypes
	int                    /* jx */,
	int                    /* jy */,
	int                    /* dev_type */
#endif
);

extern void
XTestStealMotionData(
#if NeedFunctionPrototypes
	int                    /* dx */,
	int                    /* dy */,
	int                    /* dev_type */,
	int                    /* mx */,
	int                    /* my */
#endif
);

extern Bool
XTestStealKeyData(
#if NeedFunctionPrototypes
	unsigned               /* keycode */,
	int                    /* keystate */,
	int                    /* dev_type */,
	int                    /* locx */,
	int                    /* locy */
#endif
);

extern void
parse_fake_input(
#if NeedFunctionPrototypes
	ClientPtr              /* client */,
	char *                 /* req */
#endif
);

extern void
XTestComputeWaitTime(
#if NeedFunctionPrototypes
	struct timeval *       /* waittime */
#endif
);

extern int
XTestProcessInputAction(
#if NeedFunctionPrototypes
	int                    /* readable */,
	struct timeval *       /* waittime */
#endif
);

extern void
abort_play_back(
#if NeedFunctionPrototypes
	void
#endif
);

extern void
return_input_array_size(
#if NeedFunctionPrototypes
	ClientPtr              /* client */
#endif
);

extern void XTestGenerateEvent(
#if NeedFunctionPrototypes
	int                    /* dev_type */,
	int                    /* keycode */,
	int                    /* keystate */,
	int                    /* mousex */,
	int                    /* mousey */
#endif
);

extern void XTestGetPointerPos(
#if NeedFunctionPrototypes
	short *                /* fmousex */,
	short *                /* fmousey */
#endif
);

extern void XTestJumpPointer(
#if NeedFunctionPrototypes
	int                    /* jx */,
	int                    /* jy */,
	int                    /* dev_type */
#endif
);

#endif /* XTEST1DD_H */
