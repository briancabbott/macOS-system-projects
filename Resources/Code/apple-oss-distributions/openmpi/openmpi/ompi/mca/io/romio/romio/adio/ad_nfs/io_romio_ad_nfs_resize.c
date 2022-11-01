/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 *
 *   Copyright (C) 2004 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#include "ad_nfs.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* NFS resize
 *
 * Note: we resize on all processors to guarantee that all processors
 * will have updated cache values.  This used to be the generic
 * implementation used by the majority of the ADIO implementations.
 */
void ADIOI_NFS_Resize(ADIO_File fd, ADIO_Offset size, int *error_code)
{
    int err;
    static char myname[] = "ADIOI_GEN_RESIZE";

    err = ftruncate(fd->fd_sys, size);

    /* --BEGIN ERROR HANDLING-- */
    if (err == -1) {
	*error_code = MPIO_Err_create_code(MPI_SUCCESS, MPIR_ERR_RECOVERABLE,
					   myname, __LINE__, MPI_ERR_IO,
					   "**io", "**io %s", strerror(errno));
	return;
    }
    /* --END ERROR HANDLING-- */

    *error_code = MPI_SUCCESS;
}
