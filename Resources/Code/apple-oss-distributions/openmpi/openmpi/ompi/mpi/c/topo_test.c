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
#include <stdio.h>

#include "ompi/mpi/c/bindings.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Topo_test = PMPI_Topo_test
#endif

#if OMPI_PROFILING_DEFINES
#include "ompi/mpi/c/profile/defines.h"
#endif

static const char FUNC_NAME[] = "MPI_Topo_test";


int MPI_Topo_test(MPI_Comm comm, int *status) 
{
    if ( MPI_PARAM_CHECK ) {
        OMPI_ERR_INIT_FINALIZE(FUNC_NAME);
        if (ompi_comm_invalid (comm)) {
            return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_COMM, 
                                          FUNC_NAME);
        } else if ( NULL == status ) {
            return OMPI_ERRHANDLER_INVOKE(comm, MPI_ERR_ARG, FUNC_NAME);
        }
    }

    if ( comm->c_flags & OMPI_COMM_CART ) {
        *status = MPI_CART;
    } else if ( comm->c_flags & OMPI_COMM_GRAPH ) {
        *status = MPI_GRAPH;
    } else {
        *status = MPI_UNDEFINED;
    }

    return MPI_SUCCESS;
}
