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

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_COMM_GET_ERRHANDLER = mpi_comm_get_errhandler_f
#pragma weak pmpi_comm_get_errhandler = mpi_comm_get_errhandler_f
#pragma weak pmpi_comm_get_errhandler_ = mpi_comm_get_errhandler_f
#pragma weak pmpi_comm_get_errhandler__ = mpi_comm_get_errhandler_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_COMM_GET_ERRHANDLER,
                           pmpi_comm_get_errhandler,
                           pmpi_comm_get_errhandler_,
                           pmpi_comm_get_errhandler__,
                           pmpi_comm_get_errhandler_f,
                           (MPI_Fint *comm, MPI_Fint *erhandler, MPI_Fint *ierr),
                           (comm, erhandler, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_COMM_GET_ERRHANDLER = mpi_comm_get_errhandler_f
#pragma weak mpi_comm_get_errhandler = mpi_comm_get_errhandler_f
#pragma weak mpi_comm_get_errhandler_ = mpi_comm_get_errhandler_f
#pragma weak mpi_comm_get_errhandler__ = mpi_comm_get_errhandler_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_COMM_GET_ERRHANDLER,
                           mpi_comm_get_errhandler,
                           mpi_comm_get_errhandler_,
                           mpi_comm_get_errhandler__,
                           mpi_comm_get_errhandler_f,
                           (MPI_Fint *comm, MPI_Fint *erhandler, MPI_Fint *ierr),
                           (comm, erhandler, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "ompi/mpi/f77/profile/defines.h"
#endif

void mpi_comm_get_errhandler_f(MPI_Fint *comm, MPI_Fint *errhandler, 
			       MPI_Fint *ierr)
{
    MPI_Comm c_comm;
    MPI_Errhandler c_errhandler;

    c_comm = MPI_Comm_f2c(*comm);

    *ierr = OMPI_INT_2_FINT(MPI_Comm_get_errhandler(c_comm, &c_errhandler));
    if (MPI_SUCCESS == OMPI_FINT_2_INT(*ierr)) {
        *errhandler = MPI_Errhandler_c2f(c_errhandler);
    }
}
