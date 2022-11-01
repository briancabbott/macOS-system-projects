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
 *	alltoall_intra
 *
 *	Function:	- MPI_Alltoall 
 *	Accepts:	- same as MPI_Alltoall()
 *	Returns:	- MPI_SUCCESS or an MPI error code
 */
int mca_coll_self_alltoall_intra(void *sbuf, int scount,
                                 struct ompi_datatype_t *sdtype, 
                                 void *rbuf, int rcount, 
                                 struct ompi_datatype_t *rdtype,
                                 struct ompi_communicator_t *comm)
{
    return ompi_ddt_sndrcv(sbuf, scount, sdtype,
                           rbuf, rcount, rdtype);
}
