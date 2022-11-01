/* Loop unswitching.
   Copyright (C) 2004 Free Software Foundation, Inc.
   
This file is part of GCC.
   
GCC is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.
   
GCC is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.
   
You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "rtl.h"
#include "tm_p.h"
#include "hard-reg-set.h"
#include "basic-block.h"
#include "output.h"
#include "diagnostic.h"
#include "tree-flow.h"
#include "tree-dump.h"
#include "timevar.h"
#include "cfgloop.h"
#include "domwalk.h"
#include "params.h"
#include "tree-pass.h"

/* This file implements the loop unswitching, i.e. transformation of loops like

   while (A)
     {
       if (inv)
         B;

       X;

       if (!inv)
	 C;
     }

   where inv is the loop invariant, into

   if (inv)
     {
       while (A)
	 {
           B;
	   X;
	 }
     }
   else
     {
       while (A)
	 {
	   X;
	   C;
	 }
     }

   Inv is considered invariant iff the values it compares are both invariant;
   tree-ssa-loop-im.c ensures that all the suitable conditions are in this
   shape.  */

static struct loop *tree_unswitch_loop (struct loops *, struct loop *, basic_block,
				   tree);
static bool tree_unswitch_single_loop (struct loops *, struct loop *, int);
static tree tree_may_unswitch_on (basic_block, struct loop *);

/* Main entry point.  Perform loop unswitching on all suitable LOOPS.  */

void
tree_ssa_unswitch_loops (struct loops *loops)
{
  int i, num;
  struct loop *loop;
  bool changed = false;

  /* Go through inner loops (only original ones).  */
  num = loops->num;

  for (i = 1; i < num; i++)
    {
      /* Removed loop?  */
      loop = loops->parray[i];
      if (!loop)
	continue;

      if (loop->inner)
	continue;

      changed |= tree_unswitch_single_loop (loops, loop, 0);
#ifdef ENABLE_CHECKING
      verify_dominators (CDI_DOMINATORS);
      verify_loop_structure (loops);
#endif
    }

#if 0
  /* The necessary infrastructure is not in yet.  */
  if (changed)
    cleanup_tree_cfg_loop ();
#endif
}

/* Checks whether we can unswitch LOOP on condition at end of BB -- one of its
   basic blocks (for what it means see comments below).  */

static tree
tree_may_unswitch_on (basic_block bb, struct loop *loop)
{
  tree stmt, def, cond;
  basic_block def_bb;
  use_optype uses;
  unsigned i;

  /* BB must end in a simple conditional jump.  */
  stmt = last_stmt (bb);
  if (!stmt || TREE_CODE (stmt) != COND_EXPR)
    return NULL_TREE;

  /* Condition must be invariant.  */
  get_stmt_operands (stmt);
  uses = STMT_USE_OPS (stmt);
  for (i = 0; i < NUM_USES (uses); i++)
    {
      def = SSA_NAME_DEF_STMT (USE_OP (uses, i));
      def_bb = bb_for_stmt (def);
      if (def_bb
	  && flow_bb_inside_loop_p (loop, def_bb))
	return NULL_TREE;
    }

  cond = COND_EXPR_COND (stmt);
  /* To keep the things simple, we do not directly remove the conditions,
     but just replace tests with 0/1.  Prevent the infinite loop where we
     would unswitch again on such a condition.  */
  if (integer_zerop (cond) || integer_nonzerop (cond))
    return NULL_TREE;

  return cond;
}

/* Simplifies COND using checks in front of the entry of the LOOP.  Just very
   simplish (sufficient to prevent us from duplicating loop in unswitching
   unnecessarily).  */

static tree
simplify_using_entry_checks (struct loop *loop, tree cond)
{
  edge e = loop_preheader_edge (loop);
  tree stmt;

  while (1)
    {
      stmt = last_stmt (e->src);
      if (stmt
	  && TREE_CODE (stmt) == COND_EXPR
	  && operand_equal_p (COND_EXPR_COND (stmt), cond, 0))
	return (e->flags & EDGE_TRUE_VALUE
		? boolean_true_node
		: boolean_false_node);

      if (EDGE_COUNT (e->src->preds) > 1)
	return cond;

      e = EDGE_PRED (e->src, 0);
      if (e->src == ENTRY_BLOCK_PTR)
	return cond;
    }
}

/* Unswitch single LOOP.  NUM is number of unswitchings done; we do not allow
   it to grow too much, it is too easy to create example on that the code would
   grow exponentially.  */

static bool
tree_unswitch_single_loop (struct loops *loops, struct loop *loop, int num)
{
  basic_block *bbs;
  struct loop *nloop;
  unsigned i;
  tree cond = NULL_TREE, stmt;
  bool changed = false;

  /* Do not unswitch too much.  */
  if (num > PARAM_VALUE (PARAM_MAX_UNSWITCH_LEVEL))
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file, ";; Not unswitching anymore, hit max level\n");
      return false;
    }

  /* Only unswitch innermost loops.  */
  if (loop->inner)
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file, ";; Not unswitching, not innermost loop\n");
      return false;
    }

  /* The loop should not be too large, to limit code growth.  */
  if (tree_num_loop_insns (loop)
      > (unsigned) PARAM_VALUE (PARAM_MAX_UNSWITCH_INSNS))
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file, ";; Not unswitching, loop too big\n");
      return false;
    }

  i = 0;
  bbs = get_loop_body (loop);
  
  while (1)
    {
      /* Find a bb to unswitch on.  */
      for (; i < loop->num_nodes; i++)
	if ((cond = tree_may_unswitch_on (bbs[i], loop)))
	  break;

      if (i == loop->num_nodes)
	{
	  free (bbs);
	  return changed;
	}

      cond = simplify_using_entry_checks (loop, cond);
      stmt = last_stmt (bbs[i]);
      if (integer_nonzerop (cond))
	{
	  /* Remove false path.  */
	  COND_EXPR_COND (stmt) = boolean_true_node;
	  changed = true;
	}
      else if (integer_zerop (cond))
	{
	  /* Remove true path.  */
	  COND_EXPR_COND (stmt) = boolean_false_node;
	  changed = true;
	}
      else
	break;

      modify_stmt (stmt);
      i++;
    }

  if (dump_file && (dump_flags & TDF_DETAILS))
    fprintf (dump_file, ";; Unswitching loop\n");

  /* Unswitch the loop on this condition.  */
  nloop = tree_unswitch_loop (loops, loop, bbs[i], cond);
  if (!nloop)
    return changed;

  /* Invoke itself on modified loops.  */
  tree_unswitch_single_loop (loops, nloop, num + 1);
  tree_unswitch_single_loop (loops, loop, num + 1);
  return true;
}

/* Unswitch a LOOP w.r. to given basic block UNSWITCH_ON.  We only support
   unswitching of innermost loops.  COND is the condition determining which
   loop is entered -- the new loop is entered if COND is true.  Returns NULL
   if impossible, new loop otherwise.  */

static struct loop *
tree_unswitch_loop (struct loops *loops, struct loop *loop,
		    basic_block unswitch_on, tree cond)
{
  basic_block condition_bb;

  /* Some sanity checking.  */
  gcc_assert (flow_bb_inside_loop_p (loop, unswitch_on));
  gcc_assert (EDGE_COUNT (unswitch_on->succs) == 2);
  gcc_assert (loop->inner == NULL);

  return tree_ssa_loop_version (loops, loop, unshare_expr (cond), 
				&condition_bb);
}
