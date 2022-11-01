dnl
dnl $Id: config.m4,v 1.17.2.5 2003/11/27 23:04:49 sniper Exp $
dnl

AC_ARG_WITH(thttpd,
[  --with-thttpd=SRCDIR    Build PHP as thttpd module],[
  if test ! -d $withval; then
    AC_MSG_RESULT(thttpd directory does not exist ($withval))
  fi

  PHP_EXPAND_PATH($withval, THTTPD)
  
  if grep thttpd.2.21b $withval/version.h >/dev/null; then
    patch="test -f $THTTPD/php_patched || \
    (cd $THTTPD && patch -p1 < $abs_srcdir/sapi/thttpd/thttpd_patch && touch php_patched)"

  elif grep Premium $withval/version.h >/dev/null; then
    patch=
  else
    AC_MSG_ERROR([This version only supports thttpd-2.21b and Premium thttpd])
  fi
  PHP_TARGET_RDYNAMIC
  INSTALL_IT="\
    echo 'PHP_LIBS = -L. -lphp4 \$(PHP_LIBS) \$(EXTRA_LIBS)' > $THTTPD/php_makefile; \
    echo 'PHP_LDFLAGS = \$(NATIVE_RPATHS) \$(PHP_LDFLAGS)' >> $THTTPD/php_makefile; \
    echo 'PHP_CFLAGS = \$(COMMON_FLAGS) \$(CFLAGS_CLEAN) \$(CPPFLAGS) \$(EXTRA_CFLAGS)' >> $THTTPD/php_makefile; \
    rm -f $THTTPD/php_thttpd.c $THTTPD/php_thttpd.h $THTTPD/libphp4.a; \
    \$(LN_S) $abs_srcdir/sapi/thttpd/thttpd.c $THTTPD/php_thttpd.c; \
    \$(LN_S) $abs_srcdir/sapi/thttpd/php_thttpd.h $abs_builddir/$SAPI_STATIC $THTTPD/;\
    $patch"
  PHP_THTTPD="yes, using $THTTPD"
  PHP_ADD_INCLUDE($THTTPD)
  PHP_SELECT_SAPI(thttpd, static)
],[
  PHP_THTTPD=no
])

AC_MSG_CHECKING(for thttpd)
AC_MSG_RESULT($PHP_THTTPD)
