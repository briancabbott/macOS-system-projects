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

/* layout.c:
 * Written by Emden R. Gansner
 *
 * This module provides the main bookkeeping for the fdp layout.
 * In particular, it handles the recursion and the creation of
 * ports and auxiliary graphs.
 * 
 * TODO : can we use ports to aid in layout of edges? Note that
 * at present, they are deleted.
 *
 *   Can we delay all repositioning of nodes until evalPositions, so
 * finalCC only sets the bounding boxes?
 *
 * Make sure multiple edges have an effect.
 */

/* uses PRIVATE interface */
#define FDP_PRIVATE 1

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_VALUES_H
#include <values.h>
#endif
#include <fdp.h>
#include <neatoprocs.h>
#include <comp.h>
#include <pack.h>
#include <assert.h>
#include <xlayout.h>
#include <tlayout.h>
#include <dbg.h>

typedef struct {
  attrsym_t* G_coord;
  attrsym_t* G_width;
  attrsym_t* G_height;
  int        gid;
  pack_info  pack;
} layout_info;

#define NEW_EDGE(e) (ED_to_virt(e) == 0)

/* finalCC:
 * Set graph bounding box given list of connected
 * components, each with its bounding box set.
 * If c_cnt > 1, then pts != NULL and give new positions for components.
 * Add margin about whole graph unless isRoot is true.
 * Reposition nodes based on final position of
 * nodes connected component.
 * Also, entire layout is translated to origin.
 */
static void
finalCC (graph_t* g, int c_cnt, graph_t** cc, point* pts, graph_t* rg,
         attrsym_t* G_width, attrsym_t* G_height)
{
  graph_t*  cg;
  box       b,bb;
  boxf      bbf;
  point     pt;
  int       margin;
  graph_t** cp = cc;
  point*    pp = pts;
  int       isRoot = (rg == rg->root);
  int       isEmpty = 0;

  
    /* compute graph bounding box in points */
  if (c_cnt) {
    cg = *cp++;
    bb = GD_bb(cg);
    if (c_cnt > 1) {
      pt = *pp++;
      pt.x -= bb.LL.x;
      pt.y -= bb.LL.y;
      DELTA(cg).x = pt.x;
      DELTA(cg).y = pt.y;
      bb.LL.x += pt.x;
      bb.LL.y += pt.y;
      bb.UR.x += pt.x;
      bb.UR.y += pt.y;
      while ((cg = *cp++)) {
        b = GD_bb(cg);
        pt = *pp++;
        pt.x -= b.LL.x;
        pt.y -= b.LL.y;
        DELTA(cg).x = pt.x;
        DELTA(cg).y = pt.y;
        b.LL.x += pt.x;
        b.LL.y += pt.y;
        b.UR.x += pt.x;
        b.UR.y += pt.y;
        bb.LL.x = MIN(bb.LL.x,b.LL.x);
        bb.LL.y = MIN(bb.LL.y,b.LL.y);
        bb.UR.x = MAX(bb.UR.x,b.UR.x);
        bb.UR.y = MAX(bb.UR.y,b.UR.y);
      }
    }
  }
  else {  /* empty graph */
    bb.LL.x = 0;
    bb.LL.y = 0;
    bb.UR.x = late_int(rg,G_width,POINTS(DEFAULT_NODEWIDTH),3);
    bb.UR.y = late_int(rg,G_height,POINTS(DEFAULT_NODEHEIGHT),3);
    if (GD_label(rg)) {
      point p = cvt2pt(GD_label(rg)->dimen);
      bb.UR.x = MAX(bb.UR.x,p.x);
      if (p.y >= bb.UR.y) bb.UR.y = 0; /* height of label added below */
      else bb.UR.y -= p.y;
    }
    else isEmpty = 1;
  }

  if (isRoot || isEmpty) margin = 0;
  else margin = CL_OFFSET;
  pt.x = -bb.LL.x + margin;
  pt.y = -bb.LL.y + margin + GD_border(rg)[BOTTOM_IX].y;
  bb.LL.x = 0;
  bb.LL.y = 0;
  bb.UR.x += pt.x + margin;
  bb.UR.y += pt.y + margin + GD_border(rg)[TOP_IX].y;

    /* translate nodes */
  cp = cc;
  while ((cg = *cp++)) {
    point    p = DELTA(cg);
    node_t*  n;
    pointf   del;

    p.x += pt.x;
    p.y += pt.y;
    del = cvt2ptf (p);
    for (n = agfstnode(cg); n; n = agnxtnode(cg,n)) {
      ND_pos(n)[0] += del.x;
      ND_pos(n)[1] += del.y;
    }
  }

  bbf.LL = cvt2ptf (bb.LL);
  bbf.UR = cvt2ptf (bb.UR);
  BB(g) = bbf;

}

/* mkDeriveNode:
 * Constructor for a node in a derived graph.
 * Allocates dndata.
 */
static node_t*
mkDeriveNode (graph_t* dg, char* name)
{
  node_t* dn;

  dn = agnode (dg, name);
  ND_alg(dn) = (void*)NEW(dndata); /* free in freeDeriveNode */
  ND_pos(dn) = N_GNEW(GD_ndim(dg),double);
  /* fprintf (stderr, "Creating %s\n", dn->name); */
  return dn;
}

static void
freeDeriveNode (node_t* n)
{
  free (ND_alg(n));
  free (ND_pos(n));
}

static void
freeGData (graph_t* g)
{
  free (GD_alg(g));
}

static void
freeDerivedGraph (graph_t* g, graph_t** cc)
{
  graph_t*  cg;
  node_t* dn;

  while ((cg = *cc++)) {
    freeGData (cg);
  }
  if (PORTS(g)) free (PORTS(g));
  freeGData (g);
  for (dn = agfstnode(g); dn; dn = agnxtnode(g,dn))
    freeDeriveNode (dn);
  agclose (g);
}

/* evalPositions:
 * The input is laid out, but node coordinates
 * are relative to smallest containing cluster.
 * Walk through all nodes and clusters, translating
 * the positions to absolute coordinates.
 * Assume that when called, g's bounding box is
 * in absolute coordinates and that box of root graph
 * has LL at origin.
 */
static void
evalPositions (graph_t* g)
{
  int       i;
  graph_t*  subg;
  node_t*   n;
  boxf      bb;
  boxf      sbb;

  bb = BB(g);

    /* translate nodes in g */
  if (g != g->root) {
    for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
      if (PARENT(n) != g) continue;
      ND_pos(n)[0] += bb.LL.x;
      ND_pos(n)[1] += bb.LL.y;
    }
  }

    /* translate top-level clusters and recurse */
  for (i = 1; i <= GD_n_cluster(g); i++) {
    subg = GD_clust(g)[i];
    if (g != g->root) {
      sbb = BB(subg);
      sbb.LL.x += bb.LL.x;
      sbb.LL.y += bb.LL.y;
      sbb.UR.x += bb.LL.x;
      sbb.UR.y += bb.LL.y;
      BB(subg) = sbb;
    }
    evalPositions (subg);
  }
}

#define CL_CHUNK 10

typedef struct {
  graph_t** cl;
  int       sz;
  int       cnt;
} clist_t;

static void
initCList (clist_t* clist)
{
  clist->cl = 0;
  clist->sz = 0;
  clist->cnt = 0;
}

static void
addCluster (clist_t* clist, graph_t* subg)
{
  clist->cnt++;
  if (clist->cnt >= clist->sz) {
    clist->sz += CL_CHUNK;
    clist->cl = RALLOC (clist->sz,clist->cl, graph_t*);
  }
  clist->cl[clist->cnt] = subg;
}

#define BSZ 1000

/* portName:
 * Generate a name for a port.
 * We use the name of the subgraph and names of the nodes on the edge,
 * if possible. Otherwise, we use the ids of the nodes.
 * This is for debugging. For production, just use edge id and some
 * id for the graph. Note that all the graphs are subgraphs of the
 * root graph.
 */
static char*
portName (graph_t* g, bport_t* p)
{
  edge_t*     e = p->e;
  node_t*     h = e->head;
  node_t*     t = e->tail;
  static char buf[BSZ+1];
  int         len = 8;

  len += strlen (g->name) + strlen(h->name) + strlen(t->name);
  if (len >= BSZ)
    sprintf (buf, "_port_%s_%s_%s_%d", g->name, t->name, h->name, e->id);
  else
    sprintf (buf, "_port_%s_(%d)_(%d)_%d", g->name, ND_id(t), ND_id(h), e->id);
  return buf;
}

/* chkPos:
 * If cluster has coord attribute, use to supply initial position
 * of derived node.
 * Only called if G_coord is defined.
 * We also look at the parent graph's G_coord attribute. If this
 * is identical to the child graph, we have to assume the child
 * inherited it.
 */
static void
chkPos (graph_t* g, node_t* n, attrsym_t* G_coord)
{
  char*    p;
  char*    pp;
  boxf     bb;
  double   x, y;
  char     c;
  graph_t* parent;

  p = agxget(g,G_coord->index);
  if (p[0]) {
    if (g != g->root) {
      parent = agusergraph((agfstin(g->meta_node->graph, g->meta_node))->tail);
      pp = agxget(parent,G_coord->index);
      if ((pp == p) || !strcmp(p,pp)) return;
    }
    c = '\0';
    if (sscanf(p,"%lf,%lf,%lf,%lf%c",
        &bb.LL.x,&bb.LL.y,&bb.UR.x,&bb.UR.y,&c) >= 4) {
      x = (bb.LL.x + bb.UR.x)/2.0;
      y = (bb.LL.y + bb.UR.y)/2.0;
      if (PSinputscale > 0.0) {
         x /= PSinputscale;
         y /= PSinputscale;
      }
      ND_pos(n)[0] = x;
      ND_pos(n)[1] = y;
      if (c == '!') ND_pinned(n) = P_PIN;
      else if (c == '?') ND_pinned(n) = P_FIX;
      else ND_pinned(n) = P_SET;
    }
    else agerr(AGWARN,"graph %s, coord %s, expected four doubles\n",
      g->name,p);
  }
}

/* addEdge:
 * Add real edge e to its image de in the derived graph.
 * We use the to_virt and count fields to store the list.
 */
static void
addEdge (edge_t* de, edge_t* e)
{
  short    cnt = ED_count(de);
  edge_t** el;

  el = (edge_t**)(ED_to_virt(de));
  el = ALLOC (cnt+1, el, edge_t*);
  el[cnt] = e;
  ED_to_virt(de) = (edge_t*)el;
  ED_count(de)++;
}

/* deriveGraph:
 * Create derived graph of g by collapsing clusters into
 * nodes. An edge is created between nodes if there is
 * an edge between two nodes in the clusters of the base graph.
 * Such edges record all corresponding real edges.
 * In addition, we add a node and edge for each port.
 */
static graph_t*
deriveGraph (graph_t* g, layout_info* infop)
{
  graph_t*  mg;
  edge_t*   me;
  node_t*   mn;
  graph_t*  dg;
  node_t*   dn;
  graph_t*  subg;
  char      name[100];
  bport_t*  pp;
  node_t*   n;
  clist_t   clist;
  edge_t*   de;
  int       id = 0;

  sprintf (name, "_dg_%d", infop->gid++);
  if (Verbose >= 2)fprintf (stderr, "derive graph %s of %s\n", name, g->name);
  dg = agopen(name,AGRAPHSTRICT);
  GD_alg(dg) = (void*)NEW(gdata); /* freed in freeDeriveGraph */
  GD_ndim(dg) = GD_ndim(g);
  agraphattr (dg, "overlap", "scale");

    /* create derived nodes from clusters */
  initCList (&clist);
  mg = g->meta_node->graph;
  for (me = agfstout(mg,g->meta_node); me; me = agnxtout(mg,me)) {
    mn = me->head;
    subg = agusergraph(mn);
    if (!strncmp(subg->name,"cluster",7)) {
      pointf fix_LL;
      pointf fix_UR;

      addCluster (&clist, subg);
      GD_alg(subg) = (void*)NEW(gdata); /* freed in cleanup_subgs */
      GD_ndim(subg) = GD_ndim(g);
      do_graph_label(subg);
      dn = mkDeriveNode (dg, subg->name);
      ND_clust(dn) = subg;
      ND_id(dn) = id++;
      if (infop->G_coord) chkPos (subg, dn, infop->G_coord);
      for (n = agfstnode(subg); n; n = agnxtnode(subg,n)) {
        DNODE(n) = dn;
        if (ND_pinned(n)) {
          if (ND_pinned(dn)) {
            fix_LL.x = MIN(fix_LL.x,ND_pos(n)[0]);
            fix_LL.y = MIN(fix_LL.y,ND_pos(n)[0]);
            fix_UR.x = MIN(fix_UR.x,ND_pos(n)[0]);
            fix_UR.y = MIN(fix_UR.y,ND_pos(n)[0]);
            ND_pinned(dn) = MAX(ND_pinned(dn),ND_pinned(n));
          }
          else {
            fix_LL.x = fix_UR.x = ND_pos(n)[0];
            fix_LL.y = fix_UR.y = ND_pos(n)[1]; 
            ND_pinned(dn) = ND_pinned(n);
          }
        }
      }
      if (ND_pinned(dn)) {
        ND_pos(dn)[0] = (fix_LL.x + fix_UR.x)/2;
        ND_pos(dn)[1] = (fix_LL.y + fix_UR.y)/2;
      }
    }
  }
  GD_n_cluster(g) = clist.cnt;
  if (clist.cnt)
    GD_clust(g) = RALLOC (clist.cnt+1, clist.cl, graph_t*);

    /* create derived nodes from remaining nodes */
  for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
    if (!DNODE(n)) {
      dn = mkDeriveNode (dg, n->name);
      DNODE(n) = dn;
      PARENT(n) = g;
      ND_id(dn) = id++;
      ND_width(dn) = ND_width(n);
      ND_height(dn) = ND_height(n);
      ND_xsize(dn) = ND_xsize(n);
      ND_ysize(dn) = ND_ysize(n);
      ND_shape(dn) = ND_shape(n);
      ND_shape_info(dn) = ND_shape_info(n);
      if (ND_pinned(n)) {
        ND_pos(dn)[0] = ND_pos(n)[0];
        ND_pos(dn)[1] = ND_pos(n)[1];
        ND_pinned(dn) = ND_pinned(n);
      }
      ANODE(dn) = n;
    }
  }

    /* add edges */
  for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
    edge_t*   e;
    node_t*   hd;
    node_t*   tl = DNODE(n);
    for (e = agfstout (g, n); e; e = agnxtout (g, e)) {
      hd = DNODE(e->head);
      if (hd == tl) continue;
      if (hd > tl) de = agedge (dg, tl, hd);
      else de = agedge (dg, hd, tl);
      ED_dist(de) = ED_dist(e);
      ED_factor(de) = ED_factor(e);
      /* fprintf (stderr, "edge %s -- %s\n", tl->name, hd->name); */
      WDEG(hd)++;
      WDEG(tl)++;
      if (NEW_EDGE(de)) {
        DEG(hd)++;
        DEG(tl)++;
      }
      addEdge (de, e);
    }
  }

    /* transform ports */
  if ((pp = PORTS(g))) {
    bport_t*  pq;
    node_t*   m;
    int       sz = NPORTS(g);

      /* freed in freeDeriveGraph */
    PORTS(dg) = pq = N_NEW(sz+1,bport_t);
    NPORTS(dg) = sz;
    while (pp->e) {
      dn = mkDeriveNode (dg, portName(g, pp));
      ND_id(dn) = id++;
      m = DNODE(pp->n);
      if (dn > m) de = agedge (dg, m, dn);
      else de = agedge (dg, dn, m);
      ED_dist(de) = ED_dist(pp->e);
      ED_factor(de) = ED_factor(pp->e);
      addEdge (de, pp->e);
      WDEG(dn)++;
      WDEG(m)++;
      DEG(dn)++;    /* ports are unique, so this will be the first and */
      DEG(m)++;     /* only time the edge is touched. */
      pq->n = dn;
      pq->alpha = pp->alpha;
      pq->e = de;
      pp++;
      pq++;
    }
  }

  return dg;
}

typedef struct {
  edge_t*  e;
  double   alpha;
  double   dist2;
} erec;

/* ecmp:
 * Sort edges by angle, then distance.
 */
static int
ecmp (const void* v1, const void* v2)
{
  erec* e1 = (erec*)v1;
  erec* e2 = (erec*)v2;
  if (e1->alpha > e2->alpha) return 1;
  else if (e1->alpha < e2->alpha) return -1;
  else if (e1->dist2 > e2->dist2) return 1;
  else if (e1->dist2 < e2->dist2) return -1;
  else return 0;
}

#define ANG (PI/90)    /* Maximum angular change: 2 degrees */

/* getEdgeList:
 * Generate list of edges in derived graph g using
 * node n. The list is in counterclockwise order.
 * This, of course, assumes we have an initial layout for g.
 */
static erec*
getEdgeList (node_t* n, graph_t* g)
{
  erec*    erecs;
  int      deg = DEG(n);
  int      i;
  double   dx, dy;
  edge_t*  e;
  node_t*  m;

    /* freed in expandCluster */
  erecs = N_NEW(deg+1,erec);
  i = 0;
  for (e = agfstedge(g,n); e; e = agnxtedge (g,e,n)) {
    if (e->head == n) m = e->tail;
    else m = e->head;
    dx = ND_pos(m)[0] - ND_pos(n)[0];
    dy = ND_pos(m)[1] - ND_pos(n)[1];
    erecs[i].e = e;
    erecs[i].alpha = atan2 (dy,dx);
    erecs[i].dist2 = dx*dx + dy*dy;
    i++;
  }
  assert (i == deg);
  qsort (erecs, deg, sizeof(erec), ecmp);

    /* ensure no two angles are equal */
  if (deg >= 2) {
    int      j;
    double   a, inc, delta, bnd;

    i = 0;
    while (i < deg-1) {
      a = erecs[i].alpha;
      j= i+1;
      while ((j < deg) && (erecs[j].alpha == a)) j++;
      if (j == i+1) i = j;
      else {
        if (j == deg) bnd = PI;   /* all values equal up to end */
        else bnd = erecs[j].alpha;
        delta = (bnd - a)/(j - i);
        if (delta > ANG) delta = ANG;
        inc = 0;
        for (; i < j; i++) {
          erecs[i].alpha += inc;
          inc += delta;
        }
      }
    }
  }

  return erecs;
}

/* genPorts:
 * Given list of edges with node n in derived graph, add corresponding
 * ports to port list pp, starting at index idx. Return next index.
 * If an edge in the derived graph corresponds to multiple real edges,
 * add them in order if address of n is smaller than other node address.
 * Otherwise, reverse order.
 * Attach angles. The value bnd gives next angle after er->alpha.
 */
static int
genPorts (node_t* n, erec* er, bport_t* pp, int idx, double bnd)
{
  node_t*   other;
  int       cnt;
  edge_t*   e = er->e;
  edge_t*   el;
  edge_t**  ep;
  double    angle, delta;
  int       i, j, inc;

  cnt = ED_count(e);

  if (e->head == n) other = e->tail;
  else other = e->head;

  delta = (bnd - er->alpha)/cnt;
  angle = er->alpha;
  if (delta > ANG) delta = ANG;

  if (n < other) {
    i = idx;
    inc = 1;
  }
  else {
    i = idx + cnt - 1;
    inc = -1;
    angle += delta*(cnt-1);
    delta = -delta;
  }
  
  ep = (edge_t**)(el = ED_to_virt(e));
  for (j = 0; j < ED_count(e); j++, ep++) {
    el = *ep;
    pp[i].e = el;
    pp[i].n = (DNODE(el->tail) == n ? el->tail : el->head);
    pp[i].alpha = angle;
    i += inc;
    angle += delta;
  }
  return (idx + cnt);
}

/* expandCluster;
 * Given positioned derived graph cg with node n which corresponds
 * to a cluster, generate a graph containing the interior of the
 * cluster, plus port information induced by the layout of cg.
 * Basically, we use the cluster subgraph to which n corresponds,
 * attached with port information.
 */
static graph_t*
expandCluster (node_t* n, graph_t* cg)
{
  erec*       es;
  erec*       ep;
  erec*       next;
  graph_t*    sg = ND_clust(n);
  bport_t*    pp;
  int         sz = WDEG (n);
  int         idx = 0;
  double      bnd;

  if (sz != 0) {
    /* freed in cleanup_subgs */
    pp = N_NEW(sz+1,bport_t);

    /* create sorted list of edges of n */
    es = ep = getEdgeList (n, cg);

    /* generate ports from edges */
    while (ep->e) {
      next = ep+1;
      if (next->e) bnd = next->alpha;
      else bnd = 2*PI + es->alpha;
      idx = genPorts (n, ep, pp, idx, bnd);
      ep = next;
    }
    assert (idx == sz);
  
    PORTS(sg) = pp;
    NPORTS(sg) = sz;
    free (es);
  }
  return sg;
}

/* layout:
 * Given g with ports:
 *  Derive g' from g by reducing clusters to points (deriveGraph)
 *  Compute connected components of g' (findCComp)
 *  For each cc of g': 
 *    Layout cc (tLayout)
 *    For each node n in cc of g' <-> cluster c in g:
 *      Add ports based on layout of cc to get c' (expandCluster)
 *      Layout c' (recursion)
 *    Remove ports from cc
 *    Expand nodes of cc to reflect size of c'  (xLayout)
 *  Pack connected components to get layout of g (packGraphs)
 *  Translate layout so that bounding box of layout + margin 
 *  has the origin as LL corner. 
 *  Set position of top level clusters and real nodes.
 *  Set bounding box of graph
 * 
 * TODO:
 * 
 * Possibly should modify so that only do connected components
 * on top-level derived graph. Unconnected parts of a cluster
 * could just rattle within cluster boundaries. This may mix
 * up components but give a tighter packing.
 * 
 * Add edges per components to get better packing, rather than
 * wait until the end.
 */
/* static */ void
layout (graph_t* g, layout_info* infop)
{
  point*    pts = NULL;
  graph_t*  dg;
  node_t*   dn;
  node_t*   n;
  graph_t*  cg;
  graph_t*  sg;
  graph_t** cc;
  graph_t** pg;
  int       c_cnt;
  int       pinned;
  xparams   xpms;

  /* initialize derived node pointers */
  for (n = agfstnode(g); n; n = agnxtnode(g,n))
    DNODE(n) = 0;

  dg = deriveGraph (g, infop);
  cc = pg = findCComp (dg, &c_cnt, &pinned);

  while ((cg = *pg++)) {
    fdp_tLayout (cg, &xpms);
    for (n = agfstnode(cg); n; n = agnxtnode(cg,n)) {
      if (ND_clust(n)) {
        point pt;
        sg = expandCluster (n, cg);  /* attach ports to sg */
        layout (sg, infop);
           /* bb.LL == origin */
        ND_width(n) = BB(sg).UR.x;
        ND_height(n) = BB(sg).UR.y;
        pt = cvt2pt (BB(sg).UR);
        ND_xsize(n) = pt.x;
        ND_ysize(n) = pt.y;
        if (ND_pinned(n) == P_PIN) {
            /* FIX: guarantee preserve topology? */
          ND_pos(n)[0] = (BB(sg).LL.x + BB(sg).UR.x)/2.0;
          ND_pos(n)[1] = (BB(sg).LL.y + BB(sg).UR.y)/2.0;
        }
      }
      else if (fdp_isPort(n)) agdelete (cg, n); /* remove ports from component*/
    }
    if (agnnodes(cg) >= 2)
      fdp_xLayout (cg, &xpms);
      /* set bounding box but don't use ports */
    /* setBB (cg); */
  }
  
    /* At this point, each connected component has its nodes correctly
     * positioned. If we have multiple components, we pack them together.
     * All nodes will be moved to their new positions.
     * NOTE: packGraphs uses nodes in components, so if port nodes are
     * not removed, it won't work.
     */
  if (c_cnt > 1) {
    boolean*  bp;
    if (pinned) {
      bp = N_NEW(c_cnt,boolean);
      bp[0] = TRUE;
    }
    else bp = 0;
    infop->pack.fixed = bp;
    pts = putGraphs (c_cnt, cc, NULL, &infop->pack);
    if (bp) free (bp);
  }
  else if (c_cnt == 1) {
    pts = NULL;
    neato_compute_bb (cc[0]);
  }

    /* set bounding box of dg and reposition nodes */
  finalCC (dg, c_cnt, cc, pts, g, infop->G_width, infop->G_height);

  /* record positions from derived graph to input graph */
  /* At present, this does not record port node info */
  /* In fact, as noted above, we have removed port nodes */
  for (dn = agfstnode(dg); dn; dn = agnxtnode(dg,dn)) {
    if ((sg = ND_clust(dn))) {
      BB(sg).LL.x = ND_pos(dn)[0] - ND_width(dn)/2;
      BB(sg).LL.y = ND_pos(dn)[1] - ND_height(dn)/2;
      BB(sg).UR.x = BB(sg).LL.x + ND_width(dn);
      BB(sg).UR.y = BB(sg).LL.y + ND_height(dn);
    }
    else if ((n = ANODE(dn))) {
      ND_pos(n)[0] = ND_pos(dn)[0];
      ND_pos(n)[1] = ND_pos(dn)[1];
    }
  }
  BB(g) = BB(dg);
#ifdef DEBUG
  if (g == g->root) dump (g,1,0);
#endif

  /* clean up temp graphs */
  freeDerivedGraph (dg,cc);
  free (cc);
}

/* setBB;
 * Set point box g->bb from inch box BB(g).
 */
static void
setBB (graph_t* g)
{
  int         i;
  GD_bb(g).LL = cvt2pt(BB(g).LL);
  GD_bb(g).UR = cvt2pt(BB(g).UR);
  for (i = 1; i <= GD_n_cluster(g); i++) {
    setBB (GD_clust(g)[i]);
  }
}

void
fdp_init_graph (Agraph_t *g)
{
  UseRankdir = FALSE;

  graph_init(g);
  GD_drawing(g)->engine = FDP;
  g->u.ndim = late_int(g,agfindattr(g,"dim"),2,2);
  Ndim = g->u.ndim = MIN(g->u.ndim,MAXDIM);

  fdp_initParams (g);
  fdp_init_node_edge(g);
}

/* init_info:
 * Initialize graph-dependent information and
 * state variable.s
 */
void
init_info (graph_t* g, layout_info* infop)
{
  infop->G_coord = agfindattr(g,"coords");
  infop->G_width = agfindattr(g,"width");
  infop->G_height = agfindattr(g,"height");
  infop->gid = 0;
  infop->pack.margin = getPack (g, CL_OFFSET/2, CL_OFFSET/2);
  infop->pack.doSplines = 0;
  infop->pack.mode = getPackMode (g,l_node);
}

void
fdp_layout (graph_t* g)
{
  layout_info info;

  fdp_init_graph (g);
  init_info (g, &info);
  layout (g, &info);
  evalPositions (g);

    /* Set bbox info for g and all clusters. This is needed by
     * spline_edges0. We already know the graph bbox is at the origin.
     * (We pass the origin to spline_edges0. This really isn't true,
     * as the algorithm has done many translations.)
     * On return from spline_edges0, all bounding boxes should be correct.
     */
  setBB (g);
  spline_edges0(g);
  dotneato_postprocess(g, neato_nodesize);
}

/* fdp_isPort:
 * Returns true if node is a port node.
 * At present, this uses the fact that a port
 * node will not have dn or cluster set.
 */
int
fdp_isPort (Agnode_t* n)
{
  return !(ANODE(n) || ND_clust(n));
}

