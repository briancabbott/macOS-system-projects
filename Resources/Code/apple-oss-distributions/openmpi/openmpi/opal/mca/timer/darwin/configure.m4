# -*- shell-script -*-
#
# Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
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

AC_DEFUN([MCA_timer_darwin_COMPILE_MODE], [
    AC_MSG_CHECKING([for MCA component $2:$3 compile mode])
    $4="static"
    AC_MSG_RESULT([$$4])
])


# MCA_timer_darwin_CONFIG(action-if-can-compile, 
#                        [action-if-cant-compile])
# ------------------------------------------------
AC_DEFUN([MCA_timer_darwin_CONFIG],[
    AC_ARG_WITH([timer],
        [AC_HELP_STRING([--with-timer=TYPE],
                        [Build high resolution timer component TYPE])])

    AS_IF([test "$with_timer" = "darwin"],
          [timer_darwin_happy="yes"
           timer_darwin_should_use=1],
          [timer_darwin_should_use=0
           AS_IF([test "$with_timer" = ""],
                 [timer_darwin_happy="yes"],
                 [timer_darwin_happy="no"])])

    AS_IF([test "$timer_darwin_happy" = "yes"],
          [AC_CHECK_HEADERS([mach/mach_time.h])
           AC_CHECK_FUNC([mach_absolute_time], 
                         [timer_darwin_happy="yes"],
                         [timer_darwin_happy="no"])])

   AS_IF([test "$timer_darwin_happy" = "no" -a \
               "$timer_darwin_should_use" = "1"],
         [AC_MSG_ERROR([Darwin timer requested but not available.  Aborting.])])

    AS_IF([test "$timer_darwin_happy" = "yes"], 
          [timer_base_include="darwin/timer_darwin.h"
           $1], 
          [$2])
])
