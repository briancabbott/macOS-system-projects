dnl
dnl $Id: config0.m4,v 1.2.2.3 2004/12/30 14:54:35 sniper Exp $
dnl

PHP_ARG_WITH(openssl, for OpenSSL support,
[  --with-openssl[=DIR]    Include OpenSSL support (requires OpenSSL >= 0.9.6)])

PHP_ARG_WITH(kerberos, for Kerberos support,
[  --with-kerberos[=DIR]     OPENSSL: Include Kerberos support], no, no)

if test "$PHP_OPENSSL" != "no"; then
  PHP_NEW_EXTENSION(openssl, openssl.c, $ext_shared)
  PHP_SUBST(OPENSSL_SHARED_LIBADD)

  if test "$PHP_KERBEROS" != "no"; then
    PHP_SETUP_KERBEROS(OPENSSL_SHARED_LIBADD)
  fi

  PHP_SETUP_OPENSSL(OPENSSL_SHARED_LIBADD, 
  [
    if test "$ext_shared" = "yes"; then
      AC_DEFINE(HAVE_OPENSSL_SHARED_EXT,1,[ ])
    else
      AC_DEFINE(HAVE_OPENSSL_EXT,1,[ ])
    fi
  ], [
    AC_MSG_ERROR([OpenSSL check failed. Please check config.log for more information.])
  ])
fi
