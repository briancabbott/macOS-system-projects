/* If-conversion for vectorizer.
   Copyright (C) 2004, 2005 Free Software Foundation, Inc.
   Contributed by Devang Patel <dpatel@apple.com>

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

/* This pass implements tree level if-conversion transformation of loops.
   Initial goal is to help vectorizer vectorize loops with conditions.

   A short description of if-conversion:

     o Decide if a loop is if-convertible or not.
     o Walk all loop basic blocks in breadth first order (BFS order).
       o Remove conditional statements (at the end of basic block)
         and propagate condition into destination basic blocks'
	 predicate list.
       o Replace modify expression with conditional modify expression
         using current basic block's condition.
     o Merge all basic blocks
       o Replace phi nodes with conditional modify expr
       o Merge all basic blocks into header

     Sample transformation:

     INPUT
     -----

     # i_23 = PHI <0(0), i_18(10)>;
     <L0>:;
     j_15 = A[i_23];
     if (j_15 > 41) goto <L1>; else goto <L17>;

     <L17>:;
     goto <bb 3> (<L3>);

     <L1>:;

     # iftmp.2_4 = PHI <0(8), 42(2)>;
     <L3>:;
     A[i_23] = iftmp.2_4;
     i_18 = i_23 + 1;
     if (i_18 <= 15) goto <L19>; else goto <L18>;

     <L19>:;
     goto <bb 1> (<L0>);

     <L18>:;

     OUTPUT
     ------

     # i_23 = PHI <0(0), i_18(10)>;
     <L0>:;
     j_15 = A[i_23];

     <L3>:;
     iftmp.2_4 = j_15 > 41 ? 42 : 0;
     A[i_23] = iftmp.2_4;
     i_18 = i_23 + 1;
     if (i_18 <= 15) goto <L19>; else goto <L18>;

     <L19>:;
     goto <bb 1> (<L0>);

     <L18>:;
*/

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "errors.h"
#include "tree.h"
#include "c-common.h"
#include "flags.h"
#include "timevar.h"
#include "varray.h"
#include "rtl.h"
#include "basic-block.h"
#include "diagnostic.h"
#include "tree-flow.h"
#include "tree-dump.h"
#include "cfgloop.h"
#include "tree-chrec.h"
#include "tree-data-ref.h"
#include "tree-scalar-evolution.h"
#include "tree-pass.h"
#include "target.h"

/* local function prototypes */
static void main_tree_if_conversion (void);
static tree tree_if_convert_stmt (struct loop *loop, tree, tree,
				  block_stmt_iterator *);
static void tree_if_convert_cond_expr (struct loop *, tree, tree,
				       block_stmt_iterator *);
static bool if_convertible_phi_p (struct loop *, basic_block, tree);
static bool if_convertible_modify_expr_p (struct loop *, basic_block, tree);
static bool if_convertible_stmt_p (struct loop *, basic_block, tree);
static bool if_convertible_bb_p (struct loop *, basic_block, bool);
static bool if_convertible_loop_p (struct loop *, bool);
static void add_to_predicate_list (basic_block, tree);
static tree add_to_dst_predicate_list (struct loop * loop, basic_block, tree, tree,
				       block_stmt_iterator *);
static void clean_predicate_lists (struct loop *loop);
static basic_block find_phi_replacement_condition (basic_block, tree *,
						   block_stmt_iterator *);
static void replace_phi_with_cond_modify_expr (tree, tree, basic_block,
                                               block_stmt_iterator *);
static void process_phi_nodes (struct loop *);
static void combine_blocks (struct loop *);
static tree ifc_temp_var (tree, tree);
static bool pred_blocks_visited_p (basic_block, bitmap *);
static basic_block * get_loop_body_in_if_conv_order (const struct loop *loop);
static bool bb_with_exit_edge_p (basic_block);

/* List of basic blocks in if-conversion-suitable order.  */
static basic_block *ifc_bbs;

/* Main entry point.
   Apply if-conversion to the LOOP. Return true if successful otherwise return
   false. If false is returned then loop remains unchanged.
   FOR_VECTORIZER is a boolean flag. It indicates whether if-conversion is used
   for vectorizer or not. If it is used for vectorizer, additional checks are
   used. (Vectorization checks are not yet implemented).  */

static bool
tree_if_conversion (struct loop *loop, bool for_vectorizer)
{
  basic_block bb;
  block_stmt_iterator itr;
  tree cond;
  unsigned int i;

  ifc_bbs = NULL;

  /* if-conversion is not appropriate for all loops. First, check if loop  is
     if-convertible or not.  */
  if (!if_convertible_loop_p (loop, for_vectorizer))
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file,"-------------------------\n");
      if (ifc_bbs)
	{
	  free (ifc_bbs);
	  ifc_bbs = NULL;
	}
      free_dominance_info (CDI_POST_DOMINATORS);
      free_df ();
      return false;
    }

  cond = NULL_TREE;

  /* Do actual work now.  */
  for (i = 0; i < loop->num_nodes; i++)
    {
      bb = ifc_bbs [i];

      /* Update condition using predicate list.  */
      cond = bb->aux;

      /* Process all statements in this basic block.
	 Remove conditional expression, if any, and annotate
	 destination basic block(s) appropriately.  */
      for (itr = bsi_start (bb); !bsi_end_p (itr); /* empty */)
	{
	  tree t = bsi_stmt (itr);
	  cond = tree_if_convert_stmt (loop, t, cond, &itr);
	  if (!bsi_end_p (itr))
	    bsi_next (&itr);
	}

      /* If current bb has only one successor, then consider it as an
	 unconditional goto.  */
      if (EDGE_COUNT (bb->succs) == 1)
	{
	  basic_block bb_n = EDGE_SUCC (bb, 0)->dest;
	  if (cond != NULL_TREE)
	    add_to_predicate_list (bb_n, cond);
	  cond = NULL_TREE;
	}
    }

  /* Now, all statements are if-converted and basic blocks are
     annotated appropriately. Combine all basic block into one huge
     basic block.  */
  combine_blocks (loop);

  /* clean up */
  clean_predicate_lists (loop);
  free (ifc_bbs);
  ifc_bbs = NULL;
  free_df ();

  return true;
}

/* if-convert stmt T which is part of LOOP.
   If T is a MODIFY_EXPR than it is converted into conditional modify
   expression using COND.  For conditional expressions, add condition in the
   destination basic block's predicate list and remove conditional
   expression itself. BSI is the iterator used to traverse statements of
   loop. It is used here when it is required to delete current statement.  */

static tree
tree_if_convert_stmt (struct loop *  loop, tree t, tree cond,
		      block_stmt_iterator *bsi)
{
  if (dump_file && (dump_flags & TDF_DETAILS))
    {
      fprintf (dump_file, "------if-convert stmt\n");
      print_generic_stmt (dump_file, t, TDF_SLIM);
      print_generic_stmt (dump_file, cond, TDF_SLIM);
    }

  switch (TREE_CODE (t))
    {
      /* Labels are harmless here.  */
    case LABEL_EXPR:
      break;

    case MODIFY_EXPR:
      /* This modify_expr is killing previous value of LHS. Appropriate value will
	 be selected by PHI node based on condition. It is possible that before
	 this transformation, PHI nodes was selecting default value and now it will
	 use this new value. This is OK because it does not change validity the
	 program.  */
      break;

    case GOTO_EXPR:
      /* Unconditional goto */
      add_to_predicate_list (bb_for_stmt (TREE_OPERAND (t, 1)), cond);
      bsi_remove (bsi);
      cond = NULL_TREE;
      break;

    case COND_EXPR:
      /* Update destination blocks' predicate list and remove this
	 condition expression.  */
      tree_if_convert_cond_expr (loop, t, cond, bsi);
      cond = NULL_TREE;
      break;

    default:
      gcc_unreachable ();
    }
  return cond;
}

/* STMT is COND_EXPR. Update two destination's predicate list.
   Remove COND_EXPR, if it is not the loop exit condition. Otherwise
   update loop exit condition appropriately.  BSI is the iterator
   used to traverse statement list. STMT is part of loop LOOP.  */

static void
tree_if_convert_cond_expr (struct loop *loop, tree stmt, tree cond,
			   block_stmt_iterator *bsi)
{
  tree c, c2, new_cond;
  edge true_edge, false_edge;
  new_cond = NULL_TREE;

  gcc_assert (TREE_CODE (stmt) == COND_EXPR);

  c = COND_EXPR_COND (stmt);

  /* Create temp. for condition.  */
  if (!is_gimple_condexpr (c))
    {
      tree new_stmt;
      new_stmt = ifc_temp_var (TREE_TYPE (c), unshare_expr (c));
      bsi_insert_before (bsi, new_stmt, BSI_SAME_STMT);
      c = TREE_OPERAND (new_stmt, 0);
    }

  extract_true_false_edges_from_block (bb_for_stmt (stmt),
 				       &true_edge, &false_edge);

  /* Add new condition into destination's predicate list.  */

  /* If 'c' is true then TRUE_EDGE is taken.  */
  new_cond = add_to_dst_predicate_list (loop, true_edge->dest, cond,
					unshare_expr (c), bsi);

  if (!is_gimple_reg(c) && is_gimple_condexpr (c))
    {
      tree new_stmt;
      new_stmt = ifc_temp_var (TREE_TYPE (c), unshare_expr (c));
      bsi_insert_before (bsi, new_stmt, BSI_SAME_STMT);
      c = TREE_OPERAND (new_stmt, 0);
    }

  /* If 'c' is false then FALSE_EDGE is taken.  */
  c2 = invert_truthvalue (unshare_expr (c));
  add_to_dst_predicate_list (loop, false_edge->dest, cond, c2, bsi);

  /* Now this conditional statement is redundant. Remove it.
     But, do not remove exit condition! Update exit condition
     using new condition.  */
  if (!bb_with_exit_edge_p (bb_for_stmt (stmt)))
    {
      bsi_remove (bsi);
      cond = NULL_TREE;
    }
  return;
}

/* Return true, iff PHI is if-convertible. PHI is part of loop LOOP
   and it belongs to basic block BB.
   PHI is not if-convertible
   - if it has more than 2 arguments.
   - Virtual PHI is immediately used in another PHI node.  */

static bool
if_convertible_phi_p (struct loop *loop, basic_block bb, tree phi)
{
  if (dump_file && (dump_flags & TDF_DETAILS))
    {
      fprintf (dump_file, "-------------------------\n");
      print_generic_stmt (dump_file, phi, TDF_SLIM);
    }

  if (bb != loop->header && PHI_NUM_ARGS (phi) != 2)
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file, "More than two phi node args.\n");
      return false;
    }

  if (!is_gimple_reg (SSA_NAME_VAR (PHI_RESULT (phi))))
    {
      int j;
      dataflow_t df = get_immediate_uses (phi);
      int num_uses = num_immediate_uses (df);
      for (j = 0; j < num_uses; j++)
	{
	  tree use = immediate_use (df, j);
	  if (TREE_CODE (use) == PHI_NODE)
	    {
	      if (dump_file && (dump_flags & TDF_DETAILS))
		fprintf (dump_file, "Difficult to handle this virtual phi.\n");
	      return false;
	    }
	}
    }

  return true;
}

/* Return true, if M_EXPR is if-convertible.
   MODIFY_EXPR is not if-convertible if,
   - It is not movable.
   - It could trap.
   - LHS is not var decl.
  MODIFY_EXPR is part of block BB, which is inside loop LOOP.
*/

static bool
if_convertible_modify_expr_p (struct loop *loop, basic_block bb, tree m_expr)
{
  if (dump_file && (dump_flags & TDF_DETAILS))
    {
      fprintf (dump_file, "-------------------------\n");
      print_generic_stmt (dump_file, m_expr, TDF_SLIM);
    }

  /* Be conservative and do not handle immovable expressions.  */
  if (movement_possibility (m_expr) == MOVE_IMPOSSIBLE)
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file, "stmt is movable. Don't take risk\n");
      return false;
    }

  /* See if it needs speculative loading or not.  */
  if (bb != loop->header
      && tree_could_trap_p (TREE_OPERAND (m_expr, 1)))
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file, "tree could trap...\n");
      return false;
    }

  if (TREE_CODE (TREE_OPERAND (m_expr, 1)) == CALL_EXPR)
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file, "CALL_EXPR \n");
      return false;
    }

  if (TREE_CODE (TREE_OPERAND (m_expr, 0)) != SSA_NAME
      && bb != loop->header
      && !bb_with_exit_edge_p (bb))
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	{
	  fprintf (dump_file, "LHS is not var\n");
	  print_generic_stmt (dump_file, m_expr, TDF_SLIM);
	}
      return false;
    }


  return true;
}

/* Return true, iff STMT is if-convertible.
   Statement is if-convertible if,
   - It is if-convertible MODIFY_EXPR
   - IT is LABEL_EXPR, GOTO_EXPR or COND_EXPR.
   STMT is inside block BB, which is inside loop LOOP.  */

static bool
if_convertible_stmt_p (struct loop *loop, basic_block bb, tree stmt)
{
  switch (TREE_CODE (stmt))
    {
    case LABEL_EXPR:
      break;

    case MODIFY_EXPR:

      if (!if_convertible_modify_expr_p (loop, bb, stmt))
	return false;
      break;

    case GOTO_EXPR:
    case COND_EXPR:
      break;

    default:
      /* Don't know what to do with 'em so don't do anything.  */
      if (dump_file && (dump_flags & TDF_DETAILS))
	{
	  fprintf (dump_file, "don't know what to do\n");
	  print_generic_stmt (dump_file, stmt, TDF_SLIM);
	}
      return false;
      break;
    }

  return true;
}

/* Return true, iff BB is if-convertible.
   Note: This routine does _not_ check basic block statements and phis.
   Basic block is not if-convertible if,
   - Basic block is non-empty and it is after exit block (in BFS order).
   - Basic block is after exit block but before latch.
   - Basic block edge(s) is not normal.
   EXIT_BB_SEEN is true if basic block with exit edge is already seen.
   BB is inside loop LOOP.  */

static bool
if_convertible_bb_p (struct loop *loop, basic_block bb, bool exit_bb_seen)
{
  edge e;
  edge_iterator ei;

  if (dump_file && (dump_flags & TDF_DETAILS))
    fprintf (dump_file, "----------[%d]-------------\n", bb->index);

  if (exit_bb_seen)
    {
      if (bb != loop->latch)
	{
	  if (dump_file && (dump_flags & TDF_DETAILS))
	    fprintf (dump_file, "basic block after exit bb but before latch\n");
	  return false;
	}
      else if (!empty_block_p (bb))
	{
	  if (dump_file && (dump_flags & TDF_DETAILS))
	    fprintf (dump_file, "non empty basic block after exit bb\n");
	  return false;
	}
    }

  /* Be less adventurous and handle only normal edges.  */
  FOR_EACH_EDGE (e, ei, bb->succs)
    if (e->flags &
	(EDGE_ABNORMAL_CALL | EDGE_EH | EDGE_ABNORMAL | EDGE_IRREDUCIBLE_LOOP))
      {
	if (dump_file && (dump_flags & TDF_DETAILS))
	  fprintf (dump_file,"Difficult to handle edges\n");
	return false;
      }

  return true;
}

/* Return true, iff LOOP is if-convertible.
   LOOP is if-convertible if,
   - It is innermost.
   - It has two or more basic blocks.
   - It has only one exit.
   - Loop header is not the exit edge.
   - If its basic blocks and phi nodes are if convertible. See above for
     more info.
   FOR_VECTORIZER enables vectorizer specific checks. For example, support
   for vector conditions, data dependency checks etc.. (Not implemented yet).  */

static bool
if_convertible_loop_p (struct loop *loop, bool for_vectorizer ATTRIBUTE_UNUSED)
{
  tree phi;
  basic_block bb;
  block_stmt_iterator itr;
  unsigned int i;
  edge e;
  edge_iterator ei;
  bool exit_bb_seen = false;

  /* Handle only inner most loop.  */
  if (!loop || loop->inner)
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file, "not inner most loop\n");
      return false;
    }

  flow_loop_scan (loop, LOOP_ALL);

  /* If only one block, no need for if-conversion.  */
  if (loop->num_nodes <= 2)
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file, "less than 2 basic blocks\n");
      return false;
    }

  /* More than one loop exit is too much to handle.  */
  if (loop->num_exits > 1)
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file, "multiple exits\n");
      return false;
    }

  /* ??? Check target's vector conditional operation support for vectorizer.  */

  /* If one of the loop header's edge is exit edge then do not apply
     if-conversion.  */
  FOR_EACH_EDGE (e, ei, loop->header->succs)
    if ( e->flags & EDGE_LOOP_EXIT)
      return false;

  compute_immediate_uses (TDFA_USE_OPS|TDFA_USE_VOPS, NULL);

  calculate_dominance_info (CDI_DOMINATORS);
  calculate_dominance_info (CDI_POST_DOMINATORS);

  /* Allow statements that can be handled during if-conversion.  */
  ifc_bbs = get_loop_body_in_if_conv_order (loop);
  if (!ifc_bbs)
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file,"Irreducible loop\n");
      free_dominance_info (CDI_POST_DOMINATORS);
      return false;
    }

  for (i = 0; i < loop->num_nodes; i++)
    {
      bb = ifc_bbs[i];

      if (!if_convertible_bb_p (loop, bb, exit_bb_seen))
	return false;

      /* Check statements.  */
      for (itr = bsi_start (bb); !bsi_end_p (itr); bsi_next (&itr))
	if (!if_convertible_stmt_p (loop, bb, bsi_stmt (itr)))
	  return false;
      /* ??? Check data dependency for vectorizer.  */

      /* What about phi nodes ? */
      for (phi = phi_nodes (bb); phi; phi = PHI_CHAIN (phi))
	if (!if_convertible_phi_p (loop, bb, phi))
	  return false;

      if (bb_with_exit_edge_p (bb))
	exit_bb_seen = true;
    }

  /* OK. Did not find any potential issues so go ahead in if-convert
     this loop. Now there is no looking back.  */
  if (dump_file)
    fprintf (dump_file,"Applying if-conversion\n");

  free_dominance_info (CDI_POST_DOMINATORS);
  return true;
}

/* Add condition COND into predicate list of basic block BB.  */

static void
add_to_predicate_list (basic_block bb, tree new_cond)
{
  tree cond = bb->aux;

  if (cond)
    cond = fold (build (TRUTH_OR_EXPR, boolean_type_node,
			unshare_expr (cond), new_cond));
  else
    cond = new_cond;

  bb->aux = cond;
}

/* Add condition COND into BB's predicate list.  PREV_COND is
   existing condition.  */

static tree
add_to_dst_predicate_list (struct loop * loop, basic_block bb,
			   tree prev_cond, tree cond,
			   block_stmt_iterator *bsi)
{
  tree new_cond = NULL_TREE;

  if (!flow_bb_inside_loop_p (loop, bb))
    return NULL_TREE;

  if (prev_cond == boolean_true_node || !prev_cond)
    new_cond = unshare_expr (cond);
  else
    {
      tree tmp;
      tree tmp_stmt = NULL_TREE;
      tree tmp_stmts1 = NULL_TREE;
      tree tmp_stmts2 = NULL_TREE;
      prev_cond = force_gimple_operand (unshare_expr (prev_cond),
					&tmp_stmts1, true, NULL);
      if (tmp_stmts1)
        bsi_insert_before (bsi, tmp_stmts1, BSI_SAME_STMT);

      cond = force_gimple_operand (unshare_expr (cond),
				   &tmp_stmts2, true, NULL);
      if (tmp_stmts2)
        bsi_insert_before (bsi, tmp_stmts2, BSI_SAME_STMT);

      /* new_cond == prev_cond AND cond */
      tmp = build (TRUTH_AND_EXPR, boolean_type_node,
		   unshare_expr (prev_cond), cond);
      tmp_stmt = ifc_temp_var (boolean_type_node, tmp);
      bsi_insert_before (bsi, tmp_stmt, BSI_SAME_STMT);
      new_cond = TREE_OPERAND (tmp_stmt, 0);
    }
  add_to_predicate_list (bb, new_cond);
  return new_cond;
}

/* During if-conversion aux field from basic block is used to hold predicate
   list. Clean each basic block's predicate list for the given LOOP.  */

static void
clean_predicate_lists (struct loop *loop)
{
  basic_block *bb;
  unsigned int i;
  bb = get_loop_body (loop);
  for (i = 0; i < loop->num_nodes; i++)
    bb[i]->aux = NULL;

  free (bb);
}

/* Basic block BB has two predecessors. Using predecessor's aux field, set
   appropriate condition COND for the PHI node replacement. Return true block
   whose phi arguments are selected when cond is true.  */

static basic_block
find_phi_replacement_condition (basic_block bb, tree *cond,
                                block_stmt_iterator *bsi)
{
  edge e;
  basic_block p1 = NULL;
  basic_block p2 = NULL;
  basic_block true_bb = NULL; 
  tree tmp_cond;
  edge_iterator ei;

  FOR_EACH_EDGE (e, ei, bb->preds)
    {
      if (p1 == NULL)
	p1 = e->src;
      else 
	{
	  gcc_assert (!p2);
	  p2 = e->src;
	}
    }

  /* Use condition that is not TRUTH_NOT_EXPR in conditional modify expr.  */
  tmp_cond = p1->aux;
  if (TREE_CODE (tmp_cond) == TRUTH_NOT_EXPR)
    {
      *cond  = p2->aux;
      true_bb = p2;
    }
  else
    {
      *cond  = p1->aux;
      true_bb = p1;
    }

  /* Create temp. for the condition. Vectorizer prefers to have gimple
     value as condition. Various targets use different means to communicate
     condition in vector compare operation. Using gimple value allows compiler
     to emit vector compare and select RTL without exposing compare's result.  */
  if (!is_gimple_reg (*cond) && !is_gimple_condexpr (*cond))
    {
      tree new_stmt;

      new_stmt = ifc_temp_var (TREE_TYPE (*cond), unshare_expr (*cond));
      bsi_insert_after (bsi, new_stmt, BSI_SAME_STMT);
      bsi_next (bsi);
      *cond = TREE_OPERAND (new_stmt, 0);
    }

  gcc_assert (*cond);

  return true_bb;
}


/* Replace PHI node with conditional modify expr using COND.
   This routine does not handle PHI nodes with more than two arguments.
   For example,
     S1: A = PHI <x1(1), x2(5)
   is converted into,
     S2: A = cond ? x1 : x2;
   S2 is inserted at the top of basic block's statement list.
   When COND is true, phi arg from TRUE_BB is selected.
*/

static void
replace_phi_with_cond_modify_expr (tree phi, tree cond, basic_block true_bb,
                                   block_stmt_iterator *bsi)
{
  tree new_stmt;
  basic_block bb;
  tree rhs;
  tree arg_0, arg_1;

  gcc_assert (TREE_CODE (phi) == PHI_NODE);
  
  /* If this is not filtered earlier, then now it is too late.  */
  gcc_assert (PHI_NUM_ARGS (phi) == 2);

  /* Find basic block and initialize iterator.  */
  bb = bb_for_stmt (phi);

  new_stmt = NULL_TREE;
  arg_0 = NULL_TREE;
  arg_1 = NULL_TREE;

  /* Use condition that is not TRUTH_NOT_EXPR in conditional modify expr.  */
  if (EDGE_PRED (bb, 1)->src == true_bb)
    {
      arg_0 = PHI_ARG_DEF (phi, 1);
      arg_1 = PHI_ARG_DEF (phi, 0);
    }
  else
    {
      arg_0 = PHI_ARG_DEF (phi, 0);
      arg_1 = PHI_ARG_DEF (phi, 1);
    }

  /* Build new RHS using selected condition and arguments.  */
  rhs = build (COND_EXPR, TREE_TYPE (PHI_RESULT (phi)),
	       unshare_expr (cond), unshare_expr (arg_0),
	       unshare_expr (arg_1));

  /* Create new MODIFY expression using RHS.  */
  new_stmt = build (MODIFY_EXPR, TREE_TYPE (PHI_RESULT (phi)),
		    unshare_expr (PHI_RESULT (phi)), rhs);

  /* Make new statement definition of the original phi result.  */
  SSA_NAME_DEF_STMT (PHI_RESULT (phi)) = new_stmt;

  /* Set basic block and insert using iterator.  */
  set_bb_for_stmt (new_stmt, bb);

  bsi_insert_after (bsi, new_stmt, BSI_SAME_STMT);
  bsi_next (bsi);

  modify_stmt (new_stmt);

  if (dump_file && (dump_flags & TDF_DETAILS))
    {
      fprintf (dump_file, "new phi replacement stmt\n");
      print_generic_stmt (dump_file, new_stmt, TDF_SLIM);
    }
}

/* Process phi nodes for the given  LOOP.  Replace phi nodes with cond
   modify expr.  */

static void
process_phi_nodes (struct loop *loop)
{
  basic_block bb;
  unsigned int orig_loop_num_nodes = loop->num_nodes;
  unsigned int i;

  /* Replace phi nodes with cond. modify expr.  */
  for (i = 1; i < orig_loop_num_nodes; i++)
    {
      tree phi, cond;
      block_stmt_iterator bsi;
      basic_block true_bb = NULL;
      bb = ifc_bbs[i];

      if (bb == loop->header)
	continue;

      phi = phi_nodes (bb);
      bsi = bsi_after_labels (bb);

      /* BB has two predecessors. Using predecessor's aux field, set
	 appropriate condition for the PHI node replacement.  */
      if (phi)
	true_bb = find_phi_replacement_condition (bb, &cond, &bsi);

      while (phi)
	{
	  tree next = PHI_CHAIN (phi);
	  replace_phi_with_cond_modify_expr (phi, cond, true_bb, &bsi);
	  release_phi_node (phi);
	  phi = next;
	}
      bb_ann (bb)->phi_nodes = NULL;
    }
  return;
}

/* Combine all basic block from the given LOOP into one or two super
   basic block.  Replace PHI nodes with conditional modify expression.  */

static void
combine_blocks (struct loop *loop)
{
  basic_block bb, exit_bb, merge_target_bb;
  unsigned int orig_loop_num_nodes = loop->num_nodes;
  unsigned int i;
  unsigned int n_exits;
  edge *exits = get_loop_exit_edges (loop, &n_exits);
  /* Process phi nodes to prepare blocks for merge.  */
  process_phi_nodes (loop);

  exit_bb = NULL;

  /* Merge basic blocks */
  merge_target_bb = loop->header;
  for (i = 1; i < orig_loop_num_nodes; i++)
    {
      edge e;
      block_stmt_iterator bsi;
      tree_stmt_iterator last;

      bb = ifc_bbs[i];

      if (!exit_bb && bb_with_exit_edge_p (bb))
	  exit_bb = bb;

      if (bb == exit_bb)
	{
	  edge new_e;
	  edge_iterator ei;

	  /* Connect this node with loop header.  */
	  new_e = make_edge (ifc_bbs[0], bb, EDGE_FALLTHRU);
	  set_immediate_dominator (CDI_DOMINATORS, bb, ifc_bbs[0]);

	  if (exit_bb != loop->latch)
	    {
	      /* Redirect non-exit edge to loop->latch.  */
	      FOR_EACH_EDGE (e, ei, bb->succs)
		if (!(e->flags & EDGE_LOOP_EXIT))
		  {
		    redirect_edge_and_branch (e, loop->latch);
		    set_immediate_dominator (CDI_DOMINATORS, loop->latch, bb);
		  }
	    }
	  continue;
	}

      if (bb == loop->latch && empty_block_p (bb))
	continue;

      /* It is time to remove this basic block.	 First remove edges.  */
      while (EDGE_COUNT (bb->preds) > 0)
	remove_edge (EDGE_PRED (bb, 0));

      /* This is loop latch and loop does not have exit then do not
 	 delete this basic block. Just remove its PREDS and reconnect 
 	 loop->header and loop->latch blocks.  */
      if (bb == loop->latch && n_exits == 0)
 	{
	  exits = NULL; /* To suppress unused warning.  */
 	  make_edge (loop->header, loop->latch, EDGE_FALLTHRU);
 	  set_immediate_dominator (CDI_DOMINATORS, loop->latch, loop->header);
	  continue;
 	}

      while (EDGE_COUNT (bb->succs) > 0)
	remove_edge (EDGE_SUCC (bb, 0));

      /* Remove labels and make stmts member of loop->header.  */
      for (bsi = bsi_start (bb); !bsi_end_p (bsi); )
	{
	  if (TREE_CODE (bsi_stmt (bsi)) == LABEL_EXPR)
	    bsi_remove (&bsi);
	  else
	    {
	      set_bb_for_stmt (bsi_stmt (bsi), merge_target_bb);
	      bsi_next (&bsi);
	    }
	}

      /* Update stmt list.  */
      last = tsi_last (merge_target_bb->stmt_list);
      tsi_link_after (&last, bb->stmt_list, TSI_NEW_STMT);
      bb->stmt_list = NULL;

      /* Update dominator info.  */
      if (dom_computed[CDI_DOMINATORS])
	delete_from_dominance_info (CDI_DOMINATORS, bb);
      if (dom_computed[CDI_POST_DOMINATORS])
	delete_from_dominance_info (CDI_POST_DOMINATORS, bb);

      /* Remove basic block.  */
      if (bb == loop->latch)
	loop->latch = merge_target_bb;
      remove_bb_from_loops (bb);
      expunge_block (bb);
    }

  /* Now if possible, merge loop header and block with exit edge.
     This reduces number of basic blocks to 2. Auto vectorizer addresses
     loops with two nodes only.  FIXME: Use cleanup_tree_cfg().  */
  if (exit_bb
      && loop->header != loop->latch
      && exit_bb != loop->latch 
      && empty_block_p (loop->latch))
    {
      if (can_merge_blocks_p (loop->header, exit_bb))
	{
	  remove_bb_from_loops (exit_bb);
	  merge_blocks (loop->header, exit_bb);
	}
    }
}

/* Make new  temp variable of type TYPE. Add MODIFY_EXPR to assign EXP
   to the new variable.  */

static tree
ifc_temp_var (tree type, tree exp)
{
  const char *name = "_ifc_";
  tree var, stmt, new_name;

  if (is_gimple_reg (exp))
    return exp;

  /* Create new temporary variable.  */
  var = create_tmp_var (type, name);
  add_referenced_tmp_var (var);

  /* Build new statement to assign EXP to new variable.  */
  stmt = build (MODIFY_EXPR, type, var, exp);

  /* Get SSA name for the new variable and set make new statement
     its definition statement.  */
  new_name = make_ssa_name (var, stmt);
  TREE_OPERAND (stmt, 0) = new_name;
  SSA_NAME_DEF_STMT (new_name) = stmt;

  return stmt;
}


/* Return TRUE iff, all pred blocks of BB are visited.
   Bitmap VISITED keeps history of visited blocks.  */

static bool
pred_blocks_visited_p (basic_block bb, bitmap *visited)
{
  edge e;
  edge_iterator ei;
  FOR_EACH_EDGE (e, ei, bb->preds)
    if (!bitmap_bit_p (*visited, e->src->index))
      return false;

  return true;
}

/* Get body of a LOOP in suitable order for if-conversion.
   It is caller's responsibility to deallocate basic block
   list.  If-conversion suitable order is, BFS order with one
   additional constraint. Select block in BFS block, if all
   pred are already selected.  */

static basic_block *
get_loop_body_in_if_conv_order (const struct loop *loop)
{
  basic_block *blocks, *blocks_in_bfs_order;
  basic_block bb;
  bitmap visited;
  unsigned int index = 0;
  unsigned int visited_count = 0;

  gcc_assert (loop->num_nodes);
  gcc_assert (loop->latch != EXIT_BLOCK_PTR);

  blocks = xcalloc (loop->num_nodes, sizeof (basic_block));
  visited = BITMAP_ALLOC (NULL);

  blocks_in_bfs_order = get_loop_body_in_bfs_order (loop);

  index = 0;
  while (index < loop->num_nodes)
    {
      bb = blocks_in_bfs_order [index];

      if (bb->flags & BB_IRREDUCIBLE_LOOP)
	{
	  free (blocks_in_bfs_order);
	  BITMAP_FREE (visited);
	  free (blocks);
	  return NULL;
	}
      if (!bitmap_bit_p (visited, bb->index))
	{
	  if (pred_blocks_visited_p (bb, &visited)
	      || bb == loop->header)
	    {
	      /* This block is now visited.  */
	      bitmap_set_bit (visited, bb->index);
	      blocks[visited_count++] = bb;
	    }
	}
      index++;
      if (index == loop->num_nodes
	  && visited_count != loop->num_nodes)
	{
	  /* Not done yet.  */
	  index = 0;
	}
    }
  free (blocks_in_bfs_order);
  BITMAP_FREE (visited);
  return blocks;
}

/* Return true if one of the basic block BB edge is loop exit.  */

static bool
bb_with_exit_edge_p (basic_block bb)
{
  edge e;
  edge_iterator ei;
  bool exit_edge_found = false;

  FOR_EACH_EDGE (e, ei, bb->succs)
    if (e->flags & EDGE_LOOP_EXIT)
      {
	exit_edge_found = true;
	break;
      }

  return exit_edge_found;
}

/* Tree if-conversion pass management.  */

static void
main_tree_if_conversion (void)
{
  unsigned i, loop_num;
  struct loop *loop;

  if (!current_loops)
    return;

  loop_num = current_loops->num;
  for (i = 0; i < loop_num; i++)
    {
      loop =  current_loops->parray[i];
      if (!loop)
      continue;

      tree_if_conversion (loop, true);
    }

}

static bool
gate_tree_if_conversion (void)
{
  return flag_tree_vectorize != 0;
}

struct tree_opt_pass pass_if_conversion =
{
  "ifcvt",                           /* name */
  gate_tree_if_conversion,           /* gate */
  main_tree_if_conversion,           /* execute */
  NULL,                              /* sub */
  NULL,                              /* next */
  0,                                 /* static_pass_number */
  0,                                 /* tv_id */
  PROP_cfg | PROP_ssa | PROP_alias,  /* properties_required */
  0,                                 /* properties_provided */
  0,                                 /* properties_destroyed */
  TODO_dump_func,                    /* todo_flags_start */
  TODO_dump_func
    | TODO_verify_ssa
    | TODO_verify_stmts
    | TODO_verify_flow,              /* todo_flags_finish */
  0				     /* letter */
};
