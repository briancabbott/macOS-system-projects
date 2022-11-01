%{
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

#include	"libgraph.h"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

static char		Port[SMALLBUF],*Symbol;
static char		In_decl,In_edge_stmt;
static int		Current_class,Agraph_type;
static Agraph_t		*G;
static Agnode_t		*N;
static Agedge_t		*E;
static objstack_t	*SP;
static Agraph_t		*Gstack[32];
static int			GSP;
static int			override;

/* agoverride:
 * If override == 1, initial attr_stmt is ignored if the attribute is
 *                   already defined.
 */
void agoverride(int ov)
{
  override = (ov > 0 ? ov : 0);
}

static void push_subg(Agraph_t *g)
{
	G = Gstack[GSP++] = g;
}

static Agraph_t *pop_subg(void)
{
	Agraph_t		*g;
	if (GSP == 0) {
		agerr (AGERR, "Gstack underflow in graph parser\n"); exit(1);
	}
	g = Gstack[--GSP];					/* graph being popped off */
	if (GSP > 0) G = Gstack[GSP - 1];	/* current graph */
	else G = 0;
	return g;
}

static objport_t pop_gobj(void)
{
	objport_t	rv;
	rv.obj = pop_subg();
	rv.port = NULL;
	return rv;
}

static void anonname(char* buf)
{
	static int		anon_id = 0;

	sprintf(buf,"_anonymous_%d",anon_id++);
}

static void begin_graph(char *name)
{
	Agraph_t		*g;
	char			buf[SMALLBUF];

	if (!name) {
		anonname(buf);
		name = buf;
    }
	g = AG.parsed_g = agopen(name,Agraph_type);
	Current_class = TAG_GRAPH;
	push_subg(g);
	In_decl = TRUE;
}

static void end_graph(void)
{
	pop_subg();
}

static Agnode_t *bind_node(char *name)
{
	Agnode_t	*n = agnode(G,name);
	In_decl = FALSE;
	return n;
}

static void anonsubg(void)
{
	char			buf[SMALLBUF];
	Agraph_t			*subg;

	In_decl = FALSE;
	anonname(buf);
	subg = agsubg(G,buf);
	push_subg(subg);
}

#if 0 /* NOT USED */
static int isanonsubg(Agraph_t *g)
{
	return (strncmp("_anonymous_",g->name,11) == 0);
}
#endif

static void begin_edgestmt(objport_t objp)
{
	struct objstack_t	*new_sp;

	new_sp = NEW(objstack_t);
	new_sp->link = SP;
	SP = new_sp;
	SP->list = SP->last = NEW(objlist_t);
	SP->list->data  = objp;
	SP->list->link = NULL;
	SP->in_edge_stmt = In_edge_stmt;
	SP->subg = G;
	agpushproto(G);
	In_edge_stmt = TRUE;
}

static void mid_edgestmt(objport_t objp)
{
	SP->last->link = NEW(objlist_t);
	SP->last = SP->last->link;
	SP->last->data = objp;
	SP->last->link = NULL;
}

static void end_edgestmt(void)
{
	objstack_t	*old_SP;
	objlist_t	*tailptr,*headptr,*freeptr;
	Agraph_t		*t_graph,*h_graph;
	Agnode_t	*t_node,*h_node,*t_first,*h_first;
	Agedge_t	*e;
	char		*tport,*hport;

	for (tailptr = SP->list; tailptr->link; tailptr = tailptr->link) {
		headptr = tailptr->link;
		tport = tailptr->data.port;
		hport = headptr->data.port;
		if (TAG_OF(tailptr->data.obj) == TAG_NODE) {
			t_graph = NULL;
			t_first = (Agnode_t*)(tailptr->data.obj);
		}
		else {
			t_graph = (Agraph_t*)(tailptr->data.obj);
			t_first = agfstnode(t_graph);
		}
		if (TAG_OF(headptr->data.obj) == TAG_NODE) {
			h_graph = NULL;
			h_first = (Agnode_t*)(headptr->data.obj);
		}
		else {
			h_graph = (Agraph_t*)(headptr->data.obj);
			h_first = agfstnode(h_graph);
		}

		for (t_node = t_first; t_node; t_node = t_graph ?
		  agnxtnode(t_graph,t_node) : NULL) {
			for (h_node = h_first; h_node; h_node = h_graph ?
			  agnxtnode(h_graph,h_node) : NULL ) {
				e = agedge(G,t_node,h_node);
				if (e) {
					char	*tp = tport;
					char 	*hp = hport;
					if ((e->tail != e->head) && (e->head == t_node)) {
						/* could happen with an undirected edge */
						char 	*temp;
						temp = tp; tp = hp; hp = temp;
					}
					if (tp && tp[0]) agxset(e,TAILX,tp);
					if (hp && hp[0]) agxset(e,HEADX,hp);
				}
			}
		}
	}
	tailptr = SP->list; 
	while (tailptr) {
		freeptr = tailptr;
		tailptr = tailptr->link;
		if (TAG_OF(freeptr->data.obj) == TAG_NODE)
		free(freeptr->data.port);
		free(freeptr);
	}
	if (G != SP->subg) abort();
	agpopproto(G);
	In_edge_stmt = SP->in_edge_stmt;
	old_SP = SP;
	SP = SP->link;
	In_decl = FALSE;
	free(old_SP);
	Current_class = TAG_GRAPH;
}

#if 0 /* NOT USED */
static Agraph_t *parent_of(Agraph_t *g)
{
	Agraph_t		*rv;
	rv = agusergraph(agfstin(g->meta_node->graph,g->meta_node)->tail);
	return rv;
}
#endif

static void attr_set(char *name, char *value)
{
	Agsym_t		*ap = NULL;
	char		*defval = "";

	if (In_decl && (G->root == G)) defval = value;
	switch (Current_class) {
		case TAG_NODE:
			ap = agfindattr(G->proto->n,name);
			if (ap == NULL)
				ap = agnodeattr(AG.parsed_g,name,defval);
            else if (override && In_decl)
              return;
			agxset(N,ap->index,value);
			break;
		case TAG_EDGE:
			ap = agfindattr(G->proto->e,name);
			if (ap == NULL)
				ap = agedgeattr(AG.parsed_g,name,defval);
            else if (override && In_decl)
              return;
			agxset(E,ap->index,value);
			break;
		case 0:		/* default */
		case TAG_GRAPH:
			ap = agfindattr(G,name);
			if (ap == NULL) 
				ap = agraphattr(AG.parsed_g,name,defval);
            else if (override && In_decl)
              return;
			agxset(G,ap->index,value);
			break;
	}
}

/* concat:
 */
static char*
concat (char* s1, char* s2)
{
  char*  s;
  char   buf[BUFSIZ];
  char*  sym;
  int    len = strlen(s1) + strlen(s2) + 1;

  if (len <= BUFSIZ) sym = buf;
  else sym = (char*)malloc(len);
  strcpy(sym,s1);
  strcat(sym,s2);
  s = agstrdup (sym);
  if (sym != buf) free (sym);
  return s;
}

%}

%union	{
			int					i;
			char				*str;
			struct objport_t	obj;
			struct Agnode_t		*n;
}

%token		<i>	T_graph T_digraph T_strict
%token		<i>	T_node T_edge T_edgeop
%token		<str>	T_symbol T_qsymbol
%type		<str>	symbol qsymbol optgraphname
%type		<n>		node_name
%type		<obj>	node_id subg_stmt
%left 		<i> T_subgraph	/* to avoid subgraph hdr shift/reduce conflict */
%left '{'
%%

file		:	graph_type optgraphname
				{begin_graph($2); agstrfree($2);}
			'{' stmt_list '}'
				{AG.accepting_state = TRUE; end_graph();}
		|	error
				{
					if (AG.parsed_g)
						agclose(AG.parsed_g);
					AG.parsed_g = NULL;
					/*exit(1);*/
				}
		|	/* empty*/  {AG.parsed_g = NULL;}
		;

optgraphname:	symbol {$$=$1;} | /* empty */ {$$=0;} ;

	/* it is safe to change graph type and name before contents appear */
graph_type	:	T_graph
				{Agraph_type = AGRAPH; AG.edge_op = "--";}
		|	T_strict T_graph
				{Agraph_type = AGRAPHSTRICT; AG.edge_op = "--";}
		|	T_digraph
				{Agraph_type = AGDIGRAPH; AG.edge_op = "->";}
		|	T_strict T_digraph
				{Agraph_type = AGDIGRAPHSTRICT; AG.edge_op = "->";}
		;

attr_class	:	T_graph 
				{Current_class = TAG_GRAPH;}
		|	T_node
				{Current_class = TAG_NODE; N = G->proto->n;}
		|	T_edge
				{Current_class = TAG_EDGE; E = G->proto->e;}
		;

inside_attr_list :	iattr_set optcomma inside_attr_list
		|			/* empty */
		;

optcomma	:	/* empty */ 
		|	','
		;

attr_list	:	'[' inside_attr_list ']'
		;

rec_attr_list	:	rec_attr_list attr_list
		|	/* empty */
		;

opt_attr_list	:	rec_attr_list
		;

attr_set	:	symbol '=' symbol 
					{attr_set($1,$3); agstrfree($1); agstrfree($3);}
		;

iattr_set	:	attr_set
			|	symbol 
					{attr_set($1,"true"); agstrfree($1); }
		;

stmt_list	:	stmt_list1
		|	/* empty */
		;

stmt_list1	:	stmt
		|	stmt_list1 stmt
		;

stmt		:	stmt1
		|	stmt1 ';'
		|	error {agerror("syntax error, statement skipped");}
		;

stmt1		:	node_stmt
		|	edge_stmt
		|	attr_stmt 	
		|	subg_stmt {}
		;

attr_stmt	:	attr_class attr_list
				{Current_class = TAG_GRAPH; /* reset */}
		|	attr_set
				{Current_class = TAG_GRAPH;}
		;

node_id		:	node_name node_port
				{
					objport_t		rv;
					rv.obj = $1;
					rv.port = strdup(Port);
					Port[0] = '\0';
					$$ = rv;
				} 
		;

node_name	:	symbol {$$ = bind_node($1); agstrfree($1);}
		;

node_port	:	/* empty */
		|	port_location 
		|	port_angle 			/* undocumented */
		|	port_angle port_location 	/* undocumented */
		|	port_location port_angle 	/* undocumented */
		;

port_location	:	':' symbol {strcat(Port,":"); strcat(Port,$2);}
		|	':' '(' symbol {Symbol = strdup($3);} ',' symbol ')'
				{	char buf[SMALLBUF];
					sprintf(buf,":(%s,%s)",Symbol,$6);
					strcat(Port,buf); free(Symbol);
				}
		;

port_angle	:	'@' symbol
				{	char buf[SMALLBUF];
					sprintf(buf,"@%s",$2);
					strcat(Port,buf);
				}
		;

node_stmt	:	node_id
				{Current_class = TAG_NODE; N = (Agnode_t*)($1.obj);}
			opt_attr_list
				{Current_class = TAG_GRAPH; /* reset */}
		;

edge_stmt	:	node_id
				{begin_edgestmt($1);}
			edgeRHS
				{ E = SP->subg->proto->e;
				  Current_class = TAG_EDGE; }
			opt_attr_list
				{end_edgestmt();}
		|	subg_stmt
				{begin_edgestmt($1);}
			edgeRHS
				{ E = SP->subg->proto->e;
				  Current_class = TAG_EDGE; }
			opt_attr_list
				{end_edgestmt();}
		;

edgeRHS		:	T_edgeop node_id {mid_edgestmt($2);}
		|	T_edgeop node_id
				{mid_edgestmt($2);}
			edgeRHS
		|	T_edgeop subg_stmt
				{mid_edgestmt($2);}
		|	T_edgeop subg_stmt
				{mid_edgestmt($2);}
			edgeRHS
		;


subg_stmt	:	subg_hdr '{' stmt_list '}'%prec '{' {$$ = pop_gobj();}
		|	T_subgraph '{' { anonsubg(); } stmt_list '}' {$$ = pop_gobj();}
		|	'{' { anonsubg(); } stmt_list '}' {$$ = pop_gobj();}
		|	subg_hdr %prec T_subgraph {$$ = pop_gobj();}
		;

subg_hdr	:	T_subgraph symbol
				{ Agraph_t	 *subg;
				if ((subg = agfindsubg(AG.parsed_g,$2))) aginsert(G,subg);
				else subg = agsubg(G,$2); 
				push_subg(subg);
				In_decl = FALSE;
				agstrfree($2);
				}
		;

symbol		:	T_symbol {$$ = $1; }
		|	qsymbol {$$ = $1; }
		;

qsymbol		:	T_qsymbol {$$ = $1; }
	|	qsymbol '+' T_qsymbol {$$ = concat($1,$3); agstrfree($1); agstrfree($3);}
		;
