/* Implementation of the SYSTEM_CLOCK intrinsic.
   Copyright (C) 2004 Free Software Foundation, Inc.

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
License along with libgfortran; see the file COPYING.LIB.  If not,
write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include <sys/types.h>
#include "libgfortran.h"

#include <limits.h>

#if defined(HAVE_SYS_TIME_H) && defined(HAVE_GETTIMEOFDAY)
#  include <sys/time.h>
#  define TCK 1000
#elif defined(HAVE_TIME_H)
#  include <time.h>
#  define TCK 1
#else
#define TCK 0
#endif


#if defined(HAVE_SYS_TIME_H) && defined(HAVE_GETTIMEOFDAY)
static struct timeval tp0 = {-1, 0};
#elif defined(HAVE_TIME_H)
static time_t t0 = (time_t) -2;
#endif

/* prefix(system_clock_4) is the INTEGER(4) version of the SYSTEM_CLOCK
   intrinsic subroutine.  It returns the number of clock ticks for the current
   system time, the number of ticks per second, and the maximum possible value
   for COUNT.  On the first call to SYSTEM_CLOCK, COUNT is set to zero. */

void
prefix(system_clock_4)(GFC_INTEGER_4 *count, GFC_INTEGER_4 *count_rate,
		       GFC_INTEGER_4 *count_max)
{
  GFC_INTEGER_4 cnt;
  GFC_INTEGER_4 rate;
  GFC_INTEGER_4 mx;

#if defined(HAVE_SYS_TIME_H) && defined(HAVE_GETTIMEOFDAY)
  struct timeval tp1;
  struct timezone tzp;
  double t;

  if (gettimeofday(&tp1, &tzp) == 0)
    {
      if (tp0.tv_sec < 0)
        {
          tp0 = tp1;
          cnt = 0;
        }
      else
        {
	  /* TODO: Convert this to integer arithmetic.  */
          t  = (double) (tp1.tv_sec  - tp0.tv_sec);
          t += (double) (tp1.tv_usec - tp0.tv_usec) * 1.e-6;
          t *= TCK;

          if (t > (double) GFC_INTEGER_4_HUGE)
            {
              /* Time has wrapped. */
              while (t > (double) GFC_INTEGER_4_HUGE)
                t -= (double) GFC_INTEGER_4_HUGE;
              tp0 = tp1;
            }
	  cnt = (GFC_INTEGER_4) t;
        }
      rate = TCK;
      mx = GFC_INTEGER_4_HUGE;
    }
  else
    {
      if (count != NULL) *count = - GFC_INTEGER_4_HUGE;
      if (count_rate != NULL) *count_rate = 0;
      if (count_max != NULL) *count_max = 0;
    }
#elif defined(HAVE_TIME_H)
  time_t t, t1;

  t1 = time(NULL);

  if (t1 == (time_t) -1)
    {
      cnt = - GFC_INTEGER_4_HUGE;
      mx = 0;
    }
  else if (t0 == (time_t) -2) 
    t0 = t1;
  else
    {
      /* The timer counts in seconts, so for simplicity assume it never wraps.
	 Even with 32-bit counters this only happens once every 68 years.  */
      cnt = t1 - t0;
      mx = GFC_INTEGER_4_HUGE;
    }
#else
  cnt = - GFC_INTEGER_4_HUGE;
  mx = 0;
#endif
  if (count != NULL) *count = cnt;
  if (count_rate != NULL) *count_rate = TCK;
  if (count_max != NULL) *count_max = mx;
}


/* INTEGER(8) version of the above routine.  */

void
prefix(system_clock_8)(GFC_INTEGER_8 *count, GFC_INTEGER_8 *count_rate,
		       GFC_INTEGER_8 *count_max)
{
  GFC_INTEGER_8 cnt;
  GFC_INTEGER_8 rate;
  GFC_INTEGER_8 mx;

#if defined(HAVE_SYS_TIME_H) && defined(HAVE_GETTIMEOFDAY)
  struct timeval tp1;
  struct timezone tzp;
  double t;

  if (gettimeofday(&tp1, &tzp) == 0)
    {
      if (tp0.tv_sec < 0)
        {
          tp0 = tp1;
          cnt = 0;
        }
      else
        {
	  /* TODO: Convert this to integer arithmetic.  */
          t  = (double) (tp1.tv_sec  - tp0.tv_sec);
          t += (double) (tp1.tv_usec - tp0.tv_usec) * 1.e-6;
          t *= TCK;

          if (t > (double) GFC_INTEGER_8_HUGE)
            {
              /* Time has wrapped. */
              while (t > (double) GFC_INTEGER_8_HUGE)
                t -= (double) GFC_INTEGER_8_HUGE;
              tp0 = tp1;
            }
	  cnt = (GFC_INTEGER_8) t;
        }
      rate = TCK;
      mx = GFC_INTEGER_8_HUGE;
    }
  else
    {
      if (count != NULL) *count = - GFC_INTEGER_8_HUGE;
      if (count_rate != NULL) *count_rate = 0;
      if (count_max != NULL) *count_max = 0;
    }
#elif defined(HAVE_TIME_H)
  time_t t, t1;

  t1 = time(NULL);

  if (t1 == (time_t) -1)
    {
      cnt = - GFC_INTEGER_8_HUGE;
      mx = 0;
    }
  else if (t0 == (time_t) -2) 
    t0 = t1;
  else
    {
      /* The timer counts in seconts, so for simplicity assume it never wraps.
	 Even with 32-bit counters this only happens once every 68 years.  */
      cnt = t1 - t0;
      mx = GFC_INTEGER_8_HUGE;
    }
#else
  cnt = - GFC_INTEGER_8_HUGE;
  mx = 0;
#endif
  if (count != NULL)
    *count = cnt;
  if (count_rate != NULL)
    *count_rate = TCK;
  if (count_max != NULL)
    *count_max = mx;
}

