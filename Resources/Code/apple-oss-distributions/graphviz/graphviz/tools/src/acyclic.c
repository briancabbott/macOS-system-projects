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
 * Written by Stephen North
 * Updated by Emden Gansner
 */

typedef char Agraphinfo_t;
typedef char Agedgeinfo_t;
typedef struct {
        int             mark;
        int             onstack;
} Agnodeinfo_t;

#define ND_mark(n) (n)->u.mark
#define ND_onstack(n) (n)->u.onstack

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include <stdio.h>
#include <graph.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include "compat_getopt.h"
#endif

static FILE* inFile;
static FILE* outFile;
static int   doWrite = 1;
static int   Verbose;
static char* cmd;

static void
addRevEdge (Agraph_t* g, Agedge_t*   e)
{
	Agedge_t*	 reve;
    char*        tmps;
    char**       attrs;
    extern char* agstrdup(char  *);

    attrs = g->proto->e->attr;
    g->proto->e->attr = e->attr;
    tmps = e->attr[0];
    e->attr[0] = "";
    reve = agedge(g,e->head,e->tail);
    e->attr[0] = tmps;
    g->proto->e->attr = attrs;

      /* copy key attribute, and reverse head and tail port attributes */
    reve->attr[0] = agstrdup (tmps);
    tmps = reve->attr[1];
    reve->attr[1] = reve->attr[2];
    reve->attr[2] = tmps;
}

static int
dfs(Agraph_t* g, Agnode_t* t, int hasCycle)
{
	Agedge_t*	e;
	Agedge_t*	f;
	Agnode_t*	h;

   	ND_mark(t) = 1;
    ND_onstack(t) = 1;
    for (e = agfstout(g, t); e ; e = f) {
      f = agnxtout(g, e);
      if (e->tail == e->head) continue;
      h = e->head;
      if (ND_onstack(h)) {
        if (AG_IS_STRICT(g)) {
          if (agfindedge (g, h, t) == 0)
            addRevEdge (g, e);
        }
        else addRevEdge (g, e);
        agdelete (g, e);
        hasCycle = 1;
      }
      else if (ND_mark(h) == 0) hasCycle = dfs(g,h,hasCycle);
    }
	ND_onstack(t) = 0;
	return hasCycle;
}

static char* useString =
"Usage: %s [-nv?] [-o outfile] <file>\n\
  -o <file> - put output in <file>\n\
  -n        - do not output graph\n\
  -v        - verbose\n\
  -?        - print usage\n";

static void
usage (int v)
{
  fprintf (stderr, useString, cmd);
  exit (v);
}

static FILE*
openFile (char* name, char* mode)
{
    FILE* fp;
    char* modestr;

    fp = fopen (name, mode);
    if (!fp) {
      if (*mode == 'r') modestr = "reading";
      else modestr = "writing";
      fprintf(stderr,"%s: could not open file %s for %s\n", 
        cmd, name, modestr);
      exit(-1);
    }
    return (fp);
}

static void
init (int argc, char* argv[])
{
    int   c;

    cmd = argv[0];
    aginit();

    while ((c = getopt(argc, argv, ":?vno:")) != -1)
      switch (c) {
          case 'o':
            outFile = openFile (optarg, "w");
            break;
          case 'n':
            doWrite = 0;
            break;
          case 'v':
            Verbose = 1;
            break;
          case '?':
            if (optopt == '?') usage (0);
            else {
              fprintf(stderr,"%s: option -%c unrecognized\n", cmd, optopt);
              usage (-1);
            }
            break;
          case ':':
            fprintf(stderr,"%s: missing argument for option -%c\n", 
              cmd, optopt);
            usage(-1);
            break;
      }
    if (optind < argc) {
      inFile = openFile (argv[optind], "r");
    }
    else inFile = stdin;
    if (!outFile) outFile = stdout;

}

int main(int argc, char* argv[])
{
    Agraph_t*	g;
	Agnode_t*	n;
    int         rv = 0;
    
    init(argc, argv);

    if ((g = agread(inFile)) != 0) {
      if (AG_IS_DIRECTED(g)) {
        for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
		  if (ND_mark(n) == 0) rv |= dfs(g,n,0);
	    }
        if (doWrite) {
	    agwrite (g,outFile);
	    fflush(outFile);
	  }
        if (Verbose) {
          if (rv)
            fprintf (stderr, "Graph %s has cycles\n", g->name);
          else
            fprintf (stderr, "Graph %s is acyclic\n", g->name);
	    }
	  }
      else {
        rv = 2;
        if (Verbose)
          fprintf (stderr, "Graph %s is undirected\n", g->name);
      }
      exit(rv);
    }
    else exit (-1);
}

