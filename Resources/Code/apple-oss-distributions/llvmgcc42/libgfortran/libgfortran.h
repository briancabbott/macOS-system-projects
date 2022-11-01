/* Common declarations for all of libgfortran.
   Copyright (C) 2002, 2003, 2004, 2005, 2006 Free Software Foundation, Inc.
   Contributed by Paul Brook <paul@nowt.org>, and
   Andy Vaught <andy@xena.eas.asu.edu>

This file is part of the GNU Fortran 95 runtime library (libgfortran).

Libgfortran is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

Libgfortran is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with libgfor; see the file COPYING.LIB.  If not,
write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

/* As a special exception, if you link this library with other files,
   some of which are compiled with GCC, to produce an executable,
   this library does not by itself cause the resulting executable
   to be covered by the GNU General Public License.
   This exception does not however invalidate any other reasons why
   the executable file might be covered by the GNU General Public License.  */


#ifndef LIBGFOR_H
#define LIBGFOR_H

#include <math.h>
#include <stddef.h>
#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327
#endif

#if HAVE_COMPLEX_H
# include <complex.h>
#else
#define complex __complex__
#endif

#include "config.h"
#include "c99_protos.h"

#if HAVE_IEEEFP_H
#include <ieeefp.h>
#endif

#include "gstdint.h"

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
typedef off_t gfc_offset;

#ifndef NULL
#define NULL (void *) 0
#endif

#ifndef __GNUC__
#define __attribute__(x)
#endif

/* For a library, a standard prefix is a requirement in order to partition
   the namespace.  IPREFIX is for symbols intended to be internal to the
   library.  */
#define PREFIX(x)	_gfortran_ ## x
#define IPREFIX(x)	_gfortrani_ ## x

/* Magic to rename a symbol at the compiler level.  You continue to refer
   to the symbol as OLD in the source, but it'll be named NEW in the asm.  */
#define sym_rename(old, new) sym_rename1(old, __USER_LABEL_PREFIX__, new)
#define sym_rename1(old, ulp, new) sym_rename2(old, ulp, new)
#define sym_rename2(old, ulp, new) extern __typeof(old) old __asm__(#ulp #new)

/* There are several classifications of routines:

     (1) Symbols used only within the library,
     (2) Symbols to be exported from the library,
     (3) Symbols to be exported from the library, but
	 also used inside the library.

   By telling the compiler about these different classifications we can
   tightly control the interface seen by the user, and get better code
   from the compiler at the same time.

   One of the following should be used immediately after the declaration
   of each symbol:

     internal_proto	Marks a symbol used only within the library,
			and adds IPREFIX to the assembly-level symbol
			name.  The later is important for maintaining
			the namespace partition for the static library.

     export_proto	Marks a symbol to be exported, and adds PREFIX
			to the assembly-level symbol name.

     export_proto_np	Marks a symbol to be exported without adding PREFIX.

     iexport_proto	Marks a function to be exported, but with the 
			understanding that it can be used inside as well.

     iexport_data_proto	Similarly, marks a data symbol to be exported.
			Unfortunately, some systems can't play the hidden
			symbol renaming trick on data symbols, thanks to
			the horribleness of COPY relocations.

   If iexport_proto or iexport_data_proto is used, you must also use
   iexport or iexport_data after the *definition* of the symbol.  */

#if defined(HAVE_ATTRIBUTE_VISIBILITY)
# define internal_proto(x) \
	sym_rename(x, IPREFIX (x)) __attribute__((__visibility__("hidden")))
#else
# define internal_proto(x)	sym_rename(x, IPREFIX(x))
#endif

#if defined(HAVE_ATTRIBUTE_VISIBILITY) && defined(HAVE_ATTRIBUTE_ALIAS)
# define export_proto(x)	sym_rename(x, PREFIX(x))
# define export_proto_np(x)	extern char swallow_semicolon
# define iexport_proto(x)	internal_proto(x)
# define iexport(x)		iexport1(x, __USER_LABEL_PREFIX__, IPREFIX(x))
# define iexport1(x,p,y)	iexport2(x,p,y)
# define iexport2(x,p,y) \
	extern __typeof(x) PREFIX(x) __attribute__((__alias__(#p #y)))
/* ??? We're not currently building a dll, and it's wrong to add dllexport
   to objects going into a static library archive.  */
#elif 0 && defined(HAVE_ATTRIBUTE_DLLEXPORT)
# define export_proto_np(x)	extern __typeof(x) x __attribute__((dllexport))
# define export_proto(x)    sym_rename(x, PREFIX(x)) __attribute__((dllexport))
# define iexport_proto(x)	export_proto(x)
# define iexport(x)		extern char swallow_semicolon
#else
# define export_proto(x)	sym_rename(x, PREFIX(x))
# define export_proto_np(x)	extern char swallow_semicolon
# define iexport_proto(x)	export_proto(x)
# define iexport(x)		extern char swallow_semicolon
#endif

/* TODO: detect the case when we *can* hide the symbol.  */
#define iexport_data_proto(x)	export_proto(x)
#define iexport_data(x)		extern char swallow_semicolon

/* The only reliable way to get the offset of a field in a struct
   in a system independent way is via this macro.  */
#ifndef offsetof
#define offsetof(TYPE, MEMBER)  ((size_t) &((TYPE *) 0)->MEMBER)
#endif

/* The isfinite macro is only available with C99, but some non-C99
   systems still provide fpclassify, and there is a `finite' function
   in BSD.

   Also, isfinite is broken on Cygwin.

   When isfinite is not available, try to use one of the
   alternatives, or bail out.  */

#if defined(HAVE_BROKEN_ISFINITE) || defined(__CYGWIN__)
#undef isfinite
#endif

#if defined(HAVE_BROKEN_ISNAN)
#undef isnan
#endif

#if defined(HAVE_BROKEN_FPCLASSIFY)
#undef fpclassify
#endif

#if !defined(isfinite)
#if !defined(fpclassify)
#define isfinite(x) ((x) - (x) == 0)
#else
#define isfinite(x) (fpclassify(x) != FP_NAN && fpclassify(x) != FP_INFINITE)
#endif /* !defined(fpclassify) */
#endif /* !defined(isfinite)  */

#if !defined(isnan)
#if !defined(fpclassify)
#define isnan(x) ((x) != (x))
#else
#define isnan(x) (fpclassify(x) == FP_NAN)
#endif /* !defined(fpclassify) */
#endif /* !defined(isfinite)  */

/* TODO: find the C99 version of these an move into above ifdef.  */
#define REALPART(z) (__real__(z))
#define IMAGPART(z) (__imag__(z))
#define COMPLEX_ASSIGN(z_, r_, i_) {__real__(z_) = (r_); __imag__(z_) = (i_);}

#include "kinds.h"

/* Define the type used for the current record number for large file I/O.
   The size must be consistent with the size defined on the compiler side.  */
#ifdef HAVE_GFC_INTEGER_8
typedef GFC_INTEGER_8 GFC_IO_INT;
#else
#ifdef HAVE_GFC_INTEGER_4
typedef GFC_INTEGER_4 GFC_IO_INT;
#else
#error "GFC_INTEGER_4 should be available for the library to compile".
#endif
#endif

/* The following two definitions must be consistent with the types used
   by the compiler.  */
/* The type used of array indices, amongst other things.  */
typedef ssize_t index_type;
/* The type used for the lengths of character variables.  */
typedef GFC_INTEGER_4 gfc_charlen_type;

/* This will be 0 on little-endian machines and one on big-endian machines.  */
extern int l8_to_l4_offset;
internal_proto(l8_to_l4_offset);

#define GFOR_POINTER_L8_TO_L4(p8) \
  (l8_to_l4_offset + (GFC_LOGICAL_4 *)(p8))

#define GFC_INTEGER_1_HUGE \
  (GFC_INTEGER_1)((((GFC_UINTEGER_1)1) << 7) - 1)
#define GFC_INTEGER_2_HUGE \
  (GFC_INTEGER_2)((((GFC_UINTEGER_2)1) << 15) - 1)
#define GFC_INTEGER_4_HUGE \
  (GFC_INTEGER_4)((((GFC_UINTEGER_4)1) << 31) - 1)
#define GFC_INTEGER_8_HUGE \
  (GFC_INTEGER_8)((((GFC_UINTEGER_8)1) << 63) - 1)
#ifdef HAVE_GFC_INTEGER_16
#define GFC_INTEGER_16_HUGE \
  (GFC_INTEGER_16)((((GFC_UINTEGER_16)1) << 127) - 1)
#endif

#define GFC_REAL_4_HUGE FLT_MAX
#define GFC_REAL_8_HUGE DBL_MAX
#ifdef HAVE_GFC_REAL_10
#define GFC_REAL_10_HUGE LDBL_MAX
#endif
#ifdef HAVE_GFC_REAL_16
#define GFC_REAL_16_HUGE LDBL_MAX
#endif

#define GFC_REAL_4_DIGITS FLT_MANT_DIG
#define GFC_REAL_8_DIGITS DBL_MANT_DIG
#ifdef HAVE_GFC_REAL_10
#define GFC_REAL_10_DIGITS LDBL_MANT_DIG
#endif
#ifdef HAVE_GFC_REAL_16
#define GFC_REAL_16_DIGITS LDBL_MANT_DIG
#endif

#define GFC_REAL_4_RADIX FLT_RADIX
#define GFC_REAL_8_RADIX FLT_RADIX
#ifdef HAVE_GFC_REAL_10
#define GFC_REAL_10_RADIX FLT_RADIX
#endif
#ifdef HAVE_GFC_REAL_16
#define GFC_REAL_16_RADIX FLT_RADIX
#endif

#ifndef GFC_MAX_DIMENSIONS
#define GFC_MAX_DIMENSIONS 7
#endif

typedef struct descriptor_dimension
{
  index_type stride;
  index_type lbound;
  index_type ubound;
}
descriptor_dimension;

#define GFC_ARRAY_DESCRIPTOR(r, type) \
struct {\
  type *data;\
  size_t offset;\
  index_type dtype;\
  descriptor_dimension dim[r];\
}

/* Commonly used array descriptor types.  */
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, void) gfc_array_void;
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, char) gfc_array_char;
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_INTEGER_1) gfc_array_i1;
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_INTEGER_2) gfc_array_i2;
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_INTEGER_4) gfc_array_i4;
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_INTEGER_8) gfc_array_i8;
#ifdef HAVE_GFC_INTEGER_16
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_INTEGER_16) gfc_array_i16;
#endif
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_REAL_4) gfc_array_r4;
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_REAL_8) gfc_array_r8;
#ifdef HAVE_GFC_REAL_10
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_REAL_10) gfc_array_r10;
#endif
#ifdef HAVE_GFC_REAL_16
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_REAL_16) gfc_array_r16;
#endif
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_COMPLEX_4) gfc_array_c4;
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_COMPLEX_8) gfc_array_c8;
#ifdef HAVE_GFC_COMPLEX_10
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_COMPLEX_10) gfc_array_c10;
#endif
#ifdef HAVE_GFC_COMPLEX_16
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_COMPLEX_16) gfc_array_c16;
#endif
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_LOGICAL_4) gfc_array_l4;
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_LOGICAL_8) gfc_array_l8;
#ifdef HAVE_GFC_LOGICAL_16
typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, GFC_LOGICAL_16) gfc_array_l16;
#endif

#define GFC_DTYPE_RANK_MASK 0x07
#define GFC_DTYPE_TYPE_SHIFT 3
#define GFC_DTYPE_TYPE_MASK 0x38
#define GFC_DTYPE_SIZE_SHIFT 6

enum
{
  GFC_DTYPE_UNKNOWN = 0,
  GFC_DTYPE_INTEGER,
  /* TODO: recognize logical types.  */
  GFC_DTYPE_LOGICAL,
  GFC_DTYPE_REAL,
  GFC_DTYPE_COMPLEX,
  GFC_DTYPE_DERIVED,
  GFC_DTYPE_CHARACTER
};

#define GFC_DESCRIPTOR_RANK(desc) ((desc)->dtype & GFC_DTYPE_RANK_MASK)
#define GFC_DESCRIPTOR_TYPE(desc) (((desc)->dtype & GFC_DTYPE_TYPE_MASK) \
                                   >> GFC_DTYPE_TYPE_SHIFT)
#define GFC_DESCRIPTOR_SIZE(desc) ((desc)->dtype >> GFC_DTYPE_SIZE_SHIFT)
#define GFC_DESCRIPTOR_DATA(desc) ((desc)->data)
#define GFC_DESCRIPTOR_DTYPE(desc) ((desc)->dtype)

/* Runtime library include.  */
#define stringize(x) expand_macro(x)
#define expand_macro(x) # x

/* Runtime options structure.  */

typedef struct
{
  int stdin_unit, stdout_unit, stderr_unit, optional_plus;
  int allocate_init_flag, allocate_init_value;
  int locus;

  int separator_len;
  const char *separator;

  int mem_check;
  int use_stderr, all_unbuffered, default_recl;

  int fpu_round, fpu_precision, fpe;

  int sighup, sigint;
}
options_t;

extern options_t options;
internal_proto(options);


/* Compile-time options that will influence the library.  */

typedef struct
{
  int warn_std;
  int allow_std;
  int pedantic;
  int convert;
  size_t record_marker;
  int max_subrecord_length;
}
compile_options_t;

extern compile_options_t compile_options;
internal_proto(compile_options);

extern void init_compile_options (void);
internal_proto(init_compile_options);

#define GFC_MAX_SUBRECORD_LENGTH 2147483639   /* 2**31 - 9 */

/* Structure for statement options.  */

typedef struct
{
  const char *name;
  int value;
}
st_option;

/* Runtime errors.  The EOR and EOF errors are required to be negative.  */

typedef enum
{
  ERROR_FIRST = -3,		/* Marker for the first error.  */
  ERROR_EOR = -2,
  ERROR_END = -1,
  ERROR_OK = 0,			/* Indicates success, must be zero.  */
  ERROR_OS,			/* Operating system error, more info in errno.  */
  ERROR_OPTION_CONFLICT,
  ERROR_BAD_OPTION,
  ERROR_MISSING_OPTION,
  ERROR_ALREADY_OPEN,
  ERROR_BAD_UNIT,
  ERROR_FORMAT,
  ERROR_BAD_ACTION,
  ERROR_ENDFILE,
  ERROR_BAD_US,
  ERROR_READ_VALUE,
  ERROR_READ_OVERFLOW,
  ERROR_INTERNAL,
  ERROR_INTERNAL_UNIT,
  ERROR_ALLOCATION,
  ERROR_DIRECT_EOR,
  ERROR_SHORT_RECORD,
  ERROR_CORRUPT_FILE,
  ERROR_LAST			/* Not a real error, the last error # + 1.  */
}
error_codes;


/* Flags to specify which standard/extension contains a feature.
   Keep them in sync with their counterparts in gcc/fortran/gfortran.h.  */
#define GFC_STD_LEGACY          (1<<6) /* Backward compatibility.  */
#define GFC_STD_GNU             (1<<5)    /* GNU Fortran extension.  */
#define GFC_STD_F2003           (1<<4)    /* New in F2003.  */
/* Note that no features were obsoleted nor deleted in F2003.  */
#define GFC_STD_F95             (1<<3)    /* New in F95.  */
#define GFC_STD_F95_DEL         (1<<2)    /* Deleted in F95.  */
#define GFC_STD_F95_OBS         (1<<1)    /* Obsoleted in F95.  */
#define GFC_STD_F77             (1<<0)    /* Up to and including F77.  */

/* Bitmasks for the various FPE that can be enabled.
   Keep them in sync with their counterparts in gcc/fortran/gfortran.h.  */
#define GFC_FPE_INVALID    (1<<0)
#define GFC_FPE_DENORMAL   (1<<1)
#define GFC_FPE_ZERO       (1<<2)
#define GFC_FPE_OVERFLOW   (1<<3)
#define GFC_FPE_UNDERFLOW  (1<<4)
#define GFC_FPE_PRECISION  (1<<5)

/* This is returned by notification_std to know if, given the flags
   that were given (-std=, -pedantic) we should issue an error, a warning
   or nothing.  */
typedef enum
{ SILENT, WARNING, ERROR }
notification;

/* This is returned by notify_std and several io functions.  */
typedef enum
{ SUCCESS = 1, FAILURE }
try;

/* The filename and line number don't go inside the globals structure.
   They are set by the rest of the program and must be linked to.  */

/* Location of the current library call (optional).  */
extern unsigned line;
iexport_data_proto(line);

extern char *filename;
iexport_data_proto(filename);

/* Avoid conflicting prototypes of alloca() in system headers by using 
   GCC's builtin alloca().  */
#define gfc_alloca(x)  __builtin_alloca(x)


/* main.c */

extern void stupid_function_name_for_static_linking (void);
internal_proto(stupid_function_name_for_static_linking);

struct st_parameter_common;
extern void library_start (struct st_parameter_common *);
internal_proto(library_start);

#define library_end()

extern void set_args (int, char **);
export_proto(set_args);

extern void get_args (int *, char ***);
internal_proto(get_args);

/* error.c */

#define GFC_ITOA_BUF_SIZE (sizeof (GFC_INTEGER_LARGEST) * 3 + 2)
#define GFC_XTOA_BUF_SIZE (sizeof (GFC_UINTEGER_LARGEST) * 2 + 1)
#define GFC_OTOA_BUF_SIZE (sizeof (GFC_INTEGER_LARGEST) * 3 + 1)
#define GFC_BTOA_BUF_SIZE (sizeof (GFC_INTEGER_LARGEST) * 8 + 1)

extern const char *gfc_itoa (GFC_INTEGER_LARGEST, char *, size_t);
internal_proto(gfc_itoa);

extern const char *xtoa (GFC_UINTEGER_LARGEST, char *, size_t);
internal_proto(xtoa);

extern void os_error (const char *) __attribute__ ((noreturn));
internal_proto(os_error);

extern void show_locus (struct st_parameter_common *);
internal_proto(show_locus);

extern void runtime_error (const char *) __attribute__ ((noreturn));
iexport_proto(runtime_error);

extern void internal_error (struct st_parameter_common *, const char *)
  __attribute__ ((noreturn));
internal_proto(internal_error);

extern const char *get_oserror (void);
internal_proto(get_oserror);

extern void sys_exit (int) __attribute__ ((noreturn));
internal_proto(sys_exit);

extern int st_printf (const char *, ...)
  __attribute__ ((format (printf, 1, 2)));
internal_proto(st_printf);

extern void st_sprintf (char *, const char *, ...)
  __attribute__ ((format (printf, 2, 3)));
internal_proto(st_sprintf);

extern const char *translate_error (int);
internal_proto(translate_error);

extern void generate_error (struct st_parameter_common *, int, const char *);
internal_proto(generate_error);

extern try notify_std (struct st_parameter_common *, int, const char *);
internal_proto(notify_std);

/* fpu.c */

extern void set_fpu (void);
internal_proto(set_fpu);

/* memory.c */

extern void *get_mem (size_t) __attribute__ ((malloc));
internal_proto(get_mem);

extern void free_mem (void *);
internal_proto(free_mem);

extern void *internal_malloc_size (size_t);
internal_proto(internal_malloc_size);

extern void internal_free (void *);
iexport_proto(internal_free);

/* environ.c */

extern int check_buffered (int);
internal_proto(check_buffered);

extern void init_variables (void);
internal_proto(init_variables);

extern void show_variables (void);
internal_proto(show_variables);

/* string.c */

extern int find_option (struct st_parameter_common *, const char *, int,
			const st_option *, const char *);
internal_proto(find_option);

extern int fstrlen (const char *, int);
internal_proto(fstrlen);

extern void fstrcpy (char *, int, const char *, int);
internal_proto(fstrcpy);

extern void cf_strcpy (char *, int, const char *);
internal_proto(cf_strcpy);

/* io.c */

extern void init_units (void);
internal_proto(init_units);

extern void close_units (void);
internal_proto(close_units);

extern int unit_to_fd (int);
internal_proto(unit_to_fd);

/* stop.c */

extern void stop_numeric (GFC_INTEGER_4);
iexport_proto(stop_numeric);

/* reshape_packed.c */

extern void reshape_packed (char *, index_type, const char *, index_type,
			    const char *, index_type);
internal_proto(reshape_packed);

/* Repacking functions.  */

/* ??? These aren't currently used by the compiler, though we
   certainly could do so.  */
GFC_INTEGER_4 *internal_pack_4 (gfc_array_i4 *);
internal_proto(internal_pack_4);

GFC_INTEGER_8 *internal_pack_8 (gfc_array_i8 *);
internal_proto(internal_pack_8);

#if defined HAVE_GFC_INTEGER_16
GFC_INTEGER_16 *internal_pack_16 (gfc_array_i16 *);
internal_proto(internal_pack_16);
#endif

GFC_COMPLEX_4 *internal_pack_c4 (gfc_array_c4 *);
internal_proto(internal_pack_c4);

GFC_COMPLEX_8 *internal_pack_c8 (gfc_array_c8 *);
internal_proto(internal_pack_c8);

#if defined HAVE_GFC_COMPLEX_10
GFC_COMPLEX_10 *internal_pack_c10 (gfc_array_c10 *);
internal_proto(internal_pack_c10);
#endif

extern void internal_unpack_4 (gfc_array_i4 *, const GFC_INTEGER_4 *);
internal_proto(internal_unpack_4);

extern void internal_unpack_8 (gfc_array_i8 *, const GFC_INTEGER_8 *);
internal_proto(internal_unpack_8);

#if defined HAVE_GFC_INTEGER_16
extern void internal_unpack_16 (gfc_array_i16 *, const GFC_INTEGER_16 *);
internal_proto(internal_unpack_16);
#endif

extern void internal_unpack_c4 (gfc_array_c4 *, const GFC_COMPLEX_4 *);
internal_proto(internal_unpack_c4);

extern void internal_unpack_c8 (gfc_array_c8 *, const GFC_COMPLEX_8 *);
internal_proto(internal_unpack_c8);

#if defined HAVE_GFC_COMPLEX_10
extern void internal_unpack_c10 (gfc_array_c10 *, const GFC_COMPLEX_10 *);
internal_proto(internal_unpack_c10);
#endif

#if defined HAVE_GFC_COMPLEX_16
extern void internal_unpack_c16 (gfc_array_c16 *, const GFC_COMPLEX_16 *);
internal_proto(internal_unpack_c16);
#endif

/* string_intrinsics.c */

extern GFC_INTEGER_4 compare_string (GFC_INTEGER_4, const char *,
				     GFC_INTEGER_4, const char *);
iexport_proto(compare_string);

/* random.c */

extern void random_seed (GFC_INTEGER_4 * size, gfc_array_i4 * put,
			 gfc_array_i4 * get);
iexport_proto(random_seed);

/* size.c */

typedef GFC_ARRAY_DESCRIPTOR (GFC_MAX_DIMENSIONS, void) array_t;

extern index_type size0 (const array_t * array); 
iexport_proto(size0);

#endif  /* LIBGFOR_H  */
