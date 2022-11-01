/* Loop Vectorization
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
   Contributed by Dorit Naishlos <dorit@il.ibm.com>

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

#ifndef GCC_TREE_VECTORIZER_H
#define GCC_TREE_VECTORIZER_H

/* Used for naming of new temporaries.  */
enum vect_var_kind {
  vect_simple_var,
  vect_pointer_var
};

/* Defines type of operation: unary or binary.  */
enum operation_type {
  unary_op = 1,
  binary_op
};

/*-----------------------------------------------------------------*/
/* Info on vectorized defs.                                        */
/*-----------------------------------------------------------------*/
enum stmt_vec_info_type {
  undef_vec_info_type = 0,
  load_vec_info_type,
  store_vec_info_type,
  op_vec_info_type,
  assignment_vec_info_type
};

typedef struct _stmt_vec_info {

  enum stmt_vec_info_type type;

  /* The stmt to which this info struct refers to.  */
  tree stmt;

  /* The loop with respect to which STMT is vectorized.  */
  struct loop *loop;

  /* Not all stmts in the loop need to be vectorized. e.g, the incrementation
     of the loop induction variable and computation of array indexes. relevant
     indicates whether the stmt needs to be vectorized.  */
  bool relevant;

  /* The vector type to be used.  */
  tree vectype;

  /* The vectorized version of the stmt.  */
  tree vectorized_stmt;


  /** The following is relevant only for stmts that contain a non-scalar
     data-ref (array/pointer/struct access). A GIMPLE stmt is expected to have 
     at most one such data-ref.  **/

  /* Information about the data-ref (access function, etc).  */
  struct data_reference *data_ref_info;

  /* Aliasing information.  */
  tree memtag;

  /* Data reference base. This field holds the entire invariant part of the 
     data-reference (with respect to the relevant loop), as opposed to the 
     field DR_BASE of the STMT_VINFO_DATA_REF struct, which holds only the 
     initial base; e.g:
     REF	BR_BASE	    VECT_DR_BASE
     a[i]	a		a
     a[i][j]	a		a[i]  */
  tree vect_dr_base;
} *stmt_vec_info;

/* Access Functions.  */
#define STMT_VINFO_TYPE(S)          (S)->type
#define STMT_VINFO_STMT(S)          (S)->stmt
#define STMT_VINFO_LOOP(S)          (S)->loop
#define STMT_VINFO_RELEVANT_P(S)    (S)->relevant
#define STMT_VINFO_VECTYPE(S)       (S)->vectype
#define STMT_VINFO_VEC_STMT(S)      (S)->vectorized_stmt
#define STMT_VINFO_DATA_REF(S)      (S)->data_ref_info
#define STMT_VINFO_MEMTAG(S)        (S)->memtag
#define STMT_VINFO_VECT_DR_BASE(S)  (S)->vect_dr_base

static inline void set_stmt_info (stmt_ann_t ann, stmt_vec_info stmt_info);
static inline stmt_vec_info vinfo_for_stmt (tree stmt);

static inline void
set_stmt_info (stmt_ann_t ann, stmt_vec_info stmt_info)
{
  if (ann)
    ann->common.aux = (char *) stmt_info;
}

static inline stmt_vec_info
vinfo_for_stmt (tree stmt)
{
  stmt_ann_t ann = stmt_ann (stmt);
  return ann ? (stmt_vec_info) ann->common.aux : NULL;
}

/*-----------------------------------------------------------------*/
/* Info on data references alignment.                              */
/*-----------------------------------------------------------------*/

/* The misalignment of the memory access in bytes.  */
#define DR_MISALIGNMENT(DR)   (DR)->aux

static inline bool
aligned_access_p (struct data_reference *data_ref_info)
{
  return (DR_MISALIGNMENT (data_ref_info) == 0);
}

static inline bool
unknown_alignment_for_access_p (struct data_reference *data_ref_info)
{
  return (DR_MISALIGNMENT (data_ref_info) == -1);
}

/* Perform signed modulo, always returning a non-negative value.  */
#define VECT_SMODULO(x,y) ((x) % (y) < 0 ? ((x) % (y) + (y)) : (x) % (y))


/*-----------------------------------------------------------------*/
/* Info on vectorized loops.                                       */
/*-----------------------------------------------------------------*/
typedef struct _loop_vec_info {

  /* The loop to which this info struct refers to.  */
  struct loop *loop;

  /* The loop basic blocks.  */
  basic_block *bbs;

  /* The loop exit_condition.  */
  tree exit_cond;

  /* Number of iterations. -1 if unknown.  */
  HOST_WIDE_INT num_iters;

  /* Is the loop vectorizable? */
  bool vectorizable;

  /* Unrolling factor  */
  int vectorization_factor;

  /* All data references in the loop that are being written to.  */
  varray_type data_ref_writes;

  /* All data references in the loop that are being read from.  */
  varray_type data_ref_reads;
} *loop_vec_info;

/* Access Functions.  */
#define LOOP_VINFO_LOOP(L)           (L)->loop
#define LOOP_VINFO_BBS(L)            (L)->bbs
#define LOOP_VINFO_EXIT_COND(L)      (L)->exit_cond
#define LOOP_VINFO_NITERS(L)         (L)->num_iters
#define LOOP_VINFO_VECTORIZABLE_P(L) (L)->vectorizable
#define LOOP_VINFO_VECT_FACTOR(L)    (L)->vectorization_factor
#define LOOP_VINFO_DATAREF_WRITES(L) (L)->data_ref_writes
#define LOOP_VINFO_DATAREF_READS(L)  (L)->data_ref_reads

#define LOOP_VINFO_NITERS_KNOWN_P(L) ((L)->num_iters > 0)

/*-----------------------------------------------------------------*/
/* Function prototypes.                                            */
/*-----------------------------------------------------------------*/

/* Main driver.  */
extern void vectorize_loops (struct loops *);

/* creation and deletion of loop and stmt info structs.  */
extern loop_vec_info new_loop_vec_info (struct loop *loop);
extern void destroy_loop_vec_info (loop_vec_info);
extern stmt_vec_info new_stmt_vec_info (tree stmt, struct loop *loop);

#endif  /* GCC_TREE_VECTORIZER_H  */
