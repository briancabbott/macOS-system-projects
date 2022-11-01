/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 *
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#include "mpioimpl.h"

#ifdef HAVE_WEAK_SYMBOLS

#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPI_File_read_ordered_end = PMPI_File_read_ordered_end
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPI_File_read_ordered_end MPI_File_read_ordered_end
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_File_read_ordered_end as PMPI_File_read_ordered_end
/* end of weak pragmas */
#endif

/* Include mapping from MPI->PMPI */
#define MPIO_BUILD_PROFILING
#include "mpioprof.h"
#endif

/*@
    MPI_File_read_ordered_end - Complete a split collective read using shared file pointer

Input Parameters:
. fh - file handle (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. status - status object (Status)

.N fortran
@*/
int MPI_File_read_ordered_end(MPI_File mpi_fh, void *buf, MPI_Status *status)
{
    int error_code=MPI_SUCCESS;
    ADIO_File fh;
    static char myname[] = "MPI_FILE_READ_ORDERED_END";

    MPIU_UNREFERENCED_ARG(buf);

    MPIU_THREAD_SINGLE_CS_ENTER("io");

    fh = MPIO_File_resolve(mpi_fh);

    /* --BEGIN ERROR HANDLING-- */
    MPIO_CHECK_FILE_HANDLE(fh, myname, error_code);

    if (!(fh->split_coll_count))
    {
	error_code = MPIO_Err_create_code(MPI_SUCCESS, MPIR_ERR_RECOVERABLE,
					  myname, __LINE__, MPI_ERR_IO, 
					  "**iosplitcollnone", 0);
	error_code = MPIO_Err_return_file(fh, error_code);
	goto fn_exit;
    }
    /* --END ERROR HANDLING-- */

#ifdef HAVE_STATUS_SET_BYTES
    if (status != MPI_STATUS_IGNORE)
       *status = fh->split_status;
#endif
    fh->split_coll_count = 0;

fn_exit:
    MPIU_THREAD_SINGLE_CS_EXIT("io");

    return error_code;
}
