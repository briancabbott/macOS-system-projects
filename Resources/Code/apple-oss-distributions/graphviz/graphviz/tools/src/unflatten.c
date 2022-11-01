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

#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#ifdef HAVE_UNISTD_H
#include	<unistd.h>
#endif
#include    <agraph.h>
#include    <ingraphs.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include "compat_getopt.h"
#endif

static int         Do_fans = 0;
static int         MaxMinlen = 0;
static int         ChainLimit = 0;
static int         ChainSize = 0;
static Agnode_t*   ChainNode;
static FILE*       outFile;
static char*       cmd;

static int 
isleaf(Agnode_t *n)
{
  return (agdegree(n,TRUE,TRUE) == 1);
}

static int 
ischainnode(Agnode_t *n)
{
  return ((agdegree(n,TRUE,FALSE) == 1) && agdegree(n,FALSE,TRUE) == 1);
}

static void 
adjustlen(Agedge_t *e, Agsym_t *sym, int newlen)
{
  char    buf[10];

  sprintf(buf,"%d",newlen);
  agxset(e,sym,buf);
}

static Agsym_t*
bindedgeattr(Agraph_t *g, char *str)
{
  return agattr(g, AGEDGE, str, "");
}

static void 
transform(Agraph_t *g)
{
  Agnode_t*     n;
  Agedge_t*     e;
  char*         str;
  Agsym_t         *m_ix, *s_ix;
  int           cnt, d;

  m_ix = bindedgeattr(g,"minlen");
  s_ix = bindedgeattr(g,"style");

  for (n = agfstnode(g); n; n = agnxtnode(n)) {
    d = agdegree(n,TRUE,TRUE);
    if (d == 0) {
      if (ChainLimit < 1) continue;
      if (ChainNode) {
        e = agedge(ChainNode,n,"",TRUE);
        agxset(e,s_ix,"invis");
        ChainSize++;
        if (ChainSize < ChainLimit) ChainNode = n;
        else {ChainNode = NULL; ChainSize = 0;}
      }
      else ChainNode = n;
    }
    else if (d > 1) {
      if (MaxMinlen < 1) continue;
      cnt = 0;
      for (e = agfstin(n); e; e = agnxtin(e)) {
        if (isleaf(agtail(e))) {
          str = agxget(e,m_ix);
          if (str[0] == 0) {
            adjustlen(e,m_ix,(cnt % MaxMinlen) + 1);
            cnt++;
          }
        }
      }
 
      cnt = 0;
      for (e = agfstout(n); e; e = agnxtout(e)) {
        if (isleaf(e->node) || (Do_fans && ischainnode(e->node))) {
          str = agxget(e,m_ix);
          if (str[0] == 0) adjustlen(e,m_ix,(cnt % MaxMinlen) + 1);
          cnt++;
        }
      }
    }
  }
}


static char* useString =
"Usage: %s [-f?] [-l l] [-c l] [-o outfile] <files>\n\
  -o <file> - put output in <file>\n\
  -f        - adjust fanin/fanout chains\n\
  -l <len>  - stagger length of leaf edges between [1,l]\n\
  -c <len>  - put disconnected nodes in chains of length l\n\
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

static char** 
scanargs(int argc, char **argv)
{
  int        c,ival;

  cmd = argv[0];

  while ((c = getopt(argc,argv,":?fl:c:o:")) != -1) {
    switch (c) {
    case 'f':    
      Do_fans = 1;
      break;
    case 'l':
      ival = atoi(optarg);
      if (ival > 0) MaxMinlen = ival;
      break;
    case 'c':    
      ival = atoi(optarg);
      if (ival > 0) ChainLimit = ival;
      break;
    case 'o':   
      outFile = openFile (optarg, "w");
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
  }
  argv += optind;
  argc -= optind;
  
  if (!outFile) outFile = stdout;
  if (argc) return argv;
  else return 0;
}

static Agraph_t*
gread (FILE* fp)
{
  return agread(fp,(Agdisc_t*)0);
}

int main(int argc,char ** argv)
{
  Agraph_t*       g;
  ingraph_state   ig;
  char**          files;

  files = scanargs(argc,argv);
  newIngraph (&ig, files, gread);
  while ((g = nextGraph(&ig))) {
    transform(g);
    agwrite(g,outFile);
  }
  return 0;
}

