/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis300_accel.c,v 1.14 2003/01/29 15:42:16 eich Exp $ */
/*
 * 2D Acceleration for SiS300, SiS540, SiS630, SiS730, SiS530, SiS620
 *
 * Copyright Xavier Ducoin <x.ducoin@lectra.com>
 * Copyright 2002 by Thomas Winischhofer, Vienna, Austria
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holders not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 *      Authors:
 *
 *      Xavier Ducoin <x.ducoin@lectra.com>
 *      Thomas Winischhofer <thomas@winischhofer.net>
 *
 */
#if 0
#define DEBUG
#endif

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "compiler.h"
#include "xaa.h"

#include "sis.h"
#include "sis300_accel.h"

#ifdef SISDUALHEAD
/* TW: This is the offset to the memory for each head */
#define HEADOFFSET 	(pSiS->dhmOffset)
#endif

#undef STSCE    	/* TW: Use/Don't use ScreenToScreenColorExpand - does not work */

#undef TRAP     	/* TW: Use/Don't use Trapezoid Fills - does not work - XAA provides
		         * illegal trapezoid data (left and right edges cross each other
			 * sometimes) which causes drawing errors. Further, I have not found
			 * out how to draw polygones with a height greater than 127...
                         */

static void SiSInitializeAccelerator(ScrnInfoPtr pScrn);
static void SiSSync(ScrnInfoPtr pScrn);
static void SiSSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
                                int xdir, int ydir, int rop,
                                unsigned int planemask, int trans_color);
static void SiSSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
                                int x1, int y1, int x2, int y2,
                                int width, int height);
static void SiSSetupForSolidFill(ScrnInfoPtr pScrn, int color,
                                int rop, unsigned int planemask);
static void SiSSubsequentSolidFillRect(ScrnInfoPtr pScrn,
                                int x, int y, int w, int h);
#ifdef TRAP
static void SiSSubsequentSolidFillTrap(ScrnInfoPtr pScrn, int y, int h,
	                        int left, int dxL, int dyL, int eL,
	                        int right, int dxR, int dyR, int eR);
#endif
static void SiSSetupForSolidLine(ScrnInfoPtr pScrn, int color,
                                int rop, unsigned int planemask);
static void SiSSubsequentSolidTwoPointLine(ScrnInfoPtr pScrn, int x1,
                                int y1, int x2, int y2, int flags);
static void SiSSubsequentSolidHorzVertLine(ScrnInfoPtr pScrn,
                                int x, int y, int len, int dir);
static void SiSSetupForDashedLine(ScrnInfoPtr pScrn,
                                int fg, int bg, int rop, unsigned int planemask,
                                int length, unsigned char *pattern);
static void SiSSubsequentDashedTwoPointLine(ScrnInfoPtr pScrn,
                                int x1, int y1, int x2, int y2,
                                int flags, int phase);
static void SiSSetupForMonoPatternFill(ScrnInfoPtr pScrn,
                                int patx, int paty, int fg, int bg,
                                int rop, unsigned int planemask);
static void SiSSubsequentMonoPatternFill(ScrnInfoPtr pScrn,
                                int patx, int paty,
                                int x, int y, int w, int h);
#ifdef TRAP
static void SiSSubsequentMonoPatternFillTrap(ScrnInfoPtr pScrn,
                                int patx, int paty,
                                int y, int h,
                                int left, int dxL, int dyL, int eL,
	                        int right, int dxR, int dyR, int eR );
#endif
#if 0
static void SiSSetupForColorPatternFill(ScrnInfoPtr pScrn,
                                int patx, int paty, int rop,
                                unsigned int planemask,
                                int trans_color);
static void SiSSubsequentColorPatternFill(ScrnInfoPtr pScrn,
                                int patx, int paty,
                                int x, int y, int w, int h);
#endif
#if 0
static void SiSSetupForCPUToScreenColorExpand(ScrnInfoPtr pScrn,
                                int fg, int bg,
                                int rop, unsigned int planemask);
static void SiSSubsequentCPUToScreenColorExpand(ScrnInfoPtr pScrn,
                                int x, int y, int w, int h, int skipleft);
#endif
#ifdef STSCE
static void SiSSetupForScreenToScreenColorExpand(ScrnInfoPtr pScrn,
                                int fg, int bg,
                                int rop, unsigned int planemask);
static void SiSSubsequentScreenToScreenColorExpand(ScrnInfoPtr pScrn,
                                int x, int y, int w, int h,
                                int srcx, int srcy, int skipleft);
#endif
static void SiSSetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                int fg, int bg, int rop,
                                unsigned int planemask);
static void SiSSubsequentScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
                                int x, int y, int w, int h,
                                int skipleft);
static void SiSSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno);

#ifdef SISDUALHEAD
static void SiSRestoreAccelState(ScrnInfoPtr pScrn);
#endif

static void
SiSInitializeAccelerator(ScrnInfoPtr pScrn)
{
	SISPtr  pSiS = SISPTR(pScrn);

	pSiS->DoColorExpand = FALSE;
}

Bool
SiS300AccelInit(ScreenPtr pScreen)
{
	XAAInfoRecPtr   infoPtr;
	ScrnInfoPtr     pScrn = xf86Screens[pScreen->myNum];
	SISPtr          pSiS = SISPTR(pScrn);
	int		topFB;
	int             reservedFbSize;
	int             UsableFbSize;
	unsigned char   *AvailBufBase;
	BoxRec          Avail;
	int             i;

	pSiS->AccelInfoPtr = infoPtr = XAACreateInfoRec();
	if (!infoPtr)  return FALSE;

	SiSInitializeAccelerator(pScrn);

	infoPtr->Flags = LINEAR_FRAMEBUFFER |
			 OFFSCREEN_PIXMAPS |
			 PIXMAP_CACHE;

	/* sync */
	infoPtr->Sync = SiSSync;

	/* Acceleration only supported at 8, 16 and 32 bpp */
	if((pScrn->bitsPerPixel != 8) && (pScrn->bitsPerPixel != 16) &&
	         (pScrn->bitsPerPixel != 32))
	    return FALSE;

	/* screen to screen copy - TW: We now support transparent copies */
	infoPtr->SetupForScreenToScreenCopy = SiSSetupForScreenToScreenCopy;
	infoPtr->SubsequentScreenToScreenCopy = SiSSubsequentScreenToScreenCopy;
	infoPtr->ScreenToScreenCopyFlags = NO_PLANEMASK |
	                                   TRANSPARENCY_GXCOPY_ONLY;

	/* solid fills */
	infoPtr->SetupForSolidFill = SiSSetupForSolidFill;
	infoPtr->SubsequentSolidFillRect = SiSSubsequentSolidFillRect;
#ifdef TRAP
	infoPtr->SubsequentSolidFillTrap = SiSSubsequentSolidFillTrap;
#endif
	infoPtr->SolidFillFlags = NO_PLANEMASK;

	/* solid line */
	infoPtr->SetupForSolidLine = SiSSetupForSolidLine;
	infoPtr->SubsequentSolidTwoPointLine = SiSSubsequentSolidTwoPointLine;
	infoPtr->SubsequentSolidHorVertLine = SiSSubsequentSolidHorzVertLine;
	infoPtr->SolidLineFlags = NO_PLANEMASK;

	/* dashed line */
	infoPtr->SetupForDashedLine = SiSSetupForDashedLine;
	infoPtr->SubsequentDashedTwoPointLine = SiSSubsequentDashedTwoPointLine;
	infoPtr->DashPatternMaxLength = 64;
	infoPtr->DashedLineFlags = NO_PLANEMASK |
				   LINE_PATTERN_MSBFIRST_LSBJUSTIFIED;

	/* 8x8 mono pattern fill */
	infoPtr->SetupForMono8x8PatternFill = SiSSetupForMonoPatternFill;
	infoPtr->SubsequentMono8x8PatternFillRect = SiSSubsequentMonoPatternFill;
#ifdef TRAP
	infoPtr->SubsequentMono8x8PatternFillTrap = SiSSubsequentMonoPatternFillTrap;
#endif
	infoPtr->Mono8x8PatternFillFlags = NO_PLANEMASK |
					   HARDWARE_PATTERN_SCREEN_ORIGIN |
					   HARDWARE_PATTERN_PROGRAMMED_BITS |
					   NO_TRANSPARENCY |
					   BIT_ORDER_IN_BYTE_MSBFIRST ;

#ifdef STSCE
	/* Screen To Screen Color Expand */
	/* TW: The hardware does support this the way we need it */
	infoPtr->SetupForScreenToScreenColorExpandFill =
	    			SiSSetupForScreenToScreenColorExpand;
	infoPtr->SubsequentScreenToScreenColorExpandFill =
	    			SiSSubsequentScreenToScreenColorExpand;
	infoPtr->ScreenToScreenColorExpandFillFlags = NO_PLANEMASK |
	                                              BIT_ORDER_IN_BYTE_MSBFIRST ;
#endif

#if 0
	/* CPU To Screen Color Expand --- implement another instead of this one! */
	infoPtr->SetupForCPUToScreenColorExpandFill =
	    SiSSetupForCPUToScreenColorExpand;
	infoPtr->SubsequentCPUToScreenColorExpandFill =
	    SiSSubsequentCPUToScreenColorExpand;
	infoPtr->ColorExpandRange = PATREGSIZE;
	infoPtr->ColorExpandBase = pSiS->IOBase+PBR(0);
	infoPtr->CPUToScreenColorExpandFillFlags = NO_PLANEMASK |
	    BIT_ORDER_IN_BYTE_MSBFIRST |
	    NO_TRANSPARENCY |
	    SYNC_AFTER_COLOR_EXPAND |
	    HARDWARE_PATTERN_SCREEN_ORIGIN |
	    HARDWARE_PATTERN_PROGRAMMED_BITS ;
#endif

	/* per-scanline color expansion (using indirect method) */
	if(pSiS->VGAEngine == SIS_530_VGA) {
	   pSiS->ColorExpandBufferNumber = 4;
	   pSiS->ColorExpandBufferCountMask = 0x03;
	} else {
	   pSiS->ColorExpandBufferNumber = 16;
	   pSiS->ColorExpandBufferCountMask = 0x0F;
	}
	pSiS->PerColorExpandBufferSize = ((pScrn->virtualX + 31)/32) * 4;
	infoPtr->NumScanlineColorExpandBuffers = pSiS->ColorExpandBufferNumber;
	infoPtr->ScanlineColorExpandBuffers = (unsigned char **)&pSiS->ColorExpandBufferAddr[0];

	infoPtr->SetupForScanlineCPUToScreenColorExpandFill =
	                            SiSSetupForScanlineCPUToScreenColorExpandFill;
	infoPtr->SubsequentScanlineCPUToScreenColorExpandFill =
	                            SiSSubsequentScanlineCPUToScreenColorExpandFill;
	infoPtr->SubsequentColorExpandScanline =
	                            SiSSubsequentColorExpandScanline;
	infoPtr->ScanlineCPUToScreenColorExpandFillFlags =
	    NO_PLANEMASK |
	    CPU_TRANSFER_PAD_DWORD |
	    SCANLINE_PAD_DWORD |
	    BIT_ORDER_IN_BYTE_MSBFIRST |
	    LEFT_EDGE_CLIPPING;

#ifdef SISDUALHEAD
	if (pSiS->DualHeadMode) {
		infoPtr->RestoreAccelState = SiSRestoreAccelState;
	}
#endif

	/* init Frame Buffer Manager */
	topFB = pSiS->maxxfbmem;

	reservedFbSize = pSiS->ColorExpandBufferNumber * pSiS->PerColorExpandBufferSize;

	UsableFbSize = topFB - reservedFbSize;

	/* Layout: (Sizes do not reflect correct proportions)
	 * |--------------++++++++++++++++++++^************==========~~~~~~~~~~~~|
	 *   UsableFbSize  ColorExpandBuffers | DRI-Heap  |  HWCursor  TurboQueue   300/310/325 series
	 * |--------------++++++++++++++++++++|  ====================~~~~~~~~~~~~|
	 *   UsableFbSize  ColorExpandBuffers |        TurboQueue     HWCursor      530/620
	 *                                  topFB
	 */

	AvailBufBase = pSiS->FbBase + UsableFbSize;
	for (i = 0; i < pSiS->ColorExpandBufferNumber; i++) {
		pSiS->ColorExpandBufferAddr[i] = AvailBufBase +
		    i * pSiS->PerColorExpandBufferSize;
		pSiS->ColorExpandBufferScreenOffset[i] = UsableFbSize +
		    i * pSiS->PerColorExpandBufferSize;
	}
	Avail.x1 = 0;
	Avail.y1 = 0;
	Avail.x2 = pScrn->displayWidth;
	Avail.y2 = (UsableFbSize / (pScrn->displayWidth * pScrn->bitsPerPixel/8)) - 1;

	if(Avail.y2 < 0)  Avail.y2 = 32767;

	if(Avail.y2 < pScrn->currentMode->VDisplay) {
		xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Not enough video RAM for accelerator. At least "
			"%dKB needed, %dKB available\n",
			((((pScrn->displayWidth * pScrn->bitsPerPixel/8)   /* TW: +8 for make it sure */
			     * pScrn->currentMode->VDisplay) + reservedFbSize) / 1024) + 8,
			pSiS->maxxfbmem/1024);
		pSiS->NoAccel = TRUE;
		pSiS->NoXvideo = TRUE;
		XAADestroyInfoRec(pSiS->AccelInfoPtr);
		pSiS->AccelInfoPtr = NULL;
		return FALSE;
	}

	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
		   "Frame Buffer From (%d,%d) To (%d,%d)\n",
		   Avail.x1, Avail.y1, Avail.x2, Avail.y2);

	xf86InitFBManager(pScreen, &Avail);

	return(XAAInit(pScreen, infoPtr));
}


static void 
SiSSync(ScrnInfoPtr pScrn)
{
	SISPtr pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("SiSSync()\n"));

	pSiS->DoColorExpand = FALSE;
	SiSIdle
}

#ifdef SISDUALHEAD
static void
SiSRestoreAccelState(ScrnInfoPtr pScrn)
{
	SISPtr pSiS = SISPTR(pScrn);

	/* TW: We don't need to do anything special here; forcing the
	 *     other head to re-read the CmdQueLen is not necessary:
	 *     After the Sync in RestoreAccelState(), the real queue
	 *     length is always larger than (or at least equal to)
	 *     the amount stored in CmdQueueLen of the other head,
	 *     so the only thing that might happen is one unnecessary
	 *     Sync on the other head. I think we can live with that.
	 */
	pSiS->DoColorExpand = FALSE;
	SiSIdle
}
#endif

static const int sisALUConv[] =
{
    0x00,       /* dest = 0;            0,      GXclear,        0 */
    0x88,       /* dest &= src;         DSa,    GXand,          0x1 */
    0x44,       /* dest = src & ~dest;  SDna,   GXandReverse,   0x2 */
    0xCC,       /* dest = src;          S,      GXcopy,         0x3 */
    0x22,       /* dest &= ~src;        DSna,   GXandInverted,  0x4 */
    0xAA,       /* dest = dest;         D,      GXnoop,         0x5 */
    0x66,       /* dest = ^src;         DSx,    GXxor,          0x6 */
    0xEE,       /* dest |= src;         DSo,    GXor,           0x7 */
    0x11,       /* dest = ~src & ~dest; DSon,   GXnor,          0x8 */
    0x99,       /* dest ^= ~src ;       DSxn,   GXequiv,        0x9 */
    0x55,       /* dest = ~dest;        Dn,     GXInvert,       0xA */
    0xDD,       /* dest = src|~dest ;   SDno,   GXorReverse,    0xB */
    0x33,       /* dest = ~src;         Sn,     GXcopyInverted, 0xC */
    0xBB,       /* dest |= ~src;        DSno,   GXorInverted,   0xD */
    0x77,       /* dest = ~src|~dest;   DSan,   GXnand,         0xE */
    0xFF,       /* dest = 0xFF;         1,      GXset,          0xF */
};
/* same ROP but with Pattern as Source */
static const int sisPatALUConv[] =
{
    0x00,       /* dest = 0;            0,      GXclear,        0 */
    0xA0,       /* dest &= src;         DPa,    GXand,          0x1 */
    0x50,       /* dest = src & ~dest;  PDna,   GXandReverse,   0x2 */
    0xF0,       /* dest = src;          P,      GXcopy,         0x3 */
    0x0A,       /* dest &= ~src;        DPna,   GXandInverted,  0x4 */
    0xAA,       /* dest = dest;         D,      GXnoop,         0x5 */
    0x5A,       /* dest = ^src;         DPx,    GXxor,          0x6 */
    0xFA,       /* dest |= src;         DPo,    GXor,           0x7 */
    0x05,       /* dest = ~src & ~dest; DPon,   GXnor,          0x8 */
    0xA5,       /* dest ^= ~src ;       DPxn,   GXequiv,        0x9 */
    0x55,       /* dest = ~dest;        Dn,     GXInvert,       0xA */
    0xF5,       /* dest = src|~dest ;   PDno,   GXorReverse,    0xB */
    0x0F,       /* dest = ~src;         Pn,     GXcopyInverted, 0xC */
    0xAF,       /* dest |= ~src;        DPno,   GXorInverted,   0xD */
    0x5F,       /* dest = ~src|~dest;   DPan,   GXnand,         0xE */
    0xFF,       /* dest = 0xFF;         1,      GXset,          0xF */
};

static void SiSSetupForScreenToScreenCopy(ScrnInfoPtr pScrn,
                                int xdir, int ydir, int rop,
                                unsigned int planemask, int trans_color)
{
	SISPtr  pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup ScreenCopy(%d, %d, 0x%x, 0x%x, 0x%x)\n",
					xdir, ydir, rop, planemask, trans_color));

	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupSRCPitch(pSiS->scrnOffset)
	SiSSetupDSTRect(pSiS->scrnOffset, -1)

	if(trans_color != -1) {
		SiSSetupROP(0x0A)
		SiSSetupSRCTrans(trans_color)
		SiSSetupCMDFlag(TRANSPARENT_BITBLT)
	} else {
	        SiSSetupROP(sisALUConv[rop])
	}
	if(xdir > 0) {
		SiSSetupCMDFlag(X_INC)
	}
	if(ydir > 0) {
		SiSSetupCMDFlag(Y_INC)
	}
}

static void SiSSubsequentScreenToScreenCopy(ScrnInfoPtr pScrn,
                                int src_x, int src_y, int dst_x, int dst_y,
                                int width, int height)
{
	SISPtr  pSiS = SISPTR(pScrn);
	long srcbase, dstbase;

	PDEBUG(ErrorF("Subsequent ScreenCopy(%d,%d, %d,%d, %d,%d)\n",
					src_x, src_y, dst_x, dst_y, width, height));

	srcbase = dstbase = 0;
	if(src_y >= 2048) {
		srcbase = pSiS->scrnOffset * src_y;
		src_y = 0;
	}
	if( (dst_y >= pScrn->virtualY) || (dst_y >= 2048) ) {
		dstbase = pSiS->scrnOffset * dst_y;
		dst_y = 0;
	}
#ifdef SISDUALHEAD
	if(pSiS->VGAEngine != SIS_530_VGA) {
	   	srcbase += HEADOFFSET;
	   	dstbase += HEADOFFSET;
	}
#endif
	SiSSetupSRCBase(srcbase);
	SiSSetupDSTBase(dstbase);

	if(!(pSiS->CommandReg & X_INC))  {
		src_x += width-1;
		dst_x += width-1;
	}
	if(!(pSiS->CommandReg & Y_INC))  {
		src_y += height-1;
		dst_y += height-1;
	}
	SiSSetupRect(width, height)
	SiSSetupSRCXY(src_x, src_y)
	SiSSetupDSTXY(dst_x, dst_y)

	SiSDoCMD
}

static void
SiSSetupForSolidFill(ScrnInfoPtr pScrn,
                        int color, int rop, unsigned int planemask)
{
	SISPtr  pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup SolidFill(0x%x, 0x%x, 0x%x)\n",
					color, rop, planemask));

	SiSSetupPATFG(color)
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupROP(sisPatALUConv[rop])
	/* SiSSetupCMDFlag(PATFG) - is zero */
}

static void
SiSSubsequentSolidFillRect(ScrnInfoPtr pScrn,
                        int x, int y, int w, int h)
{
	SISPtr  pSiS = SISPTR(pScrn);
	long dstbase;

	PDEBUG(ErrorF("Subsequent SolidFillRect(%d, %d, %d, %d)\n",
					x, y, w, h));
	dstbase = 0;

	if (y >= 2048) {
		dstbase = pSiS->scrnOffset * y;
		y = 0;
	}
#ifdef SISDUALHEAD
        if(pSiS->VGAEngine != SIS_530_VGA) {
	   	dstbase += HEADOFFSET;
        }
#endif
	SiSSetupDSTBase(dstbase)
	SiSSetupDSTXY(x,y)
	SiSSetupRect(w,h)
	/* Clear commandReg because Setup can be used for Rect and Trap */
	pSiS->CommandReg &= ~(T_XISMAJORL | T_XISMAJORR |
	                      T_L_X_INC | T_L_Y_INC |
	                      T_R_X_INC | T_R_Y_INC |
			      TRAPAZOID_FILL);
	SiSSetupCMDFlag(X_INC | Y_INC | BITBLT)

	SiSDoCMD
}

/* TW: Trapezoid */
/* This would work better if XAA would provide us with valid trapezoids.
 * In fact, with small trapezoids the left and the right edge often cross
 * each other or result in a line length of 0 which causes drawing errors
 * (filling over whole scanline).
 * Furthermore, I have not found out how to draw trapezoids with a height
 * greater than 127.
 */
#ifdef TRAP
static void
SiSSubsequentSolidFillTrap(ScrnInfoPtr pScrn, int y, int h,
	       int left,  int dxL, int dyL, int eL,
	       int right, int dxR, int dyR, int eR )
{
	SISPtr  pSiS = SISPTR(pScrn);
	long dstbase;
#if 0
	float kL, kR;
#endif

	dstbase = 0;
	if (y >= 2048) {
		dstbase=pSiS->scrnOffset*y;
		y = 0;
	}
#ifdef SISDUALHEAD
	if(pSiS->VGAEngine != SIS_530_VGA) {
	   	dstbase += HEADOFFSET;
	}
#endif
	SiSSetupDSTBase(dstbase)
	/* SiSSetupRect(w,h) */

#if 1
	SiSSetupPATFG(0xff0000) /* FOR TESTING */
#endif

	/* Clear CommandReg because SetUp can be used for Rect and Trap */
	pSiS->CommandReg &= ~(T_L_X_INC | T_L_Y_INC |
	                      T_R_X_INC | T_R_Y_INC |
	                      T_XISMAJORL | T_XISMAJORR |
			      BITBLT);

        xf86DrvMsg(0, X_INFO, "Trap (%d %d %d %d) dxL %d dyL %d eL %d   dxR %d dyR %d eR %d\n",
		left, right, y, h, dxL, dyL, eL, dxR, dyR, eR);

	/* Unfortunately, we must check if the right and the left edge
	 * cross each other...  INCOMPLETE (line equation wrong)
	 */
#if 0
	if (dxL == 0) kL = 0;
	else kL = (float)dyL / (float)dxL;
	if (dxR == 0) kR = 0;
	else kR = (float)dyR / (float)dxR;
	xf86DrvMsg(0, X_INFO, "kL %f kR %f!\n", kL, kR);
	if ( (kR != kL) &&
	     (!(kR == 0 && kL == 0)) &&
	     (!(kR <  0 && kL >  0)) ) {
	   xf86DrvMsg(0, X_INFO, "Inside if (%f - %d)\n", ( kL * ( ( ((float)right - (float)left) / (kL - kR) ) - left) + y), h+y);
           if ( ( ( kL * ( ( ((float)right - (float)left) / (kL - kR) ) - (float)left) + (float)y) < (h + y) ) ) {
	     xf86DrvMsg(0, X_INFO, "Cross detected!\n");
	   }
	}
#endif

	/* Determine egde angles */
	if (dxL < 0) { dxL = -dxL; }
	else { SiSSetupCMDFlag(T_L_X_INC) }
	if (dxR < 0) { dxR = -dxR; }
	else { SiSSetupCMDFlag(T_R_X_INC) }

	/* (Y direction always positive - do this anyway) */
	if (dyL < 0) { dyL = -dyL; }
	else { SiSSetupCMDFlag(T_L_Y_INC) }
	if (dyR < 0) { dyR = -dyR; }
	else { SiSSetupCMDFlag(T_R_Y_INC) }

	/* Determine major axis */
	if (dxL >= dyL) {      /* X is major axis */
		SiSSetupCMDFlag(T_XISMAJORL)
	}
	if (dxR >= dyR) {      /* X is major axis */
		SiSSetupCMDFlag(T_XISMAJORR)
	}

	/* Set up deltas */
	SiSSetupdL(dxL, dyL)
	SiSSetupdR(dxR, dyR)

#if 0   /* Could it be that this crappy engine can only draw trapezoids up to 127 pixels high? */
	h &= 0x7F;
	if (h == 0) h = 10;
#endif

	/* Set up y, h, left, right */
	SiSSetupYH(y,h)
	SiSSetupLR(left,right)

	/* Set up initial error term */
	SiSSetupEL(eL)
	SiSSetupER(eR)

	SiSSetupCMDFlag(TRAPAZOID_FILL);

	SiSDoCMD
}
#endif

static void
SiSSetupForSolidLine(ScrnInfoPtr pScrn,
                        int color, int rop, unsigned int planemask)
{
	SISPtr  pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup SolidLine(0x%x, 0x%x, 0x%x)\n",
					color, rop, planemask));

	SiSSetupLineCount(1)
	SiSSetupPATFG(color)
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupROP(sisPatALUConv[rop])
	SiSSetupCMDFlag(PATFG | LINE)
}

static void
SiSSubsequentSolidTwoPointLine(ScrnInfoPtr pScrn,
                        int x1, int y1, int x2, int y2, int flags)
{
	SISPtr  pSiS = SISPTR(pScrn);
	long dstbase,miny,maxy;

	PDEBUG(ErrorF("Subsequent SolidLine(%d, %d, %d, %d, 0x%x)\n",
					x1, y1, x2, y2, flags));

	dstbase = 0;
	miny = (y1 > y2) ? y2 : y1;
	maxy = (y1 > y2) ? y1 : y2;
	if(maxy >= 2048) {
		dstbase = pSiS->scrnOffset * miny;
		y1 -= miny;
		y2 -= miny;
	}
#ifdef SISDUALHEAD
	if(pSiS->VGAEngine != SIS_530_VGA) {
	   dstbase += HEADOFFSET;
	}
#endif
	SiSSetupDSTBase(dstbase)

	SiSSetupX0Y0(x1,y1)
	SiSSetupX1Y1(x2,y2)
	if (flags & OMIT_LAST) {
		SiSSetupCMDFlag(NO_LAST_PIXEL)
	} else {
		pSiS->CommandReg &= ~(NO_LAST_PIXEL);
	}

	SiSDoCMD
}

static void
SiSSubsequentSolidHorzVertLine(ScrnInfoPtr pScrn,
                                int x, int y, int len, int dir)
{
	SISPtr  pSiS = SISPTR(pScrn);
	long dstbase;

	PDEBUG(ErrorF("Subsequent SolidHorzVertLine(%d, %d, %d, %d)\n",
					x, y, len, dir));
	len--; /* starting point is included! */

	dstbase = 0;
	if((y >= 2048) || ((y + len) >= 2048)) {
		dstbase = pSiS->scrnOffset * y;
		y = 0;
	}
#ifdef SISDUALHEAD
	if(pSiS->VGAEngine != SIS_530_VGA) {
	   	dstbase += HEADOFFSET;
	}
#endif
	SiSSetupDSTBase(dstbase)

	SiSSetupX0Y0(x,y)
	if (dir == DEGREES_0) {
		SiSSetupX1Y1(x + len, y);
	} else {
		SiSSetupX1Y1(x, y + len);
	}

	SiSDoCMD
}

static void
SiSSetupForDashedLine(ScrnInfoPtr pScrn,
                                int fg, int bg, int rop, unsigned int planemask,
                                int length, unsigned char *pattern)
{
	SISPtr pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup DashedLine(0x%x, 0x%x, 0x%x, 0x%x, %d, 0x%x:%x)\n",
			fg, bg, rop, planemask, length, *(pattern+4), *pattern));

	SiSSetupLineCount(1)
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupStyleLow(*pattern)
	SiSSetupStyleHigh(*(pattern+4))
	SiSSetupStylePeriod(length-1);			/* TW: This was missing!!! */
	SiSSetupROP(sisPatALUConv[rop])
	SiSSetupPATFG(fg)
	SiSSetupCMDFlag(LINE | LINE_STYLE)  		/* TW: This was missing!!! */
	if(bg != -1) {
		SiSSetupPATBG(bg)
	} else {
	        SiSSetupCMDFlag(TRANSPARENT); 		/* TW: This was missing!!! */
	}
}

static void
SiSSubsequentDashedTwoPointLine(ScrnInfoPtr pScrn,
                                int x1, int y1, int x2, int y2,
                                int flags, int phase)
{
	SISPtr pSiS = SISPTR(pScrn);
	long dstbase,miny,maxy;

	PDEBUG(ErrorF("Subsequent DashedLine(%d,%d, %d,%d, 0x%x,0x%x)\n",
			x1, y1, x2, y2, flags, phase));

	dstbase = 0;
	miny = (y1 > y2) ? y2 : y1;
	maxy = (y1 > y2) ? y1 : y2;
	if(maxy >= 2048) {
		dstbase = pSiS->scrnOffset * miny;
		y1 -= miny;
		y2 -= miny;
	}
#ifdef SISDUALHEAD
	if(pSiS->VGAEngine != SIS_530_VGA) {
	   dstbase += HEADOFFSET;
	}
#endif
	SiSSetupDSTBase(dstbase)

	SiSSetupX0Y0(x1,y1)
	SiSSetupX1Y1(x2,y2)
	if(flags & OMIT_LAST) {
		SiSSetupCMDFlag(NO_LAST_PIXEL)
	} else {
		pSiS->CommandReg &= ~(NO_LAST_PIXEL);
	}

	SiSDoCMD
}

static void
SiSSetupForMonoPatternFill(ScrnInfoPtr pScrn,
                                int patx, int paty, int fg, int bg,
                                int rop, unsigned int planemask)
{
	SISPtr pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup MonoPatFill(0x%x,0x%x, 0x%x,0x%x, 0x%x, 0x%x)\n",
					patx, paty, fg, bg, rop, planemask));
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupMONOPAT(patx,paty)
	SiSSetupPATFG(fg)
	SiSSetupROP(sisPatALUConv[rop])
	SiSSetupCMDFlag(PATMONO)
	SiSSetupPATBG(bg)
}

static void
SiSSubsequentMonoPatternFill(ScrnInfoPtr pScrn,
                                int patx, int paty,
                                int x, int y, int w, int h)
{
	SISPtr pSiS = SISPTR(pScrn);
	long dstbase;

	PDEBUG(ErrorF("Subsequent MonoPatFill(0x%x,0x%x, %d,%d, %d,%d)\n",
							patx, paty, x, y, w, h));
	dstbase = 0;

	if (y >= 2048) {
		dstbase = pSiS->scrnOffset * y;
		y = 0;
	}
#ifdef SISDUALHEAD
	if(pSiS->VGAEngine != SIS_530_VGA) {
	   dstbase += HEADOFFSET;
	}
#endif
	SiSSetupDSTBase(dstbase)
	SiSSetupDSTXY(x, y)
	SiSSetupRect(w, h)
	/* Clear commandReg because Setup can be used for Rect and Trap */
	pSiS->CommandReg &= ~(T_XISMAJORL | T_XISMAJORR |
	                      T_L_X_INC | T_L_Y_INC |
	                      T_R_X_INC | T_R_Y_INC |
	                      TRAPAZOID_FILL);
	SiSSetupCMDFlag(X_INC | Y_INC)

	SiSDoCMD
}

/* Trapezoid */
#ifdef TRAP
static void
SiSSubsequentMonoPatternFillTrap(ScrnInfoPtr pScrn,
               int patx, int paty,
               int y, int h,
	       int left, int dxL, int dyL, int eL,
	       int right, int dxR, int dyR, int eR )
{
	SISPtr  pSiS = SISPTR(pScrn);
	long dstbase;

	PDEBUG(ErrorF("Subsequent Mono8x8PatternFillTrap(%d, %d, %d - %d %d/%d %d/%d)\n",
					y, h, left, right, dxL, dxR, eL, eR));

	dstbase = 0;
	if (y >= 2048) {
		dstbase=pSiS->scrnOffset*y;
		y = 0;
	}
#ifdef SISDUALHEAD
	if(pSiS->VGAEngine != SIS_530_VGA) {
	   dstbase += HEADOFFSET;
	}
#endif
	SiSSetupDSTBase(dstbase)

	/* Clear CommandReg because SetUp can be used for Rect and Trap */
	pSiS->CommandReg &= ~(T_XISMAJORL | T_XISMAJORR |
	                      T_L_X_INC | T_L_Y_INC |
			      T_R_X_INC | T_R_Y_INC |
			      BITBLT);

	if (dxL < 0) { dxL = -dxL;  }
	else { SiSSetupCMDFlag(T_L_X_INC) }
	if (dxR < 0) { dxR = -dxR; }
	else { SiSSetupCMDFlag(T_R_X_INC) }

	if (dyL < 0) { dyL = -dyL; }
	else { SiSSetupCMDFlag(T_L_Y_INC) }
	if (dyR < 0) { dyR = -dyR; }
	else { SiSSetupCMDFlag(T_R_Y_INC) }

	/* Determine major axis */
	if (dxL >= dyL) {      /* X is major axis */
		SiSSetupCMDFlag(T_XISMAJORL)
	}
	if (dxR >= dyR) {      /* X is major axis */
		SiSSetupCMDFlag(T_XISMAJORR)
	}

	SiSSetupYH(y,h)
	SiSSetupLR(left,right)

	SiSSetupdL(dxL, dyL)
	SiSSetupdR(dxR, dyR)

	SiSSetupEL(eL)
	SiSSetupER(eR)

	SiSSetupCMDFlag(TRAPAZOID_FILL);

	SiSDoCMD
}
#endif


#if 0

/* TW: The following (already commented) functions have NOT been adapted for dual-head mode */


/* ------- Color Pattern Fill --- is not useful for XAA -------------- */

static void
SiSSetupForColorPatternFill(ScrnInfoPtr pScrn,
                                int patx, int paty, int rop,
                                unsigned int planemask,
                                int trans_color)
{
	SISPtr pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup ColorPatFill(0x%x,0x%x, 0x%x,0x%x, 0x%x)\n",
					patx, paty, rop, planemask, trans_color));

/*      SiSSetupDSTRect(pSiS->scrnOffset, pScrn->virtualY)*/
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupROP(sisPatALUConv[rop])
	SiSSetupCMDFlag(PATPATREG | X_INC | Y_INC)
}

static void
SiSSubsequentColorPatternFill(ScrnInfoPtr pScrn,
                                int patx, int paty,
                                int x, int y, int w, int h)
{
	SISPtr pSiS = SISPTR(pScrn);
	long dstbase;

	PDEBUG(ErrorF("Subsequent ColorPatFill(0x%x,0x%x, %d,%d, %d,%d)\n",
							patx, paty, x, y, w, h));

	dstbase = 0;
	if (y >= 2048) {
		dstbase = pSiS->scrnOffset * y;
		y = 0;
	}
	SiSSetupDSTBase(dstbase)
	SiSSetupDSTXY(x,y)
	SiSSetupRect(w,h)
	SiSDoCMD
}

/* ----- CPU To Screen Color Expand (single task) ------------------------- */

/* This does not work. Assumingly for the same
 * reason why STSColorExpand does not work either.
 */
static void
SiSSetupForCPUToScreenColorExpand(ScrnInfoPtr pScrn,
                                int fg, int bg,
                                int rop, unsigned int planemask)
{
	SISPtr pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup CPUToScreen ColorExpand(0x%x,0x%x, 0x%x,0x%x)\n",
					fg, bg, rop, planemask));

/*      SiSSetupDSTRect(pSiS->scrnOffset, pScrn->virtualY)*/
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupDSTColorDepth(pSiS->DstColor);
	SiSSetupSRCXY(0,0)
	SiSSetupSRCFG(fg)
	SiSSetupROP(sisPatALUConv[rop])
	SiSSetupCMDFlag(X_INC | Y_INC | COLOREXP)
	if (bg == -1) {
		SiSSetupCMDFlag(TRANSPARENT)
	}
	else {
		SiSSetupSRCBG(bg)
	}
}

static void
SiSSubsequentCPUToScreenColorExpand(ScrnInfoPtr pScrn,
                                int x, int y, int w, int h, int skipleft)
{
	SISPtr pSiS = SISPTR(pScrn);
	long dstbase;

	PDEBUG(ErrorF("Subsequent CPUToScreen ColorExpand(%d,%d, %d,%d, %d)\n",
							x, y, w, h, skipleft));

	dstbase = 0;
	if (y >= 2048) {
			dstbase = pSiS->scrnOffset * y;
			y = 0;
	}
	SiSSetupDSTBase(dstbase)

/*      SiSSetupSRCPitch(((w+31)&0xFFE0)/8)*/
	SiSSetupSRCPitch((w+7)/8)
	SiSSetupDSTXY(x,y)
	SiSSetupRect(w,h)
/*      SiSDoCMD*/
	pSiS->DoColorExpand = TRUE;
}
#endif

/* ------ Screen To Screen Color Expand ------------------------------- */

/* TW: The hareware does not seem to support this the way we need it */

#ifdef STSCE
static void
SiSSetupForScreenToScreenColorExpand(ScrnInfoPtr pScrn,
                                int fg, int bg,
                                int rop, unsigned int planemask)
{
	SISPtr          pSiS = SISPTR(pScrn);

	PDEBUG(ErrorF("Setup ScreenToScreen ColorExp(0x%x,0x%x, 0x%x)\n",
							fg, bg, rop));

	SiSSetupDSTColorDepth(pSiS->DstColor)
	SiSSetupDSTRect(pSiS->scrnOffset, -1)
	SiSSetupROP(sisALUConv[rop])
	SiSSetupSRCFG(fg)
	/* SiSSetupSRCXY(0,0) */

	if(bg == -1) {
		SiSSetupCMDFlag(TRANSPARENT | ENCOLOREXP | X_INC |
				Y_INC | SRCVIDEO);
	} else {
		SiSSetupSRCBG(bg);
		SiSSetupCMDFlag(ENCOLOREXP | X_INC | Y_INC |
				SRCVIDEO);
	};
}
#endif

/* TW. This method blits in a single task; this does not seem to work
 * because the hardware does not use the source pitch as scanline
 * offset but only to calculate pattern address from source X and Y.
 * XAA provides the pattern bitmap with scrnOffset (displayWidth * bpp/8)
 * offset, but this does not seem to be supported by the hardware.
 */
#ifdef STSCE

/* For testing, these are the methods: (use only one at a time!) */

#undef npitch 		/* Normal: Use srcx/y as srcx/y, use scrnOffset as source pitch
			 * This would work if the hareware used the source pitch for
			 * incrementing the source address after each scanline - but
			 * it doesn't do this! The first line of the area is correctly
			 * color expanded, but since the source pitch is ignored and
			 * the source address not incremented correctly, the following
			 * lines are color expanded with any bit pattern that is left
			 * in the unused space of the source bitmap (which is organized
			 * with the depth of the screen framebuffer hence with a pitch
			 * of scrnOffset).
			 */

#undef pitchdw    	/* Use source pitch "displayWidth / 8" instead
		   	 * of scrnOffset (=displayWidth * bpp / 8)
			 * This can't work, because the pitch of the source
			 * bitmap is scrnoffset!
		   	 */

#define  nopitch   	/* Calculate srcbase with srcx and srcy, set the
		   	 * pitch to scrnOffset (which IS the correct pitch
		   	 * for the source bitmap) and set srcx and srcy both
		   	 * to 0.
			 * This would work if the hareware used the source pitch for
			 * incrementing the source address after each scanline - but
			 * it doesn't do this! Again: The first line of the area is
			 * correctly color expanded, but since the source pitch is
			 * ignored for scanline address incremention, the following
			 * lines are not correctly color expanded.
			 * WHATEVER I write to source pitch is ignored!
		   	 */

static void
SiSSubsequentScreenToScreenColorExpand(ScrnInfoPtr pScrn,
                                int x, int y, int w, int h,
                                int srcx, int srcy, int skipleft)
{
	SISPtr pSiS = SISPTR(pScrn);
        long srcbase, dstbase;
#if 0
	int _x0, _y0, _x1, _y1;
#endif
#ifdef pitchdw
	int newsrcx, newsrcy;

	/* srcx and srcy are provided based on a scrnOffset pitch ( = displayWidth * bpp / 8 )
	 * We recalulate srcx and srcy based on pitch = displayWidth / 8
	 */
        newsrcy = ((pSiS->scrnOffset * srcy) + (srcx * ((pScrn->bitsPerPixel+7)/8))) /
					  (pScrn->displayWidth/8);
        newsrcx = ((pSiS->scrnOffset * srcy) + (srcx * ((pScrn->bitsPerPixel+7)/8))) %
					  (pScrn->displayWidth/8);
#endif
	xf86DrvMsg(0, X_INFO, "Sub ScreenToScreen ColorExp(%d,%d, %d,%d, %d,%d, %d)\n",
					x, y, w, h, srcx, srcy, skipleft);

	srcbase = dstbase = 0;

#ifdef pitchdw
	if (newsrcy >= 2048) {
		srcbase = (pScrn->displayWidth / 8) * newsrcy;
		newsrcy = 0;
	}
#endif
#ifdef nopitch
	srcbase = (pSiS->scrnOffset * srcy) + (srcx * ((pScrn->bitsPerPixel+7)/8));
#endif
#ifdef npitch
	if (srcy >= 2048) {
		srcbase = pSiS->scrnOffset * srcy;
		srcy = 0;
	}
#endif
	if (y >= 2048) {
		dstbase = pSiS->scrnOffset * y;
		y = 0;
	}
#ifdef SISDUALHEAD
	if(pSiS->VGAEngine != SIS_530_VGA) {
	   srcbase += HEADOFFSET;
	   dstbase += HEADOFFSET;
	}
#endif
	SiSSetupSRCBase(srcbase)
	SiSSetupDSTBase(dstbase)

#ifdef pitchdw
	SiSSetupSRCPitch(pScrn->displayWidth/8)
#endif
#ifdef nopitch
	SiSSetupSRCPitch(pSiS->scrnOffset)
	/* SiSSetupSRCPitch(100) */ /* For test - has NO effect WHATSOEVER */
#endif
#ifdef npitch
	SiSSetupSRCPitch(pSiS->scrnOffset)
#endif

	SiSSetupRect(w,h)

#if 0   /* How do I implement the offset? Not this way, that's for sure.. */
	if (skipleft > 0) {
		_x0 = x+skipleft;
		_y0 = y;
		_x1 = x+w;
		_y1 = y+h;
		SiSSetupClipLT(_x0, _y0);
		SiSSetupClipRB(_x1, _y1);
		SiSSetupCMDFlag(CLIPENABLE);
	}
#endif
#ifdef pitchdw
	SiSSetupSRCXY(newsrcx, newsrcy)
#endif
#ifdef nopitch
	SiSSetupSRCXY(0,0)
#endif
#ifdef npitch
	SiSSetupSRCXY(srcx, srcy)
#endif

	SiSSetupDSTXY(x,y)

	SiSDoCMD
}
#endif

/* TW: TEST: Do it scanline-wise because the other way does not seem to
 *     be supported by the hardware. (The source pitch seems to be
 *     displayWidth * (bbp/8) as opposed by the XAA HOWTO, where
 *     it is stated that the pitch would be displayWidth pixels;
 *     besides, the hardware seems to ignore the source pitch
 *     for address increments.)
 *     Apart from this (which can be solved by doing the color
 *     expand scanline-wise), I don't know how to implement the
 *     offset argument. The current method (which uses hardware
 *     clipping) does not work.
 *
 *     THIS DOES NOT WORK IN THE CURRENT STATE.
 */
#if 0
static void
SiSSubsequentScreenToScreenColorExpand(ScrnInfoPtr pScrn,
                                int x, int y, int w, int h,
                                int srcx, int srcy, int offset)
{
	SISPtr pSiS = SISPTR(pScrn);
        long srcbase, dstbase;
	int _x0, _y0, _x1, _y1;

	int newsrcx, newsrcy;

        newsrcy = ((pSiS->scrnOffset * srcy) + (srcx * ((pScrn->bitsPerPixel+7)/8))) /
					  (((((w+7)/8)+3) >> 2) * 4);
					  /* (pScrn->displayWidth/8); */
        newsrcx = ((pSiS->scrnOffset * srcy) + (srcx * ((pScrn->bitsPerPixel+7)/8))) %
					  (((((w+7)/8)+3) >> 2) * 4);
					  /* (pScrn->displayWidth/8); */

	xf86DrvMsg(0, X_INFO, "Sub STS CE(%d,%d, %d,%d, %d,%d, %d)\n",
					x, y, w, h, srcx, srcy, skipleft);

	srcbase = dstbase = 0;
	if (newsrcy >= 2048) {
		srcbase = (((((w+7)/8)+3) >> 2) * 4) * newsrcy;
		          /* (pScrn->displayWidth/8) * newsrcy;      */
		          /* pSiS->scrnOffset * srcy;                */
		newsrcy = 0;
	}
	if (y >= 2048) {
		dstbase = pSiS->scrnOffset * y;
		y = 0;
	}
#ifdef SISDUALHEAD
	srcbase += HEADOFFSET;
	dstbase += HEADOFFSET;
#endif
	SiSSetupDSTBase(dstbase)

	SiSSetupRect(w, 1)

	SiSSetupSRCXY(newsrcx, newsrcy)

	/* SiSSetupSRCPitch(pScrn->displayWidth/8) */ /* old: (((w+31)&0xFFE0)/8) */
	SiSSetupSRCPitch(((((w+7)/8)+3) >> 2) * 4)
#if 1
        if (offset > 0) {
	    SiSSetupCMDFlag(CLIPENABLE)
        } else
            pSiS->CommandReg &= ~CLIPENABLE;
#endif

	while (h) {

	   SiSSetupSRCBase(srcbase)
#if 1
	   if (offset > 0) {
		_x0 = x+skipleft;
		_y0 = y;
		_x1 = x+w;
		_y1 = y+h;
		SiSSetupClipLT(_x0, _y0);
		SiSSetupClipRB(_x1, _y1);
	   }
#endif
	   SiSSetupDSTXY(x,y)

	   SiSDoCMD

	   srcbase += ((((w+7)/8)+3) >> 2) * 4 * 8* ((pScrn->bitsPerPixel+7)/8);
	   	      /* pSiS->scrnOffset;  */
	   y++;
	   h--;
	}
}
#endif


/* ----- CPU To Screen Color Expand (scanline-wise) ----------------- */

/* We do it using the indirect method */

static void
SiSSetupForScanlineCPUToScreenColorExpandFill(ScrnInfoPtr pScrn,
		int fg, int bg, int rop, unsigned int planemask)
{
	SISPtr pSiS=SISPTR(pScrn);

        /* TW: Make sure that current CPU-driven BitBlt buffer stage is 0
	 *     This is required!!! (Otherwise -> drawing errors)
	 */
	while((MMIO_IN16(pSiS->IOBase, 0x8242) & 0x1F00) != 0) {} /* WDR: == 0x10 */

#if 0   /* TW: This is obviously not needed */
	pSiS->ColorExpandRingHead = 0;
	pSiS->ColorExpandRingTail = pSiS->ColorExpandBufferNumber - 1;
#endif

	SiSSetupSRCXY(0,0);
	SiSSetupROP(sisALUConv[rop]);
	SiSSetupSRCFG(fg);
	SiSSetupDSTRect(pSiS->scrnOffset, -1);
	SiSSetupDSTColorDepth(pSiS->DstColor);
	if(bg == -1) {
		SiSSetupCMDFlag(TRANSPARENT |
		                ENCOLOREXP |
				X_INC | Y_INC |
				SRCCPUBLITBUF);
	} else {
		SiSSetupSRCBG(bg);
		SiSSetupCMDFlag(ENCOLOREXP |
		                X_INC | Y_INC |
				SRCCPUBLITBUF);
	}
}


static void
SiSSubsequentScanlineCPUToScreenColorExpandFill(
                        ScrnInfoPtr pScrn, int x, int y, int w,
                        int h, int skipleft)
{
	SISPtr pSiS = SISPTR(pScrn);
	int _x0, _y0, _x1, _y1;
	long dstbase;

	dstbase = 0;
	if (y >= 2048) {
		dstbase = pSiS->scrnOffset * y;
		y = 0;
	}
#ifdef SISDUALHEAD
	if(pSiS->VGAEngine != SIS_530_VGA) {
	   	dstbase += HEADOFFSET;
	}
#endif

	/* TW: Wait until there is no color expansion command in queue
	 *     (This solves the OpenOffice.org window-move bug)
	 *     Added Idle-check - bit 23 is set sometimes, although
	 *     engine is actually idle!
	 *     Update: Bit 23 is not reliable. After heavy 3D engine
	 *     action, this bit never gets cleared again. So do
	 *     SiSIdle instead.
	 */
	if((MMIO_IN16(pSiS->IOBase, 0x8242) & 0xe000) != 0xe000) {
           /* while ((MMIO_IN16(pSiS->IOBase, 0x8242) & 0x0080) != 0) {} */
	   SiSIdle
	}

	SiSSetupDSTBase(dstbase)

	if (skipleft > 0) {
		_x0 = x + skipleft;
		_y0 = y;
		_x1 = x + w;
		_y1 = y + h;
		SiSSetupClipLT(_x0, _y0);
		SiSSetupClipRB(_x1, _y1);
		SiSSetupCMDFlag(CLIPENABLE);
	} else {
		pSiS->CommandReg &= (~CLIPENABLE);
	}

	SiSSetupRect(w, 1);
        SiSSetupSRCPitch(((((w+7)/8)+3) >> 2) * 4);
	pSiS->xcurrent = x;
	pSiS->ycurrent = y;
}

static void
SiSSubsequentColorExpandScanline(ScrnInfoPtr pScrn, int bufno)
{
	SISPtr pSiS=SISPTR(pScrn);
#if 0
	int newhead,bltbufstage,newtail;
#endif
	long cbo;

	cbo = pSiS->ColorExpandBufferScreenOffset[bufno];
#ifdef SISDUALHEAD
	if(pSiS->VGAEngine != SIS_530_VGA) {
	   cbo += HEADOFFSET;
	}
#endif

	/* TW: Wait until there is no color expansion command in queue
	 *     (This solves the GTK-big-font bug)
	 *     Added Idle-check - bit 23 is set sometimes, although
	 *     engine is actually idle!
	 *     Update: Bit 23 is not reliable. After heavy 3D engine
	 *     action, this bit never gets cleared again. So do
	 *     SiSIdle instead.
	 */
	if((MMIO_IN16(pSiS->IOBase, 0x8242) & 0xe000) != 0xe000) {
	  /* while ((MMIO_IN16(pSiS->IOBase, 0x8242) & 0x0080) != 0) {} */
	    SiSIdle
	}

	SiSSetupSRCBase(cbo);

	SiSSetupDSTXY(pSiS->xcurrent, pSiS->ycurrent);

	SiSDoCMD

	pSiS->ycurrent++;

	if(pSiS->VGAEngine == SIS_530_VGA) {
	   while(MMIO_IN8(pSiS->IOBase, 0x8242) & 0x80) {}
	}

#if 0	/* TW: What is this good for? The Head/Tail data is never ever used elsewhere! */
	pSiS->ColorExpandRingHead = newhead =
			(bufno + 1) & pSiS->ColorExpandBufferCountMask;
	while (newhead == pSiS->ColorExpandRingTail) {
		bltbufstage = (int)((MMIO_IN16(pSiS->IOBase,0x8242) & 0x1F00) >> 8);
		newtail = newhead - (bltbufstage + 1);
		pSiS->ColorExpandRingTail = (newtail >= 0) ?
				newtail : (pSiS->ColorExpandBufferNumber + newtail);
	}
#endif
}

