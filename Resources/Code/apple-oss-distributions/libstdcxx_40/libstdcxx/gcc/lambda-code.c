/*  Loop transformation code generation
    Copyright (C) 2003, 2004, 2005 Free Software Foundation, Inc.
    Contributed by Daniel Berlin <dberlin@dberlin.org>

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
#include "errors.h"
#include "ggc.h"
#include "tree.h"
#include "target.h"
#include "rtl.h"
#include "basic-block.h"
#include "diagnostic.h"
#include "tree-flow.h"
#include "tree-dump.h"
#include "timevar.h"
#include "cfgloop.h"
#include "expr.h"
#include "optabs.h"
#include "tree-chrec.h"
#include "tree-data-ref.h"
#include "tree-pass.h"
#include "tree-scalar-evolution.h"
#include "vec.h"
#include "lambda.h"

/* This loop nest code generation is based on non-singular matrix
   math.
 
 A little terminology and a general sketch of the algorithm.  See "A singular
 loop transformation framework based on non-singular matrices" by Wei Li and
 Keshav Pingali for formal proofs that the various statements below are
 correct. 

 A loop iteration space represents the points traversed by the loop.  A point in the
 iteration space can be represented by a vector of size <loop depth>.  You can
 therefore represent the iteration space as an integral combinations of a set
 of basis vectors. 

 A loop iteration space is dense if every integer point between the loop
 bounds is a point in the iteration space.  Every loop with a step of 1
 therefore has a dense iteration space.

 for i = 1 to 3, step 1 is a dense iteration space.
   
 A loop iteration space is sparse if it is not dense.  That is, the iteration
 space skips integer points that are within the loop bounds.  

 for i = 1 to 3, step 2 is a sparse iteration space, because the integer point
 2 is skipped.

 Dense source spaces are easy to transform, because they don't skip any
 points to begin with.  Thus we can compute the exact bounds of the target
 space using min/max and floor/ceil.

 For a dense source space, we take the transformation matrix, decompose it
 into a lower triangular part (H) and a unimodular part (U). 
 We then compute the auxiliary space from the unimodular part (source loop
 nest . U = auxiliary space) , which has two important properties:
  1. It traverses the iterations in the same lexicographic order as the source
  space.
  2. It is a dense space when the source is a dense space (even if the target
  space is going to be sparse).
 
 Given the auxiliary space, we use the lower triangular part to compute the
 bounds in the target space by simple matrix multiplication.
 The gaps in the target space (IE the new loop step sizes) will be the
 diagonals of the H matrix.

 Sparse source spaces require another step, because you can't directly compute
 the exact bounds of the auxiliary and target space from the sparse space.
 Rather than try to come up with a separate algorithm to handle sparse source
 spaces directly, we just find a legal transformation matrix that gives you
 the sparse source space, from a dense space, and then transform the dense
 space.

 For a regular sparse space, you can represent the source space as an integer
 lattice, and the base space of that lattice will always be dense.  Thus, we
 effectively use the lattice to figure out the transformation from the lattice
 base space, to the sparse iteration space (IE what transform was applied to
 the dense space to make it sparse).  We then compose this transform with the
 transformation matrix specified by the user (since our matrix transformations
 are closed under composition, this is okay).  We can then use the base space
 (which is dense) plus the composed transformation matrix, to compute the rest
 of the transform using the dense space algorithm above.
 
 In other words, our sparse source space (B) is decomposed into a dense base
 space (A), and a matrix (L) that transforms A into B, such that A.L = B.
 We then compute the composition of L and the user transformation matrix (T),
 so that T is now a transform from A to the result, instead of from B to the
 result. 
 IE A.(LT) = result instead of B.T = result
 Since A is now a dense source space, we can use the dense source space
 algorithm above to compute the result of applying transform (LT) to A.

 Fourier-Motzkin elimination is used to compute the bounds of the base space
 of the lattice.  */


DEF_VEC_GC_P(int);

static bool perfect_nestify (struct loops *, 
			     struct loop *, VEC (tree) *, 
			     VEC (tree) *, VEC (int) *, VEC (tree) *);
/* Lattice stuff that is internal to the code generation algorithm.  */

typedef struct
{
  /* Lattice base matrix.  */
  lambda_matrix base;
  /* Lattice dimension.  */
  int dimension;
  /* Origin vector for the coefficients.  */
  lambda_vector origin;
  /* Origin matrix for the invariants.  */
  lambda_matrix origin_invariants;
  /* Number of invariants.  */
  int invariants;
} *lambda_lattice;

#define LATTICE_BASE(T) ((T)->base)
#define LATTICE_DIMENSION(T) ((T)->dimension)
#define LATTICE_ORIGIN(T) ((T)->origin)
#define LATTICE_ORIGIN_INVARIANTS(T) ((T)->origin_invariants)
#define LATTICE_INVARIANTS(T) ((T)->invariants)

static bool lle_equal (lambda_linear_expression, lambda_linear_expression,
		       int, int);
static lambda_lattice lambda_lattice_new (int, int);
static lambda_lattice lambda_lattice_compute_base (lambda_loopnest);

static tree find_induction_var_from_exit_cond (struct loop *);

/* Create a new lambda body vector.  */

lambda_body_vector
lambda_body_vector_new (int size)
{
  lambda_body_vector ret;

  ret = ggc_alloc (sizeof (*ret));
  LBV_COEFFICIENTS (ret) = lambda_vector_new (size);
  LBV_SIZE (ret) = size;
  LBV_DENOMINATOR (ret) = 1;
  return ret;
}

/* Compute the new coefficients for the vector based on the
  *inverse* of the transformation matrix.  */

lambda_body_vector
lambda_body_vector_compute_new (lambda_trans_matrix transform,
				lambda_body_vector vect)
{
  lambda_body_vector temp;
  int depth;

  /* Make sure the matrix is square.  */
  gcc_assert (LTM_ROWSIZE (transform) == LTM_COLSIZE (transform));

  depth = LTM_ROWSIZE (transform);

  temp = lambda_body_vector_new (depth);
  LBV_DENOMINATOR (temp) =
    LBV_DENOMINATOR (vect) * LTM_DENOMINATOR (transform);
  lambda_vector_matrix_mult (LBV_COEFFICIENTS (vect), depth,
			     LTM_MATRIX (transform), depth,
			     LBV_COEFFICIENTS (temp));
  LBV_SIZE (temp) = LBV_SIZE (vect);
  return temp;
}

/* Print out a lambda body vector.  */

void
print_lambda_body_vector (FILE * outfile, lambda_body_vector body)
{
  print_lambda_vector (outfile, LBV_COEFFICIENTS (body), LBV_SIZE (body));
}

/* Return TRUE if two linear expressions are equal.  */

static bool
lle_equal (lambda_linear_expression lle1, lambda_linear_expression lle2,
	   int depth, int invariants)
{
  int i;

  if (lle1 == NULL || lle2 == NULL)
    return false;
  if (LLE_CONSTANT (lle1) != LLE_CONSTANT (lle2))
    return false;
  if (LLE_DENOMINATOR (lle1) != LLE_DENOMINATOR (lle2))
    return false;
  for (i = 0; i < depth; i++)
    if (LLE_COEFFICIENTS (lle1)[i] != LLE_COEFFICIENTS (lle2)[i])
      return false;
  for (i = 0; i < invariants; i++)
    if (LLE_INVARIANT_COEFFICIENTS (lle1)[i] !=
	LLE_INVARIANT_COEFFICIENTS (lle2)[i])
      return false;
  return true;
}

/* Create a new linear expression with dimension DIM, and total number
   of invariants INVARIANTS.  */

lambda_linear_expression
lambda_linear_expression_new (int dim, int invariants)
{
  lambda_linear_expression ret;

  ret = ggc_alloc_cleared (sizeof (*ret));

  LLE_COEFFICIENTS (ret) = lambda_vector_new (dim);
  LLE_CONSTANT (ret) = 0;
  LLE_INVARIANT_COEFFICIENTS (ret) = lambda_vector_new (invariants);
  LLE_DENOMINATOR (ret) = 1;
  LLE_NEXT (ret) = NULL;

  return ret;
}

/* Print out a linear expression EXPR, with SIZE coefficients, to OUTFILE.
   The starting letter used for variable names is START.  */

static void
print_linear_expression (FILE * outfile, lambda_vector expr, int size,
			 char start)
{
  int i;
  bool first = true;
  for (i = 0; i < size; i++)
    {
      if (expr[i] != 0)
	{
	  if (first)
	    {
	      if (expr[i] < 0)
		fprintf (outfile, "-");
	      first = false;
	    }
	  else if (expr[i] > 0)
	    fprintf (outfile, " + ");
	  else
	    fprintf (outfile, " - ");
	  if (abs (expr[i]) == 1)
	    fprintf (outfile, "%c", start + i);
	  else
	    fprintf (outfile, "%d%c", abs (expr[i]), start + i);
	}
    }
}

/* Print out a lambda linear expression structure, EXPR, to OUTFILE. The
   depth/number of coefficients is given by DEPTH, the number of invariants is
   given by INVARIANTS, and the character to start variable names with is given
   by START.  */

void
print_lambda_linear_expression (FILE * outfile,
				lambda_linear_expression expr,
				int depth, int invariants, char start)
{
  fprintf (outfile, "\tLinear expression: ");
  print_linear_expression (outfile, LLE_COEFFICIENTS (expr), depth, start);
  fprintf (outfile, " constant: %d ", LLE_CONSTANT (expr));
  fprintf (outfile, "  invariants: ");
  print_linear_expression (outfile, LLE_INVARIANT_COEFFICIENTS (expr),
			   invariants, 'A');
  fprintf (outfile, "  denominator: %d\n", LLE_DENOMINATOR (expr));
}

/* Print a lambda loop structure LOOP to OUTFILE.  The depth/number of
   coefficients is given by DEPTH, the number of invariants is 
   given by INVARIANTS, and the character to start variable names with is given
   by START.  */

void
print_lambda_loop (FILE * outfile, lambda_loop loop, int depth,
		   int invariants, char start)
{
  int step;
  lambda_linear_expression expr;

  gcc_assert (loop);

  expr = LL_LINEAR_OFFSET (loop);
  step = LL_STEP (loop);
  fprintf (outfile, "  step size = %d \n", step);

  if (expr)
    {
      fprintf (outfile, "  linear offset: \n");
      print_lambda_linear_expression (outfile, expr, depth, invariants,
				      start);
    }

  fprintf (outfile, "  lower bound: \n");
  for (expr = LL_LOWER_BOUND (loop); expr != NULL; expr = LLE_NEXT (expr))
    print_lambda_linear_expression (outfile, expr, depth, invariants, start);
  fprintf (outfile, "  upper bound: \n");
  for (expr = LL_UPPER_BOUND (loop); expr != NULL; expr = LLE_NEXT (expr))
    print_lambda_linear_expression (outfile, expr, depth, invariants, start);
}

/* Create a new loop nest structure with DEPTH loops, and INVARIANTS as the
   number of invariants.  */

lambda_loopnest
lambda_loopnest_new (int depth, int invariants)
{
  lambda_loopnest ret;
  ret = ggc_alloc (sizeof (*ret));

  LN_LOOPS (ret) = ggc_alloc_cleared (depth * sizeof (lambda_loop));
  LN_DEPTH (ret) = depth;
  LN_INVARIANTS (ret) = invariants;

  return ret;
}

/* Print a lambda loopnest structure, NEST, to OUTFILE.  The starting
   character to use for loop names is given by START.  */

void
print_lambda_loopnest (FILE * outfile, lambda_loopnest nest, char start)
{
  int i;
  for (i = 0; i < LN_DEPTH (nest); i++)
    {
      fprintf (outfile, "Loop %c\n", start + i);
      print_lambda_loop (outfile, LN_LOOPS (nest)[i], LN_DEPTH (nest),
			 LN_INVARIANTS (nest), 'i');
      fprintf (outfile, "\n");
    }
}

/* Allocate a new lattice structure of DEPTH x DEPTH, with INVARIANTS number
   of invariants.  */

static lambda_lattice
lambda_lattice_new (int depth, int invariants)
{
  lambda_lattice ret;
  ret = ggc_alloc (sizeof (*ret));
  LATTICE_BASE (ret) = lambda_matrix_new (depth, depth);
  LATTICE_ORIGIN (ret) = lambda_vector_new (depth);
  LATTICE_ORIGIN_INVARIANTS (ret) = lambda_matrix_new (depth, invariants);
  LATTICE_DIMENSION (ret) = depth;
  LATTICE_INVARIANTS (ret) = invariants;
  return ret;
}

/* Compute the lattice base for NEST.  The lattice base is essentially a
   non-singular transform from a dense base space to a sparse iteration space.
   We use it so that we don't have to specially handle the case of a sparse
   iteration space in other parts of the algorithm.  As a result, this routine
   only does something interesting (IE produce a matrix that isn't the
   identity matrix) if NEST is a sparse space.  */

static lambda_lattice
lambda_lattice_compute_base (lambda_loopnest nest)
{
  lambda_lattice ret;
  int depth, invariants;
  lambda_matrix base;

  int i, j, step;
  lambda_loop loop;
  lambda_linear_expression expression;

  depth = LN_DEPTH (nest);
  invariants = LN_INVARIANTS (nest);

  ret = lambda_lattice_new (depth, invariants);
  base = LATTICE_BASE (ret);
  for (i = 0; i < depth; i++)
    {
      loop = LN_LOOPS (nest)[i];
      gcc_assert (loop);
      step = LL_STEP (loop);
      /* If we have a step of 1, then the base is one, and the
         origin and invariant coefficients are 0.  */
      if (step == 1)
	{
	  for (j = 0; j < depth; j++)
	    base[i][j] = 0;
	  base[i][i] = 1;
	  LATTICE_ORIGIN (ret)[i] = 0;
	  for (j = 0; j < invariants; j++)
	    LATTICE_ORIGIN_INVARIANTS (ret)[i][j] = 0;
	}
      else
	{
	  /* Otherwise, we need the lower bound expression (which must
	     be an affine function)  to determine the base.  */
	  expression = LL_LOWER_BOUND (loop);
	  gcc_assert (expression && !LLE_NEXT (expression) 
		      && LLE_DENOMINATOR (expression) == 1);

	  /* The lower triangular portion of the base is going to be the
	     coefficient times the step */
	  for (j = 0; j < i; j++)
	    base[i][j] = LLE_COEFFICIENTS (expression)[j]
	      * LL_STEP (LN_LOOPS (nest)[j]);
	  base[i][i] = step;
	  for (j = i + 1; j < depth; j++)
	    base[i][j] = 0;

	  /* Origin for this loop is the constant of the lower bound
	     expression.  */
	  LATTICE_ORIGIN (ret)[i] = LLE_CONSTANT (expression);

	  /* Coefficient for the invariants are equal to the invariant
	     coefficients in the expression.  */
	  for (j = 0; j < invariants; j++)
	    LATTICE_ORIGIN_INVARIANTS (ret)[i][j] =
	      LLE_INVARIANT_COEFFICIENTS (expression)[j];
	}
    }
  return ret;
}

/* Compute the greatest common denominator of two numbers (A and B) using
   Euclid's algorithm.  */

static int
gcd (int a, int b)
{

  int x, y, z;

  x = abs (a);
  y = abs (b);

  while (x > 0)
    {
      z = y % x;
      y = x;
      x = z;
    }

  return (y);
}

/* Compute the greatest common denominator of a VECTOR of SIZE numbers.  */

static int
gcd_vector (lambda_vector vector, int size)
{
  int i;
  int gcd1 = 0;

  if (size > 0)
    {
      gcd1 = vector[0];
      for (i = 1; i < size; i++)
	gcd1 = gcd (gcd1, vector[i]);
    }
  return gcd1;
}

/* Compute the least common multiple of two numbers A and B .  */

static int
lcm (int a, int b)
{
  return (abs (a) * abs (b) / gcd (a, b));
}

/* Perform Fourier-Motzkin elimination to calculate the bounds of the
   auxillary nest.
   Fourier-Motzkin is a way of reducing systems of linear inequalities so that
   it is easy to calculate the answer and bounds.
   A sketch of how it works:
   Given a system of linear inequalities, ai * xj >= bk, you can always
   rewrite the constraints so they are all of the form
   a <= x, or x <= b, or x >= constant for some x in x1 ... xj (and some b
   in b1 ... bk, and some a in a1...ai)
   You can then eliminate this x from the non-constant inequalities by
   rewriting these as a <= b, x >= constant, and delete the x variable.
   You can then repeat this for any remaining x variables, and then we have
   an easy to use variable <= constant (or no variables at all) form that we
   can construct our bounds from. 
   
   In our case, each time we eliminate, we construct part of the bound from
   the ith variable, then delete the ith variable. 
   
   Remember the constant are in our vector a, our coefficient matrix is A,
   and our invariant coefficient matrix is B.
   
   SIZE is the size of the matrices being passed.
   DEPTH is the loop nest depth.
   INVARIANTS is the number of loop invariants.
   A, B, and a are the coefficient matrix, invariant coefficient, and a
   vector of constants, respectively.  */

static lambda_loopnest 
compute_nest_using_fourier_motzkin (int size,
				    int depth, 
				    int invariants,
				    lambda_matrix A,
				    lambda_matrix B,
				    lambda_vector a)
{

  int multiple, f1, f2;
  int i, j, k;
  lambda_linear_expression expression;
  lambda_loop loop;
  lambda_loopnest auxillary_nest;
  lambda_matrix swapmatrix, A1, B1;
  lambda_vector swapvector, a1;
  int newsize;

  A1 = lambda_matrix_new (128, depth);
  B1 = lambda_matrix_new (128, invariants);
  a1 = lambda_vector_new (128);

  auxillary_nest = lambda_loopnest_new (depth, invariants);

  for (i = depth - 1; i >= 0; i--)
    {
      loop = lambda_loop_new ();
      LN_LOOPS (auxillary_nest)[i] = loop;
      LL_STEP (loop) = 1;

      for (j = 0; j < size; j++)
	{
	  if (A[j][i] < 0)
	    {
	      /* Any linear expression in the matrix with a coefficient less
		 than 0 becomes part of the new lower bound.  */ 
	      expression = lambda_linear_expression_new (depth, invariants);

	      for (k = 0; k < i; k++)
		LLE_COEFFICIENTS (expression)[k] = A[j][k];

	      for (k = 0; k < invariants; k++)
		LLE_INVARIANT_COEFFICIENTS (expression)[k] = -1 * B[j][k];

	      LLE_DENOMINATOR (expression) = -1 * A[j][i];
	      LLE_CONSTANT (expression) = -1 * a[j];

	      /* Ignore if identical to the existing lower bound.  */
	      if (!lle_equal (LL_LOWER_BOUND (loop),
			      expression, depth, invariants))
		{
		  LLE_NEXT (expression) = LL_LOWER_BOUND (loop);
		  LL_LOWER_BOUND (loop) = expression;
		}

	    }
	  else if (A[j][i] > 0)
	    {
	      /* Any linear expression with a coefficient greater than 0
		 becomes part of the new upper bound.  */ 
	      expression = lambda_linear_expression_new (depth, invariants);
	      for (k = 0; k < i; k++)
		LLE_COEFFICIENTS (expression)[k] = -1 * A[j][k];

	      for (k = 0; k < invariants; k++)
		LLE_INVARIANT_COEFFICIENTS (expression)[k] = B[j][k];

	      LLE_DENOMINATOR (expression) = A[j][i];
	      LLE_CONSTANT (expression) = a[j];

	      /* Ignore if identical to the existing upper bound.  */
	      if (!lle_equal (LL_UPPER_BOUND (loop),
			      expression, depth, invariants))
		{
		  LLE_NEXT (expression) = LL_UPPER_BOUND (loop);
		  LL_UPPER_BOUND (loop) = expression;
		}

	    }
	}

      /* This portion creates a new system of linear inequalities by deleting
	 the i'th variable, reducing the system by one variable.  */
      newsize = 0;
      for (j = 0; j < size; j++)
	{
	  /* If the coefficient for the i'th variable is 0, then we can just
	     eliminate the variable straightaway.  Otherwise, we have to
	     multiply through by the coefficients we are eliminating.  */
	  if (A[j][i] == 0)
	    {
	      lambda_vector_copy (A[j], A1[newsize], depth);
	      lambda_vector_copy (B[j], B1[newsize], invariants);
	      a1[newsize] = a[j];
	      newsize++;
	    }
	  else if (A[j][i] > 0)
	    {
	      for (k = 0; k < size; k++)
		{
		  if (A[k][i] < 0)
		    {
		      multiple = lcm (A[j][i], A[k][i]);
		      f1 = multiple / A[j][i];
		      f2 = -1 * multiple / A[k][i];

		      lambda_vector_add_mc (A[j], f1, A[k], f2,
					    A1[newsize], depth);
		      lambda_vector_add_mc (B[j], f1, B[k], f2,
					    B1[newsize], invariants);
		      a1[newsize] = f1 * a[j] + f2 * a[k];
		      newsize++;
		    }
		}
	    }
	}

      swapmatrix = A;
      A = A1;
      A1 = swapmatrix;

      swapmatrix = B;
      B = B1;
      B1 = swapmatrix;

      swapvector = a;
      a = a1;
      a1 = swapvector;

      size = newsize;
    }

  return auxillary_nest;
}

/* Compute the loop bounds for the auxiliary space NEST.
   Input system used is Ax <= b.  TRANS is the unimodular transformation.  
   Given the original nest, this function will 
   1. Convert the nest into matrix form, which consists of a matrix for the
   coefficients, a matrix for the 
   invariant coefficients, and a vector for the constants.  
   2. Use the matrix form to calculate the lattice base for the nest (which is
   a dense space) 
   3. Compose the dense space transform with the user specified transform, to 
   get a transform we can easily calculate transformed bounds for.
   4. Multiply the composed transformation matrix times the matrix form of the
   loop.
   5. Transform the newly created matrix (from step 4) back into a loop nest
   using fourier motzkin elimination to figure out the bounds.  */

static lambda_loopnest
lambda_compute_auxillary_space (lambda_loopnest nest,
				lambda_trans_matrix trans)
{
  lambda_matrix A, B, A1, B1;
  lambda_vector a, a1;
  lambda_matrix invertedtrans;
  int determinant, depth, invariants, size;
  int i, j;
  lambda_loop loop;
  lambda_linear_expression expression;
  lambda_lattice lattice;

  depth = LN_DEPTH (nest);
  invariants = LN_INVARIANTS (nest);

  /* Unfortunately, we can't know the number of constraints we'll have
     ahead of time, but this should be enough even in ridiculous loop nest
     cases. We abort if we go over this limit.  */
  A = lambda_matrix_new (128, depth);
  B = lambda_matrix_new (128, invariants);
  a = lambda_vector_new (128);

  A1 = lambda_matrix_new (128, depth);
  B1 = lambda_matrix_new (128, invariants);
  a1 = lambda_vector_new (128);

  /* Store the bounds in the equation matrix A, constant vector a, and
     invariant matrix B, so that we have Ax <= a + B.
     This requires a little equation rearranging so that everything is on the
     correct side of the inequality.  */
  size = 0;
  for (i = 0; i < depth; i++)
    {
      loop = LN_LOOPS (nest)[i];

      /* First we do the lower bound.  */
      if (LL_STEP (loop) > 0)
	expression = LL_LOWER_BOUND (loop);
      else
	expression = LL_UPPER_BOUND (loop);

      for (; expression != NULL; expression = LLE_NEXT (expression))
	{
	  /* Fill in the coefficient.  */
	  for (j = 0; j < i; j++)
	    A[size][j] = LLE_COEFFICIENTS (expression)[j];

	  /* And the invariant coefficient.  */
	  for (j = 0; j < invariants; j++)
	    B[size][j] = LLE_INVARIANT_COEFFICIENTS (expression)[j];

	  /* And the constant.  */
	  a[size] = LLE_CONSTANT (expression);

	  /* Convert (2x+3y+2+b)/4 <= z to 2x+3y-4z <= -2-b.  IE put all
	     constants and single variables on   */
	  A[size][i] = -1 * LLE_DENOMINATOR (expression);
	  a[size] *= -1;
	  for (j = 0; j < invariants; j++)
	    B[size][j] *= -1;

	  size++;
	  /* Need to increase matrix sizes above.  */
	  gcc_assert (size <= 127);
	  
	}

      /* Then do the exact same thing for the upper bounds.  */
      if (LL_STEP (loop) > 0)
	expression = LL_UPPER_BOUND (loop);
      else
	expression = LL_LOWER_BOUND (loop);

      for (; expression != NULL; expression = LLE_NEXT (expression))
	{
	  /* Fill in the coefficient.  */
	  for (j = 0; j < i; j++)
	    A[size][j] = LLE_COEFFICIENTS (expression)[j];

	  /* And the invariant coefficient.  */
	  for (j = 0; j < invariants; j++)
	    B[size][j] = LLE_INVARIANT_COEFFICIENTS (expression)[j];

	  /* And the constant.  */
	  a[size] = LLE_CONSTANT (expression);

	  /* Convert z <= (2x+3y+2+b)/4 to -2x-3y+4z <= 2+b.  */
	  for (j = 0; j < i; j++)
	    A[size][j] *= -1;
	  A[size][i] = LLE_DENOMINATOR (expression);
	  size++;
	  /* Need to increase matrix sizes above.  */
	  gcc_assert (size <= 127);

	}
    }

  /* Compute the lattice base x = base * y + origin, where y is the
     base space.  */
  lattice = lambda_lattice_compute_base (nest);

  /* Ax <= a + B then becomes ALy <= a+B - A*origin.  L is the lattice base  */

  /* A1 = A * L */
  lambda_matrix_mult (A, LATTICE_BASE (lattice), A1, size, depth, depth);

  /* a1 = a - A * origin constant.  */
  lambda_matrix_vector_mult (A, size, depth, LATTICE_ORIGIN (lattice), a1);
  lambda_vector_add_mc (a, 1, a1, -1, a1, size);

  /* B1 = B - A * origin invariant.  */
  lambda_matrix_mult (A, LATTICE_ORIGIN_INVARIANTS (lattice), B1, size, depth,
		      invariants);
  lambda_matrix_add_mc (B, 1, B1, -1, B1, size, invariants);

  /* Now compute the auxiliary space bounds by first inverting U, multiplying
     it by A1, then performing fourier motzkin.  */

  invertedtrans = lambda_matrix_new (depth, depth);

  /* Compute the inverse of U.  */
  determinant = lambda_matrix_inverse (LTM_MATRIX (trans),
				       invertedtrans, depth);

  /* A = A1 inv(U).  */
  lambda_matrix_mult (A1, invertedtrans, A, size, depth, depth);

  return compute_nest_using_fourier_motzkin (size, depth, invariants,
					     A, B1, a1);
}

/* Compute the loop bounds for the target space, using the bounds of
   the auxiliary nest AUXILLARY_NEST, and the triangular matrix H.  
   The target space loop bounds are computed by multiplying the triangular
   matrix H by the auxillary nest, to get the new loop bounds.  The sign of
   the loop steps (positive or negative) is then used to swap the bounds if
   the loop counts downwards.
   Return the target loopnest.  */

static lambda_loopnest
lambda_compute_target_space (lambda_loopnest auxillary_nest,
			     lambda_trans_matrix H, lambda_vector stepsigns)
{
  lambda_matrix inverse, H1;
  int determinant, i, j;
  int gcd1, gcd2;
  int factor;

  lambda_loopnest target_nest;
  int depth, invariants;
  lambda_matrix target;

  lambda_loop auxillary_loop, target_loop;
  lambda_linear_expression expression, auxillary_expr, target_expr, tmp_expr;

  depth = LN_DEPTH (auxillary_nest);
  invariants = LN_INVARIANTS (auxillary_nest);

  inverse = lambda_matrix_new (depth, depth);
  determinant = lambda_matrix_inverse (LTM_MATRIX (H), inverse, depth);

  /* H1 is H excluding its diagonal.  */
  H1 = lambda_matrix_new (depth, depth);
  lambda_matrix_copy (LTM_MATRIX (H), H1, depth, depth);

  for (i = 0; i < depth; i++)
    H1[i][i] = 0;

  /* Computes the linear offsets of the loop bounds.  */
  target = lambda_matrix_new (depth, depth);
  lambda_matrix_mult (H1, inverse, target, depth, depth, depth);

  target_nest = lambda_loopnest_new (depth, invariants);

  for (i = 0; i < depth; i++)
    {

      /* Get a new loop structure.  */
      target_loop = lambda_loop_new ();
      LN_LOOPS (target_nest)[i] = target_loop;

      /* Computes the gcd of the coefficients of the linear part.  */
      gcd1 = gcd_vector (target[i], i);

      /* Include the denominator in the GCD.  */
      gcd1 = gcd (gcd1, determinant);

      /* Now divide through by the gcd.  */
      for (j = 0; j < i; j++)
	target[i][j] = target[i][j] / gcd1;

      expression = lambda_linear_expression_new (depth, invariants);
      lambda_vector_copy (target[i], LLE_COEFFICIENTS (expression), depth);
      LLE_DENOMINATOR (expression) = determinant / gcd1;
      LLE_CONSTANT (expression) = 0;
      lambda_vector_clear (LLE_INVARIANT_COEFFICIENTS (expression),
			   invariants);
      LL_LINEAR_OFFSET (target_loop) = expression;
    }

  /* For each loop, compute the new bounds from H.  */
  for (i = 0; i < depth; i++)
    {
      auxillary_loop = LN_LOOPS (auxillary_nest)[i];
      target_loop = LN_LOOPS (target_nest)[i];
      LL_STEP (target_loop) = LTM_MATRIX (H)[i][i];
      factor = LTM_MATRIX (H)[i][i];

      /* First we do the lower bound.  */
      auxillary_expr = LL_LOWER_BOUND (auxillary_loop);

      for (; auxillary_expr != NULL;
	   auxillary_expr = LLE_NEXT (auxillary_expr))
	{
	  target_expr = lambda_linear_expression_new (depth, invariants);
	  lambda_vector_matrix_mult (LLE_COEFFICIENTS (auxillary_expr),
				     depth, inverse, depth,
				     LLE_COEFFICIENTS (target_expr));
	  lambda_vector_mult_const (LLE_COEFFICIENTS (target_expr),
				    LLE_COEFFICIENTS (target_expr), depth,
				    factor);

	  LLE_CONSTANT (target_expr) = LLE_CONSTANT (auxillary_expr) * factor;
	  lambda_vector_copy (LLE_INVARIANT_COEFFICIENTS (auxillary_expr),
			      LLE_INVARIANT_COEFFICIENTS (target_expr),
			      invariants);
	  lambda_vector_mult_const (LLE_INVARIANT_COEFFICIENTS (target_expr),
				    LLE_INVARIANT_COEFFICIENTS (target_expr),
				    invariants, factor);
	  LLE_DENOMINATOR (target_expr) = LLE_DENOMINATOR (auxillary_expr);

	  if (!lambda_vector_zerop (LLE_COEFFICIENTS (target_expr), depth))
	    {
	      LLE_CONSTANT (target_expr) = LLE_CONSTANT (target_expr)
		* determinant;
	      lambda_vector_mult_const (LLE_INVARIANT_COEFFICIENTS
					(target_expr),
					LLE_INVARIANT_COEFFICIENTS
					(target_expr), invariants,
					determinant);
	      LLE_DENOMINATOR (target_expr) =
		LLE_DENOMINATOR (target_expr) * determinant;
	    }
	  /* Find the gcd and divide by it here, rather than doing it
	     at the tree level.  */
	  gcd1 = gcd_vector (LLE_COEFFICIENTS (target_expr), depth);
	  gcd2 = gcd_vector (LLE_INVARIANT_COEFFICIENTS (target_expr),
			     invariants);
	  gcd1 = gcd (gcd1, gcd2);
	  gcd1 = gcd (gcd1, LLE_CONSTANT (target_expr));
	  gcd1 = gcd (gcd1, LLE_DENOMINATOR (target_expr));
	  for (j = 0; j < depth; j++)
	    LLE_COEFFICIENTS (target_expr)[j] /= gcd1;
	  for (j = 0; j < invariants; j++)
	    LLE_INVARIANT_COEFFICIENTS (target_expr)[j] /= gcd1;
	  LLE_CONSTANT (target_expr) /= gcd1;
	  LLE_DENOMINATOR (target_expr) /= gcd1;
	  /* Ignore if identical to existing bound.  */
	  if (!lle_equal (LL_LOWER_BOUND (target_loop), target_expr, depth,
			  invariants))
	    {
	      LLE_NEXT (target_expr) = LL_LOWER_BOUND (target_loop);
	      LL_LOWER_BOUND (target_loop) = target_expr;
	    }
	}
      /* Now do the upper bound.  */
      auxillary_expr = LL_UPPER_BOUND (auxillary_loop);

      for (; auxillary_expr != NULL;
	   auxillary_expr = LLE_NEXT (auxillary_expr))
	{
	  target_expr = lambda_linear_expression_new (depth, invariants);
	  lambda_vector_matrix_mult (LLE_COEFFICIENTS (auxillary_expr),
				     depth, inverse, depth,
				     LLE_COEFFICIENTS (target_expr));
	  lambda_vector_mult_const (LLE_COEFFICIENTS (target_expr),
				    LLE_COEFFICIENTS (target_expr), depth,
				    factor);
	  LLE_CONSTANT (target_expr) = LLE_CONSTANT (auxillary_expr) * factor;
	  lambda_vector_copy (LLE_INVARIANT_COEFFICIENTS (auxillary_expr),
			      LLE_INVARIANT_COEFFICIENTS (target_expr),
			      invariants);
	  lambda_vector_mult_const (LLE_INVARIANT_COEFFICIENTS (target_expr),
				    LLE_INVARIANT_COEFFICIENTS (target_expr),
				    invariants, factor);
	  LLE_DENOMINATOR (target_expr) = LLE_DENOMINATOR (auxillary_expr);

	  if (!lambda_vector_zerop (LLE_COEFFICIENTS (target_expr), depth))
	    {
	      LLE_CONSTANT (target_expr) = LLE_CONSTANT (target_expr)
		* determinant;
	      lambda_vector_mult_const (LLE_INVARIANT_COEFFICIENTS
					(target_expr),
					LLE_INVARIANT_COEFFICIENTS
					(target_expr), invariants,
					determinant);
	      LLE_DENOMINATOR (target_expr) =
		LLE_DENOMINATOR (target_expr) * determinant;
	    }
	  /* Find the gcd and divide by it here, instead of at the
	     tree level.  */
	  gcd1 = gcd_vector (LLE_COEFFICIENTS (target_expr), depth);
	  gcd2 = gcd_vector (LLE_INVARIANT_COEFFICIENTS (target_expr),
			     invariants);
	  gcd1 = gcd (gcd1, gcd2);
	  gcd1 = gcd (gcd1, LLE_CONSTANT (target_expr));
	  gcd1 = gcd (gcd1, LLE_DENOMINATOR (target_expr));
	  for (j = 0; j < depth; j++)
	    LLE_COEFFICIENTS (target_expr)[j] /= gcd1;
	  for (j = 0; j < invariants; j++)
	    LLE_INVARIANT_COEFFICIENTS (target_expr)[j] /= gcd1;
	  LLE_CONSTANT (target_expr) /= gcd1;
	  LLE_DENOMINATOR (target_expr) /= gcd1;
	  /* Ignore if equal to existing bound.  */
	  if (!lle_equal (LL_UPPER_BOUND (target_loop), target_expr, depth,
			  invariants))
	    {
	      LLE_NEXT (target_expr) = LL_UPPER_BOUND (target_loop);
	      LL_UPPER_BOUND (target_loop) = target_expr;
	    }
	}
    }
  for (i = 0; i < depth; i++)
    {
      target_loop = LN_LOOPS (target_nest)[i];
      /* If necessary, exchange the upper and lower bounds and negate
         the step size.  */
      if (stepsigns[i] < 0)
	{
	  LL_STEP (target_loop) *= -1;
	  tmp_expr = LL_LOWER_BOUND (target_loop);
	  LL_LOWER_BOUND (target_loop) = LL_UPPER_BOUND (target_loop);
	  LL_UPPER_BOUND (target_loop) = tmp_expr;
	}
    }
  return target_nest;
}

/* Compute the step signs of TRANS, using TRANS and stepsigns.  Return the new
   result.  */

static lambda_vector
lambda_compute_step_signs (lambda_trans_matrix trans, lambda_vector stepsigns)
{
  lambda_matrix matrix, H;
  int size;
  lambda_vector newsteps;
  int i, j, factor, minimum_column;
  int temp;

  matrix = LTM_MATRIX (trans);
  size = LTM_ROWSIZE (trans);
  H = lambda_matrix_new (size, size);

  newsteps = lambda_vector_new (size);
  lambda_vector_copy (stepsigns, newsteps, size);

  lambda_matrix_copy (matrix, H, size, size);

  for (j = 0; j < size; j++)
    {
      lambda_vector row;
      row = H[j];
      for (i = j; i < size; i++)
	if (row[i] < 0)
	  lambda_matrix_col_negate (H, size, i);
      while (lambda_vector_first_nz (row, size, j + 1) < size)
	{
	  minimum_column = lambda_vector_min_nz (row, size, j);
	  lambda_matrix_col_exchange (H, size, j, minimum_column);

	  temp = newsteps[j];
	  newsteps[j] = newsteps[minimum_column];
	  newsteps[minimum_column] = temp;

	  for (i = j + 1; i < size; i++)
	    {
	      factor = row[i] / row[j];
	      lambda_matrix_col_add (H, size, j, i, -1 * factor);
	    }
	}
    }
  return newsteps;
}

/* Transform NEST according to TRANS, and return the new loopnest.
   This involves
   1. Computing a lattice base for the transformation
   2. Composing the dense base with the specified transformation (TRANS)
   3. Decomposing the combined transformation into a lower triangular portion,
   and a unimodular portion. 
   4. Computing the auxillary nest using the unimodular portion.
   5. Computing the target nest using the auxillary nest and the lower
   triangular portion.  */ 

lambda_loopnest
lambda_loopnest_transform (lambda_loopnest nest, lambda_trans_matrix trans)
{
  lambda_loopnest auxillary_nest, target_nest;

  int depth, invariants;
  int i, j;
  lambda_lattice lattice;
  lambda_trans_matrix trans1, H, U;
  lambda_loop loop;
  lambda_linear_expression expression;
  lambda_vector origin;
  lambda_matrix origin_invariants;
  lambda_vector stepsigns;
  int f;

  depth = LN_DEPTH (nest);
  invariants = LN_INVARIANTS (nest);

  /* Keep track of the signs of the loop steps.  */
  stepsigns = lambda_vector_new (depth);
  for (i = 0; i < depth; i++)
    {
      if (LL_STEP (LN_LOOPS (nest)[i]) > 0)
	stepsigns[i] = 1;
      else
	stepsigns[i] = -1;
    }

  /* Compute the lattice base.  */
  lattice = lambda_lattice_compute_base (nest);
  trans1 = lambda_trans_matrix_new (depth, depth);

  /* Multiply the transformation matrix by the lattice base.  */

  lambda_matrix_mult (LTM_MATRIX (trans), LATTICE_BASE (lattice),
		      LTM_MATRIX (trans1), depth, depth, depth);

  /* Compute the Hermite normal form for the new transformation matrix.  */
  H = lambda_trans_matrix_new (depth, depth);
  U = lambda_trans_matrix_new (depth, depth);
  lambda_matrix_hermite (LTM_MATRIX (trans1), depth, LTM_MATRIX (H),
			 LTM_MATRIX (U));

  /* Compute the auxiliary loop nest's space from the unimodular
     portion.  */
  auxillary_nest = lambda_compute_auxillary_space (nest, U);

  /* Compute the loop step signs from the old step signs and the
     transformation matrix.  */
  stepsigns = lambda_compute_step_signs (trans1, stepsigns);

  /* Compute the target loop nest space from the auxiliary nest and
     the lower triangular matrix H.  */
  target_nest = lambda_compute_target_space (auxillary_nest, H, stepsigns);
  origin = lambda_vector_new (depth);
  origin_invariants = lambda_matrix_new (depth, invariants);
  lambda_matrix_vector_mult (LTM_MATRIX (trans), depth, depth,
			     LATTICE_ORIGIN (lattice), origin);
  lambda_matrix_mult (LTM_MATRIX (trans), LATTICE_ORIGIN_INVARIANTS (lattice),
		      origin_invariants, depth, depth, invariants);

  for (i = 0; i < depth; i++)
    {
      loop = LN_LOOPS (target_nest)[i];
      expression = LL_LINEAR_OFFSET (loop);
      if (lambda_vector_zerop (LLE_COEFFICIENTS (expression), depth))
	f = 1;
      else
	f = LLE_DENOMINATOR (expression);

      LLE_CONSTANT (expression) += f * origin[i];

      for (j = 0; j < invariants; j++)
	LLE_INVARIANT_COEFFICIENTS (expression)[j] +=
	  f * origin_invariants[i][j];
    }

  return target_nest;

}

/* Convert a gcc tree expression EXPR to a lambda linear expression, and
   return the new expression.  DEPTH is the depth of the loopnest.
   OUTERINDUCTIONVARS is an array of the induction variables for outer loops
   in this nest.  INVARIANTS is the array of invariants for the loop.  EXTRA
   is the amount we have to add/subtract from the expression because of the
   type of comparison it is used in.  */

static lambda_linear_expression
gcc_tree_to_linear_expression (int depth, tree expr,
			       VEC(tree) *outerinductionvars,
			       VEC(tree) *invariants, int extra)
{
  lambda_linear_expression lle = NULL;
  switch (TREE_CODE (expr))
    {
    case INTEGER_CST:
      {
	lle = lambda_linear_expression_new (depth, 2 * depth);
	LLE_CONSTANT (lle) = TREE_INT_CST_LOW (expr);
	if (extra != 0)
	  LLE_CONSTANT (lle) += extra;

	LLE_DENOMINATOR (lle) = 1;
      }
      break;
    case SSA_NAME:
      {
	tree iv, invar;
	size_t i;
	for (i = 0; VEC_iterate (tree, outerinductionvars, i, iv); i++)
	  if (iv != NULL)
	    {
	      if (SSA_NAME_VAR (iv) == SSA_NAME_VAR (expr))
		{
		  lle = lambda_linear_expression_new (depth, 2 * depth);
		  LLE_COEFFICIENTS (lle)[i] = 1;
		  if (extra != 0)
		    LLE_CONSTANT (lle) = extra;

		  LLE_DENOMINATOR (lle) = 1;
		}
	    }
	for (i = 0; VEC_iterate (tree, invariants, i, invar); i++)
	  if (invar != NULL)
	    {
	      if (SSA_NAME_VAR (invar) == SSA_NAME_VAR (expr))
		{
		  lle = lambda_linear_expression_new (depth, 2 * depth);
		  LLE_INVARIANT_COEFFICIENTS (lle)[i] = 1;
		  if (extra != 0)
		    LLE_CONSTANT (lle) = extra;
		  LLE_DENOMINATOR (lle) = 1;
		}
	    }
      }
      break;
    default:
      return NULL;
    }

  return lle;
}

/* Return the depth of the loopnest NEST */

static int 
depth_of_nest (struct loop *nest)
{
  size_t depth = 0;
  while (nest)
    {
      depth++;
      nest = nest->inner;
    }
  return depth;
}


/* Return true if OP is invariant in LOOP and all outer loops.  */

static bool
invariant_in_loop_and_outer_loops (struct loop *loop, tree op)
{
  if (is_gimple_min_invariant (op))
    return true;
  if (loop->depth == 0)
    return true;
  if (!expr_invariant_in_loop_p (loop, op))
    return false;
  if (loop->outer 
      && !invariant_in_loop_and_outer_loops (loop->outer, op))
    return false;
  return true;
}

/* Generate a lambda loop from a gcc loop LOOP.  Return the new lambda loop,
   or NULL if it could not be converted.
   DEPTH is the depth of the loop.
   INVARIANTS is a pointer to the array of loop invariants.
   The induction variable for this loop should be stored in the parameter
   OURINDUCTIONVAR.
   OUTERINDUCTIONVARS is an array of induction variables for outer loops.  */

static lambda_loop
gcc_loop_to_lambda_loop (struct loop *loop, int depth,
			 VEC (tree) ** invariants,
			 tree * ourinductionvar,
			 VEC (tree) * outerinductionvars,
			 VEC (tree) ** lboundvars,
			 VEC (tree) ** uboundvars,
			 VEC (int) ** steps)
{
  tree phi;
  tree exit_cond;
  tree access_fn, inductionvar;
  tree step;
  lambda_loop lloop = NULL;
  lambda_linear_expression lbound, ubound;
  tree test;
  int stepint;
  int extra = 0;
  tree lboundvar, uboundvar, uboundresult;
  use_optype uses;

  /* Find out induction var and exit condition.  */
  inductionvar = find_induction_var_from_exit_cond (loop);
  exit_cond = get_loop_exit_condition (loop);

  if (inductionvar == NULL || exit_cond == NULL)
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file,
		 "Unable to convert loop: Cannot determine exit condition or induction variable for loop.\n");
      return NULL;
    }

  test = TREE_OPERAND (exit_cond, 0);

  if (SSA_NAME_DEF_STMT (inductionvar) == NULL_TREE)
    {

      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file,
		 "Unable to convert loop: Cannot find PHI node for induction variable\n");

      return NULL;
    }

  phi = SSA_NAME_DEF_STMT (inductionvar);
  if (TREE_CODE (phi) != PHI_NODE)
    {
      get_stmt_operands (phi);
      uses = STMT_USE_OPS (phi);

      if (!uses)
	{

	  if (dump_file && (dump_flags & TDF_DETAILS))
	    fprintf (dump_file,
		     "Unable to convert loop: Cannot find PHI node for induction variable\n");

	  return NULL;
	}

      phi = USE_OP (uses, 0);
      phi = SSA_NAME_DEF_STMT (phi);
      if (TREE_CODE (phi) != PHI_NODE)
	{

	  if (dump_file && (dump_flags & TDF_DETAILS))
	    fprintf (dump_file,
		     "Unable to convert loop: Cannot find PHI node for induction variable\n");
	  return NULL;
	}

    }

  /* The induction variable name/version we want to put in the array is the
     result of the induction variable phi node.  */
  *ourinductionvar = PHI_RESULT (phi);
  access_fn = instantiate_parameters
    (loop, analyze_scalar_evolution (loop, PHI_RESULT (phi)));
  if (access_fn == chrec_dont_know)
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file,
		 "Unable to convert loop: Access function for induction variable phi is unknown\n");

      return NULL;
    }

  step = evolution_part_in_loop_num (access_fn, loop->num);
  if (!step || step == chrec_dont_know)
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file,
		 "Unable to convert loop: Cannot determine step of loop.\n");

      return NULL;
    }
  if (TREE_CODE (step) != INTEGER_CST)
    {

      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file,
		 "Unable to convert loop: Step of loop is not integer.\n");
      return NULL;
    }

  stepint = TREE_INT_CST_LOW (step);

  /* Only want phis for induction vars, which will have two
     arguments.  */
  if (PHI_NUM_ARGS (phi) != 2)
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file,
		 "Unable to convert loop: PHI node for induction variable has >2 arguments\n");
      return NULL;
    }

  /* Another induction variable check. One argument's source should be
     in the loop, one outside the loop.  */
  if (flow_bb_inside_loop_p (loop, PHI_ARG_EDGE (phi, 0)->src)
      && flow_bb_inside_loop_p (loop, PHI_ARG_EDGE (phi, 1)->src))
    {

      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file,
		 "Unable to convert loop: PHI edges both inside loop, or both outside loop.\n");

      return NULL;
    }

  if (flow_bb_inside_loop_p (loop, PHI_ARG_EDGE (phi, 0)->src))
    {
      lboundvar = PHI_ARG_DEF (phi, 1);
      lbound = gcc_tree_to_linear_expression (depth, lboundvar,
					      outerinductionvars, *invariants,
					      0);
    }
  else
    {
      lboundvar = PHI_ARG_DEF (phi, 0);
      lbound = gcc_tree_to_linear_expression (depth, lboundvar,
					      outerinductionvars, *invariants,
					      0);
    }
  
  if (!lbound)
    {

      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file,
		 "Unable to convert loop: Cannot convert lower bound to linear expression\n");

      return NULL;
    }
  /* One part of the test may be a loop invariant tree.  */
  if (TREE_CODE (TREE_OPERAND (test, 1)) == SSA_NAME
      && invariant_in_loop_and_outer_loops (loop, TREE_OPERAND (test, 1)))
    VEC_safe_push (tree, *invariants, TREE_OPERAND (test, 1));
  else if (TREE_CODE (TREE_OPERAND (test, 0)) == SSA_NAME
	   && invariant_in_loop_and_outer_loops (loop, TREE_OPERAND (test, 0)))
    VEC_safe_push (tree, *invariants, TREE_OPERAND (test, 0));
  
  /* The non-induction variable part of the test is the upper bound variable.
   */
  if (TREE_OPERAND (test, 0) == inductionvar)
    uboundvar = TREE_OPERAND (test, 1);
  else
    uboundvar = TREE_OPERAND (test, 0);
    

  /* We only size the vectors assuming we have, at max, 2 times as many
     invariants as we do loops (one for each bound).
     This is just an arbitrary number, but it has to be matched against the
     code below.  */
  gcc_assert (VEC_length (tree, *invariants) <= (unsigned int) (2 * depth));
  

  /* We might have some leftover.  */
  if (TREE_CODE (test) == LT_EXPR)
    extra = -1 * stepint;
  else if (TREE_CODE (test) == NE_EXPR)
    extra = -1 * stepint;
  else if (TREE_CODE (test) == GT_EXPR)
    extra = -1 * stepint;
  else if (TREE_CODE (test) == EQ_EXPR)
    extra = 1 * stepint;
  
  ubound = gcc_tree_to_linear_expression (depth, uboundvar,
					  outerinductionvars,
					  *invariants, extra);
  uboundresult = build (PLUS_EXPR, TREE_TYPE (uboundvar), uboundvar,
			build_int_cst (TREE_TYPE (uboundvar), extra));
  VEC_safe_push (tree, *uboundvars, uboundresult);
  VEC_safe_push (tree, *lboundvars, lboundvar);
  VEC_safe_push (int, *steps, stepint);
  if (!ubound)
    {
      if (dump_file && (dump_flags & TDF_DETAILS))
	fprintf (dump_file,
		 "Unable to convert loop: Cannot convert upper bound to linear expression\n");
      return NULL;
    }

  lloop = lambda_loop_new ();
  LL_STEP (lloop) = stepint;
  LL_LOWER_BOUND (lloop) = lbound;
  LL_UPPER_BOUND (lloop) = ubound;
  return lloop;
}

/* Given a LOOP, find the induction variable it is testing against in the exit
   condition.  Return the induction variable if found, NULL otherwise.  */

static tree
find_induction_var_from_exit_cond (struct loop *loop)
{
  tree expr = get_loop_exit_condition (loop);
  tree ivarop;
  tree test;
  if (expr == NULL_TREE)
    return NULL_TREE;
  if (TREE_CODE (expr) != COND_EXPR)
    return NULL_TREE;
  test = TREE_OPERAND (expr, 0);
  if (!COMPARISON_CLASS_P (test))
    return NULL_TREE;

  /* Find the side that is invariant in this loop. The ivar must be the other
     side.  */
  
  if (expr_invariant_in_loop_p (loop, TREE_OPERAND (test, 0)))
      ivarop = TREE_OPERAND (test, 1);
  else if (expr_invariant_in_loop_p (loop, TREE_OPERAND (test, 1)))
      ivarop = TREE_OPERAND (test, 0);
  else
    return NULL_TREE;

  if (TREE_CODE (ivarop) != SSA_NAME)
    return NULL_TREE;
  return ivarop;
}

DEF_VEC_GC_P(lambda_loop);
/* Generate a lambda loopnest from a gcc loopnest LOOP_NEST.
   Return the new loop nest.  
   INDUCTIONVARS is a pointer to an array of induction variables for the
   loopnest that will be filled in during this process.
   INVARIANTS is a pointer to an array of invariants that will be filled in
   during this process.  */

lambda_loopnest
gcc_loopnest_to_lambda_loopnest (struct loops *currloops,
				 struct loop * loop_nest,
				 VEC (tree) **inductionvars,
				 VEC (tree) **invariants,
				 bool need_perfect_nest)
{
  lambda_loopnest ret;
  struct loop *temp;
  int depth = 0;
  size_t i;
  VEC (lambda_loop) *loops = NULL;
  VEC (tree) *uboundvars = NULL;
  VEC (tree) *lboundvars  = NULL;
  VEC (int) *steps = NULL;
  lambda_loop newloop;
  tree inductionvar = NULL;
  
  depth = depth_of_nest (loop_nest);
  temp = loop_nest;
  while (temp)
    {
      newloop = gcc_loop_to_lambda_loop (temp, depth, invariants,
					 &inductionvar, *inductionvars,
					 &lboundvars, &uboundvars,
					 &steps);
      if (!newloop)
	return NULL;
      VEC_safe_push (tree, *inductionvars, inductionvar);
      VEC_safe_push (lambda_loop, loops, newloop);
      temp = temp->inner;
    }
  if (need_perfect_nest)
    {
      if (!perfect_nestify (currloops, loop_nest, 
			    lboundvars, uboundvars, steps, *inductionvars))
	{
	  if (dump_file)
	    fprintf (dump_file, "Not a perfect loop nest and couldn't convert to one.\n");    
	  return NULL;
	}
      else if (dump_file)
	fprintf (dump_file, "Successfully converted loop nest to perfect loop nest.\n");

      
    }
  ret = lambda_loopnest_new (depth, 2 * depth);
  for (i = 0; VEC_iterate (lambda_loop, loops, i, newloop); i++)
    LN_LOOPS (ret)[i] = newloop;

  return ret;

}


/* Convert a lambda body vector LBV to a gcc tree, and return the new tree. 
   STMTS_TO_INSERT is a pointer to a tree where the statements we need to be
   inserted for us are stored.  INDUCTION_VARS is the array of induction
   variables for the loop this LBV is from.  TYPE is the tree type to use for
   the variables and trees involved.  */

static tree
lbv_to_gcc_expression (lambda_body_vector lbv, 
		       tree type, VEC (tree) *induction_vars, 
		       tree * stmts_to_insert)
{
  tree stmts, stmt, resvar, name;
  tree iv;
  size_t i;
  tree_stmt_iterator tsi;

  /* Create a statement list and a linear expression temporary.  */
  stmts = alloc_stmt_list ();
  resvar = create_tmp_var (type, "lbvtmp");
  add_referenced_tmp_var (resvar);

  /* Start at 0.  */
  stmt = build (MODIFY_EXPR, void_type_node, resvar, integer_zero_node);
  name = make_ssa_name (resvar, stmt);
  TREE_OPERAND (stmt, 0) = name;
  tsi = tsi_last (stmts);
  tsi_link_after (&tsi, stmt, TSI_CONTINUE_LINKING);

  for (i = 0; VEC_iterate (tree, induction_vars, i, iv); i++)
    {
      if (LBV_COEFFICIENTS (lbv)[i] != 0)
	{
	  tree newname;
	  tree coeffmult;
	  
	  /* newname = coefficient * induction_variable */
	  coeffmult = build_int_cst (type, LBV_COEFFICIENTS (lbv)[i]);
	  stmt = build (MODIFY_EXPR, void_type_node, resvar,
			fold (build (MULT_EXPR, type, iv, coeffmult)));

	  newname = make_ssa_name (resvar, stmt);
	  TREE_OPERAND (stmt, 0) = newname;
	  fold_stmt (&stmt);
	  tsi = tsi_last (stmts);
	  tsi_link_after (&tsi, stmt, TSI_CONTINUE_LINKING);

	  /* name = name + newname */
	  stmt = build (MODIFY_EXPR, void_type_node, resvar,
			build (PLUS_EXPR, type, name, newname));
	  name = make_ssa_name (resvar, stmt);
	  TREE_OPERAND (stmt, 0) = name;
	  fold_stmt (&stmt);
	  tsi = tsi_last (stmts);
	  tsi_link_after (&tsi, stmt, TSI_CONTINUE_LINKING);

	}
    }

  /* Handle any denominator that occurs.  */
  if (LBV_DENOMINATOR (lbv) != 1)
    {
      tree denominator = build_int_cst (type, LBV_DENOMINATOR (lbv));
      stmt = build (MODIFY_EXPR, void_type_node, resvar,
		    build (CEIL_DIV_EXPR, type, name, denominator));
      name = make_ssa_name (resvar, stmt);
      TREE_OPERAND (stmt, 0) = name;
      fold_stmt (&stmt);
      tsi = tsi_last (stmts);
      tsi_link_after (&tsi, stmt, TSI_CONTINUE_LINKING);
    }
  *stmts_to_insert = stmts;
  return name;
}

/* Convert a linear expression from coefficient and constant form to a
   gcc tree.
   Return the tree that represents the final value of the expression.
   LLE is the linear expression to convert.
   OFFSET is the linear offset to apply to the expression.
   TYPE is the tree type to use for the variables and math. 
   INDUCTION_VARS is a vector of induction variables for the loops.
   INVARIANTS is a vector of the loop nest invariants.
   WRAP specifies what tree code to wrap the results in, if there is more than
   one (it is either MAX_EXPR, or MIN_EXPR).
   STMTS_TO_INSERT Is a pointer to the statement list we fill in with
   statements that need to be inserted for the linear expression.  */

static tree
lle_to_gcc_expression (lambda_linear_expression lle,
		       lambda_linear_expression offset,
		       tree type,
		       VEC(tree) *induction_vars,
		       VEC(tree) *invariants,
		       enum tree_code wrap, tree * stmts_to_insert)
{
  tree stmts, stmt, resvar, name;
  size_t i;
  tree_stmt_iterator tsi;
  tree iv, invar;
  VEC(tree) *results = NULL;

  name = NULL_TREE;
  /* Create a statement list and a linear expression temporary.  */
  stmts = alloc_stmt_list ();
  resvar = create_tmp_var (type, "lletmp");
  add_referenced_tmp_var (resvar);

  /* Build up the linear expressions, and put the variable representing the
     result in the results array.  */
  for (; lle != NULL; lle = LLE_NEXT (lle))
    {
      /* Start at name = 0.  */
      stmt = build (MODIFY_EXPR, void_type_node, resvar, integer_zero_node);
      name = make_ssa_name (resvar, stmt);
      TREE_OPERAND (stmt, 0) = name;
      fold_stmt (&stmt);
      tsi = tsi_last (stmts);
      tsi_link_after (&tsi, stmt, TSI_CONTINUE_LINKING);

      /* First do the induction variables.  
         at the end, name = name + all the induction variables added
         together.  */
      for (i = 0; VEC_iterate (tree, induction_vars, i, iv); i++)
	{
	  if (LLE_COEFFICIENTS (lle)[i] != 0)
	    {
	      tree newname;
	      tree mult;
	      tree coeff;

	      /* mult = induction variable * coefficient.  */
	      if (LLE_COEFFICIENTS (lle)[i] == 1)
		{
		  mult = VEC_index (tree, induction_vars, i);
		}
	      else
		{
		  coeff = build_int_cst (type,
					 LLE_COEFFICIENTS (lle)[i]);
		  mult = fold (build (MULT_EXPR, type, iv, coeff));
		}

	      /* newname = mult */
	      stmt = build (MODIFY_EXPR, void_type_node, resvar, mult);
	      newname = make_ssa_name (resvar, stmt);
	      TREE_OPERAND (stmt, 0) = newname;
	      fold_stmt (&stmt);
	      tsi = tsi_last (stmts);
	      tsi_link_after (&tsi, stmt, TSI_CONTINUE_LINKING);

	      /* name = name + newname */
	      stmt = build (MODIFY_EXPR, void_type_node, resvar,
			    build (PLUS_EXPR, type, name, newname));
	      name = make_ssa_name (resvar, stmt);
	      TREE_OPERAND (stmt, 0) = name;
	      fold_stmt (&stmt);
	      tsi = tsi_last (stmts);
	      tsi_link_after (&tsi, stmt, TSI_CONTINUE_LINKING);
	    }
	}

      /* Handle our invariants.
         At the end, we have name = name + result of adding all multiplied
         invariants.  */
      for (i = 0; VEC_iterate (tree, invariants, i, invar); i++)
	{
	  if (LLE_INVARIANT_COEFFICIENTS (lle)[i] != 0)
	    {
	      tree newname;
	      tree mult;
	      tree coeff;
	      int invcoeff = LLE_INVARIANT_COEFFICIENTS (lle)[i];
	      /* mult = invariant * coefficient  */
	      if (invcoeff == 1)
		{
		  mult = invar;
		}
	      else
		{
		  coeff = build_int_cst (type, invcoeff);
		  mult = fold (build (MULT_EXPR, type, invar, coeff));
		}

	      /* newname = mult */
	      stmt = build (MODIFY_EXPR, void_type_node, resvar, mult);
	      newname = make_ssa_name (resvar, stmt);
	      TREE_OPERAND (stmt, 0) = newname;
	      fold_stmt (&stmt);
	      tsi = tsi_last (stmts);
	      tsi_link_after (&tsi, stmt, TSI_CONTINUE_LINKING);

	      /* name = name + newname */
	      stmt = build (MODIFY_EXPR, void_type_node, resvar,
			    build (PLUS_EXPR, type, name, newname));
	      name = make_ssa_name (resvar, stmt);
	      TREE_OPERAND (stmt, 0) = name;
	      fold_stmt (&stmt);
	      tsi = tsi_last (stmts);
	      tsi_link_after (&tsi, stmt, TSI_CONTINUE_LINKING);
	    }
	}

      /* Now handle the constant.
         name = name + constant.  */
      if (LLE_CONSTANT (lle) != 0)
	{
	  stmt = build (MODIFY_EXPR, void_type_node, resvar,
			build (PLUS_EXPR, type, name, 
			       build_int_cst (type, LLE_CONSTANT (lle))));
	  name = make_ssa_name (resvar, stmt);
	  TREE_OPERAND (stmt, 0) = name;
	  fold_stmt (&stmt);
	  tsi = tsi_last (stmts);
	  tsi_link_after (&tsi, stmt, TSI_CONTINUE_LINKING);
	}

      /* Now handle the offset.
         name = name + linear offset.  */
      if (LLE_CONSTANT (offset) != 0)
	{
	  stmt = build (MODIFY_EXPR, void_type_node, resvar,
			build (PLUS_EXPR, type, name, 
			       build_int_cst (type, LLE_CONSTANT (offset))));
	  name = make_ssa_name (resvar, stmt);
	  TREE_OPERAND (stmt, 0) = name;
	  fold_stmt (&stmt);
	  tsi = tsi_last (stmts);
	  tsi_link_after (&tsi, stmt, TSI_CONTINUE_LINKING);
	}

      /* Handle any denominator that occurs.  */
      if (LLE_DENOMINATOR (lle) != 1)
	{
	  if (wrap == MAX_EXPR)
	    stmt = build (MODIFY_EXPR, void_type_node, resvar,
			  build (CEIL_DIV_EXPR, type, name, 
				 build_int_cst (type, LLE_DENOMINATOR (lle))));
	  else if (wrap == MIN_EXPR)
	    stmt = build (MODIFY_EXPR, void_type_node, resvar,
			  build (FLOOR_DIV_EXPR, type, name, 
				 build_int_cst (type, LLE_DENOMINATOR (lle))));
	  else
	    gcc_unreachable();

	  /* name = {ceil, floor}(name/denominator) */
	  name = make_ssa_name (resvar, stmt);
	  TREE_OPERAND (stmt, 0) = name;
	  tsi = tsi_last (stmts);
	  tsi_link_after (&tsi, stmt, TSI_CONTINUE_LINKING);
	}
      VEC_safe_push (tree, results, name);
    }

  /* Again, out of laziness, we don't handle this case yet.  It's not
     hard, it just hasn't occurred.  */
  gcc_assert (VEC_length (tree, results) <= 2);
  
  /* We may need to wrap the results in a MAX_EXPR or MIN_EXPR.  */
  if (VEC_length (tree, results) > 1)
    {
      tree op1 = VEC_index (tree, results, 0);
      tree op2 = VEC_index (tree, results, 1);
      stmt = build (MODIFY_EXPR, void_type_node, resvar,
		    build (wrap, type, op1, op2));
      name = make_ssa_name (resvar, stmt);
      TREE_OPERAND (stmt, 0) = name;
      tsi = tsi_last (stmts);
      tsi_link_after (&tsi, stmt, TSI_CONTINUE_LINKING);
    }

  *stmts_to_insert = stmts;
  return name;
}

/* Transform a lambda loopnest NEW_LOOPNEST, which had TRANSFORM applied to
   it, back into gcc code.  This changes the
   loops, their induction variables, and their bodies, so that they
   match the transformed loopnest.  
   OLD_LOOPNEST is the loopnest before we've replaced it with the new
   loopnest.
   OLD_IVS is a vector of induction variables from the old loopnest.
   INVARIANTS is a vector of loop invariants from the old loopnest.
   NEW_LOOPNEST is the new lambda loopnest to replace OLD_LOOPNEST with.
   TRANSFORM is the matrix transform that was applied to OLD_LOOPNEST to get 
   NEW_LOOPNEST.  */

void
lambda_loopnest_to_gcc_loopnest (struct loop *old_loopnest,
				 VEC(tree) *old_ivs,
				 VEC(tree) *invariants,
				 lambda_loopnest new_loopnest,
				 lambda_trans_matrix transform)
{

  struct loop *temp;
  size_t i = 0;
  size_t depth = 0;
  VEC(tree) *new_ivs = NULL;
  tree oldiv;
  
  block_stmt_iterator bsi;

  if (dump_file)
    {
      transform = lambda_trans_matrix_inverse (transform);
      fprintf (dump_file, "Inverse of transformation matrix:\n");
      print_lambda_trans_matrix (dump_file, transform);
    }
  depth = depth_of_nest (old_loopnest);
  temp = old_loopnest;

  while (temp)
    {
      lambda_loop newloop;
      basic_block bb;
      edge exit;
      tree ivvar, ivvarinced, exitcond, stmts;
      enum tree_code testtype;
      tree newupperbound, newlowerbound;
      lambda_linear_expression offset;
      tree type;
      bool insert_after;

      oldiv = VEC_index (tree, old_ivs, i);
      type = TREE_TYPE (oldiv);

      /* First, build the new induction variable temporary  */

      ivvar = create_tmp_var (type, "lnivtmp");
      add_referenced_tmp_var (ivvar);

      VEC_safe_push (tree, new_ivs, ivvar);

      newloop = LN_LOOPS (new_loopnest)[i];

      /* Linear offset is a bit tricky to handle.  Punt on the unhandled
         cases for now.  */
      offset = LL_LINEAR_OFFSET (newloop);
      
      gcc_assert (LLE_DENOMINATOR (offset) == 1 &&
		  lambda_vector_zerop (LLE_COEFFICIENTS (offset), depth));
	    
      /* Now build the  new lower bounds, and insert the statements
         necessary to generate it on the loop preheader.  */
      newlowerbound = lle_to_gcc_expression (LL_LOWER_BOUND (newloop),
					     LL_LINEAR_OFFSET (newloop),
					     type,
					     new_ivs,
					     invariants, MAX_EXPR, &stmts);
      bsi_insert_on_edge (loop_preheader_edge (temp), stmts);
      bsi_commit_edge_inserts ();
      /* Build the new upper bound and insert its statements in the
         basic block of the exit condition */
      newupperbound = lle_to_gcc_expression (LL_UPPER_BOUND (newloop),
					     LL_LINEAR_OFFSET (newloop),
					     type,
					     new_ivs,
					     invariants, MIN_EXPR, &stmts);
      exit = temp->single_exit;
      exitcond = get_loop_exit_condition (temp);
      bb = bb_for_stmt (exitcond);
      bsi = bsi_start (bb);
      bsi_insert_after (&bsi, stmts, BSI_NEW_STMT);

      /* Create the new iv.  */

      standard_iv_increment_position (temp, &bsi, &insert_after);
      create_iv (newlowerbound,
		 build_int_cst (type, LL_STEP (newloop)),
		 ivvar, temp, &bsi, insert_after, &ivvar,
		 &ivvarinced);

      /* Replace the exit condition with the new upper bound
         comparison.  */
      
      testtype = LL_STEP (newloop) >= 0 ? LE_EXPR : GE_EXPR;
      
      /* We want to build a conditional where true means exit the loop, and
	 false means continue the loop.
	 So swap the testtype if this isn't the way things are.*/

      if (exit->flags & EDGE_FALSE_VALUE)
	testtype = swap_tree_comparison (testtype);

      COND_EXPR_COND (exitcond) = build (testtype,
					 boolean_type_node,
					 newupperbound, ivvarinced);
      modify_stmt (exitcond);
      VEC_replace (tree, new_ivs, i, ivvar);

      i++;
      temp = temp->inner;
    }

  /* Rewrite uses of the old ivs so that they are now specified in terms of
     the new ivs.  */

  for (i = 0; VEC_iterate (tree, old_ivs, i, oldiv); i++)
    {
      int j;
      dataflow_t imm = get_immediate_uses (SSA_NAME_DEF_STMT (oldiv));
      for (j = 0; j < num_immediate_uses (imm); j++)
	{
	  tree stmt = immediate_use (imm, j);
	  use_operand_p use_p;
	  ssa_op_iter iter;
	  gcc_assert (TREE_CODE (stmt) != PHI_NODE);
	  FOR_EACH_SSA_USE_OPERAND (use_p, stmt, iter, SSA_OP_USE)
	    {
	      if (USE_FROM_PTR (use_p) == oldiv)
		{
		  tree newiv, stmts;
		  lambda_body_vector lbv, newlbv;
		  /* Compute the new expression for the induction
		     variable.  */
		  depth = VEC_length (tree, new_ivs);
		  lbv = lambda_body_vector_new (depth);
		  LBV_COEFFICIENTS (lbv)[i] = 1;
		  
		  newlbv = lambda_body_vector_compute_new (transform, lbv);

		  newiv = lbv_to_gcc_expression (newlbv, TREE_TYPE (oldiv),
						 new_ivs, &stmts);
		  bsi = bsi_for_stmt (stmt);
		  /* Insert the statements to build that
		     expression.  */
		  bsi_insert_before (&bsi, stmts, BSI_SAME_STMT);
		  propagate_value (use_p, newiv);
		  modify_stmt (stmt);
		  
		}
	    }
	}
    }
}


/* Returns true when the vector V is lexicographically positive, in
   other words, when the first nonzero element is positive.  */

static bool
lambda_vector_lexico_pos (lambda_vector v, 
			  unsigned n)
{
  unsigned i;
  for (i = 0; i < n; i++)
    {
      if (v[i] == 0)
	continue;
      if (v[i] < 0)
	return false;
      if (v[i] > 0)
	return true;
    }
  return true;
}


/* Return TRUE if this is not interesting statement from the perspective of
   determining if we have a perfect loop nest.  */

static bool
not_interesting_stmt (tree stmt)
{
  /* Note that COND_EXPR's aren't interesting because if they were exiting the
     loop, we would have already failed the number of exits tests.  */
  if (TREE_CODE (stmt) == LABEL_EXPR
      || TREE_CODE (stmt) == GOTO_EXPR
      || TREE_CODE (stmt) == COND_EXPR)
    return true;
  return false;
}

/* Return TRUE if PHI uses DEF for it's in-the-loop edge for LOOP.  */

static bool
phi_loop_edge_uses_def (struct loop *loop, tree phi, tree def)
{
  int i;
  for (i = 0; i < PHI_NUM_ARGS (phi); i++)
    if (flow_bb_inside_loop_p (loop, PHI_ARG_EDGE (phi, i)->src))
      if (PHI_ARG_DEF (phi, i) == def)
	return true;
  return false;
}

/* Return TRUE if STMT is a use of PHI_RESULT.  */

static bool
stmt_uses_phi_result (tree stmt, tree phi_result)
{
  use_optype uses = STMT_USE_OPS (stmt);
  
  /* This is conservatively true, because we only want SIMPLE bumpers
     of the form x +- constant for our pass.  */
  if (NUM_USES (uses) != 1)
    return false;
  if (USE_OP (uses, 0) == phi_result)
    return true;
  
  return false;
}

/* STMT is a bumper stmt for LOOP if the version it defines is used in the
   in-loop-edge in a phi node, and the operand it uses is the result of that
   phi node. 
   I.E. i_29 = i_3 + 1
        i_3 = PHI (0, i_29);  */

static bool
stmt_is_bumper_for_loop (struct loop *loop, tree stmt)
{
  tree use;
  tree def;
  def_optype defs = STMT_DEF_OPS (stmt);
  dataflow_t imm;
  int i;
  
  if (NUM_DEFS (defs) != 1)
    return false;
  def = DEF_OP (defs, 0);
  imm = get_immediate_uses (stmt);
  for (i = 0; i < num_immediate_uses (imm); i++)
    {
      use = immediate_use (imm, i);
      if (TREE_CODE (use) == PHI_NODE)
	{
	  if (phi_loop_edge_uses_def (loop, use, def))
	    if (stmt_uses_phi_result (stmt, PHI_RESULT (use)))
	      return true;
	} 
    }
  return false;
}


/* Return true if LOOP is a perfect loop nest.
   Perfect loop nests are those loop nests where all code occurs in the
   innermost loop body.
   If S is a program statement, then

   i.e. 
   DO I = 1, 20
       S1
       DO J = 1, 20
       ...
       END DO
   END DO
   is not a perfect loop nest because of S1.
   
   DO I = 1, 20
      DO J = 1, 20
        S1
	...
      END DO
   END DO 
   is a perfect loop nest.  

   Since we don't have high level loops anymore, we basically have to walk our
   statements and ignore those that are there because the loop needs them (IE
   the induction variable increment, and jump back to the top of the loop).  */

bool
perfect_nest_p (struct loop *loop)
{
  basic_block *bbs;
  size_t i;
  tree exit_cond;

  if (!loop->inner)
    return true;
  bbs = get_loop_body (loop);
  exit_cond = get_loop_exit_condition (loop);
  for (i = 0; i < loop->num_nodes; i++)
    {
      if (bbs[i]->loop_father == loop)
	{
	  block_stmt_iterator bsi;
	  for (bsi = bsi_start (bbs[i]); !bsi_end_p (bsi); bsi_next (&bsi))
	    {
	      tree stmt = bsi_stmt (bsi);
	      if (stmt == exit_cond
		  || not_interesting_stmt (stmt)
		  || stmt_is_bumper_for_loop (loop, stmt))
		continue;
	      free (bbs);
	      return false;
	    }
	}
    }
  free (bbs);
  /* See if the inner loops are perfectly nested as well.  */
  if (loop->inner)    
    return perfect_nest_p (loop->inner);
  return true;
}

/* Replace the USES of tree X in STMT with tree Y */

static void
replace_uses_of_x_with_y (tree stmt, tree x, tree y)
{
  use_optype uses = STMT_USE_OPS (stmt);
  size_t i;
  for (i = 0; i < NUM_USES (uses); i++)
    {
      if (USE_OP (uses, i) == x)
	SET_USE_OP (uses, i, y);
    }
}

/* Return TRUE if STMT uses tree OP in it's uses.  */

static bool
stmt_uses_op (tree stmt, tree op)
{
  use_optype uses = STMT_USE_OPS (stmt);
  size_t i;
  for (i = 0; i < NUM_USES (uses); i++)
    {
      if (USE_OP (uses, i) == op)
	return true;
    }
  return false;
}

/* Return TRUE if LOOP is an imperfect nest that we can convert to a perfect
   one.  LOOPIVS is a vector of induction variables, one per loop.  
   ATM, we only handle imperfect nests of depth 2, where all of the statements
   occur after the inner loop.  */

static bool
can_convert_to_perfect_nest (struct loop *loop,
			     VEC (tree) *loopivs)
{
  basic_block *bbs;
  tree exit_condition, phi;
  size_t i;
  block_stmt_iterator bsi;
  basic_block exitdest;

  /* Can't handle triply nested+ loops yet.  */
  if (!loop->inner || loop->inner->inner)
    return false;
  
  /* We only handle moving the after-inner-body statements right now, so make
     sure all the statements we need to move are located in that position.  */
  bbs = get_loop_body (loop);
  exit_condition = get_loop_exit_condition (loop);
  for (i = 0; i < loop->num_nodes; i++)
    {
      if (bbs[i]->loop_father == loop)
	{
	  for (bsi = bsi_start (bbs[i]); !bsi_end_p (bsi); bsi_next (&bsi))
	    { 
	      size_t j;
	      tree stmt = bsi_stmt (bsi);
	      if (stmt == exit_condition
		  || not_interesting_stmt (stmt)
		  || stmt_is_bumper_for_loop (loop, stmt))
		continue;
	      /* If the statement uses inner loop ivs, we == screwed.  */
	      for (j = 1; j < VEC_length (tree, loopivs); j++)
		if (stmt_uses_op (stmt, VEC_index (tree, loopivs, j)))
		  {
		    free (bbs);
		    return false;
		  }
	      
	      /* If the bb of a statement we care about isn't dominated by 
		 the header of the inner loop, then we are also screwed.  */
	      if (!dominated_by_p (CDI_DOMINATORS,
				   bb_for_stmt (stmt), 
				   loop->inner->header))
		{
		  free (bbs);
		  return false;
		}
	    }
	}
    }  

  /* We also need to make sure the loop exit only has simple copy phis in it,
     otherwise we don't know how to transform it into a perfect nest right
     now.  */
  exitdest = loop->single_exit->dest;
  
  for (phi = phi_nodes (exitdest); phi; phi = PHI_CHAIN (phi))
    if (PHI_NUM_ARGS (phi) != 1)
      return false;

  return true;
}

/* Transform the loop nest into a perfect nest, if possible.
   LOOPS is the current struct loops *
   LOOP is the loop nest to transform into a perfect nest
   LBOUNDS are the lower bounds for the loops to transform
   UBOUNDS are the upper bounds for the loops to transform
   STEPS is the STEPS for the loops to transform.
   LOOPIVS is the induction variables for the loops to transform.
   
   Basically, for the case of

   FOR (i = 0; i < 50; i++)
    {
     FOR (j =0; j < 50; j++)
     {
        <whatever>
     }
     <some code>
    }

   This function will transform it into a perfect loop nest by splitting the
   outer loop into two loops, like so:

   FOR (i = 0; i < 50; i++)
   {
     FOR (j = 0; j < 50; j++)
     {
         <whatever>
     }
   }
   
   FOR (i = 0; i < 50; i ++)
   {
    <some code>
   }

   Return FALSE if we can't make this loop into a perfect nest.  */
static bool
perfect_nestify (struct loops *loops,
		 struct loop *loop,
		 VEC (tree) *lbounds,
		 VEC (tree) *ubounds,
		 VEC (int) *steps,
		 VEC (tree) *loopivs)
{
  basic_block *bbs;
  tree exit_condition;
  tree then_label, else_label, cond_stmt;
  basic_block preheaderbb, headerbb, bodybb, latchbb, olddest;
  size_t i;
  block_stmt_iterator bsi;
  bool insert_after;
  edge e;
  struct loop *newloop;
  tree phi;
  tree uboundvar;
  tree stmt;
  tree oldivvar, ivvar, ivvarinced;
  VEC (tree) *phis = NULL;

  if (!can_convert_to_perfect_nest (loop, loopivs))
    return false;

  /* Create the new loop */

  olddest = loop->single_exit->dest;
  preheaderbb =  loop_split_edge_with (loop->single_exit, NULL);
  headerbb = create_empty_bb (EXIT_BLOCK_PTR->prev_bb);
  
  /* Push the exit phi nodes that we are moving.  */
  for (phi = phi_nodes (olddest); phi; phi = PHI_CHAIN (phi))
    {
      VEC_safe_push (tree, phis, PHI_RESULT (phi));
      VEC_safe_push (tree, phis, PHI_ARG_DEF (phi, 0));
    }
  e = redirect_edge_and_branch (EDGE_SUCC (preheaderbb, 0), headerbb);

  /* Remove the exit phis from the old basic block.  Make sure to set
     PHI_RESULT to null so it doesn't get released.  */
  while (phi_nodes (olddest) != NULL)
    {
      SET_PHI_RESULT (phi_nodes (olddest), NULL);
      remove_phi_node (phi_nodes (olddest), NULL, olddest);
    }      

  /* and add them back to the new basic block.  */
  while (VEC_length (tree, phis) != 0)
    {
      tree def;
      tree phiname;
      def = VEC_pop (tree, phis);
      phiname = VEC_pop (tree, phis);      
      phi = create_phi_node (phiname, preheaderbb);
      add_phi_arg (phi, def, EDGE_PRED (preheaderbb, 0));
    }       
  flush_pending_stmts (e);

  bodybb = create_empty_bb (EXIT_BLOCK_PTR->prev_bb);
  latchbb = create_empty_bb (EXIT_BLOCK_PTR->prev_bb);
  make_edge (headerbb, bodybb, EDGE_FALLTHRU); 
  then_label = build1 (GOTO_EXPR, void_type_node, tree_block_label (latchbb));
  else_label = build1 (GOTO_EXPR, void_type_node, tree_block_label (olddest));
  cond_stmt = build (COND_EXPR, void_type_node,
		     build (NE_EXPR, boolean_type_node, 
			    integer_one_node, 
			    integer_zero_node), 
		     then_label, else_label);
  bsi = bsi_start (bodybb);
  bsi_insert_after (&bsi, cond_stmt, BSI_NEW_STMT);
  e = make_edge (bodybb, olddest, EDGE_FALSE_VALUE);
  make_edge (bodybb, latchbb, EDGE_TRUE_VALUE);
  make_edge (latchbb, headerbb, EDGE_FALLTHRU);

  /* Update the loop structures.  */
  newloop = duplicate_loop (loops, loop, olddest->loop_father);  
  newloop->header = headerbb;
  newloop->latch = latchbb;
  newloop->single_exit = e;
  add_bb_to_loop (latchbb, newloop);
  add_bb_to_loop (bodybb, newloop);
  add_bb_to_loop (headerbb, newloop);
  add_bb_to_loop (preheaderbb, olddest->loop_father);
  set_immediate_dominator (CDI_DOMINATORS, bodybb, headerbb);
  set_immediate_dominator (CDI_DOMINATORS, headerbb, preheaderbb);
  set_immediate_dominator (CDI_DOMINATORS, preheaderbb, 
			   loop->single_exit->src);
  set_immediate_dominator (CDI_DOMINATORS, latchbb, bodybb);
  set_immediate_dominator (CDI_DOMINATORS, olddest, bodybb);
  /* Create the new iv.  */
  ivvar = create_tmp_var (integer_type_node, "perfectiv");
  add_referenced_tmp_var (ivvar);
  standard_iv_increment_position (newloop, &bsi, &insert_after);
  create_iv (VEC_index (tree, lbounds, 0),
	     build_int_cst (integer_type_node, VEC_index (int, steps, 0)),
	     ivvar, newloop, &bsi, insert_after, &ivvar, &ivvarinced);	     

  /* Create the new upper bound.  This may be not just a variable, so we copy
     it to one just in case.  */

  exit_condition = get_loop_exit_condition (newloop);
  uboundvar = create_tmp_var (integer_type_node, "uboundvar");
  add_referenced_tmp_var (uboundvar);
  stmt = build (MODIFY_EXPR, void_type_node, uboundvar, 
		VEC_index (tree, ubounds, 0));
  uboundvar = make_ssa_name (uboundvar, stmt);
  TREE_OPERAND (stmt, 0) = uboundvar;

  if (insert_after)
    bsi_insert_after (&bsi, stmt, BSI_SAME_STMT);
  else
    bsi_insert_before (&bsi, stmt, BSI_SAME_STMT);

  COND_EXPR_COND (exit_condition) = build (GE_EXPR, 
					   boolean_type_node,
					   uboundvar,
					   ivvarinced);

  bbs = get_loop_body (loop); 
  /* Now replace the induction variable in the moved statements with the
     correct loop induction variable.  */
  oldivvar = VEC_index (tree, loopivs, 0);
  for (i = 0; i < loop->num_nodes; i++)
    {
      block_stmt_iterator tobsi = bsi_last (bodybb);
      if (bbs[i]->loop_father == loop)
	{
	  /* Note that the bsi only needs to be explicitly incremented
	     when we don't move something, since it is automatically
	     incremented when we do.  */
	  for (bsi = bsi_start (bbs[i]); !bsi_end_p (bsi);)
	    { 
	      tree stmt = bsi_stmt (bsi);
	      if (stmt == exit_condition
		  || not_interesting_stmt (stmt)
		  || stmt_is_bumper_for_loop (loop, stmt))
		{
		  bsi_next (&bsi);
		  continue;
		}
	      replace_uses_of_x_with_y (stmt, oldivvar, ivvar);
	      bsi_move_before (&bsi, &tobsi);
	    }
	}
    }
  free (bbs);
  flow_loops_find (loops, LOOP_ALL);
  return perfect_nest_p (loop);
}

/* Return true if TRANS is a legal transformation matrix that respects
   the dependence vectors in DISTS and DIRS.  The conservative answer
   is false.

   "Wolfe proves that a unimodular transformation represented by the
   matrix T is legal when applied to a loop nest with a set of
   lexicographically non-negative distance vectors RDG if and only if
   for each vector d in RDG, (T.d >= 0) is lexicographically positive.
   i.e.: if and only if it transforms the lexicographically positive
   distance vectors to lexicographically positive vectors.  Note that
   a unimodular matrix must transform the zero vector (and only it) to
   the zero vector." S.Muchnick.  */

bool
lambda_transform_legal_p (lambda_trans_matrix trans, 
			  int nb_loops,
			  varray_type dependence_relations)
{
  unsigned int i;
  lambda_vector distres;
  struct data_dependence_relation *ddr;

#if defined ENABLE_CHECKING
  if (LTM_COLSIZE (trans) != nb_loops
      || LTM_ROWSIZE (trans) != nb_loops)
    abort ();
#endif

  /* When there is an unknown relation in the dependence_relations, we
     know that it is no worth looking at this loop nest: give up.  */
  ddr = (struct data_dependence_relation *) 
    VARRAY_GENERIC_PTR (dependence_relations, 0);
  if (ddr == NULL)
    return true;
  if (DDR_ARE_DEPENDENT (ddr) == chrec_dont_know)
    return false;

  distres = lambda_vector_new (nb_loops);

  /* For each distance vector in the dependence graph.  */
  for (i = 0; i < VARRAY_ACTIVE_SIZE (dependence_relations); i++)
    {
      ddr = (struct data_dependence_relation *) 
	VARRAY_GENERIC_PTR (dependence_relations, i);     

      /* Don't care about relations for which we know that there is no
	 dependence, nor about read-read (aka. output-dependences):
	 these data accesses can happen in any order.  */
      if (DDR_ARE_DEPENDENT (ddr) == chrec_known
	  || (DR_IS_READ (DDR_A (ddr)) && DR_IS_READ (DDR_B (ddr))))
	continue;

      /* Conservatively answer: "this transformation is not valid".  */
      if (DDR_ARE_DEPENDENT (ddr) == chrec_dont_know)
	return false;
	  
      /* If the dependence could not be captured by a distance vector,
	 conservatively answer that the transform is not valid.  */
      if (DDR_DIST_VECT (ddr) == NULL)
	return false;

      /* Compute trans.dist_vect */
      lambda_matrix_vector_mult (LTM_MATRIX (trans), nb_loops, nb_loops, 
				 DDR_DIST_VECT (ddr), distres);

      if (!lambda_vector_lexico_pos (distres, nb_loops))
	return false;
    }
  return true;
}
