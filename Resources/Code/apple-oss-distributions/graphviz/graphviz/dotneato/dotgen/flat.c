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

#include	"dot.h"


node_t	*
make_vn_slot(graph_t *g, int r, int pos)
{
	int		i;
	node_t	**v,*n;

	v = GD_rank(g)[r].v = ALLOC(GD_rank(g)[r].n+2,GD_rank(g)[r].v,node_t*);
	for (i = GD_rank(g)[r].n; i > pos; i--) {
		v[i] = v[i-1];
		v[i]->u.order++;
	}
	n = v[pos] = virtual_node(g);
	ND_order(n) = pos;	ND_rank(n) = r;
	v[++(GD_rank(g)[r].n)] = NULL;
	return v[pos];
}

#define 	HLB 	0		/* hard left bound */
#define		HRB		1		/* hard right bound */
#define		SLB		2		/* soft left bound */
#define		SRB		3		/* soft right bound */

void findlr(node_t *u, node_t *v, int *lp, int *rp)
{
	int		l,r;
	l = ND_order(u); r = ND_order(v);
	if (l > r) {int t = l; l = r; r = t;}
	*lp = l; *rp = r;
}

void setbounds(node_t *v, int *bounds,int lpos, int rpos)
{
	int		i, l,r,ord;
	edge_t	*f;

	if (ND_node_type(v) == VIRTUAL) {
		ord = ND_order(v);
		if (ND_in(v).size == 0) {	/* flat */
			assert(ND_out(v).size == 2);
			findlr(ND_out(v).list[0]->head,ND_out(v).list[1]->head,&l,&r);
				/* the other flat edge could be to the left or right */
			if (r <= lpos) bounds[SLB] = bounds[HLB] = ord;
			else if (l >= rpos) bounds[SRB] = bounds[HRB] = ord;
				/* could be spanning this one */
			else if ((l < lpos) && (r > rpos)) ; /* ignore */
				/* must have intersecting ranges */
			else {
				if ((l < lpos) || ((l == lpos) && (r < rpos)))
					bounds[SLB] = ord;
				if ((r > rpos) || ((r == rpos) && (l > lpos)))
					bounds[SRB] = ord;
			}
		}
		else {						/* forward */
			boolean		onleft,onright;
			onleft = onright = FALSE;
			for (i = 0; (f = ND_out(v).list[i]); i++) {
				if (ND_order(f->head) <= lpos) {onleft = TRUE; continue;}
				if (ND_order(f->head) >= rpos) {onright = TRUE; continue;}
			}
			if (onleft && (onright == FALSE)) bounds[HLB] = ord + 1;
			if (onright && (onleft == FALSE)) bounds[HRB] = ord - 1;
		}
	}
}

int flat_limits(graph_t* g, edge_t* e)
{
	int			lnode,rnode,r,bounds[4],lpos,rpos,pos;
	node_t		**rank;

	r = ND_rank(e->tail) - 1;
	rank = GD_rank(g)[r].v;
	lnode = 0;
	rnode = GD_rank(g)[r].n - 1;
	bounds[HLB] = bounds[SLB] = lnode - 1;
	bounds[HRB] = bounds[SRB] = rnode + 1;
	findlr(e->tail,e->head,&lpos,&rpos);
	while (lnode <= rnode) {
		setbounds(rank[lnode],bounds,lpos,rpos);
		if (lnode != rnode)
			setbounds(rank[rnode],bounds,lpos,rpos);
		lnode++; rnode--;
		if (bounds[HRB] - bounds[HLB] <= 1) break;
	}
	if (bounds[HLB] <= bounds[HRB])
		pos = (bounds[HLB] + bounds[HRB] + 1) / 2;
	else
		pos = (bounds[SLB] + bounds[SRB] + 1) / 2;
	return pos;
}

void flat_node(edge_t* e)
{
	int		r,place,ypos,h2;
	graph_t	*g;
	node_t	*n,*vn;
	edge_t	*ve;
	pointf	dimen;

	if (ED_label(e) == NULL) return;
	g = e->tail->graph;
	r = ND_rank(e->tail);

	place = flat_limits(g,e);
		/* grab ypos = LL.y of label box before make_vn_slot() */
	if ((n = GD_rank(g)[r - 1].v[0]))
		ypos = ND_coord_i(n).y - GD_rank(g)[r - 1].ht2;
	else {
		n = GD_rank(g)[r].v[0];
		ypos = ND_coord_i(n).y + GD_rank(g)[r].ht1 + GD_ranksep(g);
	}
	vn = make_vn_slot(g,r-1,place);	
	dimen = ED_label(e)->dimen;
	if (GD_left_to_right(g)) {double f = dimen.x; dimen.x = dimen.y; dimen.y = f;}
	ND_ht_i(vn) = POINTS(dimen.y); h2 = ND_ht_i(vn) / 2;
	ND_lw_i(vn) = ND_rw_i(vn) = POINTS(dimen.x)/2;
	ND_label(vn) = ED_label(e);
	ND_coord_i(vn).y = ypos + h2;
	ve = virtual_edge(vn,e->tail,e);	/* was NULL? */
		ED_tail_port(ve).p.x = -ND_lw_i(vn);
		ED_head_port(ve).p.x = ND_rw_i(e->tail);
		ED_edge_type(ve) = FLATORDER;
	ve = virtual_edge(vn,e->head,e);
		ED_tail_port(ve).p.x = ND_rw_i(vn);
		ED_head_port(ve).p.x = ND_lw_i(e->head);
		ED_edge_type(ve) = FLATORDER;
	/* another assumed symmetry of ht1/ht2 of a label node */
	if (GD_rank(g)[r-1].ht1 < h2) GD_rank(g)[r-1].ht1 = h2;
	if (GD_rank(g)[r-1].ht2 < h2) GD_rank(g)[r-1].ht2 = h2;
}

int flat_edges(graph_t* g)
{
	int		i,j,reset = FALSE;
	node_t	*n;
	edge_t	*e;

	if ((GD_rank(g)[0].flat) || (GD_n_cluster(g) > 0)) {
		for (i = 0; (n = GD_rank(g)[0].v[i]); i++) {
			for (j = 0; (e = ND_flat_in(n).list[j]); j++) {
				if (ED_label(e)) {abomination(g); break;}
			}
			if (e) break;
		}
	}
			
	rec_save_vlists(g);
	for (n = GD_nlist(g); n; n = ND_next(n)) {
		if (ND_flat_out(n).list) for (i = 0; (e = ND_flat_out(n).list[i]); i++) {
			reset = TRUE;
			flat_node(e);
		}
	}
	if (reset) rec_reset_vlists(g);
	return reset;
}

void abomination(graph_t* g)
{
	int		r;
	rank_t	*rptr;

	assert(GD_minrank(g) == 0);
		/* 3 = one for new rank, one for sentinel, one for off-by-one */
	r = GD_maxrank(g) + 3;
	rptr = ALLOC(r,GD_rank(g),rank_t);
	GD_rank(g) = rptr + 1;
	for (r = GD_maxrank(g); r >= 0; r--)
		GD_rank(g)[r] = GD_rank(g)[r-1];
	GD_rank(g)[r].n = GD_rank(g)[0].an = 0;
	GD_rank(g)[r].v = GD_rank(g)[0].av = N_NEW(2,node_t*);
	GD_rank(g)[r].flat = NULL;
	GD_rank(g)[r].ht1 = GD_rank(g)[r].ht2 = 1;
	GD_rank(g)[r].pht1 = GD_rank(g)[r].pht2 = 1;
	GD_minrank(g)--;
}
