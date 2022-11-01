/* -*- Mode: C; c-basic-offset:4 ; -*- */
/* 
 *
 *   Copyright (C) 1997 University of Chicago. 
 *   See COPYRIGHT notice in top-level directory.
 */

#include "adio.h"
#include "adio_extern.h"
#include "adio_cb_config_list.h"

#include "mpio.h"

static int is_aggregator(int rank, ADIO_File fd);
static int uses_generic_read(ADIO_File fd);
static int uses_generic_write(ADIO_File fd);

MPI_File ADIO_Open(MPI_Comm orig_comm,
		   MPI_Comm comm, char *filename, int file_system,
		   ADIOI_Fns *ops,
		   int access_mode, ADIO_Offset disp, MPI_Datatype etype, 
		   MPI_Datatype filetype,
		   MPI_Info info, int perm, int *error_code)
{
    MPI_File mpi_fh;
    ADIO_File fd;
    ADIO_cb_name_array array;
    int orig_amode_excl, orig_amode_wronly, err, rank, procs;
    char *value;
    static char myname[] = "ADIO_OPEN";
    int rank_ct, max_error_code;
    int *tmp_ranklist;
    MPI_Comm aggregator_comm = MPI_COMM_NULL; /* just for deferred opens */

    *error_code = MPI_SUCCESS;

    /* obtain MPI_File handle */
    mpi_fh = MPIO_File_create(sizeof(struct ADIOI_FileD));
    if (mpi_fh == MPI_FILE_NULL) {
    }
    fd = MPIO_File_resolve(mpi_fh);

    fd->cookie = ADIOI_FILE_COOKIE;
    fd->fp_ind = disp;
    fd->fp_sys_posn = 0;
    fd->comm = comm;       /* dup'ed in MPI_File_open */
    fd->filename = ADIOI_Strdup(filename);
    fd->file_system = file_system;
    fd->fs_ptr = NULL;

    fd->fns = ops;

    fd->disp = disp;
    fd->split_coll_count = 0;
    fd->shared_fp_fd = ADIO_FILE_NULL;
    fd->atomicity = 0;
    fd->etype = etype;          /* MPI_BYTE by default */
    fd->filetype = filetype;    /* MPI_BYTE by default */
    fd->etype_size = 1;  /* default etype is MPI_BYTE */

    fd->perm = perm;

    fd->async_count = 0;

    fd->fortran_handle = -1;

    fd->err_handler = ADIOI_DFLT_ERR_HANDLER;

/* create and initialize info object */
    fd->hints = (ADIOI_Hints *)ADIOI_Malloc(sizeof(struct ADIOI_Hints_struct));
    if (fd->hints == NULL) {
	/* NEED TO HANDLE ENOMEM ERRORS */
    }
    fd->hints->cb_config_list = NULL;
    fd->hints->ranklist = NULL;
    fd->hints->initialized = 0;
    fd->info = MPI_INFO_NULL;
    ADIO_SetInfo(fd, info, &err);

     /* deferred open: 
     * we can only do this optimization if 'fd->hints->deferred_open' is set
     * (which means the user hinted 'no_indep_rw' and collective buffering).
     * Furthermore, we only do this if our collective read/write routines use
     * our generic function, and not an fs-specific routine (we can defer opens
     * only if we use our aggreagation code). */
    if (fd->hints->deferred_open && 
		    !(uses_generic_read(fd) \
			    && uses_generic_write(fd))) {
	    fd->hints->deferred_open = 0;
    }
    if (fd->file_system == ADIO_PVFS2)
	    /* disable deferred open on PVFS2 so that scalable broadcast will
	     * always use the propper communicator */
	    fd->hints->deferred_open = 0;

/* gather the processor name array if we don't already have it */

/* this has to be done here so that we can cache the name array in both
 * the dup'd communicator (in case we want it later) and the original
 * communicator
 */
    ADIOI_cb_gather_name_array(orig_comm, comm, &array);

/* parse the cb_config_list and create a rank map on rank 0 */
    MPI_Comm_rank(comm, &rank);
    if (rank == 0) {
	MPI_Comm_size(comm, &procs);
	tmp_ranklist = (int *) ADIOI_Malloc(sizeof(int) * procs);
	if (tmp_ranklist == NULL) {
	    /* NEED TO HANDLE ENOMEM ERRORS */
	}

	rank_ct = ADIOI_cb_config_list_parse(fd->hints->cb_config_list, 
					     array, tmp_ranklist,
					     fd->hints->cb_nodes);

	/* store the ranklist using the minimum amount of memory */
	if (rank_ct > 0) {
	    fd->hints->ranklist = (int *) ADIOI_Malloc(sizeof(int) * rank_ct);
	    memcpy(fd->hints->ranklist, tmp_ranklist, sizeof(int) * rank_ct);
	}
	ADIOI_Free(tmp_ranklist);
	fd->hints->cb_nodes = rank_ct;
	/* TEMPORARY -- REMOVE WHEN NO LONGER UPDATING INFO FOR FS-INDEP. */
	value = (char *) ADIOI_Malloc((MPI_MAX_INFO_VAL+1)*sizeof(char));
	ADIOI_Snprintf(value, MPI_MAX_INFO_VAL+1, "%d", rank_ct);
	MPI_Info_set(fd->info, "cb_nodes", value);
	ADIOI_Free(value);
    }

    ADIOI_cb_bcast_rank_map(fd);
    if (fd->hints->cb_nodes <= 0) {
	*error_code = MPIO_Err_create_code(MPI_SUCCESS, MPIR_ERR_RECOVERABLE,
					   myname, __LINE__, MPI_ERR_IO,
					   "**ioagnomatch", 0);
	fd = ADIO_FILE_NULL;
        goto fn_exit;
    }


     /* deferred open: if we are an aggregator, create a new communicator.
      * we'll use this aggregator communicator for opens and closes.
      * otherwise, we have a NULL communicator until we try to do independent
      * IO */
    fd->agg_comm = MPI_COMM_NULL;
    fd->is_open = 0;
    if (fd->hints->deferred_open) {
	    /* MPI_Comm_split will create a communication group of aggregators.
	     * for non-aggregators it will return MPI_COMM_NULL .  we rely on
	     * fd->agg_comm == MPI_COMM_NULL for non-aggregators in several
	     * tests in the code  */
	    if (is_aggregator(rank, fd)) {
		    MPI_Comm_split(fd->comm, 1, 0, &aggregator_comm);
		    fd->agg_comm = aggregator_comm;
	    } else {
		    MPI_Comm_split(fd->comm, MPI_UNDEFINED, 0, &aggregator_comm);
		    fd->agg_comm = aggregator_comm;
	    }
    }

    orig_amode_excl = access_mode;

    /* optimization: by having just one process create a file, close it, then
     * have all N processes open it, we can possibly avoid contention for write
     * locks on a directory for some file systems. 
     *
     * we used to special-case EXCL|CREATE, since when N processes are trying
     * to create a file exclusively, only 1 will succeed and the rest will
     * (spuriously) fail.   Since we are now carrying out the CREATE on one
     * process anyway, the EXCL case falls out and we don't need to explicitly
     * worry about it, other than turning off both the EXCL and CREATE flags 
     */
    /* pvfs2 handles opens specially, so it is actually more efficent for that
     * file system if we skip this optimization */
    if (access_mode & ADIO_CREATE && fd->file_system != ADIO_PVFS2) {
       if(rank == fd->hints->ranklist[0]) {
	   /* remove delete_on_close flag if set */
	   if (access_mode & ADIO_DELETE_ON_CLOSE)
	       fd->access_mode = access_mode ^ ADIO_DELETE_ON_CLOSE;
	   else 
	       fd->access_mode = access_mode;
	       
	   (*(fd->fns->ADIOI_xxx_Open))(fd, error_code);
	   MPI_Bcast(error_code, 1, MPI_INT, \
		     fd->hints->ranklist[0], fd->comm);
	   /* if no error, close the file and reopen normally below */
	   if (*error_code == MPI_SUCCESS) 
	       (*(fd->fns->ADIOI_xxx_Close))(fd, error_code);

	   fd->access_mode = access_mode; /* back to original */
       }
       else MPI_Bcast(error_code, 1, MPI_INT, fd->hints->ranklist[0], fd->comm);

       if (*error_code != MPI_SUCCESS) {
           goto fn_exit;
       } 
       else {
           /* turn off CREAT (and EXCL if set) for real multi-processor open */
           access_mode ^= ADIO_CREATE; 
	   if (access_mode & ADIO_EXCL)
		   access_mode ^= ADIO_EXCL;
       }
    }

    /* if we are doing deferred open, non-aggregators should return now */
    if (fd->hints->deferred_open ) {
        if (fd->agg_comm == MPI_COMM_NULL) {
            /* we might have turned off EXCL for the aggregators.
             * restore access_mode that non-aggregators get the right
             * value from get_amode */
            fd->access_mode = orig_amode_excl;
            *error_code = MPI_SUCCESS;
            goto fn_exit;
        }
    }

/* For writing with data sieving, a read-modify-write is needed. If 
   the file is opened for write_only, the read will fail. Therefore,
   if write_only, open the file as read_write, but record it as write_only
   in fd, so that get_amode returns the right answer. */

    orig_amode_wronly = access_mode;
    if (access_mode & ADIO_WRONLY) {
	access_mode = access_mode ^ ADIO_WRONLY;
	access_mode = access_mode | ADIO_RDWR;
    }
    fd->access_mode = access_mode;

    (*(fd->fns->ADIOI_xxx_Open))(fd, error_code);

    /* if error, may be it was due to the change in amode above. 
       therefore, reopen with access mode provided by the user.*/ 
    fd->access_mode = orig_amode_wronly;  
    if (*error_code != MPI_SUCCESS) 
        (*(fd->fns->ADIOI_xxx_Open))(fd, error_code);

    /* if we turned off EXCL earlier, then we should turn it back on */
    if (fd->access_mode != orig_amode_excl) fd->access_mode = orig_amode_excl;

    /* for deferred open: this process has opened the file (because if we are
     * not an aggregaor and we are doing deferred open, we returned earlier)*/
    fd->is_open = 1;

 fn_exit:
    MPI_Allreduce(error_code, &max_error_code, 1, MPI_INT, MPI_MAX, comm);
    if (max_error_code != MPI_SUCCESS) {

        /* If the file was successfully opened, close it */
        if (*error_code == MPI_SUCCESS) {
        
            /* in the deferred open case, only those who have actually
               opened the file should close it */
            if (fd->hints->deferred_open)  {
                if (fd->agg_comm != MPI_COMM_NULL) {
                    (*(fd->fns->ADIOI_xxx_Close))(fd, error_code);
                }
            }
            else {
                (*(fd->fns->ADIOI_xxx_Close))(fd, error_code);
            }
        }
	if (fd->filename) ADIOI_Free(fd->filename);
	if (fd->hints->ranklist) ADIOI_Free(fd->hints->ranklist);
	if (fd->hints->cb_config_list) ADIOI_Free(fd->hints->cb_config_list);
	if (fd->hints) ADIOI_Free(fd->hints);
	if (fd->info != MPI_INFO_NULL) MPI_Info_free(&(fd->info));
	ADIOI_Free(fd);
        fd = ADIO_FILE_NULL;
	if (*error_code == MPI_SUCCESS)
	{
	    *error_code = MPIO_Err_create_code(MPI_SUCCESS,
					       MPIR_ERR_RECOVERABLE, myname,
					       __LINE__, MPI_ERR_IO,
					       "**oremote_fail", 0);
	}
    }

    return fd;
}

/* a simple linear search. possible enancement: add a my_cb_nodes_index member
 * ( index into cb_nodes, else -1 if not aggregator ) for faster lookups 
 *
 * fd->hints->cb_nodes is the number of aggregators
 * fd->hints->ranklist[] is an array of the ranks of aggregators
 *
 * might want to move this to adio/common/cb_config_list.c 
 */
int is_aggregator(int rank, ADIO_File fd ) {
        int i;
        
        for (i=0; i< fd->hints->cb_nodes; i++ ) {
                if ( rank == fd->hints->ranklist[i] )
                        return 1;
        }
        return 0;
}

/* 
 * we special-case TESTFS because all it does is wrap logging info around GEN 
 */
static int uses_generic_read(ADIO_File fd)
{
    ADIOI_Fns *fns = fd->fns;
    if (fns->ADIOI_xxx_ReadStridedColl == ADIOI_GEN_ReadStridedColl || 
        fd->file_system == ADIO_TESTFS )
    {
        return 1;
    }
    return 0;
}

static int uses_generic_write(ADIO_File fd)
{
    ADIOI_Fns *fns = fd->fns;
    if (fns->ADIOI_xxx_WriteStridedColl == ADIOI_GEN_WriteStridedColl ||
        fd->file_system == ADIO_TESTFS )
    {
        return 1;
    }
    return 0;
}
