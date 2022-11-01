dnl
dnl $Id: config.m4,v 1.38.2.3 2004/12/20 20:38:18 sniper Exp $
dnl

PHP_C_BIGENDIAN

if test "$ac_cv_c_bigendian_php" = "yes"; then
  order=4321
else
  order=1234
fi

PHP_ARG_ENABLE(xml,whether to enable XML support,
[  --disable-xml           Disable XML support using bundled expat lib], yes)

PHP_ARG_WITH(expat-dir, external libexpat install dir,
[  --with-expat-dir=<DIR>    XML: external libexpat install dir], no, no)

if test "$PHP_XML" = "yes"; then
  AC_DEFINE(HAVE_LIBEXPAT,  1, [ ])

  if test "$PHP_EXPAT_DIR" = "no"; then
    AC_DEFINE(HAVE_LIBEXPAT_BUNDLED, 1, [Bundled libexpat is used.])
    AC_DEFINE(XML_NS, 1, [Define to make XML Namespaces functionality available.])
    AC_DEFINE(XML_DTD, 1, [Define to make parameter entity parsing functionality available.])
    AC_DEFINE(XML_CONTEXT_BYTES, 1024, [Define to specify how much context to retain around the current parse point.])
    PHP_NEW_EXTENSION(xml, xml.c expat/xmlparse.c expat/xmlrole.c expat/xmltok.c, $ext_shared,,-DBYTEORDER=$order)
    PHP_ADD_INCLUDE($ext_srcdir/expat)
    PHP_ADD_BUILD_DIR($ext_builddir/expat)
  else
    PHP_NEW_EXTENSION(xml, xml.c, $ext_shared)

    for i in $PHP_XML $PHP_EXPAT_DIR; do
      if test -f $i/lib/libexpat.a -o -f $i/lib/libexpat.$SHLIB_SUFFIX_NAME ; then
        EXPAT_DIR=$i
      fi
    done

    if test -z "$EXPAT_DIR"; then
      AC_MSG_ERROR(not found. Please reinstall the expat distribution.)
    fi

    PHP_ADD_INCLUDE($EXPAT_DIR/include)
    PHP_ADD_LIBRARY_WITH_PATH(expat, $EXPAT_DIR/lib, XML_SHARED_LIBADD)
    PHP_SUBST(XML_SHARED_LIBADD)
  fi
fi
