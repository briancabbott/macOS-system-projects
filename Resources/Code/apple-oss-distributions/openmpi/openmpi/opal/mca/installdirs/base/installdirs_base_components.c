/*
 * Copyright (c) 2006-2007 Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * Copyright (c) 2007      Cisco Systems, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 */

#include "opal_config.h"

#include "opal/mca/mca.h"
#include "opal/mca/installdirs/installdirs.h"
#include "opal/mca/installdirs/base/base.h"
#include "opal/mca/installdirs/base/static-components.h"

int opal_installdirs_base_output;
opal_install_dirs_t opal_install_dirs;
opal_list_t opal_installdirs_components;

#define CONDITIONAL_COPY(target, origin, field)                 \
    do {                                                        \
        if (origin.field != NULL && target.field == NULL) {     \
            target.field = origin.field;                        \
        }                                                       \
    } while (0)

int
opal_installdirs_base_open(void)
{
    int i, ret;
    mca_base_component_list_item_t *cli;

    OBJ_CONSTRUCT(&opal_installdirs_components, opal_list_t);
    for (i = 0 ; mca_installdirs_base_static_components[i] != NULL ; ++i) {
        opal_installdirs_base_component_t *component = 
            (opal_installdirs_base_component_t*) 
            mca_installdirs_base_static_components[i];

        /* Save it in a global list for ompi_info */
        cli = OBJ_NEW(mca_base_component_list_item_t);
        cli->cli_component = mca_installdirs_base_static_components[i];
        opal_list_append(&opal_installdirs_components, 
                         &cli->super);

        if (NULL != component->component.mca_open_component) {
            ret = component->component.mca_open_component();
            if (OPAL_SUCCESS != ret) continue;
        }

        /* copy over the data, if something isn't already there */
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data, 
                         prefix);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data,
                         exec_prefix);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data,
                         bindir);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data,
                         sbindir);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data,
                         libexecdir);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data,
                         datarootdir);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data,
                         datadir);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data, 
                         sysconfdir);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data, 
                         sharedstatedir);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data, 
                         localstatedir);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data, 
                         libdir);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data, 
                         includedir);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data, 
                         infodir);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data, 
                         mandir);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data,
                         pkgdatadir);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data, 
                         pkglibdir);
        CONDITIONAL_COPY(opal_install_dirs, component->install_dirs_data, 
                         pkgincludedir);
    }

    /* expand out all the fields */
    opal_install_dirs.prefix = 
        opal_install_dirs_expand(opal_install_dirs.prefix);
    opal_install_dirs.exec_prefix = 
        opal_install_dirs_expand(opal_install_dirs.exec_prefix);
    opal_install_dirs.bindir = 
        opal_install_dirs_expand(opal_install_dirs.bindir);
    opal_install_dirs.sbindir = 
        opal_install_dirs_expand(opal_install_dirs.sbindir);
    opal_install_dirs.libexecdir = 
        opal_install_dirs_expand(opal_install_dirs.libexecdir);
    opal_install_dirs.datarootdir = 
        opal_install_dirs_expand(opal_install_dirs.datarootdir);
    opal_install_dirs.datadir = 
        opal_install_dirs_expand(opal_install_dirs.datadir);
    opal_install_dirs.sysconfdir = 
        opal_install_dirs_expand(opal_install_dirs.sysconfdir);
    opal_install_dirs.sharedstatedir = 
        opal_install_dirs_expand(opal_install_dirs.sharedstatedir);
    opal_install_dirs.localstatedir = 
        opal_install_dirs_expand(opal_install_dirs.localstatedir);
    opal_install_dirs.libdir = 
        opal_install_dirs_expand(opal_install_dirs.libdir);
    opal_install_dirs.includedir = 
        opal_install_dirs_expand(opal_install_dirs.includedir);
    opal_install_dirs.infodir = 
        opal_install_dirs_expand(opal_install_dirs.infodir);
    opal_install_dirs.mandir = 
        opal_install_dirs_expand(opal_install_dirs.mandir);
    opal_install_dirs.pkgdatadir = 
        opal_install_dirs_expand(opal_install_dirs.pkgdatadir);
    opal_install_dirs.pkglibdir = 
        opal_install_dirs_expand(opal_install_dirs.pkglibdir);
    opal_install_dirs.pkgincludedir = 
        opal_install_dirs_expand(opal_install_dirs.pkgincludedir);

#if 0
    fprintf(stderr, "prefix:         %s\n", opal_install_dirs.prefix);
    fprintf(stderr, "exec_prefix:    %s\n", opal_install_dirs.exec_prefix);
    fprintf(stderr, "bindir:         %s\n", opal_install_dirs.bindir);
    fprintf(stderr, "sbindir:        %s\n", opal_install_dirs.sbindir);
    fprintf(stderr, "libexecdir:     %s\n", opal_install_dirs.libexecdir);
    fprintf(stderr, "datarootdir:    %s\n", opal_install_dirs.datarootdir);
    fprintf(stderr, "datadir:        %s\n", opal_install_dirs.datadir);
    fprintf(stderr, "sysconfdir:     %s\n", opal_install_dirs.sysconfdir);
    fprintf(stderr, "sharedstatedir: %s\n", opal_install_dirs.sharedstatedir);
    fprintf(stderr, "localstatedir:  %s\n", opal_install_dirs.localstatedir);
    fprintf(stderr, "libdir:         %s\n", opal_install_dirs.libdir);
    fprintf(stderr, "includedir:     %s\n", opal_install_dirs.includedir);
    fprintf(stderr, "infodir:        %s\n", opal_install_dirs.infodir);
    fprintf(stderr, "mandir:         %s\n", opal_install_dirs.mandir);
    fprintf(stderr, "pkgdatadir:     %s\n", opal_install_dirs.pkgdatadir);
    fprintf(stderr, "pkglibdir:      %s\n", opal_install_dirs.pkglibdir);
    fprintf(stderr, "pkgincludedir:  %s\n", opal_install_dirs.pkgincludedir);
#endif

    for (i = 0 ; mca_installdirs_base_static_components[i] != NULL ; ++i) {
        if (NULL !=  mca_installdirs_base_static_components[i]->mca_close_component) {
            mca_installdirs_base_static_components[i]->mca_close_component();
        }
    }

    return OPAL_SUCCESS;
}


int
opal_installdirs_base_close(void)
{
    opal_list_item_t *item;

    free(opal_install_dirs.prefix);
    free(opal_install_dirs.exec_prefix);
    free(opal_install_dirs.bindir);
    free(opal_install_dirs.sbindir);
    free(opal_install_dirs.libexecdir);
    free(opal_install_dirs.datarootdir);
    free(opal_install_dirs.datadir);
    free(opal_install_dirs.sysconfdir);
    free(opal_install_dirs.sharedstatedir);
    free(opal_install_dirs.localstatedir);
    free(opal_install_dirs.libdir);
    free(opal_install_dirs.includedir);
    free(opal_install_dirs.infodir);
    free(opal_install_dirs.mandir);
    free(opal_install_dirs.pkgdatadir);
    free(opal_install_dirs.pkglibdir);
    free(opal_install_dirs.pkgincludedir);

    for (item = opal_list_remove_first(&opal_installdirs_components);
         NULL != item; 
         item = opal_list_remove_first(&opal_installdirs_components)) {
        OBJ_RELEASE(item);
    }
    OBJ_DESTRUCT(&opal_installdirs_components);

    return OPAL_SUCCESS;
}

