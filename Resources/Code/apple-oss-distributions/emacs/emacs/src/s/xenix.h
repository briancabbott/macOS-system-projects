/* Definitions file for GNU Emacs running SCO Xenix 386 Release 2.2
   Copyright (C) 1988, 1999, 2001, 2002, 2003, 2004,
                 2005, 2006, 2007  Free Software Foundation, Inc.

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

/*
 *	Define symbols to identify the version of Unix this is.
 *	Define all the symbols that apply correctly.
 */

/* #define UNIPLUS */
#define XENIX
#define USG5
#define USG
/* #define HPUX */
/* #define UMAX */
/* #define BSD4_1 */
/* #define BSD4_2 */
/* #define BSD4_3 */
/* #define BSD_SYSTEM */
/* #define VMS */

/* SYSTEM_TYPE should indicate the kind of system you are using.
 It sets the Lisp variable system-type.  */

#define SYSTEM_TYPE "xenix"

/* NOMULTIPLEJOBS should be defined if your system's shell
 does not have "job control" (the ability to stop a program,
 run some other program, then continue the first one).  */

#define NOMULTIPLEJOBS

/* Emacs can read input using SIGIO and buffering characters itself,
   or using CBREAK mode and making C-g cause SIGINT.
   The choice is controlled by the variable interrupt_input.
   Define INTERRUPT_INPUT to make interrupt_input = 1 the default (use SIGIO)

   SIGIO can be used only on systems that implement it (4.2 and 4.3).
   CBREAK mode has two disadvantages:
     1) At least in 4.2, it is impossible to handle the Meta key properly.
        I hear that in system V this problem does not exist.
     2) Control-G causes output to be discarded.
        I do not know whether this can be fixed in system V.

   Another method of doing input is planned but not implemented.
   It would have Emacs fork off a separate process
   to read the input and send it to the true Emacs process
   through a pipe.
*/

/* #define INTERRUPT_INPUT */

/* Letter to use in finding device name of first pty,
  if system supports pty's.  'p' means it is /dev/ptyp0  */

/* #define FIRST_PTY_LETTER 'p' */

/*
 *	Define HAVE_TERMIO if the system provides sysV-style ioctls
 *	for terminal control.
 */

#define HAVE_TERMIO

/*
 *	Define HAVE_PTYS if the system supports pty devices.
 */

/* #define HAVE_PTYS */

/* Define HAVE_SOCKETS if system supports 4.2-compatible sockets.  */

/* #define HAVE_SOCKETS */

/*
 *	Define NONSYSTEM_DIR_LIBRARY to make Emacs emulate
 *      The 4.2 opendir, etc., library functions.
 */

#define NONSYSTEM_DIR_LIBRARY

/* Define this symbol if your system has the functions bcopy, etc. */

/* #define BSTRING */

/* subprocesses should be defined if you want to
 have code for asynchronous subprocesses
 (as used in M-x compile and M-x shell).
 This is supposed to work now on system V release 2.  */

#define subprocesses

/* If your system uses COFF (Common Object File Format) then define the
   preprocessor symbol "COFF". */

/* #define COFF */

/* Xenix requires completely different unexec code
   which lives in a separate file.  Specify the file name.  */

#define UNEXEC unexenix.o

/* define MAIL_USE_FLOCK if the mailer uses flock
   to interlock access to /usr/spool/mail/$USER.
   The alternative is that a lock file named
   /usr/spool/mail/$USER.lock.  */

#define MAIL_USE_FLOCK

/* Define CLASH_DETECTION if you want lock files to be written
   so that Emacs can tell instantly when you try to modify
   a file that someone else has modified in his Emacs.  */

/* The way this is implemented requires long filenames... */
/* #define CLASH_DETECTION */

/* Define SHORTNAMES if the C compiler can distinguish only
   short names.  It means that the stuff in ../shortnames
   must be run to convert the long names to short ones.  */

/* #define SHORTNAMES */

/* Compensate for one incompatibility between Xenix and V.0.  */
#define n_zeroes n_name[0]

/* The file containing the kernel's symbol table is called /xenix.  */

#define KERNEL_FILE "/xenix"

/* The symbol in the kernel where the load average is found
   is named avenrun.  */

#define LDAV_SYMBOL "_avenrun"

/* Special hacks needed to make Emacs run on this system.  */

/*
 *	Make the sigsetmask function go away.  Don't know what the
 *	ramifications of this are, but doesn't seem possible to
 *	emulate it properly anyway at this point.
 */

#define sigsetmask(mask)	/* Null expansion */

/* setjmp and longjmp can safely replace _setjmp and _longjmp,
   but they will run slower.  */

#define _setjmp setjmp
#define _longjmp longjmp

/* On USG systems these have different names */

#define index strchr
#define rindex strrchr

/* Compiler bug bites on many systems when default ADDR_CORRECT is used.  */

#define ADDR_CORRECT(x) (x)

/* Prevent -lg from being used for debugging.  Not implemented?  */

#define LIBS_DEBUG

/* Switches for linking temacs.  */

#define LD_SWITCH_SYSTEM -i

/* Xenix implements sysV style IPC.  */

#define HAVE_SYSVIPC

/* Use terminfo instead of termcap.  */

/* Tell Emacs to use Terminfo.  */

#define TERMINFO

/* Tell Xenix curses to BE Terminfo.  */
#define M_TERMINFO

/* Control program name for etc/fakemail to run.  */

#ifdef SMAIL
#define MAIL_PROGRAM_NAME "/usr/bin/smail -q0"
#else
#define MAIL_PROGRAM_NAME "/usr/lib/mail/execmail"
#endif

/* Some variants have TIOCGETC, but the structures to go with it
   are not declared.  */

#define BROKEN_TIOCGETC

/* arch-tag: 71d3985d-4e53-4572-8276-5dce26bbd076
   (do not change this comment) */
