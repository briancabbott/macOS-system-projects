/* RTPC machine dependent defines
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
   USUAL-OPSYS="bsd4-2"  */

/* Define WORDS_BIG_ENDIAN iff lowest-numbered byte in a word
   is the most significant byte.  */

#define WORDS_BIG_ENDIAN

/* Define NO_ARG_ARRAY if you cannot take the address of the first of a
 * group of arguments and treat it as an array of the arguments.  */

#define NO_ARG_ARRAY

/* Define WORD_MACHINE if addresses and such have
 * to be corrected before they can be used as byte counts.  */

#define WORD_MACHINE

/* Now define a symbol for the cpu type, if your compiler
   does not define it automatically.  */

#define ibmrt
#define romp /* unfortunately old include files are hanging around.  */

/* Use type int rather than a union, to represent Lisp_Object */
/* This is desirable for most machines.  */

#define NO_UNION_TYPE

/* Define EXPLICIT_SIGN_EXTEND if XINT must explicitly sign-extend
   the bit field into an int.  In other words, if bit fields
   are always unsigned.

   If you use NO_UNION_TYPE, this flag does not matter.  */

#define EXPLICIT_SIGN_EXTEND

/* Data type of load average, as read out of kmem.  */

#define LOAD_AVE_TYPE double	/* For AIS (sysV) */

/* Convert that into an integer that is 100 for a load average of 1.0  */

#define LOAD_AVE_CVT(x) (int) (((double) (x)) * 100.0)

/* Define CANNOT_DUMP on machines where unexec does not work.
   Then the function dump-emacs will not be defined
   and temacs will do (load "loadup") automatically unless told otherwise.  */

/* #define CANNOT_DUMP */

/* Define VIRT_ADDR_VARIES if the virtual addresses of
   pure and impure space as loaded can vary, and even their
   relative order cannot be relied on.

   Otherwise Emacs assumes that text space precedes data space,
   numerically.  */

#undef VIRT_ADDR_VARIES

/* The data segment in this machine starts at a fixed address.
   An address of data cannot be stored correctly in a Lisp object;
   we always lose the high bits.  We must tell XPNTR to add them back.  */

#define DATA_SEG_BITS 0x10000000
#define DATA_START    0x10000000

/* The text segment always starts at a fixed address.
   This way we don't need to have a label _start defined.  */
#define TEXT_START 0

/* Taking a pointer to a char casting it as int pointer */
/* and then taking the int which the int pointer points to */
/* is practically guaranteed to give erroneous results */

#define NEED_ERRNO

#define SKTPAIR

/* BSD has BSTRING.  */

#define BSTRING

/* Special switches to give the C compiler.  */

#ifndef __GNUC__
#define C_SWITCH_MACHINE -Dalloca=_Alloca
#endif

/* Don't attempt to relabel some of the data as text when dumping.
   It does not work because their virtual addresses are not consecutive.
   This enables us to use the standard crt0.o.  */

#define NO_REMAP

/* Use the bitmap files that come with Emacs.  */
#define EMACS_BITMAP_FILES

/* arch-tag: 89aa7e7d-593e-432c-966a-3db6aa2ad665
   (do not change this comment) */
