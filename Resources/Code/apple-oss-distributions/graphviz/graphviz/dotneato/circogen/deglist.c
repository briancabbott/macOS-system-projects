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

#include <deglist.h>
#include <circular.h>
#include <blockpath.h>
#include <assert.h>

typedef struct {
	Dtlink_t   link;
	int        deg;
	Agnode_t*  np;        /* linked list of nodes of same degree */
} degitem;

static degitem*
mkItem (Dt_t* d,degitem* obj,Dtdisc_t* disc)
{
  degitem*  ap = GNEW(degitem);

  ap->np = NULL;
  ap->deg = obj->deg;
  return ap;
}

static void
freeItem (Dt_t* d,degitem* obj,Dtdisc_t* disc)
{
  free (obj);
}

static int
cmpDegree(Dt_t* d, int* key1, int* key2, Dtdisc_t* disc)
{
  if (*key1 < *key2) return -1;
  else if (*key1 > *key2) return 1;
  else return 0;
}

static Dtdisc_t nodeDisc = {
  offsetof(degitem,deg),        /* key */
  sizeof(int),                  /* size */
  offsetof(degitem,link),        /* link */
  (Dtmake_f)mkItem,
  (Dtfree_f)freeItem,
  (Dtcompar_f)cmpDegree,
  (Dthash_f)0,
  (Dtmemory_f)0,
  (Dtevent_f)0
};

/* mkDeglist:
 * Create an empty list of nodes.
 */
deglist_t* 
mkDeglist()
{
	deglist_t*  s = dtopen (&nodeDisc, Dtoset);
	return s;
}

/* freeDeglist:
 * Delete the node list.
 * Nodes are not deleted.
 */
void 
freeDeglist(deglist_t* s)
{
	dtclose (s);
}

/* insertDeglist:
 * Add a node to the node list.
 * Nodes are kept sorted by DEGREE, smallest degrees first.
 */
void 
insertDeglist(deglist_t* ns, Agnode_t* n)
{
	degitem  key;
	degitem* kp;

	key.deg = DEGREE(n);
	kp = dtinsert(ns, &key);
	ND_next(n) = kp->np;
	kp->np = n;
}

/* removeDeglist:
 * Remove n from list, if it is in the list.
 */
void
removeDeglist(deglist_t* list, Agnode_t* n)
{
	degitem key;
	degitem* ip;
	Agnode_t* np;
	Agnode_t* prev;

	key.deg = DEGREE(n);
	ip = (degitem*)dtsearch (list, &key);
	assert (ip);
	if (ip->np == n) {
		ip->np = ND_next(n);
		if (ip->np == NULL) 
			dtdelete (list, ip);
	}
	else {
		prev = ip->np;
		np = ND_next(prev);
		while (np && (np != n)) {
			prev = np;
			np = ND_next(np);
		}
		if (np) ND_next(prev) = ND_next(np);
	}
}

/* firstDeglist:
 * Return the first node in the list (smallest degree)
 * Remove from list.
 * If the list is empty, return NULL
 */
Agnode_t* 
firstDeglist (deglist_t* list)
{
	degitem*  ip;
	Agnode_t* np;

	ip = (degitem*)dtfirst (list);
	if (ip) {
		np = ip->np;
		ip->np = ND_next(np);
		if (ip->np == NULL) 
			dtdelete (list, ip);
		return np;
	}
	else return 0;
}

#ifdef DEBUG
void 
printDeglist (deglist_t* dl)
{
  degitem* ip;
  node_t*  np;
  fprintf (stderr, " dl:\n");
  for (ip = (degitem*)dtfirst(dl); ip; ip = (degitem*)dtnext(dl,ip)) {
    np = ip->np;
    if (np) fprintf (stderr, " (%d)", ip->deg);
    for (; np; np = ND_next(np)) {
      fprintf (stderr, " %s", np->name);
    }
    fprintf (stderr, "\n");
  }

}
#endif

