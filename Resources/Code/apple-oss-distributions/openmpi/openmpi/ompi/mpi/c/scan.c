/*
 * Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
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
#include "ompi/datatype/datatype.h"
#include "ompi/op/op.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Scan = PMPI_Scan
#endif

#if OMPI_PROFILING_DEFINES
#include "ompi/mpi/c/profile/defines.h"
#endif

static const char FUNC_NAME[] = "MPI_Scan";


int MPI_Scan(void *sendbuf, void *recvbuf, int count,
             MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) 
{
    int err;

    if (MPI_PARAM_CHECK) {
        char *msg;
        err = MPI_SUCCESS;
        OMPI_ERR_INIT_FINALIZE(FUNC_NAME);
        if (ompi_comm_invalid(comm)) {
            return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_COMM, 
                                          FUNC_NAME);
        }

        /* No intercommunicators allowed! (MPI does not define
           MPI_SCAN on intercommunicators) */

        else if (OMPI_COMM_IS_INTER(comm)) {
          err = MPI_ERR_COMM;
        }

        /* Unrooted operation; checks for all ranks */

        else if (MPI_OP_NULL == op) {
          err = MPI_ERR_OP;
        } else if (MPI_IN_PLACE == recvbuf) {
          err = MPI_ERR_ARG;
        } else if (!ompi_op_is_valid(op, datatype, &msg, FUNC_NAME)) {
            int ret = OMPI_ERRHANDLER_INVOKE(comm, MPI_ERR_OP, msg);
            free(msg);
            return ret;
        } else {
          OMPI_CHECK_DATATYPE_FOR_SEND(err, datatype, count);
        }
        OMPI_ERRHANDLER_CHECK(err, comm, err, FUNC_NAME);
    }

    /* Do we need to do anything? (MPI says that reductions have to
       have a count of at least 1, but at least IMB calls reduce with
       a count of 0 -- blah!) */

    if (0 == count) {
        return MPI_SUCCESS;
    }

    /* Call the coll component to actually perform the allgather */

    OBJ_RETAIN(op);
    err = comm->c_coll.coll_scan(sendbuf, recvbuf, count,
                                 datatype, op, comm);
    OBJ_RELEASE(op);
    OMPI_ERRHANDLER_RETURN(err, comm, err, FUNC_NAME);
}
