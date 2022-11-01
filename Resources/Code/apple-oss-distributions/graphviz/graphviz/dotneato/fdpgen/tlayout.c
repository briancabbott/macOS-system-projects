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

/* tlayout.c:
 * Written by Emden R. Gansner
 *
 * Module for initial layout, using point nodes and ports.
 * 
 * Note: If interior nodes are not connected, they tend to fly apart,
 * despite being tied to port nodes. This occurs because, as initially
 * coded, as the nodes tend to straighten into a line, the radius
 * grows causing more expansion. Is the problem really here and not
 * with disconnected nodes in xlayout? If here, we can either forbid
 * expansion or eliminate repulsion between nodes only connected
 * via port nodes.
 */

/* uses PRIVATE interface */
#define FDP_PRIVATE 1

#include <sys/types.h>
#include <time.h>
#ifndef MSWIN32
#include <unistd.h>
#endif
#include <xlayout.h>
#include <options.h>
#include <tlayout.h>
#include <dbg.h>
#include <grid.h>

  /* Default layout values; -1 indicates unset */
tParms_t fdp_tvals = {
  1,     /* useGrid */
  1,     /* useNew */
  1,     /* seed */
  -1,    /* maxIter */
  50,    /* unscaled */
  0.0,   /* C */
  1.0,   /* Tfact */
  -1.0,  /* K */
};
#define T_useGrid   (fdp_tvals.useGrid)
#define T_useNew    (fdp_tvals.useNew)
#define T_seed      (fdp_tvals.seed)
#define T_maxIter   (fdp_tvals.maxIter)
#define T_unscaled  (fdp_tvals.unscaled)
#define T_C         (fdp_tvals.C)
#define T_Tfact     (fdp_tvals.Tfact)
#define T_K         (fdp_tvals.K)

static int        numIters = -1;   /* number of total iterations */
static double     T0 = -1.0;       /* temperature of system */
static double     Cell = 0.0;      /* grid cell size */
static double     Cell2;           /* Cell*Cell */
static double     K2;              /* K*K */
static double     Wd;              /* half-width of boundary */
static double     Ht;              /* half-height of boundary */
static double     Wd2;             /* Wd*Wd */
static double     Ht2;             /* Ht*Ht */
static double     expFactor = 1.2; /* factor to increase radius */
static seedMode   smode;           /* seed mode */
static int        pass1;           /* iterations used in pass 1 */
static int        loopcnt;         /* actual iterations in this pass */

static int        dflt_numIters = 600;
static double     dflt_K      = 0.3;
static seedMode   dflt_smode = seed_val;

static double
cool (double temp, int t)
{
  return (T0*(numIters-t))/numIters;
}

/* reset_params:
 */
static void
reset_params (void)
{
  T0 = -1.0;
}

/* init_params:
 * Set parameters for expansion phase based on initial
 * layout parameters. If T0 is not set, we set it here
 * based on the size of the graph. In this case, we
 * return 1, so that fdp_tLayout can unset T0, to be
 * reset by a recursive call to fdp_tLayout. 
 */
static int
init_params (graph_t* g, xparams* xpms)
{
  int ret = 0;

  if (T0 == -1.0) {
    int nnodes = agnnodes(g);

    T0 = T_Tfact*T_K*sqrt(nnodes)/5;
#ifdef DEBUG
    if (Verbose) {
      prIndent();
      fprintf (stderr, "tlayout %s : T0 %f\n", g->name, T0);
    }
#endif
    ret = 1;
  }

  xpms->T0 = cool (T0, pass1);
  xpms->K = T_K;
  xpms->C = T_C;
  xpms->numIters = numIters - pass1;

  if (T_maxIter >= 0) {
    if (T_maxIter <= pass1) {
      loopcnt = T_maxIter;
      xpms->loopcnt = 0;
    }
    else if (T_maxIter <= numIters) {
      loopcnt = pass1;
      xpms->loopcnt = T_maxIter - pass1;
    }
  }
  else {
    loopcnt = pass1;
    xpms->loopcnt = xpms->numIters;
  }
  return ret;
}

/* fdp_initParams:
 * Initialize parameters based on root graph attributes.
 * Should K be a function of nnodes?
 */
void
fdp_initParams (graph_t* g)
{
  if (fdp_args.numIters == -1)
    numIters = late_int (g, agfindattr (g,"maxiter"),dflt_numIters,0);
  else
    numIters = fdp_args.numIters;
  if (fdp_args.K == -1.0)
    T_K = late_double (g, agfindattr (g,"K"),dflt_K,0.0);
  else
    T_K = fdp_args.K;
  if (fdp_args.T0 == -1.0) {
    T0 = late_double (g, agfindattr (g,"T0"),-1.0,0.0);
  }
  else
    T0 = fdp_args.T0;
  if (fdp_args.smode == seed_unset) {
    if (fdp_setSeed(&smode,agget (g,"start"))) {
      smode = dflt_smode;
    }
  }
  else
    smode = fdp_args.smode;

  pass1 = (T_unscaled*numIters)/100;
  K2 = T_K*T_K;

  if (T_useGrid) {
    if (Cell <= 0.0) Cell = 3*T_K;
    Cell2 = Cell * Cell;
  }
  if (Verbose) {
    fprintf (stderr, "Params: K %f T0 %f Tfact %f numIters %d unscaled %d\n",
      T_K, T0, T_Tfact, numIters, T_unscaled);
  }
}

static void
doRep (node_t* p, node_t* q,
     double xdelta, double ydelta, double dist2)
{
  double    force;
  double    dist;

  while (dist2 == 0.0) {
    xdelta = 5 - rand()%10;
    ydelta = 5 - rand()%10;
    dist2 = xdelta*xdelta + ydelta*ydelta;
  }
  if (T_useNew) {
    dist = sqrt (dist2);
    force = K2/(dist*dist2);
  }
  else force = K2/dist2;
  if (IS_PORT(p) && IS_PORT(q)) force *= 10.0;
  DISP(q)[0] += xdelta * force;
  DISP(q)[1] += ydelta * force;
  DISP(p)[0] -= xdelta * force;
  DISP(p)[1] -= ydelta * force;
}

/* applyRep:
 * Repulsive force = (K*K)/d
 *  or K*K/d*d
 */
static void
applyRep (Agnode_t* p, Agnode_t* q)
{
  double    xdelta, ydelta;

  xdelta = ND_pos(q)[0] - ND_pos(p)[0];
  ydelta = ND_pos(q)[1] - ND_pos(p)[1];
  doRep (p, q, xdelta, ydelta, xdelta*xdelta + ydelta*ydelta);
}

static void
doNeighbor (Grid* grid, int i, int j, node_list* nodes)
{
  cell*         cellp = findGrid (grid, i, j);
  node_list*    qs;
  Agnode_t*     p;
  Agnode_t*     q;
  double        xdelta, ydelta;
  double        dist2;

  if (cellp) {
    if (Verbose >= 3)
      fprintf (stderr, "  doNeighbor (%d,%d) : %d\n", i, j, gLength (cellp));
    for (; nodes != 0; nodes = nodes->next) {
      p = nodes->node;
      for (qs = cellp->nodes; qs != 0; qs = qs->next) {
        q = qs->node;
        xdelta = q->u.pos[0] - p->u.pos[0];
        ydelta = q->u.pos[1] - p->u.pos[1];
        dist2 = xdelta*xdelta + ydelta*ydelta;
        if (dist2 < Cell2)
          doRep (p, q, xdelta, ydelta, dist2);
      }
    }
  }
}

static int
gridRepulse (Dt_t* dt, cell* cellp, Grid* grid)
{
  node_list*    nodes = cellp->nodes;
  int           i = cellp->p.i;
  int           j = cellp->p.j;
  node_list*    p;
  node_list*    q;

  NOTUSED (dt);
  if (Verbose >= 3)
    fprintf (stderr, "gridRepulse (%d,%d) : %d\n", i, j, gLength (cellp));
  for (p = nodes; p != 0; p = p->next) {
    for (q = nodes; q != 0; q = q->next)
      if (p != q)
        applyRep (p->node, q->node);
  }

  doNeighbor(grid, i - 1, j - 1, nodes);
  doNeighbor(grid, i - 1, j    , nodes);
  doNeighbor(grid, i - 1, j + 1, nodes);
  doNeighbor(grid, i    , j - 1, nodes);
  doNeighbor(grid, i    , j + 1, nodes);
  doNeighbor(grid, i + 1, j - 1, nodes);
  doNeighbor(grid, i + 1, j    , nodes);
  doNeighbor(grid, i + 1, j + 1, nodes);

  return 0;
}

/* applyAttr:
 * Attractive force = weight*(d*d)/K
 *  or        force = (d - L(e))*weight(e)
 */
static void
applyAttr (Agnode_t* p, Agnode_t* q, Agedge_t* e)
{
  double    xdelta, ydelta;
  double    force;
  double    dist;
  double    dist2;

  xdelta = ND_pos(q)[0] - ND_pos(p)[0];
  ydelta = ND_pos(q)[1] - ND_pos(p)[1];
  dist2 = xdelta*xdelta + ydelta*ydelta;
  while (dist2 == 0.0) {
    xdelta = 5 - rand()%10;
    ydelta = 5 - rand()%10;
    dist2 = xdelta*xdelta + ydelta*ydelta;
  }
  dist = sqrt (dist2);
  if (T_useNew)
   force = (ED_factor(e) * (dist - ED_dist(e)))/dist;
  else
    force = (ED_factor(e) * dist)/ED_dist(e);
  DISP(q)[0] -= xdelta * force;
  DISP(q)[1] -= ydelta * force;
  DISP(p)[0] += xdelta * force;
  DISP(p)[1] += ydelta * force;
}

static void
updatePos (Agraph_t* g, double temp, bport_t* pp)
{
  Agnode_t*    n;
  double       temp2;
  double       len2;
  double       x,y,d;
  double       dx,dy;

  temp2 = temp * temp;
  for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
    if (ND_pinned(n) & P_FIX) continue;
    dx = DISP(n)[0];
    dy = DISP(n)[1];
    len2 = dx*dx + dy*dy;

      /* limit by temperature */
    if (len2 < temp2) {
      x = ND_pos(n)[0] + dx;
      y = ND_pos(n)[1] + dy;
    }
    else {
      double fact = temp/(sqrt(len2));
      x = ND_pos(n)[0] + dx*fact;
      y = ND_pos(n)[1] + dy*fact;
    }
    
      /* if ports, limit by boundary */
    if (pp) {
      d = sqrt((x*x)/Wd2 + (y*y)/Ht2);
      if (IS_PORT(n)) {
        ND_pos(n)[0] = x/d;
        ND_pos(n)[1] = y/d;
      }
      else if (d >= 1.0) {
        ND_pos(n)[0] = 0.95*x/d;
        ND_pos(n)[1] = 0.95*y/d;
      }
      else {
        ND_pos(n)[0] = x;
        ND_pos(n)[1] = y;
      }
    }
    else {
      ND_pos(n)[0] = x;
      ND_pos(n)[1] = y;
    }
  }
}

/* gAdjust:
 */
static void
gAdjust (Agraph_t* g, double temp, bport_t* pp, Grid* grid)
{
  Agnode_t*    n;
  Agedge_t*    e;

  if (temp <= 0.0) return;

  clearGrid (grid);

  for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
    DISP(n)[0] = DISP(n)[1] = 0;
    addGrid (grid, floor(n->u.pos[0]/Cell), floor(n->u.pos[1]/Cell), n);
  }

  for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
    for (e = agfstout(g,n); e; e = agnxtout(g,e))
      if (n != e->head) applyAttr (n, e->head, e);
  }
  walkGrid (grid, gridRepulse);


  updatePos (g, temp, pp);
}

/* adjust:
 */
static void
adjust (Agraph_t* g, double temp, bport_t* pp)
{
  Agnode_t*    n;
  Agnode_t*    n1;
  Agedge_t*    e;

  if (temp <= 0.0) return;

  for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
    DISP(n)[0] = DISP(n)[1] = 0;
  }

  for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
    for (n1 = agnxtnode(g,n); n1; n1 = agnxtnode(g,n1)) {
      applyRep (n, n1);
    }
    for (e = agfstout(g,n); e; e = agnxtout(g,e)) {
      if (n != e->head) applyAttr (n, e->head, e);
    }
  }

  updatePos (g, temp, pp);
}

/* initPositions:
 * Create initial layout of nodes
 * TODO :
 *  Position nodes near neighbors with positions.
 *  Use bbox to reset K.
 */
static pointf
initPositions (graph_t* g, bport_t* pp)
{
  int       nG = agnnodes (g) - NPORTS (g);
  double    size;
  Agnode_t* np;
  int       n_pos = 0;  /* no. of nodes with position info */
  box       bb;
  pointf    ctr;        /* center of boundary ellipse */
  long      local_seed;
  double    PItimes2 = M_PI * 2.0;

  for (np = agfstnode(g); np; np = agnxtnode(g,np)) {
    if (ND_pinned(np)) {
      if (n_pos) {
        bb.LL.x = MIN(ND_pos(np)[0],bb.LL.x);
        bb.LL.y = MIN(ND_pos(np)[1],bb.LL.y);
        bb.UR.x = MAX(ND_pos(np)[0],bb.UR.x);
        bb.UR.y = MAX(ND_pos(np)[1],bb.UR.y);
      }
      else {
        bb.UR.x = bb.LL.x = ND_pos(np)[0];
        bb.UR.y = bb.LL.y = ND_pos(np)[1];
      }
      n_pos++;
    }
  }

  size = T_K * (sqrt((double)nG) + 1.0);
  Wd = Ht = expFactor*(size/2.0);
  if (n_pos == 1) {
    ctr.x = bb.LL.x;
    ctr.y = bb.LL.y;
  }
  else if (n_pos > 1) {
    double alpha, area, width, height, quot;
    ctr.x = (bb.LL.x + bb.UR.x)/2.0;
    ctr.y = (bb.LL.y + bb.UR.y)/2.0;
    width = expFactor*(bb.UR.x - bb.LL.x);
    height = expFactor*(bb.UR.y - bb.LL.y);
    area = 4.0*Wd*Ht;
    quot = (width*height)/area;
    if (quot >= 1.0) {   /* If bbox has large enough area, use it */
      Wd = width/2.0;
      Ht = height/2.0;
    }
    else if (quot > 0.0) {  /* else scale up to have enough area */
      quot = 2.0*sqrt(quot);
      Wd = width/quot;
      Ht = height/quot;
    }
    else {    /* either width or height is 0 */
      if (width > 0) {
        height = area/width;
        Wd = width/2.0;
        Ht = height/2.0;
      }
      else if (height > 0) {
        width = area/height;
        Wd = width/2.0;
        Ht = height/2.0;
      }
      /* If width = height = 0, use Wd and Ht as defined above for
       * the case the n_pos == 0.
       */
    }
    
      /* Construct enclosing ellipse */
    alpha = atan2 (Ht, Wd);
    Wd = Wd/cos(alpha);
    Ht = Ht/sin(alpha);
  }
  else {
    ctr.x = ctr.y = 0;
  }
  Wd2 = Wd*Wd;
  Ht2 = Ht*Ht;

    /* Set seed value */
  if (smode == seed_val) local_seed = T_seed;
  else {
#ifdef MSWIN32
    local_seed = time(NULL);
#else
    local_seed = getpid() ^ time(NULL);
#endif
  }
  srand48(local_seed);

  /* If ports, place ports on and nodes within an ellipse centered at origin
   * with halfwidth Wd and halfheight Ht. 
   * If no ports, place nodes within a rectangle centered at origin
   * with halfwidth Wd and halfheight Ht. Nodes with a given position
   * are translated. Wd and Ht are set to contain all positioned points.
   * The reverse translation will be applied to all
   * nodes at the end of the layout.
   * TODO: place unfixed points using adjacent ports or fixed pts.
   */
  if (pp) {
    while (pp->e) { /* position ports on ellipse */
      np = pp->n;
      ND_pos(np)[0] = Wd * cos(pp->alpha);
      ND_pos(np)[1] = Ht * sin(pp->alpha);
      ND_pinned(np) = P_SET;
      pp++;
    }
    for (np = agfstnode(g); np; np = agnxtnode(g,np)) {
      if (IS_PORT(np)) continue;
      if (ND_pinned(np)) {
        ND_pos(np)[0] -= ctr.x;
        ND_pos(np)[1] -= ctr.y;
      }
      else {
        double angle = PItimes2 * drand48();
        double radius = 0.9 * drand48();
        ND_pos(np)[0] = radius * Wd * cos(angle);
        ND_pos(np)[1] = radius * Ht * sin(angle);
      }
    }
  }
  else {
    if (n_pos) {  /* If positioned nodes */
      for (np = agfstnode(g); np; np = agnxtnode(g,np)) {
        if (ND_pinned(np)) {
          ND_pos(np)[0] -= ctr.x;
          ND_pos(np)[1] -= ctr.y;
        }
        else {
          ND_pos(np)[0] = Wd * (2.0*drand48() - 1.0);
          ND_pos(np)[1] = Ht * (2.0*drand48() - 1.0);
        }
      }
    }
    else {   /* No ports or positions; place randomly */
      for (np = agfstnode(g); np; np = agnxtnode(g,np)) {
        ND_pos(np)[0] = Wd * (2.0*drand48() - 1.0);
        ND_pos(np)[1] = Ht * (2.0*drand48() - 1.0);
      }
    }
  }

  return ctr;
}

void
dumpstat (graph_t* g)
{
  double dx, dy;
  double l, max2 = 0.0;
  node_t* np;
  edge_t* ep;
  for (np = agfstnode(g); np; np = agnxtnode(g,np)) {
    dx = DISP(np)[0];
    dy = DISP(np)[1];
    l = dx*dx + dy*dy;
    if (l > max2) max2 = l;
    fprintf (stderr, "%s: (%f,%f) (%f,%f)\n", np->name,
      ND_pos(np)[0], ND_pos(np)[1],
      DISP(np)[0], DISP(np)[1]);
  }
  fprintf (stderr, "max delta = %f\n", sqrt(max2));
  for (np = agfstnode(g); np; np = agnxtnode(g,np)) {
    for (ep = agfstout(g,np); ep; ep = agnxtout(g,ep)) {
      dx = ND_pos(np)[0] - ND_pos(ep->head)[0];
      dy = ND_pos(np)[1] - ND_pos(ep->head)[1];
      fprintf (stderr, "  %s --  %s  (%f)\n", np->name, ep->head->name,
        sqrt(dx*dx + dy*dy));
    }
  }
}

/* fdp_tLayout:
 * Given graph g with ports nodes, layout g respecting ports.
 * If some node have position information, it may be useful to
 * reset temperature and other parameters to reflect this.
 */
void
fdp_tLayout (graph_t* g, xparams* xpms)
{
  int       i;
  int       reset;
  bport_t*  pp = PORTS(g);
  double    temp;
  Grid*     grid;
  pointf    ctr;
  Agnode_t* n;

  reset = init_params(g, xpms);
  temp = T0;

  ctr = initPositions (g, pp);
  
  if (T_useGrid) {
    grid = mkGrid (agnnodes (g));
    adjustGrid (grid, agnnodes (g));
    for (i = 0; i < loopcnt; i++) {
      temp = cool (temp, i);
      gAdjust (g, temp, pp, grid);
    }
    delGrid (grid);
  }
  else {
    for (i = 0; i < loopcnt; i++) {
      temp = cool (temp, i);
      adjust (g, temp, pp);
    }
  }

  if ((ctr.x != 0.0) || (ctr.y != 0.0)) {
    for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
      ND_pos(n)[0] += ctr.x;
      ND_pos(n)[1] += ctr.y;
    }
  }
dumpstat (g);
  if (reset) reset_params ();
}
