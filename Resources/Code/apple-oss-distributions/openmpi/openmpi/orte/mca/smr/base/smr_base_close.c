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

#include "orte_config.h"

#include "orte/orte_constants.h"
#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"

#include "orte/mca/smr/base/base.h"
#include "orte/mca/smr/base/smr_private.h"

int orte_smr_base_close(void)
{
  /* If we have a selected component and module, then finalize it */

  if (NULL != orte_smr.finalize) {
    orte_smr.finalize();
  }

  /* after the module, close the component?? */
  /* orte_smr_base_component_finalize (); */

  /* Close all remaining available components (may be one if this is a
     OMPI RTE program, or [possibly] multiple if this is ompi_info) */

  mca_base_components_close(orte_smr_base.smr_output, 
                            &orte_smr_base.smr_components, NULL);

  /* All done */

  return ORTE_SUCCESS;
}

