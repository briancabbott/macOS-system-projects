/* machine description file for AMD x86-64.
   Copyright (C) 2002, 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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
   USUAL-OPSYS="linux"  */

#define BITS_PER_LONG           64
#define BITS_PER_EMACS_INT      64

/* Define WORDS_BIG_ENDIAN iff lowest-numbered byte in a word
   is the most significant byte.  */

#undef WORDS_BIG_ENDIAN

/* Define NO_ARG_ARRAY if you cannot take the address of the first of a
 * group of arguments and treat it as an array of the arguments.  */

#define NO_ARG_ARRAY

/* Define WORD_MACHINE if addresses and such have
 * to be corrected before they can be used as byte counts.  */

/* #define WORD_MACHINE */

/* Now define a symbol for the cpu type, if your compiler
   does not define it automatically:
   Ones defined so far include vax, m68000, ns16000, pyramid,
   orion, tahoe, APOLLO and many others */
/* __x86_64 defined automatically.  */

/* Use type int rather than a union, to represent Lisp_Object */
/* This is desirable for most machines.  */

#define NO_UNION_TYPE

/* Define the type to use.  */
#define EMACS_INT               long
#define EMACS_UINT              unsigned long
#define SPECIAL_EMACS_INT

/* Define EXPLICIT_SIGN_EXTEND if XINT must explicitly sign-extend
   the 24-bit bit field into an int.  In other words, if bit fields
   are always unsigned.

   If you use NO_UNION_TYPE, this flag does not matter.  */

#define EXPLICIT_SIGN_EXTEND

/* Data type of load average, as read out of kmem.  */

#define LOAD_AVE_TYPE long

/* Convert that into an integer that is 100 for a load average of 1.0  */

#define LOAD_AVE_CVT(x) (int) (((double) (x)) * 100.0 / FSCALE)

/* Define CANNOT_DUMP on machines where unexec does not work.
   Then the function dump-emacs will not be defined
   and temacs will do (load "loadup") automatically unless told otherwise.  */

/* #define CANNOT_DUMP */

/* Define VIRT_ADDR_VARIES if the virtual addresses of
   pure and impure space as loaded can vary, and even their
   relative order cannot be relied on.

   Otherwise Emacs assumes that text space precedes data space,
   numerically.  */

/* #define VIRT_ADDR_VARIES */

/* Define NO_REMAP if memory segmentation makes it not work well
   to change the boundary between the text section and data section
   when Emacs is dumped.  If you define this, the preloaded Lisp
   code will not be sharable; but that's better than failing completely.  */

/* #define NO_REMAP */

#define PNTR_COMPARISON_TYPE unsigned long

/* Define XPNTR to avoid or'ing with DATA_SEG_BITS */
#undef DATA_SEG_BITS

#ifdef __FreeBSD__

/* The libraries for binaries native to the build host's architecture are
   installed under /usr/lib in FreeBSD, and the ones that need special paths
   are 32-bit compatibility libraries (installed under /usr/lib32).  To build
   a native binary of Emacs on FreeBSD/amd64 we can just point to /usr/lib.  */

#undef START_FILES
#define START_FILES pre-crt0.o /usr/lib/crt1.o /usr/lib/crti.o

/* The duplicate -lgcc is intentional in the definition of LIB_STANDARD.
   The reason is that some functions in libgcc.a call functions from libc.a,
   and some libc.a functions need functions from libgcc.a.  Since most
   versions of ld are one-pass linkers, we need to mention -lgcc twice,
   or else we risk getting unresolved externals.  */
#undef LIB_STANDARD
#define LIB_STANDARD -lgcc -lc -lgcc /usr/lib/crtn.o

#elif defined(__OpenBSD__)

#undef START_FILES
#define START_FILES pre-crt0.o /usr/lib/crt0.o /usr/lib/crtbegin.o
#undef LIB_STANDARD
#define LIB_STANDARD -lgcc -lc -lgcc /usr/lib/crtend.o

#elif defined(__NetBSD__)

/* LIB_STANDARD and START_FILES set correctly in s/netbsd.h */

#elif defined(sun)

#undef START_FILES
#undef LIB_STANDARD

#else /* !__OpenBSD__ && !__FreeBSD__ && !__NetBSD__ && !sun */

#undef START_FILES
#ifdef HAVE_X86_64_LIB64_DIR
#define START_FILES pre-crt0.o /usr/lib64/crt1.o /usr/lib64/crti.o
#else
#define START_FILES pre-crt0.o /usr/lib/crt1.o /usr/lib/crti.o
#endif

/* The duplicate -lgcc is intentional in the definition of LIB_STANDARD.
   The reason is that some functions in libgcc.a call functions from libc.a,
   and some libc.a functions need functions from libgcc.a.  Since most
   versions of ld are one-pass linkers, we need to mention -lgcc twice,
   or else we risk getting unresolved externals.  */
#undef LIB_STANDARD
#ifdef HAVE_X86_64_LIB64_DIR
#define LIB_STANDARD -lgcc -lc -lgcc /usr/lib64/crtn.o
#else
#define LIB_STANDARD -lgcc -lc -lgcc /usr/lib/crtn.o
#endif

#endif /* __FreeBSD__ */

/* arch-tag: 8a5e001d-e12e-4692-a3a6-0b15ba271c6e
   (do not change this comment) */
