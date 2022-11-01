dnl
dnl $Id: config0.m4,v 1.12.2.3 2003/07/01 23:54:44 sas Exp $
dnl

PHP_ARG_WITH(zlib,for ZLIB support,
[  --with-zlib[=DIR]       Include ZLIB support (requires zlib >= 1.0.9).])

PHP_ARG_WITH(zlib-dir,if the location of ZLIB install directory is defined,
[  --with-zlib-dir=<DIR>   Define the location of zlib install directory], no, no)

if test "$PHP_ZLIB" != "no" -o "$PHP_ZLIB_DIR" != "no"; then
  PHP_NEW_EXTENSION(zlib, zlib.c zlib_fopen_wrapper.c, $ext_shared)
  PHP_SUBST(ZLIB_SHARED_LIBADD)
  
  if test "$PHP_ZLIB" != "yes" -a "$PHP_ZLIB" != "no"; then 
    if test -f $PHP_ZLIB/include/zlib/zlib.h; then
      ZLIB_DIR=$PHP_ZLIB
      ZLIB_INCDIR=$ZLIB_DIR/include/zlib
    elif test -f $PHP_ZLIB/include/zlib.h; then
      ZLIB_DIR=$PHP_ZLIB
      ZLIB_INCDIR=$ZLIB_DIR/include
    fi
  else 
    for i in /usr/local /usr $PHP_ZLIB_DIR; do
      if test -f $i/include/zlib/zlib.h; then
        ZLIB_DIR=$i
        ZLIB_INCDIR=$i/include/zlib
      elif test -f $i/include/zlib.h; then
        ZLIB_DIR=$i
        ZLIB_INCDIR=$i/include
      fi
    done
  fi
  
  if test -z "$ZLIB_DIR"; then
    AC_MSG_ERROR(Cannot find libz)
  fi

  PHP_CHECK_LIBRARY(z, gzgets, [
    AC_DEFINE(HAVE_ZLIB,1,[ ]) 
  ],[
    AC_MSG_ERROR(ZLIB extension requires zlib >= 1.0.9)
  ],[
    -L$ZLIB_DIR/lib
  ])

  PHP_ADD_LIBPATH($ZLIB_DIR/lib, ZLIB_SHARED_LIBADD)

  PHP_ZLIB_DIR=$ZLIB_DIR
  PHP_ADD_LIBRARY(z,, ZLIB_SHARED_LIBADD)
  PHP_ADD_INCLUDE($ZLIB_INCDIR)

fi
