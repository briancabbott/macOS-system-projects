/* Implementation of the EXIT intrinsic.
   Copyright (C) 2004 Free Software Foundation, Inc.
   Contributed by Steven G. Kargl <kargls@comcast.net>.

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
#include "libgfortran.h"

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

/* SUBROUTINE EXIT(STATUS)
   INTEGER, INTENT(IN), OPTIONAL :: STATUS  */

extern void exit_i4 (GFC_INTEGER_4 *);
export_proto(exit_i4);

void
exit_i4 (GFC_INTEGER_4 * status)
{
  exit (status ? *status : 0);
}

extern void exit_i8 (GFC_INTEGER_8 *);
export_proto(exit_i8);

void
exit_i8 (GFC_INTEGER_8 * status)
{
  exit (status ? *status : 0);
}
