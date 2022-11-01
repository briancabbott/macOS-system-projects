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

#include <stdio.h>
#include <aghdr.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

/* a default ID allocator that works off the shared string lib */

static void *idopen(Agraph_t *g) {return g;}

static long idmap(void *state, int objtype, char *str, unsigned long *id, int createflag)
{
	char		*s;
	static unsigned long		ctr = 1;

	NOTUSED(objtype);
	if (str) {
		Agraph_t	*g;
		g = state;
		if (createflag)
			s = agstrdup(g,str);
		else
			s = agstrbind(g,str);
		*id = (unsigned long)s;
	}
	else {
		*id = ctr;
		ctr += 2;
	}
	return TRUE;
}

	/* we don't allow users to explicitly set IDs, either */
static long idalloc(void *state, int objtype, unsigned long request) {	
	NOTUSED(state); NOTUSED (objtype); NOTUSED(request);
	return FALSE;
}

static void idfree(void *state, int objtype, unsigned long id) 
{
	NOTUSED(objtype);
	if (id % 2 == 0) agstrfree((Agraph_t*)state,(char*)id);
}

static char *idprint(void *state, int objtype, unsigned long id)
{
	NOTUSED(state);
	NOTUSED(objtype);
	if (id %2 == 0) return (char*)id;
	else return NILstr;
}

static void idclose(void *state) {NOTUSED(state);}

Agiddisc_t AgIdDisc = {
	idopen,
	idmap,
	idalloc,
	idfree,
	idprint,
	idclose
};

/* aux functions incl. support for disciplines with anonymous IDs */

int agmapnametoid(Agraph_t *g, int objtype, char *str, unsigned long *result, int createflag)
{
	int		rv;

	if (str && (str[0] != LOCALNAMEPREFIX)) {
		rv = AGDISC(g,id)->map(AGCLOS(g,id),objtype,str,result,createflag);
			if (rv) return rv;
	}

	/* either an internal ID, or disc. can't map strings */
	if (str) {
		rv = aginternalmaplookup(g, objtype, str, result);
		if (rv) return rv;
	}
	else rv = 0;

	if (createflag) {
		/* get a new anonymous ID, and store in the internal map */
		rv = AGDISC(g,id)->map(AGCLOS(g,id),objtype,NILstr,result,createflag);
		if (rv && str) aginternalmapinsert(g, objtype, str, *result);
	}
	return rv;
}

int agallocid(Agraph_t *g, int objtype, unsigned long request)
{
	return AGDISC(g,id)->alloc(AGCLOS(g,id),objtype,request);
}

void agfreeid(Agraph_t *g, int objtype, unsigned long id)
{
	(void) aginternalmapdelete(g, objtype, id);
	(AGDISC(g,id)->free)(AGCLOS(g,id),objtype,id);
}

char *agnameof(void *obj)
{
	Agraph_t	*g;
	char		*rv;
	char        buf[32];

	/* perform internal lookup first */
	g = agraphof(obj);
	if ((rv = aginternalmapprint(g, AGTYPE(obj), AGID(obj))))
		return rv;

	if (AGDISC(g,id)->print) {
		if ((rv = AGDISC(g,id)->print(AGCLOS(g,id),AGTYPE(obj),AGID(obj))))
			return rv;
	}
	if (AGTYPE(obj) != AGEDGE) sprintf(buf,"%c%ld",LOCALNAMEPREFIX,AGID(obj));
	else buf[0] = 0;
	return agstrdup(g,buf);
}
