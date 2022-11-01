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

#include "ompi/constants.h"
#include "ompi/datatype/datatype.h"
#include "coll_self.h"


/*
 *	alltoallw_intra
 *
 *	Function:	- MPI_Alltoallw
 *	Accepts:	- same as MPI_Alltoallw()
 *	Returns:	- MPI_SUCCESS or an MPI error code
 */
int mca_coll_self_alltoallw_intra(void *sbuf, int *scounts, int *sdisps,
                                  struct ompi_datatype_t **sdtypes, 
                                  void *rbuf, int *rcounts, int *rdisps,
                                  struct ompi_datatype_t **rdtypes, 
                                  struct ompi_communicator_t *comm)
{
    int err;        
    ptrdiff_t lb, rextent, sextent;
    err = ompi_ddt_get_extent(sdtypes[0], &lb, &sextent);
    if (OMPI_SUCCESS != err) {
        return OMPI_ERROR;
    }
    err = ompi_ddt_get_extent(rdtypes[0], &lb, &rextent);
    if (OMPI_SUCCESS != err) {
        return OMPI_ERROR;
    }

    return ompi_ddt_sndrcv(((char *) sbuf) + sdisps[0] * sextent, 
                           scounts[0], sdtypes[0],
                           ((char *) rbuf) + rdisps[0] * rextent, 
                           rcounts[0], rdtypes[0]);
}
