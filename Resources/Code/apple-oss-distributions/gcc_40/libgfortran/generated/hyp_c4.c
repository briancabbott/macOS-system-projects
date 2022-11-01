/* Complex hyperbolic functions
   Copyright 2002 Free Software Foundation, Inc.
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


/* Complex number z = a + ib.  */

/* sinh(z) = sinh(a)cos(b) + icosh(a)sin(b)  */
GFC_COMPLEX_4
csinhf (GFC_COMPLEX_4 a)
{
  GFC_REAL_4 r;
  GFC_REAL_4 i;
  GFC_COMPLEX_4 v;

  r = REALPART (a);
  i = IMAGPART (a);
  COMPLEX_ASSIGN (v, sinhf (r) * cosf (i), coshf (r) * sinf (i));
  return v;
}

/* cosh(z) = cosh(a)cos(b) - isinh(a)sin(b)  */
GFC_COMPLEX_4
ccoshf (GFC_COMPLEX_4 a)
{
  GFC_REAL_4 r;
  GFC_REAL_4 i;
  GFC_COMPLEX_4 v;

  r = REALPART (a);
  i = IMAGPART (a);
  COMPLEX_ASSIGN (v, coshf (r) * cosf (i), - (sinhf (r) * sinf (i)));
  return v;
}

/* tanh(z) = (tanh(a) + itan(b)) / (1 - itanh(a)tan(b))  */
GFC_COMPLEX_4
ctanhf (GFC_COMPLEX_4 a)
{
  GFC_REAL_4 rt;
  GFC_REAL_4 it;
  GFC_COMPLEX_4 n;
  GFC_COMPLEX_4 d;

  rt = tanhf (REALPART (a));
  it = tanf (IMAGPART (a));
  COMPLEX_ASSIGN (n, rt, it);
  COMPLEX_ASSIGN (d, 1, - (rt * it));

  return n / d;
}

