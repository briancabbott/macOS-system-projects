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

#include <stdio.h>

#include "ompi/constants.h"
#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "ompi/mca/io/io.h"
#include "ompi/mca/io/base/base.h"
#include "ompi/mca/io/base/io_base_request.h"


int mca_io_base_close(void)
{
    /* stop the progress engine */
    mca_io_base_request_progress_fini();

    /* Destroy the freelist */

    if (mca_io_base_requests_valid) {
        OBJ_DESTRUCT(&mca_io_base_requests);
        mca_io_base_requests_valid = false;
    }

    /* Close all components that are still open.  This may be the opened
     list (if we're in ompi_info), or it may be the available list (if
     we're anywhere else). */

    if (mca_io_base_components_opened_valid) {
        mca_base_components_close(mca_io_base_output,
                                  &mca_io_base_components_opened, NULL);
        OBJ_DESTRUCT(&mca_io_base_components_opened);
        mca_io_base_components_opened_valid = false;
    } else if (mca_io_base_components_available_valid) {
        mca_base_components_close(mca_io_base_output,
                                  &mca_io_base_components_available, NULL);
        OBJ_DESTRUCT(&mca_io_base_components_available);
        mca_io_base_components_available_valid = false;
    }

    /* Destroy some io framework resrouces */

    mca_io_base_component_finalize();

    /* All done */

    return OMPI_SUCCESS;
}
