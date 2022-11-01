dnl
dnl $Id: config.m4,v 1.7.4.3 2003/11/19 04:44:24 sniper Exp $
dnl

PHP_ARG_WITH(gmp, for GNU MP support,
[  --with-gmp[=DIR]        Include GNU MP support])

if test "$PHP_GMP" != "no"; then

  for i in $PHP_GMP /usr/local /usr; do
    test -f $i/include/gmp.h && GMP_DIR=$i && break
  done

  if test -z "$GMP_DIR"; then
    AC_MSG_ERROR(Unable to locate gmp.h)
  fi
 
  PHP_CHECK_LIBRARY(gmp, __gmp_randinit_lc_2exp_size,
  [],[
    PHP_CHECK_LIBRARY(gmp, gmp_randinit_lc_2exp_size,
    [],[
      AC_MSG_ERROR([GNU MP Library version 4.1.2 or greater required.])
    ],[
      -L$GMP_DIR/lib
    ])
  ],[
    -L$GMP_DIR/lib
  ])

  PHP_ADD_LIBRARY_WITH_PATH(gmp, $GMP_DIR/lib, GMP_SHARED_LIBADD)
  PHP_ADD_INCLUDE($GMP_DIR/include)

  PHP_NEW_EXTENSION(gmp, gmp.c, $ext_shared)
  PHP_SUBST(GMP_SHARED_LIBADD)
  AC_DEFINE(HAVE_GMP, 1, [ ])
fi
