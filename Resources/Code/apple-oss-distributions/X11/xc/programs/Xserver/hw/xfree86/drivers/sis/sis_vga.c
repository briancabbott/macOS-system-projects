/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/sis/sis_vga.c,v 1.16 2003/01/29 15:42:17 eich Exp $ */
/*
 * Mode setup and video bridge detection
 *
 * Copyright 1998,1999 by Alan Hourihane, Wigan, England.
 * Parts Copyright 2001, 2002 by Thomas Winischhofer, Vienna, Austria.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of the copyright holder not be used in
 * advertising or publicity pertaining to distribution of the software without
 * specific, written prior permission.  The copyright holder makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 *
 * Authors:  Alan Hourihane, alanh@fairlite.demon.co.uk
 *           Mike Chapman <mike@paranoia.com>, 
 *           Juanjo Santamarta <santamarta@ctv.es>,
 *           Mitani Hiroshi <hmitani@drl.mei.co.jp> 
 *           David Thomas <davtom@dream.org.uk>.
 *	     Thomas Winischhofer <thomas@winischhofer.net>
 */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86_ansic.h"
#include "xf86Version.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"

#include "sis.h"
#include "sis_regs.h"
#include "sis_dac.h"

#define Midx         0
#define Nidx         1
#define VLDidx       2
#define Pidx         3
#define PSNidx       4
#define Fref         14318180
/* stability constraints for internal VCO -- MAX_VCO also determines
 * the maximum Video pixel clock */
#define MIN_VCO      Fref
#define MAX_VCO      135000000
#define MAX_VCO_5597 353000000
#define MAX_PSN      0         /* no pre scaler for this chip */
#define TOLERANCE    0.01      /* search smallest M and N in this tolerance */


static Bool  SISInit(ScrnInfoPtr pScrn, DisplayModePtr mode);
static Bool  SIS300Init(ScrnInfoPtr pScrn, DisplayModePtr mode);
/* TW: To be used internally only */
int    SISDoSense(ScrnInfoPtr pScrn, int tempbl, int tempbh, int tempcl, int tempch);
void   SISSense30x(ScrnInfoPtr pScrn);
int    SIS6326DoSense(ScrnInfoPtr pScrn, int tempbh, int tempbl, int tempch, int tempcl);
void   SISSense6326(ScrnInfoPtr pScrn);
static void SiS6326TVDelay(ScrnInfoPtr pScrn, int delay);

const CARD8 SiS6326TVRegs1_NTSC[6][14] = {
    {0x81,0x3f,0x49,0x1b,0xa9,0x03,0x00,0x09,0x08,0x7d,0x00,0x88,0x30,0x60},
    {0x81,0x3f,0x49,0x1d,0xa0,0x03,0x00,0x09,0x08,0x7d,0x00,0x88,0x30,0x60},
    {0x81,0x45,0x24,0x8e,0x26,0x0b,0x00,0x09,0x02,0xfe,0x00,0x09,0x51,0x60},
    {0x81,0x45,0x24,0x8e,0x26,0x07,0x00,0x29,0x04,0x30,0x10,0x3b,0x61,0x60},
    {0x81,0x3f,0x24,0x8e,0x26,0x09,0x00,0x09,0x02,0x30,0x10,0x3b,0x51,0x60},
    {0x83,0x5d,0x21,0xbe,0x75,0x03,0x00,0x09,0x08,0x42,0x10,0x4d,0x61,0x79}   /* 640x480u */
};

const CARD8 SiS6326TVRegs2_NTSC[6][54] = {
    {0x11, 0x17, 0x03, 0x09, 0x94, 0x02, 0x05, 0x06, 0x09, 0x50, 0x0C,
     0x0C, 0x06, 0x0D, 0x04, 0x0A, 0x94, 0x06, 0x0D, 0x04, 0x0A, 0x94,
     0xFC, 0xDF, 0x94, 0x1F, 0x4A, 0x03, 0x71, 0x07, 0x97, 0x10, 0x40,
     0x48, 0x00, 0x26, 0xB6, 0x10, 0x5C, 0xEC, 0x21, 0x2E, 0xBE, 0x10,
     0x64, 0xF4, 0x21, 0x13, 0x75, 0x08, 0x31, 0x6A, 0x01, 0xA0},
    {0x11, 0x17, 0x03, 0x0A, 0x94, 0x02, 0x05, 0x06, 0x09, 0x50, 0x0C,
     0x0D, 0x06, 0x0D, 0x04, 0x0A, 0x94, 0x06, 0x0D, 0x04, 0x0A, 0x94,
     0xFF, 0xDF, 0x94, 0x1F, 0x4A, 0x03, 0x71, 0x07, 0x97, 0x10, 0x40,
     0x48, 0x00, 0x26, 0xB6, 0x10, 0x5C, 0xEC, 0x21, 0x2E, 0xBE, 0x10,
     0x64, 0xF4, 0x21, 0x13, 0x75, 0x08, 0x31, 0x6A, 0x01, 0xA0},
    {0x11, 0x17, 0x03, 0x0A, 0x94, 0x02, 0x05, 0x06, 0x09, 0x50, 0x0C,
     0x0D, 0x06, 0x0D, 0x04, 0x0A, 0x94, 0x06, 0x0D, 0x04, 0x0A, 0x94,
     0xFF, 0xDF, 0x94, 0x3F, 0x8C, 0x06, 0xCE, 0x07, 0x27, 0x30, 0x73,
     0x7B, 0x00, 0x48, 0x68, 0x30, 0xB2, 0xD2, 0x52, 0x50, 0x70, 0x30,
     0xBA, 0xDA, 0x52, 0xDC, 0x02, 0xD1, 0x53, 0xF7, 0x02, 0xA0},
    {0x11, 0x17, 0x03, 0x09, 0x94, 0x02, 0x05, 0x06, 0x09, 0x50, 0x0C,
     0x0C, 0x06, 0x0D, 0x04, 0x0A, 0x94, 0x06, 0x0D, 0x04, 0x0A, 0x94,
     0xDC, 0xDF, 0x94, 0x3F, 0x8C, 0x06, 0xCE, 0x07, 0x27, 0x30, 0x73,
     0x7B, 0x00, 0x48, 0x68, 0x30, 0xB2, 0xD2, 0x52, 0x50, 0x70, 0x30,
     0xBA, 0xDA, 0x52, 0x00, 0x02, 0xF5, 0x53, 0xF7, 0x02, 0xA0},
    {0x11, 0x17, 0x03, 0x09, 0x94, 0x02, 0x05, 0x06, 0x09, 0x50, 0x0C,
     0x0C, 0x06, 0x0D, 0x04, 0x0A, 0x94, 0x06, 0x0D, 0x04, 0x0A, 0x94,
     0xDC, 0xDF, 0x94, 0x3F, 0x8C, 0x06, 0xCE, 0x07, 0x27, 0x30, 0x73,
     0x7B, 0x00, 0x48, 0x68, 0x30, 0xB2, 0xD2, 0x52, 0x50, 0x70, 0x30,
     0xBA, 0xDA, 0x52, 0xDC, 0x02, 0xD1, 0x53, 0xF7, 0x02, 0xA0},
    {0x11, 0x17, 0x03, 0x09, 0x94, 0x02, 0x05, 0x06, 0x09, 0x50, 0x0C,  /* 640x480u */
     0x0C, 0x06, 0x0D, 0x04, 0x0A, 0x94, 0x06, 0x0D, 0x04, 0x0A, 0x94,
     0xDC, 0xDF, 0x94, 0xAF, 0x95, 0x06, 0xDD, 0x07, 0x5F, 0x30, 0x7E,
     0x86, 0x00, 0x4C, 0xA4, 0x30, 0xE3, 0x3B, 0x62, 0x54, 0xAC, 0x30,
     0xEB, 0x43, 0x62, 0x48, 0x34, 0x3D, 0x63, 0x29, 0x03, 0xA0}
};

const CARD8 SiS6326TVRegs1_PAL[6][14] = {
    {0x81,0x2d,0xc8,0x07,0xb2,0x0b,0x00,0x09,0x02,0xed,0x00,0xf8,0x30,0x40},
    {0x80,0x2d,0xa4,0x03,0xd9,0x0b,0x00,0x09,0x02,0xed,0x10,0xf8,0x71,0x40},
    {0x81,0x2d,0xa4,0x03,0xd9,0x0b,0x00,0x09,0x02,0xed,0x10,0xf8,0x71,0x40},
    {0x81,0x2d,0xa4,0x03,0xd9,0x0b,0x00,0x09,0x02,0x8f,0x10,0x9a,0x71,0x40},
    {0x83,0x63,0xa1,0x7a,0xa3,0x0a,0x00,0x09,0x02,0xb5,0x11,0xc0,0x81,0x59},  /* 800x600u */
    {0x81,0x63,0xa4,0x03,0xd9,0x01,0x00,0x09,0x10,0x9f,0x10,0xaa,0x71,0x59}   /* 720x540  */
};

const CARD8 SiS6326TVRegs2_PAL[6][54] = {
    {0x15, 0x4E, 0x35, 0x6E, 0x94, 0x02, 0x04, 0x38, 0x3A, 0x50, 0x3D,
     0x70, 0x06, 0x3E, 0x35, 0x6D, 0x94, 0x05, 0x3F, 0x36, 0x6E, 0x94,
     0xE5, 0xDF, 0x94, 0xEF, 0x5A, 0x03, 0x7F, 0x07, 0xFF, 0x10, 0x4E,
     0x56, 0x00, 0x2B, 0x23, 0x20, 0xB4, 0xAC, 0x31, 0x33, 0x2B, 0x20,
     0xBC, 0xB4, 0x31, 0x83, 0xE1, 0x78, 0x31, 0xD6, 0x01, 0xA0},
    {0x15, 0x4E, 0x35, 0x6E, 0x94, 0x02, 0x04, 0x38, 0x3A, 0x50, 0x3D,
     0x70, 0x06, 0x3E, 0x35, 0x6D, 0x94, 0x05, 0x3F, 0x36, 0x6E, 0x94,
     0xE5, 0xDF, 0x94, 0xDF, 0xB2, 0x07, 0xFB, 0x07, 0xF7, 0x30, 0x90,
     0x98, 0x00, 0x4F, 0x3F, 0x40, 0x62, 0x52, 0x73, 0x57, 0x47, 0x40,
     0x6A, 0x5A, 0x73, 0x03, 0xC1, 0xF8, 0x63, 0xB6, 0x03, 0xA0},
    {0x15, 0x4E, 0x35, 0x6E, 0x94, 0x02, 0x04, 0x38, 0x3A, 0x50, 0x3D,
     0x70, 0x06, 0x3E, 0x35, 0x6D, 0x94, 0x05, 0x3F, 0x36, 0x6E, 0x94,
     0xE5, 0xDF, 0x94, 0xDF, 0xB2, 0x07, 0xFB, 0x07, 0xF7, 0x30, 0x90,
     0x98, 0x00, 0x4F, 0x3F, 0x40, 0x62, 0x52, 0x73, 0x57, 0x47, 0x40,
     0x6A, 0x5A, 0x73, 0x03, 0xC1, 0xF8, 0x63, 0xB6, 0x03, 0xA0},
    {0x15, 0x4E, 0x35, 0x6E, 0x94, 0x02, 0x04, 0x38, 0x3A, 0x50, 0x3D,
     0x70, 0x06, 0x3E, 0x35, 0x6D, 0x94, 0x05, 0x3F, 0x36, 0x6E, 0x94,
     0xE5, 0xDF, 0x94, 0xDF, 0xB2, 0x07, 0xFB, 0x07, 0xF7, 0x30, 0x90,
     0x98, 0x00, 0x4F, 0x3F, 0x40, 0x62, 0x52, 0x73, 0x57, 0x47, 0x40,
     0x6A, 0x5A, 0x73, 0xA0, 0xC1, 0x95, 0x73, 0xB6, 0x03, 0xA0},
    {0x15, 0x4E, 0x35, 0x6E, 0x94, 0x02, 0x04, 0x38, 0x3A, 0x50, 0x3D,  /* 800x600u */
     0x70, 0x06, 0x3E, 0x35, 0x6D, 0x94, 0x05, 0x3F, 0x36, 0x6E, 0x94,
     0xE5, 0xDF, 0x94, 0x7F, 0xBD, 0x08, 0x0E, 0x07, 0x47, 0x40, 0x9D,
     0xA5, 0x00, 0x54, 0x94, 0x40, 0xA4, 0xE4, 0x73, 0x5C, 0x9C, 0x40,
     0xAC, 0xEC, 0x73, 0x0B, 0x0E, 0x00, 0x84, 0x03, 0x04, 0xA0},
    {0x15, 0x4E, 0x35, 0x6E, 0x94, 0x02, 0x04, 0x38, 0x3A, 0x50, 0x3D,  /* 720x540  */
     0x70, 0x06, 0x3E, 0x35, 0x6D, 0x94, 0x05, 0x3F, 0x36, 0x6E, 0x94,
     0xE5, 0xDF, 0x94, 0xDF, 0xB0, 0x07, 0xFB, 0x07, 0xF7, 0x30, 0x9D,
     0xA5, 0x00, 0x4F, 0x3F, 0x40, 0x62, 0x52, 0x73, 0x57, 0x47, 0x40,
     0x6A, 0x5A, 0x73, 0xA0, 0xC1, 0x95, 0x73, 0xB6, 0x03, 0xA0}
};

const CARD8 SiS6326TVRegs1[14] = {
     0x00,0x01,0x02,0x03,0x04,0x11,0x12,0x13,0x21,0x26,0x27,0x3a,0x3c,0x43
};

const CARD8 SiS6326CR[9][15] = {
     {0x79,0x63,0x64,0x1d,0x6a,0x93,0x00,0x6f,0xf0,0x58,0x8a,0x57,0x57,0x70,0x20},  /* PAL 800x600   */
     {0x79,0x4f,0x50,0x95,0x60,0x93,0x00,0x6f,0xba,0x14,0x86,0xdf,0xe0,0x30,0x00},  /* PAL 640x480   */
     {0x5f,0x4f,0x50,0x82,0x53,0x9f,0x00,0x0b,0x3e,0xe9,0x8b,0xdf,0xe7,0x04,0x00},  /* NTSC 640x480  */
     {0x5f,0x4f,0x50,0x82,0x53,0x9f,0x00,0x0b,0x3e,0xcb,0x8d,0x8f,0x96,0xe9,0x00},  /* NTSC 640x400  */
     {0x83,0x63,0x64,0x1f,0x6d,0x9b,0x00,0x6f,0xf0,0x48,0x0a,0x23,0x57,0x70,0x20},  /* PAL 800x600u  */
     {0x79,0x59,0x5b,0x1d,0x66,0x93,0x00,0x6f,0xf0,0x42,0x04,0x1b,0x40,0x70,0x20},  /* PAL 720x540   */
     {0x66,0x4f,0x51,0x0a,0x57,0x89,0x00,0x0b,0x3e,0xd9,0x0b,0xb6,0xe7,0x04,0x00},  /* NTSC 640x480u */
     {0xce,0x9f,0x9f,0x92,0xa4,0x16,0x00,0x28,0x5a,0x00,0x04,0xff,0xff,0x29,0x39},  /* 1280x1024-75  */
     {0x09,0xc7,0xc7,0x0d,0xd2,0x0a,0x01,0xe0,0x10,0xb0,0x04,0xaf,0xaf,0xe1,0x1f}   /* 1600x1200-60  */
};

/* Initialize a display mode on 5597/5598, 6326 and 530/620 */
static Bool
SISInit(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    SISPtr         pSiS = SISPTR(pScrn);
    SISRegPtr      pReg = &pSiS->ModeReg;
    vgaRegPtr      vgaReg = &VGAHWPTR(pScrn)->ModeReg;
    unsigned char  temp;
    int            mclk = pSiS->MemClock;
    int            offset;
    int            clock = mode->Clock;
    int            width = mode->HDisplay;
    int            height = mode->VDisplay;
    int            rate = SiSCalcVRate(mode);
    int            buswidth = pSiS->BusWidth;
    unsigned int   vclk[5];
    unsigned short CRT_CPUthresholdLow;
    unsigned short CRT_CPUthresholdHigh;
    unsigned short CRT_ENGthreshold;
    double         a, b, c;
    int            d, factor;
    int            num, denum, div, sbit, scale;
    BOOL	   sis6326tvmode, sis6326himode;

    PDEBUG(xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 3, "SISInit()\n"));

    /* Save the registers for further processing */
    (*pSiS->SiSSave)(pScrn, pReg);

    /* TW: Determine if chosen mode is suitable for TV on the 6326
           and if the mode is one of our special hi-res modes.
     */
    sis6326tvmode = FALSE;
    sis6326himode = FALSE;
    if(pSiS->Chipset == PCI_CHIP_SIS6326) {
      if(pSiS->SiS6326Flags & SIS6326_HASTV) {
        if((pSiS->SiS6326Flags & SIS6326_TVDETECTED) &&
           ((strcmp(mode->name, "PAL800x600") == 0)   ||	/* TW: Special TV modes */
            (strcmp(mode->name, "PAL800x600U") == 0)  ||
	    (strcmp(mode->name, "PAL720x540") == 0)   ||
            (strcmp(mode->name, "PAL640x480") == 0)   ||
	    (strcmp(mode->name, "NTSC640x480") == 0)  ||
	    (strcmp(mode->name, "NTSC640x480U") == 0) ||
	    (strcmp(mode->name, "NTSC640x400") == 0))) {
		    sis6326tvmode = TRUE;
        } else {
          pReg->sis6326tv[0x00] &= 0xfb;
        }
      }
      if((strcmp(mode->name, "SIS1280x1024-75") == 0) ||	/* TW: Special high-res modes */
         (strcmp(mode->name, "SIS1600x1200-60") == 0)) {
	 sis6326himode = TRUE;
      }
    }

#ifdef UNLOCK_ALWAYS
    outSISIDXREG(SISSR, 0x05, 0x86);
#endif

    pReg->sisRegs3C4[0x06] &= 0x01;

    /* set interlace */
    if(!(mode->Flags & V_INTERLACE))  {
        offset = pSiS->CurrentLayout.displayWidth >> 3;
    } else  {
        offset = pSiS->CurrentLayout.displayWidth >> 2;
        pReg->sisRegs3C4[0x06] |= 0x20;
    }

    /* Enable Linear and Enhanced Gfx Mode */
    pReg->sisRegs3C4[0x06] |= 0x82;

    /* Enable MMIO at PCI Register 14H (D[6:5]: 11) */
    pReg->sisRegs3C4[0x0B] |= 0x60;

    /* Enable 32bit mem access (D7), read-ahead cache (D4) */
    pReg->sisRegs3C4[0x0C] |= 0xA0;

    /* TW: Some speed-up stuff */
    switch(pSiS->Chipset)  {
    case PCI_CHIP_SIS5597:
        /* TW: enable host bus */
	if(pSiS->NoHostBus) {
	   pReg->sisRegs3C4[0x34] &= ~0x08;
	} else {
           pReg->sisRegs3C4[0x34] |= 0x08;
	}
        /* TW: fall through */
    case PCI_CHIP_SIS6326:
    case PCI_CHIP_SIS530: 
        /* TW: Enable "dual segment register mode" (D2) and "i/o gating while
         *     write buffer is not empty" (D3)
         */
    	pReg->sisRegs3C4[0x0B] |= 0x0C;
    }

    /* set colordepth */
    if(pSiS->Chipset == PCI_CHIP_SIS530) {
        pReg->sisRegs3C4[0x09] &= 0x7F;
    }
    switch(pSiS->CurrentLayout.bitsPerPixel) {
        case 8:
            break;
        case 16:
	    offset <<= 1;
	    if(pSiS->CurrentLayout.depth == 15)
	        pReg->sisRegs3C4[0x06] |= 0x04;
	    else
                pReg->sisRegs3C4[0x06] |= 0x08;
            break;
        case 24:
            offset += (offset << 1);
            pReg->sisRegs3C4[0x06] |= 0x10;
            pReg->sisRegs3C4[0x0B] |= 0x90;
            break;
        case 32:
            if(pSiS->Chipset == PCI_CHIP_SIS530) {
	        offset <<= 2;
                pReg->sisRegs3C4[0x06] |= 0x10;
                pReg->sisRegs3C4[0x0B] |= 0x90;
                pReg->sisRegs3C4[0x09] |= 0x80;
            } else return FALSE;
            break;
    }

    /* save screen pitch for acceleration functions */
    pSiS->scrnOffset = pSiS->CurrentLayout.displayWidth *
                           ((pSiS->CurrentLayout.bitsPerPixel + 7) / 8);

    /* set linear framebuffer addresses */
    switch(pScrn->videoRam)  {
        case 512:
            temp = 0x00;  break;
        case 1024:
            temp = 0x20;  break;
        case 2048:
            temp = 0x40;  break;
        case 4096:
            temp = 0x60;  break;
        case 8192:
            temp = 0x80;  break;
        default:
            temp = 0x20;
    }
    pReg->sisRegs3C4[0x20] = (pSiS->FbAddress & 0x07F80000) >> 19;
    pReg->sisRegs3C4[0x21] = ((pSiS->FbAddress & 0xF8000000) >> 27) | temp;

    /* Set screen offset */
    vgaReg->CRTC[0x13] = offset & 0xFF;

    /* Set CR registers for our built-in TV and hi-res modes */
    if((sis6326tvmode) || (sis6326himode)) {

	int index,i;

	/* TW: We need our very private data for hi-res and TV modes */
	if(sis6326himode) {
	   if(strcmp(mode->name, "SIS1280x1024-75") == 0)  index = 7;
	   else index = 8;
	} else {
	  if(pSiS->SiS6326Flags & SIS6326_TVPAL) {
	    switch(width) {
	    case 800:
	      if((strcmp(mode->name, "PAL800x600U") == 0))
	      	index = 4;
	      else
	        index = 0;
	      break;
	    case 720:
	      index = 5;
	      break;
	    case 640:
	    default:
	      index = 1;
	    }
	  } else {
	    switch(height) {
	    case 400:
	      index = 3;
	      break;
	    case 480:
	    default:
	      if((strcmp(mode->name, "NTSC640x480U") == 0))
	        index = 6;
	      else
	        index = 2;
	    }
	  }
        }
	for(i=0; i<=5; i++) {
	    vgaReg->CRTC[i] = SiS6326CR[index][i];
	}
        pReg->sisRegs3C4[0x12] = SiS6326CR[index][6];
	vgaReg->CRTC[6] = SiS6326CR[index][7];
	vgaReg->CRTC[7] = SiS6326CR[index][8];
	vgaReg->CRTC[0x10] = SiS6326CR[index][9];
	vgaReg->CRTC[0x11] = SiS6326CR[index][10];
	vgaReg->CRTC[0x12] = SiS6326CR[index][11];
	vgaReg->CRTC[0x15] = SiS6326CR[index][12];
	vgaReg->CRTC[0x16] = SiS6326CR[index][13];
	vgaReg->CRTC[9] &= ~0x20;
	vgaReg->CRTC[9] |= (SiS6326CR[index][14] & 0x20);
	pReg->sisRegs3C4[0x0A] = ((offset & 0xF00) >> 4) | (SiS6326CR[index][14] & 0x0f);

    } else {

       /* Set extended vertical overflow register */
       pReg->sisRegs3C4[0x0A] = ((offset & 0xF00) >> 4) |
              (((mode->CrtcVTotal-2)     & 0x400) >> 10 ) |
              (((mode->CrtcVDisplay-1)   & 0x400) >>  9 ) |
/*            (((mode->CrtcVSyncStart-1) & 0x400) >>  8 ) |  */
	      (((mode->CrtcVBlankStart-1)& 0x400) >>  8 ) |
/*            (((mode->CrtcVBlankStart-1)& 0x400) >>  7 );  */
              (((mode->CrtcVSyncStart)   & 0x400) >>  7 );  

       /* Set extended horizontal overflow register */
       pReg->sisRegs3C4[0x12] &= 0xE0;
       pReg->sisRegs3C4[0x12] |= (
           (((mode->CrtcHTotal >> 3) - 5)      & 0x100) >> 8 |
           (((mode->CrtcHDisplay >> 3) - 1)    & 0x100) >> 7 |
/*         (((mode->CrtcHSyncStart >> 3) - 1)  & 0x100) >> 6 |  */
           (((mode->CrtcHBlankStart >> 3) - 1) & 0x100) >> 6 |
           ((mode->CrtcHSyncStart >> 3)        & 0x100) >> 5 |
           (((mode->CrtcHBlankEnd >> 3) - 1)   & 0x40)  >> 2);
    }

#ifdef TWDEBUG
    xf86DrvMsg(0, X_INFO, "HDisplay %d HSyncStart %d HSyncEnd %d HTotal %d\n",
		mode->CrtcHDisplay, mode->CrtcHSyncStart,
		mode->CrtcHSyncEnd, mode->CrtcHTotal);
    xf86DrvMsg(0, X_INFO, "HBlankSt %d  HBlankE %d\n",
    		mode->CrtcHBlankStart, mode->CrtcHBlankEnd);

    xf86DrvMsg(0, X_INFO, "VDisplay %d VSyncStart %d VSyncEnd %d VTotal %d\n",
		mode->CrtcVDisplay, mode->CrtcVSyncStart,
		mode->CrtcVSyncEnd, mode->CrtcVTotal);
    xf86DrvMsg(0, X_INFO, "VBlankSt %d  VBlankE %d\n",
    		mode->CrtcVBlankStart, mode->CrtcVBlankEnd);
#endif

    /* enable (or disable) line compare */
    if(mode->CrtcVDisplay >= 1024)
        pReg->sisRegs3C4[0x38] |= 0x04;
    else
        pReg->sisRegs3C4[0x38] &= 0xFB;

    /* Enable (or disable) high speed DCLK (some 6326 and 530/620 only) */
    if( ( (pSiS->Chipset == PCI_CHIP_SIS6326) &&
          ( (pSiS->ChipRev == 0xd0) || (pSiS->ChipRev == 0xd1) ||
            (pSiS->ChipRev == 0xd2) || (pSiS->ChipRev == 0x92) ||
	    (pSiS->Flags & A6326REVAB) ) ) ||
        (pSiS->oldChipset > OC_SIS6326) ) {
      if( (pSiS->CurrentLayout.bitsPerPixel == 24) ||
          (pSiS->CurrentLayout.bitsPerPixel == 32) ||
          (mode->CrtcHDisplay >= 1280) )
         pReg->sisRegs3C4[0x3E] |= 0x01;
      else
         pReg->sisRegs3C4[0x3E] &= 0xFE;
    }

    /* We use the internal VCLK */
    pReg->sisRegs3C4[0x38] &= 0xFC;

    /* Set VCLK */
    if((sis6326tvmode) || (sis6326himode)) {
        /* TW: For our built-in modes, the calculation is not suitable */
      if(sis6326himode) {
        if((strcmp(mode->name, "SIS1280x1024-75") == 0)) {
	   pReg->sisRegs3C4[0x2A] = 0x5d;	/* 1280x1024-75 */
           pReg->sisRegs3C4[0x2B] = 0xa4;
        } else {
	   pReg->sisRegs3C4[0x2A] = 0x59;	/* 1600x1200-60 */
           pReg->sisRegs3C4[0x2B] = 0xa3;
        }
	pReg->sisRegs3C4[0x13] &= ~0x40;
      } else {
        if(pSiS->SiS6326Flags & SIS6326_TVPAL) {
	   /* PAL: 31.500 Mhz */
	   if((strcmp(mode->name, "PAL800x600U") == 0)) {
   	      pReg->sisRegs3C4[0x2A] = 0x46;
              pReg->sisRegs3C4[0x2B] = 0x49;
	   } else {
	      pReg->sisRegs3C4[0x2A] = 0xab;
              pReg->sisRegs3C4[0x2B] = 0xe9;
	   }
	   pReg->sisRegs3C4[0x13] &= ~0x40;
	} else {
	   /* NTSC: 27.000 Mhz */
	   if((strcmp(mode->name, "NTSC640x480U") == 0)) {
	      pReg->sisRegs3C4[0x2A] = 0x5a;
              pReg->sisRegs3C4[0x2B] = 0x65;
	   } else {
	      pReg->sisRegs3C4[0x2A] = 0x29;
              pReg->sisRegs3C4[0x2B] = 0xe2;
	   }
	   pReg->sisRegs3C4[0x13] |= 0x40;
	}
      }
    } else if(SiS_compute_vclk(clock, &num, &denum, &div, &sbit, &scale)) {
        pReg->sisRegs3C4[0x2A] = (num - 1) & 0x7f ;
        pReg->sisRegs3C4[0x2A] |= (div == 2) ? 0x80 : 0;
        pReg->sisRegs3C4[0x2B] = ((denum - 1) & 0x1f);
        pReg->sisRegs3C4[0x2B] |= (((scale -1) & 3) << 5);

	/* When setting VCLK, we should set SR13 first */
        if(sbit)
              pReg->sisRegs3C4[0x13] |= 0x40;
        else
              pReg->sisRegs3C4[0x13] &= 0xBF;

#ifdef TWDEBUG
	xf86DrvMsg(0, X_INFO, "2a: %x 2b: %x 13: %x clock %d\n",
		pReg->sisRegs3C4[0x2A], pReg->sisRegs3C4[0x2B], pReg->sisRegs3C4[0x13], clock);
#endif

    } else {
        /* if SiS_compute_vclk cannot handle the requested clock, try sisCalcClock */
        SiSCalcClock(pScrn, clock, 2, vclk);

        pReg->sisRegs3C4[0x2A] = (vclk[Midx] - 1) & 0x7f;
        pReg->sisRegs3C4[0x2A] |= ((vclk[VLDidx] == 2) ? 1 : 0) << 7;

	/* bits [4:0] contain denumerator */
        pReg->sisRegs3C4[0x2B] = (vclk[Nidx] - 1) & 0x1f;

        if (vclk[Pidx] <= 4){
              /* postscale 1,2,3,4 */
              pReg->sisRegs3C4[0x2B] |= (vclk[Pidx] - 1) << 5;
              pReg->sisRegs3C4[0x13] &= 0xBF;
        } else {
              /* postscale 6,8 */
              pReg->sisRegs3C4[0x2B] |= ((vclk[Pidx] / 2) - 1) << 5;
              pReg->sisRegs3C4[0x13] |= 0x40;
        }
        pReg->sisRegs3C4[0x2B] |= 0x80 ;   /* gain for high frequency */
    }

    /* High speed DAC */
    if(clock > 135000)
        pReg->sisRegs3C4[0x07] |= 0x02;

    /* Programmable Clock */
    pReg->sisRegs3C2 = inb(SISMISCR) | 0x0C;

    /* 1 or 2 cycle DRAM (set by option FastVram) */
    if(pSiS->newFastVram == -1) {
        pReg->sisRegs3C4[0x34] |= 0x80;
	pReg->sisRegs3C4[0x34] &= ~0x40;
    } else if(pSiS->newFastVram == 1)
        pReg->sisRegs3C4[0x34] |= 0xC0;
    else
        pReg->sisRegs3C4[0x34] &= ~0xC0;

    /* Logical line length */
    pSiS->ValidWidth = TRUE;
    pReg->sisRegs3C4[0x27] &= 0xCF;
    if(pSiS->CurrentLayout.bitsPerPixel == 24) {
         /* Invalid logical width */
         pReg->sisRegs3C4[0x27] |= 0x30;
         pSiS->ValidWidth = FALSE;
    } else {
	 switch(pScrn->virtualX * (pSiS->CurrentLayout.bitsPerPixel >> 3)) {
         case 1024:
               pReg->sisRegs3C4[0x27] |= 0x00;
               break;
         case 2048:
               pReg->sisRegs3C4[0x27] |= 0x10;
               break;
         case 4096:
               pReg->sisRegs3C4[0x27] |= 0x20;
               break;
         default:
               /* Invalid logical width */
               pReg->sisRegs3C4[0x27] |= 0x30;
               pSiS->ValidWidth = FALSE;
               break;
         }
    }

    /* Acceleration stuff */
    if(!pSiS->NoAccel) {
         pReg->sisRegs3C4[0x27] |= 0x40;   /* Enable engine programming registers */
         if( (pSiS->TurboQueue) &&	       /* Handle TurboQueue */
 	     ( (pSiS->Chipset != PCI_CHIP_SIS530) ||
	       (pSiS->CurrentLayout.bitsPerPixel != 24) ) ) {
               pReg->sisRegs3C4[0x27] |= 0x80;        /* Enable TQ */
	       if((pSiS->Chipset == PCI_CHIP_SIS530) ||
		  ((pSiS->Chipset == PCI_CHIP_SIS6326 &&
		   (pSiS->ChipRev == 0xd0 || pSiS->ChipRev == 0xd1 ||
		    pSiS->ChipRev == 0xd2 || pSiS->ChipRev == 0x92 ||
		    pSiS->ChipRev == 0x0a || pSiS->ChipRev == 0x1a ||
		    pSiS->ChipRev == 0x2a || pSiS->ChipRev == 0x0b ||
		    pSiS->ChipRev == 0x1b || pSiS->ChipRev == 0x2b) ) ) ) {
		    /* pReg->sisRegs3C4[0x3D] |= 0x80;  */     /* Queue is 62K (530/620 specs) */
		       pReg->sisRegs3C4[0x3D] &= 0x7F;         /* Queue is 30K (530/620 specs) */
		}
		/* TW: Locate the TQ at the beginning of the last 64K block of
		 *     video RAM. The address is to be specified in 32K steps.
		 */
		pReg->sisRegs3C4[0x2C] = (pScrn->videoRam - 64) / 32;
		if(pSiS->Chipset != PCI_CHIP_SIS530) {	/* 530/620: Reserved (don't touch) */
    		        pReg->sisRegs3C4[0x3C] &= 0xFC; /* 6326: Queue is all for 2D */
		}					/* 5597: Must be 0           */
          } else {
	        pReg->sisRegs3C4[0x27] &= 0x7F;
	  }
    }

    /* TW: No idea what this does. The Windows driver does it, so we do it as well */
    if(pSiS->Chipset == PCI_CHIP_SIS6326) {
       if((pSiS->ChipRev == 0xd0) || (pSiS->ChipRev == 0xd1) ||
          (pSiS->ChipRev == 0xd2) || (pSiS->ChipRev == 0x92) ||
	  (pSiS->Flags & A6326REVAB)) {
	  if((pSiS->Flags & (SYNCDRAM | RAMFLAG)) == (SYNCDRAM | RAMFLAG)) {
	     if(!(pReg->sisRegs3C4[0x0E] & 0x03)) {
	         pReg->sisRegs3C4[0x3E] |= 0x02;
	     }
	  }
       }
    }


    /* Set memclock */
#if 0
    /* TW: We don't need to do this; the SetMClk option was not used since 4.0. */
    if((pSiS->Chipset == PCI_CHIP_SIS5597) || (pSiS->Chipset == PCI_CHIP_SIS6326)) {
      if(pSiS->MemClock > 66000) {
          SiSCalcClock(pScrn, pSiS->MemClock, 1, vclk);

          pReg->sisRegs3C4[0x28] = (vclk[Midx] - 1) & 0x7f ;
          pReg->sisRegs3C4[0x28] |= ((vclk[VLDidx] == 2 ) ? 1 : 0 ) << 7 ;
          pReg->sisRegs3C4[0x29] = (vclk[Nidx] -1) & 0x1f ;   /* bits [4:0] contain denumerator -MC */
          if(vclk[Pidx] <= 4) {
            pReg->sisRegs3C4[0x29] |= (vclk[Pidx] - 1) << 5 ; /* postscale 1,2,3,4 */
            pReg->sisRegs3C4[0x13] &= 0x7F;
          } else {
            pReg->sisRegs3C4[0x29] |= ((vclk[Pidx] / 2) - 1) << 5 ;  /* postscale 6,8 */
            pReg->sisRegs3C4[0x13] |= 0x80;
          }
          /* Check programmed memory clock. Enable only to check the above code */
/*
          mclk = 14318 * ((pReg->sisRegs3C4[0x28] & 0x7f) + 1);
          mclk /= ((pReg->sisRegs3C4[0x29] & 0x0f) + 1);
          if(!(pReg->sisRegs3C4[0x13] & 0x80)) {
             mclk /= (((pReg->sisRegs3C4[0x29] & 0x60) >> 5) + 1);
          } else {
             if ((pReg->sisRegs3C4[0x29] & 0x60) == 0x40) mclk /= 6;
             if ((pReg->sisRegs3C4[0x29] & 0x60) == 0x60) mclk /= 8;
          }
          xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO,2,
                 "Setting memory clock to %.3f MHz\n",
                 mclk/1000.0);
*/
      }
    }
#endif

    /* TW: set threshold values (rewritten) */
    /*
     * CPU/CRT Threshold:                     FIFO
     *                           MCLK     ___________      VCLK
     * cpu/engine <---o       o--------->|___________| -----------> CRT
     *                ^       ^            ^       ^
     *                 \     /             |       |
     *                  \   /              |< gap >|
     *                   \ /               |       |
     *           selector switch   Thrsh. low     high
     *
     * CRT consumes the data in the FIFO during scanline display. When the
     * amount of data in the FIFO reaches the Threshold low value, the selector
     * switch will switch to the right, and the FIFO will be refilled with data.
     * When the amount of data in the FIFO reaches the Threshold high value, the
     * selector switch will switch to the left and allows the CPU and the chip
     * engines to access video RAM.
     *
     * The Threshold low values should be increased at higher bpps, simply because
     * there is more data needed for the CRT. When Threshold low and high are very
     * close to each other, the selector switch will be activated more often, which
     * decreases performance.
     *
     */
    switch(pSiS->Chipset) {
    case PCI_CHIP_SIS5597:  factor = 65; break;
    case PCI_CHIP_SIS6326:  factor = 30; break;
    case PCI_CHIP_SIS530:   factor = (pSiS->Flags & UMA) ? 60 : 30; break;
    default:                factor = (pScrn->videoRam > (1024*1024)) ? 24 : 12;
    }
    a = width * height * rate * 1.40 * factor * ((pSiS->CurrentLayout.bitsPerPixel + 1) / 8);
    b = (mclk / 1000) * 999488.0 * (buswidth / 8);
    c = ((a / b) + 1.0) / 2;
    d = (int)c + 2;

#ifdef TWDEBUG
    xf86DrvMsg(0, X_INFO,
       "Debug: w %d h %d r %d mclk %d bus %d factor %d bpp %d\n",
       width, height, rate, mclk/1000, buswidth, factor,
       pSiS->CurrentLayout.bitsPerPixel);
    xf86DrvMsg(0, X_INFO, "Debug: a %f b %f c %f d %d (flags %x)\n",
     	a, b, c, d, pSiS->Flags);
#endif

    CRT_CPUthresholdLow = d;
    if((pSiS->Flags & (RAMFLAG | SYNCDRAM)) == (RAMFLAG | SYNCDRAM)) {
     		CRT_CPUthresholdLow += 2;
    }
    CRT_CPUthresholdHigh = CRT_CPUthresholdLow + 3;

    CRT_ENGthreshold = 0x0F;

#ifdef TWDEBUG
    xf86DrvMsg(0, X_INFO, "Debug: Thlow %d thhigh %d\n",
     	CRT_CPUthresholdLow, CRT_CPUthresholdHigh);
#endif

#if 0  /* TW: See comment in sis_dac.c on why this is commented */
    if(pSiS->Chipset == PCI_CHIP_SIS530) {
       if((pSiS->oldChipset == OC_SIS530A) &&
	  (pSiS->Flags & UMA) &&
	  (mclk == 100000) &&
	  (pSiS->Flags & ESS137xPRESENT)) {
	    if(!(pSiS->Flags & SECRETFLAG)) index = 0;
            if((temp = SiS_CalcSpecial530Threshold(pSiS, mode, index)) {
	    	CRT_CPUthresholdLow = temp;
	        break;
	    }
       }
    }
#endif

    switch(pSiS->Chipset) {
    case PCI_CHIP_SIS530:
        if(CRT_CPUthresholdLow > 0x1f)  CRT_CPUthresholdLow = 0x1f;
        CRT_CPUthresholdHigh = 0x1f;
        break;
    case PCI_CHIP_SIS5597:
    case PCI_CHIP_SIS6326:
    default:
        if(CRT_CPUthresholdLow > 0x0f)  CRT_CPUthresholdLow  = 0x0f;
        if(CRT_CPUthresholdHigh > 0x0f) CRT_CPUthresholdHigh = 0x0f;
    }

    pReg->sisRegs3C4[0x08] = ((CRT_CPUthresholdLow & 0x0F) << 4) |
			      (CRT_ENGthreshold & 0x0F);

    pReg->sisRegs3C4[0x09] &= 0xF0;
    pReg->sisRegs3C4[0x09] |= (CRT_CPUthresholdHigh & 0x0F);

    pReg->sisRegs3C4[0x3F] &= 0xEB;
    pReg->sisRegs3C4[0x3F] |= (CRT_CPUthresholdHigh & 0x10) |
                       	      ((CRT_CPUthresholdLow & 0x10) >> 2);

    if(pSiS->oldChipset >= OC_SIS530A) {
     	pReg->sisRegs3C4[0x3F] &= 0xDF;
	pReg->sisRegs3C4[0x3F] |= 0x58;
    }

    /* TW: Set SiS6326 TV registers */
    if((pSiS->Chipset == PCI_CHIP_SIS6326) && (sis6326tvmode)) {
      unsigned char tmp;
      int index=0, i, j, k;

      if(pSiS->SiS6326Flags & SIS6326_TVPAL) {
         pReg->sisRegs3C4[0x0D] |= 0x04;
         switch(width) {
	   case 800:
	      if((strcmp(mode->name, "PAL800x600U") == 0))  index = 4;
	      else	        			    index = 3;
	      break;
	   case 720:
	      index = 5;
	      break;
	   case 640:
	   default:
	      index = 2;
	      break;
	 }
	 for(i=0; i<14; i++) {
	     pReg->sis6326tv[SiS6326TVRegs1[i]] = SiS6326TVRegs1_PAL[index][i];
	 }
      } else {
	pReg->sisRegs3C4[0x0D] &= ~0x04;
	if((strcmp(mode->name, "NTSC640x480U") == 0))  index = 5;
	else 					       index = 4;
        for(i=0; i<14; i++) {
	     pReg->sis6326tv[SiS6326TVRegs1[i]] = SiS6326TVRegs1_NTSC[index][i];
	}
      }
      tmp = pReg->sis6326tv[0x43];
      if(pSiS->SiS6326Flags & SIS6326_TVCVBS) tmp |= 0x10;
      tmp |= 0x08;
      pReg->sis6326tv[0x43] = tmp;
      j = 0; k = 0;
      for(i=0; i<=0x44; i++) {
	 if(SiS6326TVRegs1[j] == i) {
	 	j++;
		continue;
	 }
	 if(pSiS->SiS6326Flags & SIS6326_TVPAL) {
		tmp = SiS6326TVRegs2_PAL[index][k];
	 } else {
		tmp = SiS6326TVRegs2_NTSC[index][k];
	 }
	 pReg->sis6326tv[i] = tmp;
	 k++;
      }
      pReg->sis6326tv[0x43] |= 0x08;
      if((pSiS->ChipRev == 0xc1) || (pSiS->ChipRev == 0xc2)) {
         pReg->sis6326tv[0x43] &= ~0x08;
      }

      tmp = pReg->sis6326tv[0];
      tmp |= 0x18;
      if(pSiS->SiS6326Flags & SIS6326_TVCVBS) tmp &= ~0x10;
      else				      tmp &= ~0x08;
      tmp |= 0x04;
      pReg->sis6326tv[0] = tmp;
    }

    return(TRUE);
}

/* TW: Init a mode for SiS 300 and 310/325 series
 *     The original intention of the followling procedure was
 *     to initialize various registers for the selected mode.
 *     This was actually done to a structure, not the hardware.
 *     (SiSRestore would write the structure to the hardware
 *     registers.)
 *     This function is now only used for setting up some
 *     variables (eg. scrnOffset).
 */
Bool
SIS300Init(ScrnInfoPtr pScrn, DisplayModePtr mode)
{
    SISPtr         pSiS = SISPTR(pScrn);
    SISRegPtr      pReg = &pSiS->ModeReg;
    unsigned short temp;

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4, "SIS300Init()\n");

    xf86DrvMsgVerb(pScrn->scrnIndex, X_INFO, 4,
        "virtualX = %d depth = %d Logical width = %d\n",
	pScrn->virtualX, pSiS->CurrentLayout.bitsPerPixel,
        pScrn->virtualX * pSiS->CurrentLayout.bitsPerPixel/8);

    /* Copy current register settings to structure */
    (*pSiS->SiSSave)(pScrn, pReg);

    /* TW: Calculate Offset/Display Pitch */
    pSiS->scrnOffset = pSiS->CurrentLayout.displayWidth *
                          ((pSiS->CurrentLayout.bitsPerPixel+7)/8);
    pSiS->scrnPitch = pSiS->scrnOffset;
    if (mode->Flags & V_INTERLACE)  pSiS->scrnPitch <<= 1;

#ifdef UNLOCK_ALWAYS
    outSISIDXREG(SISSR, 0x05, 0x86);
#endif

    switch(pSiS->CurrentLayout.bitsPerPixel) {
        case 8:
            pSiS->DstColor = 0x0000;
	    pSiS->SiS310_AccelDepth = 0x00000000;
            break;
        case 16:
	    if(pSiS->CurrentLayout.depth == 15)
	        pSiS->DstColor = (short) 0x4000;
	    else
                pSiS->DstColor = (short) 0x8000;
	    pSiS->SiS310_AccelDepth = 0x00010000;
            break;
        case 24:
            break;
        case 32:
            pSiS->DstColor = (short) 0xC000;
	    pSiS->SiS310_AccelDepth = 0x00020000;
            break;
    }

    /* TW: Enable PCI LINEAR ADDRESSING (0x80), MMIO (0x01), PCI_IO (0x20) */
    pReg->sisRegs3C4[0x20] = 0xA1;

/* TW: Now initialize TurboQueue. TB is always located at the very top of
 *     the videoRAM (notably NOT the x framebuffer memory, which can/should
 *     be limited by MaxXFbMem when using DRI). Also, enable the accelerators.
 */
    if (!pSiS->NoAccel) {
        pReg->sisRegs3C4[0x1E] |= 0x42;  /* TW: Enable 2D accelerator */
	pReg->sisRegs3C4[0x1E] |= 0x18;  /* TW: Enable 3D accelerator */
	switch (pSiS->VGAEngine) {
	case SIS_300_VGA:
	  if(pSiS->TurboQueue)  {    		/* set Turbo Queue as 512k */
	    temp = ((pScrn->videoRam/64)-8);    /* TW: 8=512k, 4=256k, 2=128k, 1=64k */
            pReg->sisRegs3C4[0x26] = temp & 0xFF;
	    pReg->sisRegs3C4[0x27] =
		(pReg->sisRegs3C4[0x27] & 0xfc) | (((temp >> 8) & 3) | 0xF0);
          }	/* TW: line above new for saving D2&3 of status register */
	  break;
	case SIS_315_VGA:
	  /* See comments in sis_driver.c */
	  pReg->sisRegs3C4[0x27] = 0x1F;
	  pReg->sisRegs3C4[0x26] = 0x22;
	  pReg->sisMMIO85C0 = (pScrn->videoRam - 512) * 1024;
	  break;
	}
    }

    return(TRUE);
}

int
SISDoSense(ScrnInfoPtr pScrn, int tempbl, int tempbh, int tempcl, int tempch)
{
    SISPtr  pSiS = SISPTR(pScrn);
    int temp;

    outSISIDXREG(SISPART4,0x11,tempbl);
    temp = tempbh | tempcl;
    setSISIDXREG(SISPART4,0x10,0xe0,temp);
    usleep(200000);
    tempch &= 0x7f;
    inSISIDXREG(SISPART4,0x03,temp);
    temp ^= 0x0e;
    temp &= tempch;
    return(temp);
}

/* TW: Sense connected devices on 30x */
void SISSense30x(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    unsigned char backupP4_0d,biosflag;
    unsigned char testsvhs_tempbl, testsvhs_tempbh;
    unsigned char testsvhs_tempcl, testsvhs_tempch;
    unsigned char testcvbs_tempbl, testcvbs_tempbh;
    unsigned char testcvbs_tempcl, testcvbs_tempch;
    unsigned char testvga2_tempbl, testvga2_tempbh;
    unsigned char testvga2_tempcl, testvga2_tempch;
    int myflag, result;
    unsigned short temp;

    inSISIDXREG(SISPART4,0x0d,backupP4_0d);
    outSISIDXREG(SISPART4,0x0d,(backupP4_0d | 0x04));

    if(pSiS->VGAEngine == SIS_315_VGA) {
       if(pSiS->sishw_ext.UseROM) {
          temp = 0xf3;
	  if(pSiS->Chipset == PCI_CHIP_SIS330) temp = 0x11b;
          if(pSiS->BIOS[temp] & 0x08) {
	      xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	          "SiS30x: Video bridge has (unsupported) DVI combo connector\n");
	      orSISIDXREG(SISCR, 0x32, 0x80);
          } else {
	      andSISIDXREG(SISCR, 0x32, 0x7f);
	  }
       }
    }

    if(pSiS->VGAEngine == SIS_300_VGA) {

        if(pSiS->sishw_ext.UseROM) {
	   testvga2_tempbh = pSiS->BIOS[0xf9]; testvga2_tempbl = pSiS->BIOS[0xf8];
	   testsvhs_tempbh = pSiS->BIOS[0xfb]; testsvhs_tempbl = pSiS->BIOS[0xfa];
	   testcvbs_tempbh = pSiS->BIOS[0xfd]; testcvbs_tempbl = pSiS->BIOS[0xfc];
	   biosflag = pSiS->BIOS[0xfe];
	} else {
	   testvga2_tempbh = 0x00; testvga2_tempbl = 0xd1;
           testsvhs_tempbh = 0x00; testsvhs_tempbl = 0xb9;
	   testcvbs_tempbh = 0x00; testcvbs_tempbl = 0xb3;
	   biosflag = 0;
	}
	if(pSiS->VBFlags & (VB_301B|VB_302B|VB_30xLV|VB_30xLVX)) {
	   testvga2_tempbh = 0x01; testvga2_tempbl = 0x90;
	   testsvhs_tempbh = 0x01; testsvhs_tempbl = 0x6b;
	   testcvbs_tempbh = 0x01; testcvbs_tempbl = 0x74;
	}
	inSISIDXREG(SISPART4,0x01,myflag);
	if(myflag & 0x04) {
	   testvga2_tempbh = 0x00; testvga2_tempbl = 0xfd;
	   testsvhs_tempbh = 0x00; testsvhs_tempbl = 0xdd;
	   testcvbs_tempbh = 0x00; testcvbs_tempbl = 0xee;
	}
	testvga2_tempch = 0x0e;	testvga2_tempcl = 0x08;
	testsvhs_tempch = 0x06;	testsvhs_tempcl = 0x04;
	testcvbs_tempch = 0x08; testcvbs_tempcl = 0x04;

    } else if((pSiS->Chipset == PCI_CHIP_SIS315) ||
    	      (pSiS->Chipset == PCI_CHIP_SIS315H) ||
	      (pSiS->Chipset == PCI_CHIP_SIS315PRO)) {

	if(pSiS->sishw_ext.UseROM) {
	   testvga2_tempbh = pSiS->BIOS[0xbe]; testvga2_tempbl = pSiS->BIOS[0xbd];
	   testsvhs_tempbh = pSiS->BIOS[0xc0]; testsvhs_tempbl = pSiS->BIOS[0xbf];
	   testcvbs_tempbh = pSiS->BIOS[0xc2]; testcvbs_tempbl = pSiS->BIOS[0xc1];
	   biosflag = pSiS->BIOS[0xf3];
	} else {
	   testvga2_tempbh = 0x00; testvga2_tempbl = 0xd1;
           testsvhs_tempbh = 0x00; testsvhs_tempbl = 0xb9;
	   testcvbs_tempbh = 0x00; testcvbs_tempbl = 0xb3;
	   biosflag = 0;
	}
	if(pSiS->VBFlags & (VB_301B|VB_302B|VB_30xLV|VB_30xLVX)) {
	   if(pSiS->sishw_ext.UseROM) {
	      testvga2_tempbh = pSiS->BIOS[0xc4]; testvga2_tempbl = pSiS->BIOS[0xc3];
	      testsvhs_tempbh = pSiS->BIOS[0xc6]; testsvhs_tempbl = pSiS->BIOS[0xc5];
	      testcvbs_tempbh = pSiS->BIOS[0xc8]; testcvbs_tempbl = pSiS->BIOS[0xc7];
	   } else {
	      testvga2_tempbh = 0x01; testvga2_tempbl = 0x90;
	      testsvhs_tempbh = 0x01; testsvhs_tempbl = 0x6b;
	      testcvbs_tempbh = 0x01; testcvbs_tempbl = 0x74;
	   }
	}
	inSISIDXREG(SISPART4,0x01,myflag);
	if(myflag & 0x04) {
	   testvga2_tempbh = 0x00; testvga2_tempbl = 0xfd;
	   testsvhs_tempbh = 0x00; testsvhs_tempbl = 0xdd;
	   testcvbs_tempbh = 0x00; testcvbs_tempbl = 0xee;
	}
	testvga2_tempch = 0x0e;	testvga2_tempcl = 0x08;
	testsvhs_tempch = 0x06;	testsvhs_tempcl = 0x04;
	testcvbs_tempch = 0x08; testcvbs_tempcl = 0x04;

    } else if(pSiS->Chipset == PCI_CHIP_SIS330) {

	if(pSiS->sishw_ext.UseROM) {
	   testvga2_tempbh = pSiS->BIOS[0xe6]; testvga2_tempbl = pSiS->BIOS[0xe5];
	   testsvhs_tempbh = pSiS->BIOS[0xe8]; testsvhs_tempbl = pSiS->BIOS[0xe7];
	   testcvbs_tempbh = pSiS->BIOS[0xea]; testcvbs_tempbl = pSiS->BIOS[0xe9];
	   biosflag = pSiS->BIOS[0x11b];
	} else {
	   testvga2_tempbh = 0x00; testvga2_tempbl = 0xd1;
           testsvhs_tempbh = 0x00; testsvhs_tempbl = 0xb9;
	   testcvbs_tempbh = 0x00; testcvbs_tempbl = 0xb3;
	   biosflag = 0;
	}
	if(pSiS->VBFlags & (VB_301B|VB_302B|VB_30xLV|VB_30xLVX)) {
	   if(pSiS->sishw_ext.UseROM) {
	      testvga2_tempbh = pSiS->BIOS[0xec]; testvga2_tempbl = pSiS->BIOS[0xeb];
	      testsvhs_tempbh = pSiS->BIOS[0xee]; testsvhs_tempbl = pSiS->BIOS[0xed];
	      testcvbs_tempbh = pSiS->BIOS[0xf0]; testcvbs_tempbl = pSiS->BIOS[0xef];
	   } else {
	      testvga2_tempbh = 0x01; testvga2_tempbl = 0x90;
	      testsvhs_tempbh = 0x01; testsvhs_tempbl = 0x6b;
	      testcvbs_tempbh = 0x01; testcvbs_tempbl = 0x74;
	   }
	}
	inSISIDXREG(SISPART4,0x01,myflag);
	if(myflag & 0x04) {
	   testvga2_tempbh = 0x00; testvga2_tempbl = 0xfd;
	   testsvhs_tempbh = 0x00; testsvhs_tempbl = 0xdd;
	   testcvbs_tempbh = 0x00; testcvbs_tempbl = 0xee;
	}
	testvga2_tempch = 0x0e;	testvga2_tempcl = 0x08;
	testsvhs_tempch = 0x06;	testsvhs_tempcl = 0x04;
	testcvbs_tempch = 0x08; testcvbs_tempcl = 0x04;

    } else {  /* 550?, 650, 740 */

        if(pSiS->sishw_ext.UseROM) {
	   testvga2_tempbh = pSiS->BIOS[0xbe]; testvga2_tempbl = pSiS->BIOS[0xbd];
	   testsvhs_tempbh = pSiS->BIOS[0xc0]; testsvhs_tempbl = pSiS->BIOS[0xbf];
	   testcvbs_tempbh = pSiS->BIOS[0xc2]; testcvbs_tempbl = pSiS->BIOS[0xc1];
	   biosflag = pSiS->BIOS[0xf3];
	} else {
	   testvga2_tempbh = 0x00; testvga2_tempbl = 0xd1;
           testsvhs_tempbh = 0x00; testsvhs_tempbl = 0xb9;
	   testcvbs_tempbh = 0x00; testcvbs_tempbl = 0xb3;
	   biosflag = 0;
	}
	testvga2_tempch = 0x0e;	testvga2_tempcl = 0x08;
	testsvhs_tempch = 0x06; testsvhs_tempcl = 0x04;
	testcvbs_tempch = 0x08; testcvbs_tempcl = 0x04;

        /* TW: Different BIOS versions use different values for the 301LV.
	       These values are from the newest versions 1.10.6? and 1.10.7?.
	       I have no idea if these values are suitable for the 301B as well.
	 */

	if(pSiS->VBFlags & (VB_301B|VB_302B|VB_30xLV|VB_30xLVX)) {
  	   if(pSiS->sishw_ext.UseROM) {
	      testvga2_tempbh = pSiS->BIOS[0xc4]; testvga2_tempbl = pSiS->BIOS[0xc3];
	      testsvhs_tempbh = pSiS->BIOS[0xc6]; testsvhs_tempbl = pSiS->BIOS[0xc5];
	      testcvbs_tempbh = pSiS->BIOS[0xc8]; testcvbs_tempbl = pSiS->BIOS[0xc7];
	      biosflag = pSiS->BIOS[0xf3];
	   } else {
	      testvga2_tempbh = 0x01; testvga2_tempbl = 0x90;
	      testsvhs_tempbh = 0x02; testsvhs_tempbl = 0x00;
	      testcvbs_tempbh = 0x01; testcvbs_tempbl = 0x00;
	      biosflag = 0;
	   }
	   testvga2_tempch = 0x0e; testvga2_tempcl = 0x08;
	   testsvhs_tempch = 0x04; testsvhs_tempcl = 0x08;
	   testcvbs_tempch = 0x08; testcvbs_tempcl = 0x08;
	}

    }

    /* TW: No VGA2 or SCART on LV bridges */
    if(pSiS->VBFlags & (VB_30xLV|VB_30xLVX)) {
	testvga2_tempbh = testvga2_tempbl = 0x00;
	testvga2_tempch = testvga2_tempcl = 0x00;
    }

    if(testvga2_tempch || testvga2_tempcl || testvga2_tempbh || testvga2_tempbl) {
#ifdef TWDEBUG
       xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                "SiS30x: Scanning for VGA2/SCART (%x %x %x %x)\n",
    		testvga2_tempbh, testvga2_tempbl, testvga2_tempch, testvga2_tempcl);
#endif

       result = SISDoSense(pScrn, testvga2_tempbl, testvga2_tempbh,
                               testvga2_tempcl, testvga2_tempch);
       if(result) {
           if(biosflag & 0x01) {
	      xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	         "SiS30x: Detected TV connected to SCART output\n");
	      pSiS->VBFlags |= TV_SCART;
	      orSISIDXREG(SISCR, 0x32, 0x04);
	      pSiS->postVBCR32 |= 0x04;
	   } else if(!(pSiS->VBFlags & (VB_30xLV|VB_30xLVX))) {
	      xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	         "SiS30x: Detected secondary VGA connection\n");
	      pSiS->VBFlags |= VGA2_CONNECTED;
	      orSISIDXREG(SISCR, 0x32, 0x10);
	      pSiS->postVBCR32 |= 0x10;
	   }
       }
    }

#ifdef TWDEBUG
    xf86DrvMsg(pScrn->scrnIndex, X_INFO,
                "SiS30x: Scanning for TV (%x %x %x %x; %x %x %x %x)\n",
    		testsvhs_tempbh, testsvhs_tempbl, testsvhs_tempch, testsvhs_tempcl,
		testcvbs_tempbh, testcvbs_tempbl, testcvbs_tempch, testcvbs_tempcl);
#endif

    result = SISDoSense(pScrn, testsvhs_tempbl, testsvhs_tempbh,
                               testsvhs_tempcl, testsvhs_tempch);
    if(result) {
        xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	      "SiS30x: Detected TV connected to SVIDEO output\n");
        /* TW: So we can be sure that there IS a SVIDEO output */
	pSiS->VBFlags |= TV_SVIDEO;
	orSISIDXREG(SISCR, 0x32, 0x02);
	pSiS->postVBCR32 |= 0x02;
    }

    if((biosflag & 0x02) || (!(result))) {

        result = SISDoSense(pScrn, testcvbs_tempbl, testcvbs_tempbh,
	                           testcvbs_tempcl, testcvbs_tempch);
	if(result) {
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	          "SiS30x: Detected TV connected to COMPOSITE output\n");
	    /* TW: So we can be sure that there IS a CVBS output */
	    pSiS->VBFlags |= TV_AVIDEO;
	    orSISIDXREG(SISCR, 0x32, 0x01);
	    pSiS->postVBCR32 |= 0x01;
	}
    }
    SISDoSense(pScrn, 0, 0, 0, 0);

    outSISIDXREG(SISPART4,0x0d,backupP4_0d);
}

static void
SiS6326TVDelay(ScrnInfoPtr pScrn, int delay)
{
    SISPtr  pSiS = SISPTR(pScrn);
    int i;
    unsigned char temp;

    for(i=0; i<delay; i++) {
    	inSISIDXREG(SISSR, 0x05, temp);
    }
}

int
SIS6326DoSense(ScrnInfoPtr pScrn, int tempbh, int tempbl, int tempch, int tempcl)
{
    unsigned char temp;

    SiS6326SetTVReg(pScrn, 0x42, tempbl);
    temp = SiS6326GetTVReg(pScrn, 0x43);
    temp &= 0xfc;
    temp |= tempbh;
    SiS6326SetTVReg(pScrn, 0x43, temp);
    SiS6326TVDelay(pScrn, 0x1000);
    temp = SiS6326GetTVReg(pScrn, 0x43);
    temp |= 0x04;
    SiS6326SetTVReg(pScrn, 0x43, temp);
    SiS6326TVDelay(pScrn, 0x8000);
    temp = SiS6326GetTVReg(pScrn, 0x44);
    if(!(tempch & temp)) tempcl = 0;
    return(tempcl);
}

void
SISSense6326(ScrnInfoPtr pScrn)
{
    SISPtr pSiS = SISPTR(pScrn);
    unsigned char temp;
    int result;

    pSiS->SiS6326Flags &= (SIS6326_HASTV | SIS6326_TVPAL);
    temp = SiS6326GetTVReg(pScrn, 0x43);
    temp &= 0xfb;
    SiS6326SetTVReg(pScrn, 0x43, temp);
    result = SIS6326DoSense(pScrn, 0x01, 0xb0, 0x06, SIS6326_TVSVIDEO);  /* 0x02 */
    pSiS->SiS6326Flags |= result;
    result = SIS6326DoSense(pScrn, 0x01, 0xa0, 0x01, SIS6326_TVCVBS);    /* 0x04 */
    pSiS->SiS6326Flags |= result;
    temp = SiS6326GetTVReg(pScrn, 0x43);
    temp &= 0xfb;
    SiS6326SetTVReg(pScrn, 0x43, temp);
    if(pSiS->SiS6326Flags & (SIS6326_TVSVIDEO | SIS6326_TVCVBS)) {
    	pSiS->SiS6326Flags |= SIS6326_TVDETECTED;
	xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"SiS6326: Detected TV connected to %s output\n",
		(pSiS->SiS6326Flags & SIS6326_TVSVIDEO) ?
			"SVIDEO" : "COMPOSITE");
    } else {
        xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		"SiS6326: No TV detected\n");
    }
}

/* TW: Detect video bridge and set VBFlags accordingly */
void SISVGAPreInit(ScrnInfoPtr pScrn)
{
    SISPtr  pSiS = SISPTR(pScrn);
    int     temp,temp1,temp2;
    int     upperlimitlvds, lowerlimitlvds;
    int     upperlimitch, lowerlimitch;
    int     chronteltype, chrontelidreg;
    static const char  *ChrontelTypeStr[] = {
        "7004",
	"7005",
	"7007",
	"7006",
	"7008",
	"7013",
	"7019",
	"7020",
	"(unknown)"
    };

    switch (pSiS->Chipset) {
        case PCI_CHIP_SIS300:
        case PCI_CHIP_SIS630:
        case PCI_CHIP_SIS540:
	case PCI_CHIP_SIS550:
	case PCI_CHIP_SIS650:
	case PCI_CHIP_SIS315:
	case PCI_CHIP_SIS315H:
	case PCI_CHIP_SIS315PRO:
	case PCI_CHIP_SIS330:
            pSiS->ModeInit = SIS300Init;
            break;
        default:
            pSiS->ModeInit = SISInit;
    }

    if((pSiS->Chipset == PCI_CHIP_SIS6326) && (pSiS->SiS6326Flags & SIS6326_HASTV)) {
        unsigned char sr0d;
	inSISIDXREG(SISSR, 0x0d, sr0d);
	if(sr0d & 0x04) {
		pSiS->SiS6326Flags |= SIS6326_TVPAL;
	}
	SISSense6326(pScrn);
    }

    pSiS->VBFlags = 0; /* reset VBFlags */

    /* TW: Videobridges only available for 300/310/325 series */
    if((pSiS->VGAEngine != SIS_300_VGA) && (pSiS->VGAEngine != SIS_315_VGA))
        return;

    inSISIDXREG(SISPART4, 0x00, temp);
    temp &= 0x0F;
    if (temp == 1) {
        inSISIDXREG(SISPART4, 0x01, temp1);
	temp1 &= 0xff;
	if (temp1 >= 0xE0) {
	   	pSiS->VBFlags |= VB_30xLVX;
		pSiS->sishw_ext.ujVBChipID = VB_CHIP_301LVX;
    		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		                "Detected SiS301LVX video bridge (Revision 0x%x)\n",
				temp1);
	} else if (temp1 >= 0xD0) {
	   	pSiS->VBFlags |= VB_30xLV;
		pSiS->sishw_ext.ujVBChipID = VB_CHIP_301LV;
    		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		                "Detected SiS301LV video bridge (Revision 0x%x)\n",
				temp1);
	} else if (temp1 >= 0xB0) {
	        pSiS->VBFlags |= VB_301B;
		pSiS->sishw_ext.ujVBChipID = VB_CHIP_301B;
    		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		                "Detected SiS301B video bridge (Revision 0x%x)\n",
				temp1);
	} else {
	        pSiS->VBFlags |= VB_301;
		pSiS->sishw_ext.ujVBChipID = VB_CHIP_301;
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		                "Detected SiS301 video bridge (Revision 0x%x)\n",
				temp1);
	}
	if (pSiS->VBFlags & (VB_30xLV | VB_30xLVX)) {
	   inSISIDXREG(SISCR, 0x38, temp);
	   if((temp & 0x03) == 0x03) {
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		                "[SiS301LV/LVX: LCD channel A]\n");
	   }
	}

	SISSense30x(pScrn);

    } else if (temp == 2) {

        inSISIDXREG(SISPART4, 0x01, temp1);
	temp1 &= 0xff;
	if (temp1 >= 0xE0) {
        	pSiS->VBFlags |= VB_30xLVX;
		pSiS->sishw_ext.ujVBChipID = VB_CHIP_302LVX;
    		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		                "Detected SiS302LVX video bridge (Revision 0x%x)\n",
				temp1);
	} else if (temp1 >= 0xD0) {
        	pSiS->VBFlags |= VB_30xLV;
		pSiS->sishw_ext.ujVBChipID = VB_CHIP_302LV;
    		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		                "Detected SiS302LV video bridge (Revision 0x%x)\n",
				temp1);
	} else {
	        pSiS->VBFlags |= VB_302B;
		pSiS->sishw_ext.ujVBChipID = VB_CHIP_302B;
    		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		                "Detected SiS302B video bridge (Revision 0x%x)\n",
				temp1);
	}
	if (pSiS->VBFlags & (VB_302B | VB_30xLV | VB_30xLVX)) {
	   inSISIDXREG(SISCR, 0x38, temp);
	   if((temp & 0x03) == 0x03) {
		xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		                "[SiS302B/LV/LVX: LCD channel A]\n");
	   }
	}

	SISSense30x(pScrn);

    } else if (temp == 3) {

        pSiS->VBFlags |= VB_303;
	pSiS->sishw_ext.ujVBChipID = VB_CHIP_303;
    	xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	           "Detected SiS303 video bridge\n");

    } else {

        pSiS->sishw_ext.ujVBChipID = VB_CHIP_UNKNOWN;
	inSISIDXREG(SISCR, 0x37, temp);
        temp = (temp >> 1) & 0x07;
#if 0   /* TW: This does not seem to be used on any machine */
	if ( (temp == 0) || (temp == 1)) {
	    pSiS->VBFlags|=VB_301;   /* TW: 301 ? */
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	               "Detected SiS301 video bridge (Irregular bridge type %d)\n", temp);
	}
#endif
        if(pSiS->VGAEngine == SIS_300_VGA) {
	   lowerlimitlvds = 2; upperlimitlvds = 4;
	   lowerlimitch   = 4; upperlimitch   = 5;
	   chronteltype = 1;   chrontelidreg  = 0x25;
        } else {
	   lowerlimitlvds = 2; upperlimitlvds = 3;
	   lowerlimitch   = 3; upperlimitch   = 3;
	   chronteltype = 2;   chrontelidreg  = 0x4b;
	}

	if((temp >= lowerlimitlvds) && (temp <= upperlimitlvds)) {
               pSiS->VBFlags |= VB_LVDS;
    	       xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	               "Detected LVDS transmitter (Bridge type %d)\n", temp);
	       if(pSiS->Chipset == PCI_CHIP_SIS650) {
	       	   inSISIDXREG(SISCR, 0x38, temp1);
                   if(temp1 & 0x02) {
		      xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		                 "[LVDS: LCD channel A]\n");
		   }
		   if(temp1 & 0x08) {
		      xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		                 "[LVDS: HDTV]\n");
		   }
		   if(temp1 & 0x08) {
		      xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		                 "[LVDS: SCART]\n");
		   }
	       }
	}
        if((temp >= lowerlimitch) && (temp <= upperlimitch))  {
	    /* TW: Set global for init301.c */
	    pSiS->SiS_Pr->SiS_IF_DEF_CH70xx = chronteltype;

	    if(chronteltype == 1) {
	       /* TW: Do something mysterious (found in Mitac BIOS) */
	       SiS_WhatIsThis(pSiS->SiS_Pr, 0x9c);
	    }

	    /* TW: Read Chrontel version number */
 	    temp1 = SiS_GetCH70xx(pSiS->SiS_Pr, chrontelidreg);
	    if(chronteltype == 1) {
	        /* TW: See Chrontel TB31 for explanation */
		temp2 = SiS_GetCH700x(pSiS->SiS_Pr, 0x0e);
		if(((temp2 & 0x07) == 0x01) || (temp & 0x04)) {
		    SiS_SetCH700x(pSiS->SiS_Pr, 0x0b0e);
		    SiS_DDC2Delay(pSiS->SiS_Pr, 300);
		}
	        temp2 = SiS_GetCH70xx(pSiS->SiS_Pr, chrontelidreg);
		if(temp2 != temp1) temp1 = temp2;
	    }
	    if(temp1 == 0xFFFF) {	/* 0xFFFF = error reading DDC port */
	    	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Detected Chrontel 70xx, but encountered error reading I2C port\n");
	    }
	    /* TW: We only support device ids 0x19-200; other values may indicate DDC problems */
	    else if((temp1 >= 0x19) && (temp1 <= 200)) {
	        pSiS->VBFlags |= VB_CHRONTEL;
		switch (temp1) {
		   case 0x32: temp2 = 0; pSiS->ChrontelType = CHRONTEL_700x; break;
		   case 0x3A: temp2 = 1; pSiS->ChrontelType = CHRONTEL_700x; break;
		   case 0x50: temp2 = 2; pSiS->ChrontelType = CHRONTEL_700x; break;
		   case 0x2A: temp2 = 3; pSiS->ChrontelType = CHRONTEL_700x; break;
		   case 0x40: temp2 = 4; pSiS->ChrontelType = CHRONTEL_700x; break;
		   case 0x22: temp2 = 5; pSiS->ChrontelType = CHRONTEL_700x; break;
	           case 0x19: temp2 = 6; pSiS->ChrontelType = CHRONTEL_701x; break;
	           case 0x20: temp2 = 7; pSiS->ChrontelType = CHRONTEL_701x; break;  /* ID for 7020? */
		   default:   temp2 = 8; pSiS->ChrontelType = CHRONTEL_701x; break;
		}
   	        xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	               "Detected Chrontel %s TV encoder (ID 0x%02x; bridge type %d)\n",
		       			ChrontelTypeStr[temp2], temp1, temp);

		/* TW: Sense connected TV's */

		if(chronteltype == 1) {

		   /* Chrontel 700x */

	    	   /* TW: Read power status */
	    	   temp1 = SiS_GetCH700x(pSiS->SiS_Pr, 0x0e);  /* Power status */
	    	   if((temp1 & 0x03) != 0x03) {
		        /* TW: Power all outputs */
	        	SiS_SetCH700x(pSiS->SiS_Pr, 0x0B0E);
			SiS_DDC2Delay(pSiS->SiS_Pr, 0x96);
	    	   }
		   /* TW: Sense connected TV devices */
	    	   SiS_SetCH700x(pSiS->SiS_Pr, 0x0110);
		   SiS_DDC2Delay(pSiS->SiS_Pr, 0x96);
	    	   SiS_SetCH700x(pSiS->SiS_Pr, 0x0010);
		   SiS_DDC2Delay(pSiS->SiS_Pr, 0x96);
	    	   temp1 = SiS_GetCH700x(pSiS->SiS_Pr, 0x10);
		   if(!(temp1 & 0x08))       temp1 = 0x02;
		   else if(!(temp1 & 0x02))  temp1 = 0x01;
		   else                      temp1 = 0;

		} else {

		   /* Chrontel 701x */

		   /* TW: Backup Power register */
		   temp1 = SiS_GetCH701x(pSiS->SiS_Pr, 0x49);

		   /* TW: Enable TV path */
		   SiS_SetCH701x(pSiS->SiS_Pr, 0x2049);

		   SiS_DDC2Delay(pSiS->SiS_Pr, 0x96);

		   /* TW: Sense connected TV devices */
		   temp2 = SiS_GetCH701x(pSiS->SiS_Pr, 0x20);
		   temp2 |= 0x01;
		   SiS_SetCH701x(pSiS->SiS_Pr, (temp2 << 8) | 0x20);

		   SiS_DDC2Delay(pSiS->SiS_Pr, 0x96);

		   temp2 ^= 0x01;
		   SiS_SetCH701x(pSiS->SiS_Pr, (temp2 << 8) | 0x20);

		   SiS_DDC2Delay(pSiS->SiS_Pr, 0x96);

		   temp2 = SiS_GetCH701x(pSiS->SiS_Pr, 0x20); 

		   /* TW: Restore Power register */
		   SiS_SetCH701x(pSiS->SiS_Pr, (temp1 << 8) | 0x49);

                   temp1 = 0;
		   if(temp2 & 0x02) temp1 |= 0x01;
		   if(temp2 & 0x10) temp1 |= 0x01;
		   if(temp2 & 0x04) temp1 |= 0x02;

		   if( (temp1 & 0x01) && (temp1 & 0x02) ) temp1 = 0x04;

                }

		switch(temp1) {
		     case 0x01:
		        xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		  	   "Chrontel: Detected TV connected to COMPOSITE output\n");
		        /* TW: So we can be sure that there IS a CVBS output */
			pSiS->VBFlags |= TV_AVIDEO;
			orSISIDXREG(SISCR, 0x32, 0x01);
			pSiS->postVBCR32 |= 0x01;
                        break;
                     case 0x02:
			xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			   "Chrontel: Detected TV connected to SVIDEO output\n");
			/* TW: So we can be sure that there IS a SVIDEO output */
			pSiS->VBFlags |= TV_SVIDEO;
			orSISIDXREG(SISCR, 0x32, 0x02);
			pSiS->postVBCR32 |= 0x02;
                        break;
		     case 0x04:
			xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
			   "Chrontel: Detected TV connected to SCART output or 480i HDTV\n");
			if(pSiS->chtvtype == -1) {
			   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			      "Chrontel: Use CHTVType option to select either SCART or HDTV\n");
			   xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			      "Chrontel: Using SCART by default\n");
			   pSiS->chtvtype = 1;
			}
			if(pSiS->chtvtype)
			    pSiS->VBFlags |= TV_CHSCART;
			else
			    pSiS->VBFlags |= TV_CHHDTV;
                        break;
		     default:
		        xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
		  	   "Chrontel: No TV detected.\n");
		}

	    } else if(temp1==0) {
	        /* TW: This indicates a communication problem, but it only occures if there
		 *     is no TV attached.
		 */
	    	xf86DrvMsg(pScrn->scrnIndex, X_INFO,
			"Detected Chrontel TV encoder in promiscuous state (DDC/I2C mix-up)\n");
	    } else {
	    	xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
			"Chrontel: Unsupported device id (%d) detected\n",temp1);
	    }
	    if(chronteltype == 1) {
	       /* TW: Do something mysterious (found in Mitac BIOS) */
	       SiS_WhatIsThis(pSiS->SiS_Pr, 0x00);
	    }
	}
	if ((pSiS->VGAEngine == SIS_300_VGA) && (temp == 3)) {
	    pSiS->VBFlags |= VB_TRUMPION;
	    xf86DrvMsg(pScrn->scrnIndex, X_PROBED,
	               "Detected Trumpion Zurac (I/II/III) LVDS scaler\n");
	}
	if (temp > upperlimitlvds) {
	    xf86DrvMsg(pScrn->scrnIndex, X_ERROR,
	               "Detected unknown bridge type (%d)\n", temp);
	}
    }
}


