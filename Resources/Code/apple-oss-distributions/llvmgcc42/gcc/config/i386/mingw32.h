/* Operating system specific defines to be used when targeting GCC for
   hosting on Windows32, using GNU tools and the Windows32 API Library.
   Copyright (C) 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004
   Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

/* LLVM LOCAL begin mainline */
#undef TARGET_VERSION
#if TARGET_64BIT_DEFAULT
#define TARGET_VERSION fprintf (stderr,"(x86_64 MinGW");
#else
#define TARGET_VERSION fprintf (stderr," (x86 MinGW)");
#endif
/* LLVM LOCAL end mainline */

/* LLVM LOCAL begin mainline */
/* See i386/crtdll.h for an alternative definition. _INTEGRAL_MAX_BITS
   is for compatibility with native compiler.  */
#define EXTRA_OS_CPP_BUILTINS()					\
  do								\
    {								\
      builtin_define ("__MSVCRT__");				\
      builtin_define ("__MINGW32__");			   	\
      builtin_define ("_WIN32");				\
      builtin_define_std ("WIN32");				\
      builtin_define_std ("WINNT");				\
      builtin_define_with_int_value ("_INTEGRAL_MAX_BITS",	\
				     TYPE_PRECISION (intmax_type_node));\
      if (TARGET_64BIT_MS_ABI)					\
	{							\
	  builtin_define ("__MINGW64__");			\
	  builtin_define_std ("WIN64");				\
	  builtin_define_std ("_WIN64");			\
	}							\
    }								\
  while (0)
/* LLVM LOCAL end mainline */

/* Override the standard choice of /usr/include as the default prefix
   to try when searching for header files.  */
#undef STANDARD_INCLUDE_DIR
/* LLVM LOCAL begin mainline */
#if TARGET_64BIT_DEFAULT
#define STANDARD_INCLUDE_DIR "/mingw/include64"
#else
#define STANDARD_INCLUDE_DIR "/mingw/include"
#endif
/* LLVM LOCAL end mainline */
#undef STANDARD_INCLUDE_COMPONENT
#define STANDARD_INCLUDE_COMPONENT "MINGW"

#undef CPP_SPEC
#define CPP_SPEC "%{posix:-D_POSIX_SOURCE} %{mthreads:-D_MT}"

/* For Windows applications, include more libraries, but always include
   kernel32.  */
#undef LIB_SPEC
#define LIB_SPEC "%{pg:-lgmon} %{mwindows:-lgdi32 -lcomdlg32} \
                  -luser32 -lkernel32 -ladvapi32 -lshell32"

/* Include in the mingw32 libraries with libgcc */
#undef LINK_SPEC
#define LINK_SPEC "%{mwindows:--subsystem windows} \
  %{mconsole:--subsystem console} \
  %{shared: %{mdll: %eshared and mdll are not compatible}} \
  %{shared: --shared} %{mdll:--dll} \
  %{static:-Bstatic} %{!static:-Bdynamic} \
  %{shared|mdll: -e _DllMainCRTStartup@12}"

/* Include in the mingw32 libraries with libgcc */
#undef LIBGCC_SPEC
#define LIBGCC_SPEC \
  "%{mthreads:-lmingwthrd} -lmingw32 -lgcc -lmoldname -lmingwex -lmsvcrt"

/* LLVM LOCAL begin mainline 125696 */
#undef STARTFILE_SPEC
#define STARTFILE_SPEC "%{shared|mdll:dllcrt2%O%s} \
  %{!shared:%{!mdll:crt2%O%s}} %{pg:gcrt2%O%s}  \
  crtbegin%O%s"

#undef ENDFILE_SPEC
#define ENDFILE_SPEC "crtend%O%s"
/* LLVM LOCAL end mainline 125696 */


/* Override startfile prefix defaults.  */
#ifndef STANDARD_STARTFILE_PREFIX_1
/* LLVM LOCAL begin mainline */
#if TARGET_64BIT_DEFAULT
#define STANDARD_STARTFILE_PREFIX_1 "/mingw/lib64/"
#else
#define STANDARD_STARTFILE_PREFIX_1 "/mingw/lib/"
#endif
/* LLVM LOCAL end mainline */
#endif
#ifndef STANDARD_STARTFILE_PREFIX_2
#define STANDARD_STARTFILE_PREFIX_2 ""
#endif

/* Output STRING, a string representing a filename, to FILE.
   We canonicalize it to be in Unix format (backslashes are replaced
   forward slashes.  */
#undef OUTPUT_QUOTED_STRING
#define OUTPUT_QUOTED_STRING(FILE, STRING)               \
do {						         \
  char c;					         \
						         \
  putc ('\"', asm_file);			         \
						         \
  while ((c = *string++) != 0)			         \
    {						         \
      if (c == '\\')				         \
	c = '/';				         \
						         \
      if (ISPRINT (c))                                   \
        {                                                \
          if (c == '\"')			         \
	    putc ('\\', asm_file);		         \
          putc (c, asm_file);			         \
        }                                                \
      else                                               \
        fprintf (asm_file, "\\%03o", (unsigned char) c); \
    }						         \
						         \
  putc ('\"', asm_file);			         \
} while (0)

/* Define as short unsigned for compatibility with MS runtime.  */
#undef WINT_TYPE
#define WINT_TYPE "short unsigned int"

/* mingw32 uses the  -mthreads option to enable thread support.  */
#undef GOMP_SELF_SPECS
#define GOMP_SELF_SPECS "%{fopenmp: -mthreads}"


/* LLVM LOCAL begin mainline */
#define TARGET_USE_JCR_SECTION 0

#undef MINGW_ENABLE_EXECUTE_STACK
#define MINGW_ENABLE_EXECUTE_STACK     \
extern void __enable_execute_stack (void *);    \
void         \
__enable_execute_stack (void *addr)					\
{									\
  MEMORY_BASIC_INFORMATION b;						\
  if (!VirtualQuery (addr, &b, sizeof(b)))				\
    abort ();								\
  VirtualProtect (b.BaseAddress, b.RegionSize, PAGE_EXECUTE_READWRITE,	\
		  &b.Protect);						\
}

#undef ENABLE_EXECUTE_STACK
#define ENABLE_EXECUTE_STACK MINGW_ENABLE_EXECUTE_STACK


#ifdef IN_LIBGCC2
#include <windows.h>
#endif

#if !TARGET_64BIT
#define MD_UNWIND_SUPPORT "config/i386/w32-unwind.h"
#endif
/* LLVM LOCAL end mainline */
