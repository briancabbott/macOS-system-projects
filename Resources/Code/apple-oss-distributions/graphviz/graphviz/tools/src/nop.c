/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <agraph.h>
#include <ingraphs.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include	<unistd.h>
#endif

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include "compat_getopt.h"
#endif

char **Files;

static char* useString = 
"Usage: nop [-?] <files>\n\
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

static Agraph_t*
gread (FILE* fp)
{
  return agread(fp,(Agdisc_t*)0);
}

int main(int argc,char **argv)
{
  Agraph_t*     g;
  ingraph_state ig;

  init (argc, argv);
  newIngraph (&ig, Files, gread);
  
  while ((g = nextGraph(&ig)) != 0) {
      agwrite(g,stdout);
      agclose (g);
  }

  exit(0);
}

