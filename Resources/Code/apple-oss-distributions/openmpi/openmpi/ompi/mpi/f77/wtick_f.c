/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
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

#include "ompi_config.h"

#include "ompi/mpi/f77/bindings.h"

/* The OMPI_GENERATE_F77_BINDINGS work only for the most common F77 bindings, the
 * one that does not return any value. There are 2 exceptions MPI_Wtick and MPI_Wtime.
 * For these 2 we can insert the bindings manually.
 */
#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_WTICK = mpi_wtick_f
#pragma weak pmpi_wtick = mpi_wtick_f
#pragma weak pmpi_wtick_ = mpi_wtick_f
#pragma weak pmpi_wtick__ = mpi_wtick_f
#elif OMPI_PROFILE_LAYER
double PMPI_WTICK(void) { return pmpi_wtick_f(); }
double pmpi_wtick(void) { return pmpi_wtick_f(); }
double pmpi_wtick_(void) { return pmpi_wtick_f(); }
double pmpi_wtick__(void) { return pmpi_wtick_f(); }
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_WTICK = mpi_wtick_f
#pragma weak mpi_wtick = mpi_wtick_f
#pragma weak mpi_wtick_ = mpi_wtick_f
#pragma weak mpi_wtick__ = mpi_wtick_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
double MPI_WTICK(void) { return mpi_wtick_f(); }
double mpi_wtick(void) { return mpi_wtick_f(); }
double mpi_wtick_(void) { return mpi_wtick_f(); }
double mpi_wtick__(void) { return mpi_wtick_f(); }
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "ompi/mpi/f77/profile/defines.h"
#endif

double mpi_wtick_f(void)
{
    return MPI_Wtick();
}
