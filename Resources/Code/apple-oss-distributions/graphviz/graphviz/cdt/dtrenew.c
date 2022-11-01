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

/*	Renew the object at the current finger.
**
**	Written by Kiem-Phong Vo (5/25/96)
*/

#if __STD_C
Void_t* dtrenew(Dt_t* dt, reg Void_t* obj)
#else
Void_t* dtrenew(dt, obj)
Dt_t*		dt;
reg Void_t*	obj;
#endif
{
	reg Void_t*	key;
	reg Dtlink_t	*e, *t, **s;
	reg Dtdisc_t*	disc = dt->disc;

	UNFLATTEN(dt);

	if(!(e = dt->data->here) || OBJ(e,disc->link) != obj)
		return NIL(Void_t*);

	if(dt->data->type&(DT_STACK|DT_QUEUE|DT_LIST))
		return obj;
	else if(dt->data->type&(DT_OSET|DT_OBAG) )
	{	if(!e->right )	/* make left child the new root */
			dt->data->here = e->left;
		else		/* make right child the new root */
		{	dt->data->here = e->right;

			/* merge left subtree to right subtree */
			if(e->left)
			{	for(t = e->right; t->left; t = t->left)
					;
				t->left = e->left;
			}
		}
	}
	else /*if(dt->data->type&(DT_SET|DT_BAG))*/
	{	s = dt->data->htab + HINDEX(dt->data->ntab,e->hash);
		if((t = *s) == e)
			*s = e->right;
		else
		{	for(; t->right != e; t = t->right)
				;
			t->right = e->right;
		}
		key = KEY(obj,disc->key,disc->size);
		e->hash = HASH(dt,key,disc,disc->size);
		dt->data->here = NIL(Dtlink_t*);
	}

	dt->data->size -= 1;
	return (*dt->meth->searchf)(dt,(Void_t*)e,DT_RENEW) ? obj : NIL(Void_t*);
}
