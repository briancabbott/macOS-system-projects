/*
 * Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
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
/** @file:
 */

#ifndef MCA_ODLS_BASE_H
#define MCA_ODLS_BASE_H

/*
 * includes
 */
#include "orte_config.h"

#include "opal/mca/mca.h"
#include "opal/class/opal_list.h"

#include "orte/mca/odls/odls.h"


#if defined(c_plusplus) || defined(__cplusplus)
extern "C" {
#endif

    /**
     * Struct to hold globals for the odls framework
     */
    typedef struct orte_odls_base_t {
        /* components are available */
        bool components_available;
        /* component has been selected */
        bool selected;
        /** List of opened components */
        opal_list_t available_components;
        /** selected component */
        orte_odls_base_component_t selected_component;
    } orte_odls_base_t;
    
    /**
     * Global instance of odls-wide framework data
     */
    ORTE_DECLSPEC extern orte_odls_base_t orte_odls_base;

    /*
     * Global functions for MCA overall collective open and close
     */

    /**
     * Open the odls framework
     */
    ORTE_DECLSPEC int orte_odls_base_open(void);
    /**
     * Select an odls module
     */
    ORTE_DECLSPEC int orte_odls_base_select(void);

    /**
     * Close the odls framework
     */
    ORTE_DECLSPEC int orte_odls_base_finalize(void);
    ORTE_DECLSPEC int orte_odls_base_close(void);

#if defined(c_plusplus) || defined(__cplusplus)
}
#endif
#endif
