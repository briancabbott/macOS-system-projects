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
 * Copyright (c) 2006      Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "ompi_config.h"

#include "ompi/mpi/f77/bindings.h"
#include "ompi/mpi/f77/strings.h"
#include "ompi/constants.h"
#include "ompi/communicator/communicator.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_COMM_GET_NAME = mpi_comm_get_name_f
#pragma weak pmpi_comm_get_name = mpi_comm_get_name_f
#pragma weak pmpi_comm_get_name_ = mpi_comm_get_name_f
#pragma weak pmpi_comm_get_name__ = mpi_comm_get_name_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_COMM_GET_NAME,
                            pmpi_comm_get_name,
                            pmpi_comm_get_name_,
                            pmpi_comm_get_name__,
                            pmpi_comm_get_name_f,
                            (MPI_Fint *comm, char *comm_name, MPI_Fint *resultlen, MPI_Fint *ierr, int name_len),
                            (comm, comm_name, resultlen, ierr, name_len) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_COMM_GET_NAME = mpi_comm_get_name_f
#pragma weak mpi_comm_get_name = mpi_comm_get_name_f
#pragma weak mpi_comm_get_name_ = mpi_comm_get_name_f
#pragma weak mpi_comm_get_name__ = mpi_comm_get_name_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_COMM_GET_NAME,
                            mpi_comm_get_name,
                            mpi_comm_get_name_,
                            mpi_comm_get_name__,
                            mpi_comm_get_name_f,
                            (MPI_Fint *comm, char *comm_name, MPI_Fint *resultlen, MPI_Fint *ierr, int name_len),
                            (comm, comm_name, resultlen, ierr, name_len) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "ompi/mpi/f77/profile/defines.h"
#endif

void mpi_comm_get_name_f(MPI_Fint *comm, char *comm_name,
			 MPI_Fint *resultlen, MPI_Fint *ierr,
                         int name_len)
{
    int err, c_len;
    MPI_Comm c_comm = MPI_Comm_f2c(*comm);
    char c_name[MPI_MAX_OBJECT_NAME];

    err = MPI_Comm_get_name(c_comm, c_name, &c_len);
    if (MPI_SUCCESS == err) {
        ompi_fortran_string_c2f(c_name, comm_name, name_len);
        *resultlen = OMPI_INT_2_FINT(c_len);
        *ierr = OMPI_INT_2_FINT(MPI_SUCCESS);
    } else {
        *ierr = OMPI_INT_2_FINT(err);
    }
}
