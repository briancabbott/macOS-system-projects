/* Callgraph handling code.
   Copyright (C) 2003 Free Software Foundation, Inc.
   Contributed by Jan Hubicka

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#ifndef GCC_CGRAPH_H
#define GCC_CGRAPH_H

/* Information about the function collected locally.
   Available after function is lowered  */

struct cgraph_local_info GTY(())
{
  /* Set when function function is visiable in current compilation unit only
     and it's address is never taken.  */
  bool local;
  /* Set when function is small enought to be inlinable many times.  */
  bool inline_many;
  /* Set when function can be inlined once (false only for functions calling
     alloca, using varargs and so on).  */
  bool can_inline_once;
};

/* Information about the function that needs to be computed globally
   once compilation is finished.  Available only with -funit-at-time.  */

struct cgraph_global_info GTY(())
{
  /* Set when the function will be inlined exactly once.  */
  bool inline_once;
};

/* Information about the function that is propagated by the RTL backend.
   Available only for functions that has been already assembled.  */

struct cgraph_rtl_info GTY(())
{
  bool const_function;
  bool pure_function;
  int preferred_incoming_stack_boundary;
};


struct cgraph_node_inliner_info GTY(())
{
  /* Number of calls in the original function.  */
  unsigned callee_count;
  /* Callgraph for inliner; calls we plan to inline, and calls that
     will appear after inlining.  This is implemented as a pointer to
     minimize memory usage if callgraph-directed inlining is off; if
     callgraph-directed inlining became popular, the pointer could be
     omitted.  */
  struct cgraph_edge *top_edge;
  /* Desirability of this body, independent of any call.  */
  double body_desirability;
  /* Desirability of the 'most_desirable_edge'.  */
  double highest_desirability;
  /* The edge representing the most-desirable, not-yet-inlined call.  */
  struct cgraph_edge *most_desirable_edge;
  /* The fully-inlined tree.  Original tree will not be modified.  */
  tree fully_inlined_decl;
  /* The name of this function.  An aid to debugging.  */
  char *name;
  /* Number of times this function was invoked.  Computed by summing
     the execution counts of all the calls to this function.  */
  HOST_WIDEST_INT execution_count;
  /* Number of invocations that won't happen because this function
     body got inlined somewhere.  */
  HOST_WIDEST_INT removed_execution_count;
  /* The number of lines inlined into this function.  Large functions,
     whether born or grown to size, are discouraged from inlining
     other functions.  */
  unsigned long additional_lines;
};

struct cgraph_edge_inliner_info GTY(())
{
  /* Address of the pointer-to-the-CALL_NODE to be replaced with an inlined body.
     Only valid during inlining; once the call has been inlined, it's gone.  */
  /* union tree_node ** GTY ((param_is (union tree_node **))) call_expr;  */
  tree * GTY((skip(""))) call_expr;
  /* Number of times this *call* (not function) was invoked.  */
  unsigned invocations;
  /* Desirability: higher numbers are more likely to be inlined.  */
  double desirability;
  /* If non-null, this call will be inlined, and this is the list of
     edges representing calls that will appear when this call is
     inlined.  */
  struct cgraph_edge *callees;
  /* Short-lived VARRAY for random access to callees.
     Used only if callees pointer is set.  */
  varray_type GTY((skip(""))) callee_array;
  /* True if the CALL_EXPR corresponding to this edge should be inlined.  */
  bool inline_this;
  /* Back link; previous (sibling) call in this function.  */
  struct cgraph_edge *prev;
  /* Up link; edge above us (our caller).  */
  struct cgraph_edge *uplink;
  /* Number of times this CALL_EXPR was executed.  From feedback.c.  */
  HOST_WIDEST_INT execution_count;
};

/* The cgraph data strutcture.
   Each function decl has assigned cgraph_node listing calees and callers.  */

struct cgraph_node GTY(())
{
  tree decl;
  struct cgraph_edge *callees;
  struct cgraph_edge *callers;
  struct cgraph_node *next;
  struct cgraph_node *previous;
  /* For nested functions points to function the node is nested in.  */
  struct cgraph_node *origin;
  /* Points to first nested function, if any.  */
  struct cgraph_node *nested;
  /* Pointer to the next function with same origin, if any.  */
  struct cgraph_node *next_nested;
  void * GTY((skip(""))) aux;

  /* Set when function must be output - it is externally visible
     or it's address is taken.  */
  bool needed;
  /* Set when function is reachable by call from other function
     that is eighter reachable or needed.  */
  bool reachable;
  /* Set when the frontend has been asked to lower representation of this
     function into trees.  Callees lists are not available when lowered
     is not set.  */
  bool lowered;
  /* Set when function is scheduled to be assembled.  */
  bool output;
  struct cgraph_local_info local;
  struct cgraph_global_info global;
  struct cgraph_rtl_info rtl;
  struct cgraph_node_inliner_info inliner;
};

/* Allocate one of these for every call in the unit.  */
struct cgraph_edge GTY(())
{
  struct cgraph_node *caller;
  struct cgraph_node *callee;
  struct cgraph_edge *next_caller;
  struct cgraph_edge *next_callee;
  struct cgraph_edge_inliner_info inliner;
};

extern GTY(()) struct cgraph_node *cgraph_nodes;
extern int cgraph_n_nodes;
extern bool cgraph_global_info_ready;

/* In cgraph.c  */
void dump_cgraph			PARAMS ((FILE *));
void cgraph_remove_call			PARAMS ((tree, tree));
void cgraph_remove_node			PARAMS ((struct cgraph_node *));
struct cgraph_edge *cgraph_record_call	PARAMS ((tree, tree, tree *, HOST_WIDE_INT));
struct cgraph_node *cgraph_node		PARAMS ((tree));
struct cgraph_edge *create_edge		PARAMS ((struct cgraph_node *, struct cgraph_node *, tree *));
bool cgraph_calls_p			PARAMS ((tree, tree));
struct cgraph_local_info *cgraph_local_info PARAMS ((tree));
struct cgraph_global_info *cgraph_global_info PARAMS ((tree));
struct cgraph_rtl_info *cgraph_rtl_info PARAMS ((tree));
void cgraph_record_inlining_choice	PARAMS ((struct cgraph_edge *));

/* In cgraphunit.c  */
void cgraph_finalize_function		PARAMS ((tree, tree));
void cgraph_finalize_compilation_unit	PARAMS ((void));
void cgraph_create_edges		PARAMS ((tree, tree));
varray_type cgraph_find_calls		PARAMS ((tree *));
double cgraph_call_desirability		PARAMS ((struct cgraph_edge *edge));
void cgraph_display_function_callgraph	PARAMS ((struct cgraph_edge *, const char *));
void cgraph_optimize			PARAMS ((void));
void cgraph_mark_needed_node		PARAMS ((struct cgraph_node *, int));

#endif  /* GCC_CGRAPH_H  */
