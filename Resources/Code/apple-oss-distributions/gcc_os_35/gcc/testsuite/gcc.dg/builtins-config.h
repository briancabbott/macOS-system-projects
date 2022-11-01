/* Copyright (C) 2003 Free Software Foundation.

   Define macros useful in tests for bulitin functions.  */

/* Define HAVE_C99_RUNTIME if the entire C99 runtime is available on
   the target system.  The value of HAVE_C99_RUNTIME should be the
   same as the value of TARGET_C99_FUNCTIONS in the GCC machine
   description.  (Perhaps GCC should predefine a special macro
   indicating whether or not TARGET_C99_FUNCTIONS is set, but it does
   not presently do that.)  */

#if defined(__hppa) && defined(__hpux)
/* PA HP-UX doesn't have the entire C99 runtime.  */
#elif defined(__sun)
/* Solaris doesn't have the entire C99 runtime.  */
#elif defined(__sgi)
/* Irix6 doesn't have the entire C99 runtime.  */
#elif defined(__FreeBSD__) && (__FreeBSD__ < 5)
/* FreeBSD before version 5 doesn't have the entire C99 runtime. */
#elif defined(__netware__)
/* NetWare doesn't have the entire C99 runtime.  */
#else
/* Newlib has the "f" variants of the math functions, but not the "l"
   variants.  TARGET_C99_FUNCTIONS is only defined if all C99
   functions are present.  Therefore, on systems using newlib, tests
   of builtins will fail for both the "f" and the "l" variants, and we
   should therefore not define HAVE_C99_RUNTIME.  Including <limits.h>
   gives us a way of seeing if _NEWLIB_VERSION is defined.  Include
   <math.h> would work too, but the GLIBC math inlines cause us to
   generate inferior code, which causes the test to fail, so it is
   not safe to include <math.h>.  */
#include <limits.h>
#ifdef _NEWLIB_VERSION
#else
#define HAVE_C99_RUNTIME
#endif
#endif
