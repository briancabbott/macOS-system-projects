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
#include "ompi/mca/topo/base/base.h"
#include "ompi/communicator/communicator.h"
#include "ompi/mca/topo/topo.h"

/*
 * function - retrieves Cartesian topology information associated with a
 *            communicator
 *
 * @param comm communicator with cartesian structure (handle)
 * @param ndims number of dimensions of the cartesian structure (integer)
 *
 * @retval MPI_SUCCESS
 * @retval MPI_ERR_COMM
 */
int mca_topo_base_cartdim_get (MPI_Comm comm,
                           int *ndims){
 
    *ndims = comm->c_topo_comm->mtc_ndims_or_nnodes;
    return MPI_SUCCESS;
}


