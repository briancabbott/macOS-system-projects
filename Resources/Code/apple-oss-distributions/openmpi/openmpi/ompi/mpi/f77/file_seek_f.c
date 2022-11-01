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

#if OMPI_HAVE_WEAK_SYMBOLS && OMPI_PROFILE_LAYER
#pragma weak PMPI_FILE_SEEK = mpi_file_seek_f
#pragma weak pmpi_file_seek = mpi_file_seek_f
#pragma weak pmpi_file_seek_ = mpi_file_seek_f
#pragma weak pmpi_file_seek__ = mpi_file_seek_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_FILE_SEEK,
                           pmpi_file_seek,
                           pmpi_file_seek_,
                           pmpi_file_seek__,
                           pmpi_file_seek_f,
                           (MPI_Fint *fh, MPI_Offset *offset, MPI_Fint *whence, MPI_Fint *ierr),
                           (fh, offset, whence, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_FILE_SEEK = mpi_file_seek_f
#pragma weak mpi_file_seek = mpi_file_seek_f
#pragma weak mpi_file_seek_ = mpi_file_seek_f
#pragma weak mpi_file_seek__ = mpi_file_seek_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_FILE_SEEK,
                           mpi_file_seek,
                           mpi_file_seek_,
                           mpi_file_seek__,
                           mpi_file_seek_f,
                           (MPI_Fint *fh, MPI_Offset *offset, MPI_Fint *whence, MPI_Fint *ierr),
                           (fh, offset, whence, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "ompi/mpi/f77/profile/defines.h"
#endif

void mpi_file_seek_f(MPI_Fint *fh, MPI_Offset *offset, 
		     MPI_Fint *whence, MPI_Fint *ierr)
{
    MPI_File c_fh = MPI_File_f2c(*fh);
    
    *ierr = OMPI_INT_2_FINT(MPI_File_seek(c_fh, (MPI_Offset) *offset,
					  OMPI_FINT_2_INT(*whence)));
}
