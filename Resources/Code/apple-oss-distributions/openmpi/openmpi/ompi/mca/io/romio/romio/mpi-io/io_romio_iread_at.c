/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 *
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#include "mpioimpl.h"

#ifdef HAVE_WEAK_SYMBOLS

#if defined(HAVE_PRAGMA_WEAK)
#pragma weak MPI_File_iread_at = PMPI_File_iread_at
#elif defined(HAVE_PRAGMA_HP_SEC_DEF)
#pragma _HP_SECONDARY_DEF PMPI_File_iread_at MPI_File_iread_at
#elif defined(HAVE_PRAGMA_CRI_DUP)
#pragma _CRI duplicate MPI_File_iread_at as PMPI_File_iread_at
/* end of weak pragmas */
#endif

/* Include mapping from MPI->PMPI */
#define MPIO_BUILD_PROFILING
#include "mpioprof.h"
#endif

/*@
    MPI_File_iread_at - Nonblocking read using explict offset

Input Parameters:
. fh - file handle (handle)
. offset - file offset (nonnegative integer)
. count - number of elements in buffer (nonnegative integer)
. datatype - datatype of each buffer element (handle)

Output Parameters:
. buf - initial address of buffer (choice)
. request - request object (handle)

.N fortran
@*/
#ifdef HAVE_MPI_GREQUEST
#include "mpiu_greq.h"

#if defined(HAVE_WINDOWS_H) && defined(USE_WIN_THREADED_IO)
typedef struct iread_at_args
{
    MPI_File file;
    MPI_Offset offset;
    void *buf;
    int count;
    MPI_Datatype datatype;
    MPIO_Request request;
    MPI_Status *status;
} iread_at_args;

static DWORD WINAPI iread_at_thread(LPVOID lpParameter)
{
    int error_code;
    iread_at_args *args = (iread_at_args *)lpParameter;

    error_code = MPI_File_read_at(args->file, args->offset, args->buf, args->count, args->datatype, args->status);
    /* ROMIO-1 doesn't do anything with status.MPI_ERROR */
    args->status->MPI_ERROR = error_code;

    MPI_Grequest_complete(args->request);
    ADIOI_Free(args);
    return 0;
}
#endif

int MPI_File_iread_at(MPI_File mpi_fh, MPI_Offset offset, void *buf,
                      int count, MPI_Datatype datatype, 
                      MPIO_Request *request)
{
    int error_code=MPI_SUCCESS;
    MPI_Status *status;
#if defined(HAVE_WINDOWS_H) && defined(USE_WIN_THREADED_IO)
    iread_at_args *args;
    HANDLE hThread;
#endif

    MPIU_THREAD_SINGLE_CS_ENTER("io");
    MPIR_Nest_incr();

    status = (MPI_Status *) ADIOI_Malloc(sizeof(MPI_Status));

#if defined(HAVE_WINDOWS_H) && defined(USE_WIN_THREADED_IO)
    /* kick off the request */
    MPI_Grequest_start(MPIU_Greq_query_fn, MPIU_Greq_free_fn, 
	MPIU_Greq_cancel_fn, status, request);

    args = (iread_at_args*) ADIOI_Malloc(sizeof(iread_at_args));
    args->file = mpi_fh;
    args->offset = offset;
    args->buf = buf;
    args->count = count;
    args->datatype = datatype;
    args->status = status;
    args->request = *request;
    hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)iread_at_thread, args, 0, NULL);
    if (hThread == NULL)
    {
	error_code = GetLastError();
	error_code = MPIO_Err_create_code(MPI_SUCCESS, MPIR_ERR_RECOVERABLE,
	    "MPI_File_iread_at", __LINE__, MPI_ERR_OTHER,
	    "**fail", "**fail %d", error_code);
	error_code = MPIO_Err_return_file(args->file, error_code);
	return error_code;
    }
    CloseHandle(hThread);

#else

    /* for now, no threads or anything fancy. 
    * just call the blocking version */
    error_code = MPI_File_read_at(mpi_fh, offset, buf, count, datatype,
	status); 
    /* ROMIO-1 doesn't do anything with status.MPI_ERROR */
    status->MPI_ERROR = error_code;

    /* --BEGIN ERROR HANDLING-- */
    if (error_code != MPI_SUCCESS)
	error_code = MPIO_Err_return_file(mpi_fh, error_code);
    /* --END ERROR HANDLING-- */

    /* kick off the request */
    MPI_Grequest_start(MPIU_Greq_query_fn, MPIU_Greq_free_fn, 
	MPIU_Greq_cancel_fn, status, request);
    /* but we did all the work already */
    MPI_Grequest_complete(*request);
    /* passed the buck to the blocking version...*/
#endif

    MPIR_Nest_decr();
    MPIU_THREAD_SINGLE_CS_EXIT("io");

    return error_code;
}
#else
int MPI_File_iread_at(MPI_File mpi_fh, MPI_Offset offset, void *buf,
                      int count, MPI_Datatype datatype, 
                      MPIO_Request *request)
{
    int error_code;
    static char myname[] = "MPI_FILE_IREAD_AT";

#ifdef MPI_hpux
    int fl_xmpi;

    HPMP_IO_START(fl_xmpi, BLKMPIFILEIREADAT, TRDTSYSTEM, mpi_fh, datatype,
		  count);
#endif /* MPI_hpux */


    error_code = MPIOI_File_iread(mpi_fh, offset, ADIO_EXPLICIT_OFFSET, buf,
				  count, datatype, myname, request);

    /* --BEGIN ERROR HANDLING-- */
    if (error_code != MPI_SUCCESS)
	error_code = MPIO_Err_return_file(mpi_fh, error_code);
    /* --END ERROR HANDLING-- */

#ifdef MPI_hpux
    HPMP_IO_END(fl_xmpi, mpi_fh, datatype, count);
#endif /* MPI_hpux */

    return error_code;
}
#endif
