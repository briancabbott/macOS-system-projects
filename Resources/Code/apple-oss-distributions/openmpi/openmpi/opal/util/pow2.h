/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef OPAL_POW2_H
#define OPAL_POW2_H
	
#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

/**
 *  This routine takes in an interger, and returns the nearest
 *  power of 2 integer, same or greater than the input.
 *
 *  @param input_integer input value
 *
 *  @returnvalue power of 2 integer same or greater than the input
 *               parameter.
 */
OPAL_DECLSPEC int opal_round_up_to_nearest_pow2(int input_integer);

#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

#endif /* OPAL_POW2_H */
