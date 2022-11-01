/* Nelper routines to convert from integer to real.
   Copyright 2004 Free Software Foundation, Inc.
   Contributed by Paul Brook.

This file is part of the GNU Fortran 95 runtime library (libgfortran).

Libgfortran is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

Ligbfortran is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with libgfor; see the file COPYING.LIB.  If not,
write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */
#include <math.h>
#include "libgfortran.h"

/* These routines can be sensitive to excess precision, so should really be
   compiled with -ffloat-store.  */

/* Return the largest value less than one representable in a REAL*4.  */

static inline GFC_REAL_4
almostone_r4 ()
{
#ifdef HAVE_NEXTAFTERF
  return nextafterf (1.0f, 0.0f);
#else
  /* The volatile is a hack to prevent excess precision on x86.  */
  static volatile GFC_REAL_4 val = 0.0f;
  GFC_REAL_4 x;

  if (val != 0.0f)
    return val;

  val = 0.9999f;
  do
    {
      x = val;
      val = (val + 1.0f) / 2.0f;
    }
  while (val > x && val < 1.0f);
  if (val == 1.0f)
    val = x;
  return val;
#endif
}


/* Return the largest value less than one representable in a REAL*8.  */

static inline GFC_REAL_8
almostone_r8 ()
{
#ifdef HAVE_NEXTAFTER
  return nextafter (1.0, 0.0);
#else
  static volatile GFC_REAL_8 val = 0.0;
  GFC_REAL_8 x;

  if (val != 0.0)
    return val;

  val = 0.9999;
  do
    {
      x = val;
      val = (val + 1.0) / 2.0;
    }
  while (val > x && val < 1.0);
  if (val == 1.0)
    val = x;
  return val;
#endif
}


/* Convert an unsigned integer in the range [0..x] into a
   real the range [0..1).  */

GFC_REAL_4
normalize_r4_i4 (GFC_UINTEGER_4 i, GFC_UINTEGER_4 x)
{
  GFC_REAL_4 r;

  r = (GFC_REAL_4) i / (GFC_REAL_4) x;
  if (r == 1.0f)
    r = almostone_r4 ();
  return r;
}


/* Convert an unsigned integer in the range [0..x] into a
   real the range [0..1).  */

GFC_REAL_8
normalize_r8_i8 (GFC_UINTEGER_8 i, GFC_UINTEGER_8 x)
{
  GFC_REAL_8 r;

  r = (GFC_REAL_8) i / (GFC_REAL_8) x;
  if (r == 1.0)
    r = almostone_r8 ();
  return r;
}
