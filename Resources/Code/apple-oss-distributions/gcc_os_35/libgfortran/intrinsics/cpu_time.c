/* Implementation of the CPU_TIME intrinsic.
   Copyright (C) 2003 Free Software Foundation, Inc.

This file is part of the GNU Fortran 95 runtime library (libgfor).

Libgfor is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

Libgfor is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with libgfor; see the file COPYING.LIB.  If not,
write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include <sys/types.h>
#include "libgfortran.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* The CPU_TIME intrinsic to "compare different algorithms on the same
   computer or discover which parts are the most expensive", so we
   need a way to get the CPU time with the finest resolution possible.
   We can only be accurate up to microseconds.

   As usual with UNIX systems, unfortunately no single way is
   available for all systems.  */

#ifdef TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
#else
#  if HAVE_SYS_TIME_H
#    include <sys/time.h>
#  else
#    ifdef HAVE_TIME_H
#      include <time.h>
#    endif
#  endif
#endif

/* The most accurate way to get the CPU time is getrusage ().
   If we have times(), that's good enough, too.  */
#if defined (HAVE_GETRUSAGE) && defined (HAVE_SYS_RESOURCE_H)
#  include <sys/resource.h>
#else
/* For times(), we _must_ know the number of clock ticks per second.  */
#  if defined (HAVE_TIMES) && (defined (HZ) || defined (_SC_CLK_TCK) || defined (CLK_TCK))
#    ifdef HAVE_SYS_PARAM_H
#      include <sys/param.h>
#    endif
#    include <sys/times.h>
#    ifndef HZ
#      if defined _SC_CLK_TCK
#        define HZ  sysconf(_SC_CLK_TCK)
#      else
#        define HZ  CLK_TCK
#      endif
#    endif
#  endif  /* HAVE_TIMES etc.  */
#endif  /* HAVE_GETRUSAGE && HAVE_SYS_RESOURCE_H  */

#if defined (__GNUC__) && (__GNUC__ >= 3)
#  define ATTRIBUTE_ALWAYS_INLINE __attribute__ ((__always_inline__))
#else
#  define ATTRIBUTE_ALWAYS_INLINE
#endif

static inline void __cpu_time_1 (long *, long *) ATTRIBUTE_ALWAYS_INLINE;

/* Helper function for the actual implementation of the CPU_TIME
   intrnsic.  Returns a CPU time in microseconds or -1 if no CPU time
   could be computed.  */
static inline void
__cpu_time_1 (long *sec, long *usec)
{
#if defined (HAVE_GETRUSAGE) && defined (HAVE_SYS_RESOURCE_H)
  struct rusage usage;
  getrusage (0, &usage);
  *sec = usage.ru_utime.tv_sec + usage.ru_stime.tv_sec;
  *usec = usage.ru_utime.tv_usec + usage.ru_stime.tv_usec;
#else /* ! HAVE_GETRUSAGE || ! HAVE_SYS_RESOURCE_H  */
#ifdef HAVE_TIMES
  struct tms buf;
  times (&buf);
  *sec = 0;
  *usec = (buf.tms_utime + buf.tms_stime) * (1000000 / HZ);
#else /* ! HAVE_TIMES */
  /* We have nothing to go on.  Return -1.  */
  *sec = -1;
  *usec = 0;
#endif  /* HAVE_TIMES */
#endif  /* HAVE_GETRUSAGE */
}

#undef CPU_TIME
#define CPU_TIME(KIND)						\
void prefix(cpu_time_##KIND) (GFC_REAL_##KIND *__time)		\
{								\
  long sec, usec;						\
  __cpu_time_1 (&sec, &usec);					\
  *__time = (GFC_REAL_##KIND) sec +				\
		((GFC_REAL_##KIND) usec) * 1.e-6;		\
}

CPU_TIME(4)
CPU_TIME(8)

void
prefix(second_sub) (GFC_REAL_4 *s)
{
  prefix(cpu_time_4)(s);
}

GFC_REAL_4
prefix(second) (void)
{
  GFC_REAL_4 s;
  prefix(cpu_time_4)(&s);
  return s;
}
