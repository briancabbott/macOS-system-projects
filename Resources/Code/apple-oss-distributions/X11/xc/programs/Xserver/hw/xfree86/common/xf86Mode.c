/* $XFree86: xc/programs/Xserver/hw/xfree86/common/xf86Mode.c,v 1.63 2003/01/22 21:44:09 tsi Exp $ */

/*
 * Copyright (c) 1997,1998 by The XFree86 Project, Inc.
 *
 * Authors: Dirk Hohndel <hohndel@XFree86.Org>
 *          David Dawes <dawes@XFree86.Org>
 *
 * This file includes helper functions for mode related things.
 */

#include "X.h"
#include "os.h"
#include "servermd.h"
#include "mibank.h"
#include "xf86.h"
#include "xf86Priv.h"
#include "xf86DDC.h"

/*
 * xf86GetNearestClock --
 *	Find closest clock to given frequency (in kHz).  This assumes the
 *	number of clocks is greater than zero.
 */
int
xf86GetNearestClock(ScrnInfoPtr scrp, int freq, Bool allowDiv2,
    int DivFactor, int MulFactor, int *divider)
{
    int nearestClock = 0, nearestDiv = 1;
    int minimumGap = abs(freq - scrp->clock[0]);
    int i, j, k, gap;

    if (allowDiv2)
	k = 2;
    else
	k = 1;

    /* Must set this here in case the best match is scrp->clock[0] */
    if (divider != NULL)
	*divider = 0;
    
    for (i = 0;  i < scrp->numClocks;  i++) {
	for (j = 1; j <= k; j++) {
	    gap = abs((freq * j) - ((scrp->clock[i] * DivFactor) / MulFactor));
	    if ((gap < minimumGap) ||
		((gap == minimumGap) && (j < nearestDiv))) {
		minimumGap = gap;
		nearestClock = i;
		nearestDiv = j;
		if (divider != NULL)
		    *divider = (j - 1) * V_CLKDIV2;
	    }
	}
    }
    return nearestClock;
}

/*
 * xf86ModeStatusToString
 *
 * Convert a ModeStatus value to a printable message
 */

const char *
xf86ModeStatusToString(ModeStatus status)
{
    switch (status) {
    case MODE_OK:
	return "Mode OK";
    case MODE_HSYNC:
	return "hsync out of range";
    case MODE_VSYNC:
	return "vrefresh out of range";
    case MODE_H_ILLEGAL:
	return "illegal horizontal timings";
    case MODE_V_ILLEGAL:
	return "illegal vertical timings";
    case MODE_BAD_WIDTH:
	return "width requires unsupported line pitch";
    case MODE_NOMODE:
	return "no mode of this name";
    case MODE_NO_INTERLACE:
	return "interlace mode not supported";
    case MODE_NO_DBLESCAN:
	return "doublescan mode not supported";
    case MODE_NO_VSCAN:
	return "multiscan mode not supported";
    case MODE_MEM:
	return "insufficient memory for mode";
    case MODE_VIRTUAL_X:
	return "width too large for virtual size";
    case MODE_VIRTUAL_Y:
	return "height too large for virtual size";
    case MODE_MEM_VIRT:
	return "insufficient memory given virtual size";
    case MODE_NOCLOCK:
	return "no clock available for mode";
    case MODE_CLOCK_HIGH:
	return "mode clock too high";
    case MODE_CLOCK_LOW:
	return "mode clock too low";
    case MODE_CLOCK_RANGE:
	return "bad mode clock/interlace/doublescan";
    case MODE_BAD_HVALUE:
	return "horizontal timing out of range";
    case MODE_BAD_VVALUE:
	return "vertical timing out of range";
    case MODE_BAD_VSCAN:
	return "VScan value out of range";
    case MODE_HSYNC_NARROW:
	return "horizontal sync too narrow";
    case MODE_HSYNC_WIDE:
	return "horizontal sync too wide";
    case MODE_HBLANK_NARROW:
	return "horizontal blanking too narrow";
    case MODE_HBLANK_WIDE:
	return "horizontal blanking too wide";
    case MODE_VSYNC_NARROW:
	return "vertical sync too narrow";
    case MODE_VSYNC_WIDE:
	return "vertical sync too wide";
    case MODE_VBLANK_NARROW:
	return "vertical blanking too narrow";
    case MODE_VBLANK_WIDE:
	return "vertical blanking too wide";
    case MODE_PANEL:
	return "exceeds panel dimensions";
    case MODE_INTERLACE_WIDTH:
	return "width too large for interlaced mode";
    case MODE_ONE_WIDTH:
        return "all modes must have the same width";
    case MODE_ONE_HEIGHT:
        return "all modes must have the same height";
    case MODE_ONE_SIZE:
        return "all modes must have the same resolution";
    case MODE_BAD:
	return "unknown reason";
    case MODE_ERROR:
	return "internal error";
    default:
	return "unknown";
    }
}

/*
 * xf86ShowClockRanges() -- Print the clock ranges allowed
 * and the clock values scaled by ClockMulFactor and ClockDivFactor
 */
void
xf86ShowClockRanges(ScrnInfoPtr scrp, ClockRangePtr clockRanges)
{
    ClockRangePtr cp;
    int MulFactor = 1;
    int DivFactor = 1;
    int i, j;
    int scaledClock;

    for (cp = clockRanges; cp != NULL; cp = cp->next) {
	DivFactor = max(1, cp->ClockDivFactor);
	MulFactor = max(1, cp->ClockMulFactor);
	if (scrp->progClock) {
	    if (cp->minClock) {
		if (cp->maxClock) {
		    xf86DrvMsg(scrp->scrnIndex, X_INFO,
			"Clock range: %6.2f to %6.2f MHz\n",
			(double)cp->minClock / 1000.0,
			(double)cp->maxClock / 1000.0);
		} else {
		    xf86DrvMsg(scrp->scrnIndex, X_INFO,
			"Minimum clock: %6.2f MHz\n",
			(double)cp->minClock / 1000.0);
		}
	    } else {
		if (cp->maxClock) {
		    xf86DrvMsg(scrp->scrnIndex, X_INFO,
			"Maximum clock: %6.2f MHz\n",
			(double)cp->maxClock / 1000.0);
		}
	    }
	} else if (DivFactor > 1 || MulFactor > 1) {
	    j = 0;
	    for (i = 0; i < scrp->numClocks; i++) {
		scaledClock = (scrp->clock[i] * DivFactor) / MulFactor;
		if (scaledClock >= cp->minClock && scaledClock <= cp->maxClock) {
		    if ((j % 8) == 0) {
			if (j > 0)
			    xf86ErrorF("\n");
			xf86DrvMsg(scrp->scrnIndex, X_INFO, "scaled clocks:");
		    }
		    xf86ErrorF(" %6.2f", (double)scaledClock / 1000.0);
		    j++;
		}
	    }
	    xf86ErrorF("\n");
	}
    }
}


/*
 * xf86FindClockRangeForMode()    [... like the name says ...]
 */
static ClockRangePtr
xf86FindClockRangeForMode(ClockRangePtr clockRanges, DisplayModePtr p)
{
    ClockRangePtr cp;

    for (cp = clockRanges; ; cp = cp->next)
	if (!cp ||
	    ((p->Clock >= cp->minClock) &&
	     (p->Clock <= cp->maxClock) &&
	     (cp->interlaceAllowed || !(p->Flags & V_INTERLACE)) &&
	     (cp->doubleScanAllowed ||
	      ((p->VScan <= 1) && !(p->Flags & V_DBLSCAN)))))
	    return cp;
}


/*
 * xf86HandleBuiltinMode() - handles built-in modes
 */
static ModeStatus
xf86HandleBuiltinMode(ScrnInfoPtr scrp,
		      DisplayModePtr p,
		      DisplayModePtr modep,
		      ClockRangePtr clockRanges,
		      Bool allowDiv2)
{
    ClockRangePtr cp;
    int extraFlags = 0;
    int MulFactor = 1;
    int DivFactor = 1;
    int clockIndex;
    
    /* Reject previously rejected modes */
    if (p->status != MODE_OK)
	return p->status;

    /* Reject previously considered modes */
    if (p->prev)
        return MODE_NOMODE;

    if ((p->type & M_T_CLOCK_C) == M_T_CLOCK_C) {
	/* Check clock is in range */
	cp = xf86FindClockRangeForMode(clockRanges, p);
	if (cp == NULL){
	    modep->type = p->type;
	    p->status = MODE_CLOCK_RANGE;
	    return MODE_CLOCK_RANGE;
	}
	DivFactor = cp->ClockDivFactor;
	MulFactor = cp->ClockMulFactor;
	if (!scrp->progClock) {
	    clockIndex = xf86GetNearestClock(scrp, p->Clock, allowDiv2,
					     cp->ClockDivFactor,
					     cp->ClockMulFactor, &extraFlags);
	    modep->Clock = (scrp->clock[clockIndex] * DivFactor)
		/ MulFactor;
	    modep->ClockIndex	= clockIndex;
	    modep->SynthClock	= scrp->clock[clockIndex];
	    if (extraFlags & V_CLKDIV2) {
		modep->Clock /= 2;
		modep->SynthClock /= 2;
	    }
	} else {
	    modep->Clock = p->Clock;
	    modep->ClockIndex = -1;
	    modep->SynthClock = (modep->Clock * MulFactor)
		/ DivFactor;
	}
	modep->PrivFlags = cp->PrivFlags;
    } else {
	if(!scrp->progClock) {
            modep->Clock = p->Clock;
	    modep->ClockIndex = p->ClockIndex;
	    modep->SynthClock = p->SynthClock;
	} else {
	    modep->Clock = p->Clock;
	    modep->ClockIndex = -1;
	    modep->SynthClock = p->SynthClock;
	}
	modep->PrivFlags = p->PrivFlags;
    }
    modep->type            = p->type;
    modep->HDisplay        = p->HDisplay;
    modep->HSyncStart      = p->HSyncStart;
    modep->HSyncEnd        = p->HSyncEnd;
    modep->HTotal          = p->HTotal;
    modep->HSkew           = p->HSkew;
    modep->VDisplay        = p->VDisplay;
    modep->VSyncStart      = p->VSyncStart;
    modep->VSyncEnd        = p->VSyncEnd;
    modep->VTotal          = p->VTotal;
    modep->VScan           = p->VScan;
    modep->Flags           = p->Flags | extraFlags;
    modep->CrtcHDisplay    = p->CrtcHDisplay;
    modep->CrtcHBlankStart = p->CrtcHBlankStart;
    modep->CrtcHSyncStart  = p->CrtcHSyncStart;
    modep->CrtcHSyncEnd    = p->CrtcHSyncEnd;
    modep->CrtcHBlankEnd   = p->CrtcHBlankEnd;
    modep->CrtcHTotal      = p->CrtcHTotal;
    modep->CrtcHSkew       = p->CrtcHSkew;
    modep->CrtcVDisplay    = p->CrtcVDisplay;
    modep->CrtcVBlankStart = p->CrtcVBlankStart;
    modep->CrtcVSyncStart  = p->CrtcVSyncStart;
    modep->CrtcVSyncEnd    = p->CrtcVSyncEnd;
    modep->CrtcVBlankEnd   = p->CrtcVBlankEnd;
    modep->CrtcVTotal      = p->CrtcVTotal;
    modep->CrtcHAdjusted   = p->CrtcHAdjusted;
    modep->CrtcVAdjusted   = p->CrtcVAdjusted;
    modep->HSync           = p->HSync;
    modep->VRefresh        = p->VRefresh;
    modep->Private         = p->Private;
    modep->PrivSize        = p->PrivSize;

    p->prev = modep;
    
    return MODE_OK;
}

/*
 * xf86LookupMode
 *
 * This function returns a mode from the given list which matches the
 * given name.  When multiple modes with the same name are available,
 * the method of picking the matching mode is determined by the
 * strategy selected.
 *
 * This function takes the following parameters:
 *    scrp         ScrnInfoPtr
 *    modep        pointer to the returned mode, which must have the name
 *                 field filled in.
 *    clockRanges  a list of clock ranges.   This is optional when all the
 *                 modes are built-in modes.
 *    strategy     how to decide which mode to use from multiple modes with
 *                 the same name
 *
 * In addition, the following fields from the ScrnInfoRec are used:
 *    modePool     the list of monitor modes compatible with the driver
 *    clocks       a list of discrete clocks
 *    numClocks    number of discrete clocks
 *    progClock    clock is programmable
 *
 * If a mode was found, its values are filled in to the area pointed to
 * by modep,  If a mode was not found the return value indicates the
 * reason.
 */

ModeStatus
xf86LookupMode(ScrnInfoPtr scrp, DisplayModePtr modep,
	       ClockRangePtr clockRanges, LookupModeFlags strategy)
{
    DisplayModePtr p, bestMode = NULL;
    ClockRangePtr cp;
    int i, k, gap, minimumGap = CLOCK_TOLERANCE + 1;
    double refresh, bestRefresh = 0.0;
    Bool found = FALSE;
    int extraFlags = 0;
    int clockIndex = -1;
    int MulFactor = 1;
    int DivFactor = 1;
    int ModePrivFlags = 0;
    ModeStatus status = MODE_NOMODE;
    Bool allowDiv2 = (strategy & LOOKUP_CLKDIV2) != 0;
    Bool haveBuiltin;

    strategy &= ~(LOOKUP_CLKDIV2 | LOOKUP_OPTIONAL_TOLERANCES);

    /* Some sanity checking */
    if (scrp == NULL || scrp->modePool == NULL ||
	(!scrp->progClock && scrp->numClocks == 0)) {
	ErrorF("xf86LookupMode: called with invalid scrnInfoRec\n");
	return MODE_ERROR;
    }
    if (modep == NULL || modep->name == NULL) {
	ErrorF("xf86LookupMode: called with invalid modep\n");
	return MODE_ERROR;
    }
    for (cp = clockRanges; cp != NULL; cp = cp->next) {
	/* DivFactor and MulFactor must be > 0 */
	cp->ClockDivFactor = max(1, cp->ClockDivFactor);
	cp->ClockMulFactor = max(1, cp->ClockMulFactor);
    }

    haveBuiltin = FALSE;
    /* Scan the mode pool for matching names */
    for (p = scrp->modePool; p != NULL; p = p->next) {
	if (strcmp(p->name, modep->name) == 0) {
	    /*
	     * Requested mode is a built-in mode. Don't let the user
	     * override it.
	     * Since built-in modes always come before user specified
	     * modes it will always be found first.  
	     */
	    if (p->type & M_T_BUILTIN) {
		haveBuiltin = TRUE;
	    }

	    if (haveBuiltin && !(p->type & M_T_BUILTIN))
		continue;

	    /* Skip over previously rejected modes */
	    if (p->status != MODE_OK) {
		if (!found)
		    status = p->status;
		continue;
	    }
		
	    /* Skip over previously considered modes */
	    if (p->prev)
		continue;

	    if (p->type & M_T_BUILTIN) {
		return xf86HandleBuiltinMode(scrp, p,modep, clockRanges,
					     allowDiv2);
	    }

	    /* Check clock is in range */
	    cp = xf86FindClockRangeForMode(clockRanges, p);
	    if (cp == NULL) {
		/*
		 * XXX Could do more here to provide a more detailed
		 * reason for not finding a mode.
		 */
		p->status = MODE_CLOCK_RANGE;
		if (!found)
		    status = MODE_CLOCK_RANGE;
		continue;
	    }

	    /*
	     * If programmable clock and strategy is not LOOKUP_BEST_REFRESH,
	     * the required mode has been found, otherwise record the refresh
	     * and continue looking.
	     */
	    if (scrp->progClock) {
		found = TRUE;
		if (strategy != LOOKUP_BEST_REFRESH) {
		    bestMode = p;
		    DivFactor = cp->ClockDivFactor;
		    MulFactor = cp->ClockMulFactor;
		    ModePrivFlags = cp->PrivFlags;
		    break;
		}
		if (p->VRefresh > 0.0)
		    refresh = p->VRefresh;
		else {
		    refresh = p->Clock * 1000.0 / p->HTotal / p->VTotal;
		    if (p->Flags & V_INTERLACE)
			refresh *= 2.0;
		    if (p->Flags & V_DBLSCAN)
			refresh /= 2.0;
		    if (p->VScan > 1)
			refresh /= p->VScan;
		}
		if (p->Flags & V_INTERLACE)
		    refresh /= INTERLACE_REFRESH_WEIGHT;
		if (refresh > bestRefresh) {
		    bestMode = p;
		    DivFactor = cp->ClockDivFactor;
		    MulFactor = cp->ClockMulFactor;
		    ModePrivFlags = cp->PrivFlags;
		    bestRefresh = refresh;
		}
		continue;
	    }

	    /*
	     * Clock is in range, so if it is not a programmable clock, find
	     * a matching clock.
	     */

	    i = xf86GetNearestClock(scrp, p->Clock, allowDiv2,
		cp->ClockDivFactor, cp->ClockMulFactor, &k);
	    /*
	     * If the clock is too far from the requested clock, this
	     * mode is no good.
	     */
	    if (k & V_CLKDIV2)
		gap = abs((p->Clock * 2) -
		    ((scrp->clock[i] * cp->ClockDivFactor) / cp->ClockMulFactor));
	    else
		gap = abs(p->Clock -
		    ((scrp->clock[i] * cp->ClockDivFactor) / cp->ClockMulFactor));
	    if (gap > minimumGap) {
		p->status = MODE_NOCLOCK;
		if (!found)
		    status = MODE_NOCLOCK;
		continue;
	    }
	    found = TRUE;

	    if (strategy == LOOKUP_BEST_REFRESH) {
		if (p->VRefresh > 0.0)
		    refresh = p->VRefresh;
		else {
		    refresh = p->Clock * 1000.0 / p->HTotal / p->VTotal;
		    if (p->Flags & V_INTERLACE)
			refresh *= 2.0;
		    if (p->Flags & V_DBLSCAN)
			refresh /= 2.0;
		    if (p->VScan > 1)
			refresh /= p->VScan;
		}
		if (p->Flags & V_INTERLACE)
		    refresh /= INTERLACE_REFRESH_WEIGHT;
		if (refresh > bestRefresh) {
		    bestMode = p;
		    DivFactor = cp->ClockDivFactor;
		    MulFactor = cp->ClockMulFactor;
		    ModePrivFlags = cp->PrivFlags;
		    extraFlags = k;
		    clockIndex = i;
		    bestRefresh = refresh;
		}
		continue;
	    }
	    if (strategy == LOOKUP_CLOSEST_CLOCK) {
		if (gap < minimumGap) {
		    bestMode = p;
		    DivFactor = cp->ClockDivFactor;
		    MulFactor = cp->ClockMulFactor;
		    ModePrivFlags = cp->PrivFlags;
		    extraFlags = k;
		    clockIndex = i;
		    minimumGap = gap;
		}
		continue;
	    }
	    /*
	     * If strategy is neither LOOKUP_BEST_REFRESH or
	     * LOOKUP_CLOSEST_CLOCK the required mode has been found.
	     */
	    bestMode = p;
	    DivFactor = cp->ClockDivFactor;
	    MulFactor = cp->ClockMulFactor;
	    ModePrivFlags = cp->PrivFlags;
	    extraFlags = k;
	    clockIndex = i;
	    break;
	}
    }
    if (!found || bestMode == NULL)
	return status;

    /* Fill in the mode parameters */
    if (scrp->progClock) {
        modep->Clock		= bestMode->Clock;
	modep->ClockIndex	= -1;
	modep->SynthClock	= (modep->Clock * MulFactor) / DivFactor;
    } else {
	modep->Clock		= (scrp->clock[clockIndex] * DivFactor) / MulFactor;
	modep->ClockIndex	= clockIndex;
	modep->SynthClock	= scrp->clock[clockIndex];
	if (extraFlags & V_CLKDIV2) {
	    modep->Clock /= 2;
	    modep->SynthClock /= 2;
	}
    }
    modep->type                 = bestMode->type;
    modep->PrivFlags		= ModePrivFlags;
    modep->HDisplay		= bestMode->HDisplay;
    modep->HSyncStart		= bestMode->HSyncStart;
    modep->HSyncEnd		= bestMode->HSyncEnd;
    modep->HTotal		= bestMode->HTotal;
    modep->HSkew		= bestMode->HSkew;
    modep->VDisplay		= bestMode->VDisplay;
    modep->VSyncStart		= bestMode->VSyncStart;
    modep->VSyncEnd		= bestMode->VSyncEnd;
    modep->VTotal		= bestMode->VTotal;
    modep->VScan		= bestMode->VScan;
    modep->Flags		= bestMode->Flags | extraFlags;
    modep->CrtcHDisplay		= bestMode->CrtcHDisplay;
    modep->CrtcHBlankStart	= bestMode->CrtcHBlankStart;
    modep->CrtcHSyncStart	= bestMode->CrtcHSyncStart;
    modep->CrtcHSyncEnd		= bestMode->CrtcHSyncEnd;
    modep->CrtcHBlankEnd	= bestMode->CrtcHBlankEnd;
    modep->CrtcHTotal		= bestMode->CrtcHTotal;
    modep->CrtcHSkew		= bestMode->CrtcHSkew;
    modep->CrtcVDisplay		= bestMode->CrtcVDisplay;
    modep->CrtcVBlankStart	= bestMode->CrtcVBlankStart;
    modep->CrtcVSyncStart	= bestMode->CrtcVSyncStart;
    modep->CrtcVSyncEnd		= bestMode->CrtcVSyncEnd;
    modep->CrtcVBlankEnd	= bestMode->CrtcVBlankEnd;
    modep->CrtcVTotal		= bestMode->CrtcVTotal;
    modep->CrtcHAdjusted	= bestMode->CrtcHAdjusted;
    modep->CrtcVAdjusted	= bestMode->CrtcVAdjusted;
    modep->HSync		= bestMode->HSync;
    modep->VRefresh		= bestMode->VRefresh;
    modep->Private		= bestMode->Private;
    modep->PrivSize		= bestMode->PrivSize;

    bestMode->prev = modep;

    return MODE_OK;
}


/*
 * xf86SetModeCrtc
 *
 * Initialises the Crtc parameters for a mode.  The initialisation includes
 * adjustments for interlaced and double scan modes.
 */
static void
xf86SetModeCrtc(DisplayModePtr p, int adjustFlags)
{
    if ((p == NULL) || ((p->type & M_T_CRTC_C) == M_T_BUILTIN))
	return;

    p->CrtcHDisplay             = p->HDisplay;
    p->CrtcHSyncStart           = p->HSyncStart;
    p->CrtcHSyncEnd             = p->HSyncEnd;
    p->CrtcHTotal               = p->HTotal;
    p->CrtcHSkew                = p->HSkew;
    p->CrtcVDisplay             = p->VDisplay;
    p->CrtcVSyncStart           = p->VSyncStart;
    p->CrtcVSyncEnd             = p->VSyncEnd;
    p->CrtcVTotal               = p->VTotal;
    if ((p->Flags & V_INTERLACE) && (adjustFlags & INTERLACE_HALVE_V))
    {
        p->CrtcVDisplay         /= 2;
        p->CrtcVSyncStart       /= 2;
        p->CrtcVSyncEnd         /= 2;
        p->CrtcVTotal           /= 2;
    }
    if (p->Flags & V_DBLSCAN) {
        p->CrtcVDisplay         *= 2;
        p->CrtcVSyncStart       *= 2;
        p->CrtcVSyncEnd         *= 2;
        p->CrtcVTotal           *= 2;
    }
    if (p->VScan > 1) {
        p->CrtcVDisplay         *= p->VScan;
        p->CrtcVSyncStart       *= p->VScan;
        p->CrtcVSyncEnd         *= p->VScan;
        p->CrtcVTotal           *= p->VScan;
    }
    p->CrtcHAdjusted = FALSE;
    p->CrtcVAdjusted = FALSE;

    /*
     * XXX
     *
     * The following is taken from VGA, but applies to other cores as well.
     */
    p->CrtcVBlankStart = min(p->CrtcVSyncStart, p->CrtcVDisplay);
    p->CrtcVBlankEnd = max(p->CrtcVSyncEnd, p->CrtcVTotal);
    if ((p->CrtcVBlankEnd - p->CrtcVBlankStart) >= 127) {
        /* 
         * V Blanking size must be < 127.
         * Moving blank start forward is safer than moving blank end
         * back, since monitors clamp just AFTER the sync pulse (or in
         * the sync pulse), but never before.
         */
        p->CrtcVBlankStart = p->CrtcVBlankEnd - 127;
    }
    p->CrtcHBlankStart = min(p->CrtcHSyncStart, p->CrtcHDisplay);
    p->CrtcHBlankEnd = max(p->CrtcHSyncEnd, p->CrtcHTotal);
    if ((p->CrtcHBlankEnd - p->CrtcHBlankStart) >= 63 * 8) {
        /*
         * H Blanking size must be < 63*8. Same remark as above.
         */
        p->CrtcHBlankStart = p->CrtcHBlankEnd - 63 * 8;
    }
}

/*
 * xf86CheckModeForMonitor
 *
 * This function takes a mode and monitor description, and determines
 * if the mode is valid for the monitor.
 */
ModeStatus
xf86CheckModeForMonitor(DisplayModePtr mode, MonPtr monitor)
{
    int i;
    float hsync, vrefresh;

    /* Sanity checks */
    if (mode == NULL || monitor == NULL) {
	ErrorF("xf86CheckModeForMonitor: called with invalid parameters\n");
	return MODE_ERROR;
    }

#ifdef DEBUG
    ErrorF("xf86CheckModeForMonitor(%p %s, %p %s)\n",
	   mode, mode->name, monitor, monitor->id);
#endif

    if (monitor->DDC) {
	xf86MonPtr DDC = (xf86MonPtr)(monitor->DDC);
	struct detailed_monitor_section* detMon;
	struct monitor_ranges *mon_range;
	int i;

	mon_range = NULL;
	for (i = 0; i < 4; i++) {
	    detMon = &DDC->det_mon[i];
	    if(detMon->type == DS_RANGES) {
		mon_range = &detMon->section.ranges;
	    }
	}
	if (mon_range) {
	    /* mode->Clock in kHz, DDC in MHz */
	    if (mon_range->max_clock < 2550 &&
		 mode->Clock / 1000.0 > mon_range->max_clock) {
		xf86Msg(X_WARNING,
		   "(%s,%s) mode clock %gMHz exceeds DDC maximum %dMHz\n",
		   mode->name, monitor->id,
		   mode->Clock/1000.0, mon_range->max_clock);
	    }
	}
    }

    /* Some basic mode validity checks */
    if (0 >= mode->HDisplay || mode->HDisplay > mode->HSyncStart ||
	mode->HSyncStart >= mode->HSyncEnd || mode->HSyncEnd >= mode->HTotal)
	return MODE_H_ILLEGAL;

    if (0 >= mode->VDisplay || mode->VDisplay > mode->VSyncStart ||
	mode->VSyncStart >= mode->VSyncEnd || mode->VSyncEnd >= mode->VTotal)
	return MODE_V_ILLEGAL;

    if (monitor->nHsync > 0) {
	/* Check hsync against the allowed ranges */
	hsync = (float)mode->Clock / (float)mode->HTotal;
	for (i = 0; i < monitor->nHsync; i++)
	    if ((hsync > monitor->hsync[i].lo * (1.0 - SYNC_TOLERANCE)) &&
		(hsync < monitor->hsync[i].hi * (1.0 + SYNC_TOLERANCE)))
		break;

	/* Now see whether we ran out of sync ranges without finding a match */
	if (i == monitor->nHsync)
	    return MODE_HSYNC;
    }

    if (monitor->nVrefresh > 0) {
	/* Check vrefresh against the allowed ranges */
	vrefresh = mode->Clock * 1000.0 / (mode->HTotal * mode->VTotal);
	if (mode->Flags & V_INTERLACE)
	    vrefresh *= 2.0;
	if (mode->Flags & V_DBLSCAN)
	    vrefresh /= 2.0;
	if (mode->VScan > 1)
	    vrefresh /= (float)(mode->VScan);
	for (i = 0; i < monitor->nVrefresh; i++)
	    if ((vrefresh > monitor->vrefresh[i].lo * (1.0 - SYNC_TOLERANCE)) &&
		(vrefresh < monitor->vrefresh[i].hi * (1.0 + SYNC_TOLERANCE)))
		break;

	/* Now see whether we ran out of refresh ranges without finding a match */
	if (i == monitor->nVrefresh)
	    return MODE_VSYNC;
    }

    /* Force interlaced modes to have an odd VTotal */
    if (mode->Flags & V_INTERLACE)
	mode->CrtcVTotal = mode->VTotal |= 1;

    return MODE_OK;
}

/*
 * xf86CheckModeSize
 *
 * An internal routine to check if a mode fits in video memory.  This tries to
 * avoid overflows that would otherwise occur when video memory size is greater
 * than 256MB.
 */
static Bool
xf86CheckModeSize(ScrnInfoPtr scrp, int w, int x, int y)
{
    int bpp = scrp->fbFormat.bitsPerPixel,
	pad = scrp->fbFormat.scanlinePad;
    int lineWidth, lastWidth;

    if (scrp->depth == 4)
	pad *= 4;		/* 4 planes */

    /* Sanity check */
    if ((w < 0) || (x < 0) || (y <= 0))
	return FALSE;

    lineWidth = (((w * bpp) + pad - 1) / pad) * pad;
    lastWidth = x * bpp;

    /*
     * At this point, we need to compare
     *
     *	(lineWidth * (y - 1)) + lastWidth
     *
     * against
     *
     *	scrp->videoRam * (1024 * 8)
     *
     * These are bit quantities.  To avoid overflows, do the comparison in
     * terms of BITMAP_SCANLINE_PAD units.  This assumes BITMAP_SCANLINE_PAD
     * is a power of 2.  We currently use 32, which limits us to a video
     * memory size of 8GB.
     */

    lineWidth = (lineWidth + (BITMAP_SCANLINE_PAD - 1)) / BITMAP_SCANLINE_PAD;
    lastWidth = (lastWidth + (BITMAP_SCANLINE_PAD - 1)) / BITMAP_SCANLINE_PAD;

    if ((lineWidth * (y - 1) + lastWidth) >
	(scrp->videoRam * ((1024 * 8) / BITMAP_SCANLINE_PAD)))
	return FALSE;

    return TRUE;
}

/*
 * xf86InitialCheckModeForDriver
 *
 * This function checks if a mode satisfies a driver's initial requirements:
 *   -  mode size fits within the available pixel area (memory)
 *   -  width lies within the range of supported line pitches
 *   -  mode size fits within virtual size (if fixed)
 *   -  horizontal timings are in range
 *
 * This function takes the following parameters:
 *    scrp         ScrnInfoPtr
 *    mode         mode to check
 *    maxPitch     (optional) maximum line pitch
 *    virtualX     (optional) virtual width requested
 *    virtualY     (optional) virtual height requested
 *
 * In addition, the following fields from the ScrnInfoRec are used:
 *    monitor      pointer to structure for monitor section
 *    fbFormat     pixel format for the framebuffer
 *    videoRam     video memory size (in kB)
 *    maxHValue    maximum horizontal timing value
 *    maxVValue    maximum vertical timing value
 */

ModeStatus
xf86InitialCheckModeForDriver(ScrnInfoPtr scrp, DisplayModePtr mode,
			      ClockRangePtr clockRanges,
			      LookupModeFlags strategy,
			      int maxPitch, int virtualX, int virtualY)
{
    MonPtr monitor;
    ClockRangePtr cp;
    ModeStatus status;
    Bool allowDiv2 = (strategy & LOOKUP_CLKDIV2) != 0;
    int i, needDiv2;

    /* Sanity checks */
    if (!scrp || !mode /*|| !clockRanges*/) {
	ErrorF("xf86InitialCheckModeForDriver: "
		"called with invalid parameters\n");
	return MODE_ERROR;
    }

#ifdef DEBUG
    ErrorF("xf86InitialCheckModeForDriver(%p, %p %s, %p, 0x%x, %d, %d, %d)\n",
	   scrp, mode, mode->name , clockRanges, strategy, maxPitch,  virtualX, virtualY);
#endif

    /* Some basic mode validity checks */
    if (0 >= mode->HDisplay || mode->HDisplay > mode->HSyncStart ||
	mode->HSyncStart >= mode->HSyncEnd || mode->HSyncEnd >= mode->HTotal)
	return MODE_H_ILLEGAL;

    if (0 >= mode->VDisplay || mode->VDisplay > mode->VSyncStart ||
	mode->VSyncStart >= mode->VSyncEnd || mode->VSyncEnd >= mode->VTotal)
	return MODE_V_ILLEGAL;

    if (!xf86CheckModeSize(scrp, mode->HDisplay, mode->HDisplay,
				 mode->VDisplay))
        return MODE_MEM;

    if (maxPitch > 0 && mode->HDisplay > maxPitch)
	return MODE_BAD_WIDTH;

    if (virtualX > 0 && mode->HDisplay > virtualX)
	return MODE_VIRTUAL_X;

    if (virtualY > 0 && mode->VDisplay > virtualY)
	return MODE_VIRTUAL_Y;

    if (scrp->maxHValue > 0 && mode->HTotal > scrp->maxHValue)
	return MODE_BAD_HVALUE;

    if (scrp->maxVValue > 0 && mode->VTotal > scrp->maxVValue)
	return MODE_BAD_VVALUE;

    /*
     * The use of the DisplayModeRec's Crtc* and SynthClock elements below is
     * provisional, in that they are later reused by the driver at mode-set
     * time.  Here, they are temporarily enlisted to contain the mode timings
     * as seen by the CRT or panel (rather than the CRTC).  The driver's
     * ValidMode() is allowed to modify these so it can deal with such things
     * as mode stretching and/or centering.  The driver should >NOT< modify the
     * user-supplied values as these are reported back when mode validation is
     * said and done.
     */
    xf86SetModeCrtc(mode, INTERLACE_HALVE_V);

    cp = xf86FindClockRangeForMode(clockRanges, mode);
    if (!cp)
	return MODE_CLOCK_RANGE;

    if (cp->ClockMulFactor < 1)
	cp->ClockMulFactor = 1;
    if (cp->ClockDivFactor < 1)
	cp->ClockDivFactor = 1;

    /*
     * XXX  The effect of clock dividers and multipliers on the monitor's
     *      pixel clock needs to be verified.
     */
    if (scrp->progClock) {
	mode->SynthClock = mode->Clock;
    } else {
	i = xf86GetNearestClock(scrp, mode->Clock, allowDiv2,
				cp->ClockDivFactor, cp->ClockMulFactor,
				&needDiv2);
	mode->SynthClock = (scrp->clock[i] * cp->ClockDivFactor) /
			   cp->ClockMulFactor;
	if (needDiv2 & V_CLKDIV2)
	    mode->SynthClock /= 2;
    }

    if (scrp->ValidMode) {
	status = (*scrp->ValidMode)(scrp->scrnIndex, mode, FALSE,
				    MODECHECK_INITIAL);
	if (status != MODE_OK)
	    return status;
    }

    if (!(monitor = scrp->monitor)) {
	ErrorF("xf86InitialCheckModeForDriver: "
		"called with invalid monitor\n");
	return MODE_ERROR;
    }

    if (mode->HSync <= 0.0)
	mode->HSync = (float)mode->SynthClock / (float)mode->CrtcHTotal;
    if (monitor->nHsync > 0) {
	/* Check hsync against the allowed ranges */
	for (i = 0; i < monitor->nHsync; i++)
	    if ((mode->HSync > monitor->hsync[i].lo * (1.0 - SYNC_TOLERANCE)) &&
		(mode->HSync < monitor->hsync[i].hi * (1.0 + SYNC_TOLERANCE)))
		break;

	/* Now see whether we ran out of sync ranges without finding a match */
	if (i == monitor->nHsync)
	    return MODE_HSYNC;
    }

    if (mode->VRefresh <= 0.0)
	mode->VRefresh = (mode->SynthClock * 1000.0) /
	    (mode->CrtcHTotal * mode->CrtcVTotal);
    if (monitor->nVrefresh > 0) {
	/* Check vrefresh against the allowed ranges */
	for (i = 0; i < monitor->nVrefresh; i++)
	    if ((mode->VRefresh >
		 monitor->vrefresh[i].lo * (1.0 - SYNC_TOLERANCE)) &&
		(mode->VRefresh <
		 monitor->vrefresh[i].hi * (1.0 + SYNC_TOLERANCE)))
		break;

	/* Now see whether we ran out of refresh ranges without finding a match */
	if (i == monitor->nVrefresh)
	    return MODE_VSYNC;
    }

    /* Force interlaced modes to have an odd VTotal */
    if (mode->Flags & V_INTERLACE)
	mode->CrtcVTotal |= 1;

    /* Assume it is OK */
    return MODE_OK;
}

/*
 * xf86CheckModeForDriver
 *
 * This function is for checking modes while the server is running (for
 * use mainly by the VidMode extension).
 *
 * This function checks if a mode satisfies a driver's requirements:
 *   -  width lies within the line pitch
 *   -  mode size fits within virtual size
 *   -  horizontal/vertical timings are in range
 *
 * This function takes the following parameters:
 *    scrp         ScrnInfoPtr
 *    mode         mode to check
 *    flags        not (currently) used
 *
 * In addition, the following fields from the ScrnInfoRec are used:
 *    maxHValue    maximum horizontal timing value
 *    maxVValue    maximum vertical timing value
 *    virtualX     virtual width
 *    virtualY     virtual height
 *    clockRanges  allowable clock ranges
 */

ModeStatus
xf86CheckModeForDriver(ScrnInfoPtr scrp, DisplayModePtr mode, int flags)
{
    ClockRangesPtr cp;
    int i, k, gap, minimumGap = CLOCK_TOLERANCE + 1;
    int extraFlags = 0;
    int clockIndex = -1;
    int MulFactor = 1;
    int DivFactor = 1;
    int ModePrivFlags = 0;
    Bool allowDiv2;
    ModeStatus status = MODE_NOMODE;

    /* Some sanity checking */
    if (scrp == NULL ||	(!scrp->progClock && scrp->numClocks == 0)) {
	ErrorF("xf86CheckModeForDriver: called with invalid scrnInfoRec\n");
	return MODE_ERROR;
    }
    if (mode == NULL) {
	ErrorF("xf86CheckModeForDriver: called with invalid modep\n");
	return MODE_ERROR;
    }

    /* Check the mode size */
    if (mode->HDisplay > scrp->virtualX)
	return MODE_VIRTUAL_X;

    if (mode->VDisplay > scrp->virtualY)
	return MODE_VIRTUAL_Y;

    if (scrp->maxHValue > 0 && mode->HTotal > scrp->maxHValue)
	return MODE_BAD_HVALUE;

    if (scrp->maxVValue > 0 && mode->VTotal > scrp->maxVValue)
	return MODE_BAD_VVALUE;

    for (cp = scrp->clockRanges; cp != NULL; cp = cp->next) {
	/* DivFactor and MulFactor must be > 0 */
	cp->ClockDivFactor = max(1, cp->ClockDivFactor);
	cp->ClockMulFactor = max(1, cp->ClockMulFactor);
    }

    if (scrp->progClock) {
	/* Check clock is in range */
	for (cp = scrp->clockRanges; cp != NULL; cp = cp->next) {
	    if ((cp->minClock <= mode->Clock) &&
		(cp->maxClock >= mode->Clock) &&
		(cp->interlaceAllowed || !(mode->Flags & V_INTERLACE)) &&
		(cp->doubleScanAllowed ||
		 ((!(mode->Flags & V_DBLSCAN)) && (mode->VScan <= 1))))
	        break;
	}
	if (cp == NULL) {
	    return MODE_CLOCK_RANGE;
	}
	/*
	 * If programmable clock the required mode has been found
	 */
    	DivFactor = cp->ClockDivFactor;
	MulFactor = cp->ClockMulFactor;
	ModePrivFlags = cp->PrivFlags;
    } else {
	 status = MODE_CLOCK_RANGE;
	/* Check clock is in range */
	for (cp = scrp->clockRanges; cp != NULL; cp = cp->next) {
	    if ((cp->minClock <= mode->Clock) &&
		(cp->maxClock >= mode->Clock) &&
		(cp->interlaceAllowed || !(mode->Flags & V_INTERLACE)) &&
		(cp->doubleScanAllowed ||
		 ((!(mode->Flags & V_DBLSCAN)) && (mode->VScan <= 1)))) {

		/*
	 	 * Clock is in range, so if it is not a programmable clock,
		 * find a matching clock.
		 */
    
		allowDiv2 = (cp->strategy & LOOKUP_CLKDIV2) != 0;
		i = xf86GetNearestClock(scrp, mode->Clock, allowDiv2,
			   cp->ClockDivFactor, cp->ClockMulFactor, &k);
		/*
		 * If the clock is too far from the requested clock, this
		 * mode is no good.
		 */
		if (k & V_CLKDIV2)
		    gap = abs((mode->Clock * 2) -
			      ((scrp->clock[i] * cp->ClockDivFactor) /
			       cp->ClockMulFactor));
		else
		    gap = abs(mode->Clock -
			      ((scrp->clock[i] * cp->ClockDivFactor) /
			       cp->ClockMulFactor));
		if (gap > minimumGap) {
		    status = MODE_NOCLOCK;
		    continue;
		}
		
		DivFactor = cp->ClockDivFactor;
		MulFactor = cp->ClockMulFactor;
		ModePrivFlags = cp->PrivFlags;
		extraFlags = k;
		clockIndex = i;
		break;
	    }
	}
	if (cp == NULL)
	    return status;
    }

    /* Fill in the mode parameters */
    if (scrp->progClock) {
	mode->ClockIndex	= -1;
	mode->SynthClock	= (mode->Clock * MulFactor) / DivFactor;
    } else {
	mode->Clock		= (scrp->clock[clockIndex] * DivFactor) / MulFactor;
	mode->ClockIndex	= clockIndex;
	mode->SynthClock	= scrp->clock[clockIndex];
	if (extraFlags & V_CLKDIV2) {
	    mode->Clock /= 2;
	    mode->SynthClock /= 2;
	}
    }
    mode->PrivFlags		= ModePrivFlags;

    return MODE_OK;
}

/*
 * xf86ValidateModes
 *
 * This function takes a set of mode names, modes and limiting conditions,
 * and selects a set of modes and parameters based on those conditions.
 *
 * This function takes the following parameters:
 *    scrp         ScrnInfoPtr
 *    availModes   the list of modes available for the monitor
 *    modeNames    (optional) list of mode names that the screen is requesting
 *    clockRanges  a list of clock ranges
 *    linePitches  (optional) a list of line pitches
 *    minPitch     (optional) minimum line pitch (in pixels)
 *    maxPitch     (optional) maximum line pitch (in pixels)
 *    pitchInc     (mandatory) pitch increment (in bits)
 *    minHeight    (optional) minimum virtual height (in pixels)
 *    maxHeight    (optional) maximum virtual height (in pixels)
 *    virtualX     (optional) virtual width requested (in pixels)
 *    virtualY     (optional) virtual height requested (in pixels)
 *    apertureSize size of video aperture (in bytes)
 *    strategy     how to decide which mode to use from multiple modes with
 *                 the same name
 *
 * In addition, the following fields from the ScrnInfoRec are used:
 *    clocks       a list of discrete clocks
 *    numClocks    number of discrete clocks
 *    progClock    clock is programmable
 *    monitor      pointer to structure for monitor section
 *    fbFormat     format of the framebuffer
 *    videoRam     video memory size
 *    maxHValue    maximum horizontal timing value
 *    maxVValue    maximum vertical timing value
 *    xInc         horizontal timing increment (defaults to 8 pixels)
 *
 * The function fills in the following ScrnInfoRec fields:
 *    modePool     A subset of the modes available to the monitor which
 *		   are compatible with the driver.
 *    modes        one mode entry for each of the requested modes, with the
 *                 status field filled in to indicate if the mode has been
 *                 accepted or not.
 *    virtualX     the resulting virtual width
 *    virtualY     the resulting virtual height
 *    displayWidth the resulting line pitch
 *
 * The function's return value is the number of matching modes found, or -1
 * if an unrecoverable error was encountered.
 */

int
xf86ValidateModes(ScrnInfoPtr scrp, DisplayModePtr availModes,
		  char **modeNames, ClockRangePtr clockRanges,
		  int *linePitches, int minPitch, int maxPitch, int pitchInc,
		  int minHeight, int maxHeight, int virtualX, int virtualY,
		  int apertureSize, LookupModeFlags strategy)
{
    DisplayModePtr p, q, r, new, last, *endp;
    int i, numModes = 0;
    ModeStatus status;
    int linePitch = -1, virtX = 0, virtY = 0;
    int newLinePitch, newVirtX, newVirtY;
    int modeSize;					/* in pixels */
    Bool validateAllDefaultModes;
    Bool userModes = FALSE;
    int saveType;
    PixmapFormatRec *BankFormat;
    ClockRangePtr cp;
    ClockRangesPtr storeClockRanges;
    struct monitor_ranges *mon_range = NULL;

#ifdef DEBUG
    ErrorF("xf86ValidateModes(%p, %p, %p, %p,\n\t\t  %p, %d, %d, %d, %d, %d, %d, %d, %d, 0x%x)\n",
	   scrp, availModes, modeNames, clockRanges,
	   linePitches, minPitch, maxPitch, pitchInc,
	   minHeight, maxHeight, virtualX, virtualY,
	   apertureSize, strategy
	   );
#endif

    /* Some sanity checking */
    if (scrp == NULL || scrp->name == NULL || !scrp->monitor ||
	(!scrp->progClock && scrp->numClocks == 0)) {
	ErrorF("xf86ValidateModes: called with invalid scrnInfoRec\n");
	return -1;
    }
    if (linePitches != NULL && linePitches[0] <= 0) {
	ErrorF("xf86ValidateModes: called with invalid linePitches\n");
	return -1;
    }
    if (pitchInc <= 0) {
	ErrorF("xf86ValidateModes: called with invalid pitchInc\n");
	return -1;
    }
    if ((virtualX > 0) != (virtualY > 0)) {
	ErrorF("xf86ValidateModes: called with invalid virtual resolution\n");
	return -1;
    }

    /* Probe monitor so that we can enforce/warn about its limits */
    if (scrp->monitor->DDC) {
	MonPtr monitor = scrp->monitor;
	xf86MonPtr DDC = (xf86MonPtr)(scrp->monitor->DDC);
	struct detailed_monitor_section* detMon;
	int i;

	for (i = 0; i < 4; i++) {
	    detMon = &DDC->det_mon[i];
	    if(detMon->type == DS_RANGES) {
		mon_range = &detMon->section.ranges;
	    }
	}
	if (mon_range) {

#ifdef DEBUG
	    ErrorF("DDC - Max clock %d, Hsync %d-%d kHz - Vrefresh %d-%d Hz\n",
	           mon_range->max_clock, mon_range->min_h, mon_range->max_h,
	           mon_range->min_v, mon_range->max_v );
#endif

#define DDC_SYNC_TOLERANCE SYNC_TOLERANCE
	    if (monitor->nHsync > 0) {
		for (i = 0; i < monitor->nHsync; i++) {
		    if ((1.0 - DDC_SYNC_TOLERANCE) * mon_range->min_h >
				monitor->hsync[i].lo ||
			(1.0 + DDC_SYNC_TOLERANCE) * mon_range->max_h <
				monitor->hsync[i].hi) {
			xf86DrvMsg(scrp->scrnIndex, X_WARNING,
			  "config file hsync range %g-%gkHz not within DDC "
			  "hsync range %d-%dkHz\n",
			  monitor->hsync[i].lo, monitor->hsync[i].hi,
			  mon_range->min_h, mon_range->max_h);
		    }
		}
	    }

	    if (monitor->nVrefresh > 0) {
		for (i=0; i<monitor->nVrefresh; i++) {
		    if ((1.0 - DDC_SYNC_TOLERANCE) * mon_range->min_v >
				monitor->vrefresh[0].lo ||
			(1.0 + DDC_SYNC_TOLERANCE) * mon_range->max_v <
				monitor->vrefresh[0].hi) {
			xf86DrvMsg(scrp->scrnIndex, X_WARNING,
			  "config file vrefresh range %g-%gHz not within DDC "
			  "vrefresh range %d-%dHz\n",
			  monitor->vrefresh[i].lo, monitor->vrefresh[i].hi,
			  mon_range->min_v, mon_range->max_v);
		    }
		}
	    }
        } /* if (mon_range) */
    }

    /*
     * If requested by the driver, allow missing hsync and/or vrefresh ranges
     * in the monitor section.
     */
    if (strategy & LOOKUP_OPTIONAL_TOLERANCES) {
	strategy &= ~LOOKUP_OPTIONAL_TOLERANCES;
    } else {
	if (scrp->monitor->nHsync <= 0) {
	    if (mon_range) {
		scrp->monitor->hsync[0].lo = mon_range->min_h;
		scrp->monitor->hsync[0].hi = mon_range->max_h;
	    } else {
		scrp->monitor->hsync[0].lo = 28;
		scrp->monitor->hsync[0].hi = 33;
	    }
	    xf86DrvMsg(scrp->scrnIndex, X_WARNING,
		       "%s: Using default hsync range of %.2f-%.2fkHz\n",
		       scrp->monitor->id,
		       scrp->monitor->hsync[0].lo, scrp->monitor->hsync[0].hi);
	    scrp->monitor->nHsync = 1;
	} else {
	  for (i = 0; i < scrp->monitor->nHsync; i++)
	    if (scrp->monitor->hsync[i].lo == scrp->monitor->hsync[i].hi)
	      xf86DrvMsg(scrp->scrnIndex, X_INFO,
			 "%s: Using hsync value of %.2f kHz\n",
			 scrp->monitor->id,
			 scrp->monitor->hsync[i].lo);
	    else
	      xf86DrvMsg(scrp->scrnIndex, X_INFO,
			 "%s: Using hsync range of %.2f-%.2f kHz\n",
			 scrp->monitor->id,
			 scrp->monitor->hsync[i].lo,
			 scrp->monitor->hsync[i].hi);
	}
	if (scrp->monitor->nVrefresh <= 0) {
	    if (mon_range) {
		scrp->monitor->vrefresh[0].lo = mon_range->min_v;
		scrp->monitor->vrefresh[0].hi = mon_range->max_v;
	    } else {
		scrp->monitor->vrefresh[0].lo = 43;
		scrp->monitor->vrefresh[0].hi = 72;
	    }
	    xf86DrvMsg(scrp->scrnIndex, X_WARNING,
		       "%s: using default vrefresh range of %.2f-%.2fHz\n",
		       scrp->monitor->id,
		       scrp->monitor->vrefresh[0].lo,
		       scrp->monitor->vrefresh[0].hi);
	    scrp->monitor->nVrefresh = 1;
	} else {
	  for (i = 0; i < scrp->monitor->nVrefresh; i++)
	    if (scrp->monitor->vrefresh[i].lo == scrp->monitor->vrefresh[i].hi)
	      xf86DrvMsg(scrp->scrnIndex, X_INFO,
			 "%s: Using vrefresh value of %.2f Hz\n",
			 scrp->monitor->id,
			 scrp->monitor->vrefresh[i].lo);
	    else
	      xf86DrvMsg(scrp->scrnIndex, X_INFO,
			 "%s: Using vrefresh range of %.2f-%.2f Hz\n",
			 scrp->monitor->id,
			 scrp->monitor->vrefresh[i].lo,
			 scrp->monitor->vrefresh[i].hi);
	}
    }

    /*
     * Store the clockRanges for later use by the VidMode extension. Must
     * also store the strategy, since ClockDiv2 flag is stored there.
     */
    storeClockRanges = scrp->clockRanges;
    while (storeClockRanges != NULL) {
	storeClockRanges = storeClockRanges->next;
    }
    for (cp = clockRanges; cp != NULL; cp = cp->next,
	   	storeClockRanges = storeClockRanges->next) {
	storeClockRanges = xnfalloc(sizeof(ClockRanges));
	if (scrp->clockRanges == NULL)
	    scrp->clockRanges = storeClockRanges;
	memcpy(storeClockRanges, cp, sizeof(ClockRange));
	storeClockRanges->strategy = strategy;
    }

    /* Determine which pixmap format to pass to miScanLineWidth() */
    if (scrp->depth > 4)
	BankFormat = &scrp->fbFormat;
    else
	BankFormat = xf86GetPixFormat(scrp, 1);	/* >not< scrp->depth! */

    if (scrp->xInc <= 0)
        scrp->xInc = 8;		/* Suitable for VGA and others */

#define _VIRTUALX(x) ((((x) + scrp->xInc - 1) / scrp->xInc) * scrp->xInc)

    /*
     * Determine maxPitch if it wasn't given explicitly.  Note linePitches
     * always takes precedence if is non-NULL.  In that case the minPitch and
     * maxPitch values passed are ignored.
     */
    if (linePitches) {
	minPitch = maxPitch = linePitches[0];
	for (i = 1; linePitches[i] > 0; i++) {
	    if (linePitches[i] > maxPitch)
		maxPitch = linePitches[i];
	    if (linePitches[i] < minPitch)
		minPitch = linePitches[i];
	}
    }

    /* Initial check of virtual size against other constraints */
    scrp->virtualFrom = X_PROBED;
    /*
     * Initialise virtX and virtY if the values are fixed.
     */
    if (virtualY > 0) {
	if (maxHeight > 0 && virtualY > maxHeight) {
	    xf86DrvMsg(scrp->scrnIndex, X_ERROR,
		       "Virtual height (%d) is too large for the hardware "
		       "(max %d)\n", virtualY, maxHeight);
	    return -1;
	}

	if (minHeight > 0 && virtualY < minHeight) {
	    xf86DrvMsg(scrp->scrnIndex, X_ERROR,
		       "Virtual height (%d) is too small for the hardware "
		       "(min %d)\n", virtualY, minHeight);
	    return -1;
	}

	virtualX = _VIRTUALX(virtualX);
	if (linePitches != NULL) {
	    for (i = 0; linePitches[i] != 0; i++) {
		if ((linePitches[i] >= virtualX) &&
		    (linePitches[i] ==
		     miScanLineWidth(virtualX, virtualY, linePitches[i],
				     apertureSize, BankFormat, pitchInc))) {
		    linePitch = linePitches[i];
		    break;
		}
	    }
	} else {
	    linePitch = miScanLineWidth(virtualX, virtualY, minPitch,
					apertureSize, BankFormat, pitchInc);
	}

	if ((linePitch < minPitch) || (linePitch > maxPitch)) {
	    xf86DrvMsg(scrp->scrnIndex, X_ERROR,
		       "Virtual width (%d) is too large for the hardware "
		       "(max %d)\n", virtualX, maxPitch);
	    return -1;
	}

	if (!xf86CheckModeSize(scrp, linePitch, virtualX, virtualY)) {
	    xf86DrvMsg(scrp->scrnIndex, X_ERROR,
		      "Virtual size (%dx%d) (pitch %d) exceeds video memory\n",
		      virtualX, virtualY, linePitch);
	    return -1;
	}

	virtX = virtualX;
	virtY = virtualY;
	scrp->virtualFrom = X_CONFIG;
    }

    /* Print clock ranges and scaled clocks */
    xf86ShowClockRanges(scrp, clockRanges);

    /*
     * If scrp->modePool hasn't been setup yet, set it up now.  This allows the
     * modes that the driver definitely can't use to be weeded out early.  Note
     * that a modePool mode's prev field is used to hold a pointer to the
     * member of the scrp->modes list for which a match was considered.
     */
    if (scrp->modePool == NULL) {
	q = NULL;
	for (p = availModes; p != NULL; p = p->next) {
	    status = xf86InitialCheckModeForDriver(scrp, p, clockRanges,
						   strategy, maxPitch,
						   virtualX, virtualY);

	    if (status == MODE_OK)
		status = xf86CheckModeForMonitor(p, scrp->monitor);

	    if (status == MODE_OK) {
		new = xnfalloc(sizeof(DisplayModeRec));
		*new = *p;
		new->next = NULL;
		if (!q) {
		    scrp->modePool = new;
		} else {
		    q->next = new;
		}
		new->prev = NULL;
		q = new;
		q->name = xnfstrdup(p->name);
	        q->status = MODE_OK;
	    } else {
		if (p->type & M_T_BUILTIN)
		    xf86DrvMsg(scrp->scrnIndex, X_INFO,
			       "Not using built-in mode \"%s\" (%s)\n",
			       p->name, xf86ModeStatusToString(status));
		else if (p->type & M_T_DEFAULT)
		    xf86DrvMsg(scrp->scrnIndex, X_INFO,
			       "Not using default mode \"%s\" (%s)\n", p->name,
			       xf86ModeStatusToString(status));
		else
		    xf86DrvMsg(scrp->scrnIndex, X_INFO,
			       "Not using mode \"%s\" (%s)\n", p->name,
			       xf86ModeStatusToString(status));
	    }
	}

	if (scrp->modePool == NULL) {
	    xf86DrvMsg(scrp->scrnIndex, X_WARNING, "Mode pool is empty\n");
	    return 0;
	}
    } else {
	for (p = scrp->modePool; p != NULL; p = p->next) {
	    p->prev = NULL;
	    p->status = MODE_OK;
	}
    }

    /*
     * Allocate one entry in scrp->modes for each named mode.
     */
    while (scrp->modes)
	xf86DeleteMode(&scrp->modes, scrp->modes);
    endp = &scrp->modes;
    last = NULL;
    if (modeNames != NULL) {
	for (i = 0; modeNames[i] != NULL; i++) {
	    userModes = TRUE;
	    new = xnfcalloc(1, sizeof(DisplayModeRec));
	    new->prev = last;
	    new->type = M_T_USERDEF;
	    new->name = xnfalloc(strlen(modeNames[i]) + 1);
	    strcpy(new->name, modeNames[i]);
	    if (new->prev)
		new->prev->next = new;
	    *endp = last = new;
	    endp = &new->next;
	}
    }

    /* Lookup each mode */
    validateAllDefaultModes = TRUE;
    for (p = scrp->modes; ; p = p->next) {
	Bool repeat;

	/*
	 * If the supplied mode names don't produce a valid mode, scan through
	 * unconsidered modePool members until one survives validation.  This
	 * is done in decreasing order by mode pixel area.
	 */

	if (p == NULL) {
	    if ((numModes > 0) && !validateAllDefaultModes)
		break;

	    validateAllDefaultModes = TRUE;
	    r = NULL;
	    modeSize = 0;
	    for (q = scrp->modePool;  q != NULL;  q = q->next) {
		if ((q->prev == NULL) && (q->status == MODE_OK)) {
		    /*
		     * Deal with the case where this mode wasn't considered
		     * because of a builtin mode of the same name.
		     */
		    for (p = scrp->modes; p != NULL; p = p->next) {
			if ((p->status != MODE_OK) &&
			    !strcmp(p->name, q->name))
			    break;
		    }

		    if (p != NULL)
			q->prev = p;
		    else {
			/*
			 * A quick check to not allow default modes with
			 * horizontal timing parameters that CRTs may have
			 * problems with.
			 */
			if ((q->type & M_T_DEFAULT) &&
			    ((double)q->HTotal / (double)q->HDisplay) < 1.15)
			    continue;

			if (modeSize < (q->HDisplay * q->VDisplay)) {
			    r = q;
			    modeSize = q->HDisplay * q->VDisplay;
			}
		    }
		}
	    }

	    if (r == NULL)
		break;

	    p = xnfcalloc(1, sizeof(DisplayModeRec));
	    p->prev = last;
	    p->name = xnfalloc(strlen(r->name) + 1);
	    if (!userModes)
		p->type = M_T_USERDEF;
	    strcpy(p->name, r->name);
	    if (p->prev)
		p->prev->next = p;
	    *endp = last = p;
	    endp = &p->next;
	}

	repeat = FALSE;
    lookupNext:
	if (repeat && ((status = p->status) != MODE_OK)) {
		if (p->type & M_T_BUILTIN)
		    xf86DrvMsg(scrp->scrnIndex, X_INFO,
			       "Not using built-in mode \"%s\" (%s)\n",
			       p->name, xf86ModeStatusToString(status));
		else if (p->type & M_T_DEFAULT)
		    xf86DrvMsg(scrp->scrnIndex, X_INFO,
			       "Not using default mode \"%s\" (%s)\n", p->name,
			       xf86ModeStatusToString(status));
		else
		    xf86DrvMsg(scrp->scrnIndex, X_INFO,
			       "Not using mode \"%s\" (%s)\n", p->name,
			       xf86ModeStatusToString(status));
	}
	saveType = p->type;
	status = xf86LookupMode(scrp, p, clockRanges, strategy);
	if (repeat && status == MODE_NOMODE) {
	    continue;
	}
	if (status != MODE_OK) {
		if (p->type & M_T_BUILTIN)
		    xf86DrvMsg(scrp->scrnIndex, X_INFO,
			       "Not using built-in mode \"%s\" (%s)\n",
			       p->name, xf86ModeStatusToString(status));
		else if (p->type & M_T_DEFAULT)
		    xf86DrvMsg(scrp->scrnIndex, X_INFO,
			       "Not using default mode \"%s\" (%s)\n", p->name,
			       xf86ModeStatusToString(status));
		else
		    xf86DrvMsg(scrp->scrnIndex, X_INFO,
			       "Not using mode \"%s\" (%s)\n", p->name,
			       xf86ModeStatusToString(status));
	}
	if (status == MODE_ERROR) {
	    ErrorF("xf86ValidateModes: "
		   "unexpected result from xf86LookupMode()\n");
	    return -1;
	}
	if (status != MODE_OK) {
	    if (p->status == MODE_OK)
		p->status = status;
	    continue;
	}
	p->type |= saveType;
	repeat = TRUE;

	newLinePitch = linePitch;
	newVirtX = virtX;
	newVirtY = virtY;

	/*
	 * Don't let non-user defined modes increase the virtual size
	 */
	if (!(p->type & M_T_USERDEF)) {
	    if (p->HDisplay > virtX) {
		p->status = MODE_VIRTUAL_X;
		goto lookupNext;
	    }
	    if (p->VDisplay > virtY) {
		p->status = MODE_VIRTUAL_Y;
		goto lookupNext;
	    }
	}
	/*
	 * Adjust virtual width and height if the mode is too large for the
	 * current values and if they are not fixed.
	 */
	if (virtualX <= 0 && p->HDisplay > newVirtX)
	    newVirtX = _VIRTUALX(p->HDisplay);
	if (virtualY <= 0 && p->VDisplay > newVirtY) {
	    if (maxHeight > 0 && p->VDisplay > maxHeight) {
		p->status = MODE_VIRTUAL_Y;	/* ? */
		goto lookupNext;
	    }
	    newVirtY = p->VDisplay;
	}

	/*
	 * If virtual resolution is to be increased, revalidate it.
	 */
	if ((virtX != newVirtX) || (virtY != newVirtY)) {
	    if (linePitches != NULL) {
		newLinePitch = -1;
		for (i = 0; linePitches[i] != 0; i++) {
		    if ((linePitches[i] >= newVirtX) &&
			(linePitches[i] >= linePitch) &&
			(linePitches[i] ==
			 miScanLineWidth(newVirtX, newVirtY, linePitches[i],
					 apertureSize, BankFormat, pitchInc))) {
			newLinePitch = linePitches[i];
			break;
		    }
		}
	    } else {
		if (linePitch < minPitch)
		    linePitch = minPitch;
		newLinePitch = miScanLineWidth(newVirtX, newVirtY, linePitch,
					       apertureSize, BankFormat,
					       pitchInc);
	    }
	    if ((newLinePitch < minPitch) || (newLinePitch > maxPitch)) {
		p->status = MODE_BAD_WIDTH;
		goto lookupNext;
	    }

	    /*
	     * Check that the pixel area required by the new virtual height
	     * and line pitch isn't too large.
	     */
	    if (!xf86CheckModeSize(scrp, newLinePitch, newVirtX, newVirtY)) {
		p->status = MODE_MEM_VIRT;
		goto lookupNext;
	    }
	}

	if (scrp->ValidMode) {
	    /*
	     * Give the driver a final say, passing it the proposed virtual
	     * geometry.
	     */
	    scrp->virtualX = newVirtX;
	    scrp->virtualY = newVirtY;
	    scrp->displayWidth = newLinePitch;
	    p->status = (scrp->ValidMode)(scrp->scrnIndex, p, FALSE,
					  MODECHECK_FINAL);

	    if (p->status != MODE_OK) {
	        goto lookupNext;
	    }
	}

	/* Mode has passed all the tests */
	virtX = newVirtX;
	virtY = newVirtY;
	linePitch = newLinePitch;
	p->status = MODE_OK;
	numModes++;
    }

#undef _VIRTUALX

    /* Update the ScrnInfoRec parameters */
    
    scrp->virtualX = virtX;
    scrp->virtualY = virtY;
    scrp->displayWidth = linePitch;
    
    /* Make the mode list into a circular list by joining up the ends */
    if (numModes > 0) {
	p = scrp->modes;
	while (p->next != NULL)
	    p = p->next;
	/* p is now the last mode on the list */
	p->next = scrp->modes;
	scrp->modes->prev = p;
    }

    if (minHeight > 0 && virtY < minHeight) {
	xf86DrvMsg(scrp->scrnIndex, X_ERROR,
		   "Virtual height (%d) is too small for the hardware "
		   "(min %d)\n", virtY, minHeight);
	return -1;
    }

    return numModes;
}

/*
 * xf86DeleteMode
 *
 * This function removes a mode from a list of modes.
 *
 * There are different types of mode lists:
 *
 *  - singly linked linear lists, ending in NULL
 *  - doubly linked linear lists, starting and ending in NULL
 *  - doubly linked circular lists
 *
 */
 
void
xf86DeleteMode(DisplayModePtr *modeList, DisplayModePtr mode)
{
    /* Catch the easy/insane cases */
    if (modeList == NULL || *modeList == NULL || mode == NULL)
	return;

    /* If the mode is at the start of the list, move the start of the list */
    if (*modeList == mode)
	*modeList = mode->next;

    /* If mode is the only one on the list, set the list to NULL */
    if ((mode == mode->prev) && (mode == mode->next)) {
	*modeList = NULL;
    } else {
	if ((mode->prev != NULL) && (mode->prev->next == mode))
	    mode->prev->next = mode->next;
	if ((mode->next != NULL) && (mode->next->prev == mode))
	    mode->next->prev = mode->prev;
    }

    xfree(mode->name);
    xfree(mode);
}

/*
 * xf86PruneDriverModes
 *
 * Remove modes from the driver's mode list which have been marked as
 * invalid.
 */

void
xf86PruneDriverModes(ScrnInfoPtr scrp)
{
    DisplayModePtr first, p, n;

    p = scrp->modes;
    if (p == NULL)
	return;

    do {
	if (!(first = scrp->modes))
	    return;
	n = p->next;
	if (p->status != MODE_OK) {
#if 0
	    if (p->type & M_T_BUILTIN)
		xf86DrvMsg(scrp->scrnIndex, X_INFO,
			   "Not using built-in mode \"%s\" (%s)\n", p->name,
			   xf86ModeStatusToString(p->status));
	    else if (p->type & M_T_DEFAULT)
		xf86DrvMsg(scrp->scrnIndex, X_INFO,
			   "Not using default mode \"%s\" (%s)\n", p->name,
			   xf86ModeStatusToString(p->status));
	    else
	        xf86DrvMsg(scrp->scrnIndex, X_INFO,
			   "Not using mode \"%s\" (%s)\n", p->name,
			   xf86ModeStatusToString(p->status));
#endif
	    xf86DeleteMode(&(scrp->modes), p);
	}
	p = n;
    } while (p != NULL && p != first);

    /* modePool is no longer needed, turf it */
    while (scrp->modePool)
	xf86DeleteMode(&scrp->modePool, scrp->modePool);
}


/*
 * xf86SetCrtcForModes
 *
 * Goes through the screen's mode list, and initialises the Crtc
 * parameters for each mode.  The initialisation includes adjustments
 * for interlaced and double scan modes.
 */
void
xf86SetCrtcForModes(ScrnInfoPtr scrp, int adjustFlags)
{
    DisplayModePtr p;

    /*
     * Store adjustFlags for use with the VidMode extension. There is an
     * implicit assumption here that SetCrtcForModes is called once.
     */
    scrp->adjustFlags = adjustFlags;

    p = scrp->modes;
    if (p == NULL)
	return;

    do {
	xf86SetModeCrtc(p, adjustFlags);
#ifdef DEBUG
	ErrorF("%sMode %s: %d (%d) %d %d (%d) %d %d (%d) %d %d (%d) %d\n",
	       (p->type & M_T_DEFAULT) ? "Default " : "",
	       p->name, p->CrtcHDisplay, p->CrtcHBlankStart,
	       p->CrtcHSyncStart, p->CrtcHSyncEnd, p->CrtcHBlankEnd,
	       p->CrtcHTotal, p->CrtcVDisplay, p->CrtcVBlankStart,
	       p->CrtcVSyncStart, p->CrtcVSyncEnd, p->CrtcVBlankEnd,
	       p->CrtcVTotal);
#endif
	p = p->next;
    } while (p != NULL && p != scrp->modes);
}


static void
add(char **p, char *new)
{
    *p = xnfrealloc(*p, strlen(*p) + strlen(new) + 2);
    strcat(*p, " ");
    strcat(*p, new);
}

static void
PrintModeline(int scrnIndex,DisplayModePtr mode)
{
    char tmp[256];
    char *flags = xnfcalloc(1, 1);

    if (mode->HSkew) { 
	snprintf(tmp, 256, "hskew %i", mode->HSkew); 
	add(&flags, tmp);
    }
    if (mode->VScan) { 
	snprintf(tmp, 256, "vscan %i", mode->VScan); 
	add(&flags, tmp);
    }
    if (mode->Flags & V_INTERLACE) add(&flags, "interlace");
    if (mode->Flags & V_CSYNC) add(&flags, "composite");
    if (mode->Flags & V_DBLSCAN) add(&flags, "doublescan");
    if (mode->Flags & V_BCAST) add(&flags, "bcast");
    if (mode->Flags & V_PHSYNC) add(&flags, "+hsync");
    if (mode->Flags & V_NHSYNC) add(&flags, "-hsync");
    if (mode->Flags & V_PVSYNC) add(&flags, "+vsync");
    if (mode->Flags & V_NVSYNC) add(&flags, "-vsync");
    if (mode->Flags & V_PCSYNC) add(&flags, "+csync");
    if (mode->Flags & V_NCSYNC) add(&flags, "-csync");
#if 0
    if (mode->Flags & V_CLKDIV2) add(&flags, "vclk/2");
#endif
    xf86DrvMsgVerb(scrnIndex, X_INFO, 3,
		   "Modeline \"%s\"  %6.2f  %i %i %i %i  %i %i %i %i%s\n",
		   mode->name, mode->Clock/1000., mode->HDisplay,
		   mode->HSyncStart, mode->HSyncEnd, mode->HTotal,
		   mode->VDisplay, mode->VSyncStart, mode->VSyncEnd,
		   mode->VTotal, flags);
    xfree(flags);
}

void
xf86PrintModes(ScrnInfoPtr scrp)
{
    DisplayModePtr p;
    float hsync, refresh = 0;
    char *desc, *desc2, *prefix, *uprefix;

    if (scrp == NULL)
	return;

    xf86DrvMsg(scrp->scrnIndex, scrp->virtualFrom, "Virtual size is %dx%d "
	       "(pitch %d)\n", scrp->virtualX, scrp->virtualY,
	       scrp->displayWidth);
    
    p = scrp->modes;
    if (p == NULL)
	return;

    do {
	desc = desc2 = "";
	if (p->HSync > 0.0)
	    hsync = p->HSync;
	else if (p->HTotal > 0)
	    hsync = (float)p->Clock / (float)p->HTotal;
	else
	    hsync = 0.0;
	if (p->VTotal > 0)
	    refresh = hsync * 1000.0 / p->VTotal;
	if (p->Flags & V_INTERLACE) {
	    refresh *= 2.0;
	    desc = " (I)";
	}
	if (p->Flags & V_DBLSCAN) {
	    refresh /= 2.0;
	    desc = " (D)";
	}
	if (p->VScan > 1) {
	    refresh /= p->VScan;
	    desc2 = " (VScan)";
	}
	if (p->VRefresh > 0.0)
	    refresh = p->VRefresh;
	if (p->type & M_T_BUILTIN)
	    prefix = "Built-in mode";
	else if (p->type & M_T_DEFAULT)
	    prefix = "Default mode";
	else
	    prefix = "Mode";
	if (p->type & M_T_USERDEF)
	    uprefix = "*";
	else
	    uprefix = " ";
	if (hsync == 0 || refresh == 0) {
	    if (p->name)
		xf86DrvMsg(scrp->scrnIndex, X_CONFIG,
			   "%s%s \"%s\"\n", uprefix, prefix, p->name);
	    else
		xf86DrvMsg(scrp->scrnIndex, X_PROBED,
			   "%s%s %dx%d (unnamed)\n",
			   uprefix, prefix, p->HDisplay, p->VDisplay);
	} else if (p->Clock == p->SynthClock) {
	    xf86DrvMsg(scrp->scrnIndex, X_CONFIG,
			"%s%s \"%s\": %.1f MHz, %.1f kHz, %.1f Hz%s%s\n",
			uprefix, prefix, p->name, p->Clock / 1000.0,
			hsync, refresh, desc, desc2);
	} else {
	    xf86DrvMsg(scrp->scrnIndex, X_CONFIG,
			"%s%s \"%s\": %.1f MHz (scaled from %.1f MHz), "
			"%.1f kHz, %.1f Hz%s%s\n",
			uprefix, prefix, p->name, p->Clock / 1000.0,
			p->SynthClock / 1000.0, hsync, refresh, desc, desc2);
	}
	if (hsync != 0 && refresh != 0)
	    PrintModeline(scrp->scrnIndex,p);
	p = p->next;
    } while (p != NULL && p != scrp->modes);
}
