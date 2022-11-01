/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/chips/ct_video.c,v 1.12 2002/11/25 14:04:58 eich Exp $ */

#include "xf86.h"
#include "xf86_OSproc.h"
#include "xf86Resources.h"
#include "xf86_ansic.h"
#include "compiler.h"
#include "xf86PciInfo.h"
#include "xf86Pci.h"
#include "xf86fbman.h"
#include "regionstr.h"

#include "ct_driver.h"
#include "Xv.h"
#include "xaa.h"
#include "xaalocal.h"
#include "dixstruct.h"
#include "fourcc.h"

#define OFF_DELAY 	200  /* milliseconds */
#define FREE_DELAY 	60000

#define OFF_TIMER 	0x01
#define FREE_TIMER	0x02
#define CLIENT_VIDEO_ON	0x04

#define TIMER_MASK      (OFF_TIMER | FREE_TIMER)

#ifndef XvExtension
void CHIPSInitVideo(ScreenPtr pScreen) {}
void CHIPSResetVideo(ScrnInfoPtr pScrn) {}
#else

static XF86VideoAdaptorPtr CHIPSSetupImageVideo(ScreenPtr);
static void CHIPSInitOffscreenImages(ScreenPtr);
static void CHIPSStopVideo(ScrnInfoPtr, pointer, Bool);
static int CHIPSSetPortAttribute(ScrnInfoPtr, Atom, INT32, pointer);
static int CHIPSGetPortAttribute(ScrnInfoPtr, Atom ,INT32 *, pointer);
static void CHIPSQueryBestSize(ScrnInfoPtr, Bool,
	short, short, short, short, unsigned int *, unsigned int *, pointer);
static int CHIPSPutImage( ScrnInfoPtr, 
	short, short, short, short, short, short, short, short,
	int, unsigned char*, short, short, Bool, RegionPtr, pointer);
static int CHIPSQueryImageAttributes(ScrnInfoPtr, 
	int, unsigned short *, unsigned short *,  int *, int *);
static void CHIPSVideoTimerCallback(ScrnInfoPtr pScrn, Time time);


#define MAKE_ATOM(a) MakeAtom(a, sizeof(a) - 1, TRUE)

static Atom xvColorKey;

void 
CHIPSInitVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    XF86VideoAdaptorPtr *adaptors, *newAdaptors = NULL;
    XF86VideoAdaptorPtr newAdaptor = NULL;
    CHIPSPtr cPtr = CHIPSPTR(pScrn);
    int num_adaptors;
	
    if (!(cPtr->Flags & ChipsOverlay8plus16) &&
       (cPtr->Flags & ChipsVideoSupport)) {
	newAdaptor = CHIPSSetupImageVideo(pScreen);
	CHIPSInitOffscreenImages(pScreen);
    }

    num_adaptors = xf86XVListGenericAdaptors(pScrn, &adaptors);

    if(newAdaptor) {
	if(!num_adaptors) {
	    num_adaptors = 1;
	    adaptors = &newAdaptor;
	} else {
	    newAdaptors =  /* need to free this someplace */
		xalloc((num_adaptors + 1) * sizeof(XF86VideoAdaptorPtr*));
	    if(newAdaptors) {
		memcpy(newAdaptors, adaptors, num_adaptors * 
					sizeof(XF86VideoAdaptorPtr));
		newAdaptors[num_adaptors] = newAdaptor;
		adaptors = newAdaptors;
		num_adaptors++;
	    }
	}
    }

    if(num_adaptors)
        xf86XVScreenInit(pScreen, adaptors, num_adaptors);

    if(newAdaptors)
	xfree(newAdaptors);
}

/* client libraries expect an encoding */
static 
XF86VideoEncodingRec DummyEncoding[1] =
{
 {
   0,
   "XV_IMAGE",
   1024, 1024,
   {1, 1}
 }
};

#define NUM_FORMATS 4

static XF86VideoFormatRec Formats[NUM_FORMATS] = 
{
  {8, PseudoColor},  {15, TrueColor}, {16, TrueColor}, {24, TrueColor}
};

#define NUM_ATTRIBUTES 1

static XF86AttributeRec Attributes[NUM_ATTRIBUTES] =
{
   {XvSettable | XvGettable, 0, (1 << 24) - 1, "XV_COLORKEY"}
};

#define NUM_IMAGES 4

static XF86ImageRec Images[NUM_IMAGES] =
{
   {
	0x35315652,
        XvRGB,
	LSBFirst,
	{'R','V','1','5',
	  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	16,
	XvPacked,
	1,
	15, 0x7C00, 0x03E0, 0x001F,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	{'R','V','B',0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   },
   {
	0x36315652,
        XvRGB,
	LSBFirst,
	{'R','V','1','6',
	  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
	16,
	XvPacked,
	1,
	16, 0xF800, 0x07E0, 0x001F,
	0, 0, 0,
	0, 0, 0,
	0, 0, 0,
	{'R','V','B',0,
	  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
	XvTopToBottom
   },
   XVIMAGE_YV12,
   XVIMAGE_YUY2
};

typedef struct {
   FBLinearPtr	linear;
   RegionRec	clip;
   CARD32	colorKey;
   CARD32	videoStatus;
   Time		offTime;
   Time		freeTime;
   Bool         doubleBuffer;
   Bool         manualDoubleBuffer;
   int          currentBuffer;
} CHIPSPortPrivRec, *CHIPSPortPrivPtr;


#define GET_PORT_PRIVATE(pScrn) \
   (CHIPSPortPrivPtr)((CHIPSPTR(pScrn))->adaptor->pPortPrivates[0].ptr)

void 
CHIPSResetVideo(ScrnInfoPtr pScrn) 
{
    CHIPSPtr cPtr = CHIPSPTR(pScrn);
    CHIPSPortPrivPtr pPriv = cPtr->adaptor->pPortPrivates[0].ptr;
    unsigned char mr3c;
    int red, green, blue;
    
    if (cPtr->Flags & ChipsAccelSupport) 
	CHIPSHiQVSync(pScrn);

    mr3c = cPtr->readMR(cPtr, 0x3C);
    cPtr->writeMR(cPtr, 0x3C, (mr3c | 0x6));
    switch (pScrn->depth) {
    case 8:
	cPtr->writeMR(cPtr, 0x3D, 0x00);
	cPtr->writeMR(cPtr, 0x3E, 0x00);
	cPtr->writeMR(cPtr, 0x3F, (pPriv->colorKey & 0xFF));
	cPtr->writeMR(cPtr, 0x40, 0xFF);
	cPtr->writeMR(cPtr, 0x41, 0xFF);
	cPtr->writeMR(cPtr, 0x42, 0x00);
	break;
    default:
	red = (pPriv->colorKey & pScrn->mask.red) >> pScrn->offset.red;
	green = (pPriv->colorKey & pScrn->mask.green) >> pScrn->offset.green;
	blue = (pPriv->colorKey & pScrn->mask.blue) >> pScrn->offset.blue;
	switch (pScrn->depth) {
	case 15:
	    cPtr->writeMR(cPtr, 0x3D, (red << 3));
	    cPtr->writeMR(cPtr, 0x3E, (green << 3));
	    cPtr->writeMR(cPtr, 0x3F, (blue << 3));
	    cPtr->writeMR(cPtr, 0x40, 0x07);
	    cPtr->writeMR(cPtr, 0x41, 0x07);
	    cPtr->writeMR(cPtr, 0x42, 0x07);
	    break;
	case 16:
	    cPtr->writeMR(cPtr, 0x3D, (red << 3));
	    cPtr->writeMR(cPtr, 0x3E, (green << 2));
	    cPtr->writeMR(cPtr, 0x3F, (blue << 3));
	    cPtr->writeMR(cPtr, 0x40, 0x07);
	    cPtr->writeMR(cPtr, 0x41, 0x03);
	    cPtr->writeMR(cPtr, 0x42, 0x07);
	    break;
	case 24:
	    cPtr->writeMR(cPtr, 0x3D, red);
	    cPtr->writeMR(cPtr, 0x3E, green);
	    cPtr->writeMR(cPtr, 0x3F, blue);
	    cPtr->writeMR(cPtr, 0x40, 0x00);
	    cPtr->writeMR(cPtr, 0x41, 0x00);
	    cPtr->writeMR(cPtr, 0x42, 0x00);
	    break;
	}    
    }    
}


static XF86VideoAdaptorPtr 
CHIPSSetupImageVideo(ScreenPtr pScreen)
{
    ScrnInfoPtr pScrn = xf86Screens[pScreen->myNum];
    CHIPSPtr cPtr = CHIPSPTR(pScrn);
    XF86VideoAdaptorPtr adapt;
    CHIPSPortPrivPtr pPriv;

    if(!(adapt = xcalloc(1, sizeof(XF86VideoAdaptorRec) +
			    sizeof(CHIPSPortPrivRec) +
			    sizeof(DevUnion))))
	return NULL;

    adapt->type = XvWindowMask | XvInputMask | XvImageMask;
    adapt->flags = VIDEO_OVERLAID_IMAGES | VIDEO_CLIP_TO_VIEWPORT;
    adapt->name = "Chips and Technologies Backend Scaler";
    adapt->nEncodings = 1;
    adapt->pEncodings = DummyEncoding;
    adapt->nFormats = NUM_FORMATS;
    adapt->pFormats = Formats;
    adapt->nPorts = 1;
    adapt->pPortPrivates = (DevUnion*)(&adapt[1]);
    pPriv = (CHIPSPortPrivPtr)(&adapt->pPortPrivates[1]);
    adapt->pPortPrivates[0].ptr = (pointer)(pPriv);
    adapt->pAttributes = Attributes;
    adapt->nImages = NUM_IMAGES;
    adapt->nAttributes = NUM_ATTRIBUTES;
    adapt->pImages = Images;
    adapt->PutVideo = NULL;
    adapt->PutStill = NULL;
    adapt->GetVideo = NULL;
    adapt->GetStill = NULL;
    adapt->StopVideo = CHIPSStopVideo;
    adapt->SetPortAttribute = CHIPSSetPortAttribute;
    adapt->GetPortAttribute = CHIPSGetPortAttribute;
    adapt->QueryBestSize = CHIPSQueryBestSize;
    adapt->PutImage = CHIPSPutImage;
    adapt->QueryImageAttributes = CHIPSQueryImageAttributes;

    pPriv->colorKey = cPtr->videoKey;
    pPriv->videoStatus = 0;
    pPriv->doubleBuffer = TRUE;
    pPriv->manualDoubleBuffer = FALSE;
    pPriv->currentBuffer	= 0;

    /* gotta uninit this someplace */
    REGION_INIT(pScreen, &pPriv->clip, NullBox, 0); 

    cPtr->adaptor = adapt;

    xvColorKey   = MAKE_ATOM("XV_COLORKEY");

    CHIPSResetVideo(pScrn);

    return adapt;
}


static Bool
RegionsEqual(RegionPtr A, RegionPtr B)
{
    int *dataA, *dataB;
    int num;

    num = REGION_NUM_RECTS(A);
    if(num != REGION_NUM_RECTS(B))
	return FALSE;

    if((A->extents.x1 != B->extents.x1) ||
       (A->extents.x2 != B->extents.x2) ||
       (A->extents.y1 != B->extents.y1) ||
       (A->extents.y2 != B->extents.y2))
	return FALSE;

    dataA = (int*)REGION_RECTS(A);
    dataB = (int*)REGION_RECTS(B);

    while(num--) {
	if((dataA[0] != dataB[0]) || (dataA[1] != dataB[1]))
	   return FALSE;
	dataA += 2; 
	dataB += 2;
    }

    return TRUE;
}


/* CHIPSClipVideo -  

   Takes the dst box in standard X BoxRec form (top and left
   edges inclusive, bottom and right exclusive).  The new dst
   box is returned.  The source boundaries are given (x1, y1 
   inclusive, x2, y2 exclusive) and returned are the new source 
   boundaries in 16.16 fixed point.
*/

static Bool
CHIPSClipVideo(
  BoxPtr dst, 
  INT32 *x1, 
  INT32 *x2, 
  INT32 *y1, 
  INT32 *y2,
  RegionPtr reg,
  INT32 width, 
  INT32 height
){
    INT32 vscale, hscale, delta;
    BoxPtr extents = REGION_EXTENTS(DummyScreen, reg);
    int diff;

    hscale = ((*x2 - *x1) << 16) / (dst->x2 - dst->x1);
    vscale = ((*y2 - *y1) << 16) / (dst->y2 - dst->y1);

    *x1 <<= 16; *x2 <<= 16;
    *y1 <<= 16; *y2 <<= 16;

    diff = extents->x1 - dst->x1;
    if(diff > 0) {
	dst->x1 = extents->x1;
	*x1 += diff * hscale;     
    }
    diff = dst->x2 - extents->x2;
    if(diff > 0) {
	dst->x2 = extents->x2;
	*x2 -= diff * hscale;     
    }
    diff = extents->y1 - dst->y1;
    if(diff > 0) {
	dst->y1 = extents->y1;
	*y1 += diff * vscale;     
    }
    diff = dst->y2 - extents->y2;
    if(diff > 0) {
	dst->y2 = extents->y2;
	*y2 -= diff * vscale;     
    }

    if(*x1 < 0) {
	diff =  (- *x1 + hscale - 1)/ hscale;
	dst->x1 += diff;
	*x1 += diff * hscale;
    }
    delta = *x2 - (width << 16);
    if(delta > 0) {
	diff = (delta + hscale - 1)/ hscale;
	dst->x2 -= diff;
	*x2 -= diff * hscale;
    }
    if(*x1 >= *x2) return FALSE;

    if(*y1 < 0) {
	diff =  (- *y1 + vscale - 1)/ vscale;
	dst->y1 += diff;
	*y1 += diff * vscale;
    }
    delta = *y2 - (height << 16);
    if(delta > 0) {
	diff = (delta + vscale - 1)/ vscale;
	dst->y2 -= diff;
	*y2 -= diff * vscale;
    }
    if(*y1 >= *y2) return FALSE;

    if((dst->x1 != extents->x1) || (dst->x2 != extents->x2) ||
       (dst->y1 != extents->y1) || (dst->y2 != extents->y2))
    {
	RegionRec clipReg;
	REGION_INIT(DummyScreen, &clipReg, dst, 1);
	REGION_INTERSECT(DummyScreen, reg, reg, &clipReg);
	REGION_UNINIT(DummyScreen, &clipReg);
    }
    return TRUE;
} 

static void 
CHIPSStopVideo(ScrnInfoPtr pScrn, pointer data, Bool shadow)
{
  CHIPSPortPrivPtr pPriv = (CHIPSPortPrivPtr)data;
  CHIPSPtr cPtr = CHIPSPTR(pScrn);
  unsigned char mr3c, tmp;

  REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   
  if (cPtr->Flags & ChipsAccelSupport) 
      CHIPSHiQVSync(pScrn);
  if(shadow) {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
	mr3c = cPtr->readMR(cPtr, 0x3C);
	cPtr->writeMR(cPtr, 0x3C, (mr3c & 0xFE));
	tmp = cPtr->readXR(cPtr, 0xD0);
	cPtr->writeXR(cPtr, 0xD0, (tmp & 0xf));
     }
     if(pPriv->linear) {
	xf86FreeOffscreenLinear(pPriv->linear);
	pPriv->linear = NULL;
     }
     pPriv->videoStatus = 0;
  } else {
     if(pPriv->videoStatus & CLIENT_VIDEO_ON) {
	pPriv->videoStatus |= OFF_TIMER;
	pPriv->offTime = currentTime.milliseconds + OFF_DELAY; 
	cPtr->VideoTimerCallback = CHIPSVideoTimerCallback;
     }
  }
}

static int 
CHIPSSetPortAttribute(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 value, 
  pointer data
){
  CHIPSPortPrivPtr pPriv = (CHIPSPortPrivPtr)data;
  CHIPSPtr cPtr = CHIPSPTR(pScrn);

  if (cPtr->Flags & ChipsAccelSupport) 
      CHIPSHiQVSync(pScrn);
  if(attribute == xvColorKey) {
	int red, green, blue;
	pPriv->colorKey = value;
	switch (pScrn->depth) {
	case 8:
	    cPtr->writeMR(cPtr, 0x3D, 0x00);
	    cPtr->writeMR(cPtr, 0x3E, 0x00);
	    cPtr->writeMR(cPtr, 0x3F, (pPriv->colorKey & 0xFF));
	    break;
	default:
	    red = (pPriv->colorKey & pScrn->mask.red) >> pScrn->offset.red;
	    green = (pPriv->colorKey & pScrn->mask.green) >> pScrn->offset.green;
	    blue = (pPriv->colorKey & pScrn->mask.blue) >> pScrn->offset.blue;
	    switch (pScrn->depth) {
	    case 15:
		cPtr->writeMR(cPtr, 0x3D, (red << 3));
		cPtr->writeMR(cPtr, 0x3E, (green << 3));
		cPtr->writeMR(cPtr, 0x3F, (blue << 3));
		break;
	    case 16:
		cPtr->writeMR(cPtr, 0x3D, (red << 3));
		cPtr->writeMR(cPtr, 0x3E, (green << 2));
		cPtr->writeMR(cPtr, 0x3F, (blue << 3));
		break;
	    case 24:
		cPtr->writeMR(cPtr, 0x3D, red);
		cPtr->writeMR(cPtr, 0x3E, green);
		cPtr->writeMR(cPtr, 0x3F, blue);
		break;
	    }    
	}    
	REGION_EMPTY(pScrn->pScreen, &pPriv->clip);   
  } else return BadMatch;

  return Success;
}

static int 
CHIPSGetPortAttribute(
  ScrnInfoPtr pScrn, 
  Atom attribute,
  INT32 *value, 
  pointer data
){
  CHIPSPortPrivPtr pPriv = (CHIPSPortPrivPtr)data;

  if(attribute == xvColorKey) {
	*value = pPriv->colorKey;
  } else return BadMatch;

  return Success;
}

static void 
CHIPSQueryBestSize(
  ScrnInfoPtr pScrn, 
  Bool motion,
  short vid_w, short vid_h, 
  short drw_w, short drw_h, 
  unsigned int *p_w, unsigned int *p_h, 
  pointer data
){
  *p_w = drw_w;
  *p_h = drw_h; 

  if(*p_w > 16384) *p_w = 16384;
}


static void
CHIPSCopyData(
  unsigned char *src,
  unsigned char *dst,
  int srcPitch,
  int dstPitch,
  int h,
  int w
){
    w <<= 1;
    while(h--) {
	memcpy(dst, src, w);
	src += srcPitch;
	dst += dstPitch;
    }
}

static void
CHIPSCopyMungedData(
   unsigned char *src1,
   unsigned char *src2,
   unsigned char *src3,
   unsigned char *dst1,
   int srcPitch,
   int srcPitch2,
   int dstPitch,
   int h,
   int w
){
   CARD32 *dst = (CARD32*)dst1;
   int i, j;

   dstPitch >>= 2;
   w >>= 1;

   for(j = 0; j < h; j++) {
	for(i = 0; i < w; i++) {
	    dst[i] = src1[i << 1] | (src1[(i << 1) + 1] << 16) |
		     (src3[i] << 8) | (src2[i] << 24);
	}
	dst += dstPitch;
	src1 += srcPitch;
	if(j & 1) {
	    src2 += srcPitch2;
	    src3 += srcPitch2;
	}
   }
}

static FBLinearPtr
CHIPSAllocateMemory(
   ScrnInfoPtr pScrn,
   FBLinearPtr linear,
   int size
){
   ScreenPtr pScreen;
   FBLinearPtr new_linear;

   if(linear) {
	if(linear->size >= size) 
	   return linear;
        
        if(xf86ResizeOffscreenLinear(linear, size))
	   return linear;

	xf86FreeOffscreenLinear(linear);
   }

   pScreen = screenInfo.screens[pScrn->scrnIndex];

   new_linear = xf86AllocateOffscreenLinear(pScreen, size, 16, 
   						NULL, NULL, NULL);

   if(!new_linear) {
	int max_size;

	xf86QueryLargestOffscreenLinear(pScreen, &max_size, 16, 
						PRIORITY_EXTREME);
	
	if(max_size < size)
	   return NULL;

	xf86PurgeUnlockedOffscreenAreas(pScreen);
	new_linear = xf86AllocateOffscreenLinear(pScreen, size, 16, 
						NULL, NULL, NULL);
   }

   return new_linear;
}

static int
CHIPSSetCurrentPlaybackBuffer(CHIPSPtr cPtr, int n)
{
  
    CARD8 mr20;
    mr20 = cPtr->readMR(cPtr, 0x20);
    mr20 &= ~0x1B;
    if (!n) mr20 |= 0x10;
    cPtr->writeMR(cPtr, 0x22, mr20);
    return n;
}

static int
CHIPSWaitGetNextFrame(CHIPSPtr cPtr)
{
    volatile CARD8 mr20;
    volatile CARD8 mr21;
    
    mr20 = cPtr->readMR(cPtr, 0x20);
    while (1) {
      mr21 = cPtr->readMR(cPtr, 0x21);
      if (!(mr20 & (1 << 5)) || !(mr21 & 1))  
	break;
    }
    mr20 &= ~0x4;
    mr20 = cPtr->readMR(cPtr, 0x20);
    return (mr21 & 2)? 0 : 1;
}

static void
CHIPSDisplayVideo(
    ScrnInfoPtr pScrn,
    int id,
    int offset,
    short width, short height,
    int pitch, 
    int x1, int y1, int x2, int y2,
    BoxPtr dstBox,
    short src_w, short src_h,
    short drw_w, short drw_h,
    Bool triggerBufSwitch
){
    CHIPSPtr cPtr = CHIPSPTR(pScrn);
    CHIPSPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);
    DisplayModePtr mode = pScrn->currentMode;
    unsigned char tmp, m1f, m1e;
    int buffer = pPriv->currentBuffer;

    if (cPtr->Flags & ChipsAccelSupport) 
	CHIPSHiQVSync(pScrn);

    tmp = cPtr->readXR(cPtr, 0xD0);
    cPtr->writeXR(cPtr, 0xD0, (tmp | 0x10));
    
    m1e = cPtr->readMR(cPtr, 0x1E);
    m1e &= 0xE0;		/* Set Zoom and Direction */
    if ((!(cPtr->PanelType & ChipsLCD)) && (mode->Flags & V_INTERLACE)) 
	m1e |= 0x10;

    m1f = cPtr->readMR(cPtr, 0x1F);
    m1f = (m1f & 0x14); /* Mask reserved bits, unset interpolation */
    switch(id) {
    case 0x35315652:		/* RGB15 */
	m1f |= 0x09;
	break;
    case 0x36315652:		/* RGB16 */
	m1f |= 0x08;
	break;
    case FOURCC_YV12:		/* YV12 */
      /* m1f |= 0x03 */
	m1f |= 0x00; 
	break;
    case FOURCC_YUY2:		/* YUY2 */
    default:
	m1f |= 0x00;		/* Do nothing here */
	break;
    }  

    offset += (x1 >> 15) & ~0x01;
    /* Setup Pointer 1 */
    if (!buffer || pPriv->manualDoubleBuffer || !pPriv->doubleBuffer) {
        cPtr->writeMR(cPtr, 0x22, (offset & 0xF8));
	cPtr->writeMR(cPtr, 0x23, ((offset >> 8) & 0xFF));
	cPtr->writeMR(cPtr, 0x24, ((offset >> 16) & 0xFF));
    }
    /* Setup Pointer 2 */
    if ((buffer && !pPriv->manualDoubleBuffer) || !pPriv->doubleBuffer) {
        cPtr->writeMR(cPtr, 0x25, (offset & 0xF8));
	cPtr->writeMR(cPtr, 0x26, ((offset >> 8) & 0xFF));
	cPtr->writeMR(cPtr, 0x27, ((offset >> 16) & 0xFF));
    }


    tmp = cPtr->readMR(cPtr, 0x04);
    if (pPriv->doubleBuffer && !pPriv->manualDoubleBuffer && triggerBufSwitch)
      tmp |= 0x18;
    cPtr->writeMR(cPtr, 0x04, tmp);

    tmp = cPtr->readMR(cPtr, 0x20);
    tmp &= 0xC3;
    if (pPriv->doubleBuffer && !pPriv->manualDoubleBuffer && triggerBufSwitch) 
	tmp |= ((1 << 2  | 1 << 5) | ((buffer) ? (1 << 4) : 0));
    cPtr->writeMR(cPtr, 0x20, tmp);

    cPtr->writeMR(cPtr, 0x28, ((width >> 2) - 1)); /* Width */ 
    cPtr->writeMR(cPtr, 0x34, ((width >> 2) - 1));

    /* Left Edge of Overlay */
    cPtr->writeMR(cPtr, 0x2A, ((cPtr->OverlaySkewX + dstBox->x1) & 0xFF));
    tmp = cPtr->readMR(cPtr, 0x2B);
    tmp = (tmp & 0xF8) + (((cPtr->OverlaySkewX + dstBox->x1) >> 8) & 0x07);
    cPtr->writeMR(cPtr, 0x2B, tmp);
    /* Right Edge of Overlay */
    cPtr->writeMR(cPtr, 0x2C, ((cPtr->OverlaySkewX + dstBox->x2 -1) 
				& 0xFF));
    tmp = cPtr->readMR(cPtr, 0x2D);
    tmp = (tmp & 0xF8) + (((cPtr->OverlaySkewX + dstBox->x2 - 1) >> 8) & 0x07);
    cPtr->writeMR(cPtr, 0x2D, tmp);
    /* Top Edge of Overlay */
    cPtr->writeMR(cPtr, 0x2E, ((cPtr->OverlaySkewY + dstBox->y1) & 0xFF));
    tmp = cPtr->readMR(cPtr, 0x2F);
    tmp = (tmp & 0xF8) + (((cPtr->OverlaySkewY + dstBox->y1) >> 8) & 0x07);
    cPtr->writeMR(cPtr, 0x2F, tmp);
    /* Bottom Edge of Overlay*/
    cPtr->writeMR(cPtr, 0x30, ((cPtr->OverlaySkewY + dstBox->y2 - 1) & 0xFF));
    tmp = cPtr->readMR(cPtr, 0x31);
    tmp = (tmp & 0xF8) + (((cPtr->OverlaySkewY + dstBox->y2 - 1) >> 8) & 0x07);
    cPtr->writeMR(cPtr, 0x31, tmp);

    /* Horizontal Zoom */
    if (drw_w > src_w) {
        m1f = m1f | 0x20; /* set H-interpolation */
	m1e = m1e | 0x04;
	tmp = cPtr->VideoZoomMax * src_w / drw_w;
	cPtr->writeMR(cPtr, 0x32, tmp);
    }

    /* Vertical Zoom */
    if (drw_h > src_h) {
        m1f = m1f | 0x80;
	m1e = m1e | 0x08; /* set V-interpolation */
	tmp = cPtr->VideoZoomMax * src_h / drw_h ;
	cPtr->writeMR(cPtr, 0x33, tmp);
    }
    cPtr->writeMR(cPtr, 0x1F, m1f); 
    cPtr->writeMR(cPtr, 0x1E, m1e);

    tmp = cPtr->readMR(cPtr, 0x3C);
    cPtr->writeMR(cPtr, 0x3C, (tmp | 0x7));
    if (cPtr->Flags & ChipsAccelSupport) 
	CHIPSHiQVSync(pScrn);
}

static int 
CHIPSPutImage( 
  ScrnInfoPtr pScrn, 
  short src_x, short src_y, 
  short drw_x, short drw_y,
  short src_w, short src_h, 
  short drw_w, short drw_h,
  int id, unsigned char* buf, 
  short width, short height, 
  Bool sync,
  RegionPtr clipBoxes, pointer data
){
   CHIPSPortPrivPtr pPriv = (CHIPSPortPrivPtr)data;
   CHIPSPtr cPtr = CHIPSPTR(pScrn);
   INT32 x1, x2, y1, y2;
   unsigned char *dst_start;
   int pitch, new_size, offset, offset2 = 0, offset3 = 0;
   int srcPitch, srcPitch2 = 0, dstPitch;
   int top, left, npixels, nlines, bpp;
   BoxRec dstBox;
   CARD32 tmp;

   if(drw_w > 16384) drw_w = 16384;

   /* Clip */
   x1 = src_x;
   x2 = src_x + src_w;
   y1 = src_y;
   y2 = src_y + src_h;

   dstBox.x1 = drw_x;
   dstBox.x2 = drw_x + drw_w;
   dstBox.y1 = drw_y;
   dstBox.y2 = drw_y + drw_h;
   
   if (!CHIPSClipVideo(&dstBox, &x1, &x2, &y1, &y2, clipBoxes, width, height))
	return Success;

   dstBox.x1 -= pScrn->frameX0 & cPtr->viewportMask;
   dstBox.x2 -= pScrn->frameX0 & cPtr->viewportMask;
   dstBox.y1 -= pScrn->frameY0;
   dstBox.y2 -= pScrn->frameY0;

   bpp = pScrn->bitsPerPixel >> 3;
   pitch = bpp * pScrn->displayWidth;

   dstPitch = ((width << 1) + 15) & ~15;
   new_size = ((dstPitch * height) + bpp - 1) / bpp;
   if (pPriv->doubleBuffer) 
       new_size <<= 1;

   switch(id) {
   case FOURCC_YV12:		/* YV12 */
	srcPitch = (width + 3) & ~3;
	offset2 = srcPitch * height;
	srcPitch2 = ((width >> 1) + 3) & ~3;
	offset3 = (srcPitch2 * (height >> 1)) + offset2;
	break;
   default:			/* RGB15, RGB16, YUY2 */
	srcPitch = (width << 1);
	break;
   }  

   if(!(pPriv->linear = CHIPSAllocateMemory(pScrn, pPriv->linear, new_size))) {
     if (pPriv->doubleBuffer &&
	 (pPriv->linear = CHIPSAllocateMemory(pScrn, pPriv->linear, 
					      new_size >> 1))) {
         new_size >>= 1;
	 pPriv->doubleBuffer = FALSE;
     } else 
	return BadAlloc;
   }

   /* copy data */
   top = y1 >> 16;
   left = (x1 >> 16) & ~1;
   npixels = ((((x2 + 0xffff) >> 16) + 1) & ~1) - left;
   left <<= 1;

   offset = pPriv->linear->offset * bpp;
   if (!pPriv->manualDoubleBuffer)
     pPriv->currentBuffer = CHIPSWaitGetNextFrame(cPtr);
   if(pPriv->doubleBuffer && pPriv->currentBuffer) 
	offset += (new_size * bpp) >> 1;

   dst_start = cPtr->FbBase + offset + left + (top * dstPitch);

   switch(id) {
   case FOURCC_YV12:		/* YV12 */
        top &= ~1;
	tmp = ((top >> 1) * srcPitch2) + (left >> 2);
	offset2 += tmp;
	offset3 += tmp; 
	nlines = ((((y2 + 0xffff) >> 16) + 1) & ~1) - top;
	CHIPSCopyMungedData(buf + (top * srcPitch) + (left >> 1), 
			  buf + offset2, buf + offset3, dst_start,
			  srcPitch, srcPitch2, dstPitch, nlines, npixels);
	break;
   default:			/* RGB15, RGB16, YUY2 */
	buf += (top * srcPitch) + left;
	nlines = ((y2 + 0xffff) >> 16) - top;
	CHIPSCopyData(buf, dst_start, srcPitch, dstPitch, nlines, npixels);
	break;
   }  

   /* update cliplist */
   if(!RegionsEqual(&pPriv->clip, clipBoxes)) {
	REGION_COPY(pScreen, &pPriv->clip, clipBoxes);
        xf86XVFillKeyHelper(pScrn->pScreen, pPriv->colorKey, clipBoxes);
   }

   offset += top * dstPitch;   
   CHIPSDisplayVideo(pScrn, id, offset, width, height, dstPitch,
	     x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h, TRUE);

   pPriv->videoStatus = CLIENT_VIDEO_ON;
   
   if (pPriv->manualDoubleBuffer)
     pPriv->currentBuffer ^= 1;   

   return Success;
}

static int 
CHIPSQueryImageAttributes(
  ScrnInfoPtr pScrn, 
  int id, 
  unsigned short *w, unsigned short *h, 
  int *pitches, int *offsets
){
    int size, tmp;

    if(*w > 1024) *w = 1024;
    if(*h > 1024) *h = 1024;

    *w = (*w + 1) & ~1;
    if(offsets) offsets[0] = 0;

    switch(id) {
    case FOURCC_YV12:		/* YV12 */
	*h = (*h + 1) & ~1;
	size = (*w + 3) & ~3;
	if(pitches) pitches[0] = size;
	size *= *h;
	if(offsets) offsets[1] = size;
	tmp = ((*w >> 1) + 3) & ~3;
	if(pitches) pitches[1] = pitches[2] = tmp;
	tmp *= (*h >> 1);
	size += tmp;
	if(offsets) offsets[2] = size;
	size += tmp;
	break;
    default:			/* RGB15, RGB16, YUY2 */
	size = *w << 1;
	if(pitches) pitches[0] = size;
	size *= *h;
	break;
    }

    return size;
}


static void
CHIPSVideoTimerCallback(ScrnInfoPtr pScrn, Time time)
{
    CHIPSPtr    cPtr = CHIPSPTR(pScrn);
    CHIPSPortPrivPtr pPriv = GET_PORT_PRIVATE(pScrn);
    unsigned char mr3c;

    if(pPriv->videoStatus & TIMER_MASK) {
	if(pPriv->videoStatus & OFF_TIMER) {
	    if(pPriv->offTime < time) {
		if (cPtr->Flags & ChipsAccelSupport) 
		    CHIPSHiQVSync(pScrn);
		mr3c = cPtr->readMR(cPtr, 0x3C);
		cPtr->writeMR(cPtr, 0x3C, (mr3c & 0xFE));
		pPriv->videoStatus = FREE_TIMER;
		pPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
	    }
	} else {  /* FREE_TIMER */
	    if(pPriv->freeTime < time) {
		if(pPriv->linear) {
		   xf86FreeOffscreenLinear(pPriv->linear);
		   pPriv->linear = NULL;
		}
		pPriv->videoStatus = 0;
	        cPtr->VideoTimerCallback = NULL;
	    }
        }
    } else  /* shouldn't get here */
	cPtr->VideoTimerCallback = NULL;
}


/****************** Offscreen stuff ***************/

typedef struct {
  FBLinearPtr linear;
  Bool isOn;
} OffscreenPrivRec, * OffscreenPrivPtr;

static int 
CHIPSAllocateSurface(
    ScrnInfoPtr pScrn,
    int id,
    unsigned short w, 	
    unsigned short h,
    XF86SurfacePtr surface
){
    FBLinearPtr linear;
    int pitch, fbpitch, size, bpp;
    OffscreenPrivPtr pPriv;

    if((w > 1024) || (h > 1024))
	return BadAlloc;

    w = (w + 1) & ~1;
    pitch = ((w << 1) + 15) & ~15;
    bpp = pScrn->bitsPerPixel >> 3;
    fbpitch = bpp * pScrn->displayWidth;
    size = ((pitch * h) + bpp - 1) / bpp;

    if(!(linear = CHIPSAllocateMemory(pScrn, NULL, size)))
	return BadAlloc;

    surface->width = w;
    surface->height = h;

    if(!(surface->pitches = xalloc(sizeof(int)))) {
	xf86FreeOffscreenLinear(linear);
	return BadAlloc;
    }
    if(!(surface->offsets = xalloc(sizeof(int)))) {
	xfree(surface->pitches);
	xf86FreeOffscreenLinear(linear);
	return BadAlloc;
    }
    if(!(pPriv = xalloc(sizeof(OffscreenPrivRec)))) {
	xfree(surface->pitches);
	xfree(surface->offsets);
	xf86FreeOffscreenLinear(linear);
	return BadAlloc;
    }

    pPriv->linear = linear;
    pPriv->isOn = FALSE;

    surface->pScrn = pScrn;
    surface->id = id;   
    surface->pitches[0] = pitch;
    surface->offsets[0] = linear->offset * bpp;
    surface->devPrivate.ptr = (pointer)pPriv;

    return Success;
}

static int 
CHIPSStopSurface(
    XF86SurfacePtr surface
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

    if(pPriv->isOn) {
	CHIPSPtr cPtr = CHIPSPTR(surface->pScrn);
	unsigned char mr3c, tmp;
	tmp = cPtr->readXR(cPtr, 0xD0);
	cPtr->writeXR(cPtr, 0xD0, (tmp & 0xf));
	mr3c = cPtr->readMR(cPtr, 0x3C);
	cPtr->writeMR(cPtr, 0x3C, (mr3c & 0xFE));
	pPriv->isOn = FALSE;
    }

    return Success;
}


static int 
CHIPSFreeSurface(
    XF86SurfacePtr surface
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;

    if(pPriv->isOn)
	CHIPSStopSurface(surface);
    xf86FreeOffscreenLinear(pPriv->linear);
    xfree(surface->pitches);
    xfree(surface->offsets);
    xfree(surface->devPrivate.ptr);

    return Success;
}

static int
CHIPSGetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 *value
){
    return CHIPSGetPortAttribute(pScrn, attribute, value, 
			(pointer)(GET_PORT_PRIVATE(pScrn)));
}

static int
CHIPSSetSurfaceAttribute(
    ScrnInfoPtr pScrn,
    Atom attribute,
    INT32 value
){
    return CHIPSSetPortAttribute(pScrn, attribute, value, 
			(pointer)(GET_PORT_PRIVATE(pScrn)));
}


static int 
CHIPSDisplaySurface(
    XF86SurfacePtr surface,
    short src_x, short src_y, 
    short drw_x, short drw_y,
    short src_w, short src_h, 
    short drw_w, short drw_h,
    RegionPtr clipBoxes
){
    OffscreenPrivPtr pPriv = (OffscreenPrivPtr)surface->devPrivate.ptr;
    ScrnInfoPtr pScrn = surface->pScrn;
    CHIPSPtr cPtr = CHIPSPTR(pScrn);
    CHIPSPortPrivPtr portPriv = GET_PORT_PRIVATE(pScrn);
    INT32 x1, y1, x2, y2;
    BoxRec dstBox;

    x1 = src_x;
    x2 = src_x + src_w;
    y1 = src_y;
    y2 = src_y + src_h;

    dstBox.x1 = drw_x;
    dstBox.x2 = drw_x + drw_w;
    dstBox.y1 = drw_y;
    dstBox.y2 = drw_y + drw_h;

    if(!CHIPSClipVideo(&dstBox, &x1, &x2, &y1, &y2, clipBoxes, 
			surface->width, surface->height))
	return Success;

    dstBox.x1 -= pScrn->frameX0;
    dstBox.x2 -= pScrn->frameX0;
    dstBox.y1 -= pScrn->frameY0;
    dstBox.y2 -= pScrn->frameY0;

    if (portPriv->doubleBuffer)
      portPriv->currentBuffer = CHIPSSetCurrentPlaybackBuffer(cPtr,0);
    else 
      portPriv->currentBuffer = 0;

    CHIPSDisplayVideo(pScrn, surface->id, surface->offsets[0], 
	     surface->width, surface->height, surface->pitches[0],
	     x1, y1, x2, y2, &dstBox, src_w, src_h, drw_w, drw_h, FALSE);
    xf86XVFillKeyHelper(pScrn->pScreen, portPriv->colorKey, clipBoxes);

    pPriv->isOn = TRUE;
    if(portPriv->videoStatus & CLIENT_VIDEO_ON) {
	REGION_EMPTY(pScrn->pScreen, &portPriv->clip);   
	UpdateCurrentTime();
	portPriv->videoStatus = FREE_TIMER;
	portPriv->freeTime = currentTime.milliseconds + FREE_DELAY;
	cPtr->VideoTimerCallback = CHIPSVideoTimerCallback;
    }

    return Success;
}


static void 
CHIPSInitOffscreenImages(ScreenPtr pScreen)
{
    XF86OffscreenImagePtr offscreenImages;

    /* need to free this someplace */
    if(!(offscreenImages = xalloc(sizeof(XF86OffscreenImageRec))))
	return;

    offscreenImages[0].image = &Images[0];
    offscreenImages[0].flags = VIDEO_OVERLAID_IMAGES | 
			       VIDEO_CLIP_TO_VIEWPORT;
    offscreenImages[0].alloc_surface = CHIPSAllocateSurface;
    offscreenImages[0].free_surface = CHIPSFreeSurface;
    offscreenImages[0].display = CHIPSDisplaySurface;
    offscreenImages[0].stop = CHIPSStopSurface;
    offscreenImages[0].setAttribute = CHIPSSetSurfaceAttribute;
    offscreenImages[0].getAttribute = CHIPSGetSurfaceAttribute;
    offscreenImages[0].max_width = 1024;
    offscreenImages[0].max_height = 1024;
    offscreenImages[0].num_attributes = NUM_ATTRIBUTES;
    offscreenImages[0].attributes = Attributes;
    
    xf86XVRegisterOffscreenImages(pScreen, offscreenImages, 1);
}

#endif  /* !XvExtension */
