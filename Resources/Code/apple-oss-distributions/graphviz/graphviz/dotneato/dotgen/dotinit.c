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

#include    "dot.h"


void dot_nodesize(node_t* n, boolean flip)
{
    double       x,y;
    int         ps;

    if (flip == FALSE) {
               x = ND_width(n);
               y = ND_height(n);
       }
    else {
               y = ND_width(n);
               x = ND_height(n);
       }
    ps = POINTS(x)/2;
    if (ps < 1)
               ps = 1;
    ND_lw_i(n) = ND_rw_i(n) = ps;
    ND_ht_i(n) = POINTS(y);
}

void dot_init_node(node_t* n)
{
	common_init_node(n);
	dot_nodesize(n,GD_left_to_right(n->graph));
	alloc_elist(4,ND_in(n));	alloc_elist(4,ND_out(n));
	alloc_elist(2,ND_flat_in(n)); alloc_elist(2,ND_flat_out(n));
	alloc_elist(2,ND_other(n));
	ND_UF_size(n) = 1;
}

void dot_init_edge(edge_t* e)
{
	char	*tailgroup,*headgroup,*p;
	GVC_t *gvc = GD_gvc(e->tail->graph->root);

	common_init_edge(e);

	ED_weight(e) = late_double(e,E_weight,1.0,0.0);
	tailgroup = late_string(e->tail,N_group,"");
	headgroup = late_string(e->head,N_group,"");
	ED_count(e) = ED_xpenalty(e) = 1;
	if (tailgroup[0] && (tailgroup == headgroup)) {
		ED_xpenalty(e) = CL_CROSS;  ED_weight(e) *= 100;
	}
	if (nonconstraint_edge(e)) {
		ED_xpenalty(e) = 0;
		ED_weight(e) = 0;
	}

	ED_showboxes(e) = late_int(e,E_showboxes,0,0);
	ED_minlen(e) = late_int(e,E_minlen,1,0);
	p = agget(e,"tailport");
	if (p[0]) ND_has_port(e->tail) = TRUE;
	gvc->n = e->tail;
	ED_tail_port(e) = ND_shape(e->tail)->fns->portfn(gvc,p);
	p = agget(e,"headport");
	if (p[0]) ND_has_port(e->head) = TRUE;
	gvc->n = e->head;
	ED_head_port(e) = ND_shape(e->head)->fns->portfn(gvc,p);
}

void dot_init_node_edge(graph_t *g)
{
	node_t *n;
	edge_t *e;

	for (n = agfstnode(g); n; n = agnxtnode(g,n)) dot_init_node(n);
	for (n = agfstnode(g); n; n = agnxtnode(g,n)) {
	    for (e = agfstout(g,n); e; e = agnxtout(g,e)) dot_init_edge(e);
	}
}

#if 0 /* not used */
static void free_edge_list(elist L)
{
	edge_t	*e;
	int	i;

	for (i=0; i<L.size; i++) {
		e=L.list[i];
		free(e);
	}
}
#endif

void dot_cleanup_node(node_t* n)
{
	GVC_t *gvc = GD_gvc(n->graph->root);
	
	gvc->n = n;

	free_list(ND_in(n));
	free_list(ND_out(n));
	free_list(ND_flat_out(n));
	free_list(ND_flat_in(n));
	free_list(ND_other(n));
	free_label(ND_label(n));
	if (ND_shape(n))
		ND_shape(n)->fns->freefn(gvc);
	memset(&(n->u),0,sizeof(Agnodeinfo_t));
}

void dot_free_splines(edge_t* e)
{
	int		i;
	if (ED_spl(e)) {
		for (i = 0; i < ED_spl(e)->size; i++) free(ED_spl(e)->list[i].list);
		free(ED_spl(e)->list);
		free(ED_spl(e));
	}
	ED_spl(e) = NULL;
}

void dot_cleanup_edge(edge_t* e)
{
	dot_free_splines(e);
	free_label(ED_label(e));
	memset(&(e->u),0,sizeof(Agedgeinfo_t));
}

static void free_virtual_edge_list(node_t *n)
{
	edge_t *e;
	int	i;

	for (i=ND_in(n).size - 1; i >= 0; i--) {
		e = ND_in(n).list[i];
		delete_fast_edge(e);
		free(e);
	}
	for (i=ND_out(n).size - 1; i >= 0; i--) {
		e = ND_out(n).list[i];
		delete_fast_edge(e);
		free(e);
	}
}

static void free_virtual_node_list(node_t *vn)
{
	node_t	*next_vn;

	while (vn) {
		next_vn = ND_next(vn);
		free_virtual_edge_list(vn);
		if (ND_node_type(vn) == VIRTUAL) {
			free_list(ND_out(vn));
			free_list(ND_in(vn));
			free(vn);
		}
		vn = next_vn;
	}
}

void dot_cleanup_graph(graph_t* g)
{
	int		i, c;
	graph_t	*clust;

	for (c = 1; c <= GD_n_cluster(g); c++) {
		clust= GD_clust(g)[c];
		dot_cleanup(clust);
	}

	free_list(GD_comp(g));
	if ((g == g->root) && GD_rank(g)) {
		for (i = GD_minrank(g); i <= GD_maxrank(g); i++)
			free(GD_rank(g)[i].v);
		free(GD_rank(g));
	}
	free_ugraph(g);
	free_label(GD_label(g));
	memset(&(g->u),0,sizeof(Agraphinfo_t));
}

/* delete the layout (but not the underlying graph) */
void dot_cleanup(graph_t* g)
{
	node_t  *n;
	edge_t  *e;

	free_virtual_node_list(GD_nlist(g));
	for (n = agfstnode(g); n; n = agnxtnode(g, n)) {
		for (e = agfstout(g, n); e; e = agnxtout(g, e)) {
			dot_cleanup_edge(e);
		}
		dot_cleanup_node(n);
	}
	dot_cleanup_graph(g);
}

void
dot_init_graph (Agraph_t *g)
{
	UseRankdir = TRUE;
    graph_init(g);
    GD_drawing(g)->engine = DOT;
    dot_init_node_edge(g);
}

void dot_layout(Agraph_t *g)
{
    dot_init_graph(g);
    dot_rank(g);
    dot_mincross(g);
    dot_position(g);
    dot_sameports(g);
    dot_splines(g);
    if (mapbool(agget(g,"compound"))) dot_compoundEdges (g);
    dotneato_postprocess(g, dot_nodesize);
}   
