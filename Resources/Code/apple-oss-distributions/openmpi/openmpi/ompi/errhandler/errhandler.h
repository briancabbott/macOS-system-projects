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
/** @file **/

#ifndef OMPI_ERRHANDLER_H
#define OMPI_ERRHANDLER_H

#include "ompi_config.h"

#include "mpi.h"
#include "opal/class/opal_object.h"
#include "ompi/class/ompi_pointer_array.h"
#include "ompi/runtime/mpiruntime.h"
#include "ompi/errhandler/errhandler_predefined.h"
#include "ompi/errhandler/errcode-internal.h"

#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif
/*
 * These must correspond to the fortran handle indices
 */
enum {
  OMPI_ERRHANDLER_NULL_FORTRAN = 0,
  OMPI_ERRORS_ARE_FATAL_FORTRAN,
  OMPI_ERRORS_RETURN_FORTRAN
};


/**
 * Typedef for all fortran errhandler functions
 */
typedef void (ompi_errhandler_fortran_handler_fn_t)(MPI_Fint *, 
                                                    MPI_Fint *, ...);

/**
 * Typedef for generic errhandler function
 */
typedef void (ompi_errhandler_generic_handler_fn_t)(void *, int *, ...);


/**
 * Enum used to describe what kind MPI object an error handler is used for
 */
enum ompi_errhandler_type_t {
    OMPI_ERRHANDLER_TYPE_PREDEFINED,
    OMPI_ERRHANDLER_TYPE_COMM,
    OMPI_ERRHANDLER_TYPE_WIN,
    OMPI_ERRHANDLER_TYPE_FILE
};
typedef enum ompi_errhandler_type_t ompi_errhandler_type_t;


/**
 * Back-end type for MPI_Errorhandler.
 */
struct ompi_errhandler_t {
    opal_object_t super;

    char eh_name[MPI_MAX_OBJECT_NAME];
    /* Type of MPI object that this handler is for */
    
    ompi_errhandler_type_t eh_mpi_object_type;

    /* Flags about the error handler */
    bool eh_fortran_function;

    /* Function pointers */
    MPI_Comm_errhandler_fn *eh_comm_fn;
    MPI_File_errhandler_fn *eh_file_fn;
    MPI_Win_errhandler_fn *eh_win_fn;
    ompi_errhandler_fortran_handler_fn_t *eh_fort_fn;

    /* index in Fortran <-> C translation array */
    int eh_f_to_c_index;
};
typedef struct ompi_errhandler_t ompi_errhandler_t;


/**
 * Global variable for MPI_ERRHANDLER_NULL
 */
OMPI_DECLSPEC extern ompi_errhandler_t ompi_mpi_errhandler_null;

/**
 * Global variable for MPI_ERRORS_ARE_FATAL
 */
OMPI_DECLSPEC extern ompi_errhandler_t ompi_mpi_errors_are_fatal;

/**
 * Global variable for MPI_ERRORS_RETURN
 */
OMPI_DECLSPEC extern ompi_errhandler_t ompi_mpi_errors_return;

/**
 * Global variable for MPI::ERRORS_THROW_EXCEPTIONS.  Will abort if
 * MPI_INIT wasn't called as MPI::INIT
 */
OMPI_DECLSPEC extern ompi_errhandler_t ompi_mpi_errors_throw_exceptions;

/**
 * Table for Fortran <-> C errhandler handle conversion
 */
OMPI_DECLSPEC extern ompi_pointer_array_t *ompi_errhandler_f_to_c_table;


/**
 * Forward declaration so that we don't have to include
 * request/request.h here.
 */
struct ompi_request_t;


/**
 * This is the macro to check the state of MPI and determine whether
 * it was properly initialized and not yet finalized.
 *
 * This macro directly invokes the ompi_mpi_errors_are_fatal_handler()
 * when an error occurs because MPI_COMM_WORLD does not exist (because
 * we're before MPI_Init() or after MPI_Finalize()).
 */
#define OMPI_ERR_INIT_FINALIZE(name) \
  if (!ompi_mpi_initialized || ompi_mpi_finalized) { \
    ompi_mpi_errors_are_fatal_comm_handler(NULL, NULL, name); \
  }

/**
 * This is the macro to invoke to directly invoke an MPI error
 * handler.
 *
 * @param mpi_object The MPI object to invoke the errhandler on (a
 *    comm, win, or win)
 * @param err_code The error code
 * @param message Any additional message; typically the name of the
 *    MPI function that is invoking the error.
 *
 * This macro is used when you want to directly invoke the error
 * handler.  It is exactly equivalent to calling
 * ompi_errhandler_invoke() directly, but is provided to have a
 * parallel invocation to OMPI_ERRHANDLER_CHECK() and OMPI_ERRHANDLER_RETURN().
 */
#define OMPI_ERRHANDLER_INVOKE(mpi_object, err_code, message) \
  ompi_errhandler_invoke((mpi_object)->error_handler, \
			 (mpi_object), \
                         (int)(mpi_object)->errhandler_type, \
                         (err_code < 0 ? (ompi_errcode_get_mpi_code(err_code)) : err_code), \
			 (message));

/**
 * Conditionally invoke an MPI error handler.
 *
 * @param rc The return code to check
 * @param mpi_object The MPI object to invoke the errhandler on (a
 *    comm, win, or win)
 * @param err_code The error code
 * @param message Any additional message; typically the name of the
 *    MPI function that is invoking the error.
 *
 * This macro will invoke the error handler if the return code is not
 * OMPI_SUCCESS.
 */
#define OMPI_ERRHANDLER_CHECK(rc, mpi_object, err_code, message) \
  if (rc != OMPI_SUCCESS) { \
    int __mpi_err_code = (err_code < 0 ? (ompi_errcode_get_mpi_code(err_code)) : err_code); \
    ompi_errhandler_invoke((mpi_object)->error_handler, \
			   (mpi_object), \
                           (int) (mpi_object)->errhandler_type, \
                           (__mpi_err_code), \
                           (message)); \
    return (__mpi_err_code); \
  }

/**
 * Conditionally invoke an MPI error handler; if there is no error,
 * return MPI_SUCCESS.
 *
 * @param rc The return code to check
 * @param mpi_object The MPI object to invoke the errhandler on (a
 *    comm, win, or win)
 * @param err_code The error code
 * @param message Any additional message; typically the name of the
 *    MPI function that is invoking the error.
 *
 * This macro will invoke the error handler if the return code is not
 * OMPI_SUCCESS.  If the return code is OMPI_SUCCESS, then return
 * MPI_SUCCESS.
 */
#define OMPI_ERRHANDLER_RETURN(rc, mpi_object, err_code, message) \
  if (rc != OMPI_SUCCESS) { \
    int __mpi_err_code = (err_code < 0 ? (ompi_errcode_get_mpi_code(err_code)) : err_code); \
    ompi_errhandler_invoke((mpi_object)->error_handler, \
                           (mpi_object), \
                           (int)(mpi_object)->errhandler_type, \
                           (__mpi_err_code), \
                           (message)); \
    return (__mpi_err_code); \
  } else { \
    return MPI_SUCCESS; \
  }



  /**
   * Initialize the error handler interface.
   *
   * @returns OMPI_SUCCESS Upon success
   * @returns OMPI_ERROR Otherwise
   *
   * Invoked from ompi_mpi_init(); sets up the error handler interface,
   * creates the predefined MPI errorhandlers, and creates the
   * corresopnding F2C translation table.
   */
  int ompi_errhandler_init(void);

  /**
   * Finalize the error handler interface.
   *
   * @returns OMPI_SUCCESS Always
   *
   * Invokes from ompi_mpi_finalize(); tears down the error handler
   * interface, and destroys the F2C translation table.
   */
  int ompi_errhandler_finalize(void);

  /**
   * \internal
   *
   * This function should not be invoked directly; it should only be
   * invoked by OMPI_ERRHANDLER_INVOKE(), OMPI_ERRHANDLER_CHECK(), or
   * OMPI_ERRHANDLER_RETURN().
   *
   * @param errhandler The MPI_Errhandler to invoke
   * @param mpi_object The MPI object to invoke the errhandler on (a
   *    comm, win, or win)
   * @param type       The type of the MPI object. Necessary, since
   *                   you can not assign a single type to the predefined
   *                   error handlers. This information is therefore 
   *                   stored on the MPI object itself.
   * @param err_code The error code
   * @param message Any additional message; typically the name of the
   *    MPI function that is invoking the error.
   *
   * @returns err_code The same value as the parameter
   *
   * This function invokes the MPI exception function on the error
   * handler.  If the errhandler was created from fortran, the error
   * handler will be invoked with fortran linkage.  Otherwise, it is
   * invoked with C linkage.
   *
   * If this function returns, it returns the err_code.  Note that it
   * may not return (e.g., for MPI_ERRORS_ARE_FATAL).
   */
  int ompi_errhandler_invoke(ompi_errhandler_t *errhandler, void *mpi_object, 
			     int type, int err_code, const char *message);


  /**
   * Invoke an MPI exception on the first request found in the array
   * that has a non-MPI_SUCCESS value for MPI_ERROR in its status.  It
   * is safe to invoke this function if none of the requests have an
   * outstanding error; MPI_SUCCESS will be returned.
   */
  int ompi_errhandler_request_invoke(int count, 
                                     struct ompi_request_t **requests,
                                     const char *message);

  /**
   * Create a ompi_errhandler_t
   *
   * @param object_type Enum of the type of MPI object
   * @param func Function pointer of the error handler
   *
   * @returns errhandler Pointer to the ompi_errorhandler_t that will be
   *   created and returned
   *
   * This function is called as the back-end of all the
   * MPI_*_CREATE_ERRHANDLER functions.  It creates a new
   * ompi_errhandler_t object, initializes it to the correct object
   * type, and sets the callback function on it.  
   *
   * The type of the function pointer is (arbitrarily) the fortran
   * function handler type.  Since this function has to accept 4
   * different function pointer types (lest we have 4 different
   * functions to create errhandlers), the fortran one was picked
   * arbitrarily.  Note that (void*) is not sufficient because at
   * least theoretically, a sizeof(void*) may not necessarily be the
   * same as sizeof(void(*)).
   *
   * NOTE: It *always* sets the "fortran" flag to false.  Fortran
   * wrappers for MPI_*_CREATE_ERRHANDLER are expected to reset this
   * flag to true manually.
   */
  ompi_errhandler_t *ompi_errhandler_create(ompi_errhandler_type_t object_type,
					    ompi_errhandler_generic_handler_fn_t *func);


/**
 * Check to see if an errhandler is intrinsic.
 *
 * @param errhandler The errhandler to check
 *
 * @returns true If the errhandler is intrinsic
 * @returns false If the errhandler is not intrinsic
 *
 * Self-explanitory.  This is needed in a few top-level MPI functions;
 * this function is provided to hide the internal structure field
 * names.
 */
static inline bool ompi_errhandler_is_intrinsic(ompi_errhandler_t *errhandler)
{
    if ( OMPI_ERRHANDLER_TYPE_PREDEFINED == errhandler->eh_mpi_object_type ) 
	return true;

    return false;
}
#if defined(c_plusplus) || defined(__cplusplus)
}
#endif

#endif /* OMPI_ERRHANDLER_H */
