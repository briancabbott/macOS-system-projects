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
 *
 * These symbols are in a file by themselves to provide nice linker
 * semantics.  Since linkers generally pull in symbols by object
 * files, keeping these symbols as the only symbols in this file
 * prevents utility programs such as "ompi_info" from having to import
 * entire components just to query their version and parameters.
 */

#include "ompi_config.h"
#include "coll_basic.h"

#include "mpi.h"
#include "ompi/mca/coll/coll.h"
#include "coll_basic.h"

/*
 * Public string showing the coll ompi_basic component version number
 */
const char *mca_coll_basic_component_version_string =
    "Open MPI basic collective MCA component version " OMPI_VERSION;

/*
 * Global variables
 */
int mca_coll_basic_priority = 10;
int mca_coll_basic_crossover = 4;

/*
 * Local function
 */
static int basic_open(void);

/*
 * Instantiate the public struct with all of our public information
 * and pointers to our public functions in it
 */

const mca_coll_base_component_1_0_0_t mca_coll_basic_component = {

    /* First, the mca_component_t struct containing meta information
     * about the component itself */

    {
     /* Indicate that we are a coll v1.0.0 component (which also implies a
      * specific MCA version) */

     MCA_COLL_BASE_VERSION_1_0_0,

     /* Component name and version */

     "basic",
     OMPI_MAJOR_VERSION,
     OMPI_MINOR_VERSION,
     OMPI_RELEASE_VERSION,

     /* Component open and close functions */

     basic_open,
     NULL
    },

    /* Next the MCA v1.0.0 component meta data */

    {
     /* Whether the component is checkpointable or not */

     true
    },

    /* Initialization / querying functions */

    mca_coll_basic_init_query,
    mca_coll_basic_comm_query,
    NULL
};


static int
basic_open(void)
{
    /* Use a low priority, but allow other components to be lower */

    mca_base_param_reg_int(&mca_coll_basic_component.collm_version,
                           "priority",
                           "Priority of the basic coll component",
                           false, false, mca_coll_basic_priority,
                           &mca_coll_basic_priority);
    mca_base_param_reg_int(&mca_coll_basic_component.collm_version,
                           "crossover",
                           "Minimum number of processes in a communicator before using the logarithmic algorithms",
                           false, false, mca_coll_basic_crossover,
                           &mca_coll_basic_crossover);

    return OMPI_SUCCESS;
}
