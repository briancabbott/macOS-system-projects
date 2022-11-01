/*
 * Copyright � 2001 Keith Packard
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of Keith Packard not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  Keith Packard makes no
 * representations about the suitability of this software for any purpose.  It
 * is provided "as is" without express or implied warranty.
 *
 * KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/* $XFree86: xc/programs/Xserver/hw/kdrive/chips/chips.c,v 1.1 2001/09/05 07:12:41 keithp Exp $ */

#include "chips.h"
#include <sys/io.h>

#undef CHIPS_DEBUG

Bool
chipsCardInit (KdCardInfo *card)
{
    int			k;
    char		*pixels;
    ChipsCardInfo	*chipsc;
    CARD8		r00, r01, r02;
    CARD8		r39;

    chipsc = (ChipsCardInfo *) xalloc (sizeof (ChipsCardInfo));
    if (!chipsc)
	return FALSE;
    
    iopl (3);
    
    if (!vesaInitialize (card, &chipsc->vesa))
    {
	xfree (chipsc);
	return FALSE;
    }
    
#ifdef USE_PCI
    chipsc->window = (CARD32 *) (chipsc->cop_base + 0x10000);
#else
    chipsc->window = 0;
#endif
    card->driver = chipsc;
    
    return TRUE;
}

Bool
chipsScreenInit (KdScreenInfo *screen)
{
    ChipsCardInfo	*chipsc = screen->card->driver;
    ChipsScreenInfo	*chipss;
    int			screen_size, memory;
    CARD32		mmio_base;
    CARD32		mmio_size;

    chipss = (ChipsScreenInfo *) xalloc (sizeof (ChipsScreenInfo));
    if (!chipss)
	return FALSE;
    memset (chipss, '\0', sizeof (ChipsScreenInfo));
    if (!vesaScreenInitialize (screen, &chipss->vesa))
    {
	xfree (chipss);
	return FALSE;
    }

    if (chipss->vesa.mapping != VESA_LINEAR)
	screen->dumb = TRUE;
    if (!screen->dumb)
    {
	chipss->mmio_base = (CARD8 *) KdMapDevice (CHIPS_MMIO_BASE(chipss),
						   CHIPS_MMIO_SIZE(chipss));
	
	if (chipss->mmio_base)
	{
	    KdSetMappedMode (CHIPS_MMIO_BASE(chipss),
			     CHIPS_MMIO_SIZE(chipss),
			     KD_MAPPED_MODE_REGISTERS);
	}
	else
	    screen->dumb = TRUE;
    }
    else
	chipss->mmio_base = 0;

    chipss->screen = chipss->vesa.fb;
    memory = chipss->vesa.fb_size;
    
    screen_size = screen->fb[0].byteStride * screen->height;
    
    if (chipss->screen && memory >= screen_size + 2048)
    {
	memory -= 2048;
	chipss->cursor_base = chipss->screen + memory - 2048;
    }
    else
	chipss->cursor_base = 0;
    memory -= screen_size;
    if (memory > screen->fb[0].byteStride)
    {
	chipss->off_screen = chipss->screen + screen_size;
	chipss->off_screen_size = memory;
    }
    else
    {
	chipss->off_screen = 0;
	chipss->off_screen_size = 0;
    }
    screen->driver = chipss;
    return TRUE;
}

Bool
chipsInitScreen (ScreenPtr pScreen)
{
    return vesaInitScreen (pScreen);
}

Bool
chipsFinishInitScreen (ScreenPtr pScreen)
{
    return vesaFinishInitScreen (pScreen);
}

CARD8
chipsReadXR (ChipsScreenInfo *chipss, CARD8 index)
{
    CARD8 value;
    outb (index, 0x3d6);
    value = inb (0x3d7);
    return value;
}

void
chipsWriteXR (ChipsScreenInfo *chipss, CARD8 index, CARD8 value)
{
    outb (index, 0x3d6);
    outb (value, 0x3d7);
}

CARD8
chipsReadFR (ChipsScreenInfo *chipss, CARD8 index)
{
    CARD8 value;
    outb (index, 0x3d0);
    value = inb (0x3d1);
    return value;
}

void
chipsWriteFR (ChipsScreenInfo *chipss, CARD8 index, CARD8 value)
{
    outb (index, 0x3d0);
    outb (value, 0x3d1);
}

CARD8
chipsReadSeq (ChipsScreenInfo *chipss, CARD8 index)
{
    CARD8   value;
    outb (index, 0x3c4);
    value = inb (0x3c5);
    return value;
}

void
chipsWriteSeq (ChipsScreenInfo *chipss, CARD8 index, CARD8 value)
{
    outb (index, 0x3c4);
    outb (value, 0x3c5);
}

void
chipsPreserve (KdCardInfo *card)
{
    ChipsCardInfo	*chipss = card->driver;
    vesaPreserve(card);
}

void
chipsSetMMIO (ChipsCardInfo *chipsc)
{
}

void
chipsResetMMIO (ChipsCardInfo *chipsc)
{
}

Bool
chipsEnable (ScreenPtr pScreen)
{
    KdScreenPriv(pScreen);
    ChipsCardInfo	*chipsc = pScreenPriv->card->driver;

    if (!vesaEnable (pScreen))
	return FALSE;
    chipsSetMMIO (chipsc);
    return TRUE;
}

Bool
chipsDPMS (ScreenPtr pScreen, int mode)
{
    KdScreenPriv(pScreen);
    chipsScreenInfo(pScreenPriv);
    
    ErrorF ("seqreg 0x01 0x%x\n", chipsReadSeq (chipss, 0x1));
    ErrorF ("dpmsreg XR61 0x%x\n", chipsReadXR (chipss, 0x61));
    ErrorF ("dpmsreg XR73 0x%x\n", chipsReadXR (chipss, 0x73));
    
    ErrorF ("flat panel FR05 0x%x\n", chipsReadFR (chipss, 0x5));
    ErrorF ("flat panel XR52 0x%x\n", chipsReadXR (chipss, 0x52));
    return TRUE;
}

void
chipsDisable (ScreenPtr pScreen)
{
    vesaDisable (pScreen);
}

void
chipsRestore (KdCardInfo *card)
{
    ChipsCardInfo	*chipsc = card->driver;

    chipsResetMMIO (chipsc);
    vesaRestore (card);
}

void
chipsScreenFini (KdScreenInfo *screen)
{
    ChipsScreenInfo	*chipss = (ChipsScreenInfo *) screen->driver;

    if (chipss->mmio_base)
    {
	KdUnmapDevice ((void *) chipss->mmio_base, CHIPS_MMIO_SIZE(chipss));
	KdResetMappedMode (CHIPS_MMIO_BASE(chipss),
			   CHIPS_MMIO_SIZE(chipss),
			   KD_MAPPED_MODE_REGISTERS);
    }
    vesaScreenFini (screen);
    xfree (chipss);
    screen->driver = 0;
}

void
chipsCardFini (KdCardInfo *card)
{
    ChipsCardInfo	*chipsc = card->driver;

    vesaCardFini (card);
}

#define chipsCursorInit	(void *) 0
#define chipsCursorEnable	(void *) 0
#define chipsCursorDisable	(void *) 0
#define chipsCursorFini	(void *) 0
#define chipsRecolorCursor	(void *) 0

KdCardFuncs	chipsFuncs = {
    chipsCardInit,	    /* cardinit */
    chipsScreenInit,	    /* scrinit */
    chipsInitScreen,	    /* initScreen */
    chipsPreserve,	    /* preserve */
    chipsEnable,	    /* enable */
    vesaDPMS,		    /* dpms */
    chipsDisable,	    /* disable */
    chipsRestore,	    /* restore */
    chipsScreenFini,	    /* scrfini */
    chipsCardFini,	    /* cardfini */
    
    chipsCursorInit,	    /* initCursor */
    chipsCursorEnable,    /* enableCursor */
    chipsCursorDisable,   /* disableCursor */
    chipsCursorFini,	    /* finiCursor */
    chipsRecolorCursor,   /* recolorCursor */
    
    chipsDrawInit,        /* initAccel */
    chipsDrawEnable,      /* enableAccel */
    chipsDrawSync,	    /* syncAccel */
    chipsDrawDisable,     /* disableAccel */
    chipsDrawFini,        /* finiAccel */
    
    vesaGetColors,    	    /* getColors */
    vesaPutColors,	    /* putColors */
    chipsFinishInitScreen /* finishInitScreen */
};
