dnl
dnl $Id: config.m4,v 1.30.2.8 2004/08/18 05:14:16 tony2001 Exp $
dnl
dnl +------------------------------------------------------------------------------+
dnl |  This is where the magic of the extension reallly is.  Depending on what     |
dnl |  backend the user chooses, this script performs the magic                    |
dnl +------------------------------------------------------------------------------+

PHP_ARG_ENABLE(xslt, whether to enable xslt support,
[  --enable-xslt           Enable xslt support.])

PHP_ARG_WITH(xslt-sablot, for XSLT Sablotron backend,
[  --with-xslt-sablot=<DIR>  XSLT: Enable the sablotron backend.], no, no)

PHP_ARG_WITH(expat-dir, for libexpat dir for Sablotron XSL support,
[  --with-expat-dir=<DIR>    XSLT: libexpat dir for Sablotron.], no, no)

PHP_ARG_WITH(iconv-dir, for iconv dir for Sablotron XSL support,
[  --with-iconv-dir=<DIR>    XSLT: iconv dir for Sablotron.], no, no)

PHP_ARG_WITH(sablot-js, for JavaScript for Sablotron XSL support,
[  --with-sablot-js=<DIR>    XSLT: enable JavaScript support for Sablotron.], no, no)


if test "$PHP_XSLT" != "no"; then

  PHP_NEW_EXTENSION(xslt, xslt.c sablot.c, $ext_shared)
  PHP_SUBST(XSLT_SHARED_LIBADD)

  if test "$PHP_XSLT_SABLOT" != "no"; then
    XSLT_CHECK_DIR=$PHP_XSLT_SABLOT
    XSLT_TEST_FILE=/include/sablot.h
    XSLT_BACKEND_NAME=Sablotron
    XSLT_LIBNAME=sablot
  fi

  if test -z "$XSLT_BACKEND_NAME"; then
    AC_MSG_ERROR([No backend specified for XSLT extension.])
  fi

  condition="$XSLT_CHECK_DIR$XSLT_TEST_FILE"

  if test -r $condition; then
    XSLT_DIR=$XSLT_CHECK_DIR
  else
    AC_MSG_CHECKING(for $XSLT_BACKEND_NAME libraries in the default path)
    for i in /usr/local /usr; do
      condition="$i$XSLT_TEST_FILE"
      if test -r $condition; then
        XSLT_DIR=$i
        break
      fi
    done
    AC_MSG_RESULT(found in $XSLT_DIR)
  fi

  if test -z "$XSLT_DIR"; then
    AC_MSG_ERROR([not found. Please re-install the $XSLT_BACKEND_NAME distribution.])
  fi
				
  if test "$PHP_XSLT_SABLOT" != "no"; then
    AC_MSG_CHECKING([for sablot-config])
    if test -x $XSLT_DIR/bin/sablot-config ; then
       AC_MSG_RESULT(found)
       AC_DEFINE(HAVE_SABLOT_CONFIG, 1, [Whether the Sablotron config file is found])
       dnl Use this script to register this information in phpinfo()
       SABINF_CFLAGS=`$XSLT_DIR/bin/sablot-config --cflags`
       SABINF_LIBS=`$XSLT_DIR/bin/sablot-config --libs`
       SABINF_PREFIX=`$XSLT_DIR/bin/sablot-config --prefix`
       SABINF_ALL="\"Cflags: $SABINF_CFLAGS Libs: $SABINF_LIBS Prefix: $SABINF_PREFIX\""
       PHP_DEFINE(SAB_INFO, "$SABINF_ALL")
    else
       AC_MSG_RESULT(not found)
    fi
    AC_MSG_CHECKING([for Sablotron version])
    old_CPPFLAGS=$CPPFLAGS
    CPPFLAGS="$CPPFLAGS -I$XSLT_DIR/include"
    AC_TRY_RUN([
#include <stdlib.h>
#include <sablot.h>

int main ()
{
	double version;
	version = atof(SAB_VERSION);
	
	if (version >= 0.96) {
		exit(0);
	}
	exit(255);
}
    ],[
      AC_MSG_RESULT([>= 0.96])
    ],[
      AC_MSG_ERROR([Sablotron version 0.96 or greater required.])
    ])
    CPPFLAGS=$old_CPPFLAGS

    found_expat=no
    for i in $PHP_EXPAT_DIR $XSLT_DIR /usr/local /usr; do
      if test -f $i/lib/libexpat.a -o -f $i/lib/libexpat.$SHLIB_SUFFIX_NAME; then
        AC_DEFINE(HAVE_LIBEXPAT2, 1, [ ])
        PHP_ADD_INCLUDE($i/include)
        PHP_ADD_LIBRARY_WITH_PATH(expat, $i/lib, XSLT_SHARED_LIBADD)
        found_expat=yes
        break
      fi
    done

    if test "$found_expat" = "no"; then
      AC_MSG_ERROR([expat not found. To build sablotron you need the expat library.])
    fi

    if test "$PHP_ICONV_DIR" != "no"; then
      PHP_ICONV=$PHP_ICONV_DIR
    fi

    if test "$PHP_ICONV" = "no"; then
      PHP_ICONV=yes
    fi

    PHP_SETUP_ICONV(XSLT_SHARED_LIBADD, [], [
      AC_MSG_ERROR([iconv not found. To build sablotron you need the iconv library.])
    ])
     
    if test "$PHP_SABLOT_JS" != "no"; then
      for i in $PHP_SABLOT_JS /usr/local /usr; do
        if test -f $i/lib/libjs.a -o -f $i/lib/libjs.$SHLIB_SUFFIX_NAME; then
          PHP_SABLOT_JS_DIR=$i
          break
        fi
      done

      PHP_CHECK_LIBRARY(js, JS_GetRuntime,
      [
        PHP_ADD_LIBRARY_WITH_PATH(js, $PHP_SABLOT_JS_DIR/lib, XSLT_SHARED_LIBADD)
        PHP_SABLOT_JS_LIBS="-L$PHP_SABLOT_JS_DIR/lib -ljs"
      ], [
        AC_MSG_ERROR([libjs not found. Please check config.log for more information.])
      ], [
        -L$PHP_SABLOT_JS_DIR/lib
      ])
    fi

    PHP_CHECK_LIBRARY(sablot, SablotSetEncoding,
    [
      AC_DEFINE(HAVE_SABLOT_SET_ENCODING, 1, [ ])
    ], [], [
      -L$XSLT_DIR/lib $PHP_SABLOT_JS_LIBS
    ])

    dnl SablotSetOptions implemented in Sablotron CVS > 2002/10/31
    PHP_CHECK_LIBRARY(sablot, SablotGetOptions,
    [
      AC_DEFINE(HAVE_SABLOT_GET_OPTIONS, 1, [Whether Sablotron supports SablotGetOptions])
    ], [], [
      -L$XSLT_DIR/lib $PHP_SABLOT_JS_LIBS
    ])

    AC_DEFINE(HAVE_SABLOT_BACKEND, 1, [ ])
  fi

  PHP_ADD_INCLUDE($XSLT_DIR/include)
  PHP_ADD_LIBRARY_WITH_PATH($XSLT_LIBNAME, $XSLT_DIR/lib, XSLT_SHARED_LIBADD)

  AC_DEFINE(HAVE_XSLT, 1, [ ])
fi
