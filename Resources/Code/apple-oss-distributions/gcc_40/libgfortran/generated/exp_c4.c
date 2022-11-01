/* Complex exponential functions
   Copyright 2002, 2004 Free Software Foundation, Inc.
   Contributed by Paul Brook <paul@nowt.org>

This file is part of the GNU Fortran 95 runtime library (libgfor).

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
write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */
#include <math.h>
#include "libgfortran.h"


/* z = a + ib  */
/* Absolute value.  */
GFC_REAL_4
cabsf (GFC_COMPLEX_4 z)
{
  return hypotf (REALPART (z), IMAGPART (z));
}

/* Complex argument.  The angle made with the +ve real axis.
   Range -pi-pi.  */
GFC_REAL_4
cargf (GFC_COMPLEX_4 z)
{
  GFC_REAL_4 arg;

  return atan2f (IMAGPART (z), REALPART (z));
}

/* exp(z) = exp(a)*(cos(b) + isin(b))  */
GFC_COMPLEX_4
cexpf (GFC_COMPLEX_4 z)
{
  GFC_REAL_4 a;
  GFC_REAL_4 b;
  GFC_COMPLEX_4 v;

  a = REALPART (z);
  b = IMAGPART (z);
  COMPLEX_ASSIGN (v, cosf (b), sinf (b));
  return expf (a) * v;
}

/* log(z) = log (cabs(z)) + i*carg(z)  */
GFC_COMPLEX_4
clogf (GFC_COMPLEX_4 z)
{
  GFC_COMPLEX_4 v;

  COMPLEX_ASSIGN (v, logf (cabsf (z)), cargf (z));
  return v;
}

/* log10(z) = log10 (cabs(z)) + i*carg(z)  */
GFC_COMPLEX_4
clog10f (GFC_COMPLEX_4 z)
{
  GFC_COMPLEX_4 v;

  COMPLEX_ASSIGN (v, log10f (cabsf (z)), cargf (z));
  return v;
}

/* pow(base, power) = cexp (power * clog (base))  */
GFC_COMPLEX_4
cpowf (GFC_COMPLEX_4 base, GFC_COMPLEX_4 power)
{
  return cexpf (power * clogf (base));
}

/* sqrt(z).  Algorithm pulled from glibc.  */
GFC_COMPLEX_4
csqrtf (GFC_COMPLEX_4 z)
{
  GFC_REAL_4 re;
  GFC_REAL_4 im;
  GFC_COMPLEX_4 v;

  re = REALPART (z);
  im = IMAGPART (z);
  if (im == 0.0)
    {
      if (re < 0.0)
        {
          COMPLEX_ASSIGN (v, 0.0, copysignf (sqrtf (-re), im));
        }
      else
        {
          COMPLEX_ASSIGN (v, fabsf (sqrt (re)),
                          copysignf (0.0, im));
        }
    }
  else if (re == 0.0)
    {
      GFC_REAL_4 r;

      r = sqrtf (0.5 * fabs (im));

      COMPLEX_ASSIGN (v, copysignf (r, im), r);
    }
  else
    {
      GFC_REAL_4 d, r, s;

      d = hypotf (re, im);
      /* Use the identity   2  Re res  Im res = Im x
         to avoid cancellation error in  d +/- Re x.  */
      if (re > 0)
        {
          r = sqrtf (0.5 * d + 0.5 * re);
          s = (0.5 * im) / r;
        }
      else
        {
          s = sqrtf (0.5 * d - 0.5 * re);
          r = fabsf ((0.5 * im) / s);
        }

      COMPLEX_ASSIGN (v, r, copysignf (s, im));
    }
  return v;
}

