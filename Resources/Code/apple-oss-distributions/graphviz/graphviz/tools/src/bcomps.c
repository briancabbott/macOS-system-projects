/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

/*
 * Generate biconnected components
 *
 * Written by Emden Gansner
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include "compat_getopt.h"
#endif

typedef struct {
  struct Agraph_t*    next;
} Agraphinfo_t;

typedef struct {
  struct Agedge_t*    next;
} Agedgeinfo_t;

typedef struct {
  int low;
  int val;
  int isCut;
} Agnodeinfo_t;

#include <graph.h>
#include <ingraphs.h>

#define Low(n) ((n)->u.low)
#define Cut(n) ((n)->u.isCut)
#define N(n) ((n)->u.val)
#define NEXT(e) ((e)->u.next)
#define NEXTBLK(g) ((g)->u.next)

#define min(a,b) ((a) < (b) ? (a) :  (b))

char** Files;
int    verbose;
int    silent;
char*  outfile = 0;
char*  path = 0;
char*  suffix = 0;
int    external;       /* emit blocks as root graphs */
int    doTree;         /* emit block-cutpoint tree */

static void
push (Agedge_t** sp, Agedge_t* e)
{
  NEXT(e) = *sp;
  *sp = e;
}

static Agedge_t*
pop (Agedge_t** sp)
{
  Agedge_t* top = *sp;

  assert(top);
  *sp = NEXT(top);
  return top;
}

#define top(sp) (sp)

typedef struct {
  int        count;
  int        nComp;
  Agedge_t*  stk;
  Agraph_t*  blks;
} bcstate;

static char*
blockName (char* gname, int d)
{
  static char*  buf;
  static int    bufsz;
  int           sz;

  sz = strlen(gname) + 32;
  if (sz > bufsz) {
    if (buf) free (buf);
    buf = (char*)malloc(sz);
  }
  
  sprintf(buf,"%s_bcc_%d",gname,d);
  return buf;
}

/* getName:
 * Generate name for output using input template.
 * Has form path_<g>_<i>.suffix, for ith write for the gth graph.
 * If isTree, use path_<g>_t.suffix.
 * If sufcnt is zero and graph 0, use outfile
 */
static char*
getName (int ng, int nb)
{
  char*        name;
  static char* buf;
  int          sz;

  if ((ng == 0) && (nb == 0)) name = outfile;
  else {
    if (!buf) {
      sz = strlen(outfile)+100; /* enough to handle '_<g>_<b>' */
      buf = (char*)malloc(sz);
    }
    if (suffix) {
      if (nb < 0) sprintf (buf, "%s_%d_T.%s", path, ng, suffix);
      else sprintf (buf, "%s_%d_%d.%s", path, ng, nb, suffix);
    }
    else {
      if (nb < 0) sprintf (buf, "%s_%d_T", path, ng);
      else sprintf (buf, "%s_%d_%d", path, ng, nb);
    }
    name = buf;
  }
  return name;
}

static void
gwrite (Agraph_t* g, int ng, int nb)
{
  FILE* outf;
  char* name;

  if (silent) return;
  if (!outfile) {
    agwrite (g, stdout);
    fflush(stdout);
  }
  else {
    name = getName(ng, nb);
    outf = fopen (name, "w");
    if (!outf) {
      fprintf (stderr, "Could not open %s for writing\n", name);
      perror ("bcomps");
      exit(1);
    }
    agwrite (g, outf);
    fclose (outf);
  }
}

static Agraph_t*
mkBlock (Agraph_t* g, bcstate* stp)
{
  Agraph_t* sg;

  stp->nComp++;
  sg = agsubg(g,blockName(g->name, stp->nComp));
  NEXTBLK(sg) = stp->blks;
  stp->blks = sg;
  return sg;
}

static void
dfs (Agraph_t* g, Agnode_t* u, bcstate* stp, Agnode_t* parent)
{
  Agnode_t*   v;
  Agedge_t*   e;
  Agedge_t*   ep;
  Agraph_t*   sg;

  stp->count++;
  Low(u) = N(u) = stp->count;
  for (e = agfstedge(g, u); e; e = agnxtedge (g, e, u)) {
    if ((v = e->head) == u) v = e->tail;
    if (v == u) continue;
    if (N(v) == 0) {
      push (&stp->stk, e);
      dfs (g, v, stp, u);
      Low(u) = min(Low(u),Low(v));
      if (Low(v) >= N(u)) { /* u is an articulation point */
        Cut(u) = 1;
        sg = mkBlock (g, stp);
        do {
          ep = pop(&stp->stk);
          aginsert (sg, ep->head);
          aginsert (sg, ep->tail);
        } while (ep != e);
      }
    }
    else if (parent != v) {
      Low(u) = min(Low(u),N(v));
      if (N(v) < N(u)) push(&stp->stk,e);
    }
  }
}

static void
nodeInduce (Agraph_t* g, Agraph_t* eg)
{
  Agnode_t    *n;
  Agedge_t    *e;

  for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
    for (e = agfstout(eg,n); e; e = agnxtout(eg,e)) {
      if (agcontains(g,e->head)) {
        aginsert(g,e);
      }
    }
  }
}

static void
addCutPts (Agraph_t* tree, Agraph_t* blk)
{
  Agnode_t*  n;
  Agnode_t*  bn;
  Agnode_t*  cn;

  bn = agnode (tree, blk->name);
  for (n = agfstnode(blk); n; n = agnxtnode(blk,n)) {
    if (Cut(n)) {
      cn = agnode (tree, n->name);
      agedge (tree, bn, cn);
    }
  }
}

static int
process (Agraph_t*  g, int gcnt)
{
  Agnode_t*  n;
  bcstate    state;
  Agraph_t*  blk;
  Agraph_t*  tree;
  int        bcnt;

  state.count = 0;
  state.nComp = 0;
  state.stk = 0;
  state.blks = 0;

  for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
    if (N(n) == 0) dfs(g, n, &state, 0);
  }
  for (blk = state.blks; blk; blk = NEXTBLK(blk)) {
    nodeInduce (blk, g);
  }
  if (external) {
    bcnt = 0;
    for (blk = state.blks; blk; blk = NEXTBLK(blk)) {
      gwrite(blk, gcnt, bcnt++);
    }
  }
  else gwrite (g, gcnt, 0);
  if (doTree) {
    tree = agopen ("blkcut_tree", AGFLAG_STRICT);
    for (blk = state.blks; blk; blk = NEXTBLK(blk))
      addCutPts (tree, blk);
    gwrite(tree, gcnt, -1);
    agclose (tree);
  }
  if (verbose) {
    int cuts = 0;
    bcnt = 0;
    for (blk = state.blks; blk; blk = NEXTBLK(blk)) bcnt++;
    for (n = agfstnode(g); n; n = agnxtnode(g,n))
      if (Cut(n)) cuts++;
    fprintf (stderr, "%s: %d blocks %d cutpoints\n",  g->name, bcnt, cuts);
  }
  if (state.blks && NEXTBLK(state.blks)) return 1;  /* >= 2 blocks */
  else return 0;
}

static char* useString =
"Usage: bcomps [-stvx?] [-o<out template>] <files>\n\
  -o - output file template\n\
  -s - don't print components\n\
  -t - emit block-cutpoint tree\n\
  -v - verbose\n\
  -x - external\n\
  -? - print usage\n\
If no files are specified, stdin is used\n";

static void
usage (int v)
{
    printf (useString);
    exit (v);
}

static void
split (char* name)
{
  char* sfx = 0;
  int   size;

  sfx = strrchr (name, '.');
  if (sfx) {
    size = sfx-name;
    suffix = sfx+1;
    path = (char*)malloc (size+1);
    strncpy (path, name, size);
    *(path+size) = '\0';
  }
  else {
    path = name;
  }
}

static void
init (int argc, char* argv[])
{
  int c;

  aginit ();
  while ((c = getopt(argc, argv, ":o:xstv?")) != -1) {
    switch (c) {
    case 'o':
      outfile = optarg;
      split (outfile);
      break;
    case 's':
      verbose = 1;
      silent = 1;
      break;
    case 'v':
      verbose = 1;
      break;
    case 't':
      doTree = 1;
      break;
    case 'x':
      external = 1;
      break;
    case '?':
      if (optopt == '?') usage(0);
      else fprintf(stderr,"bcomps: option -%c unrecognized - ignored\n", c);
      break;
    }
  }
  argv += optind;
  argc -= optind;

  if (argc) Files = argv;
}

int
main (int argc, char* argv[])
{
  Agraph_t*     g;
  ingraph_state ig;
  int           r = 0;
  int           gcnt = 0;

  init (argc, argv);
  newIngraph (&ig, Files, agread);

  while ((g = nextGraph(&ig)) != 0) {
      r |= process (g, gcnt);
      agclose (g);
      gcnt++;
  }

  return r;
}
