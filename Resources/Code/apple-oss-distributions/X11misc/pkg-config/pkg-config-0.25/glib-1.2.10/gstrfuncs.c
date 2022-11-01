/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GLib Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/*
 * MT safe
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#define _GNU_SOURCE

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <errno.h>
#include <ctype.h>		/* For tolower() */
#if !defined (HAVE_STRSIGNAL) || !defined(NO_SYS_SIGLIST_DECL)
#include <signal.h>
#endif
#include "glib.h"
/* do not include <unistd.h> in this place since it
 * inteferes with g_strsignal() on some OSes
 */

typedef union  _GDoubleIEEE754  GDoubleIEEE754;
#define G_IEEE754_DOUBLE_BIAS   (1023)
/* multiply with base2 exponent to get base10 exponent (nomal numbers) */
#define G_LOG_2_BASE_10         (0.30102999566398119521)
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
union _GDoubleIEEE754
{
  gdouble v_double;
  struct {
    guint mantissa_low : 32;
    guint mantissa_high : 20;
    guint biased_exponent : 11;
    guint sign : 1;
  } mpn;
};
#elif G_BYTE_ORDER == G_BIG_ENDIAN
union _GDoubleIEEE754
{
  gdouble v_double;
  struct {
    guint sign : 1;
    guint biased_exponent : 11;
    guint mantissa_high : 20;
    guint mantissa_low : 32;
  } mpn;
};
#else /* !G_LITTLE_ENDIAN && !G_BIG_ENDIAN */
#error unknown ENDIAN type
#endif /* !G_LITTLE_ENDIAN && !G_BIG_ENDIAN */

gchar*
g_strdup (const gchar *str)
{
  gchar *new_str;

  if (str)
    {
      new_str = g_new (char, strlen (str) + 1);
      strcpy (new_str, str);
    }
  else
    new_str = NULL;

  return new_str;
}

gpointer
g_memdup (gconstpointer mem,
	  guint         byte_size)
{
  gpointer new_mem;

  if (mem)
    {
      new_mem = g_malloc (byte_size);
      memcpy (new_mem, mem, byte_size);
    }
  else
    new_mem = NULL;

  return new_mem;
}

gchar*
g_strndup (const gchar *str,
	   guint        n)
{
  gchar *new_str;

  if (str)
    {
      new_str = g_new (gchar, n + 1);
      strncpy (new_str, str, n);
      new_str[n] = '\0';
    }
  else
    new_str = NULL;

  return new_str;
}

gchar*
g_strnfill (guint length,
	    gchar fill_char)
{
  register gchar *str, *s, *end;

  str = g_new (gchar, length + 1);
  s = str;
  end = str + length;
  while (s < end)
    *(s++) = fill_char;
  *s = 0;

  return str;
}

gchar*
g_strdup_vprintf (const gchar *format,
		  va_list      args1)
{
  gchar *buffer;
  va_list args2;

  G_VA_COPY (args2, args1);

  buffer = g_new (gchar, g_printf_string_upper_bound (format, args1));

  vsprintf (buffer, format, args2);
  va_end (args2);

  return buffer;
}

gchar*
g_strdup_printf (const gchar *format,
		 ...)
{
  gchar *buffer;
  va_list args;

  va_start (args, format);
  buffer = g_strdup_vprintf (format, args);
  va_end (args);

  return buffer;
}

gchar*
g_strconcat (const gchar *string1, ...)
{
  guint	  l;
  va_list args;
  gchar	  *s;
  gchar	  *concat;

  g_return_val_if_fail (string1 != NULL, NULL);

  l = 1 + strlen (string1);
  va_start (args, string1);
  s = va_arg (args, gchar*);
  while (s)
    {
      l += strlen (s);
      s = va_arg (args, gchar*);
    }
  va_end (args);

  concat = g_new (gchar, l);
  concat[0] = 0;

  strcat (concat, string1);
  va_start (args, string1);
  s = va_arg (args, gchar*);
  while (s)
    {
      strcat (concat, s);
      s = va_arg (args, gchar*);
    }
  va_end (args);

  return concat;
}

gdouble
g_strtod (const gchar *nptr,
	  gchar **endptr)
{
  gchar *fail_pos_1;
  gchar *fail_pos_2;
  gdouble val_1;
  gdouble val_2 = 0;

  g_return_val_if_fail (nptr != NULL, 0);

  fail_pos_1 = NULL;
  fail_pos_2 = NULL;

  val_1 = strtod (nptr, &fail_pos_1);

  if (fail_pos_1 && fail_pos_1[0] != 0)
    {
      gchar *old_locale;

      old_locale = g_strdup (setlocale (LC_NUMERIC, NULL));
      setlocale (LC_NUMERIC, "C");
      val_2 = strtod (nptr, &fail_pos_2);
      setlocale (LC_NUMERIC, old_locale);
      g_free (old_locale);
    }

  if (!fail_pos_1 || fail_pos_1[0] == 0 || fail_pos_1 >= fail_pos_2)
    {
      if (endptr)
	*endptr = fail_pos_1;
      return val_1;
    }
  else
    {
      if (endptr)
	*endptr = fail_pos_2;
      return val_2;
    }
}

gchar*
g_strerror (gint errnum)
{
  static GStaticPrivate msg_private = G_STATIC_PRIVATE_INIT;
  char *msg;

#ifdef HAVE_STRERROR
  return strerror (errnum);
#elif NO_SYS_ERRLIST
  switch (errnum)
    {
#ifdef E2BIG
    case E2BIG: return "argument list too long";
#endif
#ifdef EACCES
    case EACCES: return "permission denied";
#endif
#ifdef EADDRINUSE
    case EADDRINUSE: return "address already in use";
#endif
#ifdef EADDRNOTAVAIL
    case EADDRNOTAVAIL: return "can't assign requested address";
#endif
#ifdef EADV
    case EADV: return "advertise error";
#endif
#ifdef EAFNOSUPPORT
    case EAFNOSUPPORT: return "address family not supported by protocol family";
#endif
#ifdef EAGAIN
    case EAGAIN: return "try again";
#endif
#ifdef EALIGN
    case EALIGN: return "EALIGN";
#endif
#ifdef EALREADY
    case EALREADY: return "operation already in progress";
#endif
#ifdef EBADE
    case EBADE: return "bad exchange descriptor";
#endif
#ifdef EBADF
    case EBADF: return "bad file number";
#endif
#ifdef EBADFD
    case EBADFD: return "file descriptor in bad state";
#endif
#ifdef EBADMSG
    case EBADMSG: return "not a data message";
#endif
#ifdef EBADR
    case EBADR: return "bad request descriptor";
#endif
#ifdef EBADRPC
    case EBADRPC: return "RPC structure is bad";
#endif
#ifdef EBADRQC
    case EBADRQC: return "bad request code";
#endif
#ifdef EBADSLT
    case EBADSLT: return "invalid slot";
#endif
#ifdef EBFONT
    case EBFONT: return "bad font file format";
#endif
#ifdef EBUSY
    case EBUSY: return "mount device busy";
#endif
#ifdef ECHILD
    case ECHILD: return "no children";
#endif
#ifdef ECHRNG
    case ECHRNG: return "channel number out of range";
#endif
#ifdef ECOMM
    case ECOMM: return "communication error on send";
#endif
#ifdef ECONNABORTED
    case ECONNABORTED: return "software caused connection abort";
#endif
#ifdef ECONNREFUSED
    case ECONNREFUSED: return "connection refused";
#endif
#ifdef ECONNRESET
    case ECONNRESET: return "connection reset by peer";
#endif
#if defined(EDEADLK) && (!defined(EWOULDBLOCK) || (EDEADLK != EWOULDBLOCK))
    case EDEADLK: return "resource deadlock avoided";
#endif
#ifdef EDEADLOCK
    case EDEADLOCK: return "resource deadlock avoided";
#endif
#ifdef EDESTADDRREQ
    case EDESTADDRREQ: return "destination address required";
#endif
#ifdef EDIRTY
    case EDIRTY: return "mounting a dirty fs w/o force";
#endif
#ifdef EDOM
    case EDOM: return "math argument out of range";
#endif
#ifdef EDOTDOT
    case EDOTDOT: return "cross mount point";
#endif
#ifdef EDQUOT
    case EDQUOT: return "disk quota exceeded";
#endif
#ifdef EDUPPKG
    case EDUPPKG: return "duplicate package name";
#endif
#ifdef EEXIST
    case EEXIST: return "file already exists";
#endif
#ifdef EFAULT
    case EFAULT: return "bad address in system call argument";
#endif
#ifdef EFBIG
    case EFBIG: return "file too large";
#endif
#ifdef EHOSTDOWN
    case EHOSTDOWN: return "host is down";
#endif
#ifdef EHOSTUNREACH
    case EHOSTUNREACH: return "host is unreachable";
#endif
#ifdef EIDRM
    case EIDRM: return "identifier removed";
#endif
#ifdef EINIT
    case EINIT: return "initialization error";
#endif
#ifdef EINPROGRESS
    case EINPROGRESS: return "operation now in progress";
#endif
#ifdef EINTR
    case EINTR: return "interrupted system call";
#endif
#ifdef EINVAL
    case EINVAL: return "invalid argument";
#endif
#ifdef EIO
    case EIO: return "I/O error";
#endif
#ifdef EISCONN
    case EISCONN: return "socket is already connected";
#endif
#ifdef EISDIR
    case EISDIR: return "illegal operation on a directory";
#endif
#ifdef EISNAME
    case EISNAM: return "is a name file";
#endif
#ifdef ELBIN
    case ELBIN: return "ELBIN";
#endif
#ifdef EL2HLT
    case EL2HLT: return "level 2 halted";
#endif
#ifdef EL2NSYNC
    case EL2NSYNC: return "level 2 not synchronized";
#endif
#ifdef EL3HLT
    case EL3HLT: return "level 3 halted";
#endif
#ifdef EL3RST
    case EL3RST: return "level 3 reset";
#endif
#ifdef ELIBACC
    case ELIBACC: return "can not access a needed shared library";
#endif
#ifdef ELIBBAD
    case ELIBBAD: return "accessing a corrupted shared library";
#endif
#ifdef ELIBEXEC
    case ELIBEXEC: return "can not exec a shared library directly";
#endif
#ifdef ELIBMAX
    case ELIBMAX: return "attempting to link in more shared libraries than system limit";
#endif
#ifdef ELIBSCN
    case ELIBSCN: return ".lib section in a.out corrupted";
#endif
#ifdef ELNRNG
    case ELNRNG: return "link number out of range";
#endif
#ifdef ELOOP
    case ELOOP: return "too many levels of symbolic links";
#endif
#ifdef EMFILE
    case EMFILE: return "too many open files";
#endif
#ifdef EMLINK
    case EMLINK: return "too many links";
#endif
#ifdef EMSGSIZE
    case EMSGSIZE: return "message too long";
#endif
#ifdef EMULTIHOP
    case EMULTIHOP: return "multihop attempted";
#endif
#ifdef ENAMETOOLONG
    case ENAMETOOLONG: return "file name too long";
#endif
#ifdef ENAVAIL
    case ENAVAIL: return "not available";
#endif
#ifdef ENET
    case ENET: return "ENET";
#endif
#ifdef ENETDOWN
    case ENETDOWN: return "network is down";
#endif
#ifdef ENETRESET
    case ENETRESET: return "network dropped connection on reset";
#endif
#ifdef ENETUNREACH
    case ENETUNREACH: return "network is unreachable";
#endif
#ifdef ENFILE
    case ENFILE: return "file table overflow";
#endif
#ifdef ENOANO
    case ENOANO: return "anode table overflow";
#endif
#if defined(ENOBUFS) && (!defined(ENOSR) || (ENOBUFS != ENOSR))
    case ENOBUFS: return "no buffer space available";
#endif
#ifdef ENOCSI
    case ENOCSI: return "no CSI structure available";
#endif
#ifdef ENODATA
    case ENODATA: return "no data available";
#endif
#ifdef ENODEV
    case ENODEV: return "no such device";
#endif
#ifdef ENOENT
    case ENOENT: return "no such file or directory";
#endif
#ifdef ENOEXEC
    case ENOEXEC: return "exec format error";
#endif
#ifdef ENOLCK
    case ENOLCK: return "no locks available";
#endif
#ifdef ENOLINK
    case ENOLINK: return "link has be severed";
#endif
#ifdef ENOMEM
    case ENOMEM: return "not enough memory";
#endif
#ifdef ENOMSG
    case ENOMSG: return "no message of desired type";
#endif
#ifdef ENONET
    case ENONET: return "machine is not on the network";
#endif
#ifdef ENOPKG
    case ENOPKG: return "package not installed";
#endif
#ifdef ENOPROTOOPT
    case ENOPROTOOPT: return "bad proocol option";
#endif
#ifdef ENOSPC
    case ENOSPC: return "no space left on device";
#endif
#ifdef ENOSR
    case ENOSR: return "out of stream resources";
#endif
#ifdef ENOSTR
    case ENOSTR: return "not a stream device";
#endif
#ifdef ENOSYM
    case ENOSYM: return "unresolved symbol name";
#endif
#ifdef ENOSYS
    case ENOSYS: return "function not implemented";
#endif
#ifdef ENOTBLK
    case ENOTBLK: return "block device required";
#endif
#ifdef ENOTCONN
    case ENOTCONN: return "socket is not connected";
#endif
#ifdef ENOTDIR
    case ENOTDIR: return "not a directory";
#endif
#ifdef ENOTEMPTY
    case ENOTEMPTY: return "directory not empty";
#endif
#ifdef ENOTNAM
    case ENOTNAM: return "not a name file";
#endif
#ifdef ENOTSOCK
    case ENOTSOCK: return "socket operation on non-socket";
#endif
#ifdef ENOTTY
    case ENOTTY: return "inappropriate device for ioctl";
#endif
#ifdef ENOTUNIQ
    case ENOTUNIQ: return "name not unique on network";
#endif
#ifdef ENXIO
    case ENXIO: return "no such device or address";
#endif
#ifdef EOPNOTSUPP
    case EOPNOTSUPP: return "operation not supported on socket";
#endif
#ifdef EPERM
    case EPERM: return "not owner";
#endif
#ifdef EPFNOSUPPORT
    case EPFNOSUPPORT: return "protocol family not supported";
#endif
#ifdef EPIPE
    case EPIPE: return "broken pipe";
#endif
#ifdef EPROCLIM
    case EPROCLIM: return "too many processes";
#endif
#ifdef EPROCUNAVAIL
    case EPROCUNAVAIL: return "bad procedure for program";
#endif
#ifdef EPROGMISMATCH
    case EPROGMISMATCH: return "program version wrong";
#endif
#ifdef EPROGUNAVAIL
    case EPROGUNAVAIL: return "RPC program not available";
#endif
#ifdef EPROTO
    case EPROTO: return "protocol error";
#endif
#ifdef EPROTONOSUPPORT
    case EPROTONOSUPPORT: return "protocol not suppored";
#endif
#ifdef EPROTOTYPE
    case EPROTOTYPE: return "protocol wrong type for socket";
#endif
#ifdef ERANGE
    case ERANGE: return "math result unrepresentable";
#endif
#if defined(EREFUSED) && (!defined(ECONNREFUSED) || (EREFUSED != ECONNREFUSED))
    case EREFUSED: return "EREFUSED";
#endif
#ifdef EREMCHG
    case EREMCHG: return "remote address changed";
#endif
#ifdef EREMDEV
    case EREMDEV: return "remote device";
#endif
#ifdef EREMOTE
    case EREMOTE: return "pathname hit remote file system";
#endif
#ifdef EREMOTEIO
    case EREMOTEIO: return "remote i/o error";
#endif
#ifdef EREMOTERELEASE
    case EREMOTERELEASE: return "EREMOTERELEASE";
#endif
#ifdef EROFS
    case EROFS: return "read-only file system";
#endif
#ifdef ERPCMISMATCH
    case ERPCMISMATCH: return "RPC version is wrong";
#endif
#ifdef ERREMOTE
    case ERREMOTE: return "object is remote";
#endif
#ifdef ESHUTDOWN
    case ESHUTDOWN: return "can't send afer socket shutdown";
#endif
#ifdef ESOCKTNOSUPPORT
    case ESOCKTNOSUPPORT: return "socket type not supported";
#endif
#ifdef ESPIPE
    case ESPIPE: return "invalid seek";
#endif
#ifdef ESRCH
    case ESRCH: return "no such process";
#endif
#ifdef ESRMNT
    case ESRMNT: return "srmount error";
#endif
#ifdef ESTALE
    case ESTALE: return "stale remote file handle";
#endif
#ifdef ESUCCESS
    case ESUCCESS: return "Error 0";
#endif
#ifdef ETIME
    case ETIME: return "timer expired";
#endif
#ifdef ETIMEDOUT
    case ETIMEDOUT: return "connection timed out";
#endif
#ifdef ETOOMANYREFS
    case ETOOMANYREFS: return "too many references: can't splice";
#endif
#ifdef ETXTBSY
    case ETXTBSY: return "text file or pseudo-device busy";
#endif
#ifdef EUCLEAN
    case EUCLEAN: return "structure needs cleaning";
#endif
#ifdef EUNATCH
    case EUNATCH: return "protocol driver not attached";
#endif
#ifdef EUSERS
    case EUSERS: return "too many users";
#endif
#ifdef EVERSION
    case EVERSION: return "version mismatch";
#endif
#if defined(EWOULDBLOCK) && (!defined(EAGAIN) || (EWOULDBLOCK != EAGAIN))
    case EWOULDBLOCK: return "operation would block";
#endif
#ifdef EXDEV
    case EXDEV: return "cross-domain link";
#endif
#ifdef EXFULL
    case EXFULL: return "message tables full";
#endif
    }
#else /* NO_SYS_ERRLIST */
  extern int sys_nerr;
  extern char *sys_errlist[];

  if ((errnum > 0) && (errnum <= sys_nerr))
    return sys_errlist [errnum];
#endif /* NO_SYS_ERRLIST */

  msg = g_static_private_get (&msg_private);
  if (!msg)
    {
      msg = g_new (gchar, 64);
      g_static_private_set (&msg_private, msg, g_free);
    }

  sprintf (msg, "unknown error (%d)", errnum);

  return msg;
}

gchar*
g_strsignal (gint signum)
{
  static GStaticPrivate msg_private = G_STATIC_PRIVATE_INIT;
  char *msg;

#ifdef HAVE_STRSIGNAL
  return strsignal (signum);
#elif NO_SYS_SIGLIST
  switch (signum)
    {
#ifdef SIGHUP
    case SIGHUP: return "Hangup";
#endif
#ifdef SIGINT
    case SIGINT: return "Interrupt";
#endif
#ifdef SIGQUIT
    case SIGQUIT: return "Quit";
#endif
#ifdef SIGILL
    case SIGILL: return "Illegal instruction";
#endif
#ifdef SIGTRAP
    case SIGTRAP: return "Trace/breakpoint trap";
#endif
#ifdef SIGABRT
    case SIGABRT: return "IOT trap/Abort";
#endif
#ifdef SIGBUS
    case SIGBUS: return "Bus error";
#endif
#ifdef SIGFPE
    case SIGFPE: return "Floating point exception";
#endif
#ifdef SIGKILL
    case SIGKILL: return "Killed";
#endif
#ifdef SIGUSR1
    case SIGUSR1: return "User defined signal 1";
#endif
#ifdef SIGSEGV
    case SIGSEGV: return "Segmentation fault";
#endif
#ifdef SIGUSR2
    case SIGUSR2: return "User defined signal 2";
#endif
#ifdef SIGPIPE
    case SIGPIPE: return "Broken pipe";
#endif
#ifdef SIGALRM
    case SIGALRM: return "Alarm clock";
#endif
#ifdef SIGTERM
    case SIGTERM: return "Terminated";
#endif
#ifdef SIGSTKFLT
    case SIGSTKFLT: return "Stack fault";
#endif
#ifdef SIGCHLD
    case SIGCHLD: return "Child exited";
#endif
#ifdef SIGCONT
    case SIGCONT: return "Continued";
#endif
#ifdef SIGSTOP
    case SIGSTOP: return "Stopped (signal)";
#endif
#ifdef SIGTSTP
    case SIGTSTP: return "Stopped";
#endif
#ifdef SIGTTIN
    case SIGTTIN: return "Stopped (tty input)";
#endif
#ifdef SIGTTOU
    case SIGTTOU: return "Stopped (tty output)";
#endif
#ifdef SIGURG
    case SIGURG: return "Urgent condition";
#endif
#ifdef SIGXCPU
    case SIGXCPU: return "CPU time limit exceeded";
#endif
#ifdef SIGXFSZ
    case SIGXFSZ: return "File size limit exceeded";
#endif
#ifdef SIGVTALRM
    case SIGVTALRM: return "Virtual time alarm";
#endif
#ifdef SIGPROF
    case SIGPROF: return "Profile signal";
#endif
#ifdef SIGWINCH
    case SIGWINCH: return "Window size changed";
#endif
#ifdef SIGIO
    case SIGIO: return "Possible I/O";
#endif
#ifdef SIGPWR
    case SIGPWR: return "Power failure";
#endif
#ifdef SIGUNUSED
    case SIGUNUSED: return "Unused signal";
#endif
    }
#else /* NO_SYS_SIGLIST */

#ifdef NO_SYS_SIGLIST_DECL
  extern char *sys_siglist[];	/*(see Tue Jan 19 00:44:24 1999 in changelog)*/
#endif

  return (char*) /* this function should return const --josh */ sys_siglist [signum];
#endif /* NO_SYS_SIGLIST */

  msg = g_static_private_get (&msg_private);
  if (!msg)
    {
      msg = g_new (gchar, 64);
      g_static_private_set (&msg_private, msg, g_free);
    }

  sprintf (msg, "unknown signal (%d)", signum);

  return msg;
}

typedef struct
{
  guint min_width;
  guint precision;
  gboolean alternate_format, zero_padding, adjust_left, locale_grouping;
  gboolean add_space, add_sign, possible_sign, seen_precision;
  gboolean mod_half, mod_long, mod_extra_long;
} PrintfArgSpec;

guint
g_printf_string_upper_bound (const gchar* format,
			     va_list      args)
{
  static const gboolean honour_longs = SIZEOF_LONG > 4 || SIZEOF_VOID_P > 4;
  guint len = 1;

  if (!format)
    return len;

  while (*format)
    {
      register gchar c = *format++;

      if (c != '%')
        len += 1;
      else /* (c == '%') */
        {
          PrintfArgSpec spec = { 0, };
          gboolean seen_l = FALSE, conv_done = FALSE;
          guint conv_len = 0;
          const gchar *spec_start = format;

          do
            {
              c = *format++;
              switch (c)
                {
                  GDoubleIEEE754 u_double;
                  guint v_uint;
                  gint v_int;
                  const gchar *v_string;

                  /* beware of positional parameters
                   */
                case '$':
                  g_warning ("%s(): unable to handle positional parameters (%%n$)", 
                             G_GNUC_PRETTY_FUNCTION );
                  len += 1024; /* try adding some safety padding */
                  break;

                  /* parse flags
                   */
                case '#':
                  spec.alternate_format = TRUE;
                  break;
                case '0':
                  spec.zero_padding = TRUE;
                  break;
                case '-':
                  spec.adjust_left = TRUE;
                  break;
                case ' ':
                  spec.add_space = TRUE;
                  break;
                case '+':
                  spec.add_sign = TRUE;
                  break;
                case '\'':
                  spec.locale_grouping = TRUE;
                  break;

                  /* parse output size specifications
                   */
                case '.':
                  spec.seen_precision = TRUE;
                  break;
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                  v_uint = c - '0';
                  c = *format;
                  while (c >= '0' && c <= '9')
                    {
                      format++;
                      v_uint = v_uint * 10 + c - '0';
                      c = *format;
                    }
                  if (spec.seen_precision)
                    spec.precision = MAX (spec.precision, v_uint);
                  else
                    spec.min_width = MAX (spec.min_width, v_uint);
                  break;
                case '*':
                  v_int = va_arg (args, int);
                  if (spec.seen_precision)
                    {
                      /* forget about negative precision */
                      if (v_int >= 0)
                        spec.precision = MAX (spec.precision, v_int);
                    }
                  else
                    {
                      if (v_int < 0)
                        {
                          v_int = - v_int;
                          spec.adjust_left = TRUE;
                        }
                      spec.min_width = MAX (spec.min_width, v_int);
                    }
                  break;

                  /* parse type modifiers
                   */
                case 'h':
                  spec.mod_half = TRUE;
                  break;
                case 'l':
                  if (!seen_l)
                    {
                      spec.mod_long = TRUE;
                      seen_l = TRUE;
                      break;
                    }
                  /* else, fall through */
                case 'L':
                case 'q':
                  spec.mod_long = TRUE;
                  spec.mod_extra_long = TRUE;
                  break;
                case 'z':
                case 'Z':
#if GLIB_SIZEOF_SIZE_T > 4
                  spec.mod_long = TRUE;
                  spec.mod_extra_long = TRUE;
#endif /* GLIB_SIZEOF_SIZE_T > 4 */
                  break;
                case 't':
#if GLIB_SIZEOF_PTRDIFF_T > 4
                  spec.mod_long = TRUE;
                  spec.mod_extra_long = TRUE;
#endif /* GLIB_SIZEOF_PTRDIFF_T > 4 */
                  break;
                case 'j':
#if GLIB_SIZEOF_INTMAX_T > 4
                  spec.mod_long = TRUE;
                  spec.mod_extra_long = TRUE;
#endif /* GLIB_SIZEOF_INTMAX_T > 4 */
                  break;

                  /* parse output conversions
                   */
                case '%':
                  conv_len += 1;
                  break;
                case 'O':
                case 'D':
                case 'I':
                case 'U':
                  /* some C libraries feature long variants for these as well? */
                  spec.mod_long = TRUE;
                  /* fall through */
                case 'o':
                  conv_len += 2;
                  /* fall through */
                case 'd':
                case 'i':
                  conv_len += 1; /* sign */
                  /* fall through */
                case 'u':
                  conv_len += 4;
                  /* fall through */
                case 'x':
                case 'X':
                  spec.possible_sign = TRUE;
                  conv_len += 10;
                  if (spec.mod_long && honour_longs)
                    conv_len *= 2;
                  if (spec.mod_extra_long)
                    conv_len *= 2;
                  if (spec.mod_extra_long)
                    {
#ifdef G_HAVE_GINT64
                      (void) va_arg (args, gint64);
#else /* !G_HAVE_GINT64 */
                      (void) va_arg (args, long);
#endif /* !G_HAVE_GINT64 */
                    }
                  else if (spec.mod_long)
                    (void) va_arg (args, long);
                  else
                    (void) va_arg (args, int);
                  break;
                case 'A':
                case 'a':
                  /*          0x */
                  conv_len += 2;
                  /* fall through */
                case 'g':
                case 'G':
                case 'e':
                case 'E':
                case 'f':
                  spec.possible_sign = TRUE;
                  /*          n   .   dddddddddddddddddddddddd   E   +-  eeee */
                  conv_len += 1 + 1 + MAX (24, spec.precision) + 1 + 1 + 4;
                  if (spec.mod_extra_long)
                    g_warning ("%s(): unable to handle long double, collecting double only", 
                               G_GNUC_PRETTY_FUNCTION);
#ifdef HAVE_LONG_DOUBLE
#error need to implement special handling for long double
#endif
                  u_double.v_double = va_arg (args, double);
                  /* %f can expand up to all significant digits before '.' (308) */
                  if (c == 'f' &&
                      u_double.mpn.biased_exponent > 0 && u_double.mpn.biased_exponent < 2047)
                    {
                      gint exp = u_double.mpn.biased_exponent;

                      exp -= G_IEEE754_DOUBLE_BIAS;
                      exp = abs(exp * G_LOG_2_BASE_10) + 1;
                      conv_len += exp;
                    }
                  /* some printf() implementations require extra padding for rounding */
                  conv_len += 2;
                  /* we can't really handle locale specific grouping here */
                  if (spec.locale_grouping)
                    conv_len *= 2;
                  break;
                case 'C':
                  spec.mod_long = TRUE;
                  /* fall through */
                case 'c':
                  conv_len += spec.mod_long ? MB_LEN_MAX : 1;
                  (void) va_arg (args, int);
                  break;
                case 'S':
                  spec.mod_long = TRUE;
                  /* fall through */
                case 's':
                  v_string = va_arg (args, char*);
                  if (!v_string)
                    conv_len += 8; /* hold "(null)" */
                  else if (spec.seen_precision)
                    conv_len += spec.precision;
                  else
                    conv_len += strlen (v_string);
                  conv_done = TRUE;
                  if (spec.mod_long)
                    {
                      g_warning ("%s(): unable to handle wide char strings", 
                                 G_GNUC_PRETTY_FUNCTION);
                      len += 1024; /* try adding some safety padding */
                    }
                  break;
                case 'P': /* do we actually need this? */
                  /* fall through */
                case 'p':
                  spec.alternate_format = TRUE;
                  conv_len += 10;
                  if (honour_longs)
                    conv_len *= 2;
                  /* fall through */
                case 'n':
                  conv_done = TRUE;
                  (void) va_arg (args, void*);
                  break;
                case 'm':
                  /* there's not much we can do to be clever */
                  v_string = g_strerror (errno);
                  v_uint = v_string ? strlen (v_string) : 0;
                  conv_len += MAX (256, v_uint);
                  break;

                  /* handle invalid cases
                   */
                case '\000':
                  /* no conversion specification, bad bad */
                  conv_len += format - spec_start;
                  break;
                default:
                  g_warning ("%s(): unable to handle `%c' while parsing format",
                             G_GNUC_PRETTY_FUNCTION,
                             c);
                  break;
                }
              conv_done |= conv_len > 0;
            }
          while (!conv_done);
          /* handle width specifications */
          conv_len = MAX (conv_len, MAX (spec.precision, spec.min_width));
          /* handle flags */
          conv_len += spec.alternate_format ? 2 : 0;
          conv_len += (spec.add_space || spec.add_sign || spec.possible_sign);
          /* finally done */
          len += conv_len;
        } /* else (c == '%') */
    } /* while (*format) */

  return len;
}

void
g_strdown (gchar *string)
{
  register guchar *s;

  g_return_if_fail (string != NULL);

  s = string;

  while (*s)
    {
      *s = tolower (*s);
      s++;
    }
}

void
g_strup (gchar *string)
{
  register guchar *s;

  g_return_if_fail (string != NULL);

  s = string;

  while (*s)
    {
      *s = toupper (*s);
      s++;
    }
}

void
g_strreverse (gchar *string)
{
  g_return_if_fail (string != NULL);

  if (*string)
    {
      register gchar *h, *t;

      h = string;
      t = string + strlen (string) - 1;

      while (h < t)
	{
	  register gchar c;

	  c = *h;
	  *h = *t;
	  h++;
	  *t = c;
	  t--;
	}
    }
}

gint
g_strcasecmp (const gchar *s1,
	      const gchar *s2)
{
#ifdef HAVE_STRCASECMP
  g_return_val_if_fail (s1 != NULL, 0);
  g_return_val_if_fail (s2 != NULL, 0);

  return strcasecmp (s1, s2);
#else
  gint c1, c2;

  g_return_val_if_fail (s1 != NULL, 0);
  g_return_val_if_fail (s2 != NULL, 0);

  while (*s1 && *s2)
    {
      /* According to A. Cox, some platforms have islower's that
       * don't work right on non-uppercase
       */
      c1 = isupper ((guchar)*s1) ? tolower ((guchar)*s1) : *s1;
      c2 = isupper ((guchar)*s2) ? tolower ((guchar)*s2) : *s2;
      if (c1 != c2)
	return (c1 - c2);
      s1++; s2++;
    }

  return (((gint)(guchar) *s1) - ((gint)(guchar) *s2));
#endif
}

gint
g_strncasecmp (const gchar *s1,
	       const gchar *s2,
	       guint n)
{
#ifdef HAVE_STRNCASECMP
  return strncasecmp (s1, s2, n);
#else
  gint c1, c2;

  g_return_val_if_fail (s1 != NULL, 0);
  g_return_val_if_fail (s2 != NULL, 0);

  while (n && *s1 && *s2)
    {
      n -= 1;
      /* According to A. Cox, some platforms have islower's that
       * don't work right on non-uppercase
       */
      c1 = isupper ((guchar)*s1) ? tolower ((guchar)*s1) : *s1;
      c2 = isupper ((guchar)*s2) ? tolower ((guchar)*s2) : *s2;
      if (c1 != c2)
	return (c1 - c2);
      s1++; s2++;
    }

  if (n)
    return (((gint) (guchar) *s1) - ((gint) (guchar) *s2));
  else
    return 0;
#endif
}

gchar*
g_strdelimit (gchar	  *string,
	      const gchar *delimiters,
	      gchar	   new_delim)
{
  register gchar *c;

  g_return_val_if_fail (string != NULL, NULL);

  if (!delimiters)
    delimiters = G_STR_DELIMITERS;

  for (c = string; *c; c++)
    {
      if (strchr (delimiters, *c))
	*c = new_delim;
    }

  return string;
}

gchar*
g_strescape (gchar *string)
{
  gchar *q;
  gchar *escaped;
  guint backslashes = 0;
  gchar *p = string;

  g_return_val_if_fail (string != NULL, NULL);

  while (*p != '\000')
    backslashes += (*p++ == '\\');

  if (!backslashes)
    return g_strdup (string);

  escaped = g_new (gchar, strlen (string) + backslashes + 1);

  p = string;
  q = escaped;

  while (*p != '\000')
    {
      if (*p == '\\')
	*q++ = '\\';
      *q++ = *p++;
    }
  *q = '\000';

  return escaped;
}

/* blame Elliot for these next five routines */
gchar*
g_strchug (gchar *string)
{
  guchar *start;

  g_return_val_if_fail (string != NULL, NULL);

  for (start = string; *start && isspace (*start); start++)
    ;

  g_memmove(string, start, strlen(start) + 1);

  return string;
}

gchar*
g_strchomp (gchar *string)
{
  gchar *s;

  g_return_val_if_fail (string != NULL, NULL);

  if (!*string)
    return string;

  for (s = string + strlen (string) - 1; s >= string && isspace ((guchar)*s); 
       s--)
    *s = '\0';

  return string;
}

gchar**
g_strsplit (const gchar *string,
	    const gchar *delimiter,
	    gint         max_tokens)
{
  GSList *string_list = NULL, *slist;
  gchar **str_array, *s;
  guint i, n = 1;

  g_return_val_if_fail (string != NULL, NULL);
  g_return_val_if_fail (delimiter != NULL, NULL);

  if (max_tokens < 1)
    max_tokens = G_MAXINT;

  s = strstr (string, delimiter);
  if (s)
    {
      guint delimiter_len = strlen (delimiter);

      do
	{
	  guint len;
	  gchar *new_string;

	  len = s - string;
	  new_string = g_new (gchar, len + 1);
	  strncpy (new_string, string, len);
	  new_string[len] = 0;
	  string_list = g_slist_prepend (string_list, new_string);
	  n++;
	  string = s + delimiter_len;
	  s = strstr (string, delimiter);
	}
      while (--max_tokens && s);
    }
  if (*string)
    {
      n++;
      string_list = g_slist_prepend (string_list, g_strdup (string));
    }

  str_array = g_new (gchar*, n);

  i = n - 1;

  str_array[i--] = NULL;
  for (slist = string_list; slist; slist = slist->next)
    str_array[i--] = slist->data;

  g_slist_free (string_list);

  return str_array;
}

void
g_strfreev (gchar **str_array)
{
  if (str_array)
    {
      int i;

      for(i = 0; str_array[i] != NULL; i++)
	g_free(str_array[i]);

      g_free (str_array);
    }
}

gchar*
g_strjoinv (const gchar  *separator,
	    gchar       **str_array)
{
  gchar *string;

  g_return_val_if_fail (str_array != NULL, NULL);

  if (separator == NULL)
    separator = "";

  if (*str_array)
    {
      guint i, len;
      guint separator_len;

      separator_len = strlen (separator);
      len = 1 + strlen (str_array[0]);
      for(i = 1; str_array[i] != NULL; i++)
	len += separator_len + strlen(str_array[i]);

      string = g_new (gchar, len);
      *string = 0;
      strcat (string, *str_array);
      for (i = 1; str_array[i] != NULL; i++)
	{
	  strcat (string, separator);
	  strcat (string, str_array[i]);
	}
      }
  else
    string = g_strdup ("");

  return string;
}

gchar*
g_strjoin (const gchar  *separator,
	   ...)
{
  gchar *string, *s;
  va_list args;
  guint len;
  guint separator_len;

  if (separator == NULL)
    separator = "";

  separator_len = strlen (separator);

  va_start (args, separator);

  s = va_arg (args, gchar*);

  if (s)
    {
      len = strlen (s);
      
      s = va_arg (args, gchar*);
      while (s)
	{
	  len += separator_len + strlen (s);
	  s = va_arg (args, gchar*);
	}
      va_end (args);
      
      string = g_new (gchar, len + 1);
      *string = 0;
      
      va_start (args, separator);
      
      s = va_arg (args, gchar*);
      strcat (string, s);
      
      s = va_arg (args, gchar*);
      while (s)
	{
	  strcat (string, separator);
	  strcat (string, s);
	  s = va_arg (args, gchar*);
	}
    }
  else
    string = g_strdup ("");
  
  va_end (args);

  return string;
}
