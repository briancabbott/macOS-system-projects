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

/*	Change search method.
**
**	Written by Kiem-Phong Vo (05/25/96)
*/

#if __STD_C
Dtmethod_t* dtmethod(Dt_t* dt, Dtmethod_t* meth)
#else
Dtmethod_t* dtmethod(dt, meth)
Dt_t*		dt;
Dtmethod_t*	meth;
#endif
{
	reg Dtlink_t	*list, *r;
	reg Dtdisc_t*	disc = dt->disc;
	reg Dtmethod_t*	oldmeth = dt->meth;

	if(!meth || meth->type == oldmeth->type)
		return oldmeth;

	if(disc->eventf &&
	   (*disc->eventf)(dt,DT_METH,(Void_t*)meth,disc) < 0)
		return NIL(Dtmethod_t*);

	/* get the list of elements */
	list = dtflatten(dt);

	if(dt->data->type&(DT_LIST|DT_STACK|DT_QUEUE) )
		dt->data->head = NIL(Dtlink_t*);
	else if(dt->data->type&(DT_SET|DT_BAG) )
	{	if(dt->data->ntab > 0)
			(*dt->memoryf)(dt,(Void_t*)dt->data->htab,0,disc);
		dt->data->ntab = 0;
		dt->data->htab = NIL(Dtlink_t**);
	}

	dt->data->here = NIL(Dtlink_t*);
	dt->data->type = (dt->data->type&~(DT_METHODS|DT_FLATTEN)) | meth->type;
	dt->meth = meth;
	if(dt->searchf == oldmeth->searchf)
		dt->searchf = meth->searchf;

	if(meth->type&(DT_LIST|DT_STACK|DT_QUEUE) )
	{	if(!(oldmeth->type&(DT_LIST|DT_STACK|DT_QUEUE)) )
		{	if((r = list) )
			{	reg Dtlink_t*	t;
				for(t = r->right; t; r = t, t = t->right )
					t->left = r;
				list->left = r;
			}
		}
		dt->data->head = list;
	}
	else if(meth->type&(DT_OSET|DT_OBAG))
	{	dt->data->size = 0;
		while(list)
		{	r = list->right;
			(*meth->searchf)(dt,(Void_t*)list,DT_RENEW);
			list = r;
		}
	}
	else if(!((meth->type&DT_BAG) && (oldmeth->type&DT_SET)) )
	{	int	rehash;
		if((meth->type&(DT_SET|DT_BAG)) && !(oldmeth->type&(DT_SET|DT_BAG)))
			rehash = 1;
		else	rehash = 0;

		dt->data->size = dt->data->loop = 0;
		while(list)
		{	r = list->right;
			if(rehash)
			{	reg Void_t* key = OBJ(list,disc->link);
				key = KEY(key,disc->key,disc->size);
				list->hash = HASH(dt,key,disc,disc->size);
			}
			(void)(*meth->searchf)(dt,(Void_t*)list,DT_RENEW);
			list = r;
		}
	}

	return oldmeth;
}
