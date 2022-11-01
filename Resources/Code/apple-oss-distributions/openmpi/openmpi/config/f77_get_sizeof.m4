dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2004-2006 The Trustees of Indiana University and Indiana
dnl                         University Research and Technology
dnl                         Corporation.  All rights reserved.
dnl Copyright (c) 2004-2005 The University of Tennessee and The University
dnl                         of Tennessee Research Foundation.  All rights
dnl                         reserved.
dnl Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
dnl                         University of Stuttgart.  All rights reserved.
dnl Copyright (c) 2004-2005 The Regents of the University of California.
dnl                         All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# OMPI_F77_GET_SIZEOF(type, variable to set)
# ------------------------------------------
AC_DEFUN([OMPI_F77_GET_SIZEOF],[
    AS_VAR_PUSHDEF([type_var], [ompi_cv_f77_sizeof_$1])
    
    AC_CACHE_CHECK([size of Fortran 77 $1], type_var,
        [OMPI_F77_MAKE_C_FUNCTION([ompi_ac_size_fn], [size])
         # Fortran module
         cat > conftestf.f <<EOF
       program fsize
       external size 
       $1 x(2)
       call size(x(1),x(2))
       end
EOF

         # C module
         if test -f conftest.h; then
             ompi_conftest_h="#include \"conftest.h\""
         else
             ompi_conftest_h=""
         fi
         cat > conftest.c <<EOF
#include <stdio.h>
#include <stdlib.h>
$ompi_conftest_h

#ifdef __cplusplus
extern "C" {
#endif
void $ompi_ac_size_fn(char *a, char *b)
{
    int diff = (int) (b - a);
    FILE *f=fopen("conftestval", "w");
    if (!f) exit(1);
    fprintf(f, "%d\n", diff);
    fclose(f);
}
#ifdef __cplusplus
}
#endif
EOF

         OMPI_LOG_COMMAND([$CC $CFLAGS -I. -c conftest.c],
             [OMPI_LOG_COMMAND([$F77 $FFLAGS conftestf.f conftest.o -o conftest $LDFLAGS $LIBS],
                  [happy="yes"], [happy="no"])], [happy="no"])

         if test "$happy" = "no" ; then
             AC_MSG_ERROR([Could not determine size of $1])
         fi

         AS_IF([test "$cross_compiling" = "yes"],
             [AC_MSG_ERROR([Can not determine size of $1 when cross-compiling])],
             [OMPI_LOG_COMMAND([./conftest],
                 [AS_IF([test -f conftestval],
                      [AS_VAR_SET(type_var, [`cat conftestval`])],
                      [OMPI_LOG_MSG([conftestval not found.], 1)
                       AC_MSG_ERROR([Could not determine size of $1])])],
                 [AC_MSG_ERROR([Could not determine size of $1])])])

        unset happy ompi_conftest_h
        rm -rf conftest*])

    $2=AS_VAR_GET(type_var)
    AS_VAR_POPDEF([type_var])dnl
])dnl
