/* Tree lowering pass.  This pass converts the GENERIC functions-as-trees
   tree representation into the GIMPLE form.
   Copyright (C) 2002, 2003, 2004 Free Software Foundation, Inc.
   Major work done by Sebastian Pop <s.pop@laposte.net>,
   Diego Novillo <dnovillo@redhat.com> and Jason Merrill <jason@redhat.com>.

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

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "rtl.h"
#include "errors.h"
#include "varray.h"
#include "tree-gimple.h"
#include "tree-inline.h"
#include "diagnostic.h"
#include "langhooks.h"
#include "langhooks-def.h"
#include "tree-flow.h"
#include "cgraph.h"
#include "timevar.h"
#include "except.h"
#include "hashtab.h"
#include "flags.h"
#include "real.h"
#include "function.h"
#include "output.h"
#include "expr.h"
#include "ggc.h"
#include "target.h"

static struct gimplify_ctx
{
  tree current_bind_expr;
  tree temps;
  tree conditional_cleanups;
  tree exit_label;
  tree return_temp;
  varray_type case_labels;
  /* The formal temporary table.  Should this be persistent?  */
  htab_t temp_htab;
  int conditions;
  bool save_stack;
  bool into_ssa;
} *gimplify_ctxp;


/* Formal (expression) temporary table handling: Multiple occurrences of
   the same scalar expression are evaluated into the same temporary.  */

typedef struct gimple_temp_hash_elt
{
  tree val;   /* Key */
  tree temp;  /* Value */
} elt_t;

/* Forward declarations.  */
static enum gimplify_status gimplify_compound_expr (tree *, tree *, bool);
#ifdef ENABLE_CHECKING
static bool cpt_same_type (tree a, tree b);
#endif


/* Return a hash value for a formal temporary table entry.  */

static hashval_t
gimple_tree_hash (const void *p)
{
  tree t = ((const elt_t *) p)->val;
  return iterative_hash_expr (t, 0);
}

/* Compare two formal temporary table entries.  */

static int
gimple_tree_eq (const void *p1, const void *p2)
{
  tree t1 = ((const elt_t *) p1)->val;
  tree t2 = ((const elt_t *) p2)->val;
  enum tree_code code = TREE_CODE (t1);

  if (TREE_CODE (t2) != code
      || TREE_TYPE (t1) != TREE_TYPE (t2))
    return 0;

  if (!operand_equal_p (t1, t2, 0))
    return 0;

  /* Only allow them to compare equal if they also hash equal; otherwise
     results are nondeterminate, and we fail bootstrap comparison.  */
  gcc_assert (gimple_tree_hash (p1) == gimple_tree_hash (p2));

  return 1;
}

/* Set up a context for the gimplifier.  */

void
push_gimplify_context (void)
{
  gcc_assert (!gimplify_ctxp);
  gimplify_ctxp
    = (struct gimplify_ctx *) xcalloc (1, sizeof (struct gimplify_ctx));
  if (optimize)
    gimplify_ctxp->temp_htab
      = htab_create (1000, gimple_tree_hash, gimple_tree_eq, free);
  else
    gimplify_ctxp->temp_htab = NULL;
}

/* Tear down a context for the gimplifier.  If BODY is non-null, then
   put the temporaries into the outer BIND_EXPR.  Otherwise, put them
   in the unexpanded_var_list.  */

void
pop_gimplify_context (tree body)
{
  tree t;

  gcc_assert (gimplify_ctxp && !gimplify_ctxp->current_bind_expr);

  for (t = gimplify_ctxp->temps; t ; t = TREE_CHAIN (t))
    DECL_GIMPLE_FORMAL_TEMP_P (t) = 0;

  if (body)
    declare_tmp_vars (gimplify_ctxp->temps, body);
  else
    record_vars (gimplify_ctxp->temps);

#if 0
  if (!quiet_flag && optimize)
    fprintf (stderr, " collisions: %f ",
	     htab_collisions (gimplify_ctxp->temp_htab));
#endif

  if (optimize)
    htab_delete (gimplify_ctxp->temp_htab);
  free (gimplify_ctxp);
  gimplify_ctxp = NULL;
}

void
gimple_push_bind_expr (tree bind)
{
  TREE_CHAIN (bind) = gimplify_ctxp->current_bind_expr;
  gimplify_ctxp->current_bind_expr = bind;
}

void
gimple_pop_bind_expr (void)
{
  gimplify_ctxp->current_bind_expr
    = TREE_CHAIN (gimplify_ctxp->current_bind_expr);
}

tree
gimple_current_bind_expr (void)
{
  return gimplify_ctxp->current_bind_expr;
}

/* Returns true iff there is a COND_EXPR between us and the innermost
   CLEANUP_POINT_EXPR.  This info is used by gimple_push_cleanup.  */

static bool
gimple_conditional_context (void)
{
  return gimplify_ctxp->conditions > 0;
}

/* Note that we've entered a COND_EXPR.  */

static void
gimple_push_condition (void)
{
  ++(gimplify_ctxp->conditions);
}

/* Note that we've left a COND_EXPR.  If we're back at unconditional scope
   now, add any conditional cleanups we've seen to the prequeue.  */

static void
gimple_pop_condition (tree *pre_p)
{
  int conds = --(gimplify_ctxp->conditions);

  gcc_assert (conds >= 0);
  if (conds == 0)
    {
      append_to_statement_list (gimplify_ctxp->conditional_cleanups, pre_p);
      gimplify_ctxp->conditional_cleanups = NULL_TREE;
    }
}

/* A subroutine of append_to_statement_list{,_force}.  */

static void
append_to_statement_list_1 (tree t, tree *list_p, bool side_effects)
{
  tree list = *list_p;
  tree_stmt_iterator i;

  if (!side_effects)
    return;

  if (!list)
    {
      if (t && TREE_CODE (t) == STATEMENT_LIST)
	{
	  *list_p = t;
	  return;
	}
      *list_p = list = alloc_stmt_list ();
    }

  i = tsi_last (list);
  tsi_link_after (&i, t, TSI_CONTINUE_LINKING);
}

/* Add T to the end of the list container pointed by LIST_P.
   If T is an expression with no effects, it is ignored.  */

void
append_to_statement_list (tree t, tree *list_p)
{
  append_to_statement_list_1 (t, list_p, t ? TREE_SIDE_EFFECTS (t) : false);
}

/* Similar, but the statement is always added, regardless of side effects.  */

void
append_to_statement_list_force (tree t, tree *list_p)
{
  append_to_statement_list_1 (t, list_p, t != NULL);
}

/* Both gimplify the statement T and append it to LIST_P.  */

void
gimplify_and_add (tree t, tree *list_p)
{
  gimplify_stmt (&t);
  append_to_statement_list (t, list_p);
}

/* Strip off a legitimate source ending from the input string NAME of
   length LEN.  Rather than having to know the names used by all of
   our front ends, we strip off an ending of a period followed by
   up to five characters.  (Java uses ".class".)  */

static inline void
remove_suffix (char *name, int len)
{
  int i;

  for (i = 2;  i < 8 && len > i;  i++)
    {
      if (name[len - i] == '.')
	{
	  name[len - i] = '\0';
	  break;
	}
    }
}

/* Create a nameless artificial label and put it in the current function
   context.  Returns the newly created label.  */

tree
create_artificial_label (void)
{
  tree lab = build_decl (LABEL_DECL, NULL_TREE, void_type_node);

  DECL_ARTIFICIAL (lab) = 1;
  DECL_CONTEXT (lab) = current_function_decl;
  return lab;
}

/* Create a new temporary name with PREFIX.  Returns an identifier.  */

static GTY(()) unsigned int tmp_var_id_num;

tree
create_tmp_var_name (const char *prefix)
{
  char *tmp_name;

  if (prefix)
    {
      char *preftmp = ASTRDUP (prefix);

      remove_suffix (preftmp, strlen (preftmp));
      prefix = preftmp;
    }

  ASM_FORMAT_PRIVATE_NAME (tmp_name, prefix ? prefix : "T", tmp_var_id_num++);
  return get_identifier (tmp_name);
}


/* Create a new temporary variable declaration of type TYPE.
   Does NOT push it into the current binding.  */

tree
create_tmp_var_raw (tree type, const char *prefix)
{
  tree tmp_var;
  tree new_type;

  /* Make the type of the variable writable.  */
  new_type = build_type_variant (type, 0, 0);
  TYPE_ATTRIBUTES (new_type) = TYPE_ATTRIBUTES (type);

  tmp_var = build_decl (VAR_DECL, prefix ? create_tmp_var_name (prefix) : NULL,
			type);

  /* The variable was declared by the compiler.  */
  DECL_ARTIFICIAL (tmp_var) = 1;
  /* And we don't want debug info for it.  */
  DECL_IGNORED_P (tmp_var) = 1;

  /* Make the variable writable.  */
  TREE_READONLY (tmp_var) = 0;

  DECL_EXTERNAL (tmp_var) = 0;
  TREE_STATIC (tmp_var) = 0;
  TREE_USED (tmp_var) = 1;

  return tmp_var;
}

/* Create a new temporary variable declaration of type TYPE.  DOES push the
   variable into the current binding.  Further, assume that this is called
   only from gimplification or optimization, at which point the creation of
   certain types are bugs.  */

tree
create_tmp_var (tree type, const char *prefix)
{
  tree tmp_var;

  /* We don't allow types that are addressable (meaning we can't make copies),
     incomplete, or of variable size.  */
  gcc_assert (!TREE_ADDRESSABLE (type)
	      && COMPLETE_TYPE_P (type)
	      && TREE_CODE (TYPE_SIZE_UNIT (type)) == INTEGER_CST);

  tmp_var = create_tmp_var_raw (type, prefix);
  gimple_add_tmp_var (tmp_var);
  return tmp_var;
}

/*  Given a tree, try to return a useful variable name that we can use
    to prefix a temporary that is being assigned the value of the tree.
    I.E. given  <temp> = &A, return A.  */

const char *
get_name (tree t)
{
  tree stripped_decl;

  stripped_decl = t;
  STRIP_NOPS (stripped_decl);
  if (DECL_P (stripped_decl) && DECL_NAME (stripped_decl))
    return IDENTIFIER_POINTER (DECL_NAME (stripped_decl));
  else
    {
      switch (TREE_CODE (stripped_decl))
	{
	case ADDR_EXPR:
	  return get_name (TREE_OPERAND (stripped_decl, 0));
	  break;
	default:
	  return NULL;
	}
    }
}

/* Create a temporary with a name derived from VAL.  Subroutine of
   lookup_tmp_var; nobody else should call this function.  */

static inline tree
create_tmp_from_val (tree val)
{
  return create_tmp_var (TREE_TYPE (val), get_name (val));
}

/* Create a temporary to hold the value of VAL.  If IS_FORMAL, try to reuse
   an existing expression temporary.  */

static tree
lookup_tmp_var (tree val, bool is_formal)
{
  tree ret;

  /* If not optimizing, never really reuse a temporary.  local-alloc
     won't allocate any variable that is used in more than one basic
     block, which means it will go into memory, causing much extra
     work in reload and final and poorer code generation, outweighing
     the extra memory allocation here.  */
  if (!optimize || !is_formal || TREE_SIDE_EFFECTS (val))
    ret = create_tmp_from_val (val);
  else
    {
      elt_t elt, *elt_p;
      void **slot;

      elt.val = val;
      slot = htab_find_slot (gimplify_ctxp->temp_htab, (void *)&elt, INSERT);
      if (*slot == NULL)
	{
	  elt_p = xmalloc (sizeof (*elt_p));
	  elt_p->val = val;
	  elt_p->temp = ret = create_tmp_from_val (val);
	  *slot = (void *) elt_p;
	}
      else
	{
	  elt_p = (elt_t *) *slot;
          ret = elt_p->temp;
	}
    }

  if (is_formal)
    DECL_GIMPLE_FORMAL_TEMP_P (ret) = 1;

  return ret;
}

/* Returns a formal temporary variable initialized with VAL.  PRE_P is as
   in gimplify_expr.  Only use this function if:

   1) The value of the unfactored expression represented by VAL will not
      change between the initialization and use of the temporary, and
   2) The temporary will not be otherwise modified.

   For instance, #1 means that this is inappropriate for SAVE_EXPR temps,
   and #2 means it is inappropriate for && temps.

   For other cases, use get_initialized_tmp_var instead.  */

static tree
internal_get_tmp_var (tree val, tree *pre_p, tree *post_p, bool is_formal)
{
  tree t, mod;

  gimplify_expr (&val, pre_p, post_p, is_gimple_formal_tmp_rhs, fb_rvalue);

  t = lookup_tmp_var (val, is_formal);

  mod = build (MODIFY_EXPR, TREE_TYPE (t), t, val);

  if (EXPR_HAS_LOCATION (val))
    SET_EXPR_LOCUS (mod, EXPR_LOCUS (val));
  else
    SET_EXPR_LOCATION (mod, input_location);

  /* gimplify_modify_expr might want to reduce this further.  */
  gimplify_and_add (mod, pre_p);

  /* If we're gimplifying into ssa, gimplify_modify_expr will have
     given our temporary an ssa name.  Find and return it.  */
  if (gimplify_ctxp->into_ssa)
    t = TREE_OPERAND (mod, 0);

  return t;
}

tree
get_formal_tmp_var (tree val, tree *pre_p)
{
  return internal_get_tmp_var (val, pre_p, NULL, true);
}

/* Returns a temporary variable initialized with VAL.  PRE_P and POST_P
   are as in gimplify_expr.  */

tree
get_initialized_tmp_var (tree val, tree *pre_p, tree *post_p)
{
  return internal_get_tmp_var (val, pre_p, post_p, false);
}

/* Declares all the variables in VARS in SCOPE.  */

void
declare_tmp_vars (tree vars, tree scope)
{
  tree last = vars;
  if (last)
    {
      tree temps;

      /* C99 mode puts the default 'return 0;' for main outside the outer
	 braces.  So drill down until we find an actual scope.  */
      while (TREE_CODE (scope) == COMPOUND_EXPR)
	scope = TREE_OPERAND (scope, 0);

      gcc_assert (TREE_CODE (scope) == BIND_EXPR);

      temps = nreverse (last);
      TREE_CHAIN (last) = BIND_EXPR_VARS (scope);
      BIND_EXPR_VARS (scope) = temps;
    }
}

void
gimple_add_tmp_var (tree tmp)
{
  gcc_assert (!TREE_CHAIN (tmp) && !DECL_SEEN_IN_BIND_EXPR_P (tmp));

  DECL_CONTEXT (tmp) = current_function_decl;
  DECL_SEEN_IN_BIND_EXPR_P (tmp) = 1;

  if (gimplify_ctxp)
    {
      TREE_CHAIN (tmp) = gimplify_ctxp->temps;
      gimplify_ctxp->temps = tmp;
    }
  else if (cfun)
    record_vars (tmp);
  else
    declare_tmp_vars (tmp, DECL_SAVED_TREE (current_function_decl));
}

/* Determines whether to assign a locus to the statement STMT.  */

static bool
should_carry_locus_p (tree stmt)
{
  /* Don't emit a line note for a label.  We particularly don't want to
     emit one for the break label, since it doesn't actually correspond
     to the beginning of the loop/switch.  */
  if (TREE_CODE (stmt) == LABEL_EXPR)
    return false;

  /* Do not annotate empty statements, since it confuses gcov.  */
  if (!TREE_SIDE_EFFECTS (stmt))
    return false;

  return true;
}

static void
annotate_one_with_locus (tree t, location_t locus)
{
  if (EXPR_P (t) && ! EXPR_HAS_LOCATION (t) && should_carry_locus_p (t))
    SET_EXPR_LOCATION (t, locus);
}

void
annotate_all_with_locus (tree *stmt_p, location_t locus)
{
  tree_stmt_iterator i;

  if (!*stmt_p)
    return;

  for (i = tsi_start (*stmt_p); !tsi_end_p (i); tsi_next (&i))
    {
      tree t = tsi_stmt (i);

      /* Assuming we've already been gimplified, we shouldn't
	  see nested chaining constructs anymore.  */
      gcc_assert (TREE_CODE (t) != STATEMENT_LIST
		  && TREE_CODE (t) != COMPOUND_EXPR);

      annotate_one_with_locus (t, locus);
    }
}

/* Similar to copy_tree_r() but do not copy SAVE_EXPR or TARGET_EXPR nodes.
   These nodes model computations that should only be done once.  If we
   were to unshare something like SAVE_EXPR(i++), the gimplification
   process would create wrong code.  */

static tree
mostly_copy_tree_r (tree *tp, int *walk_subtrees, void *data)
{
  enum tree_code code = TREE_CODE (*tp);
  /* Don't unshare types, decls, constants and SAVE_EXPR nodes.  */
  if (TREE_CODE_CLASS (code) == tcc_type
      || TREE_CODE_CLASS (code) == tcc_declaration
      || TREE_CODE_CLASS (code) == tcc_constant
      || code == SAVE_EXPR || code == TARGET_EXPR
      /* We can't do anything sensible with a BLOCK used as an expression,
	 but we also can't abort when we see it because of non-expression
	 uses.  So just avert our eyes and cross our fingers.  Silly Java.  */
      || code == BLOCK)
    *walk_subtrees = 0;
  else
    {
      gcc_assert (code != BIND_EXPR);
      copy_tree_r (tp, walk_subtrees, data);
    }

  return NULL_TREE;
}

/* APPLE LOCAL begin PR 14498, etc --bowdidge */
/* This routine was deleted from FSF mainline and lno-branch; however,
   we're still using it in the next routine.  Keep a local copy until 
   we can rewrite that other stuff.  The FIXME on this worries me. */

/* Mark all the _DECL nodes under *TP as volatile.  FIXME: This must die
   after VA_ARG_EXPRs are properly lowered.  */

static tree
mark_decls_volatile_r (tree *tp, int *walk_subtrees ATTRIBUTE_UNUSED,
		       void *data ATTRIBUTE_UNUSED)
{
  if (SSA_VAR_P (*tp))
    TREE_THIS_VOLATILE (*tp) = 1;

  return NULL_TREE;
}
/* APPLE LOCAL end PR 14498, etc --bowdidge */

/* Callback for walk_tree to unshare most of the shared trees rooted at
   *TP.  If *TP has been visited already (i.e., TREE_VISITED (*TP) == 1),
   then *TP is deep copied by calling copy_tree_r.

   This unshares the same trees as copy_tree_r with the exception of
   SAVE_EXPR nodes.  These nodes model computations that should only be
   done once.  If we were to unshare something like SAVE_EXPR(i++), the
   gimplification process would create wrong code.  */

static tree
copy_if_shared_r (tree *tp, int *walk_subtrees ATTRIBUTE_UNUSED,
		  void *data ATTRIBUTE_UNUSED)
{
  tree t = *tp;
  enum tree_code code = TREE_CODE (t);

  /* Skip types, decls, and constants.  But we do want to look at their
     types and the bounds of types.  Mark them as visited so we properly
     unmark their subtrees on the unmark pass.  If we've already seen them,
     don't look down further.  */
  if (TREE_CODE_CLASS (code) == tcc_type
      || TREE_CODE_CLASS (code) == tcc_declaration
      || TREE_CODE_CLASS (code) == tcc_constant)
    {
      if (TREE_VISITED (t))
	*walk_subtrees = 0;
      else
	TREE_VISITED (t) = 1;
    }

  /* If this node has been visited already, unshare it and don't look
     any deeper.  */
  else if (TREE_VISITED (t))
    {
      walk_tree (tp, mostly_copy_tree_r, NULL, NULL);
      *walk_subtrees = 0;
    }

  /* Otherwise, mark the tree as visited and keep looking.  */
  else
    /* APPLE LOCAL begin PR 14498 etc. --dbj */
    /* History is complicated, this was in mainline prior to merge,
       temporarily and erroneously removed at merge snapshot,
       later put back, still later replaced by different mechanism. */
    {
      TREE_VISITED (t) = 1;
      if (TREE_CODE (*tp) == VA_ARG_EXPR)
	{
	  /* Mark any _DECL inside the operand as volatile to avoid
	     the optimizers messing around with it. We have to do this
	     early, otherwise we might mark a variable as volatile
	     after we gimplify other statements that use the variable
	     assuming it's not volatile.  */
	  walk_tree (&TREE_OPERAND (*tp, 0), mark_decls_volatile_r,
		     NULL, NULL);
	}
    }
    /* APPLE LOCAL end PR 14498 etc. */

  return NULL_TREE;
}

static tree
unmark_visited_r (tree *tp, int *walk_subtrees ATTRIBUTE_UNUSED,
		  void *data ATTRIBUTE_UNUSED)
{
  if (TREE_VISITED (*tp))
    TREE_VISITED (*tp) = 0;
  else
    *walk_subtrees = 0;

  return NULL_TREE;
}

/* Unshare all the trees in BODY_P, a pointer into the body of FNDECL, and the
   bodies of any nested functions if we are unsharing the entire body of
   FNDECL.  */

static void
unshare_body (tree *body_p, tree fndecl)
{
  struct cgraph_node *cgn = cgraph_node (fndecl);

  walk_tree (body_p, copy_if_shared_r, NULL, NULL);
  if (body_p == &DECL_SAVED_TREE (fndecl))
    for (cgn = cgn->nested; cgn; cgn = cgn->next_nested)
      unshare_body (&DECL_SAVED_TREE (cgn->decl), cgn->decl);
}

/* Likewise, but mark all trees as not visited.  */

static void
unvisit_body (tree *body_p, tree fndecl)
{
  struct cgraph_node *cgn = cgraph_node (fndecl);

  walk_tree (body_p, unmark_visited_r, NULL, NULL);
  if (body_p == &DECL_SAVED_TREE (fndecl))
    for (cgn = cgn->nested; cgn; cgn = cgn->next_nested)
      unvisit_body (&DECL_SAVED_TREE (cgn->decl), cgn->decl);
}

/* Unshare T and all the trees reached from T via TREE_CHAIN.  */

void
unshare_all_trees (tree t)
{
  walk_tree (&t, copy_if_shared_r, NULL, NULL);
  walk_tree (&t, unmark_visited_r, NULL, NULL);
}

/* Unconditionally make an unshared copy of EXPR.  This is used when using
   stored expressions which span multiple functions, such as BINFO_VTABLE,
   as the normal unsharing process can't tell that they're shared.  */

tree
unshare_expr (tree expr)
{
  walk_tree (&expr, mostly_copy_tree_r, NULL, NULL);
  return expr;
}

/* A terser interface for building a representation of a exception
   specification.  */

tree
gimple_build_eh_filter (tree body, tree allowed, tree failure)
{
  tree t;

  /* FIXME should the allowed types go in TREE_TYPE?  */
  t = build (EH_FILTER_EXPR, void_type_node, allowed, NULL_TREE);
  append_to_statement_list (failure, &EH_FILTER_FAILURE (t));

  t = build (TRY_CATCH_EXPR, void_type_node, NULL_TREE, t);
  append_to_statement_list (body, &TREE_OPERAND (t, 0));

  return t;
}


/* WRAPPER is a code such as BIND_EXPR or CLEANUP_POINT_EXPR which can both
   contain statements and have a value.  Assign its value to a temporary
   and give it void_type_node.  Returns the temporary, or NULL_TREE if
   WRAPPER was already void.  */

tree
voidify_wrapper_expr (tree wrapper, tree temp)
{
  if (!VOID_TYPE_P (TREE_TYPE (wrapper)))
    {
      tree *p, sub = wrapper;

    restart:
      /* Set p to point to the body of the wrapper.  */
      switch (TREE_CODE (sub))
	{
	case BIND_EXPR:
	  /* For a BIND_EXPR, the body is operand 1.  */
	  p = &BIND_EXPR_BODY (sub);
	  break;

	default:
	  p = &TREE_OPERAND (sub, 0);
	  break;
	}

      /* Advance to the last statement.  Set all container types to void.  */
      if (TREE_CODE (*p) == STATEMENT_LIST)
	{
	  tree_stmt_iterator i = tsi_last (*p);
	  p = tsi_end_p (i) ? NULL : tsi_stmt_ptr (i);
	}
      else
	{
	  for (; TREE_CODE (*p) == COMPOUND_EXPR; p = &TREE_OPERAND (*p, 1))
	    {
	      TREE_SIDE_EFFECTS (*p) = 1;
	      TREE_TYPE (*p) = void_type_node;
	    }
	}

      if (p == NULL || IS_EMPTY_STMT (*p))
	;
      /* Look through exception handling.  */
      else if (TREE_CODE (*p) == TRY_FINALLY_EXPR
	       || TREE_CODE (*p) == TRY_CATCH_EXPR)
	{
	  sub = *p;
	  goto restart;
	}
      /* The C++ frontend already did this for us.  */
      else if (TREE_CODE (*p) == INIT_EXPR
	       || TREE_CODE (*p) == TARGET_EXPR)
	temp = TREE_OPERAND (*p, 0);
      /* If we're returning a dereference, move the dereference
	 outside the wrapper.  */
      else if (TREE_CODE (*p) == INDIRECT_REF)
	{
	  tree ptr = TREE_OPERAND (*p, 0);
	  temp = create_tmp_var (TREE_TYPE (ptr), "retval");
	  *p = build (MODIFY_EXPR, TREE_TYPE (ptr), temp, ptr);
	  temp = build1 (INDIRECT_REF, TREE_TYPE (TREE_TYPE (temp)), temp);
	  /* If this is a BIND_EXPR for a const inline function, it might not
	     have TREE_SIDE_EFFECTS set.  That is no longer accurate.  */
	  TREE_SIDE_EFFECTS (wrapper) = 1;
	}
      else
	{
	  if (!temp)
	    temp = create_tmp_var (TREE_TYPE (wrapper), "retval");
	  *p = build (MODIFY_EXPR, TREE_TYPE (temp), temp, *p);
	  TREE_SIDE_EFFECTS (wrapper) = 1;
	}

      TREE_TYPE (wrapper) = void_type_node;
      return temp;
    }

  return NULL_TREE;
}

/* Prepare calls to builtins to SAVE and RESTORE the stack as well as
   a temporary through which they communicate.  */

static void
build_stack_save_restore (tree *save, tree *restore)
{
  tree save_call, tmp_var;

  save_call =
      build_function_call_expr (implicit_built_in_decls[BUILT_IN_STACK_SAVE],
				NULL_TREE);
  tmp_var = create_tmp_var (ptr_type_node, "saved_stack");

  *save = build (MODIFY_EXPR, ptr_type_node, tmp_var, save_call);
  *restore =
    build_function_call_expr (implicit_built_in_decls[BUILT_IN_STACK_RESTORE],
			      tree_cons (NULL_TREE, tmp_var, NULL_TREE));
}

/* Gimplify a BIND_EXPR.  Just voidify and recurse.  */

static enum gimplify_status
gimplify_bind_expr (tree *expr_p, tree temp, tree *pre_p)
{
  tree bind_expr = *expr_p;
  bool old_save_stack = gimplify_ctxp->save_stack;
  tree t;

  temp = voidify_wrapper_expr (bind_expr, temp);

  /* Mark variables seen in this bind expr.  */
  for (t = BIND_EXPR_VARS (bind_expr); t ; t = TREE_CHAIN (t))
    DECL_SEEN_IN_BIND_EXPR_P (t) = 1;

  gimple_push_bind_expr (bind_expr);
  gimplify_ctxp->save_stack = false;

  gimplify_to_stmt_list (&BIND_EXPR_BODY (bind_expr));

  if (gimplify_ctxp->save_stack)
    {
      tree stack_save, stack_restore;

      /* Save stack on entry and restore it on exit.  Add a try_finally
	 block to achieve this.  Note that mudflap depends on the
	 format of the emitted code: see mx_register_decls().  */
      build_stack_save_restore (&stack_save, &stack_restore);

      t = build (TRY_FINALLY_EXPR, void_type_node,
		 BIND_EXPR_BODY (bind_expr), NULL_TREE);
      append_to_statement_list (stack_restore, &TREE_OPERAND (t, 1));

      BIND_EXPR_BODY (bind_expr) = NULL_TREE;
      append_to_statement_list (stack_save, &BIND_EXPR_BODY (bind_expr));
      append_to_statement_list (t, &BIND_EXPR_BODY (bind_expr));
    }

  gimplify_ctxp->save_stack = old_save_stack;
  gimple_pop_bind_expr ();

  if (temp)
    {
      *expr_p = temp;
      append_to_statement_list (bind_expr, pre_p);
      return GS_OK;
    }
  else
    return GS_ALL_DONE;
}

/* Gimplify a RETURN_EXPR.  If the expression to be returned is not a
   GIMPLE value, it is assigned to a new temporary and the statement is
   re-written to return the temporary.

   PRE_P points to the list where side effects that must happen before
   STMT should be stored.  */

static enum gimplify_status
gimplify_return_expr (tree stmt, tree *pre_p)
{
  tree ret_expr = TREE_OPERAND (stmt, 0);
  tree result_decl, result;

  if (!ret_expr || TREE_CODE (ret_expr) == RESULT_DECL
      || ret_expr == error_mark_node)
    return GS_ALL_DONE;

  if (VOID_TYPE_P (TREE_TYPE (TREE_TYPE (current_function_decl))))
    result_decl = NULL_TREE;
  else
    {
      result_decl = TREE_OPERAND (ret_expr, 0);
      if (TREE_CODE (result_decl) == INDIRECT_REF)
	/* See through a return by reference.  */
	result_decl = TREE_OPERAND (result_decl, 0);

      gcc_assert ((TREE_CODE (ret_expr) == MODIFY_EXPR
		   || TREE_CODE (ret_expr) == INIT_EXPR)
		  && TREE_CODE (result_decl) == RESULT_DECL);
    }

  /* If aggregate_value_p is true, then we can return the bare RESULT_DECL.
     Recall that aggregate_value_p is FALSE for any aggregate type that is
     returned in registers.  If we're returning values in registers, then
     we don't want to extend the lifetime of the RESULT_DECL, particularly
     across another call.  In addition, for those aggregates for which
     hard_function_value generates a PARALLEL, we'll abort during normal
     expansion of structure assignments; there's special code in expand_return
     to handle this case that does not exist in expand_expr.  */
  if (!result_decl
      || aggregate_value_p (result_decl, TREE_TYPE (current_function_decl)))
    result = result_decl;
  else if (gimplify_ctxp->return_temp)
    result = gimplify_ctxp->return_temp;
  else
    {
      result = create_tmp_var (TREE_TYPE (result_decl), NULL);

      /* ??? With complex control flow (usually involving abnormal edges),
	 we can wind up warning about an uninitialized value for this.  Due
	 to how this variable is constructed and initialized, this is never
	 true.  Give up and never warn.  */
      TREE_NO_WARNING (result) = 1;

      gimplify_ctxp->return_temp = result;
    }

  /* Smash the lhs of the MODIFY_EXPR to the temporary we plan to use.
     Then gimplify the whole thing.  */
  if (result != result_decl)
    TREE_OPERAND (ret_expr, 0) = result;

  gimplify_and_add (TREE_OPERAND (stmt, 0), pre_p);

  /* If we didn't use a temporary, then the result is just the result_decl.
     Otherwise we need a simple copy.  This should already be gimple.  */
  if (result == result_decl)
    ret_expr = result;
  else
    ret_expr = build (MODIFY_EXPR, TREE_TYPE (result), result_decl, result);
  TREE_OPERAND (stmt, 0) = ret_expr;

  return GS_ALL_DONE;
}

/* Gimplifies a DECL_EXPR node *STMT_P by making any necessary allocation
   and initialization explicit.  */

static enum gimplify_status
gimplify_decl_expr (tree *stmt_p)
{
  tree stmt = *stmt_p;
  tree decl = DECL_EXPR_DECL (stmt);

  *stmt_p = NULL_TREE;

  if (TREE_TYPE (decl) == error_mark_node)
    return GS_ERROR;

  else if (TREE_CODE (decl) == TYPE_DECL)
    gimplify_type_sizes (TREE_TYPE (decl), stmt_p);

  else if (TREE_CODE (decl) == VAR_DECL && !DECL_EXTERNAL (decl))
    {
      tree init = DECL_INITIAL (decl);

      if (!TREE_CONSTANT (DECL_SIZE (decl)))
	{
	  /* This is a variable-sized decl.  Simplify its size and mark it
	     for deferred expansion.  Note that mudflap depends on the format
	     of the emitted code: see mx_register_decls().  */
	  tree t, args, addr, ptr_type;

	  gimplify_type_sizes (TREE_TYPE (decl), stmt_p);
	  gimplify_one_sizepos (&DECL_SIZE (decl), stmt_p);
	  gimplify_one_sizepos (&DECL_SIZE_UNIT (decl), stmt_p);

	  /* All occurrences of this decl in final gimplified code will be
	     replaced by indirection.  Setting DECL_VALUE_EXPR does two
	     things: First, it lets the rest of the gimplifier know what
	     replacement to use.  Second, it lets the debug info know
	     where to find the value.  */
	  ptr_type = build_pointer_type (TREE_TYPE (decl));
	  addr = create_tmp_var (ptr_type, get_name (decl));
	  DECL_IGNORED_P (addr) = 0;
	  t = build_fold_indirect_ref (addr);
	  DECL_VALUE_EXPR (decl) = t;

	  args = tree_cons (NULL, DECL_SIZE_UNIT (decl), NULL);
	  t = built_in_decls[BUILT_IN_ALLOCA];
	  t = build_function_call_expr (t, args);
	  t = fold_convert (ptr_type, t);
	  t = build2 (MODIFY_EXPR, void_type_node, addr, t);

	  gimplify_and_add (t, stmt_p);

	  /* Indicate that we need to restore the stack level when the
	     enclosing BIND_EXPR is exited.  */
	  gimplify_ctxp->save_stack = true;
	}

      if (init && init != error_mark_node)
	{
	  if (!TREE_STATIC (decl))
	    {
	      DECL_INITIAL (decl) = NULL_TREE;
	      init = build (MODIFY_EXPR, void_type_node, decl, init);
	      gimplify_and_add (init, stmt_p);
	    }
	  else
	    /* We must still examine initializers for static variables
	       as they may contain a label address.  */
	    walk_tree (&init, force_labels_r, NULL, NULL);
	}

      /* This decl isn't mentioned in the enclosing block, so add it to the
	 list of temps.  FIXME it seems a bit of a kludge to say that
	 anonymous artificial vars aren't pushed, but everything else is.  */
      if (DECL_ARTIFICIAL (decl) && DECL_NAME (decl) == NULL_TREE)
	gimple_add_tmp_var (decl);
    }

  return GS_ALL_DONE;
}

/* Gimplify a LOOP_EXPR.  Normally this just involves gimplifying the body
   and replacing the LOOP_EXPR with goto, but if the loop contains an
   EXIT_EXPR, we need to append a label for it to jump to.  */

static enum gimplify_status
gimplify_loop_expr (tree *expr_p, tree *pre_p)
{
  tree saved_label = gimplify_ctxp->exit_label;
  tree start_label = build1 (LABEL_EXPR, void_type_node, NULL_TREE);
  tree jump_stmt = build_and_jump (&LABEL_EXPR_LABEL (start_label));

  append_to_statement_list (start_label, pre_p);

  gimplify_ctxp->exit_label = NULL_TREE;

  gimplify_and_add (LOOP_EXPR_BODY (*expr_p), pre_p);

  if (gimplify_ctxp->exit_label)
    {
      append_to_statement_list (jump_stmt, pre_p);
      *expr_p = build1 (LABEL_EXPR, void_type_node, gimplify_ctxp->exit_label);
    }
  else
    *expr_p = jump_stmt;

  gimplify_ctxp->exit_label = saved_label;

  return GS_ALL_DONE;
}

/* Compare two case labels.  Because the front end should already have
   made sure that case ranges do not overlap, it is enough to only compare
   the CASE_LOW values of each case label.  */

static int
compare_case_labels (const void *p1, const void *p2)
{
  tree case1 = *(tree *)p1;
  tree case2 = *(tree *)p2;

  return tree_int_cst_compare (CASE_LOW (case1), CASE_LOW (case2));
}

/* Sort the case labels in LABEL_VEC in place in ascending order.  */

void
sort_case_labels (tree label_vec)
{
  size_t len = TREE_VEC_LENGTH (label_vec);
  tree default_case = TREE_VEC_ELT (label_vec, len - 1);

  if (CASE_LOW (default_case))
    {
      size_t i;

      /* The last label in the vector should be the default case
         but it is not.  */
      for (i = 0; i < len; ++i)
	{
	  tree t = TREE_VEC_ELT (label_vec, i);
	  if (!CASE_LOW (t))
	    {
	      default_case = t;
	      TREE_VEC_ELT (label_vec, i) = TREE_VEC_ELT (label_vec, len - 1);
	      TREE_VEC_ELT (label_vec, len - 1) = default_case;
	      break;
	    }
	}
    }

  qsort (&TREE_VEC_ELT (label_vec, 0), len - 1, sizeof (tree),
	 compare_case_labels);
}

/* Gimplify a SWITCH_EXPR, and collect a TREE_VEC of the labels it can
   branch to.  */

static enum gimplify_status
gimplify_switch_expr (tree *expr_p, tree *pre_p)
{
  tree switch_expr = *expr_p;
  enum gimplify_status ret;

  ret = gimplify_expr (&SWITCH_COND (switch_expr), pre_p, NULL,
		       is_gimple_val, fb_rvalue);

  if (SWITCH_BODY (switch_expr))
    {
      varray_type labels, saved_labels;
      tree label_vec, default_case = NULL_TREE;
      size_t i, len;

      /* If someone can be bothered to fill in the labels, they can
	 be bothered to null out the body too.  */
      gcc_assert (!SWITCH_LABELS (switch_expr));

      saved_labels = gimplify_ctxp->case_labels;
      VARRAY_TREE_INIT (gimplify_ctxp->case_labels, 8, "case_labels");

      gimplify_to_stmt_list (&SWITCH_BODY (switch_expr));

      labels = gimplify_ctxp->case_labels;
      gimplify_ctxp->case_labels = saved_labels;

      len = VARRAY_ACTIVE_SIZE (labels);

      for (i = 0; i < len; ++i)
	{
	  tree t = VARRAY_TREE (labels, i);
	  if (!CASE_LOW (t))
	    {
	      /* The default case must be the last label in the list.  */
	      default_case = t;
	      VARRAY_TREE (labels, i) = VARRAY_TREE (labels, len - 1);
	      len--;
	      break;
	    }
	}

      label_vec = make_tree_vec (len + 1);
      SWITCH_LABELS (*expr_p) = label_vec;
      append_to_statement_list (switch_expr, pre_p);

      if (! default_case)
	{
	  /* If the switch has no default label, add one, so that we jump
	     around the switch body.  */
	  default_case = build (CASE_LABEL_EXPR, void_type_node, NULL_TREE,
				NULL_TREE, create_artificial_label ());
	  append_to_statement_list (SWITCH_BODY (switch_expr), pre_p);
	  *expr_p = build (LABEL_EXPR, void_type_node,
			   CASE_LABEL (default_case));
	}
      else
	*expr_p = SWITCH_BODY (switch_expr);

      for (i = 0; i < len; ++i)
	TREE_VEC_ELT (label_vec, i) = VARRAY_TREE (labels, i);
      TREE_VEC_ELT (label_vec, len) = default_case;

      sort_case_labels (label_vec);

      SWITCH_BODY (switch_expr) = NULL;
    }
  else
    gcc_assert (SWITCH_LABELS (switch_expr));

  return ret;
}

static enum gimplify_status
gimplify_case_label_expr (tree *expr_p)
{
  tree expr = *expr_p;

  gcc_assert (gimplify_ctxp->case_labels);
  VARRAY_PUSH_TREE (gimplify_ctxp->case_labels, expr);
  *expr_p = build (LABEL_EXPR, void_type_node, CASE_LABEL (expr));
  return GS_ALL_DONE;
}

/* Gimplify a LABELED_BLOCK_EXPR into a LABEL_EXPR following
   a (possibly empty) body.  */

static enum gimplify_status
gimplify_labeled_block_expr (tree *expr_p)
{
  tree body = LABELED_BLOCK_BODY (*expr_p);
  tree label = LABELED_BLOCK_LABEL (*expr_p);
  tree t;

  DECL_CONTEXT (label) = current_function_decl;
  t = build (LABEL_EXPR, void_type_node, label);
  if (body != NULL_TREE)
    t = build (COMPOUND_EXPR, void_type_node, body, t);
  *expr_p = t;

  return GS_OK;
}

/* Gimplify a EXIT_BLOCK_EXPR into a GOTO_EXPR.  */

static enum gimplify_status
gimplify_exit_block_expr (tree *expr_p)
{
  tree labeled_block = TREE_OPERAND (*expr_p, 0);
  tree label;

  /* First operand must be a LABELED_BLOCK_EXPR, which should
     already be lowered (or partially lowered) when we get here.  */
  gcc_assert (TREE_CODE (labeled_block) == LABELED_BLOCK_EXPR);

  label = LABELED_BLOCK_LABEL (labeled_block);
  *expr_p = build1 (GOTO_EXPR, void_type_node, label);

  return GS_OK;
}

/* Build a GOTO to the LABEL_DECL pointed to by LABEL_P, building it first
   if necessary.  */

tree
build_and_jump (tree *label_p)
{
  if (label_p == NULL)
    /* If there's nowhere to jump, just fall through.  */
    return NULL_TREE;

  if (*label_p == NULL_TREE)
    {
      tree label = create_artificial_label ();
      *label_p = label;
    }

  return build1 (GOTO_EXPR, void_type_node, *label_p);
}

/* Gimplify an EXIT_EXPR by converting to a GOTO_EXPR inside a COND_EXPR.
   This also involves building a label to jump to and communicating it to
   gimplify_loop_expr through gimplify_ctxp->exit_label.  */

static enum gimplify_status
gimplify_exit_expr (tree *expr_p)
{
  tree cond = TREE_OPERAND (*expr_p, 0);
  tree expr;

  expr = build_and_jump (&gimplify_ctxp->exit_label);
  expr = build (COND_EXPR, void_type_node, cond, expr, NULL_TREE);
  *expr_p = expr;

  return GS_OK;
}

/* A helper function to be called via walk_tree.  Mark all labels under *TP
   as being forced.  To be called for DECL_INITIAL of static variables.  */

tree
force_labels_r (tree *tp, int *walk_subtrees, void *data ATTRIBUTE_UNUSED)
{
  if (TYPE_P (*tp))
    *walk_subtrees = 0;
  if (TREE_CODE (*tp) == LABEL_DECL)
    FORCED_LABEL (*tp) = 1;

  return NULL_TREE;
}

/* *EXPR_P is a COMPONENT_REF being used as an rvalue.  If its type is
   different from its canonical type, wrap the whole thing inside a
   NOP_EXPR and force the type of the COMPONENT_REF to be the canonical
   type.

   The canonical type of a COMPONENT_REF is the type of the field being
   referenced--unless the field is a bit-field which can be read directly
   in a smaller mode, in which case the canonical type is the
   sign-appropriate type corresponding to that mode.  */

static void
canonicalize_component_ref (tree *expr_p)
{
  tree expr = *expr_p;
  tree type;

  gcc_assert (TREE_CODE (expr) == COMPONENT_REF);

  if (INTEGRAL_TYPE_P (TREE_TYPE (expr)))
    type = TREE_TYPE (get_unwidened (expr, NULL_TREE));
  else
    type = TREE_TYPE (TREE_OPERAND (expr, 1));

  if (TREE_TYPE (expr) != type)
    {
      tree old_type = TREE_TYPE (expr);

      /* Set the type of the COMPONENT_REF to the underlying type.  */
      TREE_TYPE (expr) = type;

      /* And wrap the whole thing inside a NOP_EXPR.  */
      expr = build1 (NOP_EXPR, old_type, expr);

      *expr_p = expr;
    }
}

/* If a NOP conversion is changing a pointer to array of foo to a pointer
   to foo, embed that change in the ADDR_EXPR by converting
      T array[U];
      (T *)&array
   ==>
      &array[L]
   where L is the lower bound.  For simplicity, only do this for constant
   lower bound.  */

static void
canonicalize_addr_expr (tree *expr_p)
{
  tree expr = *expr_p;
  tree ctype = TREE_TYPE (expr);
  tree addr_expr = TREE_OPERAND (expr, 0);
  tree atype = TREE_TYPE (addr_expr);
  tree dctype, datype, ddatype, otype, obj_expr;

  /* Both cast and addr_expr types should be pointers.  */
  if (!POINTER_TYPE_P (ctype) || !POINTER_TYPE_P (atype))
    return;

  /* The addr_expr type should be a pointer to an array.  */
  datype = TREE_TYPE (atype);
  if (TREE_CODE (datype) != ARRAY_TYPE)
    return;

  /* Both cast and addr_expr types should address the same object type.  */
  dctype = TREE_TYPE (ctype);
  ddatype = TREE_TYPE (datype);
  if (!lang_hooks.types_compatible_p (ddatype, dctype))
    return;

  /* The addr_expr and the object type should match.  */
  obj_expr = TREE_OPERAND (addr_expr, 0);
  otype = TREE_TYPE (obj_expr);
  if (!lang_hooks.types_compatible_p (otype, datype))
    return;

  /* The lower bound and element sizes must be constant.  */
  if (TREE_CODE (TYPE_SIZE_UNIT (dctype)) != INTEGER_CST
      || !TYPE_DOMAIN (datype) || !TYPE_MIN_VALUE (TYPE_DOMAIN (datype))
      || TREE_CODE (TYPE_MIN_VALUE (TYPE_DOMAIN (datype))) != INTEGER_CST)
    return;

  /* All checks succeeded.  Build a new node to merge the cast.  */
  *expr_p = build4 (ARRAY_REF, dctype, obj_expr,
		    TYPE_MIN_VALUE (TYPE_DOMAIN (datype)),
		    TYPE_MIN_VALUE (TYPE_DOMAIN (datype)),
		    size_binop (EXACT_DIV_EXPR, TYPE_SIZE_UNIT (dctype),
				size_int (TYPE_ALIGN_UNIT (dctype))));
  *expr_p = build1 (ADDR_EXPR, ctype, *expr_p);
}

/* *EXPR_P is a NOP_EXPR or CONVERT_EXPR.  Remove it and/or other conversions
   underneath as appropriate.  */

static enum gimplify_status
gimplify_conversion (tree *expr_p)
{
  /* If we still have a conversion at the toplevel, then strip
     away all but the outermost conversion.  */
  if (TREE_CODE (*expr_p) == NOP_EXPR || TREE_CODE (*expr_p) == CONVERT_EXPR)
    {
      STRIP_SIGN_NOPS (TREE_OPERAND (*expr_p, 0));

      /* And remove the outermost conversion if it's useless.  */
      if (tree_ssa_useless_type_conversion (*expr_p))
	*expr_p = TREE_OPERAND (*expr_p, 0);
    }

  /* If we still have a conversion at the toplevel,
     then canonicalize some constructs.  */
  if (TREE_CODE (*expr_p) == NOP_EXPR || TREE_CODE (*expr_p) == CONVERT_EXPR)
    {
      tree sub = TREE_OPERAND (*expr_p, 0);

      /* If a NOP conversion is changing the type of a COMPONENT_REF
	 expression, then canonicalize its type now in order to expose more
	 redundant conversions.  */
      if (TREE_CODE (sub) == COMPONENT_REF)
	canonicalize_component_ref (&TREE_OPERAND (*expr_p, 0));

      /* If a NOP conversion is changing a pointer to array of foo
	 to a pointer to foo, embed that change in the ADDR_EXPR.  */
      else if (TREE_CODE (sub) == ADDR_EXPR)
	canonicalize_addr_expr (expr_p);
    }

  return GS_OK;
}

/* APPLE LOCAL mainline remove gimplify_array_ref_to_plus -- apinski */

/* Gimplify the COMPONENT_REF, ARRAY_REF, REALPART_EXPR or IMAGPART_EXPR
   node pointed by EXPR_P.

      compound_lval
	      : min_lval '[' val ']'
	      | min_lval '.' ID
	      | compound_lval '[' val ']'
	      | compound_lval '.' ID

   This is not part of the original SIMPLE definition, which separates
   array and member references, but it seems reasonable to handle them
   together.  Also, this way we don't run into problems with union
   aliasing; gcc requires that for accesses through a union to alias, the
   union reference must be explicit, which was not always the case when we
   were splitting up array and member refs.

   PRE_P points to the list where side effects that must happen before
     *EXPR_P should be stored.

   POST_P points to the list where side effects that must happen after
     *EXPR_P should be stored.  */

static enum gimplify_status
gimplify_compound_lval (tree *expr_p, tree *pre_p,
			tree *post_p, fallback_t fallback)
{
  tree *p;
  varray_type stack;
  enum gimplify_status ret = GS_OK, tret;
  int i;

  /* Create a stack of the subexpressions so later we can walk them in
     order from inner to outer.

     This array is very memory consuming.  Don't even think of making
     it VARRAY_TREE.  */
  VARRAY_GENERIC_PTR_NOGC_INIT (stack, 10, "stack");

  /* We can either handle REALPART_EXPR, IMAGEPART_EXPR anything that
     handled_components can deal with.  */
  for (p = expr_p;
       (handled_component_p (*p)
	|| TREE_CODE (*p) == REALPART_EXPR || TREE_CODE (*p) == IMAGPART_EXPR);
       p = &TREE_OPERAND (*p, 0))
    VARRAY_PUSH_GENERIC_PTR_NOGC (stack, *p);

  gcc_assert (VARRAY_ACTIVE_SIZE (stack));

  /* Now STACK is a stack of pointers to all the refs we've walked through
     and P points to the innermost expression.

     Java requires that we elaborated nodes in source order.  That
     means we must gimplify the inner expression followed by each of
     the indices, in order.  But we can't gimplify the inner
     expression until we deal with any variable bounds, sizes, or
     positions in order to deal with PLACEHOLDER_EXPRs.

     So we do this in three steps.  First we deal with the annotations
     for any variables in the components, then we gimplify the base,
     then we gimplify any indices, from left to right.  */
  for (i = VARRAY_ACTIVE_SIZE (stack) - 1; i >= 0; i--)
    {
      tree t = VARRAY_GENERIC_PTR_NOGC (stack, i);

      if (TREE_CODE (t) == ARRAY_REF || TREE_CODE (t) == ARRAY_RANGE_REF)
	{
	  /* Gimplify the low bound and element type size and put them into
	     the ARRAY_REF.  If these values are set, they have already been
	     gimplified.  */
	  if (!TREE_OPERAND (t, 2))
	    {
	      tree low = unshare_expr (array_ref_low_bound (t));
	      if (!is_gimple_min_invariant (low))
		{
	          TREE_OPERAND (t, 2) = low;
		  tret = gimplify_expr (&TREE_OPERAND (t, 2), pre_p, post_p,
					is_gimple_formal_tmp_reg, fb_rvalue);
		  ret = MIN (ret, tret);
		}
	    }

	  if (!TREE_OPERAND (t, 3))
	    {
	      tree elmt_type = TREE_TYPE (TREE_TYPE (TREE_OPERAND (t, 0)));
	      tree elmt_size = unshare_expr (array_ref_element_size (t));
	      tree factor = size_int (TYPE_ALIGN_UNIT (elmt_type));

	      /* Divide the element size by the alignment of the element
		 type (above).  */
	      elmt_size = size_binop (EXACT_DIV_EXPR, elmt_size, factor);

	      if (!is_gimple_min_invariant (elmt_size))
		{
	          TREE_OPERAND (t, 3) = elmt_size;
		  tret = gimplify_expr (&TREE_OPERAND (t, 3), pre_p, post_p,
					is_gimple_formal_tmp_reg, fb_rvalue);
		  ret = MIN (ret, tret);
		}
	    }
	}
      else if (TREE_CODE (t) == COMPONENT_REF)
	{
	  /* Set the field offset into T and gimplify it.  */
	  if (!TREE_OPERAND (t, 2))
	    {
	      tree offset = unshare_expr (component_ref_field_offset (t));
	      tree field = TREE_OPERAND (t, 1);
	      tree factor
		= size_int (DECL_OFFSET_ALIGN (field) / BITS_PER_UNIT);

	      /* Divide the offset by its alignment.  */
	      offset = size_binop (EXACT_DIV_EXPR, offset, factor);

	      if (!is_gimple_min_invariant (offset))
		{
	          TREE_OPERAND (t, 2) = offset;
		  tret = gimplify_expr (&TREE_OPERAND (t, 2), pre_p, post_p,
					is_gimple_formal_tmp_reg, fb_rvalue);
		  ret = MIN (ret, tret);
		}
	    }
	}
    }

  /* Step 2 is to gimplify the base expression.  */
  tret = gimplify_expr (p, pre_p, post_p, is_gimple_min_lval, fallback);
  ret = MIN (ret, tret);

  /* And finally, the indices and operands to BIT_FIELD_REF.  During this
     loop we also remove any useless conversions.  */
  for (; VARRAY_ACTIVE_SIZE (stack) > 0; )
    {
      tree t = VARRAY_TOP_TREE (stack);

      if (TREE_CODE (t) == ARRAY_REF || TREE_CODE (t) == ARRAY_RANGE_REF)
	{
	  /* Gimplify the dimension.
	     Temporary fix for gcc.c-torture/execute/20040313-1.c.
	     Gimplify non-constant array indices into a temporary
	     variable.
	     FIXME - The real fix is to gimplify post-modify
	     expressions into a minimal gimple lvalue.  However, that
	     exposes bugs in alias analysis.  The alias analyzer does
	     not handle &PTR->FIELD very well.  Will fix after the
	     branch is merged into mainline (dnovillo 2004-05-03).  */
	  if (!is_gimple_min_invariant (TREE_OPERAND (t, 1)))
	    {
	      tret = gimplify_expr (&TREE_OPERAND (t, 1), pre_p, post_p,
				    is_gimple_formal_tmp_reg, fb_rvalue);
	      ret = MIN (ret, tret);
	    }
	}
      else if (TREE_CODE (t) == BIT_FIELD_REF)
	{
	  tret = gimplify_expr (&TREE_OPERAND (t, 1), pre_p, post_p,
				is_gimple_val, fb_rvalue);
	  ret = MIN (ret, tret);
	  tret = gimplify_expr (&TREE_OPERAND (t, 2), pre_p, post_p,
				is_gimple_val, fb_rvalue);
	  ret = MIN (ret, tret);
	}

      STRIP_USELESS_TYPE_CONVERSION (TREE_OPERAND (t, 0));

      /* The innermost expression P may have originally had TREE_SIDE_EFFECTS
	 set which would have caused all the outer expressions in EXPR_P
	 leading to P to also have had TREE_SIDE_EFFECTS set.  */
      recalculate_side_effects (t);
      VARRAY_POP (stack);
    }

  tret = gimplify_expr (p, pre_p, post_p, is_gimple_min_lval, fallback);
  ret = MIN (ret, tret);

  /* If the outermost expression is a COMPONENT_REF, canonicalize its type.  */
  if ((fallback & fb_rvalue) && TREE_CODE (*expr_p) == COMPONENT_REF)
    {
      canonicalize_component_ref (expr_p);
      ret = MIN (ret, GS_OK);
    }

  VARRAY_FREE (stack);

  return ret;
}

/*  Gimplify the self modifying expression pointed by EXPR_P (++, --, +=, -=).

    PRE_P points to the list where side effects that must happen before
	*EXPR_P should be stored.

    POST_P points to the list where side effects that must happen after
	*EXPR_P should be stored.

    WANT_VALUE is nonzero iff we want to use the value of this expression
	in another expression.  */

static enum gimplify_status
gimplify_self_mod_expr (tree *expr_p, tree *pre_p, tree *post_p,
			bool want_value)
{
  enum tree_code code;
  tree lhs, lvalue, rhs, t1;
  bool postfix;
  enum tree_code arith_code;
  enum gimplify_status ret;

  code = TREE_CODE (*expr_p);

  gcc_assert (code == POSTINCREMENT_EXPR || code == POSTDECREMENT_EXPR
	      || code == PREINCREMENT_EXPR || code == PREDECREMENT_EXPR);

  /* Prefix or postfix?  */
  if (code == POSTINCREMENT_EXPR || code == POSTDECREMENT_EXPR)
    /* Faster to treat as prefix if result is not used.  */
    postfix = want_value;
  else
    postfix = false;

  /* Add or subtract?  */
  if (code == PREINCREMENT_EXPR || code == POSTINCREMENT_EXPR)
    arith_code = PLUS_EXPR;
  else
    arith_code = MINUS_EXPR;

  /* Gimplify the LHS into a GIMPLE lvalue.  */
  lvalue = TREE_OPERAND (*expr_p, 0);
  ret = gimplify_expr (&lvalue, pre_p, post_p, is_gimple_lvalue, fb_lvalue);
  if (ret == GS_ERROR)
    return ret;

  /* Extract the operands to the arithmetic operation.  */
  lhs = lvalue;
  rhs = TREE_OPERAND (*expr_p, 1);

  /* For postfix operator, we evaluate the LHS to an rvalue and then use
     that as the result value and in the postqueue operation.  */
  if (postfix)
    {
      ret = gimplify_expr (&lhs, pre_p, post_p, is_gimple_val, fb_rvalue);
      if (ret == GS_ERROR)
	return ret;
    }

  t1 = build (arith_code, TREE_TYPE (*expr_p), lhs, rhs);
  t1 = build (MODIFY_EXPR, TREE_TYPE (lvalue), lvalue, t1);

  if (postfix)
    {
      gimplify_and_add (t1, post_p);
      *expr_p = lhs;
      return GS_ALL_DONE;
    }
  else
    {
      *expr_p = t1;
      return GS_OK;
    }
}

/* If *EXPR_P has a variable sized type, wrap it in a WITH_SIZE_EXPR.  */

static void
maybe_with_size_expr (tree *expr_p)
{
  tree expr = *expr_p;
  tree type = TREE_TYPE (expr);
  tree size;

  /* If we've already wrapped this or the type is error_mark_node, we can't do
     anything.  */
  if (TREE_CODE (expr) == WITH_SIZE_EXPR
      || type == error_mark_node)
    return;

  /* If the size isn't known or is a constant, we have nothing to do.  */
  size = TYPE_SIZE_UNIT (type);
  if (!size || TREE_CODE (size) == INTEGER_CST)
    return;

  /* Otherwise, make a WITH_SIZE_EXPR.  */
  size = unshare_expr (size);
  size = SUBSTITUTE_PLACEHOLDER_IN_EXPR (size, expr);
  *expr_p = build2 (WITH_SIZE_EXPR, type, expr, size);
}

/* Subroutine of gimplify_call_expr:  Gimplify a single argument.  */

static enum gimplify_status
gimplify_arg (tree *expr_p, tree *pre_p)
{
  bool (*test) (tree);
  fallback_t fb;

  /* In general, we allow lvalues for function arguments to avoid
     extra overhead of copying large aggregates out of even larger
     aggregates into temporaries only to copy the temporaries to
     the argument list.  Make optimizers happy by pulling out to
     temporaries those types that fit in registers.  */
  if (is_gimple_reg_type (TREE_TYPE (*expr_p)))
    test = is_gimple_val, fb = fb_rvalue;
  else
    test = is_gimple_lvalue, fb = fb_either;

  /* If this is a variable sized type, we must remember the size.  */
  maybe_with_size_expr (expr_p);

  /* There is a sequence point before a function call.  Side effects in
     the argument list must occur before the actual call. So, when
     gimplifying arguments, force gimplify_expr to use an internal
     post queue which is then appended to the end of PRE_P.  */
  return gimplify_expr (expr_p, pre_p, NULL, test, fb);
}

/* Gimplify the CALL_EXPR node pointed by EXPR_P.  PRE_P points to the
   list where side effects that must happen before *EXPR_P should be stored.
   WANT_VALUE is true if the result of the call is desired.  */

static enum gimplify_status
gimplify_call_expr (tree *expr_p, tree *pre_p, bool want_value)
{
  tree decl;
  tree arglist;
  enum gimplify_status ret;

  gcc_assert (TREE_CODE (*expr_p) == CALL_EXPR);

  /* For reliable diagnostics during inlining, it is necessary that
     every call_expr be annotated with file and line.  */
  if (! EXPR_HAS_LOCATION (*expr_p))
    SET_EXPR_LOCATION (*expr_p, input_location);

  /* This may be a call to a builtin function.

     Builtin function calls may be transformed into different
     (and more efficient) builtin function calls under certain
     circumstances.  Unfortunately, gimplification can muck things
     up enough that the builtin expanders are not aware that certain
     transformations are still valid.

     So we attempt transformation/gimplification of the call before
     we gimplify the CALL_EXPR.  At this time we do not manage to
     transform all calls in the same manner as the expanders do, but
     we do transform most of them.  */
  decl = get_callee_fndecl (*expr_p);
  if (decl && DECL_BUILT_IN (decl))
    {
      tree new = fold_builtin (*expr_p, !want_value);

      if (new && new != *expr_p)
	{
	  /* There was a transformation of this call which computes the
	     same value, but in a more efficient way.  Return and try
	     again.  */
	  *expr_p = new;
	  return GS_OK;
	}

      if (DECL_FUNCTION_CODE (decl) == BUILT_IN_VA_START)
	/* Avoid gimplifying the second argument to va_start, which needs
	   to be the plain PARM_DECL.  */
	return gimplify_arg (&TREE_VALUE (TREE_OPERAND (*expr_p, 1)), pre_p);
    }

  /* There is a sequence point before the call, so any side effects in
     the calling expression must occur before the actual call.  Force
     gimplify_expr to use an internal post queue.  */
  ret = gimplify_expr (&TREE_OPERAND (*expr_p, 0), pre_p, NULL,
		       is_gimple_call_addr, fb_rvalue);

  if (PUSH_ARGS_REVERSED)
    TREE_OPERAND (*expr_p, 1) = nreverse (TREE_OPERAND (*expr_p, 1));
  for (arglist = TREE_OPERAND (*expr_p, 1); arglist;
       arglist = TREE_CHAIN (arglist))
    {
      enum gimplify_status t;

      t = gimplify_arg (&TREE_VALUE (arglist), pre_p);

      if (t == GS_ERROR)
	ret = GS_ERROR;
    }
  if (PUSH_ARGS_REVERSED)
    TREE_OPERAND (*expr_p, 1) = nreverse (TREE_OPERAND (*expr_p, 1));

  /* Try this again in case gimplification exposed something.  */
  if (ret != GS_ERROR && decl && DECL_BUILT_IN (decl))
    {
      tree new = fold_builtin (*expr_p, !want_value);

      if (new && new != *expr_p)
	{
	  /* There was a transformation of this call which computes the
	     same value, but in a more efficient way.  Return and try
	     again.  */
	  *expr_p = new;
	  return GS_OK;
	}
    }

  /* If the function is "const" or "pure", then clear TREE_SIDE_EFFECTS on its
     decl.  This allows us to eliminate redundant or useless
     calls to "const" functions.  */
  if (TREE_CODE (*expr_p) == CALL_EXPR
      && (call_expr_flags (*expr_p) & (ECF_CONST | ECF_PURE)))
    TREE_SIDE_EFFECTS (*expr_p) = 0;

  return ret;
}

/* Handle shortcut semantics in the predicate operand of a COND_EXPR by
   rewriting it into multiple COND_EXPRs, and possibly GOTO_EXPRs.

   TRUE_LABEL_P and FALSE_LABEL_P point to the labels to jump to if the
   condition is true or false, respectively.  If null, we should generate
   our own to skip over the evaluation of this specific expression.

   This function is the tree equivalent of do_jump.

   shortcut_cond_r should only be called by shortcut_cond_expr.  */

static tree
shortcut_cond_r (tree pred, tree *true_label_p, tree *false_label_p)
{
  tree local_label = NULL_TREE;
  tree t, expr = NULL;

  /* OK, it's not a simple case; we need to pull apart the COND_EXPR to
     retain the shortcut semantics.  Just insert the gotos here;
     shortcut_cond_expr will append the real blocks later.  */
  if (TREE_CODE (pred) == TRUTH_ANDIF_EXPR)
    {
      /* Turn if (a && b) into

	 if (a); else goto no;
	 if (b) goto yes; else goto no;
	 (no:) */

      if (false_label_p == NULL)
	false_label_p = &local_label;

      t = shortcut_cond_r (TREE_OPERAND (pred, 0), NULL, false_label_p);
      append_to_statement_list (t, &expr);

      t = shortcut_cond_r (TREE_OPERAND (pred, 1), true_label_p,
			   false_label_p);
      append_to_statement_list (t, &expr);
    }
  else if (TREE_CODE (pred) == TRUTH_ORIF_EXPR)
    {
      /* Turn if (a || b) into

	 if (a) goto yes;
	 if (b) goto yes; else goto no;
	 (yes:) */

      if (true_label_p == NULL)
	true_label_p = &local_label;

      t = shortcut_cond_r (TREE_OPERAND (pred, 0), true_label_p, NULL);
      append_to_statement_list (t, &expr);

      t = shortcut_cond_r (TREE_OPERAND (pred, 1), true_label_p,
			   false_label_p);
      append_to_statement_list (t, &expr);
    }
  else if (TREE_CODE (pred) == COND_EXPR)
    {
      /* As long as we're messing with gotos, turn if (a ? b : c) into
	 if (a)
	   if (b) goto yes; else goto no;
	 else
	   if (c) goto yes; else goto no;  */
      expr = build (COND_EXPR, void_type_node, TREE_OPERAND (pred, 0),
		    shortcut_cond_r (TREE_OPERAND (pred, 1), true_label_p,
				     false_label_p),
		    shortcut_cond_r (TREE_OPERAND (pred, 2), true_label_p,
				     false_label_p));
    }
  else
    {
      expr = build (COND_EXPR, void_type_node, pred,
		    build_and_jump (true_label_p),
		    build_and_jump (false_label_p));
    }

  if (local_label)
    {
      t = build1 (LABEL_EXPR, void_type_node, local_label);
      append_to_statement_list (t, &expr);
    }

  return expr;
}

static tree
shortcut_cond_expr (tree expr)
{
  tree pred = TREE_OPERAND (expr, 0);
  tree then_ = TREE_OPERAND (expr, 1);
  tree else_ = TREE_OPERAND (expr, 2);
  tree true_label, false_label, end_label, t;
  tree *true_label_p;
  tree *false_label_p;
  bool emit_end, emit_false;
  bool then_se = then_ && TREE_SIDE_EFFECTS (then_);
  bool else_se = else_ && TREE_SIDE_EFFECTS (else_);

  /* First do simple transformations.  */
  if (!else_se)
    {
      /* If there is no 'else', turn (a && b) into if (a) if (b).  */
      while (TREE_CODE (pred) == TRUTH_ANDIF_EXPR)
	{
	  TREE_OPERAND (expr, 0) = TREE_OPERAND (pred, 1);
	  then_ = shortcut_cond_expr (expr);
	  pred = TREE_OPERAND (pred, 0);
	  expr = build (COND_EXPR, void_type_node, pred, then_, NULL_TREE);
	}
    }
  if (!then_se)
    {
      /* If there is no 'then', turn
	   if (a || b); else d
	 into
	   if (a); else if (b); else d.  */
      while (TREE_CODE (pred) == TRUTH_ORIF_EXPR)
	{
	  TREE_OPERAND (expr, 0) = TREE_OPERAND (pred, 1);
	  else_ = shortcut_cond_expr (expr);
	  pred = TREE_OPERAND (pred, 0);
	  expr = build (COND_EXPR, void_type_node, pred, NULL_TREE, else_);
	}
    }

  /* If we're done, great.  */
  if (TREE_CODE (pred) != TRUTH_ANDIF_EXPR
      && TREE_CODE (pred) != TRUTH_ORIF_EXPR)
    return expr;

  /* Otherwise we need to mess with gotos.  Change
       if (a) c; else d;
     to
       if (a); else goto no;
       c; goto end;
       no: d; end:
     and recursively gimplify the condition.  */

  true_label = false_label = end_label = NULL_TREE;

  /* If our arms just jump somewhere, hijack those labels so we don't
     generate jumps to jumps.  */

  if (then_
      && TREE_CODE (then_) == GOTO_EXPR
      && TREE_CODE (GOTO_DESTINATION (then_)) == LABEL_DECL)
    {
      true_label = GOTO_DESTINATION (then_);
      then_ = NULL;
      then_se = false;
    }

  if (else_
      && TREE_CODE (else_) == GOTO_EXPR
      && TREE_CODE (GOTO_DESTINATION (else_)) == LABEL_DECL)
    {
      false_label = GOTO_DESTINATION (else_);
      else_ = NULL;
      else_se = false;
    }

  /* If we aren't hijacking a label for the 'then' branch, it falls through.  */
  if (true_label)
    true_label_p = &true_label;
  else
    true_label_p = NULL;

  /* The 'else' branch also needs a label if it contains interesting code.  */
  if (false_label || else_se)
    false_label_p = &false_label;
  else
    false_label_p = NULL;

  /* If there was nothing else in our arms, just forward the label(s).  */
  if (!then_se && !else_se)
    return shortcut_cond_r (pred, true_label_p, false_label_p);

  /* If our last subexpression already has a terminal label, reuse it.  */
  if (else_se)
    expr = expr_last (else_);
  else if (then_se)
    expr = expr_last (then_);
  else
    expr = NULL;
  if (expr && TREE_CODE (expr) == LABEL_EXPR)
    end_label = LABEL_EXPR_LABEL (expr);

  /* If we don't care about jumping to the 'else' branch, jump to the end
     if the condition is false.  */
  if (!false_label_p)
    false_label_p = &end_label;

  /* We only want to emit these labels if we aren't hijacking them.  */
  emit_end = (end_label == NULL_TREE);
  emit_false = (false_label == NULL_TREE);

  pred = shortcut_cond_r (pred, true_label_p, false_label_p);

  expr = NULL;
  append_to_statement_list (pred, &expr);

  append_to_statement_list (then_, &expr);
  if (else_se)
    {
      t = build_and_jump (&end_label);
      append_to_statement_list (t, &expr);
      if (emit_false)
	{
	  t = build1 (LABEL_EXPR, void_type_node, false_label);
	  append_to_statement_list (t, &expr);
	}
      append_to_statement_list (else_, &expr);
    }
  if (emit_end && end_label)
    {
      t = build1 (LABEL_EXPR, void_type_node, end_label);
      append_to_statement_list (t, &expr);
    }

  return expr;
}

/* EXPR is used in a boolean context; make sure it has BOOLEAN_TYPE.  */

static tree
gimple_boolify (tree expr)
{
  tree type = TREE_TYPE (expr);

  if (TREE_CODE (type) == BOOLEAN_TYPE)
    return expr;

  /* If this is the predicate of a COND_EXPR, it might not even be a
     truthvalue yet.  */
  expr = lang_hooks.truthvalue_conversion (expr);

  switch (TREE_CODE (expr))
    {
    case TRUTH_AND_EXPR:
    case TRUTH_OR_EXPR:
    case TRUTH_XOR_EXPR:
    case TRUTH_ANDIF_EXPR:
    case TRUTH_ORIF_EXPR:
      /* Also boolify the arguments of truth exprs.  */
      TREE_OPERAND (expr, 1) = gimple_boolify (TREE_OPERAND (expr, 1));
      /* FALLTHRU */

    case TRUTH_NOT_EXPR:
      TREE_OPERAND (expr, 0) = gimple_boolify (TREE_OPERAND (expr, 0));
      /* FALLTHRU */

    case EQ_EXPR: case NE_EXPR:
    case LE_EXPR: case GE_EXPR: case LT_EXPR: case GT_EXPR:
      /* These expressions always produce boolean results.  */
      TREE_TYPE (expr) = boolean_type_node;
      return expr;

    default:
      /* Other expressions that get here must have boolean values, but
	 might need to be converted to the appropriate mode.  */
      return convert (boolean_type_node, expr);
    }
}

/*  Convert the conditional expression pointed by EXPR_P '(p) ? a : b;'
    into

    if (p)			if (p)
      t1 = a;			  a;
    else		or	else
      t1 = b;			  b;
    t1;

    The second form is used when *EXPR_P is of type void.

    TARGET is the tree for T1 above.

    PRE_P points to the list where side effects that must happen before
	*EXPR_P should be stored.  */

static enum gimplify_status
gimplify_cond_expr (tree *expr_p, tree *pre_p, tree target)
{
  tree expr = *expr_p;
  tree tmp, tmp2, type;
  enum gimplify_status ret;

  type = TREE_TYPE (expr);
  if (!type)
    TREE_TYPE (expr) = void_type_node;

  /* If this COND_EXPR has a value, copy the values into a temporary within
     the arms.  */
  else if (! VOID_TYPE_P (type))
    {
      if (target)
	{
	  ret = gimplify_expr (&target, pre_p, NULL,
			       is_gimple_min_lval, fb_lvalue);
	  if (ret != GS_ERROR)
	    ret = GS_OK;
	  tmp = target;
	  tmp2 = unshare_expr (target);
	}
      else
	{
	  tmp2 = tmp = create_tmp_var (TREE_TYPE (expr), "iftmp");
	  ret = GS_ALL_DONE;
	}

      /* Build the then clause, 't1 = a;'.  But don't build an assignment
	 if this branch is void; in C++ it can be, if it's a throw.  */
      if (TREE_TYPE (TREE_OPERAND (expr, 1)) != void_type_node)
	TREE_OPERAND (expr, 1)
	  = build (MODIFY_EXPR, void_type_node, tmp, TREE_OPERAND (expr, 1));

      /* Build the else clause, 't1 = b;'.  */
      if (TREE_TYPE (TREE_OPERAND (expr, 2)) != void_type_node)
	TREE_OPERAND (expr, 2)
	  = build (MODIFY_EXPR, void_type_node, tmp2, TREE_OPERAND (expr, 2));

      TREE_TYPE (expr) = void_type_node;
      recalculate_side_effects (expr);

      /* Move the COND_EXPR to the prequeue.  */
      gimplify_and_add (expr, pre_p);

      *expr_p = tmp;
      return ret;
    }

  /* Make sure the condition has BOOLEAN_TYPE.  */
  TREE_OPERAND (expr, 0) = gimple_boolify (TREE_OPERAND (expr, 0));

  /* Break apart && and || conditions.  */
  if (TREE_CODE (TREE_OPERAND (expr, 0)) == TRUTH_ANDIF_EXPR
      || TREE_CODE (TREE_OPERAND (expr, 0)) == TRUTH_ORIF_EXPR)
    {
      expr = shortcut_cond_expr (expr);

      if (expr != *expr_p)
	{
	  *expr_p = expr;

	  /* We can't rely on gimplify_expr to re-gimplify the expanded
	     form properly, as cleanups might cause the target labels to be
	     wrapped in a TRY_FINALLY_EXPR.  To prevent that, we need to
	     set up a conditional context.  */
	  gimple_push_condition ();
	  gimplify_stmt (expr_p);
	  gimple_pop_condition (pre_p);

	  return GS_ALL_DONE;
	}
    }

  /* Now do the normal gimplification.  */
  ret = gimplify_expr (&TREE_OPERAND (expr, 0), pre_p, NULL,
		       is_gimple_condexpr, fb_rvalue);

  gimple_push_condition ();

  gimplify_to_stmt_list (&TREE_OPERAND (expr, 1));
  gimplify_to_stmt_list (&TREE_OPERAND (expr, 2));
  recalculate_side_effects (expr);

  gimple_pop_condition (pre_p);

  if (ret == GS_ERROR)
    ;
  else if (TREE_SIDE_EFFECTS (TREE_OPERAND (expr, 1)))
    ret = GS_ALL_DONE;
  else if (TREE_SIDE_EFFECTS (TREE_OPERAND (expr, 2)))
    /* Rewrite "if (a); else b" to "if (!a) b"  */
    {
      TREE_OPERAND (expr, 0) = invert_truthvalue (TREE_OPERAND (expr, 0));
      ret = gimplify_expr (&TREE_OPERAND (expr, 0), pre_p, NULL,
			   is_gimple_condexpr, fb_rvalue);

      tmp = TREE_OPERAND (expr, 1);
      TREE_OPERAND (expr, 1) = TREE_OPERAND (expr, 2);
      TREE_OPERAND (expr, 2) = tmp;
    }
  else
    /* Both arms are empty; replace the COND_EXPR with its predicate.  */
    expr = TREE_OPERAND (expr, 0);

  *expr_p = expr;
  return ret;
}

/* A subroutine of gimplify_modify_expr.  Replace a MODIFY_EXPR with
   a call to __builtin_memcpy.  */

static enum gimplify_status
gimplify_modify_expr_to_memcpy (tree *expr_p, tree size, bool want_value)
{
  tree args, t, to, to_ptr, from;

  to = TREE_OPERAND (*expr_p, 0);
  from = TREE_OPERAND (*expr_p, 1);

  args = tree_cons (NULL, size, NULL);

  t = build_fold_addr_expr (from);
  args = tree_cons (NULL, t, args);

  to_ptr = build_fold_addr_expr (to);
  args = tree_cons (NULL, to_ptr, args);
  t = implicit_built_in_decls[BUILT_IN_MEMCPY];
  t = build_function_call_expr (t, args);

  if (want_value)
    {
      t = build1 (NOP_EXPR, TREE_TYPE (to_ptr), t);
      t = build1 (INDIRECT_REF, TREE_TYPE (to), t);
    }

  *expr_p = t;
  return GS_OK;
}

/* A subroutine of gimplify_modify_expr.  Replace a MODIFY_EXPR with
   a call to __builtin_memset.  In this case we know that the RHS is
   a CONSTRUCTOR with an empty element list.  */

static enum gimplify_status
gimplify_modify_expr_to_memset (tree *expr_p, tree size, bool want_value)
{
  tree args, t, to, to_ptr;

  to = TREE_OPERAND (*expr_p, 0);

  args = tree_cons (NULL, size, NULL);

  args = tree_cons (NULL, integer_zero_node, args);

  to_ptr = build_fold_addr_expr (to);
  args = tree_cons (NULL, to_ptr, args);
  t = implicit_built_in_decls[BUILT_IN_MEMSET];
  t = build_function_call_expr (t, args);

  if (want_value)
    {
      t = build1 (NOP_EXPR, TREE_TYPE (to_ptr), t);
      t = build1 (INDIRECT_REF, TREE_TYPE (to), t);
    }

  *expr_p = t;
  return GS_OK;
}

/* A subroutine of gimplify_init_ctor_preeval.  Called via walk_tree,
   determine, cautiously, if a CONSTRUCTOR overlaps the lhs of an
   assignment.  Returns non-null if we detect a potential overlap.  */

struct gimplify_init_ctor_preeval_data
{
  /* The base decl of the lhs object.  May be NULL, in which case we
     have to assume the lhs is indirect.  */
  tree lhs_base_decl;

  /* The alias set of the lhs object.  */
  int lhs_alias_set;
};

static tree
gimplify_init_ctor_preeval_1 (tree *tp, int *walk_subtrees, void *xdata)
{
  struct gimplify_init_ctor_preeval_data *data
    = (struct gimplify_init_ctor_preeval_data *) xdata;
  tree t = *tp;

  /* If we find the base object, obviously we have overlap.  */
  if (data->lhs_base_decl == t)
    return t;

  /* If the constructor component is indirect, determine if we have a
     potential overlap with the lhs.  The only bits of information we
     have to go on at this point are addressability and alias sets.  */
  if (TREE_CODE (t) == INDIRECT_REF
      && (!data->lhs_base_decl || TREE_ADDRESSABLE (data->lhs_base_decl))
      && alias_sets_conflict_p (data->lhs_alias_set, get_alias_set (t)))
    return t;

  if (IS_TYPE_OR_DECL_P (t))
    *walk_subtrees = 0;
  return NULL;
}

/* A subroutine of gimplify_init_constructor.  Pre-evaluate *EXPR_P,
   force values that overlap with the lhs (as described by *DATA)
   into temporaries.  */

static void
gimplify_init_ctor_preeval (tree *expr_p, tree *pre_p, tree *post_p,
			    struct gimplify_init_ctor_preeval_data *data)
{
  enum gimplify_status one;

  /* If the value is invariant, then there's nothing to pre-evaluate.
     But ensure it doesn't have any side-effects since a SAVE_EXPR is
     invariant but has side effects and might contain a reference to
     the object we're initializing.  */
  if (TREE_INVARIANT (*expr_p) && !TREE_SIDE_EFFECTS (*expr_p))
    return;

  /* If the type has non-trivial constructors, we can't pre-evaluate.  */
  if (TREE_ADDRESSABLE (TREE_TYPE (*expr_p)))
    return;

  /* Recurse for nested constructors.  */
  if (TREE_CODE (*expr_p) == CONSTRUCTOR)
    {
      tree list;
      for (list = CONSTRUCTOR_ELTS (*expr_p); list ; list = TREE_CHAIN (list))
	gimplify_init_ctor_preeval (&TREE_VALUE (list), pre_p, post_p, data);
      return;
    }

  /* We can't preevaluate if the type contains a placeholder.  */
  if (type_contains_placeholder_p (TREE_TYPE (*expr_p)))
    return;

  /* Gimplify the constructor element to something appropriate for the rhs
     of a MODIFY_EXPR.  Given that we know the lhs is an aggregate, we know
     the gimplifier will consider this a store to memory.  Doing this
     gimplification now means that we won't have to deal with complicated
     language-specific trees, nor trees like SAVE_EXPR that can induce
     exponential search behavior.  */
  one = gimplify_expr (expr_p, pre_p, post_p, is_gimple_mem_rhs, fb_rvalue);
  if (one == GS_ERROR)
    {
      *expr_p = NULL;
      return;
    }

  /* If we gimplified to a bare decl, we can be sure that it doesn't overlap
     with the lhs, since "a = { .x=a }" doesn't make sense.  This will
     always be true for all scalars, since is_gimple_mem_rhs insists on a
     temporary variable for them.  */
  if (DECL_P (*expr_p))
    return;

  /* If this is of variable size, we have no choice but to assume it doesn't
     overlap since we can't make a temporary for it.  */
  if (!TREE_CONSTANT (TYPE_SIZE (TREE_TYPE (*expr_p))))
    return;

  /* Otherwise, we must search for overlap ...  */
  if (!walk_tree (expr_p, gimplify_init_ctor_preeval_1, data, NULL))
    return;

  /* ... and if found, force the value into a temporary.  */
  *expr_p = get_formal_tmp_var (*expr_p, pre_p);
}

/* A subroutine of gimplify_init_constructor.  Generate individual
   MODIFY_EXPRs for a CONSTRUCTOR.  OBJECT is the LHS against which the
   assignments should happen.  LIST is the CONSTRUCTOR_ELTS of the
   CONSTRUCTOR.  CLEARED is true if the entire LHS object has been
   zeroed first.  */

static void
gimplify_init_ctor_eval (tree object, tree list, tree *pre_p, bool cleared)
{
  tree array_elt_type = NULL;

  if (TREE_CODE (TREE_TYPE (object)) == ARRAY_TYPE)
    array_elt_type = TYPE_MAIN_VARIANT (TREE_TYPE (TREE_TYPE (object)));

  for (; list; list = TREE_CHAIN (list))
    {
      tree purpose, value, cref, init;

      purpose = TREE_PURPOSE (list);
      value = TREE_VALUE (list);

      /* NULL values are created above for gimplification errors.  */
      if (value == NULL)
	continue;

      if (cleared && initializer_zerop (value))
	continue;

      if (array_elt_type)
	{
	  /* ??? Here's to hoping the front end fills in all of the indicies,
	     so we don't have to figure out what's missing ourselves.  */
	  gcc_assert (purpose);
	  /* ??? Need to handle this.  */
	  gcc_assert (TREE_CODE (purpose) != RANGE_EXPR);

	  cref = build (ARRAY_REF, array_elt_type, unshare_expr (object),
			purpose, NULL_TREE, NULL_TREE);
	}
      else
	cref = build (COMPONENT_REF, TREE_TYPE (purpose),
		      unshare_expr (object), purpose, NULL_TREE);

      if (TREE_CODE (value) == CONSTRUCTOR)
	gimplify_init_ctor_eval (cref, CONSTRUCTOR_ELTS (value),
				 pre_p, cleared);
      else
	{
	  init = build (MODIFY_EXPR, TREE_TYPE (cref), cref, value);
	  gimplify_and_add (init, pre_p);
	}
    }
}

/* A subroutine of gimplify_modify_expr.  Break out elements of a
   CONSTRUCTOR used as an initializer into separate MODIFY_EXPRs.

   Note that we still need to clear any elements that don't have explicit
   initializers, so if not all elements are initialized we keep the
   original MODIFY_EXPR, we just remove all of the constructor elements.  */

static enum gimplify_status
gimplify_init_constructor (tree *expr_p, tree *pre_p,
			   tree *post_p, bool want_value)
{
  tree object;
  tree ctor = TREE_OPERAND (*expr_p, 1);
  tree type = TREE_TYPE (ctor);
  enum gimplify_status ret;
  tree elt_list;

  if (TREE_CODE (ctor) != CONSTRUCTOR)
    return GS_UNHANDLED;

  ret = gimplify_expr (&TREE_OPERAND (*expr_p, 0), pre_p, post_p,
		       is_gimple_lvalue, fb_lvalue);
  if (ret == GS_ERROR)
    return ret;
  object = TREE_OPERAND (*expr_p, 0);

  elt_list = CONSTRUCTOR_ELTS (ctor);

  ret = GS_ALL_DONE;
  switch (TREE_CODE (type))
    {
    case RECORD_TYPE:
    case UNION_TYPE:
    case QUAL_UNION_TYPE:
    case ARRAY_TYPE:
      {
	struct gimplify_init_ctor_preeval_data preeval_data;
	HOST_WIDE_INT num_elements, num_nonzero_elements;
	HOST_WIDE_INT num_nonconstant_elements;
	bool cleared;

	/* Aggregate types must lower constructors to initialization of
	   individual elements.  The exception is that a CONSTRUCTOR node
	   with no elements indicates zero-initialization of the whole.  */
	if (elt_list == NULL)
	  break;

	categorize_ctor_elements (ctor, &num_nonzero_elements,
				  &num_nonconstant_elements);

	/* If a const aggregate variable is being initialized, then it
	   should never be a lose to promote the variable to be static.  */
	if (num_nonconstant_elements == 0
	    && TREE_READONLY (object)
	    && TREE_CODE (object) == VAR_DECL)
	  {
	    DECL_INITIAL (object) = ctor;
	    TREE_STATIC (object) = 1;
	    if (!DECL_NAME (object))
	      DECL_NAME (object) = create_tmp_var_name ("C");
	    walk_tree (&DECL_INITIAL (object), force_labels_r, NULL, NULL);

	    /* ??? C++ doesn't automatically append a .<number> to the
	       assembler name, and even when it does, it looks a FE private
	       data structures to figure out what that number should be,
	       which are not set for this variable.  I suppose this is
	       important for local statics for inline functions, which aren't
	       "local" in the object file sense.  So in order to get a unique
	       TU-local symbol, we must invoke the lhd version now.  */
	    lhd_set_decl_assembler_name (object);

	    *expr_p = NULL_TREE;
	    break;
	  }

	/* If there are "lots" of initialized elements, and all of them
	   are valid address constants, then the entire initializer can
	   be dropped to memory, and then memcpy'd out.  */
	if (num_nonconstant_elements == 0)
	  {
	    HOST_WIDE_INT size = int_size_in_bytes (type);
	    unsigned int align;

	    /* ??? We can still get unbounded array types, at least
	       from the C++ front end.  This seems wrong, but attempt
	       to work around it for now.  */
	    if (size < 0)
	      {
		size = int_size_in_bytes (TREE_TYPE (object));
		if (size >= 0)
		  TREE_TYPE (ctor) = type = TREE_TYPE (object);
	      }

	    /* Find the maximum alignment we can assume for the object.  */
	    /* ??? Make use of DECL_OFFSET_ALIGN.  */
	    if (DECL_P (object))
	      align = DECL_ALIGN (object);
	    else
	      align = TYPE_ALIGN (type);

	    if (size > 0 && !can_move_by_pieces (size, align))
	      {
		tree new = create_tmp_var_raw (type, "C");

		gimple_add_tmp_var (new);
		TREE_STATIC (new) = 1;
		TREE_READONLY (new) = 1;
		DECL_INITIAL (new) = ctor;
		if (align > DECL_ALIGN (new))
		  {
		    DECL_ALIGN (new) = align;
		    DECL_USER_ALIGN (new) = 1;
		  }
	        walk_tree (&DECL_INITIAL (new), force_labels_r, NULL, NULL);

		TREE_OPERAND (*expr_p, 1) = new;

		/* This is no longer an assignment of a CONSTRUCTOR, but
		   we still may have processing to do on the LHS.  So
		   pretend we didn't do anything here to let that happen.  */
		return GS_UNHANDLED;
	      }
	  }

	/* If there are "lots" of initialized elements, even discounting
	   those that are not address constants (and thus *must* be
	   computed at runtime), then partition the constructor into
	   constant and non-constant parts.  Block copy the constant
	   parts in, then generate code for the non-constant parts.  */
	/* TODO.  There's code in cp/typeck.c to do this.  */

	num_elements = count_type_elements (TREE_TYPE (ctor));

	/* If there are "lots" of zeros, then block clear the object first.  */
	cleared = false;
	if (num_elements - num_nonzero_elements > CLEAR_RATIO
	    && num_nonzero_elements < num_elements/4)
	  cleared = true;

	/* ??? This bit ought not be needed.  For any element not present
	   in the initializer, we should simply set them to zero.  Except
	   we'd need to *find* the elements that are not present, and that
	   requires trickery to avoid quadratic compile-time behavior in
	   large cases or excessive memory use in small cases.  */
	else
	  {
	    HOST_WIDE_INT len = list_length (elt_list);
	    if (TREE_CODE (type) == ARRAY_TYPE)
	      {
		tree nelts = array_type_nelts (type);
		if (!host_integerp (nelts, 1)
		    || tree_low_cst (nelts, 1) + 1 != len)
		  cleared = true;
	      }
	    else if (len != fields_length (type))
	      cleared = true;
	  }

	if (cleared)
	  {
	    /* Zap the CONSTRUCTOR element list, which simplifies this case.
	       Note that we still have to gimplify, in order to handle the
	       case of variable sized types.  Avoid shared tree structures.  */
	    CONSTRUCTOR_ELTS (ctor) = NULL_TREE;
	    object = unshare_expr (object);
	    gimplify_stmt (expr_p);
	    append_to_statement_list (*expr_p, pre_p);
	  }

	preeval_data.lhs_base_decl = get_base_address (object);
	if (!DECL_P (preeval_data.lhs_base_decl))
	  preeval_data.lhs_base_decl = NULL;
	preeval_data.lhs_alias_set = get_alias_set (object);

	gimplify_init_ctor_preeval (&TREE_OPERAND (*expr_p, 1),
				    pre_p, post_p, &preeval_data);
	gimplify_init_ctor_eval (object, elt_list, pre_p, cleared);

	*expr_p = NULL_TREE;
      }
      break;

    case COMPLEX_TYPE:
      {
	tree r, i;

	/* Extract the real and imaginary parts out of the ctor.  */
	r = i = NULL_TREE;
	if (elt_list)
	  {
	    r = TREE_VALUE (elt_list);
	    elt_list = TREE_CHAIN (elt_list);
	    if (elt_list)
	      {
		i = TREE_VALUE (elt_list);
		gcc_assert (!TREE_CHAIN (elt_list));
	      }
	  }
	if (r == NULL || i == NULL)
	  {
	    tree zero = convert (TREE_TYPE (type), integer_zero_node);
	    if (r == NULL)
	      r = zero;
	    if (i == NULL)
	      i = zero;
	  }

	/* Complex types have either COMPLEX_CST or COMPLEX_EXPR to
	   represent creation of a complex value.  */
	if (TREE_CONSTANT (r) && TREE_CONSTANT (i))
	  {
	    ctor = build_complex (type, r, i);
	    TREE_OPERAND (*expr_p, 1) = ctor;
	  }
	else
	  {
	    ctor = build (COMPLEX_EXPR, type, r, i);
	    TREE_OPERAND (*expr_p, 1) = ctor;
	    ret = gimplify_expr (&TREE_OPERAND (*expr_p, 1), pre_p, post_p,
				 rhs_predicate_for (TREE_OPERAND (*expr_p, 0)),
				 fb_rvalue);
	  }
      }
      break;

    case VECTOR_TYPE:
      /* Go ahead and simplify constant constructors to VECTOR_CST.  */
      if (TREE_CONSTANT (ctor))
  	{
	  TREE_OPERAND (*expr_p, 1) = build_vector (type, elt_list);
	  /* APPLE LOCAL AltiVec construction. */
	  /* Only RHS has been handled here. */
	  return GS_UNHANDLED;
	}
      else
	{
	  /* Vector types use CONSTRUCTOR all the way through gimple
	     compilation as a general initializer.  */
	  for (; elt_list; elt_list = TREE_CHAIN (elt_list))
	    {
	      enum gimplify_status tret;
	      tret = gimplify_expr (&TREE_VALUE (elt_list), pre_p, post_p,
				    is_gimple_val, fb_rvalue);
	      if (tret == GS_ERROR)
		ret = GS_ERROR;
	    }
	}
      break;

    default:
      /* So how did we get a CONSTRUCTOR for a scalar type?  */
      gcc_unreachable ();
    }

  if (ret == GS_ERROR)
    return GS_ERROR;
  else if (want_value)
    {
      append_to_statement_list (*expr_p, pre_p);
      *expr_p = object;
      return GS_OK;
    }
  else
    return GS_ALL_DONE;
}

/* Subroutine of gimplify_modify_expr to do simplifications of MODIFY_EXPRs
   based on the code of the RHS.  We loop for as long as something changes.  */

static enum gimplify_status
gimplify_modify_expr_rhs (tree *expr_p, tree *from_p, tree *to_p, tree *pre_p,
			  tree *post_p, bool want_value)
{
  enum gimplify_status ret = GS_OK;

  while (ret != GS_UNHANDLED)
    switch (TREE_CODE (*from_p))
      {
      case TARGET_EXPR:
	{
	  /* If we are initializing something from a TARGET_EXPR, strip the
	     TARGET_EXPR and initialize it directly, if possible.  This can't
	     be done if the initializer is void, since that implies that the
	     temporary is set in some non-trivial way.

	     ??? What about code that pulls out the temp and uses it
	     elsewhere? I think that such code never uses the TARGET_EXPR as
	     an initializer.  If I'm wrong, we'll abort because the temp won't
	     have any RTL.  In that case, I guess we'll need to replace
	     references somehow.  */
	  tree init = TARGET_EXPR_INITIAL (*from_p);

	  if (!VOID_TYPE_P (TREE_TYPE (init)))
	    {
	      *from_p = init;
	      ret = GS_OK;
	    }
	  else
	    ret = GS_UNHANDLED;
	}
	break;

      case COMPOUND_EXPR:
	/* Remove any COMPOUND_EXPR in the RHS so the following cases will be
	   caught.  */
	gimplify_compound_expr (from_p, pre_p, true);
	ret = GS_OK;
	break;

      case CONSTRUCTOR:
	/* If we're initializing from a CONSTRUCTOR, break this into
	   individual MODIFY_EXPRs.  */
	return gimplify_init_constructor (expr_p, pre_p, post_p, want_value);

      case COND_EXPR:
	/* If we're assigning to a non-register type, push the assignment
	   down into the branches.  This is mandatory for ADDRESSABLE types,
	   since we cannot generate temporaries for such, but it saves a
	   copy in other cases as well.  */
	if (!is_gimple_reg_type (TREE_TYPE (*from_p)))
	  {
	    *expr_p = *from_p;
	    return gimplify_cond_expr (expr_p, pre_p, *to_p);
	  }
	else
	  ret = GS_UNHANDLED;
	break;

      default:
	ret = GS_UNHANDLED;
	break;
      }

  return ret;
}

/* Gimplify the MODIFY_EXPR node pointed by EXPR_P.

      modify_expr
	      : varname '=' rhs
	      | '*' ID '=' rhs

    PRE_P points to the list where side effects that must happen before
	*EXPR_P should be stored.

    POST_P points to the list where side effects that must happen after
	*EXPR_P should be stored.

    WANT_VALUE is nonzero iff we want to use the value of this expression
	in another expression.  */

static enum gimplify_status
gimplify_modify_expr (tree *expr_p, tree *pre_p, tree *post_p, bool want_value)
{
  tree *from_p = &TREE_OPERAND (*expr_p, 1);
  tree *to_p = &TREE_OPERAND (*expr_p, 0);
  enum gimplify_status ret = GS_UNHANDLED;

  gcc_assert (TREE_CODE (*expr_p) == MODIFY_EXPR
	      || TREE_CODE (*expr_p) == INIT_EXPR);

  /* The distinction between MODIFY_EXPR and INIT_EXPR is no longer useful.  */
  if (TREE_CODE (*expr_p) == INIT_EXPR)
    TREE_SET_CODE (*expr_p, MODIFY_EXPR);

  /* See if any simplifications can be done based on what the RHS is.  */
  ret = gimplify_modify_expr_rhs (expr_p, from_p, to_p, pre_p, post_p,
				  want_value);
  if (ret != GS_UNHANDLED)
    return ret;

  /* If the value being copied is of variable width, compute the length
     of the copy into a WITH_SIZE_EXPR.   Note that we need to do this
     before gimplifying any of the operands so that we can resolve any
     PLACEHOLDER_EXPRs in the size.  Also note that the RTL expander uses
     the size of the expression to be copied, not of the destination, so
     that is what we must here.  */
  maybe_with_size_expr (from_p);

  ret = gimplify_expr (to_p, pre_p, post_p, is_gimple_lvalue, fb_lvalue);
  if (ret == GS_ERROR)
    return ret;

  ret = gimplify_expr (from_p, pre_p, post_p,
		       rhs_predicate_for (*to_p), fb_rvalue);
  if (ret == GS_ERROR)
    return ret;

  /* Now see if the above changed *from_p to something we handle specially.  */
  ret = gimplify_modify_expr_rhs (expr_p, from_p, to_p, pre_p, post_p,
				  want_value);
  if (ret != GS_UNHANDLED)
    return ret;

  /* If we've got a variable sized assignment between two lvalues (i.e. does
     not involve a call), then we can make things a bit more straightforward
     by converting the assignment to memcpy or memset.  */
  if (TREE_CODE (*from_p) == WITH_SIZE_EXPR)
    {
      tree from = TREE_OPERAND (*from_p, 0);
      tree size = TREE_OPERAND (*from_p, 1);

      if (TREE_CODE (from) == CONSTRUCTOR)
	return gimplify_modify_expr_to_memset (expr_p, size, want_value);
      if (is_gimple_addressable (from))
	{
	  *from_p = from;
	  return gimplify_modify_expr_to_memcpy (expr_p, size, want_value);
	}
    }

  if (gimplify_ctxp->into_ssa && is_gimple_reg (*to_p))
    {
      /* If we've somehow already got an SSA_NAME on the LHS, then
	 we're probably modifying it twice.  Not good.  */
      gcc_assert (TREE_CODE (*to_p) != SSA_NAME);
      *to_p = make_ssa_name (*to_p, *expr_p);
    }

  if (want_value)
    {
      append_to_statement_list (*expr_p, pre_p);
      *expr_p = *to_p;
      return GS_OK;
    }

  return GS_ALL_DONE;
}

/*  Gimplify a comparison between two variable-sized objects.  Do this
    with a call to BUILT_IN_MEMCMP.  */

static enum gimplify_status
gimplify_variable_sized_compare (tree *expr_p)
{
  tree op0 = TREE_OPERAND (*expr_p, 0);
  tree op1 = TREE_OPERAND (*expr_p, 1);
  tree args, t, dest;

  t = TYPE_SIZE_UNIT (TREE_TYPE (op0));
  t = unshare_expr (t);
  t = SUBSTITUTE_PLACEHOLDER_IN_EXPR (t, op0);
  args = tree_cons (NULL, t, NULL);
  t = build_fold_addr_expr (op1);
  args = tree_cons (NULL, t, args);
  dest = build_fold_addr_expr (op0);
  args = tree_cons (NULL, dest, args);
  t = implicit_built_in_decls[BUILT_IN_MEMCMP];
  t = build_function_call_expr (t, args);
  *expr_p
    = build (TREE_CODE (*expr_p), TREE_TYPE (*expr_p), t, integer_zero_node);

  return GS_OK;
}

/*  Gimplify TRUTH_ANDIF_EXPR and TRUTH_ORIF_EXPR expressions.  EXPR_P
    points to the expression to gimplify.

    Expressions of the form 'a && b' are gimplified to:

	a && b ? true : false

    gimplify_cond_expr will do the rest.

    PRE_P points to the list where side effects that must happen before
	*EXPR_P should be stored.  */

static enum gimplify_status
gimplify_boolean_expr (tree *expr_p)
{
  /* Preserve the original type of the expression.  */
  tree type = TREE_TYPE (*expr_p);

  *expr_p = build (COND_EXPR, type, *expr_p,
		   convert (type, boolean_true_node),
		   convert (type, boolean_false_node));

  return GS_OK;
}

/* Gimplifies an expression sequence.  This function gimplifies each
   expression and re-writes the original expression with the last
   expression of the sequence in GIMPLE form.

   PRE_P points to the list where the side effects for all the
       expressions in the sequence will be emitted.

   WANT_VALUE is true when the result of the last COMPOUND_EXPR is used.  */
/* ??? Should rearrange to share the pre-queue with all the indirect
   invocations of gimplify_expr.  Would probably save on creations
   of statement_list nodes.  */

static enum gimplify_status
gimplify_compound_expr (tree *expr_p, tree *pre_p, bool want_value)
{
  tree t = *expr_p;

  do
    {
      tree *sub_p = &TREE_OPERAND (t, 0);

      if (TREE_CODE (*sub_p) == COMPOUND_EXPR)
	gimplify_compound_expr (sub_p, pre_p, false);
      else
	gimplify_stmt (sub_p);
      append_to_statement_list (*sub_p, pre_p);

      t = TREE_OPERAND (t, 1);
    }
  while (TREE_CODE (t) == COMPOUND_EXPR);

  *expr_p = t;
  if (want_value)
    return GS_OK;
  else
    {
      gimplify_stmt (expr_p);
      return GS_ALL_DONE;
    }
}

/* Gimplifies a statement list.  These may be created either by an
   enlightened front-end, or by shortcut_cond_expr.  */

static enum gimplify_status
gimplify_statement_list (tree *expr_p)
{
  tree_stmt_iterator i = tsi_start (*expr_p);

  while (!tsi_end_p (i))
    {
      tree t;

      gimplify_stmt (tsi_stmt_ptr (i));

      t = tsi_stmt (i);
      if (t == NULL)
	tsi_delink (&i);
      else if (TREE_CODE (t) == STATEMENT_LIST)
	{
	  tsi_link_before (&i, t, TSI_SAME_STMT);
	  tsi_delink (&i);
	}
      else
	tsi_next (&i);
    }

  return GS_ALL_DONE;
}

/*  Gimplify a SAVE_EXPR node.  EXPR_P points to the expression to
    gimplify.  After gimplification, EXPR_P will point to a new temporary
    that holds the original value of the SAVE_EXPR node.

    PRE_P points to the list where side effects that must happen before
	*EXPR_P should be stored.  */

static enum gimplify_status
gimplify_save_expr (tree *expr_p, tree *pre_p, tree *post_p)
{
  enum gimplify_status ret = GS_ALL_DONE;
  tree val;

  gcc_assert (TREE_CODE (*expr_p) == SAVE_EXPR);
  val = TREE_OPERAND (*expr_p, 0);

  /* If the SAVE_EXPR has not been resolved, then evaluate it once.  */
  if (!SAVE_EXPR_RESOLVED_P (*expr_p))
    {
      /* The operand may be a void-valued expression such as SAVE_EXPRs
	 generated by the Java frontend for class initialization.  It is
	 being executed only for its side-effects.  */
      if (TREE_TYPE (val) == void_type_node)
	{
	  ret = gimplify_expr (&TREE_OPERAND (*expr_p, 0), pre_p, post_p,
			       is_gimple_stmt, fb_none);
	  append_to_statement_list (TREE_OPERAND (*expr_p, 0), pre_p);
	  val = NULL;
	}
      else
	val = get_initialized_tmp_var (val, pre_p, post_p);

      TREE_OPERAND (*expr_p, 0) = val;
      SAVE_EXPR_RESOLVED_P (*expr_p) = 1;
    }

  *expr_p = val;

  return ret;
}

/*  Re-write the ADDR_EXPR node pointed by EXPR_P

      unary_expr
	      : ...
	      | '&' varname
	      ...

    PRE_P points to the list where side effects that must happen before
	*EXPR_P should be stored.

    POST_P points to the list where side effects that must happen after
	*EXPR_P should be stored.  */

static enum gimplify_status
gimplify_addr_expr (tree *expr_p, tree *pre_p, tree *post_p)
{
  tree expr = *expr_p;
  tree op0 = TREE_OPERAND (expr, 0);
  enum gimplify_status ret;

  switch (TREE_CODE (op0))
    {
    case INDIRECT_REF:
    case MISALIGNED_INDIRECT_REF:
    do_indirect_ref:
      /* Check if we are dealing with an expression of the form '&*ptr'.
	 While the front end folds away '&*ptr' into 'ptr', these
	 expressions may be generated internally by the compiler (e.g.,
	 builtins like __builtin_va_end).  */
      /* Caution: the silent array decomposition semantics we allow for
	 ADDR_EXPR means we can't always discard the pair.  */
      {
	tree op00 = TREE_OPERAND (op0, 0);
	tree t_expr = TREE_TYPE (expr);
	tree t_op00 = TREE_TYPE (op00);

        if (!lang_hooks.types_compatible_p (t_expr, t_op00))
	  {
#ifdef ENABLE_CHECKING
	    tree t_op0 = TREE_TYPE (op0);
	    gcc_assert (TREE_CODE (t_op0) == ARRAY_TYPE
			&& POINTER_TYPE_P (t_expr)
			&& cpt_same_type (TREE_TYPE (t_op0),
					  TREE_TYPE (t_expr))
			&& POINTER_TYPE_P (t_op00)
			&& cpt_same_type (t_op0, TREE_TYPE (t_op00)));
#endif
	    op00 = fold_convert (TREE_TYPE (expr), op00);
	  }
        *expr_p = op00;
        ret = GS_OK;
      }
      break;

    case VIEW_CONVERT_EXPR:
      /* Take the address of our operand and then convert it to the type of
	 this ADDR_EXPR.

	 ??? The interactions of VIEW_CONVERT_EXPR and aliasing is not at
	 all clear.  The impact of this transformation is even less clear.  */
      *expr_p = fold_convert (TREE_TYPE (expr),
			      build_fold_addr_expr (TREE_OPERAND (op0, 0)));
      ret = GS_OK;
      break;

    default:
      /* We use fb_either here because the C frontend sometimes takes
	 the address of a call that returns a struct; see
	 gcc.dg/c99-array-lval-1.c.  The gimplifier will correctly make
	 the implied temporary explicit.  */
      ret = gimplify_expr (&TREE_OPERAND (expr, 0), pre_p, post_p,
			   is_gimple_addressable, fb_either);
      if (ret != GS_ERROR)
	{
	  op0 = TREE_OPERAND (expr, 0);

	  /* For various reasons, the gimplification of the expression
	     may have made a new INDIRECT_REF.  */
	  if (TREE_CODE (op0) == INDIRECT_REF)
	    goto do_indirect_ref;

	  /* Make sure TREE_INVARIANT, TREE_CONSTANT, and TREE_SIDE_EFFECTS
	     is set properly.  */
	  recompute_tree_invarant_for_addr_expr (expr);

	  /* Mark the RHS addressable.  */
	  lang_hooks.mark_addressable (TREE_OPERAND (expr, 0));
	}
      break;
    }

  return ret;
}

/* Gimplify the operands of an ASM_EXPR.  Input operands should be a gimple
   value; output operands should be a gimple lvalue.  */

static enum gimplify_status
gimplify_asm_expr (tree *expr_p, tree *pre_p, tree *post_p)
{
  tree expr = *expr_p;
  int noutputs = list_length (ASM_OUTPUTS (expr));
  const char **oconstraints
    = (const char **) alloca ((noutputs) * sizeof (const char *));
  int i;
  tree link;
  const char *constraint;
  bool allows_mem, allows_reg, is_inout;
  enum gimplify_status ret, tret;

  ASM_STRING (expr)
    = resolve_asm_operand_names (ASM_STRING (expr), ASM_OUTPUTS (expr),
				 ASM_INPUTS (expr));

  ret = GS_ALL_DONE;
  for (i = 0, link = ASM_OUTPUTS (expr); link; ++i, link = TREE_CHAIN (link))
    {
      oconstraints[i] = constraint
	= TREE_STRING_POINTER (TREE_VALUE (TREE_PURPOSE (link)));

      parse_output_constraint (&constraint, i, 0, 0,
			       &allows_mem, &allows_reg, &is_inout);

      if (!allows_reg && allows_mem)
	lang_hooks.mark_addressable (TREE_VALUE (link));

      tret = gimplify_expr (&TREE_VALUE (link), pre_p, post_p,
			    is_inout ? is_gimple_min_lval : is_gimple_lvalue,
			    fb_lvalue | fb_mayfail);
      if (tret == GS_ERROR)
	{
	  error ("invalid lvalue in asm output %d", i);
	  ret = tret;
	}

      if (is_inout)
	{
	  /* An input/output operand.  To give the optimizers more
	     flexibility, split it into separate input and output
 	     operands.  */
	  tree input;
	  char buf[10];
	  size_t constraint_len = strlen (constraint);

	  /* Turn the in/out constraint into an output constraint.  */
	  char *p = xstrdup (constraint);
	  p[0] = '=';
	  TREE_VALUE (TREE_PURPOSE (link)) = build_string (constraint_len, p);
	  free (p);

	  /* And add a matching input constraint.  */
	  if (allows_reg)
	    {
	      sprintf (buf, "%d", i);
	      input = build_string (strlen (buf), buf);
	    }
	  else
	    input = build_string (constraint_len - 1, constraint + 1);
	  input = build_tree_list (build_tree_list (NULL_TREE, input),
				   unshare_expr (TREE_VALUE (link)));
	  ASM_INPUTS (expr) = chainon (ASM_INPUTS (expr), input);
	}
    }

  for (link = ASM_INPUTS (expr); link; ++i, link = TREE_CHAIN (link))
    {
      constraint
	= TREE_STRING_POINTER (TREE_VALUE (TREE_PURPOSE (link)));
      parse_input_constraint (&constraint, 0, 0, noutputs, 0,
			      oconstraints, &allows_mem, &allows_reg);

      /* If the operand is a memory input, it should be an lvalue.  */
      if (!allows_reg && allows_mem)
	{
	  lang_hooks.mark_addressable (TREE_VALUE (link));
	  tret = gimplify_expr (&TREE_VALUE (link), pre_p, post_p,
				is_gimple_lvalue, fb_lvalue | fb_mayfail);
	  if (tret == GS_ERROR)
	    {
	      error ("memory input %d is not directly addressable", i);
	      ret = tret;
	    }
	}
      else
	{
	  tret = gimplify_expr (&TREE_VALUE (link), pre_p, post_p,
				is_gimple_asm_val, fb_rvalue);
	  if (tret == GS_ERROR)
	    ret = tret;
	}
    }

  return ret;
}

/* Gimplify a CLEANUP_POINT_EXPR.  Currently this works by adding
   WITH_CLEANUP_EXPRs to the prequeue as we encounter cleanups while
   gimplifying the body, and converting them to TRY_FINALLY_EXPRs when we
   return to this function.

   FIXME should we complexify the prequeue handling instead?  Or use flags
   for all the cleanups and let the optimizer tighten them up?  The current
   code seems pretty fragile; it will break on a cleanup within any
   non-conditional nesting.  But any such nesting would be broken, anyway;
   we can't write a TRY_FINALLY_EXPR that starts inside a nesting construct
   and continues out of it.  We can do that at the RTL level, though, so
   having an optimizer to tighten up try/finally regions would be a Good
   Thing.  */

static enum gimplify_status
gimplify_cleanup_point_expr (tree *expr_p, tree *pre_p)
{
  tree_stmt_iterator iter;
  tree body;

  tree temp = voidify_wrapper_expr (*expr_p, NULL);

  /* We only care about the number of conditions between the innermost
     CLEANUP_POINT_EXPR and the cleanup.  So save and reset the count.  */
  int old_conds = gimplify_ctxp->conditions;
  gimplify_ctxp->conditions = 0;

  body = TREE_OPERAND (*expr_p, 0);
  gimplify_to_stmt_list (&body);

  gimplify_ctxp->conditions = old_conds;

  for (iter = tsi_start (body); !tsi_end_p (iter); )
    {
      tree *wce_p = tsi_stmt_ptr (iter);
      tree wce = *wce_p;

      if (TREE_CODE (wce) == WITH_CLEANUP_EXPR)
	{
	  if (tsi_one_before_end_p (iter))
	    {
	      tsi_link_before (&iter, TREE_OPERAND (wce, 0), TSI_SAME_STMT);
	      tsi_delink (&iter);
	      break;
	    }
	  else
	    {
	      tree sl, tfe;
	      enum tree_code code;

	      if (CLEANUP_EH_ONLY (wce))
		code = TRY_CATCH_EXPR;
	      else
		code = TRY_FINALLY_EXPR;

	      sl = tsi_split_statement_list_after (&iter);
	      tfe = build (code, void_type_node, sl, NULL_TREE);
	      append_to_statement_list (TREE_OPERAND (wce, 0),
				        &TREE_OPERAND (tfe, 1));
	      *wce_p = tfe;
	      iter = tsi_start (sl);
	    }
	}
      else
	tsi_next (&iter);
    }

  if (temp)
    {
      *expr_p = temp;
      append_to_statement_list (body, pre_p);
      return GS_OK;
    }
  else
    {
      *expr_p = body;
      return GS_ALL_DONE;
    }
}

/* Insert a cleanup marker for gimplify_cleanup_point_expr.  CLEANUP
   is the cleanup action required.  */

static void
gimple_push_cleanup (tree var, tree cleanup, bool eh_only, tree *pre_p)
{
  tree wce;

  /* Errors can result in improperly nested cleanups.  Which results in
     confusion when trying to resolve the WITH_CLEANUP_EXPR.  */
  if (errorcount || sorrycount)
    return;

  if (gimple_conditional_context ())
    {
      /* If we're in a conditional context, this is more complex.  We only
	 want to run the cleanup if we actually ran the initialization that
	 necessitates it, but we want to run it after the end of the
	 conditional context.  So we wrap the try/finally around the
	 condition and use a flag to determine whether or not to actually
	 run the destructor.  Thus

	   test ? f(A()) : 0

	 becomes (approximately)

	   flag = 0;
	   try {
	     if (test) { A::A(temp); flag = 1; val = f(temp); }
	     else { val = 0; }
	   } finally {
	     if (flag) A::~A(temp);
	   }
	   val
      */

      tree flag = create_tmp_var (boolean_type_node, "cleanup");
      tree ffalse = build (MODIFY_EXPR, void_type_node, flag,
			   boolean_false_node);
      tree ftrue = build (MODIFY_EXPR, void_type_node, flag,
			  boolean_true_node);
      cleanup = build (COND_EXPR, void_type_node, flag, cleanup, NULL);
      wce = build (WITH_CLEANUP_EXPR, void_type_node, cleanup);
      append_to_statement_list (ffalse, &gimplify_ctxp->conditional_cleanups);
      append_to_statement_list (wce, &gimplify_ctxp->conditional_cleanups);
      append_to_statement_list (ftrue, pre_p);

      /* Because of this manipulation, and the EH edges that jump
	 threading cannot redirect, the temporary (VAR) will appear
	 to be used uninitialized.  Don't warn.  */
      TREE_NO_WARNING (var) = 1;
    }
  else
    {
      wce = build (WITH_CLEANUP_EXPR, void_type_node, cleanup);
      CLEANUP_EH_ONLY (wce) = eh_only;
      append_to_statement_list (wce, pre_p);
    }

  gimplify_stmt (&TREE_OPERAND (wce, 0));
}

/* Gimplify a TARGET_EXPR which doesn't appear on the rhs of an INIT_EXPR.  */

static enum gimplify_status
gimplify_target_expr (tree *expr_p, tree *pre_p, tree *post_p)
{
  tree targ = *expr_p;
  tree temp = TARGET_EXPR_SLOT (targ);
  tree init = TARGET_EXPR_INITIAL (targ);
  enum gimplify_status ret;

  if (init)
    {
      /* TARGET_EXPR temps aren't part of the enclosing block, so add it
	 to the temps list.  */
      gimple_add_tmp_var (temp);

      /* If TARGET_EXPR_INITIAL is void, then the mere evaluation of the
	 expression is supposed to initialize the slot.  */
      if (VOID_TYPE_P (TREE_TYPE (init)))
	ret = gimplify_expr (&init, pre_p, post_p, is_gimple_stmt, fb_none);
      else
	{
          /* Special handling for BIND_EXPR can result in fewer temps.  */
	  ret = GS_OK;
          if (TREE_CODE (init) == BIND_EXPR)
	    gimplify_bind_expr (&init, temp, pre_p);
          if (init != temp)
	    {
	      init = build (MODIFY_EXPR, void_type_node, temp, init);
	      ret = gimplify_expr (&init, pre_p, post_p, is_gimple_stmt,
				   fb_none);
	    }
	}
      if (ret == GS_ERROR)
	return GS_ERROR;
      append_to_statement_list (init, pre_p);

      /* If needed, push the cleanup for the temp.  */
      if (TARGET_EXPR_CLEANUP (targ))
	{
	  gimplify_stmt (&TARGET_EXPR_CLEANUP (targ));
	  gimple_push_cleanup (temp, TARGET_EXPR_CLEANUP (targ),
			       CLEANUP_EH_ONLY (targ), pre_p);
	}

      /* Only expand this once.  */
      TREE_OPERAND (targ, 3) = init;
      TARGET_EXPR_INITIAL (targ) = NULL_TREE;
    }
  else
    /* We should have expanded this before.  */
    gcc_assert (DECL_SEEN_IN_BIND_EXPR_P (temp));

  *expr_p = temp;
  return GS_OK;
}

/* Gimplification of expression trees.  */

/* Gimplify an expression which appears at statement context; usually, this
   means replacing it with a suitably gimple STATEMENT_LIST.  */

void
gimplify_stmt (tree *stmt_p)
{
  gimplify_expr (stmt_p, NULL, NULL, is_gimple_stmt, fb_none);
}

/* Similarly, but force the result to be a STATEMENT_LIST.  */

void
gimplify_to_stmt_list (tree *stmt_p)
{
  gimplify_stmt (stmt_p);
  if (!*stmt_p)
    *stmt_p = alloc_stmt_list ();
  else if (TREE_CODE (*stmt_p) != STATEMENT_LIST)
    {
      tree t = *stmt_p;
      *stmt_p = alloc_stmt_list ();
      append_to_statement_list (t, stmt_p);
    }
}


/*  Gimplifies the expression tree pointed by EXPR_P.  Return 0 if
    gimplification failed.

    PRE_P points to the list where side effects that must happen before
	EXPR should be stored.

    POST_P points to the list where side effects that must happen after
	EXPR should be stored, or NULL if there is no suitable list.  In
	that case, we copy the result to a temporary, emit the
	post-effects, and then return the temporary.

    GIMPLE_TEST_F points to a function that takes a tree T and
	returns nonzero if T is in the GIMPLE form requested by the
	caller.  The GIMPLE predicates are in tree-gimple.c.

	This test is used twice.  Before gimplification, the test is
	invoked to determine whether *EXPR_P is already gimple enough.  If
	that fails, *EXPR_P is gimplified according to its code and
	GIMPLE_TEST_F is called again.  If the test still fails, then a new
	temporary variable is created and assigned the value of the
	gimplified expression.

    FALLBACK tells the function what sort of a temporary we want.  If the 1
	bit is set, an rvalue is OK.  If the 2 bit is set, an lvalue is OK.
	If both are set, either is OK, but an lvalue is preferable.

    The return value is either GS_ERROR or GS_ALL_DONE, since this function
    iterates until solution.  */

enum gimplify_status
gimplify_expr (tree *expr_p, tree *pre_p, tree *post_p,
	       bool (* gimple_test_f) (tree), fallback_t fallback)
{
  tree tmp;
  tree internal_pre = NULL_TREE;
  tree internal_post = NULL_TREE;
  tree save_expr;
  int is_statement = (pre_p == NULL);
  location_t saved_location;
  enum gimplify_status ret;

  save_expr = *expr_p;
  if (save_expr == NULL_TREE)
    return GS_ALL_DONE;

  /* We used to check the predicate here and return immediately if it
     succeeds.  This is wrong; the design is for gimplification to be
     idempotent, and for the predicates to only test for valid forms, not
     whether they are fully simplified.  */

  /* Set up our internal queues if needed.  */
  if (pre_p == NULL)
    pre_p = &internal_pre;
  if (post_p == NULL)
    post_p = &internal_post;

  saved_location = input_location;
  if (save_expr != error_mark_node
      && EXPR_HAS_LOCATION (*expr_p))
    input_location = EXPR_LOCATION (*expr_p);

  /* Loop over the specific gimplifiers until the toplevel node
     remains the same.  */
  do
    {
      /* Strip away as many useless type conversions as possible
	 at the toplevel.  */
      STRIP_USELESS_TYPE_CONVERSION (*expr_p);

      /* Remember the expr.  */
      save_expr = *expr_p;

      /* Die, die, die, my darling.  */
      if (save_expr == error_mark_node
	  || (TREE_TYPE (save_expr)
	      && TREE_TYPE (save_expr) == error_mark_node))
	{
	  ret = GS_ERROR;
	  break;
	}

      /* Do any language-specific gimplification.  */
      ret = lang_hooks.gimplify_expr (expr_p, pre_p, post_p);
      if (ret == GS_OK)
	{
	  if (*expr_p == NULL_TREE)
	    break;
	  if (*expr_p != save_expr)
	    continue;
	}
      else if (ret != GS_UNHANDLED)
	break;

      ret = GS_OK;
      switch (TREE_CODE (*expr_p))
	{
	  /* First deal with the special cases.  */

	case POSTINCREMENT_EXPR:
	case POSTDECREMENT_EXPR:
	case PREINCREMENT_EXPR:
	case PREDECREMENT_EXPR:
	  ret = gimplify_self_mod_expr (expr_p, pre_p, post_p,
					fallback != fb_none);
	  break;

	case ARRAY_REF:
	case ARRAY_RANGE_REF:
	case REALPART_EXPR:
	case IMAGPART_EXPR:
	case COMPONENT_REF:
	case VIEW_CONVERT_EXPR:
	  ret = gimplify_compound_lval (expr_p, pre_p, post_p,
					fallback ? fallback : fb_rvalue);
	  break;

	case COND_EXPR:
	  ret = gimplify_cond_expr (expr_p, pre_p, NULL_TREE);
	  break;

	case CALL_EXPR:
	  ret = gimplify_call_expr (expr_p, pre_p, fallback != fb_none);
	  break;

	case TREE_LIST:
	  gcc_unreachable ();

	case COMPOUND_EXPR:
	  ret = gimplify_compound_expr (expr_p, pre_p, fallback != fb_none);
	  break;

	case MODIFY_EXPR:
	case INIT_EXPR:
	  ret = gimplify_modify_expr (expr_p, pre_p, post_p,
				      fallback != fb_none);
	  break;

	case TRUTH_ANDIF_EXPR:
	case TRUTH_ORIF_EXPR:
	  ret = gimplify_boolean_expr (expr_p);
	  break;

	case TRUTH_NOT_EXPR:
	  TREE_OPERAND (*expr_p, 0)
	    = gimple_boolify (TREE_OPERAND (*expr_p, 0));
	  ret = gimplify_expr (&TREE_OPERAND (*expr_p, 0), pre_p, post_p,
			       is_gimple_val, fb_rvalue);
	  recalculate_side_effects (*expr_p);
	  break;

	case ADDR_EXPR:
	  ret = gimplify_addr_expr (expr_p, pre_p, post_p);
	  break;

	case VA_ARG_EXPR:
	  ret = gimplify_va_arg_expr (expr_p, pre_p, post_p);
	  break;

	case CONVERT_EXPR:
	case NOP_EXPR:
	  if (IS_EMPTY_STMT (*expr_p))
	    {
	      ret = GS_ALL_DONE;
	      break;
	    }

	  if (VOID_TYPE_P (TREE_TYPE (*expr_p))
	      || fallback == fb_none)
	    {
	      /* Just strip a conversion to void (or in void context) and
		 try again.  */
	      *expr_p = TREE_OPERAND (*expr_p, 0);
	      break;
	    }

	  ret = gimplify_conversion (expr_p);
	  if (ret == GS_ERROR)
	    break;
	  if (*expr_p != save_expr)
	    break;
	  /* FALLTHRU */

	case FIX_TRUNC_EXPR:
	case FIX_CEIL_EXPR:
	case FIX_FLOOR_EXPR:
	case FIX_ROUND_EXPR:
	  /* unary_expr: ... | '(' cast ')' val | ...  */
	  ret = gimplify_expr (&TREE_OPERAND (*expr_p, 0), pre_p, post_p,
			       is_gimple_val, fb_rvalue);
	  recalculate_side_effects (*expr_p);
	  break;

	case ALIGN_INDIRECT_REF:
	case MISALIGNED_INDIRECT_REF:
	case INDIRECT_REF:
	  ret = gimplify_expr (&TREE_OPERAND (*expr_p, 0), pre_p, post_p,
			       is_gimple_reg, fb_rvalue);
	  recalculate_side_effects (*expr_p);
	  break;

	  /* Constants need not be gimplified.  */
	case INTEGER_CST:
	case REAL_CST:
	case STRING_CST:
	case COMPLEX_CST:
	case VECTOR_CST:
	  ret = GS_ALL_DONE;
	  break;

	case CONST_DECL:
	  /* If we require an lvalue, such as for ADDR_EXPR, retain the
	     CONST_DECL node.  Otherwise the decl is replaceable by its
	     value.  */
	  /* ??? Should be == fb_lvalue, but ADDR_EXPR passes fb_either.  */
	  if (fallback & fb_lvalue)
	    ret = GS_ALL_DONE;
	  else
	    *expr_p = DECL_INITIAL (*expr_p);
	  break;

	case DECL_EXPR:
	  ret = gimplify_decl_expr (expr_p);
	  break;

	case EXC_PTR_EXPR:
	  /* FIXME make this a decl.  */
	  ret = GS_ALL_DONE;
	  break;

	case BIND_EXPR:
	  ret = gimplify_bind_expr (expr_p, NULL, pre_p);
	  break;

	case LOOP_EXPR:
	  ret = gimplify_loop_expr (expr_p, pre_p);
	  break;

	case SWITCH_EXPR:
	  ret = gimplify_switch_expr (expr_p, pre_p);
	  break;

	case LABELED_BLOCK_EXPR:
	  ret = gimplify_labeled_block_expr (expr_p);
	  break;

	case EXIT_BLOCK_EXPR:
	  ret = gimplify_exit_block_expr (expr_p);
	  break;

	case EXIT_EXPR:
	  ret = gimplify_exit_expr (expr_p);
	  break;

	case GOTO_EXPR:
	  /* If the target is not LABEL, then it is a computed jump
	     and the target needs to be gimplified.  */
	  if (TREE_CODE (GOTO_DESTINATION (*expr_p)) != LABEL_DECL)
	    ret = gimplify_expr (&GOTO_DESTINATION (*expr_p), pre_p,
				 NULL, is_gimple_val, fb_rvalue);
	  break;

	case LABEL_EXPR:
	  ret = GS_ALL_DONE;
	  gcc_assert (decl_function_context (LABEL_EXPR_LABEL (*expr_p))
		      == current_function_decl);
	  break;

	case CASE_LABEL_EXPR:
	  ret = gimplify_case_label_expr (expr_p);
	  break;

	case RETURN_EXPR:
	  ret = gimplify_return_expr (*expr_p, pre_p);
	  break;

	case CONSTRUCTOR:
	  /* Don't reduce this in place; let gimplify_init_constructor work its
	     magic.  Buf if we're just elaborating this for side effects, just
	     gimplify any element that has side-effects.  */
	  if (fallback == fb_none)
	    {
	      for (tmp = CONSTRUCTOR_ELTS (*expr_p); tmp;
		   tmp = TREE_CHAIN (tmp))
		if (TREE_SIDE_EFFECTS (TREE_VALUE (tmp)))
		  gimplify_expr (&TREE_VALUE (tmp), pre_p, post_p,
				 gimple_test_f, fallback);

	      *expr_p = NULL_TREE;
	    }

	  ret = GS_ALL_DONE;
	  break;

	  /* The following are special cases that are not handled by the
	     original GIMPLE grammar.  */

	  /* SAVE_EXPR nodes are converted into a GIMPLE identifier and
	     eliminated.  */
	case SAVE_EXPR:
	  ret = gimplify_save_expr (expr_p, pre_p, post_p);
	  break;

	case BIT_FIELD_REF:
	  {
	    enum gimplify_status r0, r1, r2;

	    r0 = gimplify_expr (&TREE_OPERAND (*expr_p, 0), pre_p, post_p,
				is_gimple_lvalue, fb_either);
	    r1 = gimplify_expr (&TREE_OPERAND (*expr_p, 1), pre_p, post_p,
				is_gimple_val, fb_rvalue);
	    r2 = gimplify_expr (&TREE_OPERAND (*expr_p, 2), pre_p, post_p,
				is_gimple_val, fb_rvalue);
	    recalculate_side_effects (*expr_p);

	    ret = MIN (r0, MIN (r1, r2));
	  }
	  break;

	case NON_LVALUE_EXPR:
	  /* This should have been stripped above.  */
	  gcc_unreachable ();

	case ASM_EXPR:
	  ret = gimplify_asm_expr (expr_p, pre_p, post_p);
	  break;

	case TRY_FINALLY_EXPR:
	case TRY_CATCH_EXPR:
	  gimplify_to_stmt_list (&TREE_OPERAND (*expr_p, 0));
	  gimplify_to_stmt_list (&TREE_OPERAND (*expr_p, 1));
	  ret = GS_ALL_DONE;
	  break;

	case CLEANUP_POINT_EXPR:
	  ret = gimplify_cleanup_point_expr (expr_p, pre_p);
	  break;

	case TARGET_EXPR:
	  ret = gimplify_target_expr (expr_p, pre_p, post_p);
	  break;

	case CATCH_EXPR:
	  gimplify_to_stmt_list (&CATCH_BODY (*expr_p));
	  ret = GS_ALL_DONE;
	  break;

	case EH_FILTER_EXPR:
	  gimplify_to_stmt_list (&EH_FILTER_FAILURE (*expr_p));
	  ret = GS_ALL_DONE;
	  break;

	case OBJ_TYPE_REF:
	  {
	    enum gimplify_status r0, r1;
	    r0 = gimplify_expr (&OBJ_TYPE_REF_OBJECT (*expr_p), pre_p, post_p,
			        is_gimple_val, fb_rvalue);
	    r1 = gimplify_expr (&OBJ_TYPE_REF_EXPR (*expr_p), pre_p, post_p,
			        is_gimple_val, fb_rvalue);
	    ret = MIN (r0, r1);
	  }
	  break;

	case LABEL_DECL:
	  /* We get here when taking the address of a label.  We mark
	     the label as "forced"; meaning it can never be removed and
	     it is a potential target for any computed goto.  */
	  FORCED_LABEL (*expr_p) = 1;
	  ret = GS_ALL_DONE;
	  break;

	case STATEMENT_LIST:
	  ret = gimplify_statement_list (expr_p);
	  break;

	case WITH_SIZE_EXPR:
	  {
	    enum gimplify_status r0, r1;
	    r0 = gimplify_expr (&TREE_OPERAND (*expr_p, 0), pre_p,
				post_p == &internal_post ? NULL : post_p,
				gimple_test_f, fallback);
	    r1 = gimplify_expr (&TREE_OPERAND (*expr_p, 1), pre_p, post_p,
				is_gimple_val, fb_rvalue);
	  }
	  break;

	case VAR_DECL:
	  /* ??? If this is a local variable, and it has not been seen in any
	     outer BIND_EXPR, then it's probably the result of a duplicate
	     declaration, for which we've already issued an error.  It would
	     be really nice if the front end wouldn't leak these at all.
	     Currently the only known culprit is C++ destructors, as seen
	     in g++.old-deja/g++.jason/binding.C.  */
	  tmp = *expr_p;
	  if (!TREE_STATIC (tmp) && !DECL_EXTERNAL (tmp)
	      && decl_function_context (tmp) == current_function_decl
	      && !DECL_SEEN_IN_BIND_EXPR_P (tmp))
	    {
	      gcc_assert (errorcount || sorrycount);
	      ret = GS_ERROR;
	      break;
	    }

	  /* If this is a local variable sized decl, it must be accessed
	     indirectly.  Perform that substitution.  */
	  if (DECL_VALUE_EXPR (tmp))
	    {
	      *expr_p = unshare_expr (DECL_VALUE_EXPR (tmp));
	      ret = GS_OK;
	      break;
	    }

	  ret = GS_ALL_DONE;
	  break;

	case SSA_NAME:
	  /* Allow callbacks into the gimplifier during optimization.  */
	  ret = GS_ALL_DONE;
	  break;

	default:
	  switch (TREE_CODE_CLASS (TREE_CODE (*expr_p)))
	    {
	    case tcc_comparison:
	      /* If this is a comparison of objects of aggregate type,
	     	 handle it specially (by converting to a call to
	     	 memcmp).  It would be nice to only have to do this
	     	 for variable-sized objects, but then we'd have to
	     	 allow the same nest of reference nodes we allow for
	     	 MODIFY_EXPR and that's too complex.  */
	      if (!AGGREGATE_TYPE_P (TREE_TYPE (TREE_OPERAND (*expr_p, 1))))
		goto expr_2;
	      ret = gimplify_variable_sized_compare (expr_p);
	      break;

	    /* If *EXPR_P does not need to be special-cased, handle it
	       according to its class.  */
	    case tcc_unary:
	      ret = gimplify_expr (&TREE_OPERAND (*expr_p, 0), pre_p,
				   post_p, is_gimple_val, fb_rvalue);
	      break;

	    case tcc_binary:
	    expr_2:
	      {
		enum gimplify_status r0, r1;

		r0 = gimplify_expr (&TREE_OPERAND (*expr_p, 0), pre_p,
				    post_p, is_gimple_val, fb_rvalue);
		r1 = gimplify_expr (&TREE_OPERAND (*expr_p, 1), pre_p,
				    post_p, is_gimple_val, fb_rvalue);

		ret = MIN (r0, r1);
		break;
	      }

	    case tcc_declaration:
	    case tcc_constant:
	      ret = GS_ALL_DONE;
	      goto dont_recalculate;

	    default:
	      gcc_assert (TREE_CODE (*expr_p) == TRUTH_AND_EXPR
			  || TREE_CODE (*expr_p) == TRUTH_OR_EXPR
			  || TREE_CODE (*expr_p) == TRUTH_XOR_EXPR);
	      goto expr_2;
	    }

	  recalculate_side_effects (*expr_p);
	dont_recalculate:
	  break;
	}

      /* If we replaced *expr_p, gimplify again.  */
      if (ret == GS_OK && (*expr_p == NULL || *expr_p == save_expr))
	ret = GS_ALL_DONE;
    }
  while (ret == GS_OK);

  /* If we encountered an error_mark somewhere nested inside, either
     stub out the statement or propagate the error back out.  */
  if (ret == GS_ERROR)
    {
      if (is_statement)
	*expr_p = NULL;
      goto out;
    }

  /* This was only valid as a return value from the langhook, which
     we handled.  Make sure it doesn't escape from any other context.  */
  gcc_assert (ret != GS_UNHANDLED);

  if (fallback == fb_none && *expr_p && !is_gimple_stmt (*expr_p))
    {
      /* We aren't looking for a value, and we don't have a valid
	 statement.  If it doesn't have side-effects, throw it away.  */
      if (!TREE_SIDE_EFFECTS (*expr_p))
	*expr_p = NULL;
      else if (!TREE_THIS_VOLATILE (*expr_p))
	{
	  /* This is probably a _REF that contains something nested that
	     has side effects.  Recurse through the operands to find it.  */
	  enum tree_code code = TREE_CODE (*expr_p);

	  switch (code)
	    {
	    case COMPONENT_REF:
	    case REALPART_EXPR: case IMAGPART_EXPR:
	      gimplify_expr (&TREE_OPERAND (*expr_p, 0), pre_p, post_p,
			     gimple_test_f, fallback);
	      break;

	    case ARRAY_REF: case ARRAY_RANGE_REF:
	      gimplify_expr (&TREE_OPERAND (*expr_p, 0), pre_p, post_p,
			     gimple_test_f, fallback);
	      gimplify_expr (&TREE_OPERAND (*expr_p, 1), pre_p, post_p,
			     gimple_test_f, fallback);
	      break;

	    default:
	       /* Anything else with side-effects must be converted to
	       	  a valid statement before we get here.  */
	      gcc_unreachable ();
	    }

	  *expr_p = NULL;
	}
      else if (COMPLETE_TYPE_P (TREE_TYPE (*expr_p)))
	{
	  /* Historically, the compiler has treated a bare
	     reference to a volatile lvalue as forcing a load.  */
	  tree tmp = create_tmp_var (TREE_TYPE (*expr_p), "vol");
	  *expr_p = build (MODIFY_EXPR, TREE_TYPE (tmp), tmp, *expr_p);
	}
      else
	/* We can't do anything useful with a volatile reference to
	   incomplete type, so just throw it away.  */
	*expr_p = NULL;
    }

  /* If we are gimplifying at the statement level, we're done.  Tack
     everything together and replace the original statement with the
     gimplified form.  */
  if (fallback == fb_none || is_statement)
    {
      if (internal_pre || internal_post)
	{
	  append_to_statement_list (*expr_p, &internal_pre);
	  append_to_statement_list (internal_post, &internal_pre);
	  annotate_all_with_locus (&internal_pre, input_location);
	  *expr_p = internal_pre;
	}
      else if (!*expr_p)
	;
      else if (TREE_CODE (*expr_p) == STATEMENT_LIST)
	annotate_all_with_locus (expr_p, input_location);
      else
	annotate_one_with_locus (*expr_p, input_location);
      goto out;
    }

  /* Otherwise we're gimplifying a subexpression, so the resulting value is
     interesting.  */

  /* If it's sufficiently simple already, we're done.  Unless we are
     handling some post-effects internally; if that's the case, we need to
     copy into a temp before adding the post-effects to the tree.  */
  if (!internal_post && (*gimple_test_f) (*expr_p))
    goto out;

  /* Otherwise, we need to create a new temporary for the gimplified
     expression.  */

  /* We can't return an lvalue if we have an internal postqueue.  The
     object the lvalue refers to would (probably) be modified by the
     postqueue; we need to copy the value out first, which means an
     rvalue.  */
  if ((fallback & fb_lvalue) && !internal_post
      && is_gimple_addressable (*expr_p))
    {
      /* An lvalue will do.  Take the address of the expression, store it
	 in a temporary, and replace the expression with an INDIRECT_REF of
	 that temporary.  */
      tmp = build_fold_addr_expr (*expr_p);
      gimplify_expr (&tmp, pre_p, post_p, is_gimple_reg, fb_rvalue);
      *expr_p = build1 (INDIRECT_REF, TREE_TYPE (TREE_TYPE (tmp)), tmp);
    }
  else if ((fallback & fb_rvalue) && is_gimple_formal_tmp_rhs (*expr_p))
    {
      gcc_assert (!VOID_TYPE_P (TREE_TYPE (*expr_p)));

      /* An rvalue will do.  Assign the gimplified expression into a new
	 temporary TMP and replace the original expression with TMP.  */

      if (internal_post || (fallback & fb_lvalue))
	/* The postqueue might change the value of the expression between
	   the initialization and use of the temporary, so we can't use a
	   formal temp.  FIXME do we care?  */
	*expr_p = get_initialized_tmp_var (*expr_p, pre_p, post_p);
      else
	*expr_p = get_formal_tmp_var (*expr_p, pre_p);

      if (TREE_CODE (*expr_p) != SSA_NAME)
	DECL_GIMPLE_FORMAL_TEMP_P (*expr_p) = 1;
    }
  else
    {
#ifdef ENABLE_CHECKING
      if (!(fallback & fb_mayfail))
	{
	  fprintf (stderr, "gimplification failed:\n");
	  print_generic_expr (stderr, *expr_p, 0);
	  debug_tree (*expr_p);
	  internal_error ("gimplification failed");
	}
#endif
      gcc_assert (fallback & fb_mayfail);
      /* If this is an asm statement, and the user asked for the
	 impossible, don't abort.  Fail and let gimplify_asm_expr
	 issue an error.  */
      ret = GS_ERROR;
      goto out;
    }

  /* Make sure the temporary matches our predicate.  */
  gcc_assert ((*gimple_test_f) (*expr_p));

  if (internal_post)
    {
      annotate_all_with_locus (&internal_post, input_location);
      append_to_statement_list (internal_post, pre_p);
    }

 out:
  input_location = saved_location;
  return ret;
}

/* Look through TYPE for variable-sized objects and gimplify each such
   size that we find.  Add to LIST_P any statements generated.  */

void
gimplify_type_sizes (tree type, tree *list_p)
{
  tree field;

  switch (TREE_CODE (type))
    {
    case ERROR_MARK:
      return;

    case INTEGER_TYPE:
    case ENUMERAL_TYPE:
    case BOOLEAN_TYPE:
    case CHAR_TYPE:
    case REAL_TYPE:
      gimplify_one_sizepos (&TYPE_MIN_VALUE (type), list_p);
      gimplify_one_sizepos (&TYPE_MAX_VALUE (type), list_p);
      break;

    case ARRAY_TYPE:
      /* These anonymous types don't have declarations, so handle them here.  */
      gimplify_type_sizes (TYPE_DOMAIN (type), list_p);
      break;

    case RECORD_TYPE:
    case UNION_TYPE:
    case QUAL_UNION_TYPE:
      for (field = TYPE_FIELDS (type); field; field = TREE_CHAIN (field))
	if (TREE_CODE (field) == FIELD_DECL)
	  gimplify_one_sizepos (&DECL_FIELD_OFFSET (field), list_p);
      break;

    default:
      break;
    }

  gimplify_one_sizepos (&TYPE_SIZE (type), list_p);
  gimplify_one_sizepos (&TYPE_SIZE_UNIT (type), list_p);
}

/* Subroutine of the above to gimplify one size or position, *EXPR_P.
   We add any required statements to STMT_P.  */

void
gimplify_one_sizepos (tree *expr_p, tree *stmt_p)
{
  /* We don't do anything if the value isn't there, is constant, or contains
     A PLACEHOLDER_EXPR.  We also don't want to do anything if it's already
     a VAR_DECL.  If it's a VAR_DECL from another function, the gimplfier
     will want to replace it with a new variable, but that will cause problems
     if this type is from outside the function.  It's OK to have that here.  */
  if (*expr_p == NULL_TREE || TREE_CONSTANT (*expr_p)
      || TREE_CODE (*expr_p) == VAR_DECL
      || CONTAINS_PLACEHOLDER_P (*expr_p))
    return;

  gimplify_expr (expr_p, stmt_p, NULL, is_gimple_val, fb_rvalue);
}

#ifdef ENABLE_CHECKING
/* Compare types A and B for a "close enough" match.  */

static bool
cpt_same_type (tree a, tree b)
{
  if (lang_hooks.types_compatible_p (a, b))
    return true;

  /* ??? The C++ FE decomposes METHOD_TYPES to FUNCTION_TYPES and doesn't
     link them together.  This routine is intended to catch type errors
     that will affect the optimizers, and the optimizers don't add new
     dereferences of function pointers, so ignore it.  */
  if ((TREE_CODE (a) == FUNCTION_TYPE || TREE_CODE (a) == METHOD_TYPE)
      && (TREE_CODE (b) == FUNCTION_TYPE || TREE_CODE (b) == METHOD_TYPE))
    return true;

  /* ??? The C FE pushes type qualifiers after the fact into the type of
     the element from the type of the array.  See build_unary_op's handling
     of ADDR_EXPR.  This seems wrong -- if we were going to do this, we
     should have done it when creating the variable in the first place.
     Alternately, why aren't the two array types made variants?  */
  if (TREE_CODE (a) == ARRAY_TYPE && TREE_CODE (b) == ARRAY_TYPE)
    return cpt_same_type (TREE_TYPE (a), TREE_TYPE (b));

  /* And because of those, we have to recurse down through pointers.  */
  if (POINTER_TYPE_P (a) && POINTER_TYPE_P (b))
    return cpt_same_type (TREE_TYPE (a), TREE_TYPE (b));

  return false;
}

/* Check for some cases of the front end missing cast expressions.
   The type of a dereference should correspond to the pointer type;
   similarly the type of an address should match its object.  */

static tree
check_pointer_types_r (tree *tp, int *walk_subtrees ATTRIBUTE_UNUSED,
		       void *data ATTRIBUTE_UNUSED)
{
  tree t = *tp;
  tree ptype, otype, dtype;

  switch (TREE_CODE (t))
    {
    case INDIRECT_REF:
    case ARRAY_REF:
      otype = TREE_TYPE (t);
      ptype = TREE_TYPE (TREE_OPERAND (t, 0));
      dtype = TREE_TYPE (ptype);
      gcc_assert (cpt_same_type (otype, dtype));
      break;

    case ADDR_EXPR:
      ptype = TREE_TYPE (t);
      otype = TREE_TYPE (TREE_OPERAND (t, 0));
      dtype = TREE_TYPE (ptype);
      if (!cpt_same_type (otype, dtype))
	{
	  /* &array is allowed to produce a pointer to the element, rather than
	     a pointer to the array type.  We must allow this in order to
	     properly represent assigning the address of an array in C into
	     pointer to the element type.  */
	  gcc_assert (TREE_CODE (otype) == ARRAY_TYPE
		      && POINTER_TYPE_P (ptype)
		      && cpt_same_type (TREE_TYPE (otype), dtype));
	  break;
	}
      break;

    default:
      return NULL_TREE;
    }


  return NULL_TREE;
}
#endif

/* Gimplify the body of statements pointed by BODY_P.  FNDECL is the
   function decl containing BODY.  */

void
gimplify_body (tree *body_p, tree fndecl)
{
  location_t saved_location = input_location;
  tree body;

  timevar_push (TV_TREE_GIMPLIFY);
  push_gimplify_context ();

  /* Unshare most shared trees in the body and in that of any nested functions.
     It would seem we don't have to do this for nested functions because
     they are supposed to be output and then the outer function gimplified
     first, but the g++ front end doesn't always do it that way.  */
  unshare_body (body_p, fndecl);
  unvisit_body (body_p, fndecl);

  /* Make sure input_location isn't set to something wierd.  */
  input_location = DECL_SOURCE_LOCATION (fndecl);

  /* Gimplify the function's body.  */
  gimplify_stmt (body_p);
  body = *body_p;

  /* Unshare again, in case gimplification was sloppy.  */
  unshare_all_trees (body);

  if (!body)
    body = alloc_stmt_list ();
  else if (TREE_CODE (body) == STATEMENT_LIST)
    {
      tree t = expr_only (*body_p);
      if (t)
	body = t;
    }

  /* If there isn't an outer BIND_EXPR, add one.  */
  if (TREE_CODE (body) != BIND_EXPR)
    {
      tree b = build (BIND_EXPR, void_type_node, NULL_TREE,
		      NULL_TREE, NULL_TREE);
      TREE_SIDE_EFFECTS (b) = 1;
      append_to_statement_list_force (body, &BIND_EXPR_BODY (b));
      body = b;
    }
  *body_p = body;

  pop_gimplify_context (body);

#ifdef ENABLE_CHECKING
  walk_tree (body_p, check_pointer_types_r, NULL, NULL);
#endif

  timevar_pop (TV_TREE_GIMPLIFY);
  input_location = saved_location;
}

/* Entry point to the gimplification pass.  FNDECL is the FUNCTION_DECL
   node for the function we want to gimplify.  */

void
gimplify_function_tree (tree fndecl)
{
  tree oldfn;

  oldfn = current_function_decl;
  current_function_decl = fndecl;

  gimplify_body (&DECL_SAVED_TREE (fndecl), fndecl);

  /* If we're instrumenting function entry/exit, then prepend the call to
     the entry hook and wrap the whole function in a TRY_FINALLY_EXPR to
     catch the exit hook.  */
  /* ??? Add some way to ignore exceptions for this TFE.  */
  if (flag_instrument_function_entry_exit
      && ! DECL_NO_INSTRUMENT_FUNCTION_ENTRY_EXIT (fndecl))
    {
      tree tf, x, bind;

      tf = build (TRY_FINALLY_EXPR, void_type_node, NULL, NULL);
      TREE_SIDE_EFFECTS (tf) = 1;
      x = DECL_SAVED_TREE (fndecl);
      append_to_statement_list (x, &TREE_OPERAND (tf, 0));
      x = implicit_built_in_decls[BUILT_IN_PROFILE_FUNC_EXIT];
      x = build_function_call_expr (x, NULL);
      append_to_statement_list (x, &TREE_OPERAND (tf, 1));

      bind = build (BIND_EXPR, void_type_node, NULL, NULL, NULL);
      TREE_SIDE_EFFECTS (bind) = 1;
      x = implicit_built_in_decls[BUILT_IN_PROFILE_FUNC_ENTER];
      x = build_function_call_expr (x, NULL);
      append_to_statement_list (x, &BIND_EXPR_BODY (bind));
      append_to_statement_list (tf, &BIND_EXPR_BODY (bind));

      DECL_SAVED_TREE (fndecl) = bind;
    }

  current_function_decl = oldfn;
}


/* Expands EXPR to list of gimple statements STMTS.  If SIMPLE is true,
   force the result to be either ssa_name or an invariant, otherwise
   just force it to be a rhs expression.  If VAR is not NULL, make the
   base variable of the final destination be VAR if suitable.  */

tree
force_gimple_operand (tree expr, tree *stmts, bool simple, tree var)
{
  tree t;
  enum gimplify_status ret;
  gimple_predicate gimple_test_f;

  *stmts = NULL_TREE;

  if (is_gimple_val (expr))
    return expr;

  gimple_test_f = simple ? is_gimple_val : is_gimple_reg_rhs;

  push_gimplify_context ();
  gimplify_ctxp->into_ssa = true;

  if (var)
    expr = build (MODIFY_EXPR, TREE_TYPE (var), var, expr);

  ret = gimplify_expr (&expr, stmts, NULL,
		       gimple_test_f, fb_rvalue);
  gcc_assert (ret != GS_ERROR);

  for (t = gimplify_ctxp->temps; t ; t = TREE_CHAIN (t))
    add_referenced_tmp_var (t);

  pop_gimplify_context (NULL);

  return expr;
}

#include "gt-gimplify.h"
