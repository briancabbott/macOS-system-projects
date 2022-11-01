/* s/ file for netbsd system.

   Copyright (C) 2001, 2002, 2003, 2004, 2005, 2006,
                 2007  Free Software Foundation, Inc.

This file is part of GNU Emacs.

GNU Emacs is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Emacs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */


/* Get most of the stuff from bsd4.3 */
#include "bsd4-3.h"

#if defined (__alpha__) && !defined (__ELF__)
#define NO_SHARED_LIBS
#endif

/* For mem-limits.h.  */
#define BSD4_2

#undef KERNEL_FILE
#undef LDAV_SYMBOL
#define HAVE_GETLOADAVG 1

#define HAVE_UNION_WAIT

#define SIGNALS_VIA_CHARACTERS

#define PENDING_OUTPUT_COUNT(FILE) ((FILE)->_p - (FILE)->_bf._base)

/* netbsd uses OXTABS instead of the expected TAB3.  */
#define TABDLY OXTABS
#define TAB3 OXTABS

#define A_TEXT_OFFSET(x) (sizeof (struct exec))
#define A_TEXT_SEEK(hdr) (N_TXTOFF(hdr) + A_TEXT_OFFSET(hdr))

#define HAVE_TERMIOS
#define NO_TERMIO

#define LIBS_DEBUG
/* -lutil is not needed for NetBSD >0.9.  */
/* #define LIBS_SYSTEM -lutil */
#define LIBS_TERMCAP -ltermcap

#define NEED_ERRNO
#define SYSV_SYSTEM_DIR

/* Netbsd has POSIX-style pgrp behavior.  */
#undef BSD_PGRPS

#define GETPGRP_NO_ARG

#if !defined (NO_SHARED_LIBS) && ! defined (__ELF__)
/* These definitions should work for either dynamic or static linking,
   whichever is the default for `cc -nostdlib'.  */
#define HAVE_TEXT_START		/* No need to define `start_of_text'.  */
#define START_FILES pre-crt0.o /usr/lib/crt0.o
#define UNEXEC unexsunos4.o
#define RUN_TIME_REMAP

/* Try to make this work for both 0.9 and >0.9.  */
#ifndef N_TRELOFF
#define N_PAGSIZ(x) __LDPGSZ
#define N_BSSADDR(x) (N_ALIGN(x, N_DATADDR(x)+x.a_data))
#define N_TRELOFF(x) N_RELOFF(x)
#endif
#endif /* not NO_SHARED_LIBS and not ELF */

#if !defined (NO_SHARED_LIBS) && defined (__ELF__)
#define START_FILES pre-crt0.o /usr/lib/crt0.o START_FILES_1 /usr/lib/crtbegin.o
#define UNEXEC unexelf.o
#define LIB_STANDARD -lgcc -lc -lgcc /usr/lib/crtend.o END_FILES_1
#undef LIB_GCC
#define LIB_GCC
#endif

#ifdef HAVE_CRTIN
#define START_FILES_1 /usr/lib/crti.o 
#define END_FILES_1 /usr/lib/crtn.o
#else
#define START_FILES_1
#define END_FILES_1
#endif

#define HAVE_WAIT_HEADER
#define WAIT_USE_INT

#define AMPERSAND_FULL_NAME

#ifdef __ELF__
/* Here is how to find X Windows.  LD_SWITCH_X_SITE_AUX gives an -R option
   says where to find X windows at run time.  We convert it to a -rpath option
   which is what OSF1 uses.  */
#define LD_SWITCH_SYSTEM_tmp `echo LD_SWITCH_X_SITE_AUX | sed -e 's/-R/-Wl,-rpath,/'`
#define LD_SWITCH_SYSTEM LD_SWITCH_SYSTEM_tmp -Wl,-rpath,/usr/pkg/lib -L/usr/pkg/lib -Wl,-rpath,/usr/local/lib -L/usr/local/lib

/* The following is needed to make `configure' find Xpm, Xaw3d and
   image include and library files if using /usr/bin/gcc.  That
   compiler seems to be modified to not find headers in
   /usr/local/include or libs in /usr/local/lib by default.  */

#define C_SWITCH_SYSTEM -I/usr/X11R6/include -I/usr/pkg/include -I/usr/local/include -L/usr/pkg/lib -L/usr/local/lib

/* Link temacs with -z nocombreloc so that unexec works right, whether or
   not -z combreloc is the default.  GNU ld ignores unknown -z KEYWORD
   switches, so this also works with older versions that don't implement
   -z combreloc.  */

#define LD_SWITCH_SYSTEM_TEMACS -Wl,-z,nocombreloc

#endif /* __ELF__ */

/* On post 1.3 releases of NetBSD, gcc -nostdlib also clears
   the library search parth, i.e. it won't search /usr/lib
   for libc and friends. Using -nostartfiles instead avoids
   this problem, and will also work on earlier NetBSD releases */

#define LINKER $(CC) -nostartfiles

#define NARROWPROTO 1

#define DEFAULT_SOUND_DEVICE "/dev/audio"

/* Greg A. Woods <woods@weird.com> says we must include signal.h
   before syssignal.h is included, to work around interface conflicts
   that are handled with CPP __RENAME() macro in signal.h.  */

#ifndef NOT_C_CODE
#include <signal.h>
#endif

/* Don't close pty in process.c to make it as controlling terminal.
   It is already a controlling terminal of subprocess, because we did
   ioctl TIOCSCTTY.  */

#define DONT_REOPEN_PTY

/* Tell that garbage collector that setjmp is known to save all
   registers relevant for conservative garbage collection in the
   jmp_buf.  */

#define GC_SETJMP_WORKS 1

/* Use the GC_MAKE_GCPROS_NOOPS (see lisp.h) method.  */

#define GC_MARK_STACK	GC_MAKE_GCPROS_NOOPS

/* Use sigprocmask and friends instead of sigblock;
   sigblock is considered obsolete on NetBSD.  */

#define POSIX_SIGNALS	1

/* arch-tag: e80f364a-04e9-4faf-93cb-f36a0fe95c81
   (do not change this comment) */
