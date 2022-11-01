dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
dnl                         University Research and Technology
dnl                         Corporation.  All rights reserved.
dnl Copyright (c) 2004-2005 The University of Tennessee and The University
dnl                         of Tennessee Research Foundation.  All rights
dnl                         reserved.
dnl Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
dnl                         University of Stuttgart.  All rights reserved.
dnl Copyright (c) 2004-2005 The Regents of the University of California.
dnl                         All rights reserved.
dnl Copyright (c) 2006      Los Alamos National Security, LLC.  All rights
dnl                         reserved. 
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl
dnl sets:
dnl  F77                   : full pathname to compiler
dnl  BASEF77               : compiler name (no path)
dnl  OMPI_WANT_F77_BINDINGS : (actually set by ompi_configure_options, may be
dnl                          redefined here)
dnl  FC                    : Same as F77.  Side effect of AC_PROG_FC.  Should
dnl                          not be used
dnl defines:
dnl  OMPI_F77               : same as F77
dnl  OMPI_WANT_F77_BINDINGS :
dnl am_conditional:
dnl  OMPI_WANT_F77_BINDINGS :

AC_DEFUN([OMPI_SETUP_F77],[

# Modularize this setup so that sub-configure.in scripts can use this
# same setup code.

ompi_show_subtitle "Fortran 77 compiler" 

#
# Check for the compiler
#
# Note that we don't actually *use* the fortran compiler to build
# anything in OMPI; it's only used here in configure to find out
# symbol conventions, type sizes, etc.  We also pass it down to
# the wrapper compiler mpif77.
#
# Always run this test, even if fortran isn't wanted so that F77 has
# value for the Fint tests
#
ompi_fflags_save="$FFLAGS"
AC_PROG_F77([gfortran g77 f77 xlf frt ifort pgf77 fort77 fl32 af77])
FFLAGS="$ompi_fflags_save"
if test -z "$F77"; then
    AC_MSG_WARN([*** Fortran 77 bindings disabled (could not find compiler)])
    OMPI_WANT_F77_BINDINGS=0
    OMPI_F77="none"
    BASEF77="none"
    OMPI_F77_ABSOLUTE="none"
else
    OMPI_F77="$F77"
    set dummy $OMPI_F77
    OMPI_F77_ARGV0=[$]2
    BASEF77="`basename $OMPI_F77_ARGV0`"
    OMPI_F77_ABSOLUTE="`which $OMPI_F77_ARGV0`"
    
    if test "$OMPI_WANT_F77_BINDINGS" = "0" ; then
        AC_MSG_WARN([*** Fortran 77 bindings disabled by user])
        OMPI_WANT_F77_BINDINGS=0
    else
        OMPI_WANT_F77_BINDINGS=1
    fi
fi

# make sure the compiler actually works, if not cross-compiling
# Don't just use the AC macro so that we can have a pretty
# message.
AS_IF([test $OMPI_WANT_F77_BINDINGS -eq 1],
       [OMPI_CHECK_COMPILER_WORKS([Fortran 77], [], [], [], 
           [AC_MSG_ERROR([Could not run a simple Fortran 77 program.  Aborting.])])])

# now make sure we know our linking convention...
OMPI_F77_FIND_EXT_SYMBOL_CONVENTION

# Make sure we can link with C code...
AS_IF([test $OMPI_WANT_F77_BINDINGS -eq 1],
  [OMPI_LANG_LINK_WITH_C([Fortran 77], [],
    [cat <<EOF
**********************************************************************
* It appears that your Fortran 77 compiler is unable to link against
* object files created by your C compiler.  This generally indicates
* either a conflict between the options specified in CFLAGS and FFLAGS
* or a problem with the local compiler installation.  More
* information (including exactly what command was given to the 
* compilers and what error resulted when the commands were executed) is
* available in the config.log file in this directory.
**********************************************************************
EOF
     AC_MSG_ERROR([C and Fortran 77 compilers are not link compatible.  Can not continue.])])])

AC_DEFINE_UNQUOTED(OMPI_WANT_F77_BINDINGS, $OMPI_WANT_F77_BINDINGS,
    [Whether we want the MPI f77 bindings or not])
AC_DEFINE_UNQUOTED(OMPI_F77, "$OMPI_F77", [OMPI underlying F77 compiler])
AM_CONDITIONAL(OMPI_WANT_F77_BINDINGS, test "$OMPI_WANT_F77_BINDINGS" = "1")
AC_SUBST(OMPI_F77_ABSOLUTE)
])
