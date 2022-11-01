#ifndef ACTIONS_H
#define ACTIONS_H

#include <agraph.h>
#include <expr.h>

extern void nodeInduce(Agraph_t *selected);
extern Agobj_t* clone(Agraph_t *g, Agobj_t* obj);
extern Agobj_t* copy(Agraph_t *g, Agobj_t* obj);
extern int indexOf (char* s1, char* s2);
extern int match (char* str, char* pat);
extern int lockGraph (Agraph_t* g, int);
extern Agraph_t* compOf (Agraph_t* g, Agnode_t* n);
extern Agedge_t* isEdge (Agnode_t* t, Agnode_t* h, char* key);
extern int isIn (Agraph_t* gp, Agobj_t*);
extern Agnode_t* addNode (Agraph_t* g, Agnode_t* n);
extern Agedge_t* addEdge (Agraph_t* g, Agedge_t* e);
extern Agraph_t* sameG (void* p1, void* p2, char* fn, char* msg);
extern int compare (Agobj_t*, Agobj_t*);
extern int writeFile (Agraph_t*, char*);
extern int fwriteFile (Expr_t*, Agraph_t*, int);
extern Agraph_t* readFile (char*);
extern Agraph_t* freadFile (Expr_t*, int);
extern int openFile (Expr_t*, char*, char*);
extern int closeFile (Expr_t*, int);
extern char* readLine (Expr_t*, int);
extern char* canon (Expr_t* pgm, char*);
extern int deleteObj (Agraph_t* g, Agobj_t* obj);

#endif
