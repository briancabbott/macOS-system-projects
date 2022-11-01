# float_h.m4 serial 1
dnl Copyright (C) 2007 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.

AC_DEFUN([gl_FLOAT_H],
[
  AC_REQUIRE([AC_PROG_CC])
  AC_REQUIRE([AC_CANONICAL_HOST])
  FLOAT_H=
  case "$host_os" in
    beos*)
      FLOAT_H=float.h
      gl_ABSOLUTE_HEADER([float.h])
      ABSOLUTE_FLOAT_H=\"$gl_cv_absolute_float_h\"
      AC_SUBST([ABSOLUTE_FLOAT_H])
      ;;
  esac
  AC_SUBST([FLOAT_H])
])
