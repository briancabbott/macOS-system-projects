/* Chains of recurrences.
   Copyright (C) 2003, 2004, 2005 Free Software Foundation, Inc.
   Contributed by Sebastian Pop <s.pop@laposte.net>

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

/* This file implements operations on chains of recurrences.  Chains
   of recurrences are used for modeling evolution functions of scalar
   variables.
*/

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "errors.h"
#include "ggc.h"
#include "tree.h"
#include "diagnostic.h"
#include "varray.h"
#include "tree-chrec.h"
#include "tree-pass.h"



/* Extended folder for chrecs.  */

/* Determines whether CST is not a constant evolution.  */

static inline bool
is_not_constant_evolution (tree cst)
{
  return (TREE_CODE (cst) == POLYNOMIAL_CHREC);
}

/* Fold CODE for a polynomial function and a constant.  */

static inline tree 
chrec_fold_poly_cst (enum tree_code code, 
		     tree type, 
		     tree poly, 
		     tree cst)
{
  gcc_assert (poly);
  gcc_assert (cst);
  gcc_assert (TREE_CODE (poly) == POLYNOMIAL_CHREC);
  gcc_assert (!is_not_constant_evolution (cst));
  
  switch (code)
    {
    case PLUS_EXPR:
      return build_polynomial_chrec 
	(CHREC_VARIABLE (poly), 
	 chrec_fold_plus (type, CHREC_LEFT (poly), cst),
	 CHREC_RIGHT (poly));
      
    case MINUS_EXPR:
      return build_polynomial_chrec 
	(CHREC_VARIABLE (poly), 
	 chrec_fold_minus (type, CHREC_LEFT (poly), cst),
	 CHREC_RIGHT (poly));
      
    case MULT_EXPR:
      return build_polynomial_chrec 
	(CHREC_VARIABLE (poly), 
	 chrec_fold_multiply (type, CHREC_LEFT (poly), cst),
	 chrec_fold_multiply (type, CHREC_RIGHT (poly), cst));
      
    default:
      return chrec_dont_know;
    }
}

/* Fold the addition of two polynomial functions.  */

static inline tree 
chrec_fold_plus_poly_poly (enum tree_code code, 
			   tree type, 
			   tree poly0, 
			   tree poly1)
{
  tree left, right;

  gcc_assert (poly0);
  gcc_assert (poly1);
  gcc_assert (TREE_CODE (poly0) == POLYNOMIAL_CHREC);
  gcc_assert (TREE_CODE (poly1) == POLYNOMIAL_CHREC);
  
  /*
    {a, +, b}_1 + {c, +, d}_2  ->  {{a, +, b}_1 + c, +, d}_2,
    {a, +, b}_2 + {c, +, d}_1  ->  {{c, +, d}_1 + a, +, b}_2,
    {a, +, b}_x + {c, +, d}_x  ->  {a+c, +, b+d}_x.  */
  if (CHREC_VARIABLE (poly0) < CHREC_VARIABLE (poly1))
    {
      if (code == PLUS_EXPR)
	return build_polynomial_chrec 
	  (CHREC_VARIABLE (poly1), 
	   chrec_fold_plus (type, poly0, CHREC_LEFT (poly1)),
	   CHREC_RIGHT (poly1));
      else
	return build_polynomial_chrec 
	  (CHREC_VARIABLE (poly1), 
	   chrec_fold_minus (type, poly0, CHREC_LEFT (poly1)),
	   chrec_fold_multiply (type, CHREC_RIGHT (poly1), 
				build_int_cst_type (type, -1)));
    }
  
  if (CHREC_VARIABLE (poly0) > CHREC_VARIABLE (poly1))
    {
      if (code == PLUS_EXPR)
	return build_polynomial_chrec 
	  (CHREC_VARIABLE (poly0), 
	   chrec_fold_plus (type, CHREC_LEFT (poly0), poly1),
	   CHREC_RIGHT (poly0));
      else
	return build_polynomial_chrec 
	  (CHREC_VARIABLE (poly0), 
	   chrec_fold_minus (type, CHREC_LEFT (poly0), poly1),
	   CHREC_RIGHT (poly0));
    }
  
  if (code == PLUS_EXPR)
    {
      left = chrec_fold_plus 
	(type, CHREC_LEFT (poly0), CHREC_LEFT (poly1));
      right = chrec_fold_plus 
	(type, CHREC_RIGHT (poly0), CHREC_RIGHT (poly1));
    }
  else
    {
      left = chrec_fold_minus 
	(type, CHREC_LEFT (poly0), CHREC_LEFT (poly1));
      right = chrec_fold_minus 
	(type, CHREC_RIGHT (poly0), CHREC_RIGHT (poly1));
    }

  if (chrec_zerop (right))
    return left;
  else
    return build_polynomial_chrec 
      (CHREC_VARIABLE (poly0), left, right); 
}



/* Fold the multiplication of two polynomial functions.  */

static inline tree 
chrec_fold_multiply_poly_poly (tree type, 
			       tree poly0, 
			       tree poly1)
{
  gcc_assert (poly0);
  gcc_assert (poly1);
  gcc_assert (TREE_CODE (poly0) == POLYNOMIAL_CHREC);
  gcc_assert (TREE_CODE (poly1) == POLYNOMIAL_CHREC);
  
  /* {a, +, b}_1 * {c, +, d}_2  ->  {c*{a, +, b}_1, +, d}_2,
     {a, +, b}_2 * {c, +, d}_1  ->  {a*{c, +, d}_1, +, b}_2,
     {a, +, b}_x * {c, +, d}_x  ->  {a*c, +, a*d + b*c + b*d, +, 2*b*d}_x.  */
  if (CHREC_VARIABLE (poly0) < CHREC_VARIABLE (poly1))
    /* poly0 is a constant wrt. poly1.  */
    return build_polynomial_chrec 
      (CHREC_VARIABLE (poly1), 
       chrec_fold_multiply (type, CHREC_LEFT (poly1), poly0),
       CHREC_RIGHT (poly1));
  
  if (CHREC_VARIABLE (poly1) < CHREC_VARIABLE (poly0))
    /* poly1 is a constant wrt. poly0.  */
    return build_polynomial_chrec 
      (CHREC_VARIABLE (poly0), 
       chrec_fold_multiply (type, CHREC_LEFT (poly0), poly1),
       CHREC_RIGHT (poly0));
  
  /* poly0 and poly1 are two polynomials in the same variable,
     {a, +, b}_x * {c, +, d}_x  ->  {a*c, +, a*d + b*c + b*d, +, 2*b*d}_x.  */
  return 
    build_polynomial_chrec 
    (CHREC_VARIABLE (poly0), 
     build_polynomial_chrec 
     (CHREC_VARIABLE (poly0), 
      
      /* "a*c".  */
      chrec_fold_multiply (type, CHREC_LEFT (poly0), CHREC_LEFT (poly1)),
      
      /* "a*d + b*c + b*d".  */
      chrec_fold_plus 
      (type, chrec_fold_multiply (type, CHREC_LEFT (poly0), CHREC_RIGHT (poly1)),
       
       chrec_fold_plus 
       (type, 
	chrec_fold_multiply (type, CHREC_RIGHT (poly0), CHREC_LEFT (poly1)),
	chrec_fold_multiply (type, CHREC_RIGHT (poly0), CHREC_RIGHT (poly1))))),
     
     /* "2*b*d".  */
     chrec_fold_multiply
     (type, build_int_cst (NULL_TREE, 2),
      chrec_fold_multiply (type, CHREC_RIGHT (poly0), CHREC_RIGHT (poly1))));
}

/* When the operands are automatically_generated_chrec_p, the fold has
   to respect the semantics of the operands.  */

static inline tree 
chrec_fold_automatically_generated_operands (tree op0, 
					     tree op1)
{
  if (op0 == chrec_dont_know
      || op1 == chrec_dont_know)
    return chrec_dont_know;
  
  if (op0 == chrec_known
      || op1 == chrec_known)
    return chrec_known;
  
  if (op0 == chrec_not_analyzed_yet
      || op1 == chrec_not_analyzed_yet)
    return chrec_not_analyzed_yet;
  
  /* The default case produces a safe result.  */
  return chrec_dont_know;
}

/* Fold the addition of two chrecs.  */

static tree
chrec_fold_plus_1 (enum tree_code code, 
		   tree type, 
		   tree op0,
		   tree op1)
{
  if (automatically_generated_chrec_p (op0)
      || automatically_generated_chrec_p (op1))
    return chrec_fold_automatically_generated_operands (op0, op1);
  
  switch (TREE_CODE (op0))
    {
    case POLYNOMIAL_CHREC:
      switch (TREE_CODE (op1))
	{
	case POLYNOMIAL_CHREC:
	  return chrec_fold_plus_poly_poly (code, type, op0, op1);

	default:
	  if (code == PLUS_EXPR)
	    return build_polynomial_chrec 
	      (CHREC_VARIABLE (op0), 
	       chrec_fold_plus (type, CHREC_LEFT (op0), op1),
	       CHREC_RIGHT (op0));
	  else
	    return build_polynomial_chrec 
	      (CHREC_VARIABLE (op0), 
	       chrec_fold_minus (type, CHREC_LEFT (op0), op1),
	       CHREC_RIGHT (op0));
	}

    default:
      switch (TREE_CODE (op1))
	{
	case POLYNOMIAL_CHREC:
	  if (code == PLUS_EXPR)
	    return build_polynomial_chrec 
	      (CHREC_VARIABLE (op1), 
	       chrec_fold_plus (type, op0, CHREC_LEFT (op1)),
	       CHREC_RIGHT (op1));
	  else
	    return build_polynomial_chrec 
	      (CHREC_VARIABLE (op1), 
	       chrec_fold_minus (type, op0, CHREC_LEFT (op1)),
	       chrec_fold_multiply (type, CHREC_RIGHT (op1),
				    build_int_cst_type (type, -1)));

	default:
	  if (tree_contains_chrecs (op0)
	      || tree_contains_chrecs (op1))
	    return build (code, type, op0, op1);
	  else
	    return fold (build (code, type, op0, op1));
	}
    }
}

/* Fold the addition of two chrecs.  */

tree
chrec_fold_plus (tree type, 
		 tree op0,
		 tree op1)
{
  if (integer_zerop (op0))
    return op1;
  if (integer_zerop (op1))
    return op0;
  
  return chrec_fold_plus_1 (PLUS_EXPR, type, op0, op1);
}

/* Fold the subtraction of two chrecs.  */

tree 
chrec_fold_minus (tree type, 
		  tree op0, 
		  tree op1)
{
  if (integer_zerop (op1))
    return op0;
  
  return chrec_fold_plus_1 (MINUS_EXPR, type, op0, op1);
}

/* Fold the multiplication of two chrecs.  */

tree
chrec_fold_multiply (tree type, 
		     tree op0,
		     tree op1)
{
  if (automatically_generated_chrec_p (op0)
      || automatically_generated_chrec_p (op1))
    return chrec_fold_automatically_generated_operands (op0, op1);
  
  switch (TREE_CODE (op0))
    {
    case POLYNOMIAL_CHREC:
      switch (TREE_CODE (op1))
	{
	case POLYNOMIAL_CHREC:
	  return chrec_fold_multiply_poly_poly (type, op0, op1);
	  
	default:
	  if (integer_onep (op1))
	    return op0;
	  if (integer_zerop (op1))
	    return build_int_cst_type (type, 0);
	  
	  return build_polynomial_chrec 
	    (CHREC_VARIABLE (op0), 
	     chrec_fold_multiply (type, CHREC_LEFT (op0), op1),
	     chrec_fold_multiply (type, CHREC_RIGHT (op0), op1));
	}
      
    default:
      if (integer_onep (op0))
	return op1;
      
      if (integer_zerop (op0))
    	return build_int_cst_type (type, 0);
      
      switch (TREE_CODE (op1))
	{
	case POLYNOMIAL_CHREC:
	  return build_polynomial_chrec 
	    (CHREC_VARIABLE (op1), 
	     chrec_fold_multiply (type, CHREC_LEFT (op1), op0),
	     chrec_fold_multiply (type, CHREC_RIGHT (op1), op0));
	  
	default:
	  if (integer_onep (op1))
	    return op0;
	  if (integer_zerop (op1))
	    return build_int_cst_type (type, 0);
	  return fold (build (MULT_EXPR, type, op0, op1));
	}
    }
}



/* Operations.  */

/* Evaluate the binomial coefficient.  Return NULL_TREE if the intermediate
   calculation overflows, otherwise return C(n,k) with type TYPE.  */
static tree
tree_fold_binomial (tree type, tree n, unsigned int k)
{
  unsigned HOST_WIDE_INT lidx, lnum, ldenom, lres, ldum;
  HOST_WIDE_INT hidx, hnum, hdenom, hres, hdum;
  unsigned int i;
  tree res;

  /* Handle the most frequent cases.  */
  if (k == 0)
    return build_int_cst (type, 1);
  if (k == 1)
    return fold_convert (type, n);

  /* Check that k <= n.  */
  if (TREE_INT_CST_HIGH (n) == 0
      && TREE_INT_CST_LOW (n) < k)
    return NULL_TREE;

  /* Numerator = n.  */
  lnum = TREE_INT_CST_LOW (n);
  hnum = TREE_INT_CST_HIGH (n);

  /* Denominator = 2.  */
  ldenom = 2;
  hdenom = 0;

  /* Index = Numerator-1.  */
  if (lnum == 0)
    {
      hidx = hnum - 1;
      lidx = ~ (unsigned HOST_WIDE_INT) 0;
    }
  else
    {
      hidx = hnum;
      lidx = lnum - 1;
    }

  /* Numerator = Numerator*Index = n*(n-1).  */
  if (mul_double (lnum, hnum, lidx, hidx, &lnum, &hnum))
    return NULL_TREE;
  /* Numerator = Numerator*Index = n*(n-1).  */
  if (mul_double (lnum, hnum, lidx, hidx, &lnum, &hnum))
    return NULL_TREE;

  for (i = 3; i <= k; i++)
    {
      /* Index--.  */
      if (lidx == 0)
      {
        hidx--;
        lidx = ~ (unsigned HOST_WIDE_INT) 0;
      }
      else
        lidx--;

      /* Numerator *= Index.  */
      if (mul_double (lnum, hnum, lidx, hidx, &lnum, &hnum))
      return NULL_TREE;

      /* Denominator *= i.  */
      mul_double (ldenom, hdenom, i, 0, &ldenom, &hdenom);
    }

  /* Result = Numerator / Denominator.  */
  div_and_round_double (EXACT_DIV_EXPR, 1, lnum, hnum, ldenom, hdenom,
                        &lres, &hres, &ldum, &hdum);

  res = build_int_cst_wide (type, lres, hres);
  return int_fits_type_p (res, type) ? res : NULL_TREE;
}


/* Helper function.  Use the Newton's interpolating formula for
   evaluating the value of the evolution function.  */

static tree
chrec_evaluate (unsigned var, tree chrec, tree n, unsigned int k)
{
  tree arg0, arg1, binomial_n_k;
  tree type = TREE_TYPE (chrec);

  while (TREE_CODE (chrec) == POLYNOMIAL_CHREC
       && CHREC_VARIABLE (chrec) > var)
    chrec = CHREC_LEFT (chrec);
 
  if (TREE_CODE (chrec) == POLYNOMIAL_CHREC
      && CHREC_VARIABLE (chrec) == var)
    {
      arg0 = chrec_evaluate (var, CHREC_RIGHT (chrec), n, k + 1);
      if (arg0 == chrec_dont_know)
        return chrec_dont_know;
      binomial_n_k = tree_fold_binomial (type, n, k);
      if (!binomial_n_k)
        return chrec_dont_know;
      arg1 = fold (build2 (MULT_EXPR, type,
                         CHREC_LEFT (chrec), binomial_n_k));
      return chrec_fold_plus (type, arg0, arg1);
    }

  binomial_n_k = tree_fold_binomial (type, n, k);
  if (!binomial_n_k)
    return chrec_dont_know;
 
  return fold (build2 (MULT_EXPR, type, chrec, binomial_n_k));
}

/* Evaluates "CHREC (X)" when the varying variable is VAR.  
   Example:  Given the following parameters, 
   
   var = 1
   chrec = {3, +, 4}_1
   x = 10
   
   The result is given by the Newton's interpolating formula: 
   3 * \binom{10}{0} + 4 * \binom{10}{1}.
*/

tree 
chrec_apply (unsigned var,
	     tree chrec, 
	     tree x)
{
  tree type = chrec_type (chrec);
  tree res = chrec_dont_know;

  if (automatically_generated_chrec_p (chrec)
      || automatically_generated_chrec_p (x)

      /* When the symbols are defined in an outer loop, it is possible
	 to symbolically compute the apply, since the symbols are
	 constants with respect to the varying loop.  */
      || chrec_contains_symbols_defined_in_loop (chrec, var)
      || chrec_contains_symbols (x))
    return chrec_dont_know;
  
  if (dump_file && (dump_flags & TDF_DETAILS))
    fprintf (dump_file, "(chrec_apply \n");

  if (evolution_function_is_affine_p (chrec))
    {
      /* "{a, +, b} (x)"  ->  "a + b*x".  */
      if (TREE_CODE (CHREC_LEFT (chrec)) == INTEGER_CST
	  && integer_zerop (CHREC_LEFT (chrec)))
	res = chrec_fold_multiply (type, CHREC_RIGHT (chrec), x);
      
      else
	res = chrec_fold_plus (type, CHREC_LEFT (chrec), 
			       chrec_fold_multiply (type, 
						    CHREC_RIGHT (chrec), x));
    }
  
  else if (TREE_CODE (chrec) != POLYNOMIAL_CHREC)
    res = chrec;
  
  else if (TREE_CODE (x) == INTEGER_CST
	   && tree_int_cst_sgn (x) == 1)
    /* testsuite/.../ssa-chrec-38.c.  */
    res = chrec_evaluate (var, chrec, x, 0);

  else
    res = chrec_dont_know;
  
  if (dump_file && (dump_flags & TDF_DETAILS))
    {
      fprintf (dump_file, "  (varying_loop = %d\n", var);
      fprintf (dump_file, ")\n  (chrec = ");
      print_generic_expr (dump_file, chrec, 0);
      fprintf (dump_file, ")\n  (x = ");
      print_generic_expr (dump_file, x, 0);
      fprintf (dump_file, ")\n  (res = ");
      print_generic_expr (dump_file, res, 0);
      fprintf (dump_file, "))\n");
    }
  
  return res;
}

/* Replaces the initial condition in CHREC with INIT_COND.  */

tree 
chrec_replace_initial_condition (tree chrec, 
				 tree init_cond)
{
  if (automatically_generated_chrec_p (chrec))
    return chrec;
  
  switch (TREE_CODE (chrec))
    {
    case POLYNOMIAL_CHREC:
      return build_polynomial_chrec 
	(CHREC_VARIABLE (chrec),
	 chrec_replace_initial_condition (CHREC_LEFT (chrec), init_cond),
	 CHREC_RIGHT (chrec));
      
    default:
      return init_cond;
    }
}

/* Returns the initial condition of a given CHREC.  */

tree 
initial_condition (tree chrec)
{
  if (automatically_generated_chrec_p (chrec))
    return chrec;
  
  if (TREE_CODE (chrec) == POLYNOMIAL_CHREC)
    return initial_condition (CHREC_LEFT (chrec));
  else
    return chrec;
}

/* Returns a univariate function that represents the evolution in
   LOOP_NUM.  Mask the evolution of any other loop.  */

tree 
hide_evolution_in_other_loops_than_loop (tree chrec, 
					 unsigned loop_num)
{
  if (automatically_generated_chrec_p (chrec))
    return chrec;
  
  switch (TREE_CODE (chrec))
    {
    case POLYNOMIAL_CHREC:
      if (CHREC_VARIABLE (chrec) == loop_num)
	return build_polynomial_chrec 
	  (loop_num, 
	   hide_evolution_in_other_loops_than_loop (CHREC_LEFT (chrec), 
						    loop_num), 
	   CHREC_RIGHT (chrec));
      
      else if (CHREC_VARIABLE (chrec) < loop_num)
	/* There is no evolution in this loop.  */
	return initial_condition (chrec);
      
      else
	return hide_evolution_in_other_loops_than_loop (CHREC_LEFT (chrec), 
							loop_num);
      
    default:
      return chrec;
    }
}

/* Returns the evolution part of CHREC in LOOP_NUM when RIGHT is
   true, otherwise returns the initial condition in LOOP_NUM.  */

static tree 
chrec_component_in_loop_num (tree chrec, 
			     unsigned loop_num,
			     bool right)
{
  tree component;

  if (automatically_generated_chrec_p (chrec))
    return chrec;
  
  switch (TREE_CODE (chrec))
    {
    case POLYNOMIAL_CHREC:
      if (CHREC_VARIABLE (chrec) == loop_num)
	{
	  if (right)
	    component = CHREC_RIGHT (chrec);
	  else
	    component = CHREC_LEFT (chrec);

	  if (TREE_CODE (CHREC_LEFT (chrec)) != POLYNOMIAL_CHREC
	      || CHREC_VARIABLE (CHREC_LEFT (chrec)) != CHREC_VARIABLE (chrec))
	    return component;
	  
	  else
	    return build_polynomial_chrec
	      (loop_num, 
	       chrec_component_in_loop_num (CHREC_LEFT (chrec), 
					    loop_num, 
					    right), 
	       component);
	}
      
      else if (CHREC_VARIABLE (chrec) < loop_num)
	/* There is no evolution part in this loop.  */
	return NULL_TREE;
      
      else
	return chrec_component_in_loop_num (CHREC_LEFT (chrec), 
					    loop_num, 
					    right);
      
     default:
      if (right)
	return NULL_TREE;
      else
	return chrec;
    }
}

/* Returns the evolution part in LOOP_NUM.  Example: the call
   evolution_part_in_loop_num ({{0, +, 1}_1, +, 2}_1, 1) returns 
   {1, +, 2}_1  */

tree 
evolution_part_in_loop_num (tree chrec, 
			    unsigned loop_num)
{
  return chrec_component_in_loop_num (chrec, loop_num, true);
}

/* Returns the initial condition in LOOP_NUM.  Example: the call
   initial_condition_in_loop_num ({{0, +, 1}_1, +, 2}_2, 2) returns 
   {0, +, 1}_1  */

tree 
initial_condition_in_loop_num (tree chrec, 
			       unsigned loop_num)
{
  return chrec_component_in_loop_num (chrec, loop_num, false);
}

/* Set or reset the evolution of CHREC to NEW_EVOL in loop LOOP_NUM.
   This function is essentially used for setting the evolution to
   chrec_dont_know, for example after having determined that it is
   impossible to say how many times a loop will execute.  */

tree 
reset_evolution_in_loop (unsigned loop_num,
			 tree chrec, 
			 tree new_evol)
{
  if (TREE_CODE (chrec) == POLYNOMIAL_CHREC
      && CHREC_VARIABLE (chrec) > loop_num)
    return build 
      (TREE_CODE (chrec), 
       build_int_cst (NULL_TREE, CHREC_VARIABLE (chrec)), 
       reset_evolution_in_loop (loop_num, CHREC_LEFT (chrec), new_evol), 
       reset_evolution_in_loop (loop_num, CHREC_RIGHT (chrec), new_evol));
  
  while (TREE_CODE (chrec) == POLYNOMIAL_CHREC
	 && CHREC_VARIABLE (chrec) == loop_num)
    chrec = CHREC_LEFT (chrec);
  
  return build_polynomial_chrec (loop_num, chrec, new_evol);
}

/* Merges two evolution functions that were found by following two
   alternate paths of a conditional expression.  */

tree
chrec_merge (tree chrec1, 
	     tree chrec2)
{
  if (chrec1 == chrec_dont_know
      || chrec2 == chrec_dont_know)
    return chrec_dont_know;

  if (chrec1 == chrec_known 
      || chrec2 == chrec_known)
    return chrec_known;

  if (chrec1 == chrec_not_analyzed_yet)
    return chrec2;
  if (chrec2 == chrec_not_analyzed_yet)
    return chrec1;

  if (operand_equal_p (chrec1, chrec2, 0))
    return chrec1;

  return chrec_dont_know;
}



/* Observers.  */

/* Helper function for is_multivariate_chrec.  */

static bool 
is_multivariate_chrec_rec (tree chrec, unsigned int rec_var)
{
  if (chrec == NULL_TREE)
    return false;
  
  if (TREE_CODE (chrec) == POLYNOMIAL_CHREC)
    {
      if (CHREC_VARIABLE (chrec) != rec_var)
	return true;
      else
	return (is_multivariate_chrec_rec (CHREC_LEFT (chrec), rec_var) 
		|| is_multivariate_chrec_rec (CHREC_RIGHT (chrec), rec_var));
    }
  else
    return false;
}

/* Determine whether the given chrec is multivariate or not.  */

bool 
is_multivariate_chrec (tree chrec)
{
  if (chrec == NULL_TREE)
    return false;
  
  if (TREE_CODE (chrec) == POLYNOMIAL_CHREC)
    return (is_multivariate_chrec_rec (CHREC_LEFT (chrec), 
				       CHREC_VARIABLE (chrec))
	    || is_multivariate_chrec_rec (CHREC_RIGHT (chrec), 
					  CHREC_VARIABLE (chrec)));
  else
    return false;
}

/* Determines whether the chrec contains symbolic names or not.  */

bool 
chrec_contains_symbols (tree chrec)
{
  if (chrec == NULL_TREE)
    return false;
  
  if (TREE_CODE (chrec) == SSA_NAME
      || TREE_CODE (chrec) == VAR_DECL
      || TREE_CODE (chrec) == PARM_DECL
      || TREE_CODE (chrec) == FUNCTION_DECL
      || TREE_CODE (chrec) == LABEL_DECL
      || TREE_CODE (chrec) == RESULT_DECL
      || TREE_CODE (chrec) == FIELD_DECL)
    return true;
  
  switch (TREE_CODE_LENGTH (TREE_CODE (chrec)))
    {
    case 3:
      if (chrec_contains_symbols (TREE_OPERAND (chrec, 2)))
	return true;
      
    case 2:
      if (chrec_contains_symbols (TREE_OPERAND (chrec, 1)))
	return true;
      
    case 1:
      if (chrec_contains_symbols (TREE_OPERAND (chrec, 0)))
	return true;
      
    default:
      return false;
    }
}

/* Determines whether the chrec contains undetermined coefficients.  */

bool 
chrec_contains_undetermined (tree chrec)
{
  if (chrec == chrec_dont_know
      || chrec == chrec_not_analyzed_yet
      || chrec == NULL_TREE)
    return true;
  
  switch (TREE_CODE_LENGTH (TREE_CODE (chrec)))
    {
    case 3:
      if (chrec_contains_undetermined (TREE_OPERAND (chrec, 2)))
	return true;
      
    case 2:
      if (chrec_contains_undetermined (TREE_OPERAND (chrec, 1)))
	return true;
      
    case 1:
      if (chrec_contains_undetermined (TREE_OPERAND (chrec, 0)))
	return true;
      
    default:
      return false;
    }
}

/* Determines whether the tree EXPR contains chrecs.  */

bool
tree_contains_chrecs (tree expr)
{
  if (expr == NULL_TREE)
    return false;
  
  if (tree_is_chrec (expr))
    return true;
  
  switch (TREE_CODE_LENGTH (TREE_CODE (expr)))
    {
    case 3:
      if (tree_contains_chrecs (TREE_OPERAND (expr, 2)))
	return true;
      
    case 2:
      if (tree_contains_chrecs (TREE_OPERAND (expr, 1)))
	return true;
      
    case 1:
      if (tree_contains_chrecs (TREE_OPERAND (expr, 0)))
	return true;
      
    default:
      return false;
    }
}

/* Determine whether the given tree is an affine multivariate
   evolution.  */

bool 
evolution_function_is_affine_multivariate_p (tree chrec)
{
  if (chrec == NULL_TREE)
    return false;
  
  switch (TREE_CODE (chrec))
    {
    case POLYNOMIAL_CHREC:
      if (evolution_function_is_constant_p (CHREC_LEFT (chrec)))
	{
	  if (evolution_function_is_constant_p (CHREC_RIGHT (chrec)))
	    return true;
	  else
	    {
	      if (TREE_CODE (CHREC_RIGHT (chrec)) == POLYNOMIAL_CHREC
		  && CHREC_VARIABLE (CHREC_RIGHT (chrec)) 
		     != CHREC_VARIABLE (chrec)
		  && evolution_function_is_affine_multivariate_p 
		  (CHREC_RIGHT (chrec)))
		return true;
	      else
		return false;
	    }
	}
      else
	{
	  if (evolution_function_is_constant_p (CHREC_RIGHT (chrec))
	      && TREE_CODE (CHREC_LEFT (chrec)) == POLYNOMIAL_CHREC
	      && CHREC_VARIABLE (CHREC_LEFT (chrec)) != CHREC_VARIABLE (chrec)
	      && evolution_function_is_affine_multivariate_p 
	      (CHREC_LEFT (chrec)))
	    return true;
	  else
	    return false;
	}
      
    default:
      return false;
    }
}

/* Determine whether the given tree is a function in zero or one 
   variables.  */

bool
evolution_function_is_univariate_p (tree chrec)
{
  if (chrec == NULL_TREE)
    return true;
  
  switch (TREE_CODE (chrec))
    {
    case POLYNOMIAL_CHREC:
      switch (TREE_CODE (CHREC_LEFT (chrec)))
	{
	case POLYNOMIAL_CHREC:
	  if (CHREC_VARIABLE (chrec) != CHREC_VARIABLE (CHREC_LEFT (chrec)))
	    return false;
	  if (!evolution_function_is_univariate_p (CHREC_LEFT (chrec)))
	    return false;
	  break;
	  
	default:
	  break;
	}
      
      switch (TREE_CODE (CHREC_RIGHT (chrec)))
	{
	case POLYNOMIAL_CHREC:
	  if (CHREC_VARIABLE (chrec) != CHREC_VARIABLE (CHREC_RIGHT (chrec)))
	    return false;
	  if (!evolution_function_is_univariate_p (CHREC_RIGHT (chrec)))
	    return false;
	  break;
	  
	default:
	  break;	  
	}
      
    default:
      return true;
    }
}

/* Returns the number of variables of CHREC.  Example: the call
   nb_vars_in_chrec ({{0, +, 1}_5, +, 2}_6) returns 2.  */

unsigned 
nb_vars_in_chrec (tree chrec)
{
  if (chrec == NULL_TREE)
    return 0;

  switch (TREE_CODE (chrec))
    {
    case POLYNOMIAL_CHREC:
      return 1 + nb_vars_in_chrec 
	(initial_condition_in_loop_num (chrec, CHREC_VARIABLE (chrec)));

    default:
      return 0;
    }
}



/* Convert the initial condition of chrec to type.  */

tree 
chrec_convert (tree type, 
	       tree chrec)
{
  tree ct;
  
  if (automatically_generated_chrec_p (chrec))
    return chrec;
  
  ct = chrec_type (chrec);
  if (ct == type)
    return chrec;

  if (TYPE_PRECISION (ct) < TYPE_PRECISION (type))
    return count_ev_in_wider_type (type, chrec);

  switch (TREE_CODE (chrec))
    {
    case POLYNOMIAL_CHREC:
      return build_polynomial_chrec (CHREC_VARIABLE (chrec),
				     chrec_convert (type,
						    CHREC_LEFT (chrec)),
				     chrec_convert (type,
						    CHREC_RIGHT (chrec)));

    default:
      {
	tree res = convert (type, chrec);

	/* Don't propagate overflows.  */
	TREE_OVERFLOW (res) = 0;
	if (CONSTANT_CLASS_P (res))
	  TREE_CONSTANT_OVERFLOW (res) = 0;
	return res;
      }
    }
}

/* Returns the type of the chrec.  */

tree 
chrec_type (tree chrec)
{
  if (automatically_generated_chrec_p (chrec))
    return NULL_TREE;
  
  return TREE_TYPE (chrec);
}
