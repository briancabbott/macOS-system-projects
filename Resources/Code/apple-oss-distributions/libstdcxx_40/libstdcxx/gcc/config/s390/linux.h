/* Definitions for Linux for S/390.
   Copyright (C) 1999, 2000, 2001, 2002, 2004 Free Software Foundation, Inc.
   Contributed by Hartmut Penner (hpenner@de.ibm.com) and
                  Ulrich Weigand (uweigand@de.ibm.com).

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#ifndef _LINUX_H
#define _LINUX_H

/* Target specific version string.  */

#ifdef DEFAULT_TARGET_64BIT
#undef  TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (Linux for zSeries)");
#else
#undef  TARGET_VERSION
#define TARGET_VERSION fprintf (stderr, " (Linux for S/390)");
#endif


/* Target specific type definitions.  */

/* ??? Do we really want long as size_t on 31-bit?  */
#undef  SIZE_TYPE
#define SIZE_TYPE (TARGET_64BIT ? "long unsigned int" : "long unsigned int")
#undef  PTRDIFF_TYPE
#define PTRDIFF_TYPE (TARGET_64BIT ? "long int" : "int")

#undef  WCHAR_TYPE
#define WCHAR_TYPE "int"
#undef  WCHAR_TYPE_SIZE
#define WCHAR_TYPE_SIZE 32


/* Target specific preprocessor settings.  */

#define TARGET_OS_CPP_BUILTINS()		\
  do						\
    {						\
      LINUX_TARGET_OS_CPP_BUILTINS();		\
      if (flag_pic)				\
        {					\
          builtin_define ("__PIC__");		\
          builtin_define ("__pic__");		\
        }					\
    }						\
  while (0)


/* Target specific assembler settings.  */

#undef  ASM_SPEC
#define ASM_SPEC "%{m31&m64}%{mesa&mzarch}%{march=*}"


/* Target specific linker settings.  */

#ifdef DEFAULT_TARGET_64BIT
#define MULTILIB_DEFAULTS { "m64" }
#else
#define MULTILIB_DEFAULTS { "m31" }
#endif

#undef  LINK_SPEC
#define LINK_SPEC \
  "%{m31:-m elf_s390}%{m64:-m elf64_s390} \
   %{shared:-shared} \
   %{!shared: \
      %{static:-static} \
      %{!static: \
	%{rdynamic:-export-dynamic} \
	%{!dynamic-linker: \
          %{m31:-dynamic-linker /lib/ld.so.1} \
          %{m64:-dynamic-linker /lib/ld64.so.1}}}}"


#define TARGET_ASM_FILE_END file_end_indicate_exec_stack

#define MD_UNWIND_SUPPORT "config/s390/linux-unwind.h"

#endif
