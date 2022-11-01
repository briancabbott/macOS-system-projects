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
#include "mem.h"
#include "geometry.h"
#include "edges.h"
#include "hedges.h"
#include "heap.h"
#include "voronoi.h"


void
voronoi(int triangulate, Site *(*nextsite)())
{
    Site *newsite, *bot, *top, *temp, *p;
    Site *v;
    Point newintstar;
    char pm;
    Halfedge *lbnd, *rbnd, *llbnd, *rrbnd, *bisector;
    Edge *e;

    edgeinit ();
    siteinit ();
    PQinitialize();
    bottomsite = (*nextsite)();
#ifdef STANDALONE
    out_site(bottomsite);
#endif
    ELinitialize();

    newsite = (*nextsite)();
    while(1) {
        if(!PQempty()) newintstar = PQ_min();

        if (newsite != (struct Site *)NULL 
           && (PQempty() 
             || newsite -> coord.y < newintstar.y
             || (newsite->coord.y == newintstar.y 
                 && newsite->coord.x < newintstar.x))) {
    /* new site is smallest */
#ifdef STANDALONE
            out_site(newsite);
#endif
            lbnd = ELleftbnd(&(newsite->coord));
            rbnd = ELright(lbnd);
            bot = rightreg(lbnd);
            e = bisect(bot, newsite);
            bisector = HEcreate(e, le);
            ELinsert(lbnd, bisector);
            if ((p = hintersect(lbnd, bisector)) != (struct Site *) NULL) {
                PQdelete(lbnd);
                PQinsert(lbnd, p, dist(p,newsite));
            }
            lbnd = bisector;
            bisector = HEcreate(e, re);
            ELinsert(lbnd, bisector);
            if ((p = hintersect(bisector, rbnd)) != (struct Site *) NULL)
                PQinsert(bisector, p, dist(p,newsite));
            newsite = (*nextsite)();
        }
        else if (!PQempty()) { 
    /* intersection is smallest */
            lbnd = PQextractmin();
            llbnd = ELleft(lbnd);
            rbnd = ELright(lbnd);
            rrbnd = ELright(rbnd);
            bot = leftreg(lbnd);
            top = rightreg(rbnd);
#ifdef STANDALONE
            out_triple(bot, top, rightreg(lbnd));
#endif
            v = lbnd->vertex;
            makevertex(v);
            endpoint(lbnd->ELedge,lbnd->ELpm,v);
            endpoint(rbnd->ELedge,rbnd->ELpm,v);
            ELdelete(lbnd); 
            PQdelete(rbnd);
            ELdelete(rbnd); 
            pm = le;
            if (bot->coord.y > top->coord.y) {
                temp = bot; 
                bot = top; 
                top = temp; 
                pm = re;
            }
            e = bisect(bot, top);
            bisector = HEcreate(e, pm);
            ELinsert(llbnd, bisector);
            endpoint(e, re-pm, v);
            deref(v);
            if((p = hintersect(llbnd, bisector)) != (struct Site *) NULL) {
                PQdelete(llbnd);
                PQinsert(llbnd, p, dist(p,bot));
            }
            if ((p = hintersect(bisector, rrbnd)) != (struct Site *) NULL) {
                PQinsert(bisector, p, dist(p,bot));
            }
        }
        else break;
    }

    for(lbnd=ELright(ELleftend); lbnd != ELrightend; lbnd=ELright(lbnd)) {
        e = lbnd -> ELedge;
        clip_line(e);
#ifdef STANDALONE
        out_ep(e);
#endif
    }
}
