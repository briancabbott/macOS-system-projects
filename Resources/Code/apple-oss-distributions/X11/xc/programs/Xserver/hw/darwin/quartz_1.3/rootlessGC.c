/*
 * Graphics Context support for Mac OS X rootless X server
 *
 * Greg Parker     gparker@cs.stanford.edu
 *
 * February 2001  Created
 * March 3, 2001  Restructured as generic rootless mode
 */
/* $XFree86: xc/programs/Xserver/hw/darwin/quartz_1.3/rootlessGC.c,v 1.1 2002/03/28 02:21:20 torrey Exp $ */

#include "mi.h"
#include "scrnintstr.h"
#include "gcstruct.h"
#include "pixmapstr.h"
#include "windowstr.h"
#include "dixfontstr.h"
#include "mivalidate.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "rootlessCommon.h"


// GC functions
static void RootlessValidateGC(GCPtr pGC, unsigned long changes,
                               DrawablePtr pDrawable);
static void RootlessChangeGC(GCPtr pGC, unsigned long mask);
static void RootlessCopyGC(GCPtr pGCSrc, unsigned long mask, GCPtr pGCDst);
static void RootlessDestroyGC(GCPtr pGC);
static void RootlessChangeClip(GCPtr pGC, int type, pointer pvalue,
                               int nrects);
static void RootlessDestroyClip(GCPtr pGC);
static void RootlessCopyClip(GCPtr pgcDst, GCPtr pgcSrc);

GCFuncs rootlessGCFuncs = {
    RootlessValidateGC,
    RootlessChangeGC,
    RootlessCopyGC,
    RootlessDestroyGC,
    RootlessChangeClip,
    RootlessDestroyClip,
    RootlessCopyClip,
};

// GC operations
static void RootlessFillSpans();
static void RootlessSetSpans();
static void RootlessPutImage();
static RegionPtr RootlessCopyArea();
static RegionPtr RootlessCopyPlane();
static void RootlessPolyPoint();
static void RootlessPolylines();
static void RootlessPolySegment();
static void RootlessPolyRectangle();
static void RootlessPolyArc();
static void RootlessFillPolygon();
static void RootlessPolyFillRect();
static void RootlessPolyFillArc();
static int RootlessPolyText8();
static int RootlessPolyText16();
static void RootlessImageText8();
static void RootlessImageText16();
static void RootlessImageGlyphBlt();
static void RootlessPolyGlyphBlt();
static void RootlessPushPixels();

static GCOps rootlessGCOps = {
    RootlessFillSpans,
    RootlessSetSpans,
    RootlessPutImage,
    RootlessCopyArea,
    RootlessCopyPlane,
    RootlessPolyPoint,
    RootlessPolylines,
    RootlessPolySegment,
    RootlessPolyRectangle,
    RootlessPolyArc,
    RootlessFillPolygon,
    RootlessPolyFillRect,
    RootlessPolyFillArc,
    RootlessPolyText8,
    RootlessPolyText16,
    RootlessImageText8,
    RootlessImageText16,
    RootlessImageGlyphBlt,
    RootlessPolyGlyphBlt,
    RootlessPushPixels
#ifdef NEED_LINEHELPER
    , NULL
#endif
};


Bool
RootlessCreateGC(GCPtr pGC)
{
    RootlessGCRec *gcrec;
    RootlessScreenRec *s;
    Bool result;

    SCREEN_UNWRAP(pGC->pScreen, CreateGC);
    s = (RootlessScreenRec *) pGC->pScreen->
            devPrivates[rootlessScreenPrivateIndex].ptr;
    result = s->CreateGC(pGC);
    gcrec = (RootlessGCRec *) pGC->devPrivates[rootlessGCPrivateIndex].ptr;
    gcrec->originalOps = NULL; // don't wrap ops yet
    gcrec->originalFuncs = pGC->funcs;
    pGC->funcs = &rootlessGCFuncs;

    SCREEN_WRAP(pGC->pScreen, CreateGC);
    return result;
}


// GC func wrapping
// ValidateGC wraps gcOps iff dest is viewable. All others just unwrap&call.

// GCFUN_UNRAP assumes funcs have been wrapped and 
// does not assume ops have been wrapped
#define GCFUNC_UNWRAP(pGC) \
    RootlessGCRec *gcrec = (RootlessGCRec *) \
        (pGC)->devPrivates[rootlessGCPrivateIndex].ptr; \
    (pGC)->funcs = gcrec->originalFuncs; \
    if (gcrec->originalOps) { \
        (pGC)->ops = gcrec->originalOps; \
}

#define GCFUNC_WRAP(pGC) \
    gcrec->originalFuncs = (pGC)->funcs; \
    (pGC)->funcs = &rootlessGCFuncs; \
    if (gcrec->originalOps) { \
        gcrec->originalOps = (pGC)->ops; \
        (pGC)->ops = &rootlessGCOps; \
}


static void
RootlessValidateGC(GCPtr pGC, unsigned long changes, DrawablePtr pDrawable)
{
    GCFUNC_UNWRAP(pGC);

    pGC->funcs->ValidateGC(pGC, changes, pDrawable);

    gcrec->originalOps = NULL;

    if (pDrawable->type == DRAWABLE_WINDOW) {
        WindowPtr pWin = (WindowPtr) pDrawable;

        if (pWin->viewable) {
            gcrec->originalOps = pGC->ops;
        }
    }

    GCFUNC_WRAP(pGC);
}

static void RootlessChangeGC(GCPtr pGC, unsigned long mask)
{
    GCFUNC_UNWRAP(pGC);
    pGC->funcs->ChangeGC(pGC, mask);
    GCFUNC_WRAP(pGC);
}

static void RootlessCopyGC(GCPtr pGCSrc, unsigned long mask, GCPtr pGCDst)
{
    GCFUNC_UNWRAP(pGCDst);
    pGCDst->funcs->CopyGC(pGCSrc, mask, pGCDst);
    GCFUNC_WRAP(pGCDst);
}

static void RootlessDestroyGC(GCPtr pGC)
{
    GCFUNC_UNWRAP(pGC);
    pGC->funcs->DestroyGC(pGC);
    GCFUNC_WRAP(pGC);
}

static void RootlessChangeClip(GCPtr pGC, int type, pointer pvalue, int nrects)
{
    GCFUNC_UNWRAP(pGC);
    pGC->funcs->ChangeClip(pGC, type, pvalue, nrects);
    GCFUNC_WRAP(pGC);
}

static void RootlessDestroyClip(GCPtr pGC)
{
    GCFUNC_UNWRAP(pGC);
    pGC->funcs->DestroyClip(pGC);
    GCFUNC_WRAP(pGC);
}

static void RootlessCopyClip(GCPtr pgcDst, GCPtr pgcSrc)
{
    GCFUNC_UNWRAP(pgcDst);
    pgcDst->funcs->CopyClip(pgcDst, pgcSrc);
    GCFUNC_WRAP(pgcDst);
}


// GC ops
// We can't use shadowfb because shadowfb assumes one pixmap
// and our root window is a special case.
// So much of this code is copied from shadowfb.

// assumes both funcs and ops are wrapped
#define GCOP_UNWRAP(pGC) \
    RootlessGCRec *gcrec = (RootlessGCRec *) \
        (pGC)->devPrivates[rootlessGCPrivateIndex].ptr; \
    GCFuncs *saveFuncs = pGC->funcs; \
    (pGC)->funcs = gcrec->originalFuncs; \
    (pGC)->ops = gcrec->originalOps;

#define GCOP_WRAP(pGC) \
    gcrec->originalOps = (pGC)->ops; \
    (pGC)->funcs = saveFuncs; \
    (pGC)->ops = &rootlessGCOps;


static void
RootlessFillSpans(DrawablePtr dst, GCPtr pGC, int nInit,
                  DDXPointPtr pptInit, int *pwidthInit, int sorted)
{
    GCOP_UNWRAP(pGC);
    RL_DEBUG_MSG("fill spans start ");

    if (nInit <= 0) {
        pGC->ops->FillSpans(dst, pGC, nInit, pptInit, pwidthInit, sorted);
    } else {
        DDXPointPtr ppt = pptInit;
        int *pwidth = pwidthInit;
        int i = nInit;
        BoxRec box;

        box.x1 = ppt->x;
        box.x2 = box.x1 + *pwidth;
        box.y2 = box.y1 = ppt->y;

        while(--i) {
            ppt++;
            pwidthInit++;
            if(box.x1 > ppt->x)
                box.x1 = ppt->x;
            if(box.x2 < (ppt->x + *pwidth))
                box.x2 = ppt->x + *pwidth;
            if(box.y1 > ppt->y)
                box.y1 = ppt->y;
            else if(box.y2 < ppt->y)
                box.y2 = ppt->y;
        }

        box.y2++;

        pGC->ops->FillSpans(dst, pGC, nInit, pptInit, pwidthInit, sorted);

        TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
        if(BOX_NOT_EMPTY(box))
            RootlessDamageBox ((WindowPtr) dst, &box);
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("fill spans end\n");
}

static void
RootlessSetSpans(DrawablePtr dst, GCPtr pGC, char *pSrc,
                 DDXPointPtr pptInit, int *pwidthInit,
                 int nspans, int sorted)
{
    GCOP_UNWRAP(pGC);
    RL_DEBUG_MSG("set spans start ");

    if (nspans <= 0) {
        pGC->ops->SetSpans(dst, pGC, pSrc, pptInit, pwidthInit,
                           nspans, sorted);
    } else {
        DDXPointPtr ppt = pptInit;
        int *pwidth = pwidthInit;
        int i = nspans;
        BoxRec box;

        box.x1 = ppt->x;
        box.x2 = box.x1 + *pwidth;
        box.y2 = box.y1 = ppt->y;

        while(--i) {
            ppt++;
            pwidth++;
            if(box.x1 > ppt->x)
                box.x1 = ppt->x;
            if(box.x2 < (ppt->x + *pwidth))
                box.x2 = ppt->x + *pwidth;
            if(box.y1 > ppt->y)
                box.y1 = ppt->y;
            else if(box.y2 < ppt->y)
                box.y2 = ppt->y;
        }

        box.y2++;

        pGC->ops->SetSpans(dst, pGC, pSrc, pptInit, pwidthInit,
                           nspans, sorted);

        TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
        if(BOX_NOT_EMPTY(box))
            RootlessDamageBox ((WindowPtr) dst, &box);
    }
    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("set spans end\n");
}

static void
RootlessPutImage(DrawablePtr dst, GCPtr pGC,
                 int depth, int x, int y, int w, int h,
                 int leftPad, int format, char *pBits)
{
    BoxRec box;

    GCOP_UNWRAP(pGC);
    RL_DEBUG_MSG("put image start ");

    pGC->ops->PutImage(dst, pGC, depth, x,y,w,h, leftPad, format, pBits);

    box.x1 = x + dst->x;
    box.x2 = box.x1 + w;
    box.y1 = y + dst->y;
    box.y2 = box.y1 + h;

    TRIM_BOX(box, pGC);
    if(BOX_NOT_EMPTY(box))
        RootlessDamageBox ((WindowPtr) dst, &box);

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("put image end\n");
}

/* changed area is *dest* rect */
/* If this code ever goes back go StartDrawing/StopDrawing:
 *   start and stop dst always 
 *   start and stop src if src->type is DRAWABLE_WINDOW and src is framed
 */
static RegionPtr
RootlessCopyArea(DrawablePtr pSrc, DrawablePtr dst, GCPtr pGC,
                 int srcx, int srcy, int w, int h,
                 int dstx, int dsty)
{
    RegionPtr result;
    BoxRec box;

    GCOP_UNWRAP(pGC);
    RL_DEBUG_MSG("copy area start (src 0x%x, dst 0x%x)", pSrc, dst);

    result = pGC->ops->CopyArea(pSrc, dst, pGC, srcx, srcy, w, h, dstx, dsty);

    box.x1 = dstx + dst->x;
    box.x2 = box.x1 + w;
    box.y1 = dsty + dst->y;
    box.y2 = box.y1 + h;

    TRIM_BOX(box, pGC);
    if(BOX_NOT_EMPTY(box))
        RootlessDamageBox ((WindowPtr) dst, &box);

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("copy area end\n");
    return result;
}

/* changed area is *dest* rect */
/* If this code ever goes back go StartDrawing/StopDrawing:
 *   start and stop dst always 
 *   start and stop src if src->type is DRAWABLE_WINDOW and src is framed
 */
static RegionPtr RootlessCopyPlane(DrawablePtr pSrc, DrawablePtr dst,
                                   GCPtr pGC, int srcx, int srcy,
                                   int w, int h, int dstx, int dsty,
                                   unsigned long plane)
{
    RegionPtr result;
    BoxRec box;

    GCOP_UNWRAP(pGC);
    RL_DEBUG_MSG("copy plane start ");

    result = pGC->ops->CopyPlane(pSrc, dst, pGC, srcx, srcy, w, h,
                                 dstx, dsty, plane);

    box.x1 = dstx + dst->x;
    box.x2 = box.x1 + w;
    box.y1 = dsty + dst->y;
    box.y2 = box.y1 + h;

    TRIM_BOX(box, pGC);
    if(BOX_NOT_EMPTY(box))
        RootlessDamageBox ((WindowPtr) dst, &box);

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("copy plane end\n");
    return result;
}

// Options for size of changed area:
//  0 = box per point
//  1 = big box around all points
//  2 = accumulate point in 20 pixel radius
#define ROOTLESS_CHANGED_AREA 1
#define abs(a) ((a) > 0 ? (a) : -(a))

/* changed area is box around all points */
static void RootlessPolyPoint(DrawablePtr dst, GCPtr pGC,
                              int mode, int npt, DDXPointPtr pptInit)
{
    GCOP_UNWRAP(pGC);

    RL_DEBUG_MSG("polypoint start ");

    pGC->ops->PolyPoint(dst, pGC, mode, npt, pptInit);

    if (npt > 0) {
#if ROOTLESS_CHANGED_AREA==0
        // box per point
        BoxRec box;

        while (npt) {
            box.x1 = pptInit->x;
            box.y1 = pptInit->y;
            box.x2 = box.x1 + 1;
            box.y2 = box.y1 + 1;

            TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
            if(BOX_NOT_EMPTY(box))
                RootlessDamageBox ((WindowPtr) dst, &box);

            npt--;
            pptInit++;
        }

#elif ROOTLESS_CHANGED_AREA==1
        // one big box
        BoxRec box;

        box.x2 = box.x1 = pptInit->x;
        box.y2 = box.y1 = pptInit->y;
        while(--npt) {
            pptInit++;
            if(box.x1 > pptInit->x)
                box.x1 = pptInit->x;
            else if(box.x2 < pptInit->x)
                box.x2 = pptInit->x;
            if(box.y1 > pptInit->y)
                box.y1 = pptInit->y;
            else if(box.y2 < pptInit->y)
                box.y2 = pptInit->y;
        }

        box.x2++;
        box.y2++;

        TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
        if(BOX_NOT_EMPTY(box))
            RootlessDamageBox ((WindowPtr) dst, &box);

#elif ROOTLESS_CHANGED_AREA==2
        // clever(?) method: accumulate point in 20-pixel radius
        BoxRec box;
        int firstx, firsty;

        box.x2 = box.x1 = firstx = pptInit->x;
        box.y2 = box.y1 = firsty = pptInit->y;
        while(--npt) {
            pptInit++;
            if (abs(pptInit->x - firstx) > 20 ||
                abs(pptInit->y - firsty) > 20) {
                box.x2++;
                box.y2++;
                TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
                if(BOX_NOT_EMPTY(box))
                RootlessDamageBox ((WindowPtr) dst, &box);
                box.x2 = box.x1 = firstx = pptInit->x;
                box.y2 = box.y1 = firsty = pptInit->y;
            } else {
                if (box.x1 > pptInit->x) box.x1 = pptInit->x;
                else if (box.x2 < pptInit->x) box.x2 = pptInit->x;
                if (box.y1 > pptInit->y) box.y1 = pptInit->y;
                else if (box.y2 < pptInit->y) box.y2 = pptInit->y;
            }
        }
        box.x2++;
        box.y2++;
        TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
        if(BOX_NOT_EMPTY(box))
        RootlessDamageBox ((WindowPtr) dst, &box);
#endif  /* ROOTLESS_CHANGED_AREA */
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("polypoint end\n");
}

#undef ROOTLESS_CHANGED_AREA

/* changed area is box around each line */
static void RootlessPolylines(DrawablePtr dst, GCPtr pGC,
                              int mode, int npt, DDXPointPtr pptInit)
{
    GCOP_UNWRAP(pGC);

    RL_DEBUG_MSG("poly lines start ");

    pGC->ops->Polylines(dst, pGC, mode, npt, pptInit);

    if (npt > 0) {
        BoxRec box;
        int extra = pGC->lineWidth >> 1;

        box.x2 = box.x1 = pptInit->x;
        box.y2 = box.y1 = pptInit->y;

        if(npt > 1) {
            if(pGC->joinStyle == JoinMiter)
                extra = 6 * pGC->lineWidth;
            else if(pGC->capStyle == CapProjecting)
                extra = pGC->lineWidth;
        }

        if(mode == CoordModePrevious) {
            int x = box.x1;
            int y = box.y1;

            while(--npt) {
                pptInit++;
                x += pptInit->x;
                y += pptInit->y;
                if(box.x1 > x)
                    box.x1 = x;
                else if(box.x2 < x)
                    box.x2 = x;
                if(box.y1 > y)
                    box.y1 = y;
                else if(box.y2 < y)
                    box.y2 = y;
            }
        } else {
            while(--npt) {
                pptInit++;
                if(box.x1 > pptInit->x)
                    box.x1 = pptInit->x;
                else if(box.x2 < pptInit->x)
                    box.x2 = pptInit->x;
                if(box.y1 > pptInit->y)
                    box.y1 = pptInit->y;
                else if(box.y2 < pptInit->y)
                    box.y2 = pptInit->y;
            }
        }

        box.x2++;
        box.y2++;

        if(extra) {
            box.x1 -= extra;
            box.x2 += extra;
            box.y1 -= extra;
            box.y2 += extra;
        }

        TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
        if(BOX_NOT_EMPTY(box))
        RootlessDamageBox ((WindowPtr) dst, &box);
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("poly lines end\n");
}

/* changed area is box around each line segment */
static void RootlessPolySegment(DrawablePtr dst, GCPtr pGC,
                                int nseg, xSegment *pSeg)
{
    GCOP_UNWRAP(pGC);

    RL_DEBUG_MSG("poly segment start (win 0x%x)", dst);

    pGC->ops->PolySegment(dst, pGC, nseg, pSeg);

    if (nseg > 0) {
        BoxRec box;
        int extra = pGC->lineWidth;

        if(pGC->capStyle != CapProjecting)
        extra >>= 1;

        if(pSeg->x2 > pSeg->x1) {
            box.x1 = pSeg->x1;
            box.x2 = pSeg->x2;
        } else {
            box.x2 = pSeg->x1;
            box.x1 = pSeg->x2;
        }

        if(pSeg->y2 > pSeg->y1) {
            box.y1 = pSeg->y1;
            box.y2 = pSeg->y2;
        } else {
            box.y2 = pSeg->y1;
            box.y1 = pSeg->y2;
        }

        while(--nseg) {
            pSeg++;
            if(pSeg->x2 > pSeg->x1) {
                if(pSeg->x1 < box.x1) box.x1 = pSeg->x1;
                if(pSeg->x2 > box.x2) box.x2 = pSeg->x2;
            } else {
                if(pSeg->x2 < box.x1) box.x1 = pSeg->x2;
                if(pSeg->x1 > box.x2) box.x2 = pSeg->x1;
            }
            if(pSeg->y2 > pSeg->y1) {
                if(pSeg->y1 < box.y1) box.y1 = pSeg->y1;
                if(pSeg->y2 > box.y2) box.y2 = pSeg->y2;
            } else {
                if(pSeg->y2 < box.y1) box.y1 = pSeg->y2;
                if(pSeg->y1 > box.y2) box.y2 = pSeg->y1;
            }
        }

        box.x2++;
        box.y2++;

        if(extra) {
            box.x1 -= extra;
            box.x2 += extra;
            box.y1 -= extra;
            box.y2 += extra;
        }

        TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
        if(BOX_NOT_EMPTY(box))
            RootlessDamageBox ((WindowPtr) dst, &box);
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("poly segment end\n");
}

/* changed area is box around each line (not entire rects) */
static void RootlessPolyRectangle(DrawablePtr dst, GCPtr pGC,
				  int nRects, xRectangle *pRects)
{
    GCOP_UNWRAP(pGC);

    RL_DEBUG_MSG("poly rectangle start ");

    pGC->ops->PolyRectangle(dst, pGC, nRects, pRects);

    if (nRects > 0) {
        BoxRec box;
        int offset1, offset2, offset3;

        offset2 = pGC->lineWidth;
        if(!offset2) offset2 = 1;
        offset1 = offset2 >> 1;
        offset3 = offset2 - offset1;

        while(nRects--) {
            box.x1 = pRects->x - offset1;
            box.y1 = pRects->y - offset1;
            box.x2 = box.x1 + pRects->width + offset2;
            box.y2 = box.y1 + offset2;		
            TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
            if(BOX_NOT_EMPTY(box))
                RootlessDamageBox ((WindowPtr) dst, &box);

            box.x1 = pRects->x - offset1;
            box.y1 = pRects->y + offset3;
            box.x2 = box.x1 + offset2;
            box.y2 = box.y1 + pRects->height - offset2;		
            TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
            if(BOX_NOT_EMPTY(box))
                RootlessDamageBox ((WindowPtr) dst, &box);

            box.x1 = pRects->x + pRects->width - offset1;
            box.y1 = pRects->y + offset3;
            box.x2 = box.x1 + offset2;
            box.y2 = box.y1 + pRects->height - offset2;		
            TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
            if(BOX_NOT_EMPTY(box))
                RootlessDamageBox ((WindowPtr) dst, &box);

            box.x1 = pRects->x - offset1;
            box.y1 = pRects->y + pRects->height - offset1;
            box.x2 = box.x1 + pRects->width + offset2;
            box.y2 = box.y1 + offset2;		
            TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
            if(BOX_NOT_EMPTY(box))
                RootlessDamageBox ((WindowPtr) dst, &box);

            pRects++;
        }
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("poly rectangle end\n");
}


/* changed area is box around each arc (assumes all arcs are 360 degrees) */
static void RootlessPolyArc(DrawablePtr dst, GCPtr pGC, int narcs, xArc *parcs)
{
    GCOP_UNWRAP(pGC);

    RL_DEBUG_MSG("poly arc start ");

    pGC->ops->PolyArc(dst, pGC, narcs, parcs);

    if (narcs > 0) {
        int extra = pGC->lineWidth >> 1;
        BoxRec box;

        box.x1 = parcs->x;
        box.x2 = box.x1 + parcs->width;
        box.y1 = parcs->y;
        box.y2 = box.y1 + parcs->height;

        /* should I break these up instead ? */

        while(--narcs) {
            parcs++;
            if(box.x1 > parcs->x)
                box.x1 = parcs->x;
            if(box.x2 < (parcs->x + parcs->width))
                box.x2 = parcs->x + parcs->width;
            if(box.y1 > parcs->y)
                box.y1 = parcs->y;
            if(box.y2 < (parcs->y + parcs->height))
                box.y2 = parcs->y + parcs->height;
        }

        if(extra) {
            box.x1 -= extra;
            box.x2 += extra;
            box.y1 -= extra;
            box.y2 += extra;
        }

        box.x2++;
        box.y2++;

        TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
        if(BOX_NOT_EMPTY(box))
            RootlessDamageBox ((WindowPtr) dst, &box);
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("poly arc end\n");
}


/* changed area is box around each poly */
static void RootlessFillPolygon(DrawablePtr dst, GCPtr pGC,
				int shape, int mode, int count,
				DDXPointPtr pptInit)
{
    GCOP_UNWRAP(pGC);

    RL_DEBUG_MSG("fill poly start ");

    if (count <= 2) {
        pGC->ops->FillPolygon(dst, pGC, shape, mode, count, pptInit);
    } else {
        DDXPointPtr ppt = pptInit;
        int i = count;
        BoxRec box;

        box.x2 = box.x1 = ppt->x;
        box.y2 = box.y1 = ppt->y;

        if(mode != CoordModeOrigin) {
            int x = box.x1;
            int y = box.y1;

            while(--i) {
                ppt++;
                x += ppt->x;
                y += ppt->y;
                if(box.x1 > x)
                    box.x1 = x;
                else if(box.x2 < x)
                    box.x2 = x;
                if(box.y1 > y)
                    box.y1 = y;
                else if(box.y2 < y)
                    box.y2 = y;
            }
        } else {
            while(--i) {
                ppt++;
                if(box.x1 > ppt->x)
                    box.x1 = ppt->x;
                else if(box.x2 < ppt->x)
                    box.x2 = ppt->x;
                if(box.y1 > ppt->y)
                    box.y1 = ppt->y;
                else if(box.y2 < ppt->y)
                    box.y2 = ppt->y;
            }
        }

        box.x2++;
        box.y2++;

        pGC->ops->FillPolygon(dst, pGC, shape, mode, count, pptInit);

        TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
        if(BOX_NOT_EMPTY(box))
        RootlessDamageBox ((WindowPtr) dst, &box);
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("fill poly end\n");
}

/* changed area is the rects */
static void RootlessPolyFillRect(DrawablePtr dst, GCPtr pGC,
				 int nRectsInit, xRectangle *pRectsInit)
{
    GCOP_UNWRAP(pGC);

    RL_DEBUG_MSG("fill rect start (win 0x%x)", dst);

    if (nRectsInit <= 0) {
        pGC->ops->PolyFillRect(dst, pGC, nRectsInit, pRectsInit);
    } else {
        BoxRec box;
        xRectangle *pRects = pRectsInit;
        int nRects = nRectsInit;

        box.x1 = pRects->x;
        box.x2 = box.x1 + pRects->width;
        box.y1 = pRects->y;
        box.y2 = box.y1 + pRects->height;

        while(--nRects) {
            pRects++;
            if(box.x1 > pRects->x)
                box.x1 = pRects->x;
            if(box.x2 < (pRects->x + pRects->width))
                box.x2 = pRects->x + pRects->width;
            if(box.y1 > pRects->y)
                box.y1 = pRects->y;
            if(box.y2 < (pRects->y + pRects->height))
                box.y2 = pRects->y + pRects->height;
        }

        /* cfb messes with the pRectsInit so we have to do our
        calculations first */

        pGC->ops->PolyFillRect(dst, pGC, nRectsInit, pRectsInit);

        TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
        if(BOX_NOT_EMPTY(box))
            RootlessDamageBox ((WindowPtr) dst, &box);
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("fill rect end\n");
}


/* changed area is box around each arc (assuming arcs are all 360 degrees) */
static void RootlessPolyFillArc(DrawablePtr dst, GCPtr pGC,
				int narcs, xArc *parcs)
{
    GCOP_UNWRAP(pGC);

    RL_DEBUG_MSG("fill arc start ");

    pGC->ops->PolyFillArc(dst, pGC, narcs, parcs);

    if (narcs > 0) {
        BoxRec box;

        box.x1 = parcs->x;
        box.x2 = box.x1 + parcs->width;
        box.y1 = parcs->y;
        box.y2 = box.y1 + parcs->height;

        /* should I break these up instead ? */

        while(--narcs) {
            parcs++;
            if(box.x1 > parcs->x)
                box.x1 = parcs->x;
            if(box.x2 < (parcs->x + parcs->width))
                box.x2 = parcs->x + parcs->width;
            if(box.y1 > parcs->y)
                box.y1 = parcs->y;
            if(box.y2 < (parcs->y + parcs->height))
                box.y2 = parcs->y + parcs->height;
        }

        TRIM_AND_TRANSLATE_BOX(box, dst, pGC);
        if(BOX_NOT_EMPTY(box))
            RootlessDamageBox ((WindowPtr) dst, &box);
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("fill arc end");
}


static void RootlessImageText8(DrawablePtr dst, GCPtr pGC,
			       int x, int y, int count, char *chars)
{
    GCOP_UNWRAP(pGC);
    RL_DEBUG_MSG("imagetext8 start ");

    pGC->ops->ImageText8(dst, pGC, x, y, count, chars);

    if (count > 0) {
        int top, bot, Min, Max;
        BoxRec box;

        top = max(FONTMAXBOUNDS(pGC->font, ascent), FONTASCENT(pGC->font));
        bot = max(FONTMAXBOUNDS(pGC->font, descent), FONTDESCENT(pGC->font));

        Min = count * FONTMINBOUNDS(pGC->font, characterWidth);
        if(Min > 0) Min = 0;
        Max = count * FONTMAXBOUNDS(pGC->font, characterWidth);	
        if(Max < 0) Max = 0;

        /* ugh */
        box.x1 = dst->x + x + Min +
        FONTMINBOUNDS(pGC->font, leftSideBearing);
        box.x2 = dst->x + x + Max +
        FONTMAXBOUNDS(pGC->font, rightSideBearing);

        box.y1 = dst->y + y - top;
        box.y2 = dst->y + y + bot;

        TRIM_BOX(box, pGC);
        if(BOX_NOT_EMPTY(box))
            RootlessDamageBox ((WindowPtr) dst, &box);
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("imagetext8 end\n");
}

static int RootlessPolyText8(DrawablePtr dst, GCPtr pGC,
                             int x, int y, int count, char *chars)
{
    int width; // the result, sorta

    GCOP_UNWRAP(pGC);
    RL_DEBUG_MSG("polytext8 start ");

    width = pGC->ops->PolyText8(dst, pGC, x, y, count, chars);
    width -= x;

    if(width > 0) {
        BoxRec box;

        /* ugh */
        box.x1 = dst->x + x + FONTMINBOUNDS(pGC->font, leftSideBearing);
        box.x2 = dst->x + x + FONTMAXBOUNDS(pGC->font, rightSideBearing);

        if(count > 1) {
            if(width > 0) box.x2 += width;
            else box.x1 += width;
        }

        box.y1 = dst->y + y - FONTMAXBOUNDS(pGC->font, ascent);
        box.y2 = dst->y + y + FONTMAXBOUNDS(pGC->font, descent);

        TRIM_BOX(box, pGC);
        if(BOX_NOT_EMPTY(box))
            RootlessDamageBox ((WindowPtr) dst, &box);
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("polytext8 end\n");
    return (width + x);
}

static void RootlessImageText16(DrawablePtr dst, GCPtr pGC,
                                int x, int y, int count, unsigned short *chars)
{
    GCOP_UNWRAP(pGC);
    RL_DEBUG_MSG("imagetext16 start ");

    pGC->ops->ImageText16(dst, pGC, x, y, count, chars);

    if (count > 0) {
        int top, bot, Min, Max;
        BoxRec box;

        top = max(FONTMAXBOUNDS(pGC->font, ascent), FONTASCENT(pGC->font));
        bot = max(FONTMAXBOUNDS(pGC->font, descent), FONTDESCENT(pGC->font));

        Min = count * FONTMINBOUNDS(pGC->font, characterWidth);
        if(Min > 0) Min = 0;
        Max = count * FONTMAXBOUNDS(pGC->font, characterWidth);
        if(Max < 0) Max = 0;

        /* ugh */
        box.x1 = dst->x + x + Min +
            FONTMINBOUNDS(pGC->font, leftSideBearing);
        box.x2 = dst->x + x + Max +
            FONTMAXBOUNDS(pGC->font, rightSideBearing);

        box.y1 = dst->y + y - top;
        box.y2 = dst->y + y + bot;

        TRIM_BOX(box, pGC);
        if(BOX_NOT_EMPTY(box))
            RootlessDamageBox ((WindowPtr) dst, &box);
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("imagetext16 end\n");
}

static int RootlessPolyText16(DrawablePtr dst, GCPtr pGC,
                            int x, int y, int count, unsigned short *chars)
{
    int width; // the result, sorta

    GCOP_UNWRAP(pGC);
    RL_DEBUG_MSG("polytext16 start ");

    width = pGC->ops->PolyText16(dst, pGC, x, y, count, chars);
    width -= x;

    if (width > 0) {
        BoxRec box;

        /* ugh */
        box.x1 = dst->x + x + FONTMINBOUNDS(pGC->font, leftSideBearing);
        box.x2 = dst->x + x + FONTMAXBOUNDS(pGC->font, rightSideBearing);

        if(count > 1) {
            if(width > 0) box.x2 += width;
            else box.x1 += width;
        }

        box.y1 = dst->y + y - FONTMAXBOUNDS(pGC->font, ascent);
        box.y2 = dst->y + y + FONTMAXBOUNDS(pGC->font, descent);

        TRIM_BOX(box, pGC);
        if(BOX_NOT_EMPTY(box))
            RootlessDamageBox ((WindowPtr) dst, &box);
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("polytext16 end\n");
    return width + x;
}

static void RootlessImageGlyphBlt(DrawablePtr dst, GCPtr pGC,
                                  int x, int y, unsigned int nglyph,
                                  CharInfoPtr *ppci, pointer unused)
{
    GCOP_UNWRAP(pGC);
    RL_DEBUG_MSG("imageglyph start ");

    pGC->ops->ImageGlyphBlt(dst, pGC, x, y, nglyph, ppci, unused);

    if (nglyph > 0) {
        int top, bot, width = 0;
        BoxRec box;

        top = max(FONTMAXBOUNDS(pGC->font, ascent), FONTASCENT(pGC->font));
        bot = max(FONTMAXBOUNDS(pGC->font, descent), FONTDESCENT(pGC->font));

        box.x1 = ppci[0]->metrics.leftSideBearing;
        if(box.x1 > 0) box.x1 = 0;
        box.x2 = ppci[nglyph - 1]->metrics.rightSideBearing -
            ppci[nglyph - 1]->metrics.characterWidth;
        if(box.x2 < 0) box.x2 = 0;

        box.x2 += dst->x + x;
        box.x1 += dst->x + x;

        while(nglyph--) {
            width += (*ppci)->metrics.characterWidth;
            ppci++;
        }

        if(width > 0)
            box.x2 += width;
        else
            box.x1 += width;

        box.y1 = dst->y + y - top;
        box.y2 = dst->y + y + bot;

        TRIM_BOX(box, pGC);
        if(BOX_NOT_EMPTY(box))
            RootlessDamageBox ((WindowPtr) dst, &box);
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("imageglyph end\n");
}

static void RootlessPolyGlyphBlt(DrawablePtr dst, GCPtr pGC,
                                 int x, int y, unsigned int nglyph,
                                 CharInfoPtr *ppci, pointer pglyphBase)
{
    GCOP_UNWRAP(pGC);
    RL_DEBUG_MSG("polyglyph start ");

    pGC->ops->PolyGlyphBlt(dst, pGC, x, y, nglyph, ppci, pglyphBase);

    if (nglyph > 0) {
        BoxRec box;

        /* ugh */
        box.x1 = dst->x + x + ppci[0]->metrics.leftSideBearing;
        box.x2 = dst->x + x + ppci[nglyph - 1]->metrics.rightSideBearing;

        if(nglyph > 1) {
            int width = 0;

            while(--nglyph) {
                width += (*ppci)->metrics.characterWidth;
                ppci++;
            }

            if(width > 0) box.x2 += width;
            else box.x1 += width;
        }

        box.y1 = dst->y + y - FONTMAXBOUNDS(pGC->font, ascent);
        box.y2 = dst->y + y + FONTMAXBOUNDS(pGC->font, descent);

        TRIM_BOX(box, pGC);
        if(BOX_NOT_EMPTY(box))
        RootlessDamageBox ((WindowPtr) dst, &box);
    }

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("polyglyph end\n");
}


/* changed area is in dest */
static void
RootlessPushPixels(GCPtr pGC, PixmapPtr pBitMap, DrawablePtr dst,
                   int dx, int dy, int xOrg, int yOrg)
{
    BoxRec box;
    GCOP_UNWRAP(pGC);

    pGC->ops->PushPixels(pGC, pBitMap, dst, dx, dy, xOrg, yOrg);

    box.x1 = xOrg + dst->x;
    box.x2 = box.x1 + dx;
    box.y1 = yOrg + dst->y;
    box.y2 = box.y1 + dy;

    TRIM_BOX(box, pGC);
    if(BOX_NOT_EMPTY(box))
        RootlessDamageBox ((WindowPtr) dst, &box);

    GCOP_WRAP(pGC);
    RL_DEBUG_MSG("push pixels end\n");
}
