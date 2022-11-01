dnl See whether we can use IPv6 related functions
dnl contributed by Hajimu UMEMOTO

AC_DEFUN(IPv6_CHECK_FUNC, [
changequote(, )dnl
ac_tr_lib=HAVE_`echo $1 | sed -e 's/[^a-zA-Z0-9_]/_/g' \
  -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/'`
changequote([, ])dnl
AC_CHECK_FUNC($1, [dnl
  AC_DEFINE_UNQUOTED($ac_tr_lib)
  ac_cv_lib_socket_$1=no
  ac_cv_lib_inet6_$1=no
], [dnl
  AC_CHECK_LIB(socket, $1, [dnl
    AC_DEFINE_UNQUOTED($ac_tr_lib)
    LIBS="$LIBS -lsocket"
    ac_cv_lib_inet6_$1=no
  ], [dnl
    AC_MSG_CHECKING([whether your system has IPv6 directory])
    AC_CACHE_VAL(ipv6_cv_dir, [dnl
      for ipv6_cv_dir in /usr/local/v6 /usr/inet6 no; do
	if test $ipv6_cv_dir = no -o -d $ipv6_cv_dir; then
	  break
	fi
      done])dnl
    AC_MSG_RESULT($ipv6_cv_dir)
    if test $ipv6_cv_dir = no; then
      ac_cv_lib_inet6_$1=no
    else
      if test x$ipv6_libinet6 = x; then
	ipv6_libinet6=no
	SAVELDFLAGS="$LDFLAGS"
	LDFLAGS="$LDFLAGS -L$ipv6_cv_dir/lib"
      fi
      AC_CHECK_LIB(inet6, $1, [dnl
	AC_DEFINE_UNQUOTED($ac_tr_lib)
	if test $ipv6_libinet6 = no; then
	  ipv6_libinet6=yes
	  LIBS="$LIBS -linet6"
	fi],)dnl
      if test $ipv6_libinet6 = no; then
	LDFLAGS="$SAVELDFLAGS"
      fi
    fi])dnl
])dnl
if test $ac_cv_func_$1 = yes -o $ac_cv_lib_socket_$1 = yes \
     -o $ac_cv_lib_inet6_$1 = yes
then
  ipv6_cv_$1=yes
  ifelse([$2], , :, [$2])
else
  ipv6_cv_$1=no
  ifelse([$3], , :, [$3])
fi])


dnl See whether we have ss_family in sockaddr_storage
AC_DEFUN(IPv6_CHECK_SS_FAMILY, [
AC_MSG_CHECKING([whether you have ss_family in struct sockaddr_storage])
AC_CACHE_VAL(ipv6_cv_ss_family, [dnl
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/socket.h>],
	[struct sockaddr_storage ss; int i = ss.ss_family;],
	[ipv6_cv_ss_family=yes], [ipv6_cv_ss_family=no])])dnl
if test $ipv6_cv_ss_family = yes; then
  ifelse([$1], , AC_DEFINE(HAVE_SS_FAMILY), [$1])
else
  ifelse([$2], , :, [$2])
fi
AC_MSG_RESULT($ipv6_cv_ss_family)])


dnl whether you have sa_len in struct sockaddr
AC_DEFUN(IPv6_CHECK_SA_LEN, [
AC_MSG_CHECKING([whether you have sa_len in struct sockaddr])
AC_CACHE_VAL(ipv6_cv_sa_len, [dnl
AC_TRY_COMPILE([#include <sys/types.h>
#include <sys/socket.h>],
	       [struct sockaddr sa; int i = sa.sa_len;],
	       [ipv6_cv_sa_len=yes], [ipv6_cv_sa_len=no])])dnl
if test $ipv6_cv_sa_len = yes; then
  ifelse([$1], , AC_DEFINE(HAVE_SOCKADDR_SA_LEN), [$1])
else
  ifelse([$2], , :, [$2])
fi
AC_MSG_RESULT($ipv6_cv_sa_len)])


dnl See whether sys/socket.h has socklen_t
AC_DEFUN(IPv6_CHECK_SOCKLEN_T, [
AC_MSG_CHECKING(for socklen_t)
AC_CACHE_VAL(ipv6_cv_socklen_t, [dnl
AC_TRY_LINK([#include <sys/types.h>
#include <sys/socket.h>],
	    [socklen_t len = 0;],
	    [ipv6_cv_socklen_t=yes], [ipv6_cv_socklen_t=no])])dnl
if test $ipv6_cv_socklen_t = yes; then
  ifelse([$1], , AC_DEFINE(HAVE_SOCKLEN_T), [$1])
else
  ifelse([$2], , :, [$2])
fi
AC_MSG_RESULT($ipv6_cv_socklen_t)])