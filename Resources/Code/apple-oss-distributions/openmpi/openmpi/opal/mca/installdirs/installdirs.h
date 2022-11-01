/*
 * Copyright (c) 2006-2007 Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef OPAL_MCA_INSTALLDIRS_INSTALLDIRS_H
#define OPAL_MCA_INSTALLDIRS_INSTALLDIRS_H

#include "opal_config.h"

#include "opal/mca/mca.h"
#include "opal/mca/base/base.h"

/*
 * Most of this file is just for ompi_info.  The only public interface
 * once opal_init has been called is the opal_install_dirs structure
 * and the opal_install_dirs_expand() call */
struct opal_install_dirs_t {
    char* prefix;
    char* exec_prefix;
    char* bindir;
    char* sbindir;
    char* libexecdir;
    char* datarootdir;
    char* datadir;
    char* sysconfdir;
    char* sharedstatedir;
    char* localstatedir;
    char* libdir;
    char* includedir;
    char* infodir;
    char* mandir;
    char* pkgdatadir;
    char* pkglibdir;
    char* pkgincludedir;
};
typedef struct opal_install_dirs_t opal_install_dirs_t;

/* Install directories.  Only available after opal_init() */
extern opal_install_dirs_t opal_install_dirs;

/**
 * Expand out path variables (such as ${prefix}) in the input string
 * using the current opal_install_dirs structure */
char * opal_install_dirs_expand(const char* input);


/**
 * Structure for installdirs v1.0.0 components.
 * Chained to MCA v1.0.0
 */
struct opal_installdirs_base_component_1_0_0_t {
    /** MCA base component */
    mca_base_component_t component;
    /** MCA base data */
    mca_base_component_data_1_0_0_t component_data;
    /** install directories provided by the given component */
    opal_install_dirs_t install_dirs_data;
};
/**
 * Convenience typedef
 */
typedef struct opal_installdirs_base_component_1_0_0_t opal_installdirs_base_component_t;

/*
 * Macro for use in components that are of type installdirs v1.0.0
 */
#define OPAL_INSTALLDIRS_BASE_VERSION_1_0_0 \
    /* installdirs v1.0 is chained to MCA v1.0 */ \
    MCA_BASE_VERSION_1_0_0, \
    /* installdirs v1.0 */ \
    "installdirs", 1, 0, 0

#endif /* OPAL_MCA_INSTALLDIRS_INSTALLDIRS_H */
