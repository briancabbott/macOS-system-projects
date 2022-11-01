dnl
dnl $Id: config.m4,v 1.23.2.2 2003/10/01 02:54:06 sniper Exp $
dnl

PHP_ARG_ENABLE(session, whether to enable PHP sessions,
[  --disable-session       Disable session support], yes)

PHP_ARG_WITH(mm,for mm support,
[  --with-mm[=DIR]         Include mm support for session storage], no, no)

if test "$PHP_SESSION" != "no"; then
  PHP_PWRITE_TEST
  PHP_PREAD_TEST
  PHP_NEW_EXTENSION(session, session.c mod_files.c mod_mm.c mod_user.c, $ext_shared)
  PHP_SUBST(SESSION_SHARED_LIBADD)
  AC_DEFINE(HAVE_PHP_SESSION,1,[ ])
fi

if test "$PHP_MM" != "no"; then
  for i in $PHP_MM /usr/local /usr; do
    test -f "$i/include/mm.h" && MM_DIR=$i && break
  done

  if test -z "$MM_DIR" ; then
    AC_MSG_ERROR(cannot find mm library)
  fi
  
  PHP_ADD_LIBRARY_WITH_PATH(mm, $MM_DIR/lib, SESSION_SHARED_LIBADD)
  PHP_ADD_INCLUDE($MM_DIR/include)
  AC_DEFINE(HAVE_LIBMM, 1, [Whether you have libmm])
fi
