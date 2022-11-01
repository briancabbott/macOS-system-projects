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


#include "opal_config.h"

#include "opal/constants.h"
#include "opal/util/output.h"
#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"
#include "opal/mca/base/mca_base_param.h"
#include "opal/mca/backtrace/backtrace.h"
#include "opal/mca/backtrace/base/base.h"


/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */
#include "opal/mca/backtrace/base/static-components.h"


/*
 * Globals
 */
opal_list_t opal_backtrace_base_components_opened;


int
opal_backtrace_base_open(void)
{
    /* Open up all available components */
    if (OPAL_SUCCESS !=
        mca_base_components_open("backtrace", 0,
                                 mca_backtrace_base_static_components,
                                 &opal_backtrace_base_components_opened, 
                                 true)) {
        return OPAL_ERROR;
    }

    /* All done */
    return OPAL_SUCCESS;
}


int
opal_backtrace_base_close(void)
{
    /* Close all components that are still open */
    mca_base_components_close(0, 
                              &opal_backtrace_base_components_opened, 
                              NULL);
    OBJ_DESTRUCT(&opal_backtrace_base_components_opened);

    /* All done */
    return OPAL_SUCCESS;
}
