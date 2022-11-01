dnl
dnl $Id: config.m4,v 1.22.2.2 2003/10/01 02:53:56 sniper Exp $
dnl

PHP_ARG_WITH(fdftk, for FDF support,
[  --with-fdftk[=DIR]      Include FDF support.])

if test "$PHP_FDFTK" != "no"; then

  case $host_os in
    aix*[)]
      libtype=aix
    ;;
    solaris*[)]
      libtype=solaris
    ;;
    linux*[)]
      libtype=linux
    ;;
    *[)]
      AC_MSG_ERROR([The fdf toolkit is not available for $host_os.])
    ;;
  esac

  if test "$PHP_FDFTK" = "yes"; then 
    PHP_FDFTK="/usr/local /usr ../FDFToolkitForUNIX ext/fdf/FDFToolkitForUNIX ../fdftk ext/fdf/fdftk"
  fi

  for dir in $PHP_FDFTK; do
    for subdir in include HeadersAndLibraries/headers; do
      if test -r $dir/$subdir/FdfTk.h; then
        FDFTK_DIR=$dir
        FDFTK_H_DIR=$dir/$subdir
        break 2
      elif test -r $dir/$subdir/fdftk.h; then
        AC_DEFINE(HAVE_FDFTK_H_LOWER,1,[ ])
        FDFTK_DIR=$dir
        FDFTK_H_DIR=$dir/$subdir
        break 2
      fi
    done
  done

  if test -z "$FDFTK_DIR"; then
    AC_MSG_ERROR([FdfTk.h or fdftk.h not found. Please reinstall the fdf toolkit.])
  fi

  PHP_ADD_INCLUDE($FDFTK_H_DIR)

  FDFLIBRARY=""
  for file in fdftk FdfTk; do
    for dir in $FDFTK_DIR/lib $FDFTK_DIR/HeadersAndLibraries/$libtype/C; do
      if test -r $dir/lib$file.so; then
        PHP_CHECK_LIBRARY($file, FDFOpen, [FDFLIBRARY=$file], [], [-L$dir -lm])
        if test "$FDFLIBRARY"; then
          PHP_CHECK_LIBRARY($file, FDFGetFDFVersion, [AC_DEFINE(HAVE_FDFTK_5,1,[ ])], [], [-L$dir -lm])
          FDFTK_LIB_DIR=$dir
          break 2
        fi
      fi
    done
  done

  if test -z "$FDFLIBRARY"; then
    AC_MSG_ERROR(no usable fdf library found)
  fi

  PHP_ADD_LIBRARY_WITH_PATH($FDFLIBRARY, $FDFTK_LIB_DIR, FDF_SHARED_LIBADD)

  PHP_NEW_EXTENSION(fdf, fdf.c, $ext_shared)
  PHP_SUBST(FDF_SHARED_LIBADD)
  AC_DEFINE(HAVE_FDFLIB,1,[ ])
fi
