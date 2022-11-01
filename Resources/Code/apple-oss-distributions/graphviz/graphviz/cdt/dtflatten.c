/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/
#include	"dthdr.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

/*	Flatten a dictionary into a linked list.
**	This may be used when many traversals are likely.
**
**	Written by Kiem-Phong Vo (5/25/96).
*/

#if __STD_C
Dtlink_t* dtflatten(Dt_t* dt)
#else
Dtlink_t* dtflatten(dt)
Dt_t*	dt;
#endif
{
	reg Dtlink_t	*t, *r, *list, *last, **s, **ends;

	/* already flattened */
	if(dt->data->type&DT_FLATTEN )
		return dt->data->here;

	list = last = NIL(Dtlink_t*);
	if(dt->data->type&(DT_SET|DT_BAG))
	{	for(ends = (s = dt->data->htab) + dt->data->ntab; s < ends; ++s)
		{	if((t = *s) )
			{	if(last)
					last->right = t;
				else	list = last = t;
				while(last->right)
					last = last->right;
				*s = last;
			}
		}
	}
	else if(dt->data->type&(DT_LIST|DT_STACK|DT_QUEUE) )
		list = dt->data->head;
	else if((r = dt->data->here) ) /*if(dt->data->type&(DT_OSET|DT_OBAG))*/
	{	while((t = r->left) )
			RROTATE(r,t);
		for(list = last = r, r = r->right; r; last = r, r = r->right)
		{	if((t = r->left) )
			{	do	RROTATE(r,t);
				while((t = r->left) );

				last->right = r;
			}
		}
	}

	dt->data->here = list;
	dt->data->type |= DT_FLATTEN;

	return list;
}
