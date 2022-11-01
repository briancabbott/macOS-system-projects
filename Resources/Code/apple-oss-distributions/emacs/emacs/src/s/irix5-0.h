/* Definitions file for GNU Emacs running on Silicon Graphics Irix system 5.0.

   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004, 2005, 2006,
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


#include "usg5-4.h"

#define IRIX5

#undef sigsetmask  /* use sys_sigsetmask */
#undef _longjmp /* use system versions, not conservative aliases */
#undef _setjmp

#define SETPGRP_RELEASES_CTTY

#ifdef LIBS_SYSTEM
#undef LIBS_SYSTEM
#endif

#ifdef LIB_STANDARD
#undef LIB_STANDARD
#endif

#ifdef SYSTEM_TYPE
#undef SYSTEM_TYPE
#endif
#define SYSTEM_TYPE "irix"

#ifdef SETUP_SLAVE_PTY
#undef SETUP_SLAVE_PTY
#endif

/* thomas@mathematik.uni-bremen.de says this is needed.  */
/* Make process_send_signal work by "typing" a signal character on the pty.  */
#define SIGNALS_VIA_CHARACTERS

/* SGI has all the fancy wait stuff, but we can't include sys/wait.h
   because it defines BIG_ENDIAN and LITTLE_ENDIAN (ugh!.)  Instead
   we'll just define WNOHANG right here.
   (An implicit decl is good enough for wait3.)  */

/* #define WNOHANG		0x1 */

/* No need to use sprintf to get the tty name--we get that from _getpty.  */
#ifdef PTY_TTY_NAME_SPRINTF
#undef PTY_TTY_NAME_SPRINTF
#endif
#define PTY_TTY_NAME_SPRINTF
/* No need to get the pty name at all.  */
#ifdef PTY_NAME_SPRINTF
#undef PTY_NAME_SPRINTF
#endif
#define PTY_NAME_SPRINTF
#ifdef emacs
char *_getpty();
#endif
/* We need only try once to open a pty.  */
#define PTY_ITERATION
/* Here is how to do it.  */
#define PTY_OPEN					    \
{							    \
  struct sigaction ocstat, cstat;			    \
  char * name;						    \
  sigemptyset(&cstat.sa_mask);				    \
  cstat.sa_handler = SIG_DFL;				    \
  cstat.sa_flags = 0;					    \
  sigaction(SIGCLD, &cstat, &ocstat);			    \
  name = _getpty (&fd, O_RDWR | O_NDELAY, 0600, 0);	    \
  sigaction(SIGCLD, &ocstat, (struct sigaction *)0);	    \
  if (name == 0)					    \
    return -1;						    \
  if (fd < 0)						    \
    return -1;						    \
  if (fstat (fd, &stb) < 0)				    \
    return -1;						    \
  strcpy (pty_name, name);				    \
}

/* Since we use POSIX constructs in PTY_OPEN, we must force POSIX
   throughout. */
#define POSIX_SIGNALS

/* Info from simon@lia.di.epfl.ch (Simon Leinen) suggests this is needed.  */
#define GETPGRP_NO_ARG

/* Ulimit(UL_GMEMLIM) is busted...  */
#define ULIMIT_BREAK_VALUE 0x14000000

/* Tell process_send_signal to use VSUSP instead of VSWTCH.  */
#define PREFER_VSUSP

/* define MAIL_USE_FLOCK if the mailer uses flock
   to interlock access to /usr/spool/mail/$USER.
   The alternative is that a lock file named
   /usr/spool/mail/$USER.lock.  */

#define MAIL_USE_FLOCK

/* use K&R C */
#if 0
#ifndef __GNUC__
#define C_SWITCH_SYSTEM -cckr
#endif
#endif

/* -g used not to work on Irix unless you used gas, and since gcc
   warns if you use it, turn off the warning.  */
/* -g does now work, at least on recent Irix 6 versions with gcc 2.95;
    I'm not sure about Irix 5 -- fx  */
#ifdef __GNUC__
#define C_DEBUG_SWITCH
#endif

/* Prevent the variable ospeed from being defined by -lcurses
   because it defines it with too few bytes.  */
#define ospeed ospeed_

#define NARROWPROTO 1

#define USE_MMAP_FOR_BUFFERS 1

/* arch-tag: ad0660e0-acf8-46ae-b866-4f3df5b1101b
   (do not change this comment) */
