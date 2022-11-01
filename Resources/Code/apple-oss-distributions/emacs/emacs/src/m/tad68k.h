/* Machine-dependent configuration for GNU Emacs for Tadpole 68k machines
   Copyright (C) 1986, 2001, 2002, 2003, 2004, 2005,
                 2006, 2007  Free Software Foundation, Inc.

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


/* The following line tells the configuration script what sort of
   operating system this machine is likely to run.
   USUAL-OPSYS="usg5-3"  */

/* Define WORDS_BIG_ENDIAN iff lowest-numbered byte in a word
   is the most significant byte.  */

#define WORDS_BIG_ENDIAN

/* Define NO_ARG_ARRAY if you cannot take the address of the first of a
 * group of arguments and treat it as an array of the arguments.  */

/* #define NO_ARG_ARRAY */

/* Define WORD_MACHINE if addresses and such have
 * to be corrected before they can be used as byte counts.  */

/* #define WORD_MACHINE */

/* Now define a symbol for the cpu type, if your compiler
   does not define it automatically */

/* Use type int rather than a union, to represent Lisp_Object */
/* This is desirable for most machines.  */

#define NO_UNION_TYPE

/* Define EXPLICIT_SIGN_EXTEND if XINT must explicitly sign-extend
   the 24-bit bit field into an int.  In other words, if bit fields
   are always unsigned.

   If you use NO_UNION_TYPE, this flag does not matter.  */

/* #define EXPLICIT_SIGN_EXTEND */

/* Data type of load average, as read out of kmem.  */
/* #define LOAD_AVE_TYPE long */

/* Convert that into an integer that is 100 for a load average of 1.0  */

/* #define LOAD_AVE_CVT(x) (int) (((double) (x)) * 100.0 / FSCALE) */

/* Define CANNOT_DUMP on machines where unexec does not work.
   Then the function dump-emacs will not be defined
   and temacs will do (load "loadup") automatically unless told otherwise.  */
/* #define CANNOT_DUMP */

/* Define VIRT_ADDR_VARIES if the virtual addresses of
   pure and impure space as loaded can vary, and even their
   relative order cannot be relied on.

   Otherwise Emacs assumes that text space precedes data space,
   numerically.  */

/* #define VIRT_ADDR_VARIES */  /* Karl Kleinpaste says this isn't needed.  */

/* SysV has alloca in the PW library */

#define LIB_STANDARD -lPW -lc

/* Define NO_REMAP if memory segmentation makes it not work well
   to change the boundary between the text section and data section
   when Emacs is dumped.  If you define this, the preloaded Lisp
   code will not be sharable; but that's better than failing completely.  */

/* #define NO_REMAP */

/* Use Terminfo, not Termcap.  */

#define TERMINFO

/* TPIX extras */

#define TPIX				/* used in various source files */
#define BSTRING				/* we do have the BSTRING functions */
#define CLASH_DETECTION			/* we want to know about clashes */
#undef ADDR_CORRECT			/* don't need this bug fix */
#define fchmod				/* we don't have fchmod() */
#define SECTION_ALIGNMENT (2048-1)	/* 2k boundaries required in unexec */
#define SEGMENT_MASK (128*1024-1)	/* 128k offsets required in unexec */
#define C_DEBUG_SWITCH -O		/* build with -O (TPIX has GCC 1.34) */

#define BROKEN_TIOCGWINSZ		/* Don't try to use TIOCGWINSZ.  */

/* omit next four lines if no TCP installed */

#define select gnu_select		/* avoid select() name clash */
#define HAVE_PTYS			/* we do have PTYs if we have TCP */
#define HAVE_SOCKETS			/* we do have sockets if we have TCP */
#define LIBS_SYSTEM -lsocket		/* get TCP networking functions */

/* arch-tag: e0c09754-b0f2-48da-a8a5-aee3c94838f4
   (do not change this comment) */
