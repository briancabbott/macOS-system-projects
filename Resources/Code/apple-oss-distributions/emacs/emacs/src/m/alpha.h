/* machine description file For the alpha chip.
   Copyright (C) 1994, 1997, 1999, 2001, 2002, 2003, 2004,
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


/* The following line tells the configuration script what sort of
   operating system this machine is likely to run.
   USUAL-OPSYS="note"

NOTE-START
Use -opsystem=osf1
NOTE-END

*/

#ifndef _LP64
#define _LP64			/* This doesn't appear to be necessary
				   on OSF 4/5  -- fx.  */
#endif

/* Define WORDS_BIG_ENDIAN iff lowest-numbered byte in a word
   is the most significant byte.  */

#undef WORDS_BIG_ENDIAN

/* Define NO_ARG_ARRAY if you cannot take the address of the first of a
 * group of arguments and treat it as an array of the arguments.  */

#define NO_ARG_ARRAY

/* Now define a symbol for the cpu type, if your compiler
   does not define it automatically:
   Ones defined so far include vax, m68000, ns16000, pyramid,
   orion, tahoe, APOLLO and many others */

/* __alpha defined automatically */


/* Use type EMACS_INT rather than a union, to represent Lisp_Object */
/* This is desirable for most machines.  */
#define NO_UNION_TYPE

/* Define EXPLICIT_SIGN_EXTEND if XINT must explicitly sign-extend
   the 24-bit bit field into an int.  In other words, if bit fields
   are always unsigned.

   If you use NO_UNION_TYPE, this flag does not matter.  */

#define EXPLICIT_SIGN_EXTEND

/* Data type of load average, as read out of kmem.  */

#define LOAD_AVE_TYPE long

/* Convert that into an integer that is 100 for a load average of 1.0  */

#define LOAD_AVE_CVT(x) (int) (((double) (x)) * 100.0 / FSCALE)

/* GNU malloc and the relocating allocator do not work together
   with X.   [Who wrote that?]  */

/* May 1995: reportedly [Rainer Schoepf <schoepf@uni-mainz.de>] both the
   system and the gnu malloc system work with "alpha-dec-osf3.0" and
   "alpha-dec-osf3.2".  */

/* May 1995: it seems to me [Morten Welinder <terra@diku.dk>] that both
   mallocs work with "alpha-dec-osf2.0", but I daren't break anything
   right now.  Feel free to play if you want.  */

/* #define SYSTEM_MALLOC */

#ifdef __ELF__
/* With ELF, make sure that all common symbols get allocated to in the
   data section.  Otherwise, the dump of temacs may miss variables in
   the shared library that have been initialized.  For example, with
   GNU libc, __malloc_initialized would normally be resolved to the
   shared library's .bss section, which is fatal.  */
# ifdef __GNUC__
#  define C_SWITCH_MACHINE	-fno-common
# else
#  error What gives?  Fix me if DEC Unix supports ELF now.
# endif
#endif

#if defined(__OpenBSD__)
#define ORDINARY_LINK
#endif

#ifdef __ELF__
#undef UNEXEC
#define UNEXEC unexelf.o
#ifndef LINUX
#define DATA_START    0x140000000
#endif
#endif

#ifndef __ELF__

/* Describe layout of the address space in an executing process.  */

#define TEXT_START    0x120000000
#define DATA_START    0x140000000

/* The program to be used for unexec. */

#define UNEXEC unexalpha.o

#endif /* notdef __ELF__ */

#if defined (LINUX) && __GNU_LIBRARY__ - 0 < 6
/* This controls a conditional in main.  */
#define LINUX_SBRK_BUG
#endif

/* On the Alpha it's best to avoid including TERMIO since struct
   termio and struct termios are mutually incompatible.  */
#define NO_TERMIO

#if defined (LINUX) || defined (__NetBSD__) || defined (__OpenBSD__)
# define TEXT_END ({ extern int _etext; &_etext; })
# ifndef __ELF__
#  define COFF
#  define DATA_END ({ extern int _EDATA; &_EDATA; })
# endif /* notdef __ELF__ */
#endif

#if (defined (__NetBSD__) || defined (__OpenBSD__)) && defined (__ELF__)
#define HAVE_TEXT_START
#endif

/* Many Alpha implementations (e.g. gas 2.8) can't handle DBL_MIN:
   they generate code that uses a signaling NaN instead of DBL_MIN.
   Define DBL_MIN_REPLACEMENT to be the next value larger than DBL_MIN:
   this avoids the assembler bug.  */
#define DBL_MIN_REPLACEMENT 2.2250738585072019e-308

/* arch-tag: 978cb578-1e25-4a60-819b-adae0972aa78
   (do not change this comment) */
