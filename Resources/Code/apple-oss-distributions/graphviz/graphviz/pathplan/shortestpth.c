/*
    This software may only be used by you under license from AT&T Corp.
    ("AT&T").  A copy of AT&T's Source Code Agreement is available at
    AT&T's Internet website having the URL:
    <http://www.research.att.com/sw/tools/graphviz/license/source.html>
    If you received this software without first entering into a license
    with AT&T, you have an infringing copy of this software and cannot use
    it without violating AT&T's intellectual property rights.
*/

#include <vis.h>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

COORD unseen = (double)INT_MAX;

/* shortestPath:
 * Given a VxV weighted adjacency matrix, compute the shortest
 * path vector from root to target. The returned vector (dad) encodes the
 * shorted path from target to the root. That path is given by
 * i, dad[i], dad[dad[i]], ..., root
 * We have dad[root] = -1.
 *
 * Based on Dijkstra's algorithm (Sedgewick, 2nd. ed., p. 466).
 *
 * This implementation only uses the lower left triangle of the
 * adjacency matrix, i.e., the values a[i][j] where i >= j.
 */
int* shortestPath (int root, int target, int V, array2 wadj)
{
    int*   dad;
    COORD* vl;
    COORD* val;
    int    min;
    int    k, t;
    
    /* allocate arrays */
    dad = (int*)malloc(V * sizeof(int));
    vl  = (COORD*)malloc((V+1) * sizeof(COORD));  /* One extra for sentinel */
    val = vl + 1;

    /* initialize arrays */
    for (k = 0;k < V; k++) {
      dad[k] = -1;
      val[k] = -unseen;
    }
    val[-1] = -(unseen + (COORD)1);    /* Set sentinel */
    min = root;

      /* use (min >= 0) to fill entire tree */
    while (min != target) {
      k = min;
      val[k] *= -1;
      min = -1;
      if (val[k] == unseen) val[k] = 0;

      for (t = 0; t < V; t++) {
        if (val[t] < 0) {
          COORD newpri;
          COORD wkt;

           /* Use lower triangle */
          if (k >= t) wkt = wadj[k][t];
          else wkt = wadj[t][k];

          newpri = -(val[k] + wkt);
          if ((wkt != 0) && (val[t] < newpri)) {
            val[t] = newpri;
            dad[t] = k;
          }
          if (val[t] > val[min]) min = t;
        }
      }
    }

    free (vl);
    return dad;
}

/* makePath:
 * Given two points p and q in two polygons pp and qp of a vconfig_t conf, 
 * and the visibility vectors of p and q relative to conf, 
 * compute the shortest path from p to q.
 * If dad is the returned array and V is the number of polygon vertices in
 * conf, then the path is V(==q), dad[V], dad[dad[V]], ..., V+1(==p).
 * NB: This is the only path that is guaranteed to be valid.
 * We have dad[V+1] = -1.
 * 
 */
int* makePath (Ppoint_t p, int pp, COORD* pvis,
               Ppoint_t q, int qp, COORD* qvis,
               vconfig_t* conf)
{
    int    V = conf->N;

    if (directVis(p, pp, q, qp, conf)) {
      int* dad = (int*)malloc(sizeof(int)*(V+2));
      dad[V] = V+1;
      dad[V+1] = -1;
      return dad;
    }
    else {
      array2 wadj = conf->vis;
      wadj[V] = qvis;
      wadj[V+1] = pvis;
      return (shortestPath (V+1,V,V+2,wadj));
    }
}
