/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#pragma prototyped
/* poly.c
 */

#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <math.h>
#include "neato.h"
#include "poly.h"
#include "mem.h"


#define BOX 1
#define ISBOX(p) ((p)->kind & BOX)
#define CIRCLE 2
#define ISCIRCLE(p) ((p)->kind & CIRCLE)

static int maxcnt = 0;
static Point* tp1 = NULL;
static Point* tp2 = NULL;
static Point* tp3 = NULL;

void polyFree ()
{
    maxcnt = 0;
    free (tp1);
    free (tp2);
    free (tp3);
    tp1 = NULL;
    tp2 = NULL;
    tp3 = NULL;
}

void breakPoly (Poly* pp)
{
    free (pp->verts);
}

static void
bbox (Point* verts, int cnt, Point* o, Point* c)
{
    double  xmin, ymin, xmax, ymax;
    int    i;

    xmin = xmax = verts->x;
    ymin = ymax = verts->y;
    for (i = 1; i < cnt; i++) {
      verts++;
      if (verts->x < xmin) xmin = verts->x;
      if (verts->y < ymin) ymin = verts->y;
      if (verts->x > xmax) xmax = verts->x;
      if (verts->y > ymax) ymax = verts->y;
    }
    o->x = xmin;
    o->y = ymin;
    c->x = xmax;
    c->y = ymax;
}

#ifdef OLD
static void
inflate (Point* prev, Point* cur, Point* next, double margin)
{
    double theta = atan2 (prev->y - cur->y, prev->x - cur->x);
    double phi = atan2 (next->y - cur->y, next->x - cur->x);
    double beta = (theta + phi)/2.0;
    double gamma = (PI + phi - theta)/2.0;
    double denom;

    denom =  cos (gamma);
    cur->x -= margin*(cos (beta))/denom;
    cur->y -= margin*(sin (beta))/denom;
}

static void
inflatePts (Point* verts, int cnt, double margin)
{
    int    i;
    Point  first;
    Point  savepoint;
    Point  prevpoint;
    Point* prev = &prevpoint;
    Point* cur;
    Point* next;

    first = verts[0];
    prevpoint = verts[cnt-1];
    cur = &verts[0];
    next = &verts[1];
    for (i = 0; i < cnt-1; i++) {
      savepoint = *cur;
      inflate (prev,cur,next,margin);
      cur++;
      next++;
      prevpoint = savepoint;
    }

    next = &first;
    inflate (prev,cur,next,margin);
}
#else
static void
inflatePts (Point* verts, int cnt, double margin)
{
    int    i;
    Point* cur;

    cur = &verts[0];
    for (i = 0; i < cnt; i++) {
      cur->x *= margin;
      cur->y *= margin;
      cur++;
    }
}
#endif

static int
isBox (Point* verts, int cnt)
{
    if (cnt != 4) return 0;

    if (verts[0].y == verts[1].y)
      return ((verts[2].y == verts[3].y) &&
              (verts[0].x == verts[3].x) &&
              (verts[1].x == verts[2].x));
    else 
      return ((verts[0].x == verts[1].x) &&
              (verts[2].x == verts[3].x) &&
              (verts[0].y == verts[3].y) &&
              (verts[1].y == verts[2].y));
}

#define DFLT_SAMPLE 20

static Point
makeScaledPoint(int x, int y)
{
	Point	rv;
	rv.x = PS2INCH(x);
	rv.y = PS2INCH(y);
	return rv;
}

static Point*
genRound (Agnode_t* n, int* sidep)
{
  int    sides = 0;
  Point* verts;
  char*  p = agget(n,"samplepoints");
  int    i;

  if (p) sides = atoi(p);
  if (sides < 3) sides = DFLT_SAMPLE;
  verts = N_GNEW (sides, Point);
  for (i = 0; i < sides; i++) {
    verts[i].x = ND_width(n)/2.0  * cos(i/(double)sides * PI * 2.0);
    verts[i].y = ND_height(n)/2.0 * sin(i/(double)sides * PI * 2.0);
  }
  *sidep = sides;
  return verts;
}

void
makePoly (Poly* pp, Agnode_t* n, double margin)
{
    extern void poly_init(GVC_t *gvc);
    extern void record_init(GVC_t *gvc);
    extern void point_init(GVC_t *gvc);
    int    i;
    int    sides;
    Point* verts;
    polygon_t *poly;

    if (ND_shape(n)->fns->initfn == poly_init) {
      poly = (polygon_t*) ND_shape_info(n);
      sides = poly->sides;
      if (sides >= 3) {        /* real polygon */
        verts = N_GNEW (sides, Point);
		for (i = 0; i < sides; i++) {
          verts[i].x = poly->vertices[i].x;
          verts[i].y = poly->vertices[i].y;
        }
      }
      else verts = genRound (n, &sides);

      if (streq (ND_shape(n)->name, "box"))
        pp->kind = BOX;
      else if (streq (ND_shape(n)->name, "polygon") && isBox (verts, sides))
        pp->kind = BOX;
      else if ((poly->sides < 3) && poly->regular)
        pp->kind = CIRCLE;
      else
        pp->kind = 0;

    }
    else if (ND_shape(n)->fns->initfn == record_init) {
      box b;
      sides = 4;
      verts = N_GNEW (sides, Point);
	  b = ((field_t*) ND_shape_info(n))->b;
	  verts[0] = makeScaledPoint(b.LL.x,b.LL.y);
	  verts[1] = makeScaledPoint(b.UR.x,b.LL.y);
	  verts[2] = makeScaledPoint(b.UR.x,b.UR.y);
	  verts[3] = makeScaledPoint(b.LL.x,b.UR.y);
      pp->kind = BOX;
    }
    else if (ND_shape(n)->fns->initfn == point_init) {
      pp->kind = CIRCLE;
      verts = genRound (n, &sides);
    }
    else {
      agerr(AGERR, "makePoly: unknown shape type %s\n", ND_shape(n)->name);
      exit(1);
    }

#ifdef OLD
    if (margin != 0.0)
      inflatePts (verts,sides,margin);
#else
    if (margin != 1.0)
      inflatePts (verts,sides,margin);
#endif

    pp->verts = verts;
    pp->nverts = sides;
    bbox (verts, sides, &pp->origin, &pp->corner);

    if (sides > maxcnt)
      maxcnt = sides;
}

static int
pintersect (Point originp, Point cornerp, Point originq, Point cornerq)
{
    return ((originp.x <= cornerq.x) && (originq.x <= cornerp.x) &&
            (originp.y <= cornerq.y) && (originq.y <= cornerp.y));
}

#define Pin 1
#define Qin 2
#define Unknown 0

#define advance(A,B,N) (B++, A = (A+1)%N)

static int
edgesIntersect (Point* P, Point* Q, int n, int m)
{
    int      a = 0;
    int      b = 0;
    int      aa = 0;
    int      ba = 0;
    int      a1, b1;
    Point    A, B;
    double    cross;
    int      bHA, aHB;
    Point    p;
    int      inflag = Unknown;
    /* int      i = 0; */
    /* int      Reset = 0; */

    do {
      a1 = (a + n - 1) % n;
      b1 = (b + m - 1) % m;

      subPt(&A, P[a], P[a1]);
      subPt(&B, Q[b], Q[b1]);

      cross = area_2(origin, A, B );
      bHA = leftOf( P[a1], P[a], Q[b] );
      aHB = leftOf( Q[b1], Q[b], P[a] );

        /* If A & B intersect, update inflag. */
      if (intersection( P[a1], P[a], Q[b1], Q[b], &p ) )
        return 1;

                /* Advance rules. */
      if ( (cross == 0) && !bHA && !aHB ) {
        if ( inflag == Pin )
          advance( b, ba, m);
        else
          advance( a, aa, n);
      }
      else if ( cross >= 0 )
        if ( bHA )
          advance( a, aa, n);
        else {
          advance( b, ba, m);
      }
      else /* if ( cross < 0 ) */{
        if ( aHB )
          advance( b, ba, m);
        else
          advance( a, aa, n);
      }

    } while ( ((aa < n) || (ba < m)) && (aa < 2*n) && (ba < 2*m) );

    return 0;

}

static int	
inPoly(Point vertex[], int n, Point q)
{
	int	i, i1;		/* point index; i1 = i-1 mod n */
	double	x;		/* x intersection of e with ray */
	double	crossings = 0;	/* number of edge/ray crossings */

    if (tp3 == NULL)
      tp3 = N_GNEW (maxcnt, Point);

	/* Shift so that q is the origin. */
	for (i = 0; i < n; i++) {
		tp3[i].x = vertex[i].x - q.x;
		tp3[i].y = vertex[i].y - q.y;
	}

	/* For each edge e=(i-1,i), see if crosses ray. */
	for( i = 0; i < n; i++ ) {
		i1 = ( i + n - 1 ) % n;

		/* if edge is horizontal, test to see if the point is on it */
		if(  ( tp3[i].y == 0 ) && ( tp3[i1].y == 0 ) ) {
			if ((tp3[i].x*tp3[i1].x) < 0) {
				return 1 ;
			}
			else {
				continue;
			}
		}

		/* if e straddles the x-axis... */
		if( ( ( tp3[i].y >= 0 ) && ( tp3[i1].y <= 0 ) ) ||
		    ( ( tp3[i1].y >= 0 ) && ( tp3[i].y <= 0 ) ) ) {
			/* e straddles ray, so compute intersection with ray. */
			x = (tp3[i].x * tp3[i1].y - tp3[i1].x * tp3[i].y)
				/ (double)(tp3[i1].y - tp3[i].y);

			/* if intersect at origin, we've found intersection */
			if (x == 0) 
				return 1;;

			/* crosses ray if strictly positive intersection. */
			if (x > 0) {
				if ( (tp3[i].y == 0) || (tp3[i1].y == 0) ) {
					crossings += .5 ;  /* goes thru vertex*/
				}
				else {
					crossings += 1.0;
				}
			}
		}
	}

	/* q inside if an odd number of crossings. */
	if( (((int) crossings) % 2) == 1 )
		return	1;
	else	return	0;
}

static int 
inBox (Point p, Point origin, Point corner)
{
    return ((p.x <= corner.x) &&
            (p.x >= origin.x) &&
            (p.y <= corner.y) &&
            (p.y >= origin.y));

}

static void
transCopy (Point* inp, int cnt, Point off, Point* outp)
{
    int    i;

    for (i = 0; i < cnt; i++) {
      outp->x = inp->x + off.x;
      outp->y = inp->y + off.y;
      inp++;
      outp++;
    }
}

int 
polyOverlap (Point p, Poly* pp, Point q, Poly* qp)
{
    Point op, cp;
    Point oq, cq;

      /* translate bounding boxes */
    addPt (&op, p,pp->origin);
    addPt (&cp, p,pp->corner);
    addPt (&oq, q,qp->origin);
    addPt (&cq, q,qp->corner);

      /* If bounding boxes don't overlap, done */
    if (!pintersect(op,cp,oq,cq))
      return 0;
    
    if (ISBOX(pp) && ISBOX(qp)) return 1;
    if (ISCIRCLE(pp) && ISCIRCLE(qp)) {
      double d = (pp->corner.x - pp->origin.x + qp->corner.x - qp->origin.x);
      double dx = p.x - q.x;
      double dy = p.y - q.y;
      if ((dx*dx + dy*dy) > (d*d)/4.0) return 0;
      else return 1; 
    }

    if (tp1 == NULL) {
      tp1 = N_GNEW (maxcnt, Point);
      tp2 = N_GNEW (maxcnt, Point);
    }

    transCopy (pp->verts, pp->nverts, p, tp1);
    transCopy (qp->verts, qp->nverts, q, tp2);
    return (edgesIntersect (tp1, tp2, pp->nverts, qp->nverts) ||
            (inBox(*tp1, oq, cq) && inPoly(tp2, qp->nverts, *tp1)) ||
            (inBox(*tp2, op, cp) && inPoly(tp1, pp->nverts, *tp2)) );
}
