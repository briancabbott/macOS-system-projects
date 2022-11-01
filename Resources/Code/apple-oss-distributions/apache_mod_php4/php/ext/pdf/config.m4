dnl
dnl $Id: config.m4,v 1.35.2.6 2004/11/29 09:13:39 derick Exp $
dnl

PHP_ARG_WITH(pdflib,for PDFlib support,
[  --with-pdflib[=DIR]     Include PDFlib support.])

if test -z "$PHP_JPEG_DIR"; then
  PHP_ARG_WITH(jpeg-dir, for the location of libjpeg, 
  [  --with-jpeg-dir[=DIR]     PDFLIB: define libjpeg install directory.
                                     (OPTIONAL for PDFlib v4)], no, no)
fi

if test -z "$PHP_PNG_DIR"; then
  PHP_ARG_WITH(png-dir, for the location of libpng, 
  [  --with-png-dir[=DIR]      PDFLIB: define libpng install directory.
                                     (OPTIONAL for PDFlib v4)], no, no)
fi

if test -z "$PHP_ZLIB_DIR"; then
  PHP_ARG_WITH(zlib-dir, for the location of libz, 
  [  --with-zlib-dir[=DIR]     PDFLIB: define libz install directory.
                                     (OPTIONAL for PDFlib v4)], no, no)
fi

PHP_ARG_WITH(tiff-dir, for the location of libtiff,
[  --with-tiff-dir[=DIR]     PDFLIB: define libtiff install directory.
                                     (OPTIONAL for PDFlib v4)], no, no)

if test "$PHP_PDFLIB" != "no"; then

  PHP_NEW_EXTENSION(pdf, pdf.c, $ext_shared)
  PHP_SUBST(PDF_SHARED_LIBADD)

  dnl #
  dnl # Optional libraries for PDFlib 
  dnl #

  dnl # libjpeg
  if test "$PHP_JPEG_DIR" != "no"; then
    PHP_CHECK_LIBRARY(jpeg,jpeg_read_header, 
    [
      PHP_ADD_LIBRARY_WITH_PATH(jpeg, $PHP_JPEG_DIR/lib, PDF_SHARED_LIBADD)
    ],[
      AC_MSG_ERROR([libjpeg not found!])
    ],[
      -L$PHP_JPEG_DIR/lib
    ])
  else
    AC_MSG_WARN([If configure fails, try --with-jpeg-dir=<DIR>])
  fi

  dnl # libpng
  if test "$PHP_PNG_DIR" != "no"; then
    PHP_CHECK_LIBRARY(png,png_create_info_struct, 
    [
      PHP_ADD_LIBRARY_WITH_PATH(png, $PHP_PNG_DIR/lib, PDF_SHARED_LIBADD)
    ],[
      AC_MSG_ERROR([libpng not found!])
    ],[
      -L$PHP_PNG_DIR/lib
    ])
  else
    AC_MSG_WARN([If configure fails, try --with-png-dir=<DIR>])
  fi

  dnl # libtiff
  if test "$PHP_TIFF_DIR" != "no"; then
    PHP_CHECK_LIBRARY(tiff,TIFFOpen, 
    [
      PHP_ADD_LIBRARY_WITH_PATH(tiff, $PHP_TIFF_DIR/lib, PDF_SHARED_LIBADD)
    ],[
      AC_MSG_ERROR([libtiff not found!])
    ],[
      -L$PHP_TIFF_DIR/lib
    ])
  else
    AC_MSG_WARN([If configure fails, try --with-tiff-dir=<DIR>])
  fi

  dnl # zlib
  AC_MSG_CHECKING([for the location of zlib])
  if test "$PHP_ZLIB_DIR" = "no"; then
    AC_MSG_RESULT([no. If configure fails, try --with-zlib-dir=<DIR>])
  else           
    AC_MSG_RESULT([$PHP_ZLIB_DIR])
    PHP_ADD_LIBRARY_WITH_PATH(z, $PHP_ZLIB_DIR/lib, PDF_SHARED_LIBADD)
  fi

  dnl #
  dnl # The main PDFlib configure
  dnl #

  dnl # MacOSX requires this
  case $host_alias in
    *darwin*)
      PHP_ADD_FRAMEWORK(CoreServices)
      PHP_ADD_FRAMEWORK(ApplicationServices)
      ;;
  esac

  case $PHP_PDFLIB in
    yes)
      AC_CHECK_LIB(pdf, PDF_show_boxed, [
        AC_DEFINE(HAVE_PDFLIB,1,[ ])
        PHP_ADD_LIBRARY(pdf,, PDF_SHARED_LIBADD)
      ],[
        AC_MSG_ERROR([
PDFlib extension requires at least pdflib 3.x. You may also need libtiff, libjpeg, libpng and libz.
Use the options --with-tiff-dir=<DIR>, --with-jpeg-dir=<DIR>, --with-png-dir=<DIR> and --with-zlib-dir=<DIR>
See config.log for more information.
])
      ])
    ;;
    *)
      if test -f "$PHP_PDFLIB/include/pdflib.h" ; then

        PHP_CHECK_LIBRARY(pdf, PDF_show_boxed, 
        [
          AC_DEFINE(HAVE_PDFLIB,1,[ ]) 
          PHP_ADD_LIBRARY_WITH_PATH(pdf, $PHP_PDFLIB/lib, PDF_SHARED_LIBADD)
          PHP_ADD_INCLUDE($PHP_PDFLIB/include)
        ],[
          AC_MSG_ERROR([
PDFlib extension requires at least pdflib 3.x. You may also need libtiff, libjpeg, libpng and libz.
Use the options --with-tiff-dir=<DIR>, --with-jpeg-dir=<DIR>, --with-png-dir=<DIR> and --with-zlib-dir=<DIR>
See config.log for more information.
])
        ],[
          -L$PHP_PDFLIB/lib $PDF_SHARED_LIBADD
        ])
      else
        AC_MSG_ERROR([pdflib.h not found! Check the path passed to --with-pdflib=<PATH>. PATH should be the install prefix directory.])
      fi
    ;;
  esac
fi
