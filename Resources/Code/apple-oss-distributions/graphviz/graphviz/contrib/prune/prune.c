/*
  $Id: prune.c,v 1.1 2004/05/04 21:50:44 yakimoto Exp $

  Copyright (c) 2002 Marcus Harnisch <marcus.harnisch@gmx.net>

  This is free software. It is provided to you without any warranty
  that this software is useful or that it *won't* destroy your
  computer (It didn't destroy mine if that makes you feel better).

  You are granted the right to use and redistribute the compiled
  software its source code or parts of the source code, provided that
  the copyright notice and the license terms will not be removed AND
  that this use does not violate the license that comes with the
  graphviz software.

*/
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_GETOPT_H
#include <getopt.h>
#else
#include "compat_getopt.h"
#endif

#include <agraph.h>
#include <ingraphs.h>

#include <generic_list.h>

/* structure to hold an attribute specified on the commandline */
typedef struct strattr_s
{
      char* n;
      char* v;
} strattr_t;

void remove_child(Agraph_t *graph, Agnode_t* node);
void help_message(const char* progname);

generic_list_t*	addattr(generic_list_t* l, char* a);
generic_list_t*	addnode(generic_list_t* l, char* n);

int verbose = 0; /* Flag to indicate verbose message output */

/* wrapper to match libagraph conventions with libingraph */
Agraph_t* gread (FILE* fp);

#define NDNAME "mk"

typedef struct {
  Agrec_t   h;
  int       mark;
} ndata;

 /* The following macros for marking nodes rely on the ndata record
  * being locked at the front of a node's list of records.
  */
#define NDATA(n)    ((ndata*)(AGDATA(n)))
#define MARKED(n)  (((NDATA(n))->mark)&1)
#define MARK(n)  (((NDATA(n))->mark) = 1)
#define UNMARK(n)  (((NDATA(n))->mark) = 0)

int
main(int argc, char **argv)
{
   int      c;
   char     *progname;

   ingraph_state ig;

   Agraph_t *graph;
   Agnode_t *node;
   Agedge_t *edge;
   Agedge_t *nexte;
   Agsym_t  *attr;

   char **files;

   generic_list_t *attr_list;
   generic_list_t *node_list;

   unsigned long i,j;

   opterr = 0;

   progname = strrchr(argv[0], '/');
   if (progname == NULL)
   {
      progname = argv[0];
   }
   else
   {
      progname++; /* character after last '/' */
   }

   attr_list = new_generic_list(16);
   node_list = new_generic_list(16);

   while ((c = getopt(argc, argv, "hvn:N:")) != -1) 
   {
      switch(c)
      {
	 case 'N':
	 {
	    attr_list = addattr(attr_list, optarg);
	    break;
	 }
	 case 'n':
	 {
	    node_list = addnode(node_list, optarg);
	    break;
	 }
	 case 'h':
	 {
	    help_message(progname);
	    exit(EXIT_SUCCESS);
	    break;
	 }
	 case 'v':
	 {
	    verbose = 1;
	    break;
	 }
	 case '?':
	    if (isprint (optopt))
	    {
               fprintf (stderr, "Unknown option `-%c'.\n", optopt);
	    }
	    else
	    {
               fprintf (stderr, "Unknown option character `\\x%X'.\n", optopt);
	    }
	    exit(EXIT_FAILURE);
	    break;

	 default:
	    help_message(progname);
	    exit(EXIT_FAILURE);
	    break;
      }
   }

   /* Any arguments left? */
   if (optind < argc)
   {
      files = &argv[optind];
   }
   else
   {
      files = NULL;
   }

   newIngraph (&ig, files, gread);
   while ((graph = nextGraph(&ig)) != NULL) {
      if (agisdirected(graph) == 0) {
	 fprintf(stderr, "*** Error: Graph is undirected! Pruning works only with directed graphs!\n");
	 exit(EXIT_FAILURE);
      }

      /* attach node data for marking to all nodes */
      aginit(graph,AGNODE,NDNAME,sizeof(ndata),1);

      /* prune all nodes specified on the commandline */
      for (i=0; i < node_list->used; i++) {
	 if (verbose == 1) fprintf(stderr, "Pruning node %s\n", (char*)node_list->data[i]);

	 /* check whether a node of that name exists at all */
	 node = agnode(graph, (char*)node_list->data[i], 0);
	 if (node == NULL) {
	    fprintf(stderr, "*** Warning: No such node: %s -- gracefully skipping this one\n", (char*)node_list->data[i]);
	 }
	 else {
    	    MARK(node); /* Avoid cycles */
	    /* Iterate over all outgoing edges */
	    for (edge = agfstout(node); edge; edge = nexte) {
               nexte = agnxtout (edge);
               if (aghead(edge) != node) {  /* non-loop edges */
	         if (verbose == 1) fprintf(stderr, "Processing descendant: %s\n", agnameof(aghead(edge)));
	         remove_child(graph, aghead(edge));
	         agdelete(graph, edge);
	      }
	    }
    	    UNMARK(node); /* Unmark so that it can be removed in later passes */

	    /* Change attribute (e.g. border style) to show that node has been pruneed */
	    for (j=0; j < attr_list->used; j++) {
	       /* create attribute if it doesn't exist and set it */
	       attr = agattr(graph, AGNODE, ((strattr_t*)attr_list->data[j])->n, "");
	       if (attr == NULL) {
		  fprintf(stderr, "Couldn't create attribute: %s\n", ((strattr_t*)attr_list->data[j])->n);
		  exit(EXIT_FAILURE);
	       }
	       agxset(node, attr, ((strattr_t*)attr_list->data[j])->v);
	    }
	 }
      }
      agwrite(graph, stdout);
      agclose(graph);
   }
   free(attr_list);
   free(node_list);
   exit(EXIT_SUCCESS);
}

void
remove_child(Agraph_t *graph, Agnode_t* node) {
   Agedge_t *edge;
   Agedge_t *nexte;

   /* Avoid cycles */
   if MARKED(node) return;
   MARK(node);

   /* Skip nodes with more than one parent */
   edge = agfstin(node);
   if (edge && (agnxtin(edge) != NULL)) {
      UNMARK(node);
      return;
   }

   /* recursively remove children */
   for (edge = agfstout(node); edge; edge = nexte) {
      nexte = agnxtout(edge);
      if (aghead(edge) != node) {
         if (verbose) fprintf(stderr, "Processing descendant: %s\n", agnameof(aghead(edge)));
         remove_child(graph, aghead(edge));
         agdeledge(edge);
      }
   }

   agdelnode(node);
   return;
}

void
help_message(const char* progname) {
   fprintf (stderr, "\
Usage: %s [options] [<files>]\n\
\n\
Options:\n\
  -h :           Print this message\n\
  -n<node> :     Name node to prune.\n\
  -N<attrspec> : Attribute specification to apply to pruned nodes\n\
\n\
Both options `-n' and `-N' can be used multiple times on the command line.\n", progname);
}

/* wrapper to match libagraph conventions with libingraph */
Agraph_t*
gread (FILE* fp) {
  return agread(fp,(Agdisc_t*)0);
}

/* add element to attribute list */
generic_list_t*
addattr(generic_list_t *l, char *a) {
   char      *p;
   strattr_t *sp;
   
   sp = (strattr_t *)malloc(sizeof(strattr_t));
   if (sp == NULL) {
      perror("[addattr()->malloc()]");
      exit(EXIT_FAILURE);
   }

   /* Split argument spec. at first '=' */
   p = strchr(a, '=');
   if (p == NULL) {
      fprintf(stderr, "Invalid argument specification: %s\n", a);
      exit(EXIT_FAILURE);
   }
   *(p++) = '\0';
   
   /* pointer to argument name */
   sp->n = strdup(a);
   if (sp->n == NULL) {
      perror("[addattr()->strdup()]");
      exit(EXIT_FAILURE);
   }
   
   /* pointer to argument value */
   sp->v=strdup(p);
   if (sp->v == NULL) {
      perror("[addattr()->strdup()]");
      exit(EXIT_FAILURE);
   }

   return add_to_generic_list(l, (gl_data)sp);
}

/* add element to node list */
generic_list_t*
addnode(generic_list_t *l, char *n) {
   char *sp;

   sp = strdup(n);
   if (sp == NULL) {
      perror("[addnode()->strdup()]");
      exit(EXIT_FAILURE);
   }

   return add_to_generic_list(l, (gl_data)sp);
}
