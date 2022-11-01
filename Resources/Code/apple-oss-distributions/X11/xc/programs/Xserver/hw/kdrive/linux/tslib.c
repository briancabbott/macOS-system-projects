/*
 * $XFree86: xc/programs/Xserver/hw/kdrive/linux/tslib.c,v 1.2 2002/11/05 05:28:06 keithp Exp $
 * TSLIB based touchscreen driver for TinyX
 * Derived from ts.c by Keith Packard
 * Derived from ps2.c by Jim Gettys
 *
 * Copyright � 1999 Keith Packard
 * Copyright � 2000 Compaq Computer Corporation
 * Copyright � 2002 MontaVista Software Inc.
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard or Compaq not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard and Compaq makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD AND COMPAQ DISCLAIM ALL WARRANTIES WITH REGARD TO THIS 
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, 
 * IN NO EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * 
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Michael Taht or MontaVista not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Michael Taht and Montavista make no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * MICHAEL TAHT AND MONTAVISTA DISCLAIM ALL WARRANTIES WITH REGARD TO THIS 
 * SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, 
 * IN NO EVENT SHALL EITHER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */


#define NEED_EVENTS
#include "X.h"
#include "Xproto.h"
#include "inputstr.h"
#include "scrnintstr.h"
#include "kdrive.h"
#include "Xpoll.h"
#include <sys/ioctl.h>
#include <tslib.h>

static long lastx = 0, lasty = 0;
static struct tsdev *tsDev = NULL;

void
TsRead (int tsPort, void *closure)
{
    KdMouseInfo	    *mi = closure;
    int		    fd = (int) mi->driver;
    struct ts_sample event;
    int		    n;
    long	    pressure;
    long	    x, y;
    unsigned long   flags;
    unsigned long   buttons;

    n = ts_read(tsDev, &event, 1);
    if (n == 1)  
    {
	if (event.pressure) 
	{
	    /* 
	     * HACK ATTACK.  (static global variables used !)
	     * Here we test for the touch screen driver actually being on the
	     * touch screen, if it is we send absolute coordinates. If not,
	     * then we send delta's so that we can track the entire vga screen.
	     */
	    if (KdTsCurScreen == KdTsPhyScreen) {
	    	flags = KD_BUTTON_1;
	    	x = event.x;
	    	y = event.y;
	    } else {
	    	flags = /* KD_BUTTON_1 |*/ KD_MOUSE_DELTA;
	    	if ((lastx == 0) || (lasty == 0)) {
	    	    x = 0;
	    	    y = 0;
	    	} else {
	    	    x = event.x - lastx;
	    	    y = event.y - lasty;
	    	}
	    	lastx = event.x;
	    	lasty = event.y;
	    }
	} else {
	    flags = KD_MOUSE_DELTA;
	    x = 0;
	    y = 0;
	    lastx = 0;
	    lasty = 0;
	}
	KdEnqueueMouseEvent (mi, flags, x, y);
    }
}

static char *TsNames[] = {
  "/dev/ts",	
  "/dev/touchscreen/0",
};

#define NUM_TS_NAMES	(sizeof (TsNames) / sizeof (TsNames[0]))

int TsInputType;

int
TslibInit (void)
{
    int		i;
    KdMouseInfo	*mi, *next;
    int		fd= 0;
    int		n = 0;

    if (!TsInputType)
	TsInputType = KdAllocInputType ();
    
    for (mi = kdMouseInfo; mi; mi = next)
    {
	next = mi->next;
	if (mi->inputType)
	    continue;

	if (!mi->name)
	{
	    for (i = 0; i < NUM_TS_NAMES; i++)    
	    {
		if(!(tsDev = ts_open(TsNames[i],0))) continue;
	        ts_config(tsDev); 
	        fd=ts_fd(tsDev);
		if (fd >= 0) 
		{
		    mi->name = KdSaveString (TsNames[i]);
		    break;
		}
	    }
	}

	if (fd > 0 && tsDev != 0) 
	  {
	    mi->driver = (void *) fd;
	    mi->inputType = TsInputType;
	    	if (KdRegisterFd (TsInputType, fd, TsRead, (void *) mi))
		    n++;
	  } 
	else 
	  if (fd > 0) close(fd);
	}
}

void
TslibFini (void)
{
    KdMouseInfo	*mi;

    KdUnregisterFds (TsInputType, TRUE);
    for (mi = kdMouseInfo; mi; mi = mi->next)
    {
	if (mi->inputType == TsInputType)
	{
	    if(mi->driver) ts_close(tsDev);
	    mi->driver = 0;
	    mi->inputType = 0;
	}
    }
}

KdMouseFuncs TsFuncs = {
    TslibInit,
    TslibFini
};
