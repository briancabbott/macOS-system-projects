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
#include "ompi/attribute/attribute.h"
#include "ompi/communicator/communicator.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_COMM_GET_ATTR = mpi_comm_get_attr_f
#pragma weak pmpi_comm_get_attr = mpi_comm_get_attr_f
#pragma weak pmpi_comm_get_attr_ = mpi_comm_get_attr_f
#pragma weak pmpi_comm_get_attr__ = mpi_comm_get_attr_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_COMM_GET_ATTR,
                           pmpi_comm_get_attr,
                           pmpi_comm_get_attr_,
                           pmpi_comm_get_attr__,
                           pmpi_comm_get_attr_f,
                           (MPI_Fint *comm, MPI_Fint *comm_keyval, MPI_Aint *attribute_val, MPI_Flogical *flag, MPI_Fint *ierr),
                           (comm, comm_keyval, attribute_val, flag, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_COMM_GET_ATTR = mpi_comm_get_attr_f
#pragma weak mpi_comm_get_attr = mpi_comm_get_attr_f
#pragma weak mpi_comm_get_attr_ = mpi_comm_get_attr_f
#pragma weak mpi_comm_get_attr__ = mpi_comm_get_attr_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_COMM_GET_ATTR,
                           mpi_comm_get_attr,
                           mpi_comm_get_attr_,
                           mpi_comm_get_attr__,
                           mpi_comm_get_attr_f,
                           (MPI_Fint *comm, MPI_Fint *comm_keyval, MPI_Aint *attribute_val, MPI_Flogical *flag, MPI_Fint *ierr),
                           (comm, comm_keyval, attribute_val, flag, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "ompi/mpi/f77/profile/defines.h"
#endif

void mpi_comm_get_attr_f(MPI_Fint *comm, MPI_Fint *comm_keyval,
                         MPI_Aint *attribute_val, MPI_Flogical *flag,
                         MPI_Fint *ierr)
{
    int c_err;
    MPI_Comm c_comm = MPI_Comm_f2c(*comm);
    OMPI_LOGICAL_NAME_DECL(flag);

    /* This stuff is very confusing.  Be sure to see the comment at
       the top of src/attributes/attributes.c. */

    c_err = ompi_attr_get_fortran_mpi2(c_comm->c_keyhash,
                                       OMPI_FINT_2_INT(*comm_keyval),
                                       attribute_val,
                                       OMPI_LOGICAL_SINGLE_NAME_CONVERT(flag));
    *ierr = OMPI_INT_2_FINT(c_err);
    OMPI_SINGLE_INT_2_LOGICAL(flag);
}
