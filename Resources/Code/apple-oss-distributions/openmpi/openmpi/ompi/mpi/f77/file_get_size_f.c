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
#pragma weak PMPI_FILE_GET_SIZE = mpi_file_get_size_f
#pragma weak pmpi_file_get_size = mpi_file_get_size_f
#pragma weak pmpi_file_get_size_ = mpi_file_get_size_f
#pragma weak pmpi_file_get_size__ = mpi_file_get_size_f
#elif OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (PMPI_FILE_GET_SIZE,
                           pmpi_file_get_size,
                           pmpi_file_get_size_,
                           pmpi_file_get_size__,
                           pmpi_file_get_size_f,
                           (MPI_Fint *fh, MPI_Offset *size, MPI_Fint *ierr),
                           (fh, size, ierr) )
#endif

#if OMPI_HAVE_WEAK_SYMBOLS
#pragma weak MPI_FILE_GET_SIZE = mpi_file_get_size_f
#pragma weak mpi_file_get_size = mpi_file_get_size_f
#pragma weak mpi_file_get_size_ = mpi_file_get_size_f
#pragma weak mpi_file_get_size__ = mpi_file_get_size_f
#endif

#if ! OMPI_HAVE_WEAK_SYMBOLS && ! OMPI_PROFILE_LAYER
OMPI_GENERATE_F77_BINDINGS (MPI_FILE_GET_SIZE,
                           mpi_file_get_size,
                           mpi_file_get_size_,
                           mpi_file_get_size__,
                           mpi_file_get_size_f,
                           (MPI_Fint *fh, MPI_Offset *size, MPI_Fint *ierr),
                           (fh, size, ierr) )
#endif


#if OMPI_PROFILE_LAYER && ! OMPI_HAVE_WEAK_SYMBOLS
#include "ompi/mpi/f77/profile/defines.h"
#endif

void mpi_file_get_size_f(MPI_Fint *fh, MPI_Offset *size, MPI_Fint *ierr)
{
    MPI_File c_fh = MPI_File_f2c(*fh);
    MPI_Offset c_size;

    *ierr = OMPI_INT_2_FINT(MPI_File_get_size(c_fh, 
					      &c_size));
    if (MPI_SUCCESS == OMPI_FINT_2_INT(*ierr)) {
        *size = (MPI_Fint) c_size;
    }
}
