/* Implementation of the RRSPACING intrinsic
   Copyright 2006 Free Software Foundation, Inc.
   Contributed by Steven G. Kargl <kargl@gcc.gnu.org>

This file is part of the GNU Fortran 95 runtime library (libgfortran).

Libgfortran is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

In addition to the permissions in the GNU General Public License, the
Free Software Foundation gives you unlimited permission to link the
compiled version of this file into combinations with other programs,
and to distribute those combinations without any restriction coming
from the use of this file.  (The General Public License restrictions
do apply in other respects; for example, they cover modification of
the file, and distribution when not linked into a combine
executable.)

Libgfortran is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public
License along with libgfortran; see the file COPYING.  If not,
write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

#include "config.h"
#include <math.h>
#include "libgfortran.h"


#if defined (HAVE_GFC_REAL_4) && defined (HAVE_FABSF) && defined (HAVE_FREXPF)

extern GFC_REAL_4 rrspacing_r4 (GFC_REAL_4 s, int p);
export_proto(rrspacing_r4);

GFC_REAL_4
rrspacing_r4 (GFC_REAL_4 s, int p)
{
  int e;
  GFC_REAL_4 x;
  x = fabsf (s);
  if (x == 0.)
    return 0.;
  frexpf (s, &e);
#if defined (HAVE_LDEXPF)
  return ldexpf (x, p - e);
#else
  return scalbnf (x, p - e);
#endif

}

#endif
