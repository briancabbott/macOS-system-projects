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
#include "simple.h"
#include <stdlib.h>
#include <render.h>


extern void
find_intersection(struct vertex *l,
                  struct vertex *m,
                  struct intersection ilist[],
                  struct data *input);

void
find_ints(struct vertex vertex_list[],
          struct polygon polygon_list[],
          struct data *input,
          struct intersection   ilist[])
{       
	int i,j,k,gt();
	struct active_edge_list all;
	struct active_edge *new,*tempa;
	struct vertex *pt1,*pt2,*templ,**pvertex;

	input->ninters = 0;
	all.first = all.final = 0;	 all.number = 0;

	pvertex = N_GNEW(input->nvertices,struct vertex *);

	for (i = 0; i < input->nvertices ; i++ )
		pvertex[i] = vertex_list + i ;	

/* sort vertices by x coordinate	*/
	qsort(pvertex,input->nvertices,sizeof(struct vertex *),gt);

/* walk through the vertices in order of increasing x coordinate	*/
	for (i = 0 ; i < input->nvertices ; i++ )  {
		pt1 = pvertex[i]; templ = pt2 = prior(pvertex[i]);
		for (k = 0 ; k < 2 ; k++ )      {/* each vertex has 2 edges*/
			switch (gt(&pt1,&pt2))              {

case -1:  /* forward edge, test and insert	*/	

	for (tempa=all.first,j=0 ; j<all.number ; j++ , tempa = tempa->next )
		find_intersection(tempa->name,templ,ilist,input);   /* test*/

	new = GNEW(struct active_edge);
	if (all.number == 0) { all.first = new; new->last = 0; } /* insert */
		else { all.final->next = new; new->last = all.final; }

	new->name = templ;	 new->next = 0;
	templ->active = new; all.final = new;	 all.number++;

	break;	/* end of case -1	*/

case 1:	/* backward edge, delete	*/

	if( (tempa = templ->active) == 0) 	{
		agerr(AGERR, "trying to delete a non line\n");
		exit(1);			 }
	if (all.number == 1) all.final = all.first = 0;  /* delete the line*/
	    else if (tempa == all.first)     
		    { all.first =  all.first->next; all.first->last = 0; }
		else if (tempa == all.final)  {
		        all.final = all.final->last; all.final->next = 0;  }
    		    else        { tempa->last->next = tempa->next;
				tempa->next->last = tempa->last; }
	free((char *) tempa);
	all.number--; templ->active = 0;
	break;	/* end of case 1	*/

}       /* end switch   */

	pt2 = after(pvertex[i]);	 templ =  pvertex[i];/*second neighbor*/
	}       /* end k for loop       */
	}       /* end i for loop       */
}

int gt(i,j)
struct vertex **i,**j;
{     /* i > j if i.x > j.x or i.x = j.x and i.y > j.y	*/ 
	double t;
	if ((t = (*i)->pos.x - (*j)->pos.x) != 0.) return( (t > 0.) ? 1 : -1 );
	if ((t = (*i)->pos.y - (*j)->pos.y) == 0.) return(0); 
	else return( (t > 0.) ? 1 : -1 );
}
