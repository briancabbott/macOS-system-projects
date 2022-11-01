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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* if NC changes, a bunch of scanf calls below are in trouble */
#define	NC	3		/* size of HSB color vector */

typedef struct Agnodeinfo_t {
	double	relrank;	/* coordinate of its rank, smaller means lower rank */
	double	x[NC];		/* color vector */
} Agnodeinfo_t;

typedef struct Agedgeinfo_t  {char for_ansi_C;} Agedgeinfo_t;
typedef struct Agraphinfo_t {char for_ansi_C;} Agraphinfo_t;

#define ND_relrank(n) (n)->u.relrank
#define ND_x(n) (n)->u.x

#include <graph.h>
#include <ingraphs.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include	<unistd.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include "compat_getopt.h"
#endif

double Defcolor[NC] = {0.0, 0.0, 1.0}; 	/* white */
int		Forward = 1;	/* how to propagate colors w.r.t. ranks */
int		LR = 0;			/* rank orientation */

int		AdjustSaturation;
double	MinRankSaturation;
double	MaxRankSaturation;

extern char * colorxlate(char* str, char* buf);

static int 
cmpf(Agnode_t **n0, Agnode_t **n1)
{
	double		t;
	t =  ((*n0)->u.relrank - (*n1)->u.relrank);
	if (t < 0.0) return -1;
	if (t > 0.0) return 1;
	return 0;
}

static void
setcolor(char* p,double* v)
{
	char	buf[64];
	if ((sscanf(p,"%lf %lf %lf",v,v+1,v+2) != 3) && p[0]) {
		colorxlate(p,buf);
		sscanf(buf,"%lf %lf %lf",v,v+1,v+2);
	}
}

static char **Files;

static char* useString =
"Usage: gvcolor [-?] <files>\n\
  -? - print usage\n\
If no files are specified, stdin is used\n";

static void
usage (int v)
{
    printf (useString);
    exit (v);
}

static void
init (int argc, char* argv[])
{
  int c;

  while ((c = getopt(argc, argv, ":?")) != -1) {
    switch (c) {
    case '?':
      if (optopt == '?') usage(0);
      else fprintf(stderr,"gc: option -%c unrecognized - ignored\n", c);
      break;
    }
  }
  argv += optind;
  argc -= optind;

  if (argc) Files = argv;
}

static void
color (Agraph_t* g)
{
	int			nn,i,j,cnt;
	Agnode_t	*n,*v,**nlist;
	Agedge_t	*e;
	char		*p;
	double		x,y,maxrank = 0.0;
	double 		sum[NC],d,lowsat,highsat;

	if (agfindattr(g->proto->n,"pos") == NULL) {
		fprintf(stderr,"graph must be run through 'dot' before 'gvcolor'\n");
		exit(1);
	}
	if (agfindattr(g->proto->n,"style") == NULL)
		agnodeattr(g,"style","filled");
	if ((p = agget(g,"Defcolor"))) setcolor(p,Defcolor);

	if ((p = agget(g,"rankdir")) && (p[0] == 'L')) LR = 1;
	if ((p = agget(g,"flow")) && (p[0] == 'b')) Forward = 0;
	if ((p = agget(g,"saturation"))) {
		if (sscanf(p,"%lf,%lf",&lowsat,&highsat) == 2) {
			MinRankSaturation = lowsat;
			MaxRankSaturation = highsat;
			AdjustSaturation = 1;
		}
	}

	/* assemble the sorted list of nodes and store the initial colors */
	nn = agnnodes(g);
	nlist = (Agnode_t**)malloc(nn * sizeof(Agnode_t*));
	i = 0;
	for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
		nlist[i++] = n;
		if ((p = agget(n,"color"))) setcolor(p,ND_x(n));
		p = agget(n,"pos");
		sscanf(p,"%lf,%lf",&x,&y);
		ND_relrank(n) = (LR? x : y);
		if (maxrank < ND_relrank(n)) maxrank = ND_relrank(n);
	}
	if (LR != Forward) for (i = 0; i < nn; i++) {
		n = nlist[i];
		ND_relrank(n) = maxrank - ND_relrank(n);
	}
	qsort((void*)nlist,(size_t)nn,sizeof(Agnode_t*),(int(*)(const void*, const void*))cmpf);

	/* this is the pass that pushes the colors through the edges */
	for (i = 0; i < nn; i++) {
		n = nlist[i];

		/* skip nodes that were manually colored */
		cnt = 0;
		for (j = 0; j < NC; j++) if (ND_x(n)[j] != 0.0) cnt++;
		if (cnt > 0) continue;

		for (j = 0; j < NC; j++) sum[j] = 0.0;
		cnt = 0;
		for (e = agfstedge(g,n); e; e = agnxtedge(g,e,n)) {
			v = e->head;
			if (v == n) v = e->tail;
			d = ND_relrank(v) - ND_relrank(n) - 0.01;
			if (d < 0) {
				double t = 0.0;
				for (j = 0; j < NC; j++) {
					t += ND_x(v)[j];
					sum[j] += ND_x(v)[j];
				}
				if (t > 0.0) cnt++;
			}
		}
		if (cnt) for (j = 0; j < NC; j++) ND_x(n)[j] = sum[j] / cnt;
	}

	/* apply saturation adjustment and convert color to string */
	for (i = 0; i < nn; i++) {
		double	h,s,b,t;
		char buf[64];

		n = nlist[i];

		t = 0.0;
		for (j = 0; j < NC; j++) t += ND_x(n)[j];
		if (t > 0.0) {
			h = ND_x(n)[0];
			if (AdjustSaturation) {
				s = ND_relrank(n) / maxrank;
				if (!Forward) s = 1.0 - s;
				s = MinRankSaturation
					+ s * (MaxRankSaturation - MinRankSaturation);
			}
			else s = 1.0;
			s = s * ND_x(n)[1];
			b = ND_x(n)[2];
		}
		else {h = Defcolor[0]; s = Defcolor[1]; b = Defcolor[2];}
		sprintf(buf,"%f %f %f",h,s,b);
		agset(n,"color",buf);
	}
}

int main(int argc,char **argv)
{
	Agraph_t	*g;
    ingraph_state ig;

    init (argc, argv);
    newIngraph (&ig, Files, agread);
	aginit();

    while ((g = nextGraph(&ig)) != 0) {
      color (g);
      agwrite(g,stdout);
      fflush(stdout);
      agclose (g);
    }

    exit(0);
}


