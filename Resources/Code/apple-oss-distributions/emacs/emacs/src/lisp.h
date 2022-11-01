/* Fundamental definitions for GNU Emacs Lisp interpreter.
   Copyright (C) 1985, 1986, 1987, 1993, 1994, 1995, 1997, 1998, 1999, 2000,
                 2001, 2002, 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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

#ifndef EMACS_LISP_H
#define EMACS_LISP_H

/* Declare the prototype for a general external function.  */
#if defined (PROTOTYPES) || defined (WINDOWSNT)
#define P_(proto) proto
#else
#define P_(proto) ()
#endif

#if 0
/* Define this temporarily to hunt a bug.  If defined, the size of
   strings is redundantly recorded in sdata structures so that it can
   be compared to the sizes recorded in Lisp strings.  */

#define GC_CHECK_STRING_BYTES 1

/* Define this to check for short string overrun.  */

#define GC_CHECK_STRING_OVERRUN 1

/* Define this to check the string free list.  */

#define GC_CHECK_STRING_FREE_LIST 1

/* Define this to check for malloc buffer overrun.  */

#define XMALLOC_OVERRUN_CHECK 1

/* Define this to check for errors in cons list.  */
/* #define GC_CHECK_CONS_LIST 1 */

#endif /* 0 */

#ifdef GC_CHECK_CONS_LIST
#define CHECK_CONS_LIST() check_cons_list()
#else
#define CHECK_CONS_LIST() 0
#endif

/* These are default choices for the types to use.  */
#ifdef _LP64
#ifndef EMACS_INT
#define EMACS_INT long
#define BITS_PER_EMACS_INT BITS_PER_LONG
#endif
#ifndef EMACS_UINT
#define EMACS_UINT unsigned long
#endif
#else /* not _LP64 */
#ifndef EMACS_INT
#define EMACS_INT int
#define BITS_PER_EMACS_INT BITS_PER_INT
#endif
#ifndef EMACS_UINT
#define EMACS_UINT unsigned int
#endif
#endif

/* Extra internal type checking?  */
extern int suppress_checking;
extern void die P_((const char *, const char *, int)) NO_RETURN;

#ifdef ENABLE_CHECKING

#define CHECK(check,msg) (((check) || suppress_checking		\
			   ? (void) 0				\
			   : die ((msg), __FILE__, __LINE__)),	\
			  0)
#else

/* Produce same side effects and result, but don't complain.  */
#define CHECK(check,msg) ((check),0)

#endif

/* Used for making sure that Emacs is compilable in all
   configurations.  */

#ifdef USE_LISP_UNION_TYPE
#undef NO_UNION_TYPE
#endif

/* Define an Emacs version of "assert", since some system ones are
   flaky.  */
#ifndef ENABLE_CHECKING
#define eassert(X)	(void) 0
#else /* ENABLE_CHECKING */
#if defined (__GNUC__) && __GNUC__ >= 2 && defined (__STDC__)
#define eassert(cond) CHECK(cond,"assertion failed: " #cond)
#else
#define eassert(cond) CHECK(cond,"assertion failed")
#endif
#endif /* ENABLE_CHECKING */

/* Define the fundamental Lisp data structures.  */

/* This is the set of Lisp data types.  */

enum Lisp_Type
  {
    /* Integer.  XINT (obj) is the integer value.  */
    Lisp_Int,

    /* Symbol.  XSYMBOL (object) points to a struct Lisp_Symbol.  */
    Lisp_Symbol,

    /* Miscellaneous.  XMISC (object) points to a union Lisp_Misc,
       whose first member indicates the subtype.  */
    Lisp_Misc,

    /* String.  XSTRING (object) points to a struct Lisp_String.
       The length of the string, and its contents, are stored therein.  */
    Lisp_String,

    /* Vector of Lisp objects, or something resembling it.
       XVECTOR (object) points to a struct Lisp_Vector, which contains
       the size and contents.  The size field also contains the type
       information, if it's not a real vector object.  */
    Lisp_Vectorlike,

    /* Cons.  XCONS (object) points to a struct Lisp_Cons.  */
    Lisp_Cons,

    Lisp_Float,

    /* This is not a type code.  It is for range checking.  */
    Lisp_Type_Limit
  };

/* This is the set of data types that share a common structure.
   The first member of the structure is a type code from this set.
   The enum values are arbitrary, but we'll use large numbers to make it
   more likely that we'll spot the error if a random word in memory is
   mistakenly interpreted as a Lisp_Misc.  */
enum Lisp_Misc_Type
  {
    Lisp_Misc_Free = 0x5eab,
    Lisp_Misc_Marker,
    Lisp_Misc_Intfwd,
    Lisp_Misc_Boolfwd,
    Lisp_Misc_Objfwd,
    Lisp_Misc_Buffer_Objfwd,
    Lisp_Misc_Buffer_Local_Value,
    Lisp_Misc_Some_Buffer_Local_Value,
    Lisp_Misc_Overlay,
    Lisp_Misc_Kboard_Objfwd,
    Lisp_Misc_Save_Value,
    /* Currently floats are not a misc type,
       but let's define this in case we want to change that.  */
    Lisp_Misc_Float,
    /* This is not a type code.  It is for range checking.  */
    Lisp_Misc_Limit
  };

#ifndef GCTYPEBITS
#define GCTYPEBITS 3
#endif

/* These values are overridden by the m- file on some machines.  */
#ifndef VALBITS
#define VALBITS (BITS_PER_EMACS_INT - GCTYPEBITS)
#endif

#ifndef NO_UNION_TYPE

#ifndef WORDS_BIG_ENDIAN

/* Definition of Lisp_Object for little-endian machines.  */

typedef
union Lisp_Object
  {
    /* Used for comparing two Lisp_Objects;
       also, positive integers can be accessed fast this way.  */
    EMACS_INT i;

    struct
      {
	EMACS_INT val  : VALBITS;
	enum Lisp_Type type : GCTYPEBITS;
      } s;
    struct
      {
	EMACS_UINT val : VALBITS;
	enum Lisp_Type type : GCTYPEBITS;
      } u;
  }
Lisp_Object;

#else /* If WORDS_BIG_ENDIAN */

typedef
union Lisp_Object
  {
    /* Used for comparing two Lisp_Objects;
       also, positive integers can be accessed fast this way.  */
    EMACS_INT i;

    struct
      {
	enum Lisp_Type type : GCTYPEBITS;
	EMACS_INT val  : VALBITS;
      } s;
    struct
      {
	enum Lisp_Type type : GCTYPEBITS;
	EMACS_UINT val : VALBITS;
      } u;
  }
Lisp_Object;

#endif /* WORDS_BIG_ENDIAN */

#ifdef __GNUC__
static __inline__ Lisp_Object
LISP_MAKE_RVALUE (Lisp_Object o)
{
    return o;
}
#else
/* This isn't quite right - it keeps the argument as an lvalue.
   Making it const via casting would help avoid code actually
   modifying the location in question, but the casting could cover
   other type-related bugs.  */
#define LISP_MAKE_RVALUE(o) (o)
#endif

#endif /* NO_UNION_TYPE */


/* If union type is not wanted, define Lisp_Object as just a number.  */

#ifdef NO_UNION_TYPE
typedef EMACS_INT Lisp_Object;
#define LISP_MAKE_RVALUE(o) (0+(o))
#endif /* NO_UNION_TYPE */

/* Two flags that are set during GC.  On some machines, these flags
   are defined differently by the m- file.  */

/* In the size word of a vector, this bit means the vector has been marked.  */

#ifndef ARRAY_MARK_FLAG
#define ARRAY_MARK_FLAG ((EMACS_INT) ((EMACS_UINT) 1 << (VALBITS + GCTYPEBITS - 1)))
#endif /* no ARRAY_MARK_FLAG */

/* In the size word of a struct Lisp_Vector, this bit means it's really
   some other vector-like object.  */
#ifndef PSEUDOVECTOR_FLAG
#define PSEUDOVECTOR_FLAG ((ARRAY_MARK_FLAG >> 1) & ~ARRAY_MARK_FLAG)
#endif

/* In a pseudovector, the size field actually contains a word with one
   PSEUDOVECTOR_FLAG bit set, and exactly one of the following bits to
   indicate the actual type.  */
enum pvec_type
{
  PVEC_NORMAL_VECTOR = 0,
  PVEC_PROCESS = 0x200,
  PVEC_FRAME = 0x400,
  PVEC_COMPILED = 0x800,
  PVEC_WINDOW = 0x1000,
  PVEC_WINDOW_CONFIGURATION = 0x2000,
  PVEC_SUBR = 0x4000,
  PVEC_CHAR_TABLE = 0x8000,
  PVEC_BOOL_VECTOR = 0x10000,
  PVEC_BUFFER = 0x20000,
  PVEC_HASH_TABLE = 0x40000,
  PVEC_TYPE_MASK = 0x7fe00

#if 0 /* This is used to make the value of PSEUDOVECTOR_FLAG available to
	 GDB.  It doesn't work on OS Alpha.  Moved to a variable in
	 emacs.c.  */
  PVEC_FLAG = PSEUDOVECTOR_FLAG
#endif
};

/* For convenience, we also store the number of elements in these bits.
   Note that this size is not necessarily the memory-footprint size, but
   only the number of Lisp_Object fields (that need to be traced by the GC).
   The distinction is used e.g. by Lisp_Process which places extra
   non-Lisp_Object fields at the end of the structure.  */
#define PSEUDOVECTOR_SIZE_MASK 0x1ff

/* Number of bits to put in each character in the internal representation
   of bool vectors.  This should not vary across implementations.  */
#define BOOL_VECTOR_BITS_PER_CHAR 8

/***** Select the tagging scheme.  *****/
/* There are basically two options that control the tagging scheme:
   - NO_UNION_TYPE says that Lisp_Object should be an integer instead
     of a union.
   - USE_LSB_TAG means that we can assume the least 3 bits of pointers are
     always 0, and we can thus use them to hold tag bits, without
     restricting our addressing space.

   If USE_LSB_TAG is not set, then we use the top 3 bits for tagging, thus
   restricting our possible address range.  Currently USE_LSB_TAG is not
   allowed together with a union.  This is not due to any fundamental
   technical (or political ;-) problem: nobody wrote the code to do it yet.

   USE_LSB_TAG not only requires the least 3 bits of pointers returned by
   malloc to be 0 but also needs to be able to impose a mult-of-8 alignment
   on the few static Lisp_Objects used: all the defsubr as well
   as the two special buffers buffer_defaults and buffer_local_symbols.  */

/* First, try and define DECL_ALIGN(type,var) which declares a static
   variable VAR of type TYPE with the added requirement that it be
   TYPEBITS-aligned. */
#ifndef NO_DECL_ALIGN
# ifndef DECL_ALIGN
/* What compiler directive should we use for non-gcc compilers?  -stef  */
#  if defined (__GNUC__)
#   define DECL_ALIGN(type, var) \
     type __attribute__ ((__aligned__ (1 << GCTYPEBITS))) var
#  endif
# endif
#endif

/* Let's USE_LSB_TAG on systems where we know malloc returns mult-of-8.  */
#if defined GNU_MALLOC || defined DOUG_LEA_MALLOC || defined __GLIBC__ || defined MAC_OSX
/* We also need to be able to specify mult-of-8 alignment on static vars.  */
# if defined DECL_ALIGN
/* We currently do not support USE_LSB_TAG with a union Lisp_Object.  */
#  if defined NO_UNION_TYPE
#   define USE_LSB_TAG
#  endif
# endif
#endif

/* If we cannot use 8-byte alignment, make DECL_ALIGN a no-op.  */
#ifndef DECL_ALIGN
# ifdef USE_LSB_TAG
#  error "USE_LSB_TAG used without defining DECL_ALIGN"
# endif
# define DECL_ALIGN(type, var) type var
#endif


/* These macros extract various sorts of values from a Lisp_Object.
 For example, if tem is a Lisp_Object whose type is Lisp_Cons,
 XCONS (tem) is the struct Lisp_Cons * pointing to the memory for that cons.  */

#ifdef NO_UNION_TYPE

#ifdef USE_LSB_TAG

#define TYPEMASK ((((EMACS_INT) 1) << GCTYPEBITS) - 1)
#define XTYPE(a) ((enum Lisp_Type) (((EMACS_UINT) (a)) & TYPEMASK))
#define XINT(a) (((EMACS_INT) (a)) >> GCTYPEBITS)
#define XUINT(a) (((EMACS_UINT) (a)) >> GCTYPEBITS)
#define XSET(var, type, ptr)					\
    (eassert (XTYPE (ptr) == 0), /* Check alignment.  */	\
     (var) = ((EMACS_INT) (type)) | ((EMACS_INT) (ptr)))
#define make_number(N) (((EMACS_INT) (N)) << GCTYPEBITS)

/* XFASTINT and XSETFASTINT are for use when the integer is known to be
   positive, in which case the implementation can sometimes be faster
   depending on the tagging scheme.  With USE_LSB_TAG, there's no benefit.  */
#define XFASTINT(a) XINT (a)
#define XSETFASTINT(a, b) ((a) = make_number (b))

#define XPNTR(a) ((EMACS_INT) ((a) & ~TYPEMASK))

#else  /* not USE_LSB_TAG */

#define VALMASK ((((EMACS_INT) 1) << VALBITS) - 1)

/* One need to override this if there must be high bits set in data space
   (doing the result of the below & ((1 << (GCTYPE + 1)) - 1) would work
    on all machines, but would penalize machines which don't need it)
 */
#define XTYPE(a) ((enum Lisp_Type) (((EMACS_UINT) (a)) >> VALBITS))

/* For integers known to be positive, XFASTINT provides fast retrieval
   and XSETFASTINT provides fast storage.  This takes advantage of the
   fact that Lisp_Int is 0.  */
#define XFASTINT(a) ((a) + 0)
#define XSETFASTINT(a, b) ((a) = (b))

/* Extract the value of a Lisp_Object as a signed integer.  */

#ifndef XINT   /* Some machines need to do this differently.  */
#define XINT(a) ((((EMACS_INT) (a)) << (BITS_PER_EMACS_INT - VALBITS))	\
		 >> (BITS_PER_EMACS_INT - VALBITS))
#endif

/* Extract the value as an unsigned integer.  This is a basis
   for extracting it as a pointer to a structure in storage.  */

#ifndef XUINT
#define XUINT(a) ((EMACS_UINT) ((a) & VALMASK))
#endif

#ifndef XSET
#define XSET(var, type, ptr) \
   ((var) = ((EMACS_INT)(type) << VALBITS) + ((EMACS_INT) (ptr) & VALMASK))
#endif

/* Convert a C integer into a Lisp_Object integer.  */

#define make_number(N)		\
  ((((EMACS_INT) (N)) & VALMASK) | ((EMACS_INT) Lisp_Int) << VALBITS)

#endif /* not USE_LSB_TAG */

#define EQ(x, y) ((x) == (y))

#else /* not NO_UNION_TYPE */

#define XTYPE(a) ((enum Lisp_Type) (a).u.type)

/* For integers known to be positive, XFASTINT provides fast retrieval
   and XSETFASTINT provides fast storage.  This takes advantage of the
   fact that Lisp_Int is 0.  */
#define XFASTINT(a) ((a).i + 0)
#define XSETFASTINT(a, b) ((a).i = (b))

#ifdef EXPLICIT_SIGN_EXTEND
/* Make sure we sign-extend; compilers have been known to fail to do so.  */
#define XINT(a) (((a).s.val << (BITS_PER_EMACS_INT - VALBITS)) \
		 >> (BITS_PER_EMACS_INT - VALBITS))
#else
#define XINT(a) ((a).s.val)
#endif /* EXPLICIT_SIGN_EXTEND */

#define XUINT(a) ((a).u.val)

#define XSET(var, vartype, ptr) \
   (((var).s.val = ((EMACS_INT) (ptr))), ((var).s.type = ((char) (vartype))))

#if __GNUC__ >= 2 && defined (__OPTIMIZE__)
#define make_number(N) \
  (__extension__ ({ Lisp_Object _l; _l.s.val = (N); _l.s.type = Lisp_Int; _l; }))
#else
extern Lisp_Object make_number P_ ((EMACS_INT));
#endif

#define EQ(x, y) ((x).i == (y).i)

#endif /* NO_UNION_TYPE */

/* During garbage collection, XGCTYPE must be used for extracting types
 so that the mark bit is ignored.  XMARKBIT accesses the markbit.
 Markbits are used only in particular slots of particular structure types.
 Other markbits are always zero.
 Outside of garbage collection, all mark bits are always zero.  */

#ifndef XGCTYPE
/* The distinction does not exist now that the MARKBIT has been eliminated.  */
#define XGCTYPE(a) XTYPE (a)
#endif

#ifndef XPNTR
#ifdef HAVE_SHM
/* In this representation, data is found in two widely separated segments.  */
extern size_t pure_size;
#define XPNTR(a) \
  (XUINT (a) | (XUINT (a) > pure_size ? DATA_SEG_BITS : PURE_SEG_BITS))
#else /* not HAVE_SHM */
#ifdef DATA_SEG_BITS
/* This case is used for the rt-pc.
   In the diffs I was given, it checked for ptr = 0
   and did not adjust it in that case.
   But I don't think that zero should ever be found
   in a Lisp object whose data type says it points to something.  */
#define XPNTR(a) (XUINT (a) | DATA_SEG_BITS)
#else
/* Some versions of gcc seem to consider the bitfield width when
   issuing the "cast to pointer from integer of different size"
   warning, so the cast is here to widen the value back to its natural
   size.  */
#define XPNTR(a) ((EMACS_INT) XUINT (a))
#endif
#endif /* not HAVE_SHM */
#endif /* no XPNTR */

/* Largest and smallest representable fixnum values.  These are the C
   values.  */

#define MOST_NEGATIVE_FIXNUM	- ((EMACS_INT) 1 << (VALBITS - 1))
#define MOST_POSITIVE_FIXNUM	(((EMACS_INT) 1 << (VALBITS - 1)) - 1)
/* Mask indicating the significant bits of a Lisp_Int.
   I.e. (x & INTMASK) == XUINT (make_number (x)).  */
#define INTMASK ((((EMACS_INT) 1) << VALBITS) - 1)

/* Value is non-zero if C integer I doesn't fit into a Lisp fixnum.  */

#define FIXNUM_OVERFLOW_P(i) \
  ((EMACS_INT)(i) > MOST_POSITIVE_FIXNUM \
   || (EMACS_INT) (i) < MOST_NEGATIVE_FIXNUM)

/* Extract a value or address from a Lisp_Object.  */

#define XCONS(a) (eassert (GC_CONSP(a)),(struct Lisp_Cons *) XPNTR(a))
#define XVECTOR(a) (eassert (GC_VECTORLIKEP(a)),(struct Lisp_Vector *) XPNTR(a))
#define XSTRING(a) (eassert (GC_STRINGP(a)),(struct Lisp_String *) XPNTR(a))
#define XSYMBOL(a) (eassert (GC_SYMBOLP(a)),(struct Lisp_Symbol *) XPNTR(a))
#define XFLOAT(a) (eassert (GC_FLOATP(a)),(struct Lisp_Float *) XPNTR(a))

/* Misc types.  */

#define XMISC(a)   ((union Lisp_Misc *) XPNTR(a))
#define XMISCTYPE(a)   (XMARKER (a)->type)
#define XMARKER(a) (&(XMISC(a)->u_marker))
#define XINTFWD(a) (&(XMISC(a)->u_intfwd))
#define XBOOLFWD(a) (&(XMISC(a)->u_boolfwd))
#define XOBJFWD(a) (&(XMISC(a)->u_objfwd))
#define XBUFFER_OBJFWD(a) (&(XMISC(a)->u_buffer_objfwd))
#define XBUFFER_LOCAL_VALUE(a) (&(XMISC(a)->u_buffer_local_value))
#define XOVERLAY(a) (&(XMISC(a)->u_overlay))
#define XKBOARD_OBJFWD(a) (&(XMISC(a)->u_kboard_objfwd))
#define XSAVE_VALUE(a) (&(XMISC(a)->u_save_value))

/* Pseudovector types.  */

#define XPROCESS(a) (eassert (GC_PROCESSP(a)),(struct Lisp_Process *) XPNTR(a))
#define XWINDOW(a) (eassert (GC_WINDOWP(a)),(struct window *) XPNTR(a))
#define XSUBR(a) (eassert (GC_SUBRP(a)),(struct Lisp_Subr *) XPNTR(a))
#define XBUFFER(a) (eassert (GC_BUFFERP(a)),(struct buffer *) XPNTR(a))
#define XCHAR_TABLE(a) ((struct Lisp_Char_Table *) XPNTR(a))
#define XBOOL_VECTOR(a) ((struct Lisp_Bool_Vector *) XPNTR(a))

/* Construct a Lisp_Object from a value or address.  */

#define XSETINT(a, b) (a) = make_number (b)
#define XSETCONS(a, b) XSET (a, Lisp_Cons, b)
#define XSETVECTOR(a, b) XSET (a, Lisp_Vectorlike, b)
#define XSETSTRING(a, b) XSET (a, Lisp_String, b)
#define XSETSYMBOL(a, b) XSET (a, Lisp_Symbol, b)
#define XSETFLOAT(a, b) XSET (a, Lisp_Float, b)

/* Misc types.  */

#define XSETMISC(a, b) XSET (a, Lisp_Misc, b)
#define XSETMARKER(a, b) (XSETMISC (a, b), XMISCTYPE (a) = Lisp_Misc_Marker)

/* Pseudovector types.  */

#define XSETPSEUDOVECTOR(a, b, code) \
  (XSETVECTOR (a, b), XVECTOR (a)->size |= PSEUDOVECTOR_FLAG | (code))
#define XSETWINDOW_CONFIGURATION(a, b) \
  (XSETPSEUDOVECTOR (a, b, PVEC_WINDOW_CONFIGURATION))
#define XSETPROCESS(a, b) (XSETPSEUDOVECTOR (a, b, PVEC_PROCESS))
#define XSETWINDOW(a, b) (XSETPSEUDOVECTOR (a, b, PVEC_WINDOW))
#define XSETSUBR(a, b) (XSETPSEUDOVECTOR (a, b, PVEC_SUBR))
#define XSETCOMPILED(a, b) (XSETPSEUDOVECTOR (a, b, PVEC_COMPILED))
#define XSETBUFFER(a, b) (XSETPSEUDOVECTOR (a, b, PVEC_BUFFER))
#define XSETCHAR_TABLE(a, b) (XSETPSEUDOVECTOR (a, b, PVEC_CHAR_TABLE))
#define XSETBOOL_VECTOR(a, b) (XSETPSEUDOVECTOR (a, b, PVEC_BOOL_VECTOR))

/* Convenience macros for dealing with Lisp arrays.  */

#define AREF(ARRAY, IDX)	XVECTOR ((ARRAY))->contents[IDX]
#define ASET(ARRAY, IDX, VAL)	(AREF ((ARRAY), (IDX)) = (VAL))
#define ASIZE(ARRAY)		XVECTOR ((ARRAY))->size

/* Convenience macros for dealing with Lisp strings.  */

#define SREF(string, index)	(XSTRING (string)->data[index] + 0)
#define SSET(string, index, new) (XSTRING (string)->data[index] = (new))
#define SDATA(string)		(XSTRING (string)->data + 0)
#define SCHARS(string)		(XSTRING (string)->size + 0)
#define SBYTES(string)		(STRING_BYTES (XSTRING (string)) + 0)

#define STRING_SET_CHARS(string, newsize) \
    (XSTRING (string)->size = (newsize))

#define STRING_COPYIN(string, index, new, count) \
    bcopy (new, XSTRING (string)->data + index, count)

/* Type checking.  */

#define CHECK_TYPE(ok, Qxxxp, x) \
  do { if (!(ok)) wrong_type_argument (Qxxxp, (x)); } while (0)



/* See the macros in intervals.h.  */

typedef struct interval *INTERVAL;

/* Complain if object is not string or buffer type */
#define CHECK_STRING_OR_BUFFER(x) \
  CHECK_TYPE (STRINGP (x) || BUFFERP (x), Qbuffer_or_string_p, x)


/* In a cons, the markbit of the car is the gc mark bit */

struct Lisp_Cons
  {
    /* Please do not use the names of these elements in code other
       than the core lisp implementation.  Use XCAR and XCDR below.  */
#ifdef HIDE_LISP_IMPLEMENTATION
    Lisp_Object car_;
    union
    {
      Lisp_Object cdr_;
      struct Lisp_Cons *chain;
    } u;
#else
    Lisp_Object car;
    union
    {
      Lisp_Object cdr;
      struct Lisp_Cons *chain;
    } u;
#endif
  };

/* Take the car or cdr of something known to be a cons cell.  */
/* The _AS_LVALUE macros shouldn't be used outside of the minimal set
   of code that has to know what a cons cell looks like.  Other code not
   part of the basic lisp implementation should assume that the car and cdr
   fields are not accessible as lvalues.  (What if we want to switch to
   a copying collector someday?  Cached cons cell field addresses may be
   invalidated at arbitrary points.)  */
#ifdef HIDE_LISP_IMPLEMENTATION
#define XCAR_AS_LVALUE(c) (XCONS ((c))->car_)
#define XCDR_AS_LVALUE(c) (XCONS ((c))->u.cdr_)
#else
#define XCAR_AS_LVALUE(c) (XCONS ((c))->car)
#define XCDR_AS_LVALUE(c) (XCONS ((c))->u.cdr)
#endif

/* Use these from normal code.  */
#define XCAR(c)	LISP_MAKE_RVALUE(XCAR_AS_LVALUE(c))
#define XCDR(c) LISP_MAKE_RVALUE(XCDR_AS_LVALUE(c))

/* Use these to set the fields of a cons cell.

   Note that both arguments may refer to the same object, so 'n'
   should not be read after 'c' is first modified.  Also, neither
   argument should be evaluated more than once; side effects are
   especially common in the second argument.  */
#define XSETCAR(c,n) (XCAR_AS_LVALUE(c) = (n))
#define XSETCDR(c,n) (XCDR_AS_LVALUE(c) = (n))

/* For performance: Fast storage of positive integers into the
   fields of a cons cell.  See above caveats.  */
#define XSETCARFASTINT(c,n)  XSETFASTINT(XCAR_AS_LVALUE(c),(n))
#define XSETCDRFASTINT(c,n)  XSETFASTINT(XCDR_AS_LVALUE(c),(n))

/* Take the car or cdr of something whose type is not known.  */
#define CAR(c)					\
 (CONSP ((c)) ? XCAR ((c))			\
  : NILP ((c)) ? Qnil				\
  : wrong_type_argument (Qlistp, (c)))

#define CDR(c)					\
 (CONSP ((c)) ? XCDR ((c))			\
  : NILP ((c)) ? Qnil				\
  : wrong_type_argument (Qlistp, (c)))

/* Take the car or cdr of something whose type is not known.  */
#define CAR_SAFE(c)				\
  (CONSP ((c)) ? XCAR ((c)) : Qnil)

#define CDR_SAFE(c)				\
  (CONSP ((c)) ? XCDR ((c)) : Qnil)

/* Nonzero if STR is a multibyte string.  */
#define STRING_MULTIBYTE(STR)  \
  (XSTRING (STR)->size_byte >= 0)

/* Return the length in bytes of STR.  */

#ifdef GC_CHECK_STRING_BYTES

struct Lisp_String;
extern int string_bytes P_ ((struct Lisp_String *));
#define STRING_BYTES(S) string_bytes ((S))

#else /* not GC_CHECK_STRING_BYTES */

#define STRING_BYTES(STR)  \
  ((STR)->size_byte < 0 ? (STR)->size : (STR)->size_byte)

#endif /* not GC_CHECK_STRING_BYTES */

/* Mark STR as a unibyte string.  */
#define STRING_SET_UNIBYTE(STR)      (XSTRING (STR)->size_byte = -1)

/* Get text properties.  */
#define STRING_INTERVALS(STR)  (XSTRING (STR)->intervals + 0)

/* Set text properties.  */
#define STRING_SET_INTERVALS(STR, INT) (XSTRING (STR)->intervals = (INT))

/* In a string or vector, the sign bit of the `size' is the gc mark bit */

struct Lisp_String
  {
    EMACS_INT size;
    EMACS_INT size_byte;
    INTERVAL intervals;		/* text properties in this string */
    unsigned char *data;
  };

#ifdef offsetof
#define OFFSETOF(type,field) offsetof(type,field)
#else
#define OFFSETOF(type,field) \
  ((int)((char*)&((type*)0)->field - (char*)0))
#endif

struct Lisp_Vector
  {
    EMACS_INT size;
    struct Lisp_Vector *next;
    Lisp_Object contents[1];
  };

/* If a struct is made to look like a vector, this macro returns the length
   of the shortest vector that would hold that struct.  */
#define VECSIZE(type) ((sizeof (type) - (sizeof (struct Lisp_Vector)  \
                                         - sizeof (Lisp_Object))      \
                        + sizeof(Lisp_Object) - 1) /* round up */     \
		       / sizeof (Lisp_Object))

/* Like VECSIZE, but used when the pseudo-vector has non-Lisp_Object fields
   at the end and we need to compute the number of Lisp_Object fields (the
   ones that the GC needs to trace).  */
#define PSEUDOVECSIZE(type, nonlispfield) \
  ((OFFSETOF(type, nonlispfield) - OFFSETOF(struct Lisp_Vector, contents[0])) \
   / sizeof (Lisp_Object))

/* A char table is a kind of vectorlike, with contents are like a
   vector but with a few other slots.  For some purposes, it makes
   sense to handle a chartable with type struct Lisp_Vector.  An
   element of a char table can be any Lisp objects, but if it is a sub
   char-table, we treat it a table that contains information of a
   group of characters of the same charsets or a specific character of
   a charset.  A sub char-table has the same structure as a char table
   except for that the former omits several slots at the tail.  A sub
   char table appears only in an element of a char table, and there's
   no way to access it directly from Emacs Lisp program.  */

/* This is the number of slots that apply to characters or character
   sets.  The first 128 are for ASCII, the next 128 are for 8-bit
   European characters, and the last 128 are for multibyte characters.
   The first 256 are indexed by the code itself, but the last 128 are
   indexed by (charset-id + 128).  */
#define CHAR_TABLE_ORDINARY_SLOTS 384

/* These are the slot of the default values for single byte
   characters.  As 0x9A is never be a charset-id, it is safe to use
   that slot for ASCII.  0x9E and 0x80 are charset-ids of
   eight-bit-control and eight-bit-graphic respectively.  */
#define CHAR_TABLE_DEFAULT_SLOT_ASCII (0x9A + 128)
#define CHAR_TABLE_DEFAULT_SLOT_8_BIT_CONTROL (0x9E + 128)
#define CHAR_TABLE_DEFAULT_SLOT_8_BIT_GRAPHIC (0x80 + 128)

/* This is the number of slots that apply to characters of ASCII and
   8-bit Europeans only.  */
#define CHAR_TABLE_SINGLE_BYTE_SLOTS 256

/* This is the number of slots that every char table must have.  This
   counts the ordinary slots and the top, defalt, parent, and purpose
   slots.  */
#define CHAR_TABLE_STANDARD_SLOTS (CHAR_TABLE_ORDINARY_SLOTS + 4)

/* This is the number of slots that apply to position-code-1 and
   position-code-2 of a multibyte character at the 2nd and 3rd level
   sub char tables respectively.  */
#define SUB_CHAR_TABLE_ORDINARY_SLOTS 128

/* This is the number of slots that every sub char table must have.
   This counts the ordinary slots and the top and defalt slot.  */
#define SUB_CHAR_TABLE_STANDARD_SLOTS (SUB_CHAR_TABLE_ORDINARY_SLOTS + 2)

/* Return the number of "extra" slots in the char table CT.  */

#define CHAR_TABLE_EXTRA_SLOTS(CT)	\
  (((CT)->size & PSEUDOVECTOR_SIZE_MASK) - CHAR_TABLE_STANDARD_SLOTS)

/* Almost equivalent to Faref (CT, IDX) with optimization for ASCII
   and 8-bit Europeans characters.  For these characters, do not check
   validity of CT.  Do not follow parent.  */
#define CHAR_TABLE_REF(CT, IDX)				\
  ((IDX) >= 0 && (IDX) < CHAR_TABLE_SINGLE_BYTE_SLOTS	\
   ? (!NILP (XCHAR_TABLE (CT)->contents[IDX])		\
      ? XCHAR_TABLE (CT)->contents[IDX]			\
      : XCHAR_TABLE (CT)->defalt)			\
   : Faref (CT, make_number (IDX)))

/* Almost equivalent to Faref (CT, IDX) with optimization for ASCII
   and 8-bit Europeans characters.  However, if the result is nil,
   return IDX.

   For these characters, do not check validity of CT
   and do not follow parent.  */
#define CHAR_TABLE_TRANSLATE(CT, IDX)			\
  ((IDX) < CHAR_TABLE_SINGLE_BYTE_SLOTS			\
   ? (!NILP (XCHAR_TABLE (CT)->contents[IDX])		\
      ? XINT (XCHAR_TABLE (CT)->contents[IDX])		\
      : IDX)						\
   : char_table_translate (CT, IDX))

/* Equivalent to Faset (CT, IDX, VAL) with optimization for ASCII and
   8-bit Europeans characters.  Do not check validity of CT.  */
#define CHAR_TABLE_SET(CT, IDX, VAL)			\
  do {							\
    if (XFASTINT (IDX) < CHAR_TABLE_SINGLE_BYTE_SLOTS)	\
      XCHAR_TABLE (CT)->contents[XFASTINT (IDX)] = VAL;	\
    else						\
      Faset (CT, IDX, VAL);				\
  } while (0)

struct Lisp_Char_Table
  {
    /* This is the vector's size field, which also holds the
       pseudovector type information.  It holds the size, too.
       The size counts the top, defalt, purpose, and parent slots.
       The last three are not counted if this is a sub char table.  */
    EMACS_INT size;
    struct Lisp_Vector *next;
    /* This holds a flag to tell if this is a top level char table (t)
       or a sub char table (nil).  */
    Lisp_Object top;
    /* This holds a default value,
       which is used whenever the value for a specific character is nil.  */
    Lisp_Object defalt;
    /* This holds an actual value of each element.  A sub char table
       has only SUB_CHAR_TABLE_ORDINARY_SLOTS number of elements.  */
    Lisp_Object contents[CHAR_TABLE_ORDINARY_SLOTS];

    /* A sub char table doesn't has the following slots.  */

    /* This points to another char table, which we inherit from
       when the value for a specific character is nil.
       The `defalt' slot takes precedence over this.  */
    Lisp_Object parent;
    /* This should be a symbol which says what kind of use
       this char-table is meant for.
       Typically now the values can be `syntax-table' and `display-table'.  */
    Lisp_Object purpose;
    /* These hold additional data.  */
    Lisp_Object extras[1];
  };

/* A boolvector is a kind of vectorlike, with contents are like a string.  */
struct Lisp_Bool_Vector
  {
    /* This is the vector's size field.  It doesn't have the real size,
       just the subtype information.  */
    EMACS_INT vector_size;
    struct Lisp_Vector *next;
    /* This is the size in bits.  */
    EMACS_INT size;
    /* This contains the actual bits, packed into bytes.  */
    unsigned char data[1];
  };

/* This structure describes a built-in function.
   It is generated by the DEFUN macro only.
   defsubr makes it into a Lisp object.

   This type is treated in most respects as a pseudovector,
   but since we never dynamically allocate or free them,
   we don't need a next-vector field.  */

struct Lisp_Subr
  {
    EMACS_INT size;
    Lisp_Object (*function) ();
    short min_args, max_args;
    char *symbol_name;
    char *prompt;
    char *doc;
  };


/***********************************************************************
			       Symbols
 ***********************************************************************/

/* Interned state of a symbol.  */

enum symbol_interned
{
  SYMBOL_UNINTERNED = 0,
  SYMBOL_INTERNED = 1,
  SYMBOL_INTERNED_IN_INITIAL_OBARRAY = 2
};

/* In a symbol, the markbit of the plist is used as the gc mark bit */

struct Lisp_Symbol
{
  unsigned gcmarkbit : 1;

  /* Non-zero means symbol serves as a variable alias.  The symbol
     holding the real value is found in the value slot.  */
  unsigned indirect_variable : 1;

  /* Non-zero means symbol is constant, i.e. changing its value
     should signal an error.  */
  unsigned constant : 1;

  /* Interned state of the symbol.  This is an enumerator from
     enum symbol_interned.  */
  unsigned interned : 2;

  /* The symbol's name, as a Lisp string.

     The name "xname" is used to intentionally break code referring to
     the old field "name" of type pointer to struct Lisp_String.  */
  Lisp_Object xname;

  /* Value of the symbol or Qunbound if unbound.  If this symbol is a
     defvaralias, `value' contains the symbol for which it is an
     alias.  Use the SYMBOL_VALUE and SET_SYMBOL_VALUE macros to get
     and set a symbol's value, to take defvaralias into account.  */
  Lisp_Object value;

  /* Function value of the symbol or Qunbound if not fboundp.  */
  Lisp_Object function;

  /* The symbol's property list.  */
  Lisp_Object plist;

  /* Next symbol in obarray bucket, if the symbol is interned.  */
  struct Lisp_Symbol *next;
};

/* Value is name of symbol.  */

#define SYMBOL_NAME(sym)  \
     LISP_MAKE_RVALUE (XSYMBOL (sym)->xname)

/* Value is non-zero if SYM is an interned symbol.  */

#define SYMBOL_INTERNED_P(sym)  \
     (XSYMBOL (sym)->interned != SYMBOL_UNINTERNED)

/* Value is non-zero if SYM is interned in initial_obarray.  */

#define SYMBOL_INTERNED_IN_INITIAL_OBARRAY_P(sym) \
     (XSYMBOL (sym)->interned == SYMBOL_INTERNED_IN_INITIAL_OBARRAY)

/* Value is non-zero if symbol is considered a constant, i.e. its
   value cannot be changed (there is an exception for keyword symbols,
   whose value can be set to the keyword symbol itself).  */

#define SYMBOL_CONSTANT_P(sym)   XSYMBOL (sym)->constant

/* Value is the value of SYM, with defvaralias taken into
   account.  */

#define SYMBOL_VALUE(sym)			\
   (XSYMBOL (sym)->indirect_variable		\
    ? XSYMBOL (indirect_variable (sym))->value	\
    : XSYMBOL (sym)->value)

/* Set SYM's value to VAL, taking defvaralias into account.  */

#define SET_SYMBOL_VALUE(sym, val)				\
     do {							\
       if (XSYMBOL (sym)->indirect_variable)			\
	 XSYMBOL (indirect_variable ((sym)))->value = (val);	\
       else							\
	 XSYMBOL (sym)->value = (val);				\
     } while (0)


/***********************************************************************
			     Hash Tables
 ***********************************************************************/

/* The structure of a Lisp hash table.  */

struct Lisp_Hash_Table
{
  /* Vector fields.  The hash table code doesn't refer to these.  */
  EMACS_INT size;
  struct Lisp_Vector *vec_next;

  /* Function used to compare keys.  */
  Lisp_Object test;

  /* Nil if table is non-weak.  Otherwise a symbol describing the
     weakness of the table.  */
  Lisp_Object weak;

  /* When the table is resized, and this is an integer, compute the
     new size by adding this to the old size.  If a float, compute the
     new size by multiplying the old size with this factor.  */
  Lisp_Object rehash_size;

  /* Resize hash table when number of entries/ table size is >= this
     ratio, a float.  */
  Lisp_Object rehash_threshold;

  /* Number of key/value entries in the table.  */
  Lisp_Object count;

  /* Vector of keys and values.  The key of item I is found at index
     2 * I, the value is found at index 2 * I + 1.  */
  Lisp_Object key_and_value;

  /* Vector of hash codes.. If hash[I] is nil, this means that that
     entry I is unused.  */
  Lisp_Object hash;

  /* Vector used to chain entries.  If entry I is free, next[I] is the
     entry number of the next free item.  If entry I is non-free,
     next[I] is the index of the next entry in the collision chain.  */
  Lisp_Object next;

  /* Index of first free entry in free list.  */
  Lisp_Object next_free;

  /* Bucket vector.  A non-nil entry is the index of the first item in
     a collision chain.  This vector's size can be larger than the
     hash table size to reduce collisions.  */
  Lisp_Object index;

  /* Next weak hash table if this is a weak hash table.  The head
     of the list is in Vweak_hash_tables.  */
  Lisp_Object next_weak;

  /* User-supplied hash function, or nil.  */
  Lisp_Object user_hash_function;

  /* User-supplied key comparison function, or nil.  */
  Lisp_Object user_cmp_function;

  /* C function to compare two keys.  */
  int (* cmpfn) P_ ((struct Lisp_Hash_Table *, Lisp_Object,
		     unsigned, Lisp_Object, unsigned));

  /* C function to compute hash code.  */
  unsigned (* hashfn) P_ ((struct Lisp_Hash_Table *, Lisp_Object));
};


#define XHASH_TABLE(OBJ) \
     ((struct Lisp_Hash_Table *) XPNTR (OBJ))

#define XSET_HASH_TABLE(VAR, PTR) \
     (XSETPSEUDOVECTOR (VAR, PTR, PVEC_HASH_TABLE))

#define HASH_TABLE_P(OBJ)  PSEUDOVECTORP (OBJ, PVEC_HASH_TABLE)
#define GC_HASH_TABLE_P(x) GC_PSEUDOVECTORP (x, PVEC_HASH_TABLE)

#define CHECK_HASH_TABLE(x) \
  CHECK_TYPE (HASH_TABLE_P (x), Qhash_table_p, x)

/* Value is the key part of entry IDX in hash table H.  */

#define HASH_KEY(H, IDX)   AREF ((H)->key_and_value, 2 * (IDX))

/* Value is the value part of entry IDX in hash table H.  */

#define HASH_VALUE(H, IDX) AREF ((H)->key_and_value, 2 * (IDX) + 1)

/* Value is the index of the next entry following the one at IDX
   in hash table H.  */

#define HASH_NEXT(H, IDX)  AREF ((H)->next, (IDX))

/* Value is the hash code computed for entry IDX in hash table H.  */

#define HASH_HASH(H, IDX)  AREF ((H)->hash, (IDX))

/* Value is the index of the element in hash table H that is the
   start of the collision list at index IDX in the index vector of H.  */

#define HASH_INDEX(H, IDX)  AREF ((H)->index, (IDX))

/* Value is the size of hash table H.  */

#define HASH_TABLE_SIZE(H) XVECTOR ((H)->next)->size

/* Default size for hash tables if not specified.  */

#define DEFAULT_HASH_SIZE 65

/* Default threshold specifying when to resize a hash table.  The
   value gives the ratio of current entries in the hash table and the
   size of the hash table.  */

#define DEFAULT_REHASH_THRESHOLD 0.8

/* Default factor by which to increase the size of a hash table.  */

#define DEFAULT_REHASH_SIZE 1.5


/* These structures are used for various misc types.  */

struct Lisp_Marker
{
  int type : 16;		/* = Lisp_Misc_Marker */
  unsigned gcmarkbit : 1;
  int spacer : 14;
  /* 1 means normal insertion at the marker's position
     leaves the marker after the inserted text.  */
  unsigned int insertion_type : 1;
  /* This is the buffer that the marker points into,
     or 0 if it points nowhere.  */
  struct buffer *buffer;

  /* The remaining fields are meaningless in a marker that
     does not point anywhere.  */

  /* For markers that point somewhere,
     this is used to chain of all the markers in a given buffer.  */
  struct Lisp_Marker *next;
  /* This is the char position where the marker points.  */
  EMACS_INT charpos;
  /* This is the byte position.  */
  EMACS_INT bytepos;
};

/* Forwarding pointer to an int variable.
   This is allowed only in the value cell of a symbol,
   and it means that the symbol's value really lives in the
   specified int variable.  */
struct Lisp_Intfwd
  {
    int type : 16;	/* = Lisp_Misc_Intfwd */
    unsigned gcmarkbit : 1;
    int spacer : 15;
    EMACS_INT *intvar;
  };

/* Boolean forwarding pointer to an int variable.
   This is like Lisp_Intfwd except that the ostensible
   "value" of the symbol is t if the int variable is nonzero,
   nil if it is zero.  */
struct Lisp_Boolfwd
  {
    int type : 16;	/* = Lisp_Misc_Boolfwd */
    unsigned gcmarkbit : 1;
    int spacer : 15;
    int *boolvar;
  };

/* Forwarding pointer to a Lisp_Object variable.
   This is allowed only in the value cell of a symbol,
   and it means that the symbol's value really lives in the
   specified variable.  */
struct Lisp_Objfwd
  {
    int type : 16;	/* = Lisp_Misc_Objfwd */
    unsigned gcmarkbit : 1;
    int spacer : 15;
    Lisp_Object *objvar;
  };

/* Like Lisp_Objfwd except that value lives in a slot in the
   current buffer.  Value is byte index of slot within buffer.  */
struct Lisp_Buffer_Objfwd
  {
    int type : 16;	/* = Lisp_Misc_Buffer_Objfwd */
    unsigned gcmarkbit : 1;
    int spacer : 15;
    int offset;
  };

/* struct Lisp_Buffer_Local_Value is used in a symbol value cell when
   the symbol has buffer-local or frame-local bindings.  (Exception:
   some buffer-local variables are built-in, with their values stored
   in the buffer structure itself.  They are handled differently,
   using struct Lisp_Buffer_Objfwd.)

   The `realvalue' slot holds the variable's current value, or a
   forwarding pointer to where that value is kept.  This value is the
   one that corresponds to the loaded binding.  To read or set the
   variable, you must first make sure the right binding is loaded;
   then you can access the value in (or through) `realvalue'.

   `buffer' and `frame' are the buffer and frame for which the loaded
   binding was found.  If those have changed, to make sure the right
   binding is loaded it is necessary to find which binding goes with
   the current buffer and selected frame, then load it.  To load it,
   first unload the previous binding, then copy the value of the new
   binding into `realvalue' (or through it).  Also update
   LOADED-BINDING to point to the newly loaded binding.

   Lisp_Misc_Buffer_Local_Value and Lisp_Misc_Some_Buffer_Local_Value
   both use this kind of structure.  With the former, merely setting
   the variable creates a local binding for the current buffer.  With
   the latter, setting the variable does not do that; only
   make-local-variable does that.  */

struct Lisp_Buffer_Local_Value
  {
    int type : 16;      /* = Lisp_Misc_Buffer_Local_Value
			   or Lisp_Misc_Some_Buffer_Local_Value */
    unsigned gcmarkbit : 1;
    int spacer : 12;

    /* 1 means this variable is allowed to have frame-local bindings,
       so check for them when looking for the proper binding.  */
    unsigned int check_frame : 1;
    /* 1 means that the binding now loaded was found
       as a local binding for the buffer in the `buffer' slot.  */
    unsigned int found_for_buffer : 1;
    /* 1 means that the binding now loaded was found
       as a local binding for the frame in the `frame' slot.  */
    unsigned int found_for_frame : 1;
    Lisp_Object realvalue;
    /* The buffer and frame for which the loaded binding was found.  */
    Lisp_Object buffer, frame;

    /* A cons cell, (LOADED-BINDING . DEFAULT-VALUE).

       LOADED-BINDING is the binding now loaded.  It is a cons cell
       whose cdr is the binding's value.  The cons cell may be an
       element of a buffer's local-variable alist, or an element of a
       frame's parameter alist, or it may be this cons cell.

       DEFAULT-VALUE is the variable's default value, seen when the
       current buffer and selected frame do not have their own
       bindings for the variable.  When the default binding is loaded,
       LOADED-BINDING is actually this very cons cell; thus, its car
       points to itself.  */
    Lisp_Object cdr;
  };

/* START and END are markers in the overlay's buffer, and
   PLIST is the overlay's property list.  */
struct Lisp_Overlay
  {
    int type : 16;	/* = Lisp_Misc_Overlay */
    unsigned gcmarkbit : 1;
    int spacer : 15;
    struct Lisp_Overlay *next;
    Lisp_Object start, end, plist;
  };

/* Like Lisp_Objfwd except that value lives in a slot in the
   current kboard.  */
struct Lisp_Kboard_Objfwd
  {
    int type : 16;	/* = Lisp_Misc_Kboard_Objfwd */
    unsigned gcmarkbit : 1;
    int spacer : 15;
    int offset;
  };

/* Hold a C pointer for later use.
   This type of object is used in the arg to record_unwind_protect.  */
struct Lisp_Save_Value
  {
    int type : 16;	/* = Lisp_Misc_Save_Value */
    unsigned gcmarkbit : 1;
    int spacer : 14;
    /* If DOGC is set, POINTER is the address of a memory
       area containing INTEGER potential Lisp_Objects.  */
    unsigned int dogc : 1;
    void *pointer;
    int integer;
  };


/* A miscellaneous object, when it's on the free list.  */
struct Lisp_Free
  {
    int type : 16;	/* = Lisp_Misc_Free */
    unsigned gcmarkbit : 1;
    int spacer : 15;
    union Lisp_Misc *chain;
#ifdef USE_LSB_TAG
    /* Try to make sure that sizeof(Lisp_Misc) preserves TYPEBITS-alignment.
       This assumes that Lisp_Marker is the largest of the alternatives and
       that Lisp_Intfwd has the same size as "Lisp_Free w/o padding".  */
    char padding[((((sizeof (struct Lisp_Marker) - 1) >> GCTYPEBITS) + 1)
		  << GCTYPEBITS) - sizeof (struct Lisp_Intfwd)];
#endif
  };

/* To get the type field of a union Lisp_Misc, use XMISCTYPE.
   It uses one of these struct subtypes to get the type field.  */

union Lisp_Misc
  {
    struct Lisp_Free u_free;
    struct Lisp_Marker u_marker;
    struct Lisp_Intfwd u_intfwd;
    struct Lisp_Boolfwd u_boolfwd;
    struct Lisp_Objfwd u_objfwd;
    struct Lisp_Buffer_Objfwd u_buffer_objfwd;
    struct Lisp_Buffer_Local_Value u_buffer_local_value;
    struct Lisp_Overlay u_overlay;
    struct Lisp_Kboard_Objfwd u_kboard_objfwd;
    struct Lisp_Save_Value u_save_value;
  };

/* Lisp floating point type */
struct Lisp_Float
  {
    union
    {
#ifdef HIDE_LISP_IMPLEMENTATION
      double data_;
#else
      double data;
#endif
      struct Lisp_Float *chain;
    } u;
  };

#ifdef HIDE_LISP_IMPLEMENTATION
#define XFLOAT_DATA(f)	(XFLOAT (f)->u.data_)
#else
#define XFLOAT_DATA(f)	(XFLOAT (f)->u.data)
#endif

/* A character, declared with the following typedef, is a member
   of some character set associated with the current buffer.  */
#ifndef _UCHAR_T  /* Protect against something in ctab.h on AIX.  */
#define _UCHAR_T
typedef unsigned char UCHAR;
#endif

/* Meanings of slots in a Lisp_Compiled:  */

#define COMPILED_ARGLIST 0
#define COMPILED_BYTECODE 1
#define COMPILED_CONSTANTS 2
#define COMPILED_STACK_DEPTH 3
#define COMPILED_DOC_STRING 4
#define COMPILED_INTERACTIVE 5

/* Flag bits in a character.  These also get used in termhooks.h.
   Richard Stallman <rms@gnu.ai.mit.edu> thinks that MULE
   (MUlti-Lingual Emacs) might need 22 bits for the character value
   itself, so we probably shouldn't use any bits lower than 0x0400000.  */
#define CHAR_ALT   (0x0400000)
#define CHAR_SUPER (0x0800000)
#define CHAR_HYPER (0x1000000)
#define CHAR_SHIFT (0x2000000)
#define CHAR_CTL   (0x4000000)
#define CHAR_META  (0x8000000)

#define CHAR_MODIFIER_MASK \
  (CHAR_ALT | CHAR_SUPER | CHAR_HYPER  | CHAR_SHIFT | CHAR_CTL | CHAR_META)


/* Actually, the current Emacs uses 19 bits for the character value
   itself.  */
#define CHARACTERBITS 19

/* The maximum byte size consumed by push_key_description.
   All callers should assure that at least this size of memory is
   allocated at the place pointed by the second argument.

   Thers are 6 modifiers, each consumes 2 chars.
   The octal form of a character code consumes
   (1 + CHARACTERBITS / 3 + 1) chars (including backslash at the head).
   We need one more byte for string terminator `\0'.  */
#define KEY_DESCRIPTION_SIZE ((2 * 6) + 1 + (CHARACTERBITS / 3) + 1 + 1)

#ifdef USE_X_TOOLKIT
#ifdef NO_UNION_TYPE
/* Use this for turning a (void *) into a Lisp_Object, as when the
   Lisp_Object is passed into a toolkit callback function.  */
#define VOID_TO_LISP(larg,varg) \
  do { ((larg) = ((Lisp_Object) (varg))); } while (0)
#define CVOID_TO_LISP VOID_TO_LISP

/* Use this for turning a Lisp_Object into a  (void *), as when the
   Lisp_Object is passed into a toolkit callback function.  */
#define LISP_TO_VOID(larg) ((void *) (larg))
#define LISP_TO_CVOID(varg) ((const void *) (larg))

#else /* not NO_UNION_TYPE */
/* Use this for turning a (void *) into a Lisp_Object, as when the
  Lisp_Object is passed into a toolkit callback function.  */
#define VOID_TO_LISP(larg,varg) \
  do { ((larg).v = (void *) (varg)); } while (0)
#define CVOID_TO_LISP(larg,varg) \
  do { ((larg).cv = (const void *) (varg)); } while (0)

/* Use this for turning a Lisp_Object into a  (void *), as when the
   Lisp_Object is passed into a toolkit callback function.  */
#define LISP_TO_VOID(larg) ((larg).v)
#define LISP_TO_CVOID(larg) ((larg).cv)
#endif /* not NO_UNION_TYPE */
#endif /* USE_X_TOOLKIT */


/* The glyph datatype, used to represent characters on the display.  */

/* Glyph code to use as an index to the glyph table.  If it is out of
   range for the glyph table, or the corresonding element in the table
   is nil, the low 8 bits are the single byte character code, and the
   bits above are the numeric face ID.  If FID is the face ID of a
   glyph on a frame F, then F->display.x->faces[FID] contains the
   description of that face.  This is an int instead of a short, so we
   can support a good bunch of face ID's (2^(31 - 8)); given that we
   have no mechanism for tossing unused frame face ID's yet, we'll
   probably run out of 255 pretty quickly.
   This is always -1 for a multibyte character.  */
#define GLYPH int

/* Mask bits for face.  */
#define GLYPH_MASK_FACE    0x7FF80000
 /* Mask bits for character code.  */
#define GLYPH_MASK_CHAR    0x0007FFFF /* The lowest 19 bits */

/* The FAST macros assume that we already know we're in an X window.  */

/* Set a character code and a face ID in a glyph G.  */
#define FAST_MAKE_GLYPH(char, face) ((char) | ((face) << CHARACTERBITS))

/* Return a glyph's character code.  */
#define FAST_GLYPH_CHAR(glyph) ((glyph) & GLYPH_MASK_CHAR)

/* Return a glyph's face ID.  */
#define FAST_GLYPH_FACE(glyph) (((glyph) & GLYPH_MASK_FACE) >> CHARACTERBITS)

/* Slower versions that test the frame type first.  */
#define MAKE_GLYPH(f, char, face) (FAST_MAKE_GLYPH (char, face))
#define GLYPH_CHAR(f, g) (FAST_GLYPH_CHAR (g))
#define GLYPH_FACE(f, g) (FAST_GLYPH_FACE (g))

/* Return 1 iff GLYPH contains valid character code.  */
#define GLYPH_CHAR_VALID_P(glyph) CHAR_VALID_P (FAST_GLYPH_CHAR (glyph), 1)

/* The ID of the mode line highlighting face.  */
#define GLYPH_MODE_LINE_FACE 1

/* Data type checking */

#define NILP(x)  EQ (x, Qnil)
#define GC_NILP(x) GC_EQ (x, Qnil)

#define NUMBERP(x) (INTEGERP (x) || FLOATP (x))
#define GC_NUMBERP(x) (GC_INTEGERP (x) || GC_FLOATP (x))
#define NATNUMP(x) (INTEGERP (x) && XINT (x) >= 0)
#define GC_NATNUMP(x) (GC_INTEGERP (x) && XINT (x) >= 0)

#define INTEGERP(x) (XTYPE ((x)) == Lisp_Int)
#define GC_INTEGERP(x) INTEGERP (x)
#define SYMBOLP(x) (XTYPE ((x)) == Lisp_Symbol)
#define GC_SYMBOLP(x) (XGCTYPE ((x)) == Lisp_Symbol)
#define MISCP(x) (XTYPE ((x)) == Lisp_Misc)
#define GC_MISCP(x) (XGCTYPE ((x)) == Lisp_Misc)
#define VECTORLIKEP(x) (XTYPE ((x)) == Lisp_Vectorlike)
#define GC_VECTORLIKEP(x) (XGCTYPE ((x)) == Lisp_Vectorlike)
#define STRINGP(x) (XTYPE ((x)) == Lisp_String)
#define GC_STRINGP(x) (XGCTYPE ((x)) == Lisp_String)
#define CONSP(x) (XTYPE ((x)) == Lisp_Cons)
#define GC_CONSP(x) (XGCTYPE ((x)) == Lisp_Cons)

#define FLOATP(x) (XTYPE ((x)) == Lisp_Float)
#define GC_FLOATP(x) (XGCTYPE ((x)) == Lisp_Float)
#define VECTORP(x) (VECTORLIKEP (x) && !(XVECTOR (x)->size & PSEUDOVECTOR_FLAG))
#define GC_VECTORP(x) (GC_VECTORLIKEP (x) && !(XVECTOR (x)->size & PSEUDOVECTOR_FLAG))
#define OVERLAYP(x) (MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Overlay)
#define GC_OVERLAYP(x) (GC_MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Overlay)
#define MARKERP(x) (MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Marker)
#define GC_MARKERP(x) (GC_MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Marker)
#define INTFWDP(x) (MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Intfwd)
#define GC_INTFWDP(x) (GC_MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Intfwd)
#define BOOLFWDP(x) (MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Boolfwd)
#define GC_BOOLFWDP(x) (GC_MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Boolfwd)
#define OBJFWDP(x) (MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Objfwd)
#define GC_OBJFWDP(x) (GC_MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Objfwd)
#define BUFFER_OBJFWDP(x) (MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Buffer_Objfwd)
#define GC_BUFFER_OBJFWDP(x) (GC_MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Buffer_Objfwd)
#define BUFFER_LOCAL_VALUEP(x) (MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Buffer_Local_Value)
#define GC_BUFFER_LOCAL_VALUEP(x) (GC_MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Buffer_Local_Value)
#define SOME_BUFFER_LOCAL_VALUEP(x) (MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Some_Buffer_Local_Value)
#define GC_SOME_BUFFER_LOCAL_VALUEP(x) (GC_MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Some_Buffer_Local_Value)
#define KBOARD_OBJFWDP(x) (MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Kboard_Objfwd)
#define GC_KBOARD_OBJFWDP(x) (GC_MISCP (x) && XMISCTYPE (x) == Lisp_Misc_Kboard_Objfwd)


/* True if object X is a pseudovector whose code is CODE.  */
#define PSEUDOVECTORP(x, code)					\
  (VECTORLIKEP (x)						\
   && (((XVECTOR (x)->size & (PSEUDOVECTOR_FLAG | (code))))	\
       == (PSEUDOVECTOR_FLAG | (code))))

/* True if object X is a pseudovector whose code is CODE.
   This one works during GC.  */
#define GC_PSEUDOVECTORP(x, code)				\
  (GC_VECTORLIKEP (x)						\
   && (((XVECTOR (x)->size & (PSEUDOVECTOR_FLAG | (code))))	\
       == (PSEUDOVECTOR_FLAG | (code))))

/* Test for specific pseudovector types.  */
#define WINDOW_CONFIGURATIONP(x) PSEUDOVECTORP (x, PVEC_WINDOW_CONFIGURATION)
#define GC_WINDOW_CONFIGURATIONP(x) GC_PSEUDOVECTORP (x, PVEC_WINDOW_CONFIGURATION)
#define PROCESSP(x) PSEUDOVECTORP (x, PVEC_PROCESS)
#define GC_PROCESSP(x) GC_PSEUDOVECTORP (x, PVEC_PROCESS)
#define WINDOWP(x) PSEUDOVECTORP (x, PVEC_WINDOW)
#define GC_WINDOWP(x) GC_PSEUDOVECTORP (x, PVEC_WINDOW)
#define SUBRP(x) PSEUDOVECTORP (x, PVEC_SUBR)
#define GC_SUBRP(x) GC_PSEUDOVECTORP (x, PVEC_SUBR)
#define COMPILEDP(x) PSEUDOVECTORP (x, PVEC_COMPILED)
#define GC_COMPILEDP(x) GC_PSEUDOVECTORP (x, PVEC_COMPILED)
#define BUFFERP(x) PSEUDOVECTORP (x, PVEC_BUFFER)
#define GC_BUFFERP(x) GC_PSEUDOVECTORP (x, PVEC_BUFFER)
#define CHAR_TABLE_P(x) PSEUDOVECTORP (x, PVEC_CHAR_TABLE)
#define GC_CHAR_TABLE_P(x) GC_PSEUDOVECTORP (x, PVEC_CHAR_TABLE)
#define BOOL_VECTOR_P(x) PSEUDOVECTORP (x, PVEC_BOOL_VECTOR)
#define GC_BOOL_VECTOR_P(x) GC_PSEUDOVECTORP (x, PVEC_BOOL_VECTOR)
#define FRAMEP(x) PSEUDOVECTORP (x, PVEC_FRAME)
#define GC_FRAMEP(x) GC_PSEUDOVECTORP (x, PVEC_FRAME)

#define SUB_CHAR_TABLE_P(x) (CHAR_TABLE_P (x) && NILP (XCHAR_TABLE (x)->top))

/* Test for image (image . spec)  */
#define IMAGEP(x) (CONSP (x) && EQ (XCAR (x), Qimage))

/* Array types.  */

#define ARRAYP(x) \
  (VECTORP (x) || STRINGP (x) || CHAR_TABLE_P (x) || BOOL_VECTOR_P (x))

#define GC_EQ(x, y) EQ (x, y)

#define CHECK_LIST(x) \
  CHECK_TYPE (CONSP (x) || NILP (x), Qlistp, x)

#define CHECK_LIST_CONS(x, y) \
  CHECK_TYPE (CONSP (x), Qlistp, y)

#define CHECK_LIST_END(x, y) \
  CHECK_TYPE (NILP (x), Qlistp, y)

#define CHECK_STRING(x) \
  CHECK_TYPE (STRINGP (x), Qstringp, x)

#define CHECK_STRING_CAR(x) \
  CHECK_TYPE (STRINGP (XCAR (x)), Qstringp, XCAR (x))

#define CHECK_CONS(x) \
  CHECK_TYPE (CONSP (x), Qconsp, x)

#define CHECK_SYMBOL(x) \
  CHECK_TYPE (SYMBOLP (x), Qsymbolp, x)

#define CHECK_CHAR_TABLE(x) \
  CHECK_TYPE (CHAR_TABLE_P (x), Qchar_table_p, x)

#define CHECK_VECTOR(x) \
  CHECK_TYPE (VECTORP (x), Qvectorp, x)

#define CHECK_VECTOR_OR_STRING(x) \
  CHECK_TYPE (VECTORP (x) || STRINGP (x), Qarrayp, x)

#define CHECK_ARRAY(x, Qxxxp)							\
  CHECK_TYPE (ARRAYP (x), Qxxxp, x)

#define CHECK_VECTOR_OR_CHAR_TABLE(x) \
  CHECK_TYPE (VECTORP (x) || CHAR_TABLE_P (x), Qvector_or_char_table_p, x)

#define CHECK_BUFFER(x) \
  CHECK_TYPE (BUFFERP (x), Qbufferp, x)

#define CHECK_WINDOW(x) \
  CHECK_TYPE (WINDOWP (x), Qwindowp, x)

#define CHECK_WINDOW_CONFIGURATION(x) \
  CHECK_TYPE (WINDOW_CONFIGURATIONP (x), Qwindow_configuration_p, x)

/* This macro rejects windows on the interior of the window tree as
   "dead", which is what we want; this is an argument-checking macro, and
   the user should never get access to interior windows.

   A window of any sort, leaf or interior, is dead iff the buffer,
   vchild, and hchild members are all nil.  */

#define CHECK_LIVE_WINDOW(x) \
  CHECK_TYPE (WINDOWP (x) && !NILP (XWINDOW (x)->buffer), Qwindow_live_p, x)

#define CHECK_PROCESS(x) \
  CHECK_TYPE (PROCESSP (x), Qprocessp, x)

#define CHECK_SUBR(x) \
  CHECK_TYPE (SUBRP (x), Qsubrp, x)

#define CHECK_NUMBER(x) \
  CHECK_TYPE (INTEGERP (x), Qintegerp, x)

#define CHECK_NATNUM(x) \
  CHECK_TYPE (NATNUMP (x), Qwholenump, x)

#define CHECK_MARKER(x) \
  CHECK_TYPE (MARKERP (x), Qmarkerp, x)

#define CHECK_NUMBER_COERCE_MARKER(x) \
  do { if (MARKERP ((x))) XSETFASTINT (x, marker_position (x)); \
    else CHECK_TYPE (INTEGERP (x), Qinteger_or_marker_p, x); } while (0)

#define XFLOATINT(n) extract_float((n))

#define CHECK_FLOAT(x)		\
  CHECK_TYPE (FLOATP (x), Qfloatp, x)

#define CHECK_NUMBER_OR_FLOAT(x)	\
  CHECK_TYPE (FLOATP (x) || INTEGERP (x), Qnumberp, x)

#define CHECK_NUMBER_OR_FLOAT_COERCE_MARKER(x) \
  do { if (MARKERP (x)) XSETFASTINT (x, marker_position (x));	\
    else CHECK_TYPE (INTEGERP (x) || FLOATP (x), Qnumber_or_marker_p, x); } while (0)

#define CHECK_OVERLAY(x) \
  CHECK_TYPE (OVERLAYP (x), Qoverlayp, x)

/* Since we can't assign directly to the CAR or CDR fields of a cons
   cell, use these when checking that those fields contain numbers.  */
#define CHECK_NUMBER_CAR(x) \
  do {					\
    Lisp_Object tmp = XCAR (x);		\
    CHECK_NUMBER (tmp);			\
    XSETCAR ((x), tmp);			\
  } while (0)

#define CHECK_NUMBER_CDR(x) \
  do {					\
    Lisp_Object tmp = XCDR (x);		\
    CHECK_NUMBER (tmp);			\
    XSETCDR ((x), tmp);			\
  } while (0)

/* Cast pointers to this type to compare them.  Some machines want int.  */
#ifndef PNTR_COMPARISON_TYPE
#define PNTR_COMPARISON_TYPE EMACS_UINT
#endif

/* Define a built-in function for calling from Lisp.
 `lname' should be the name to give the function in Lisp,
    as a null-terminated C string.
 `fnname' should be the name of the function in C.
    By convention, it starts with F.
 `sname' should be the name for the C constant structure
    that records information on this function for internal use.
    By convention, it should be the same as `fnname' but with S instead of F.
    It's too bad that C macros can't compute this from `fnname'.
 `minargs' should be a number, the minimum number of arguments allowed.
 `maxargs' should be a number, the maximum number of arguments allowed,
    or else MANY or UNEVALLED.
    MANY means pass a vector of evaluated arguments,
	 in the form of an integer number-of-arguments
	 followed by the address of a vector of Lisp_Objects
	 which contains the argument values.
    UNEVALLED means pass the list of unevaluated arguments
 `prompt' says how to read arguments for an interactive call.
    See the doc string for `interactive'.
    A null string means call interactively with no arguments.
 `doc' is documentation for the user.  */

#if (!defined (__STDC__) && !defined (PROTOTYPES)) \
    || defined (USE_NONANSI_DEFUN)

#define DEFUN(lname, fnname, sname, minargs, maxargs, prompt, doc)	\
  Lisp_Object fnname ();						\
  DECL_ALIGN (struct Lisp_Subr, sname) =				\
    { PVEC_SUBR | (sizeof (struct Lisp_Subr) / sizeof (EMACS_INT)),	\
      fnname, minargs, maxargs, lname, prompt, 0};			\
  Lisp_Object fnname

#else

/* This version of DEFUN declares a function prototype with the right
   arguments, so we can catch errors with maxargs at compile-time.  */
#define DEFUN(lname, fnname, sname, minargs, maxargs, prompt, doc)	\
  Lisp_Object fnname DEFUN_ARGS_ ## maxargs ;				\
  DECL_ALIGN (struct Lisp_Subr, sname) =				\
    { PVEC_SUBR | (sizeof (struct Lisp_Subr) / sizeof (EMACS_INT)),	\
      fnname, minargs, maxargs, lname, prompt, 0};			\
  Lisp_Object fnname

/* Note that the weird token-substitution semantics of ANSI C makes
   this work for MANY and UNEVALLED.  */
#define DEFUN_ARGS_MANY		(int, Lisp_Object *)
#define DEFUN_ARGS_UNEVALLED	(Lisp_Object)
#define DEFUN_ARGS_0	(void)
#define DEFUN_ARGS_1	(Lisp_Object)
#define DEFUN_ARGS_2	(Lisp_Object, Lisp_Object)
#define DEFUN_ARGS_3	(Lisp_Object, Lisp_Object, Lisp_Object)
#define DEFUN_ARGS_4	(Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object)
#define DEFUN_ARGS_5	(Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object, \
			 Lisp_Object)
#define DEFUN_ARGS_6	(Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object, \
			 Lisp_Object, Lisp_Object)
#define DEFUN_ARGS_7	(Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object, \
			 Lisp_Object, Lisp_Object, Lisp_Object)
#define DEFUN_ARGS_8	(Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object, \
			 Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object)
#endif

/* Non-zero if OBJ is a Lisp function.  */

#define FUNCTIONP(OBJ)					\
     ((CONSP (OBJ) && EQ (XCAR (OBJ), Qlambda))		\
      || (SYMBOLP (OBJ) && !NILP (Ffboundp (OBJ)))	\
      || COMPILEDP (OBJ)				\
      || SUBRP (OBJ))

/* defsubr (Sname);
   is how we define the symbol for function `name' at start-up time.  */
extern void defsubr P_ ((struct Lisp_Subr *));

#define MANY -2
#define UNEVALLED -1

extern void defvar_lisp P_ ((char *, Lisp_Object *));
extern void defvar_lisp_nopro P_ ((char *, Lisp_Object *));
extern void defvar_bool P_ ((char *, int *));
extern void defvar_int P_ ((char *, EMACS_INT *));
extern void defvar_per_buffer P_ ((char *, Lisp_Object *, Lisp_Object, char *));
extern void defvar_kboard P_ ((char *, int));

/* Macros we use to define forwarded Lisp variables.
   These are used in the syms_of_FILENAME functions.  */

#define DEFVAR_LISP(lname, vname, doc) defvar_lisp (lname, vname)
#define DEFVAR_LISP_NOPRO(lname, vname, doc) defvar_lisp_nopro (lname, vname)
#define DEFVAR_BOOL(lname, vname, doc) defvar_bool (lname, vname)
#define DEFVAR_INT(lname, vname, doc) defvar_int (lname, vname)

/* TYPE is nil for a general Lisp variable.
   An integer specifies a type; then only LIsp values
   with that type code are allowed (except that nil is allowed too).
   LNAME is the LIsp-level variable name.
   VNAME is the name of the buffer slot.
   DOC is a dummy where you write the doc string as a comment.  */
#define DEFVAR_PER_BUFFER(lname, vname, type, doc)  \
 defvar_per_buffer (lname, vname, type, 0)

#define DEFVAR_KBOARD(lname, vname, doc) \
 defvar_kboard (lname, \
		(int)((char *)(&current_kboard->vname) \
		      - (char *)current_kboard))



/* Structure for recording Lisp call stack for backtrace purposes.  */

/* The special binding stack holds the outer values of variables while
   they are bound by a function application or a let form, stores the
   code to be executed for Lisp unwind-protect forms, and stores the C
   functions to be called for record_unwind_protect.

   If func is non-zero, undoing this binding applies func to old_value;
      This implements record_unwind_protect.

   Otherwise, the element is a variable binding.

   If the symbol field is a symbol, it is an ordinary variable binding.

   Otherwise, it should be a structure (SYMBOL WHERE
   . CURRENT-BUFFER), which means having bound a local value while
   CURRENT-BUFFER was active.  If WHERE is nil this means we saw the
   default value when binding SYMBOL.  WHERE being a buffer or frame
   means we saw a buffer-local or frame-local value.  Other values of
   WHERE mean an internal error.  */

typedef Lisp_Object (*specbinding_func) P_ ((Lisp_Object));

struct specbinding
  {
    Lisp_Object symbol, old_value;
    specbinding_func func;
    Lisp_Object unused;		/* Dividing by 16 is faster than by 12 */
  };

extern struct specbinding *specpdl;
extern struct specbinding *specpdl_ptr;
extern int specpdl_size;

extern EMACS_INT max_specpdl_size;

#define SPECPDL_INDEX()	(specpdl_ptr - specpdl)

/* Everything needed to describe an active condition case.  */
struct handler
  {
    /* The handler clauses and variable from the condition-case form.  */
    /* For a handler set up in Lisp code, this is always a list.
       For an internal handler set up by internal_condition_case*,
       this can instead be the symbol t or `error'.
       t: handle all conditions.
       error: handle all conditions, and errors can run the debugger
              or display a backtrace.  */
    Lisp_Object handler;
    Lisp_Object var;
    /* Fsignal stores here the condition-case clause that applies,
       and Fcondition_case thus knows which clause to run.  */
    Lisp_Object chosen_clause;

    /* Used to effect the longjump out to the handler.  */
    struct catchtag *tag;

    /* The next enclosing handler.  */
    struct handler *next;
  };

extern struct handler *handlerlist;

extern struct catchtag *catchlist;
extern struct backtrace *backtrace_list;

extern Lisp_Object memory_signal_data;

/* An address near the bottom of the stack.
   Tells GC how to save a copy of the stack.  */
extern char *stack_bottom;

/* Check quit-flag and quit if it is non-nil.
   Typing C-g does not directly cause a quit; it only sets Vquit_flag.
   So the program needs to do QUIT at times when it is safe to quit.
   Every loop that might run for a long time or might not exit
   ought to do QUIT at least once, at a safe place.
   Unless that is impossible, of course.
   But it is very desirable to avoid creating loops where QUIT is impossible.

   Exception: if you set immediate_quit to nonzero,
   then the handler that responds to the C-g does the quit itself.
   This is a good thing to do around a loop that has no side effects
   and (in particular) cannot call arbitrary Lisp code.  */

#ifdef SYNC_INPUT
extern void handle_async_input P_ ((void));
extern int interrupt_input_pending;

#define QUIT						\
  do {							\
    if (!NILP (Vquit_flag) && NILP (Vinhibit_quit))	\
      {							\
        Lisp_Object flag = Vquit_flag;			\
	Vquit_flag = Qnil;				\
	if (EQ (Vthrow_on_input, flag))			\
	  Fthrow (Vthrow_on_input, Qt);			\
	Fsignal (Qquit, Qnil);				\
      }							\
    else if (interrupt_input_pending)			\
      handle_async_input ();				\
  } while (0)

#else  /* not SYNC_INPUT */

#define QUIT						\
  do {							\
    if (!NILP (Vquit_flag) && NILP (Vinhibit_quit))	\
      {							\
        Lisp_Object flag = Vquit_flag;			\
	Vquit_flag = Qnil;				\
	if (EQ (Vthrow_on_input, flag))			\
	  Fthrow (Vthrow_on_input, Qt);			\
	Fsignal (Qquit, Qnil);				\
      }							\
  } while (0)

#endif	/* not SYNC_INPUT */


/* Nonzero if ought to quit now.  */

#define QUITP (!NILP (Vquit_flag) && NILP (Vinhibit_quit))

/* Variables used locally in the following case handling macros.  */
extern int case_temp1;
extern Lisp_Object case_temp2;

/* Current buffer's map from characters to lower-case characters.  */

#define DOWNCASE_TABLE current_buffer->downcase_table

/* Current buffer's map from characters to upper-case characters.  */

#define UPCASE_TABLE current_buffer->upcase_table

/* Downcase a character, or make no change if that cannot be done.  */

#define DOWNCASE(CH)						\
  ((case_temp1 = (CH),						\
    case_temp2 = CHAR_TABLE_REF (DOWNCASE_TABLE, case_temp1),	\
    NATNUMP (case_temp2))					\
   ? XFASTINT (case_temp2) : case_temp1)

/* 1 if CH is upper case.  */

#define UPPERCASEP(CH) (DOWNCASE (CH) != (CH))

/* 1 if CH is neither upper nor lower case.  */

#define NOCASEP(CH) (UPCASE1 (CH) == (CH))

/* 1 if CH is lower case.  */

#define LOWERCASEP(CH) (!UPPERCASEP (CH) && !NOCASEP(CH))

/* Upcase a character, or make no change if that cannot be done.  */

#define UPCASE(CH) (!UPPERCASEP (CH) ? UPCASE1 (CH) : (CH))

/* Upcase a character known to be not upper case.  */

#define UPCASE1(CH)						\
  ((case_temp1 = (CH),						\
    case_temp2 = CHAR_TABLE_REF (UPCASE_TABLE, case_temp1),	\
    NATNUMP (case_temp2))					\
   ? XFASTINT (case_temp2) : case_temp1)

extern Lisp_Object Vascii_downcase_table, Vascii_upcase_table;
extern Lisp_Object Vascii_canon_table, Vascii_eqv_table;

/* Number of bytes of structure consed since last GC.  */

extern int consing_since_gc;

/* Thresholds for doing another gc.  */

extern EMACS_INT gc_cons_threshold;

extern EMACS_INT gc_relative_threshold;

extern EMACS_INT memory_full_cons_threshold;

/* Structure for recording stack slots that need marking.  */

/* This is a chain of structures, each of which points at a Lisp_Object variable
 whose value should be marked in garbage collection.
 Normally every link of the chain is an automatic variable of a function,
 and its `val' points to some argument or local variable of the function.
 On exit to the function, the chain is set back to the value it had on entry.
 This way, no link remains in the chain when the stack frame containing the
 link disappears.

 Every function that can call Feval must protect in this fashion all
 Lisp_Object variables whose contents will be used again.  */

extern struct gcpro *gcprolist;

struct gcpro
{
  struct gcpro *next;

  /* Address of first protected variable.  */
  volatile Lisp_Object *var;

  /* Number of consecutive protected variables.  */
  int nvars;

#ifdef DEBUG_GCPRO
  int level;
#endif
};

/* Values of GC_MARK_STACK during compilation:

   0	Use GCPRO as before
   1	Do the real thing, make GCPROs and UNGCPRO no-ops.
   2    Mark the stack, and check that everything GCPRO'd is
	marked.
   3	Mark using GCPRO's, mark stack last, and count how many
	dead objects are kept alive.  */


#define GC_USE_GCPROS_AS_BEFORE		0
#define GC_MAKE_GCPROS_NOOPS		1
#define GC_MARK_STACK_CHECK_GCPROS	2
#define GC_USE_GCPROS_CHECK_ZOMBIES	3

#ifndef GC_MARK_STACK
#define GC_MARK_STACK GC_USE_GCPROS_AS_BEFORE
#endif

#if GC_MARK_STACK == GC_MAKE_GCPROS_NOOPS

/* Do something silly with gcproN vars just so gcc shuts up.  */
/* You get warnings from MIPSPro...  */

#define GCPRO1(varname) ((void) gcpro1)
#define GCPRO2(varname1, varname2)(((void) gcpro2, (void) gcpro1))
#define GCPRO3(varname1, varname2, varname3) \
  (((void) gcpro3, (void) gcpro2, (void) gcpro1))
#define GCPRO4(varname1, varname2, varname3, varname4) \
  (((void) gcpro4, (void) gcpro3, (void) gcpro2, (void) gcpro1))
#define GCPRO5(varname1, varname2, varname3, varname4, varname5) \
  (((void) gcpro5, (void) gcpro4, (void) gcpro3, (void) gcpro2, (void) gcpro1))
#define GCPRO6(varname1, varname2, varname3, varname4, varname5, varname6) \
  (((void) gcpro6, (void) gcpro5, (void) gcpro4, (void) gcpro3, (void) gcpro2, (void) gcpro1))
#define UNGCPRO ((void) 0)

#else /* GC_MARK_STACK != GC_MAKE_GCPROS_NOOPS */

#ifndef DEBUG_GCPRO

#define GCPRO1(varname) \
 {gcpro1.next = gcprolist; gcpro1.var = &varname; gcpro1.nvars = 1; \
  gcprolist = &gcpro1; }

#define GCPRO2(varname1, varname2) \
 {gcpro1.next = gcprolist; gcpro1.var = &varname1; gcpro1.nvars = 1; \
  gcpro2.next = &gcpro1; gcpro2.var = &varname2; gcpro2.nvars = 1; \
  gcprolist = &gcpro2; }

#define GCPRO3(varname1, varname2, varname3) \
 {gcpro1.next = gcprolist; gcpro1.var = &varname1; gcpro1.nvars = 1; \
  gcpro2.next = &gcpro1; gcpro2.var = &varname2; gcpro2.nvars = 1; \
  gcpro3.next = &gcpro2; gcpro3.var = &varname3; gcpro3.nvars = 1; \
  gcprolist = &gcpro3; }

#define GCPRO4(varname1, varname2, varname3, varname4) \
 {gcpro1.next = gcprolist; gcpro1.var = &varname1; gcpro1.nvars = 1; \
  gcpro2.next = &gcpro1; gcpro2.var = &varname2; gcpro2.nvars = 1; \
  gcpro3.next = &gcpro2; gcpro3.var = &varname3; gcpro3.nvars = 1; \
  gcpro4.next = &gcpro3; gcpro4.var = &varname4; gcpro4.nvars = 1; \
  gcprolist = &gcpro4; }

#define GCPRO5(varname1, varname2, varname3, varname4, varname5) \
 {gcpro1.next = gcprolist; gcpro1.var = &varname1; gcpro1.nvars = 1; \
  gcpro2.next = &gcpro1; gcpro2.var = &varname2; gcpro2.nvars = 1; \
  gcpro3.next = &gcpro2; gcpro3.var = &varname3; gcpro3.nvars = 1; \
  gcpro4.next = &gcpro3; gcpro4.var = &varname4; gcpro4.nvars = 1; \
  gcpro5.next = &gcpro4; gcpro5.var = &varname5; gcpro5.nvars = 1; \
  gcprolist = &gcpro5; }

#define GCPRO6(varname1, varname2, varname3, varname4, varname5, varname6) \
 {gcpro1.next = gcprolist; gcpro1.var = &varname1; gcpro1.nvars = 1; \
  gcpro2.next = &gcpro1; gcpro2.var = &varname2; gcpro2.nvars = 1; \
  gcpro3.next = &gcpro2; gcpro3.var = &varname3; gcpro3.nvars = 1; \
  gcpro4.next = &gcpro3; gcpro4.var = &varname4; gcpro4.nvars = 1; \
  gcpro5.next = &gcpro4; gcpro5.var = &varname5; gcpro5.nvars = 1; \
  gcpro6.next = &gcpro5; gcpro6.var = &varname6; gcpro6.nvars = 1; \
  gcprolist = &gcpro6; }

#define UNGCPRO (gcprolist = gcpro1.next)

#else

extern int gcpro_level;

#define GCPRO1(varname) \
 {gcpro1.next = gcprolist; gcpro1.var = &varname; gcpro1.nvars = 1; \
  gcpro1.level = gcpro_level++; \
  gcprolist = &gcpro1; }

#define GCPRO2(varname1, varname2) \
 {gcpro1.next = gcprolist; gcpro1.var = &varname1; gcpro1.nvars = 1; \
  gcpro1.level = gcpro_level; \
  gcpro2.next = &gcpro1; gcpro2.var = &varname2; gcpro2.nvars = 1; \
  gcpro2.level = gcpro_level++; \
  gcprolist = &gcpro2; }

#define GCPRO3(varname1, varname2, varname3) \
 {gcpro1.next = gcprolist; gcpro1.var = &varname1; gcpro1.nvars = 1; \
  gcpro1.level = gcpro_level; \
  gcpro2.next = &gcpro1; gcpro2.var = &varname2; gcpro2.nvars = 1; \
  gcpro3.next = &gcpro2; gcpro3.var = &varname3; gcpro3.nvars = 1; \
  gcpro3.level = gcpro_level++; \
  gcprolist = &gcpro3; }

#define GCPRO4(varname1, varname2, varname3, varname4) \
 {gcpro1.next = gcprolist; gcpro1.var = &varname1; gcpro1.nvars = 1; \
  gcpro1.level = gcpro_level; \
  gcpro2.next = &gcpro1; gcpro2.var = &varname2; gcpro2.nvars = 1; \
  gcpro3.next = &gcpro2; gcpro3.var = &varname3; gcpro3.nvars = 1; \
  gcpro4.next = &gcpro3; gcpro4.var = &varname4; gcpro4.nvars = 1; \
  gcpro4.level = gcpro_level++; \
  gcprolist = &gcpro4; }

#define GCPRO5(varname1, varname2, varname3, varname4, varname5) \
 {gcpro1.next = gcprolist; gcpro1.var = &varname1; gcpro1.nvars = 1; \
  gcpro1.level = gcpro_level; \
  gcpro2.next = &gcpro1; gcpro2.var = &varname2; gcpro2.nvars = 1; \
  gcpro3.next = &gcpro2; gcpro3.var = &varname3; gcpro3.nvars = 1; \
  gcpro4.next = &gcpro3; gcpro4.var = &varname4; gcpro4.nvars = 1; \
  gcpro5.next = &gcpro4; gcpro5.var = &varname5; gcpro5.nvars = 1; \
  gcpro5.level = gcpro_level++; \
  gcprolist = &gcpro5; }

#define GCPRO6(varname1, varname2, varname3, varname4, varname5, varname6) \
 {gcpro1.next = gcprolist; gcpro1.var = &varname1; gcpro1.nvars = 1; \
  gcpro1.level = gcpro_level; \
  gcpro2.next = &gcpro1; gcpro2.var = &varname2; gcpro2.nvars = 1; \
  gcpro3.next = &gcpro2; gcpro3.var = &varname3; gcpro3.nvars = 1; \
  gcpro4.next = &gcpro3; gcpro4.var = &varname4; gcpro4.nvars = 1; \
  gcpro5.next = &gcpro4; gcpro5.var = &varname5; gcpro5.nvars = 1; \
  gcpro6.next = &gcpro5; gcpro6.var = &varname6; gcpro6.nvars = 1; \
  gcpro6.level = gcpro_level++; \
  gcprolist = &gcpro6; }

#define UNGCPRO					\
 ((--gcpro_level != gcpro1.level)		\
  ? (abort (), 0)				\
  : ((gcprolist = gcpro1.next), 0))

#endif /* DEBUG_GCPRO */
#endif /* GC_MARK_STACK != GC_MAKE_GCPROS_NOOPS */


/* Evaluate expr, UNGCPRO, and then return the value of expr.  */
#define RETURN_UNGCPRO(expr)			\
do						\
    {						\
      Lisp_Object ret_ungc_val;			\
      ret_ungc_val = (expr);			\
      UNGCPRO;					\
      return ret_ungc_val;			\
    }						\
while (0)

/* Call staticpro (&var) to protect static variable `var'.  */

void staticpro P_ ((Lisp_Object *));

/* Declare a Lisp-callable function.  The MAXARGS parameter has the same
   meaning as in the DEFUN macro, and is used to construct a prototype.  */
#if (!defined (__STDC__) &&  !defined (PROTOTYPES)) \
    || defined (USE_NONANSI_DEFUN)
#define EXFUN(fnname, maxargs) \
  extern Lisp_Object fnname ()
#else
/* We can use the same trick as in the DEFUN macro to generate the
   appropriate prototype.  */
#define EXFUN(fnname, maxargs) \
  extern Lisp_Object fnname DEFUN_ARGS_ ## maxargs
#endif

/* Forward declarations for prototypes.  */
struct window;
struct frame;

/* Defined in data.c */
extern Lisp_Object Qnil, Qt, Qquote, Qlambda, Qsubr, Qunbound;
extern Lisp_Object Qerror_conditions, Qerror_message, Qtop_level;
extern Lisp_Object Qerror, Qquit, Qwrong_type_argument, Qargs_out_of_range;
extern Lisp_Object Qvoid_variable, Qvoid_function;
extern Lisp_Object Qsetting_constant, Qinvalid_read_syntax;
extern Lisp_Object Qinvalid_function, Qwrong_number_of_arguments, Qno_catch;
extern Lisp_Object Qend_of_file, Qarith_error, Qmark_inactive;
extern Lisp_Object Qbeginning_of_buffer, Qend_of_buffer, Qbuffer_read_only;
extern Lisp_Object Qtext_read_only;

extern Lisp_Object Qintegerp, Qnatnump, Qwholenump, Qsymbolp, Qlistp, Qconsp;
extern Lisp_Object Qstringp, Qarrayp, Qsequencep, Qbufferp;
extern Lisp_Object Qchar_or_string_p, Qmarkerp, Qinteger_or_marker_p, Qvectorp;
extern Lisp_Object Qbuffer_or_string_p;
extern Lisp_Object Qboundp, Qfboundp;
extern Lisp_Object Qchar_table_p, Qvector_or_char_table_p;

extern Lisp_Object Qcdr;

extern Lisp_Object Qrange_error, Qdomain_error, Qsingularity_error;
extern Lisp_Object Qoverflow_error, Qunderflow_error;

extern Lisp_Object Qfloatp;
extern Lisp_Object Qnumberp, Qnumber_or_marker_p;

extern Lisp_Object Qinteger;

extern void circular_list_error P_ ((Lisp_Object)) NO_RETURN;
EXFUN (Finteractive_form, 1);

/* Defined in frame.c */
extern Lisp_Object Qframep;

EXFUN (Feq, 2);
EXFUN (Fnull, 1);
EXFUN (Flistp, 1);
EXFUN (Fconsp, 1);
EXFUN (Fatom, 1);
EXFUN (Fnlistp, 1);
EXFUN (Fintegerp, 1);
EXFUN (Fnatnump, 1);
EXFUN (Fsymbolp, 1);
EXFUN (Fvectorp, 1);
EXFUN (Fstringp, 1);
EXFUN (Fmultibyte_string_p, 1);
EXFUN (Farrayp, 1);
EXFUN (Fsequencep, 1);
EXFUN (Fbufferp, 1);
EXFUN (Fmarkerp, 1);
EXFUN (Fsubrp, 1);
EXFUN (Fchar_or_string_p, 1);
EXFUN (Finteger_or_marker_p, 1);
EXFUN (Ffloatp, 1);
EXFUN (Finteger_or_floatp, 1);
EXFUN (Finteger_or_float_or_marker_p, 1);

EXFUN (Fcar, 1);
EXFUN (Fcar_safe, 1);
EXFUN (Fcdr, 1);
EXFUN (Fcdr_safe, 1);
EXFUN (Fsetcar, 2);
EXFUN (Fsetcdr, 2);
EXFUN (Fboundp, 1);
EXFUN (Ffboundp, 1);
EXFUN (Fmakunbound, 1);
EXFUN (Ffmakunbound, 1);
EXFUN (Fsymbol_function, 1);
EXFUN (Fsymbol_plist, 1);
EXFUN (Fsymbol_name, 1);
extern Lisp_Object indirect_function P_ ((Lisp_Object));
EXFUN (Findirect_function, 2);
EXFUN (Ffset, 2);
EXFUN (Fsetplist, 2);
EXFUN (Fsymbol_value, 1);
extern Lisp_Object find_symbol_value P_ ((Lisp_Object));
EXFUN (Fset, 2);
EXFUN (Fdefault_value, 1);
EXFUN (Fset_default, 2);
EXFUN (Fdefault_boundp, 1);
EXFUN (Fmake_local_variable, 1);
EXFUN (Flocal_variable_p, 2);
EXFUN (Flocal_variable_if_set_p, 2);

EXFUN (Faref, 2);
EXFUN (Faset, 3);

EXFUN (Fstring_to_number, 2);
EXFUN (Fnumber_to_string, 1);
EXFUN (Feqlsign, 2);
EXFUN (Fgtr, 2);
EXFUN (Flss, 2);
EXFUN (Fgeq, 2);
EXFUN (Fleq, 2);
EXFUN (Fneq, 2);
EXFUN (Fzerop, 1);
EXFUN (Fplus, MANY);
EXFUN (Fminus, MANY);
EXFUN (Ftimes, MANY);
EXFUN (Fquo, MANY);
EXFUN (Frem, 2);
EXFUN (Fmax, MANY);
EXFUN (Fmin, MANY);
EXFUN (Flogand, MANY);
EXFUN (Flogior, MANY);
EXFUN (Flogxor, MANY);
EXFUN (Flognot, 1);
EXFUN (Flsh, 2);
EXFUN (Fash, 2);

EXFUN (Fadd1, 1);
EXFUN (Fsub1, 1);
EXFUN (Fmake_variable_buffer_local, 1);

extern Lisp_Object indirect_variable P_ ((Lisp_Object));
extern Lisp_Object long_to_cons P_ ((unsigned long));
extern unsigned long cons_to_long P_ ((Lisp_Object));
extern void args_out_of_range P_ ((Lisp_Object, Lisp_Object)) NO_RETURN;
extern void args_out_of_range_3 P_ ((Lisp_Object, Lisp_Object,
				     Lisp_Object)) NO_RETURN;
extern Lisp_Object wrong_type_argument P_ ((Lisp_Object, Lisp_Object)) NO_RETURN;
extern void store_symval_forwarding P_ ((Lisp_Object, Lisp_Object,
					 Lisp_Object, struct buffer *));
extern Lisp_Object do_symval_forwarding P_ ((Lisp_Object));
extern Lisp_Object set_internal P_ ((Lisp_Object, Lisp_Object, struct buffer *, int));
extern void syms_of_data P_ ((void));
extern void init_data P_ ((void));
extern void swap_in_global_binding P_ ((Lisp_Object));

/* Defined in cmds.c */
EXFUN (Fend_of_line, 1);
EXFUN (Fforward_char, 1);
EXFUN (Fforward_line, 1);
extern int internal_self_insert P_ ((int, int));
extern void syms_of_cmds P_ ((void));
extern void keys_of_cmds P_ ((void));

/* Defined in coding.c */
EXFUN (Fcoding_system_p, 1);
EXFUN (Fcheck_coding_system, 1);
EXFUN (Fread_coding_system, 2);
EXFUN (Fread_non_nil_coding_system, 1);
EXFUN (Ffind_operation_coding_system, MANY);
EXFUN (Fupdate_coding_systems_internal, 0);
EXFUN (Fencode_coding_string, 3);
EXFUN (Fdecode_coding_string, 3);
extern Lisp_Object detect_coding_system P_ ((const unsigned char *, int, int,
					     int));
extern void init_coding P_ ((void));
extern void init_coding_once P_ ((void));
extern void syms_of_coding P_ ((void));
extern Lisp_Object code_convert_string_norecord P_ ((Lisp_Object, Lisp_Object,
						     int));

/* Defined in charset.c */
extern EMACS_INT nonascii_insert_offset;
extern Lisp_Object Vnonascii_translation_table;
EXFUN (Fchar_bytes, 1);
EXFUN (Fchar_width, 1);
EXFUN (Fstring, MANY);
extern int chars_in_text P_ ((const unsigned char *, int));
extern int multibyte_chars_in_text P_ ((const unsigned char *, int));
extern int unibyte_char_to_multibyte P_ ((int));
extern int multibyte_char_to_unibyte P_ ((int, Lisp_Object));
extern Lisp_Object Qcharset;
extern void init_charset_once P_ ((void));
extern void syms_of_charset P_ ((void));

/* Defined in syntax.c */
EXFUN (Fforward_word, 1);
EXFUN (Fskip_chars_forward, 2);
EXFUN (Fskip_chars_backward, 2);
EXFUN (Fsyntax_table_p, 1);
EXFUN (Fsyntax_table, 0);
EXFUN (Fset_syntax_table, 1);
extern void init_syntax_once P_ ((void));
extern void syms_of_syntax P_ ((void));

/* Defined in fns.c */
extern int use_dialog_box;
extern int next_almost_prime P_ ((int));
extern Lisp_Object larger_vector P_ ((Lisp_Object, int, Lisp_Object));
extern void sweep_weak_hash_tables P_ ((void));
extern Lisp_Object Qstring_lessp;
EXFUN (Foptimize_char_table, 1);
extern Lisp_Object Vfeatures;
extern Lisp_Object QCtest, QCweakness, Qequal;
unsigned sxhash P_ ((Lisp_Object, int));
Lisp_Object make_hash_table P_ ((Lisp_Object, Lisp_Object, Lisp_Object,
				 Lisp_Object, Lisp_Object, Lisp_Object,
				 Lisp_Object));
Lisp_Object copy_hash_table P_ ((struct Lisp_Hash_Table *));
int hash_lookup P_ ((struct Lisp_Hash_Table *, Lisp_Object, unsigned *));
int hash_put P_ ((struct Lisp_Hash_Table *, Lisp_Object, Lisp_Object,
		  unsigned));
void hash_remove P_ ((struct Lisp_Hash_Table *, Lisp_Object));
void hash_clear P_ ((struct Lisp_Hash_Table *));
void remove_hash_entry P_ ((struct Lisp_Hash_Table *, int));
extern void init_fns P_ ((void));
EXFUN (Fsxhash, 1);
EXFUN (Fmake_hash_table, MANY);
EXFUN (Fcopy_hash_table, 1);
EXFUN (Fhash_table_count, 1);
EXFUN (Fhash_table_rehash_size, 1);
EXFUN (Fhash_table_rehash_threshold, 1);
EXFUN (Fhash_table_size, 1);
EXFUN (Fhash_table_test, 1);
EXFUN (Fhash_table_weak, 1);
EXFUN (Fhash_table_p, 1);
EXFUN (Fclrhash, 1);
EXFUN (Fgethash, 3);
EXFUN (Fputhash, 3);
EXFUN (Fremhash, 2);
EXFUN (Fmaphash, 2);
EXFUN (Fdefine_hash_table_test, 3);

EXFUN (Fidentity, 1);
EXFUN (Frandom, 1);
EXFUN (Flength, 1);
EXFUN (Fsafe_length, 1);
EXFUN (Fappend, MANY);
EXFUN (Fconcat, MANY);
EXFUN (Fvconcat, MANY);
EXFUN (Fcopy_sequence, 1);
EXFUN (Fstring_make_multibyte, 1);
EXFUN (Fstring_make_unibyte, 1);
EXFUN (Fstring_as_multibyte, 1);
EXFUN (Fstring_as_unibyte, 1);
EXFUN (Fstring_to_multibyte, 1);
EXFUN (Fsubstring, 3);
extern Lisp_Object substring_both P_ ((Lisp_Object, int, int, int, int));
EXFUN (Fnth, 2);
EXFUN (Fnthcdr, 2);
EXFUN (Fmemq, 2);
EXFUN (Fassq, 2);
EXFUN (Fassoc, 2);
EXFUN (Felt, 2);
EXFUN (Fmember, 2);
EXFUN (Frassq, 2);
EXFUN (Fdelq, 2);
EXFUN (Fdelete, 2);
EXFUN (Fsort, 2);
EXFUN (Freverse, 1);
EXFUN (Fnreverse, 1);
EXFUN (Fget, 2);
EXFUN (Fput, 3);
EXFUN (Fequal, 2);
EXFUN (Ffillarray, 2);
EXFUN (Fnconc, MANY);
EXFUN (Fmapcar, 2);
EXFUN (Fmapconcat, 3);
EXFUN (Fy_or_n_p, 1);
extern Lisp_Object do_yes_or_no_p P_ ((Lisp_Object));
EXFUN (Frequire, 3);
EXFUN (Fprovide, 2);
extern Lisp_Object concat2 P_ ((Lisp_Object, Lisp_Object));
extern Lisp_Object concat3 P_ ((Lisp_Object, Lisp_Object, Lisp_Object));
extern Lisp_Object nconc2 P_ ((Lisp_Object, Lisp_Object));
extern Lisp_Object assq_no_quit P_ ((Lisp_Object, Lisp_Object));
extern void clear_string_char_byte_cache P_ ((void));
extern int string_char_to_byte P_ ((Lisp_Object, int));
extern int string_byte_to_char P_ ((Lisp_Object, int));
extern Lisp_Object string_make_multibyte P_ ((Lisp_Object));
extern Lisp_Object string_to_multibyte P_ ((Lisp_Object));
extern Lisp_Object string_make_unibyte P_ ((Lisp_Object));
EXFUN (Fcopy_alist, 1);
EXFUN (Fplist_get, 2);
EXFUN (Fplist_put, 3);
EXFUN (Fplist_member, 2);
EXFUN (Fset_char_table_parent, 2);
EXFUN (Fchar_table_extra_slot, 2);
EXFUN (Fset_char_table_extra_slot, 3);
EXFUN (Frassoc, 2);
EXFUN (Fstring_equal, 2);
EXFUN (Fcompare_strings, 7);
EXFUN (Fstring_lessp, 2);
extern int char_table_translate P_ ((Lisp_Object, int));
extern void map_char_table P_ ((void (*) (Lisp_Object, Lisp_Object, Lisp_Object),
				Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object, int,
				Lisp_Object *));
extern Lisp_Object char_table_ref_and_index P_ ((Lisp_Object, int, int *));
extern void syms_of_fns P_ ((void));

/* Defined in floatfns.c */
extern double extract_float P_ ((Lisp_Object));
EXFUN (Ffloat, 1);
EXFUN (Ftruncate, 2);
extern void init_floatfns P_ ((void));
extern void syms_of_floatfns P_ ((void));

/* Defined in fringe.c */
extern void syms_of_fringe P_ ((void));
extern void init_fringe P_ ((void));
extern void init_fringe_once P_ ((void));

/* Defined in image.c */
EXFUN (Finit_image_library, 2);
extern void syms_of_image P_ ((void));
extern void init_image P_ ((void));

/* Defined in insdel.c */
extern Lisp_Object Qinhibit_modification_hooks;
extern void move_gap P_ ((int));
extern void move_gap_both P_ ((int, int));
extern void make_gap P_ ((int));
extern int copy_text P_ ((const unsigned char *, unsigned char *, int, int, int));
extern int count_size_as_multibyte P_ ((const unsigned char *, int));
extern int count_combining_before P_ ((const unsigned char *, int, int, int));
extern int count_combining_after P_ ((const unsigned char *, int, int, int));
extern void insert P_ ((const unsigned char *, int));
extern void insert_and_inherit P_ ((const unsigned char *, int));
extern void insert_1 P_ ((const unsigned char *, int, int, int, int));
extern void insert_1_both P_ ((const unsigned char *, int, int, int, int, int));
extern void insert_from_string P_ ((Lisp_Object, int, int, int, int, int));
extern void insert_from_buffer P_ ((struct buffer *, int, int, int));
extern void insert_char P_ ((int));
extern void insert_string P_ ((const char *));
extern void insert_before_markers P_ ((const unsigned char *, int));
extern void insert_before_markers_and_inherit P_ ((const unsigned char *, int));
extern void insert_from_string_before_markers P_ ((Lisp_Object, int, int, int, int, int));
extern void del_range P_ ((int, int));
extern Lisp_Object del_range_1 P_ ((int, int, int, int));
extern void del_range_byte P_ ((int, int, int));
extern void del_range_both P_ ((int, int, int, int, int));
extern Lisp_Object del_range_2 P_ ((int, int, int, int, int));
extern void modify_region P_ ((struct buffer *, int, int, int));
extern void prepare_to_modify_buffer P_ ((int, int, int *));
extern void signal_before_change P_ ((int, int, int *));
extern void signal_after_change P_ ((int, int, int));
extern void adjust_after_replace P_ ((int, int, Lisp_Object, int, int));
extern void adjust_after_replace_noundo P_ ((int, int, int, int, int, int));
extern void adjust_after_insert P_ ((int, int, int, int, int));
extern void replace_range P_ ((int, int, Lisp_Object, int, int, int));
extern void replace_range_2 P_ ((int, int, int, int, char *, int, int, int));
extern void syms_of_insdel P_ ((void));

/* Defined in dispnew.c */
extern Lisp_Object selected_frame;
extern EMACS_INT baud_rate;
EXFUN (Fding, 1);
EXFUN (Fredraw_frame, 1);
EXFUN (Fredraw_display, 0);
EXFUN (Fsleep_for, 2);
EXFUN (Fredisplay, 1);
extern Lisp_Object sit_for P_ ((Lisp_Object, int, int));
extern void init_display P_ ((void));
extern void syms_of_display P_ ((void));
extern void safe_bcopy P_ ((const char *, char *, int));

/* Defined in xdisp.c */
extern Lisp_Object Qinhibit_point_motion_hooks;
extern Lisp_Object Qinhibit_redisplay, Qdisplay;
extern Lisp_Object Qinhibit_eval_during_redisplay;
extern Lisp_Object Qmessage_truncate_lines;
extern Lisp_Object Qimage;
extern Lisp_Object Vmessage_log_max;
extern int message_enable_multibyte;
extern Lisp_Object echo_area_buffer[2];
extern void check_message_stack P_ ((void));
extern void setup_echo_area_for_printing P_ ((int));
extern int push_message P_ ((void));
extern Lisp_Object pop_message_unwind P_ ((Lisp_Object));
extern Lisp_Object restore_message_unwind P_ ((Lisp_Object));
extern void pop_message P_ ((void));
extern void restore_message P_ ((void));
extern Lisp_Object current_message P_ ((void));
extern void set_message P_ ((const char *s, Lisp_Object, int, int));
extern void clear_message P_ ((int, int));
extern void message P_ ((/* char *, ... */));
extern void message_nolog P_ ((/* char *, ... */));
extern void message1 P_ ((char *));
extern void message1_nolog P_ ((char *));
extern void message2 P_ ((const char *, int, int));
extern void message2_nolog P_ ((const char *, int, int));
extern void message3 P_ ((Lisp_Object, int, int));
extern void message3_nolog P_ ((Lisp_Object, int, int));
extern void message_dolog P_ ((const char *, int, int, int));
extern void message_with_string P_ ((char *, Lisp_Object, int));
extern void message_log_maybe_newline P_ ((void));
extern void update_echo_area P_ ((void));
extern void truncate_echo_area P_ ((int));
extern void redisplay P_ ((void));
extern int check_point_in_composition
	P_ ((struct buffer *, int, struct buffer *, int));
extern void redisplay_preserve_echo_area P_ ((int));
extern void prepare_menu_bars P_ ((void));

void set_frame_cursor_types P_ ((struct frame *, Lisp_Object));
extern void syms_of_xdisp P_ ((void));
extern void init_xdisp P_ ((void));
extern Lisp_Object safe_eval P_ ((Lisp_Object));
extern int pos_visible_p P_ ((struct window *, int, int *,
			      int *, int *, int *, int *, int *));

/* Defined in vm-limit.c.  */
extern void memory_warnings P_ ((POINTER_TYPE *, void (*warnfun) ()));

/* Defined in alloc.c */
extern void check_pure_size P_ ((void));
extern void allocate_string_data P_ ((struct Lisp_String *, int, int));
extern void reset_malloc_hooks P_ ((void));
extern void uninterrupt_malloc P_ ((void));
extern void malloc_warning P_ ((char *));
extern void memory_full P_ ((void)) NO_RETURN;
extern void buffer_memory_full P_ ((void)) NO_RETURN;
extern int survives_gc_p P_ ((Lisp_Object));
extern void mark_object P_ ((Lisp_Object));
extern Lisp_Object Vpurify_flag;
extern Lisp_Object Vmemory_full;
EXFUN (Fcons, 2);
EXFUN (list1, 1);
EXFUN (list2, 2);
EXFUN (list3, 3);
EXFUN (list4, 4);
EXFUN (list5, 5);
EXFUN (Flist, MANY);
EXFUN (Fmake_list, 2);
extern Lisp_Object allocate_misc P_ ((void));
EXFUN (Fmake_vector, 2);
EXFUN (Fvector, MANY);
EXFUN (Fmake_symbol, 1);
EXFUN (Fmake_marker, 0);
EXFUN (Fmake_string, 2);
extern Lisp_Object build_string P_ ((const char *));
extern Lisp_Object make_string P_ ((const char *, int));
extern Lisp_Object make_unibyte_string P_ ((const char *, int));
extern Lisp_Object make_multibyte_string P_ ((const char *, int, int));
extern Lisp_Object make_event_array P_ ((int, Lisp_Object *));
extern Lisp_Object make_uninit_string P_ ((int));
extern Lisp_Object make_uninit_multibyte_string P_ ((int, int));
extern Lisp_Object make_string_from_bytes P_ ((const char *, int, int));
extern Lisp_Object make_specified_string P_ ((const char *, int, int, int));
EXFUN (Fpurecopy, 1);
extern Lisp_Object make_pure_string P_ ((char *, int, int, int));
extern Lisp_Object pure_cons P_ ((Lisp_Object, Lisp_Object));
extern Lisp_Object make_pure_vector P_ ((EMACS_INT));
EXFUN (Fgarbage_collect, 0);
EXFUN (Fmake_byte_code, MANY);
EXFUN (Fmake_bool_vector, 2);
EXFUN (Fmake_char_table, 2);
extern Lisp_Object make_sub_char_table P_ ((Lisp_Object));
extern Lisp_Object Qchar_table_extra_slots;
extern struct Lisp_Vector *allocate_vector P_ ((EMACS_INT));
extern struct Lisp_Vector *allocate_other_vector P_ ((EMACS_INT));
extern struct Lisp_Hash_Table *allocate_hash_table P_ ((void));
extern struct window *allocate_window P_ ((void));
extern struct frame *allocate_frame P_ ((void));
extern struct Lisp_Process *allocate_process P_ ((void));
extern int gc_in_progress;
extern int abort_on_gc;
extern Lisp_Object make_float P_ ((double));
extern void display_malloc_warning P_ ((void));
extern int inhibit_garbage_collection P_ ((void));
extern Lisp_Object make_save_value P_ ((void *, int));
extern void free_misc P_ ((Lisp_Object));
extern void free_marker P_ ((Lisp_Object));
extern void free_cons P_ ((struct Lisp_Cons *));
extern void init_alloc_once P_ ((void));
extern void init_alloc P_ ((void));
extern void syms_of_alloc P_ ((void));
extern struct buffer * allocate_buffer P_ ((void));
extern int valid_lisp_object_p P_ ((Lisp_Object));

/* Defined in print.c */
extern Lisp_Object Vprin1_to_string_buffer;
extern void debug_print P_ ((Lisp_Object));
EXFUN (Fprin1, 2);
EXFUN (Fprin1_to_string, 2);
EXFUN (Fprinc, 2);
EXFUN (Fterpri, 1);
EXFUN (Fprint, 2);
EXFUN (Ferror_message_string, 1);
extern Lisp_Object Vstandard_output, Qstandard_output;
extern Lisp_Object Qexternal_debugging_output;
extern void temp_output_buffer_setup P_ ((const char *));
extern int print_level, print_escape_newlines;
extern Lisp_Object Qprint_escape_newlines;
extern void write_string P_ ((char *, int));
extern void write_string_1 P_ ((char *, int, Lisp_Object));
extern void print_error_message P_ ((Lisp_Object, Lisp_Object, char *, Lisp_Object));
extern Lisp_Object internal_with_output_to_temp_buffer
	P_ ((const char *, Lisp_Object (*) (Lisp_Object), Lisp_Object));
extern void float_to_string P_ ((unsigned char *, double));
extern void syms_of_print P_ ((void));

/* Defined in doprnt.c */
extern int doprnt P_ ((char *, int, char *, char *, int, char **));
extern int doprnt_lisp P_ ((char *, int, char *, char *, int, char **));

/* Defined in lread.c */
extern Lisp_Object Qvariable_documentation, Qstandard_input;
extern Lisp_Object Vobarray, initial_obarray, Vstandard_input;
EXFUN (Fread, 1);
EXFUN (Fread_from_string, 3);
EXFUN (Fintern, 2);
EXFUN (Fintern_soft, 2);
EXFUN (Fload, 5);
EXFUN (Fget_load_suffixes, 0);
EXFUN (Fget_file_char, 0);
EXFUN (Fread_char, 3);
EXFUN (Fread_event, 3);
extern Lisp_Object read_filtered_event P_ ((int, int, int, int, Lisp_Object));
EXFUN (Feval_region, 4);
extern Lisp_Object check_obarray P_ ((Lisp_Object));
extern Lisp_Object intern P_ ((const char *));
extern Lisp_Object make_symbol P_ ((char *));
extern Lisp_Object oblookup P_ ((Lisp_Object, const char *, int, int));
#define LOADHIST_ATTACH(x) \
 if (initialized) Vcurrent_load_list = Fcons (x, Vcurrent_load_list)
extern Lisp_Object Vcurrent_load_list;
extern Lisp_Object Vload_history, Vload_suffixes, Vload_file_rep_suffixes;
extern int openp P_ ((Lisp_Object, Lisp_Object, Lisp_Object,
		      Lisp_Object *, Lisp_Object));
extern int isfloat_string P_ ((char *));
extern void map_obarray P_ ((Lisp_Object, void (*) (Lisp_Object, Lisp_Object),
			     Lisp_Object));
extern void dir_warning P_ ((char *, Lisp_Object));
extern void close_load_descs P_ ((void));
extern void init_obarray P_ ((void));
extern void init_lread P_ ((void));
extern void syms_of_lread P_ ((void));

/* Defined in eval.c */
extern Lisp_Object Qautoload, Qexit, Qinteractive, Qcommandp, Qdefun, Qmacro;
extern Lisp_Object Vinhibit_quit, Qinhibit_quit, Vquit_flag;
extern Lisp_Object Vautoload_queue;
extern Lisp_Object Vdebug_on_error;
extern Lisp_Object Vsignaling_function;
extern int handling_signal;
extern int interactive_p P_ ((int));

/* To run a normal hook, use the appropriate function from the list below.
   The calling convention:

   if (!NILP (Vrun_hooks))
     call1 (Vrun_hooks, Qmy_funny_hook);

   should no longer be used.  */
extern Lisp_Object Vrun_hooks;
EXFUN (Frun_hooks, MANY);
EXFUN (Frun_hook_with_args, MANY);
EXFUN (Frun_hook_with_args_until_success, MANY);
EXFUN (Frun_hook_with_args_until_failure, MANY);
extern Lisp_Object run_hook_list_with_args P_ ((Lisp_Object, int, Lisp_Object *));
extern void run_hook_with_args_2 P_ ((Lisp_Object, Lisp_Object, Lisp_Object));
EXFUN (Fand, UNEVALLED);
EXFUN (For, UNEVALLED);
EXFUN (Fif, UNEVALLED);
EXFUN (Fprogn, UNEVALLED);
EXFUN (Fprog1, UNEVALLED);
EXFUN (Fprog2, UNEVALLED);
EXFUN (Fsetq, UNEVALLED);
EXFUN (Fquote, UNEVALLED);
EXFUN (Fuser_variable_p, 1);
EXFUN (Finteractive_p, 0);
EXFUN (Fdefun, UNEVALLED);
EXFUN (Flet, UNEVALLED);
EXFUN (FletX, UNEVALLED);
EXFUN (Fwhile, UNEVALLED);
EXFUN (Fcatch, UNEVALLED);
EXFUN (Fthrow, 2) NO_RETURN;
EXFUN (Funwind_protect, UNEVALLED);
EXFUN (Fcondition_case, UNEVALLED);
EXFUN (Fsignal, 2);
extern void xsignal P_ ((Lisp_Object, Lisp_Object)) NO_RETURN;
extern void xsignal0 P_ ((Lisp_Object)) NO_RETURN;
extern void xsignal1 P_ ((Lisp_Object, Lisp_Object)) NO_RETURN;
extern void xsignal2 P_ ((Lisp_Object, Lisp_Object, Lisp_Object)) NO_RETURN;
extern void xsignal3 P_ ((Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object)) NO_RETURN;
extern void signal_error P_ ((char *, Lisp_Object)) NO_RETURN;
EXFUN (Fautoload, 5);
EXFUN (Fcommandp, 2);
EXFUN (Feval, 1);
EXFUN (Fapply, MANY);
EXFUN (Ffuncall, MANY);
EXFUN (Fbacktrace, 0);
extern Lisp_Object apply1 P_ ((Lisp_Object, Lisp_Object));
extern Lisp_Object call0 P_ ((Lisp_Object));
extern Lisp_Object call1 P_ ((Lisp_Object, Lisp_Object));
extern Lisp_Object call2 P_ ((Lisp_Object, Lisp_Object, Lisp_Object));
extern Lisp_Object call3 P_ ((Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object));
extern Lisp_Object call4 P_ ((Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object));
extern Lisp_Object call5 P_ ((Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object));
extern Lisp_Object call6 P_ ((Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object, Lisp_Object));
EXFUN (Fdo_auto_save, 2);
extern Lisp_Object apply_lambda P_ ((Lisp_Object, Lisp_Object, int));
extern Lisp_Object internal_catch P_ ((Lisp_Object, Lisp_Object (*) (Lisp_Object), Lisp_Object));
extern Lisp_Object internal_lisp_condition_case P_ ((Lisp_Object, Lisp_Object, Lisp_Object));
extern Lisp_Object internal_condition_case P_ ((Lisp_Object (*) (void), Lisp_Object, Lisp_Object (*) (Lisp_Object)));
extern Lisp_Object internal_condition_case_1 P_ ((Lisp_Object (*) (Lisp_Object), Lisp_Object, Lisp_Object, Lisp_Object (*) (Lisp_Object)));
extern Lisp_Object internal_condition_case_2 P_ ((Lisp_Object (*) (int, Lisp_Object *), int, Lisp_Object *, Lisp_Object, Lisp_Object (*) (Lisp_Object)));
extern void specbind P_ ((Lisp_Object, Lisp_Object));
extern void record_unwind_protect P_ ((Lisp_Object (*) (Lisp_Object), Lisp_Object));
extern Lisp_Object unbind_to P_ ((int, Lisp_Object));
extern void error P_ ((/* char *, ... */)) NO_RETURN;
extern void do_autoload P_ ((Lisp_Object, Lisp_Object));
extern Lisp_Object un_autoload P_ ((Lisp_Object));
EXFUN (Ffetch_bytecode, 1);
extern void init_eval_once P_ ((void));
extern Lisp_Object safe_call P_ ((int, Lisp_Object *));
extern Lisp_Object safe_call1 P_ ((Lisp_Object, Lisp_Object));
extern void init_eval P_ ((void));
extern void syms_of_eval P_ ((void));

/* Defined in editfns.c */
EXFUN (Fpropertize, MANY);
EXFUN (Fcurrent_message, 0);
EXFUN (Fgoto_char, 1);
EXFUN (Fpoint_min_marker, 0);
EXFUN (Fpoint_max_marker, 0);
EXFUN (Fpoint_min, 0);
EXFUN (Fpoint_max, 0);
EXFUN (Fpoint, 0);
EXFUN (Fpoint_marker, 0);
EXFUN (Fmark_marker, 0);
EXFUN (Fline_beginning_position, 1);
EXFUN (Fline_end_position, 1);
EXFUN (Ffollowing_char, 0);
EXFUN (Fprevious_char, 0);
EXFUN (Fchar_after, 1);
EXFUN (Finsert, MANY);
EXFUN (Finsert_and_inherit, MANY);
EXFUN (Finsert_before_markers, MANY);
EXFUN (Finsert_buffer_substring, 3);
EXFUN (Finsert_char, 3);
extern void insert1 P_ ((Lisp_Object));
EXFUN (Feolp, 0);
EXFUN (Feobp, 0);
EXFUN (Fbolp, 0);
EXFUN (Fbobp, 0);
EXFUN (Fformat, MANY);
EXFUN (Fmessage, MANY);
extern Lisp_Object format2 P_ ((char *, Lisp_Object, Lisp_Object));
EXFUN (Fbuffer_substring, 2);
EXFUN (Fbuffer_string, 0);
extern Lisp_Object save_excursion_save P_ ((void));
extern Lisp_Object save_restriction_save P_ ((void));
extern Lisp_Object save_excursion_restore P_ ((Lisp_Object));
extern Lisp_Object save_restriction_restore P_ ((Lisp_Object));
EXFUN (Fchar_to_string, 1);
EXFUN (Fdelete_region, 2);
EXFUN (Fnarrow_to_region, 2);
EXFUN (Fwiden, 0);
EXFUN (Fuser_login_name, 1);
EXFUN (Fsystem_name, 0);
EXFUN (Fcurrent_time, 0);
extern int clip_to_bounds P_ ((int, int, int));
extern Lisp_Object make_buffer_string P_ ((int, int, int));
extern Lisp_Object make_buffer_string_both P_ ((int, int, int, int, int));
extern void init_editfns P_ ((void));
extern void syms_of_editfns P_ ((void));
extern Lisp_Object Vinhibit_field_text_motion;
EXFUN (Fconstrain_to_field, 5);
EXFUN (Ffield_string, 1);
EXFUN (Fdelete_field, 1);
EXFUN (Ffield_beginning, 3);
EXFUN (Ffield_end, 3);
EXFUN (Ffield_string_no_properties, 1);
extern void set_time_zone_rule P_ ((char *));

/* defined in buffer.c */
extern int mouse_face_overlay_overlaps P_ ((Lisp_Object));
extern void nsberror P_ ((Lisp_Object)) NO_RETURN;
extern char *no_switch_window P_ ((Lisp_Object window));
EXFUN (Fset_buffer_multibyte, 1);
EXFUN (Foverlay_start, 1);
EXFUN (Foverlay_end, 1);
extern void adjust_overlays_for_insert P_ ((EMACS_INT, EMACS_INT));
extern void adjust_overlays_for_delete P_ ((EMACS_INT, EMACS_INT));
extern void fix_start_end_in_overlays P_ ((int, int));
extern void report_overlay_modification P_ ((Lisp_Object, Lisp_Object, int,
					     Lisp_Object, Lisp_Object, Lisp_Object));
extern int overlay_touches_p P_ ((int));
extern Lisp_Object Vbuffer_alist, Vinhibit_read_only;
EXFUN (Fget_buffer, 1);
EXFUN (Fget_buffer_create, 1);
EXFUN (Fset_buffer, 1);
EXFUN (set_buffer_if_live, 1);
EXFUN (Fbarf_if_buffer_read_only, 0);
EXFUN (Fcurrent_buffer, 0);
EXFUN (Fswitch_to_buffer, 2);
EXFUN (Fpop_to_buffer, 3);
EXFUN (Fother_buffer, 3);
EXFUN (Foverlay_get, 2);
EXFUN (Fbuffer_modified_p, 1);
EXFUN (Fset_buffer_modified_p, 1);
EXFUN (Fkill_buffer, 1);
EXFUN (Fkill_all_local_variables, 0);
EXFUN (Fbuffer_disable_undo, 1);
EXFUN (Fbuffer_enable_undo, 1);
EXFUN (Ferase_buffer, 0);
extern Lisp_Object Qoverlayp;
extern Lisp_Object Qevaporate;
extern Lisp_Object get_truename_buffer P_ ((Lisp_Object));
extern struct buffer *all_buffers;
EXFUN (Fprevious_overlay_change, 1);
EXFUN (Fbuffer_file_name, 1);
extern void init_buffer_once P_ ((void));
extern void init_buffer P_ ((void));
extern void syms_of_buffer P_ ((void));
extern void keys_of_buffer P_ ((void));

/* defined in marker.c */

EXFUN (Fmarker_position, 1);
EXFUN (Fmarker_buffer, 1);
EXFUN (Fcopy_marker, 2);
EXFUN (Fset_marker, 3);
extern int marker_position P_ ((Lisp_Object));
extern int marker_byte_position P_ ((Lisp_Object));
extern void clear_charpos_cache P_ ((struct buffer *));
extern int charpos_to_bytepos P_ ((int));
extern int buf_charpos_to_bytepos P_ ((struct buffer *, int));
extern int buf_bytepos_to_charpos P_ ((struct buffer *, int));
extern void unchain_marker P_ ((struct Lisp_Marker *marker));
extern Lisp_Object set_marker_restricted P_ ((Lisp_Object, Lisp_Object, Lisp_Object));
extern Lisp_Object set_marker_both P_ ((Lisp_Object, Lisp_Object, int, int));
extern Lisp_Object set_marker_restricted_both P_ ((Lisp_Object, Lisp_Object,
						   int, int));
extern void syms_of_marker P_ ((void));

/* Defined in fileio.c */

extern Lisp_Object Qfile_error;
EXFUN (Ffind_file_name_handler, 2);
EXFUN (Ffile_name_as_directory, 1);
EXFUN (Fmake_temp_name, 1);
EXFUN (Fexpand_file_name, 2);
EXFUN (Ffile_name_nondirectory, 1);
EXFUN (Fsubstitute_in_file_name, 1);
EXFUN (Ffile_symlink_p, 1);
EXFUN (Fverify_visited_file_modtime, 1);
EXFUN (Ffile_exists_p, 1);
EXFUN (Ffile_name_absolute_p, 1);
EXFUN (Fdirectory_file_name, 1);
EXFUN (Ffile_name_directory, 1);
extern Lisp_Object expand_and_dir_to_file P_ ((Lisp_Object, Lisp_Object));
EXFUN (Ffile_accessible_directory_p, 1);
EXFUN (Funhandled_file_name_directory, 1);
EXFUN (Ffile_directory_p, 1);
EXFUN (Fwrite_region, 7);
EXFUN (Ffile_readable_p, 1);
EXFUN (Ffile_executable_p, 1);
EXFUN (Fread_file_name, 6);
extern Lisp_Object close_file_unwind P_ ((Lisp_Object));
extern void report_file_error P_ ((const char *, Lisp_Object)) NO_RETURN;
extern int internal_delete_file P_ ((Lisp_Object));
extern void syms_of_fileio P_ ((void));
extern void init_fileio_once P_ ((void));
extern Lisp_Object make_temp_name P_ ((Lisp_Object, int));
EXFUN (Fmake_symbolic_link, 3);

/* Defined in abbrev.c */

extern void syms_of_abbrev P_ ((void));

/* defined in search.c */
extern void shrink_regexp_cache P_ ((void));
EXFUN (Fstring_match, 3);
extern void restore_search_regs P_ ((void));
EXFUN (Fmatch_data, 3);
EXFUN (Fset_match_data, 2);
EXFUN (Fmatch_beginning, 1);
EXFUN (Fmatch_end, 1);
extern void record_unwind_save_match_data P_ ((void));
EXFUN (Flooking_at, 1);
extern int fast_string_match P_ ((Lisp_Object, Lisp_Object));
extern int fast_c_string_match_ignore_case P_ ((Lisp_Object, const char *));
extern int fast_string_match_ignore_case P_ ((Lisp_Object, Lisp_Object));
extern int scan_buffer P_ ((int, int, int, int, int *, int));
extern int scan_newline P_ ((int, int, int, int, int, int));
extern int find_next_newline P_ ((int, int));
extern int find_next_newline_no_quit P_ ((int, int));
extern int find_before_next_newline P_ ((int, int, int));
extern void syms_of_search P_ ((void));
extern void clear_regexp_cache P_ ((void));

/* defined in minibuf.c */

extern Lisp_Object last_minibuf_string;
extern void choose_minibuf_frame P_ ((void));
EXFUN (Fcompleting_read, 8);
EXFUN (Fread_from_minibuffer, 7);
EXFUN (Fread_variable, 2);
EXFUN (Fread_buffer, 3);
EXFUN (Fread_minibuffer, 2);
EXFUN (Feval_minibuffer, 2);
EXFUN (Fread_string, 5);
EXFUN (Fread_no_blanks_input, 3);
extern Lisp_Object get_minibuffer P_ ((int));
extern void temp_echo_area_glyphs P_ ((Lisp_Object));
extern void init_minibuf_once P_ ((void));
extern void syms_of_minibuf P_ ((void));
extern void keys_of_minibuf P_ ((void));

/* Defined in callint.c */

extern Lisp_Object Qminus, Qplus, Vcurrent_prefix_arg;
extern Lisp_Object Vcommand_history;
extern Lisp_Object Qcall_interactively, Qmouse_leave_buffer_hook;
EXFUN (Fcall_interactively, 3);
EXFUN (Fprefix_numeric_value, 1);
extern void syms_of_callint P_ ((void));

/* defined in casefiddle.c */

EXFUN (Fdowncase, 1);
EXFUN (Fupcase, 1);
EXFUN (Fcapitalize, 1);
EXFUN (Fupcase_region, 2);
EXFUN (Fupcase_initials, 1);
EXFUN (Fupcase_initials_region, 2);
extern void syms_of_casefiddle P_ ((void));
extern void keys_of_casefiddle P_ ((void));

/* defined in casetab.c */

EXFUN (Fset_case_table, 1);
EXFUN (Fset_standard_case_table, 1);
extern void init_casetab_once P_ ((void));
extern void syms_of_casetab P_ ((void));

/* defined in keyboard.c */

extern int echoing;
extern Lisp_Object echo_message_buffer;
extern struct kboard *echo_kboard;
extern void cancel_echoing P_ ((void));
extern Lisp_Object Qdisabled, QCfilter;
extern Lisp_Object Vtty_erase_char, Vhelp_form, Vtop_level;
extern Lisp_Object Vthrow_on_input;
extern int input_pending;
EXFUN (Fdiscard_input, 0);
EXFUN (Frecursive_edit, 0);
EXFUN (Ftop_level, 0);
EXFUN (Fcommand_execute, 4);
EXFUN (Finput_pending_p, 0);
extern Lisp_Object menu_bar_items P_ ((Lisp_Object));
extern Lisp_Object tool_bar_items P_ ((Lisp_Object, int *));
extern Lisp_Object Qvertical_scroll_bar;
extern void discard_mouse_events P_ ((void));
EXFUN (Fevent_convert_list, 1);
EXFUN (Fread_key_sequence, 5);
EXFUN (Fset_input_mode, 4);
extern int detect_input_pending P_ ((void));
extern int detect_input_pending_ignore_squeezables P_ ((void));
extern int detect_input_pending_run_timers P_ ((int));
extern void safe_run_hooks P_ ((Lisp_Object));
extern void cmd_error_internal P_ ((Lisp_Object, char *));
extern Lisp_Object command_loop_1 P_ ((void));
extern Lisp_Object recursive_edit_1 P_ ((void));
extern void record_auto_save P_ ((void));
extern void init_keyboard P_ ((void));
extern void syms_of_keyboard P_ ((void));
extern void keys_of_keyboard P_ ((void));
extern char *push_key_description P_ ((unsigned int, char *, int));


/* defined in indent.c */
EXFUN (Fvertical_motion, 2);
EXFUN (Findent_to, 2);
EXFUN (Fcurrent_column, 0);
EXFUN (Fmove_to_column, 2);
extern double current_column P_ ((void));
extern void invalidate_current_column P_ ((void));
extern int indented_beyond_p P_ ((int, int, double));
extern void syms_of_indent P_ ((void));

/* defined in frame.c */
#ifdef HAVE_WINDOW_SYSTEM
extern Lisp_Object Vx_resource_name;
extern Lisp_Object Vx_resource_class;
#endif /* HAVE_WINDOW_SYSTEM */
extern Lisp_Object Qvisible;
extern void store_frame_param P_ ((struct frame *, Lisp_Object, Lisp_Object));
extern void store_in_alist P_ ((Lisp_Object *, Lisp_Object, Lisp_Object));
extern Lisp_Object do_switch_frame P_ ((Lisp_Object, int, int));
extern Lisp_Object get_frame_param P_ ((struct frame *, Lisp_Object));
extern Lisp_Object frame_buffer_predicate P_ ((Lisp_Object));
EXFUN (Fframep, 1);
EXFUN (Fselect_frame, 1);
EXFUN (Fselected_frame, 0);
EXFUN (Fwindow_frame, 1);
EXFUN (Fframe_root_window, 1);
EXFUN (Fframe_first_window, 1);
EXFUN (Fframe_selected_window, 1);
EXFUN (Fframe_list, 0);
EXFUN (Fnext_frame, 2);
EXFUN (Fdelete_frame, 2);
EXFUN (Fset_mouse_position, 3);
EXFUN (Fmake_frame_visible, 1);
EXFUN (Fmake_frame_invisible, 2);
EXFUN (Ficonify_frame, 1);
EXFUN (Fframe_visible_p, 1);
EXFUN (Fvisible_frame_list, 0);
EXFUN (Fframe_parameter, 2);
EXFUN (Fframe_parameters, 1);
EXFUN (Fmodify_frame_parameters, 2);
EXFUN (Fset_frame_height, 3);
EXFUN (Fset_frame_width, 3);
EXFUN (Fset_frame_size, 3);
EXFUN (Fset_frame_position, 3);
EXFUN (Fraise_frame, 1);
EXFUN (Fredirect_frame_focus, 2);
EXFUN (Fset_frame_selected_window, 2);
extern Lisp_Object frame_buffer_list P_ ((Lisp_Object));
extern void frames_discard_buffer P_ ((Lisp_Object));
extern void set_frame_buffer_list P_ ((Lisp_Object, Lisp_Object));
extern void frames_bury_buffer P_ ((Lisp_Object));
extern void syms_of_frame P_ ((void));

/* defined in emacs.c */
extern Lisp_Object decode_env_path P_ ((char *, char *));
extern Lisp_Object Vinvocation_name, Vinvocation_directory;
extern Lisp_Object Vinstallation_directory, empty_string;
EXFUN (Fkill_emacs, 1);
#if HAVE_SETLOCALE
void fixup_locale P_ ((void));
void synchronize_system_messages_locale P_ ((void));
void synchronize_system_time_locale P_ ((void));
#else
#define setlocale(category, locale)
#define fixup_locale()
#define synchronize_system_messages_locale()
#define synchronize_system_time_locale()
#endif
void shut_down_emacs P_ ((int, int, Lisp_Object));
/* Nonzero means don't do interactive redisplay and don't change tty modes */
extern int noninteractive;
/* Nonzero means don't do use window-system-specific display code */
extern int inhibit_window_system;
/* Nonzero means that a filter or a sentinel is running.  */
extern int running_asynch_code;

/* defined in process.c */
EXFUN (Fget_process, 1);
EXFUN (Fget_buffer_process, 1);
EXFUN (Fprocessp, 1);
EXFUN (Fprocess_status, 1);
EXFUN (Fkill_process, 2);
EXFUN (Fprocess_send_eof, 1);
EXFUN (Fwaiting_for_user_input_p, 0);
extern Lisp_Object Qprocessp;
extern void kill_buffer_processes P_ ((Lisp_Object));
extern int wait_reading_process_output P_ ((int, int, int, int,
					    Lisp_Object,
					    struct Lisp_Process *,
					    int));
extern void add_keyboard_wait_descriptor P_ ((int));
extern void delete_keyboard_wait_descriptor P_ ((int));
extern void close_process_descs P_ ((void));
extern void init_process P_ ((void));
extern void syms_of_process P_ ((void));
extern void setup_process_coding_systems P_ ((Lisp_Object));

/* defined in callproc.c */
extern Lisp_Object Vexec_path, Vexec_suffixes,
                   Vexec_directory, Vdata_directory;
extern Lisp_Object Vdoc_directory;
EXFUN (Fcall_process, MANY);
extern int child_setup P_ ((int, int, int, char **, int, Lisp_Object));
extern void init_callproc_1 P_ ((void));
extern void init_callproc P_ ((void));
extern void set_process_environment P_ ((void));
extern void syms_of_callproc P_ ((void));

/* defined in doc.c */
extern Lisp_Object Vdoc_file_name;
EXFUN (Fsubstitute_command_keys, 1);
EXFUN (Fdocumentation, 2);
EXFUN (Fdocumentation_property, 3);
extern Lisp_Object read_doc_string P_ ((Lisp_Object));
extern Lisp_Object get_doc_string P_ ((Lisp_Object, int, int));
extern void syms_of_doc P_ ((void));
extern int read_bytecode_char P_ ((int));

/* defined in bytecode.c */
extern Lisp_Object Qbytecode;
EXFUN (Fbyte_code, 3);
extern void syms_of_bytecode P_ ((void));
extern struct byte_stack *byte_stack_list;
extern void mark_byte_stack P_ ((void));
extern void unmark_byte_stack P_ ((void));

/* defined in macros.c */
extern Lisp_Object Qexecute_kbd_macro;
EXFUN (Fexecute_kbd_macro, 3);
EXFUN (Fcancel_kbd_macro_events, 0);
extern void init_macros P_ ((void));
extern void syms_of_macros P_ ((void));

/* defined in undo.c */
extern Lisp_Object Qinhibit_read_only;
EXFUN (Fundo_boundary, 0);
extern void truncate_undo_list P_ ((struct buffer *));
extern void record_marker_adjustment P_ ((Lisp_Object, int));
extern void record_insert P_ ((int, int));
extern void record_delete P_ ((int, Lisp_Object));
extern void record_first_change P_ ((void));
extern void record_change P_ ((int, int));
extern void record_property_change P_ ((int, int, Lisp_Object, Lisp_Object,
					Lisp_Object));
extern void syms_of_undo P_ ((void));
extern Lisp_Object Vundo_outer_limit;

/* defined in textprop.c */
extern Lisp_Object Qfont, Qmouse_face;
extern Lisp_Object Qinsert_in_front_hooks, Qinsert_behind_hooks;
EXFUN (Fnext_single_property_change, 4);
EXFUN (Fnext_single_char_property_change, 4);
EXFUN (Fprevious_single_property_change, 4);
EXFUN (Fput_text_property, 5);
EXFUN (Fprevious_char_property_change, 2);
EXFUN (Fnext_char_property_change, 2);
extern void report_interval_modification P_ ((Lisp_Object, Lisp_Object));
extern Lisp_Object next_single_char_property_change P_ ((Lisp_Object,
							 Lisp_Object,
							 Lisp_Object,
							 Lisp_Object));

/* defined in xmenu.c */
EXFUN (Fx_popup_menu, 2);
EXFUN (Fx_popup_dialog, 3);
extern void syms_of_xmenu P_ ((void));

/* defined in sysdep.c */
#ifndef HAVE_GET_CURRENT_DIR_NAME
extern char *get_current_dir_name P_ ((void));
#endif
extern void stuff_char P_ ((char c));
extern void init_sigio P_ ((int));
extern void request_sigio P_ ((void));
extern void unrequest_sigio P_ ((void));
extern void reset_sys_modes P_ ((void));
extern void sys_subshell P_ ((void));
extern void sys_suspend P_ ((void));
extern void discard_tty_input P_ ((void));
extern void init_sys_modes P_ ((void));
extern void get_frame_size P_ ((int *, int *));
extern void wait_for_termination P_ ((int));
extern void flush_pending_output P_ ((int));
extern void child_setup_tty P_ ((int));
extern void setup_pty P_ ((int));
extern int set_window_size P_ ((int, int, int));
extern void create_process P_ ((Lisp_Object, char **, Lisp_Object));
extern int tabs_safe_p P_ ((void));
extern void init_baud_rate P_ ((void));
extern int emacs_open P_ ((const char *, int, int));
extern int emacs_close P_ ((int));
extern int emacs_read P_ ((int, char *, unsigned int));
extern int emacs_write P_ ((int, const char *, unsigned int));

/* defined in filelock.c */
EXFUN (Funlock_buffer, 0);
EXFUN (Ffile_locked_p, 1);
extern void unlock_all_files P_ ((void));
extern void lock_file P_ ((Lisp_Object));
extern void unlock_file P_ ((Lisp_Object));
extern void unlock_buffer P_ ((struct buffer *));
extern void syms_of_filelock P_ ((void));
extern void init_filelock P_ ((void));

/* Defined in sound.c */
extern void syms_of_sound P_ ((void));
extern void init_sound P_ ((void));

/* Defined in category.c */
extern void init_category_once P_ ((void));
extern void syms_of_category P_ ((void));

/* Defined in ccl.c */
extern void syms_of_ccl P_ ((void));

/* Defined in dired.c */
EXFUN (Ffile_attributes, 2);
extern void syms_of_dired P_ ((void));

/* Defined in term.c */
extern void syms_of_term P_ ((void));
extern void fatal () NO_RETURN;

#ifdef HAVE_WINDOW_SYSTEM
/* Defined in fontset.c */
extern void syms_of_fontset P_ ((void));
EXFUN (Fset_fontset_font, 4);

/* Defined in xfns.c, w32fns.c, or macfns.c */
EXFUN (Fxw_display_color_p, 1);
EXFUN (Fx_file_dialog, 5);
#endif

/* Defined in xfaces.c */
extern void syms_of_xfaces P_ ((void));

#ifndef HAVE_GETLOADAVG
/* Defined in getloadavg.c */
extern int getloadavg P_ ((double *, int));
#endif

#ifdef HAVE_X_WINDOWS
/* Defined in xfns.c */
extern void syms_of_xfns P_ ((void));

/* Defined in xsmfns.c */
extern void syms_of_xsmfns P_ ((void));

/* Defined in xselect.c */
extern void syms_of_xselect P_ ((void));

/* Defined in xterm.c */
extern void syms_of_xterm P_ ((void));
#endif /* HAVE_X_WINDOWS */

#ifdef MSDOS
/* Defined in msdos.c */
EXFUN (Fmsdos_downcase_filename, 1);
#endif

#ifdef MAC_OS
/* Defined in macfns.c */
extern void syms_of_macfns P_ ((void));

/* Defined in macselect.c */
extern void syms_of_macselect P_ ((void));

/* Defined in macterm.c */
extern void syms_of_macterm P_ ((void));

/* Defined in macmenu.c */
extern void syms_of_macmenu P_ ((void));

/* Defined in mac.c */
extern void syms_of_mac P_ ((void));
#ifdef MAC_OSX
extern void init_mac_osx_environment P_ ((void));
#endif /* MAC_OSX */
#endif /* MAC_OS */

/* Nonzero means Emacs has already been initialized.
   Used during startup to detect startup of dumped Emacs.  */
extern int initialized;

extern int immediate_quit;	    /* Nonzero means ^G can quit instantly */

extern POINTER_TYPE *xmalloc P_ ((size_t));
extern POINTER_TYPE *xrealloc P_ ((POINTER_TYPE *, size_t));
extern void xfree P_ ((POINTER_TYPE *));

extern char *xstrdup P_ ((const char *));

extern char *egetenv P_ ((char *));

/* Set up the name of the machine we're running on.  */
extern void init_system_name P_ ((void));

/* Some systems (e.g., NT) use a different path separator than Unix,
   in addition to a device separator.  Default the path separator
   to '/', and don't test for a device separator in IS_ANY_SEP.  */

#ifdef WINDOWSNT
extern Lisp_Object Vdirectory_sep_char;
#endif

#ifndef DIRECTORY_SEP
#define DIRECTORY_SEP '/'
#endif
#ifndef IS_DIRECTORY_SEP
#define IS_DIRECTORY_SEP(_c_) ((_c_) == DIRECTORY_SEP)
#endif
#ifndef IS_DEVICE_SEP
#ifndef DEVICE_SEP
#define IS_DEVICE_SEP(_c_) 0
#else
#define IS_DEVICE_SEP(_c_) ((_c_) == DEVICE_SEP)
#endif
#endif
#ifndef IS_ANY_SEP
#define IS_ANY_SEP(_c_) (IS_DIRECTORY_SEP (_c_))
#endif

#ifdef SWITCH_ENUM_BUG
#define SWITCH_ENUM_CAST(x) ((int)(x))
#else
#define SWITCH_ENUM_CAST(x) (x)
#endif

/* Loop over Lisp list LIST.  Signal an error if LIST is not a proper
   list, or if it contains circles.

   HARE and TORTOISE should be the names of Lisp_Object variables, and
   N should be the name of an EMACS_INT variable declared in the
   function where the macro is used.  Each nested loop should use
   its own variables.

   In the loop body, HARE is set to each cons of LIST, and N is the
   length of the list processed so far.  */

#define LIST_END_P(list, obj)				\
  (NILP (obj)						\
   ? 1							\
   : (CONSP (obj)					\
      ? 0						\
      : (wrong_type_argument (Qlistp, (list))), 1))

#define FOREACH(hare, list, tortoise, n)		\
  for (tortoise = hare = (list), n = 0;			\
       !LIST_END_P (list, hare);			\
       (hare = XCDR (hare), ++n,			\
	((n & 1) != 0					\
	 ? (tortoise = XCDR (tortoise),			\
	    (EQ (hare, tortoise)			\
	     && (circular_list_error ((list)), 1)))	\
	 : 0)))

/* The ubiquitous min and max macros.  */

#ifdef max
#undef max
#undef min
#endif
#define min(a, b)	((a) < (b) ? (a) : (b))
#define max(a, b)	((a) > (b) ? (a) : (b))

/* Return a fixnum or float, depending on whether VAL fits in a Lisp
   fixnum.  */

#define make_fixnum_or_float(val) \
   (FIXNUM_OVERFLOW_P (val) \
    ? make_float (val) \
    : make_number ((EMACS_INT)(val)))


/* Checks the `cycle check' variable CHECK to see if it indicates that
   EL is part of a cycle; CHECK must be either Qnil or a value returned
   by an earlier use of CYCLE_CHECK.  SUSPICIOUS is the number of
   elements after which a cycle might be suspected; after that many
   elements, this macro begins consing in order to keep more precise
   track of elements.

   Returns nil if a cycle was detected, otherwise a new value for CHECK
   that includes EL.

   CHECK is evaluated multiple times, EL and SUSPICIOUS 0 or 1 times, so
   the caller should make sure that's ok.  */

#define CYCLE_CHECK(check, el, suspicious)	\
  (NILP (check)					\
   ? make_number (0)				\
   : (INTEGERP (check)				\
      ? (XFASTINT (check) < (suspicious)	\
	 ? make_number (XFASTINT (check) + 1)	\
	 : Fcons (el, Qnil))			\
      : (!NILP (Fmemq ((el), (check)))		\
	 ? Qnil					\
	 : Fcons ((el), (check)))))


/* SAFE_ALLOCA normally allocates memory on the stack, but if size is
   larger than MAX_ALLOCA, use xmalloc to avoid overflowing the stack.  */

#define MAX_ALLOCA 16*1024

extern Lisp_Object safe_alloca_unwind (Lisp_Object);

#define USE_SAFE_ALLOCA			\
  int sa_count = SPECPDL_INDEX (), sa_must_free = 0

/* SAFE_ALLOCA allocates a simple buffer.  */

#define SAFE_ALLOCA(buf, type, size)			  \
  do {							  \
    if ((size) < MAX_ALLOCA)				  \
      buf = (type) alloca (size);			  \
    else						  \
      {							  \
	buf = (type) xmalloc (size);			  \
	sa_must_free++;					  \
	record_unwind_protect (safe_alloca_unwind,	  \
			       make_save_value (buf, 0)); \
      }							  \
  } while (0)

/* SAFE_FREE frees xmalloced memory and enables GC as needed.  */

#define SAFE_FREE()			\
  do {					\
    if (sa_must_free) {			\
      sa_must_free = 0;			\
      unbind_to (sa_count, Qnil);	\
    }					\
  } while (0)


/* SAFE_ALLOCA_LISP allocates an array of Lisp_Objects.  */

#define SAFE_ALLOCA_LISP(buf, nelt)			  \
  do {							  \
    int size_ = (nelt) * sizeof (Lisp_Object);		  \
    if (size_ < MAX_ALLOCA)				  \
      buf = (Lisp_Object *) alloca (size_);		  \
    else						  \
      {							  \
	Lisp_Object arg_;				  \
	buf = (Lisp_Object *) xmalloc (size_);		  \
	arg_ = make_save_value (buf, nelt);		  \
	XSAVE_VALUE (arg_)->dogc = 1;			  \
	sa_must_free++;					  \
	record_unwind_protect (safe_alloca_unwind, arg_); \
      }							  \
  } while (0)


#endif /* EMACS_LISP_H */

/* arch-tag: 9b2ed020-70eb-47ac-94ee-e1c2a5107d5e
   (do not change this comment) */
