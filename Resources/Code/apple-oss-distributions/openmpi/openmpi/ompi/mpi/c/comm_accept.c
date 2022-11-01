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
#include <stdio.h>

#include "ompi/mpi/c/bindings.h"
#include "ompi/info/info.h"

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILING_DEFINES
#pragma weak MPI_Comm_accept = PMPI_Comm_accept
#endif

#if OMPI_PROFILING_DEFINES
#include "ompi/mpi/c/profile/defines.h"
#endif

static const char FUNC_NAME[] = "MPI_Comm_accept";


int MPI_Comm_accept(char *port_name, MPI_Info info, int root,
                    MPI_Comm comm, MPI_Comm *newcomm) 
{
    int rank, rc;
    int send_first=0; /*wrong, we receive first */
    ompi_communicator_t *newcomp=MPI_COMM_NULL;
    char *tmp_port=NULL;
    orte_rml_tag_t tag;

    if ( MPI_PARAM_CHECK ) {
        OMPI_ERR_INIT_FINALIZE(FUNC_NAME);

        if (ompi_comm_invalid (comm)) {
            return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_COMM, 
                                          FUNC_NAME);
        }
        if ( OMPI_COMM_IS_INTER(comm)) {
            return OMPI_ERRHANDLER_INVOKE(comm, MPI_ERR_COMM,
                                          FUNC_NAME);
        }
        if ( (0 > root) || (ompi_comm_size(comm) <= root) ) {
            return OMPI_ERRHANDLER_INVOKE(comm, MPI_ERR_ARG, 
                                          FUNC_NAME);
        }
        if ( NULL == newcomm ) {
            return OMPI_ERRHANDLER_INVOKE(comm, MPI_ERR_ARG, 
                                          FUNC_NAME);
        }
        if (NULL == info || ompi_info_is_freed(info)) {
          return OMPI_ERRHANDLER_INVOKE(MPI_COMM_WORLD, MPI_ERR_INFO,
                                        FUNC_NAME);
        }
    }
    
    rank = ompi_comm_rank ( comm );
    if ( MPI_PARAM_CHECK ) {
        if ( rank == root ) {
            if ( NULL == port_name ) 
                return OMPI_ERRHANDLER_INVOKE(comm, MPI_ERR_ARG, 
                                              FUNC_NAME);
        }
    }

    /* parse info object. no prefedined values for this function in MPI-2 
     * so lets ignore it for the moment.
     * if ( rank == root && MPI_INFO_NULL != info ) {
     * }
     */
    
    /*
     * Our own port_name is not of interest here, so we pass in NULL.
     * The two leaders will figure this out later. However, we need the tag.
     */
    if ( rank == root ) {
	    tmp_port = ompi_parse_port(port_name, &tag);
	    free (tmp_port);
    }
    rc = ompi_comm_connect_accept (comm, root, NULL, send_first, &newcomp, tag);

    *newcomm = newcomp;
    OMPI_ERRHANDLER_RETURN(rc, comm, rc, FUNC_NAME );
}
