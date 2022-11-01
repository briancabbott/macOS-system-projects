/* Implementation of the DATE_AND_TIME intrinsic.
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
   Contributed by Steven Bosscher.

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
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "libgfortran.h"

#undef HAVE_NO_DATE_TIME
#if TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
#else
#  if HAVE_SYS_TIME_H
#    include <sys/time.h>
#  else
#    ifdef HAVE_TIME_H
#      include <time.h>
#    else
#      define HAVE_NO_DATE_TIME
#    endif  /* HAVE_TIME_H  */
#  endif  /* HAVE_SYS_TIME_H  */
#endif  /* TIME_WITH_SYS_TIME  */

#ifndef abs
#define abs(x) ((x)>=0 ? (x) : -(x))
#endif

/* DATE_AND_TIME ([DATE, TIME, ZONE, VALUES])

   Description: Returns data on the real-time clock and date in a form
   compatible with the representations defined in ISO 8601:1988.

   Class: Non-elemental subroutine.

   Arguments:

   DATE (optional) shall be scalar and of type default character, and
   shall be of length at least 8 in order to contain the complete
   value. It is an INTENT (OUT) argument. Its leftmost 8 characters
   are assigned a value of the form CCYYMMDD, where CC is the century,
   YY the year within the century, MM the month within the year, and
   DD the day within the month. If there is no date available, they
   are assigned blanks.

   TIME (optional) shall be scalar and of type default character, and
   shall be of length at least 10 in order to contain the complete
   value. It is an INTENT (OUT) argument. Its leftmost 10 characters
   are assigned a value of the form hhmmss.sss, where hh is the hour
   of the day, mm is the minutes of the hour, and ss.sss is the
   seconds and milliseconds of the minute. If there is no clock
   available, they are assigned blanks.

   ZONE (optional) shall be scalar and of type default character, and
   shall be of length at least 5 in order to contain the complete
   value. It is an INTENT (OUT) argument. Its leftmost 5 characters
   are assigned a value of the form ±hhmm, where hh and mm are the
   time difference with respect to Coordinated Universal Time (UTC) in
   hours and parts of an hour expressed in minutes, respectively. If
   there is no clock available, they are assigned blanks.

   VALUES (optional) shall be of type default integer and of rank
   one. It is an INTENT (OUT) argument. Its size shall be at least
   8. The values returned in VALUES are as follows:

      VALUES (1) the year (for example, 2003), or HUGE (0) if there is
      no date available;

      VALUES (2) the month of the year, or HUGE (0) if there
      is no date available;

      VALUES (3) the day of the month, or HUGE (0) if there is no date
      available;

      VALUES (4) the time difference with respect to Coordinated
      Universal Time (UTC) in minutes, or HUGE (0) if this information
      is not available;

      VALUES (5) the hour of the day, in the range of 0 to 23, or HUGE
      (0) if there is no clock;

      VALUES (6) the minutes of the hour, in the range 0 to 59, or
      HUGE (0) if there is no clock;

      VALUES (7) the seconds of the minute, in the range 0 to 60, or
      HUGE (0) if there is no clock;

      VALUES (8) the milliseconds of the second, in the range 0 to
      999, or HUGE (0) if there is no clock.

   NULL pointer represent missing OPTIONAL arguments.  All arguments
   have INTENT(OUT).  Because of the -i8 option, we must implement
   VALUES for INTEGER(kind=4) and INTEGER(kind=8).

   Based on libU77's date_time_.c.

   TODO :
   - Check year boundaries.
   - There is no STDC/POSIX way to get VALUES(8).  A GNUish way may
     be to use ftime.
*/

void
date_and_time (char *__date,
	       char *__time,
	       char *__zone,
	       gfc_array_i4 *__values,
	       GFC_INTEGER_4 __date_len,
	       GFC_INTEGER_4 __time_len,
	       GFC_INTEGER_4 __zone_len)
{
#define DATE_LEN 8
#define TIME_LEN 10   
#define ZONE_LEN 5
#define VALUES_SIZE 8
  char date[DATE_LEN + 1];
  char timec[TIME_LEN + 1];
  char zone[ZONE_LEN + 1];
  GFC_INTEGER_4 values[VALUES_SIZE];

#ifndef HAVE_NO_DATE_TIME
  time_t lt = time (NULL);
  struct tm local_time = *localtime (&lt);
  struct tm UTC_time = *gmtime (&lt);

  /* All arguments can be derived from VALUES.  */
  values[0] = 1900 + local_time.tm_year;
  values[1] = 1 + local_time.tm_mon;
  values[2] = local_time.tm_mday;
  values[3] = (local_time.tm_min - UTC_time.tm_min +
	       60 * (local_time.tm_hour - UTC_time.tm_hour +
		     24 * (local_time.tm_yday - UTC_time.tm_yday)));
  values[4] = local_time.tm_hour;
  values[5] = local_time.tm_min;
  values[6] = local_time.tm_sec;
#if HAVE_GETTIMEOFDAY
    {
      struct timeval tp;
#  if GETTIMEOFDAY_ONE_ARGUMENT
      if (!gettimeofday (&tp))
#  else
#    if HAVE_STRUCT_TIMEZONE
      struct timezone tzp;

      /* Some systems such as HP-UX, do have struct timezone, but
	 gettimeofday takes void* as the 2nd arg.  However, the
	 effect of passing anything other than a null pointer is
	 unspecified on HPUX.  Configure checks if gettimeofday
	 actually fails with a non-NULL arg and pretends that
	 struct timezone is missing if it does fail.  */
      if (!gettimeofday (&tp, &tzp))
#    else
      if (!gettimeofday (&tp, (void *) 0))
#    endif /* HAVE_STRUCT_TIMEZONE  */
#  endif /* GETTIMEOFDAY_ONE_ARGUMENT  */
	values[7] = tp.tv_usec / 1000;
    }
#else
   values[7] = GFC_INTEGER_4_HUGE;
#endif /* HAVE_GETTIMEOFDAY */

  if (__date)
    {
      snprintf (date, DATE_LEN + 1, "%04d%02d%02d",
		values[0], values[1], values[2]);
    }

  if (__time)
    {
      snprintf (timec, TIME_LEN + 1, "%02d%02d%02d.%03d",
		values[4], values[5], values[6], values[7]);
    }

  if (__zone)
    {
      snprintf (zone, ZONE_LEN + 1, "%+03d%02d",
		values[3] / 60, abs (values[3] % 60));
    }
#else /* if defined HAVE_NO_DATE_TIME  */
  /* We really have *nothing* to return, so return blanks and HUGE(0).  */
    {
      int i;

      memset (date, ' ', DATE_LEN);
      date[DATE_LEN] = '\0';

      memset (timec, ' ', TIME_LEN);
      time[TIME_LEN] = '\0';

      memset (zone, ' ', ZONE_LEN);
      zone[ZONE_LEN] = '\0';

      for (i = 0; i < VALUES_SIZE; i++)
        values[i] = GFC_INTEGER_4_HUGE;
    }
#endif  /* HAVE_NO_DATE_TIME  */

  /* Copy the values into the arguments.  */
  if (__values)
    {
      int i;
      size_t len, delta, elt_size;

      elt_size = GFC_DESCRIPTOR_SIZE (__values);
      len = __values->dim[0].ubound + 1 - __values->dim[0].lbound;
      delta = __values->dim[0].stride;
      if (delta == 0)
	delta = 1;

      assert (len >= VALUES_SIZE);
      /* Cope with different type kinds.  */
      if (elt_size == 4)
        {
	  GFC_INTEGER_4 *vptr4 = __values->data;

	  for (i = 0; i < VALUES_SIZE; i++, vptr4 += delta)
	    {
	      *vptr4 = values[i];
	    }
	}
      else if (elt_size == 8)
        {
	  GFC_INTEGER_8 *vptr8 = (GFC_INTEGER_8 *)__values->data;

	  for (i = 0; i < VALUES_SIZE; i++, vptr8 += delta)
	    {
	      if (values[i] == GFC_INTEGER_4_HUGE)
		*vptr8 = GFC_INTEGER_8_HUGE;
	      else
		*vptr8 = values[i];
	    }
	}
      else 
	abort ();
    }

  if (__zone)
    {
      assert (__zone_len >= ZONE_LEN);
      fstrcpy (__zone, ZONE_LEN, zone, ZONE_LEN);
    }

  if (__time)
    {
      assert (__time_len >= TIME_LEN);
      fstrcpy (__time, TIME_LEN, timec, TIME_LEN);
    }

  if (__date)
    {
      assert (__date_len >= DATE_LEN);
      fstrcpy (__date, DATE_LEN, date, DATE_LEN);
    }
#undef DATE_LEN
#undef TIME_LEN   
#undef ZONE_LEN
#undef VALUES_SIZE
}
