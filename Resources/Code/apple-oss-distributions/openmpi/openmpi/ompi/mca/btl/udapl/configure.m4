# -*- shell-script -*-
#
# Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#


# MCA_btl_udapl_CONFIG([action-if-can-compile], 
#                      [action-if-cant-compile])
# ------------------------------------------------
AC_DEFUN([MCA_btl_udapl_CONFIG],[
    OMPI_CHECK_UDAPL([btl_udapl],
                     [btl_udapl_happy="yes"],
                     [btl_udapl_happy="no"])

    AS_IF([test "$btl_udapl_happy" = "yes"],
          [btl_udapl_WRAPPER_EXTRA_LDFLAGS="$btl_udapl_LDFLAGS"
           btl_udapl_WRAPPER_EXTRA_LIBS="$btl_udapl_LIBS"
           $1],
          [$2])

    # Borrowed from MVAPI BTL - a data structure in the uDAPL headers
    # is not fully ISO C.  Remove -pedantic to silence a warning.
    btl_udapl_CFLAGS="`echo $CFLAGS | sed 's/-pedantic//g'`"
    AS_IF([test "$btl_udapl_CFLAGS" != "$CFLAGS" -a "$btl_udapl_happy" = "yes"],
          [AC_MSG_WARN([Removed -pedantic from CFLAGS for
uDAPL component because the uDAPL headers are not fully ISO C])])

    # substitute in the things needed to build udapl
    AC_SUBST([btl_udapl_CFLAGS])
    AC_SUBST([btl_udapl_CPPFLAGS])
    AC_SUBST([btl_udapl_LDFLAGS])
    AC_SUBST([btl_udapl_LIBS])
])dnl
