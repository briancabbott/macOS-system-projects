`/* Implementation of the SHAPE intrinsic
   Copyright 2002, 2006 Free Software Foundation, Inc.
   Contributed by Paul Brook <paul@nowt.org>

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
#include <stdlib.h>
#include <assert.h>
#include "libgfortran.h"'
include(iparm.m4)dnl

`#if defined (HAVE_'rtype_name`)'

extern void shape_`'rtype_kind (rtype * const restrict ret, 
	const rtype * const restrict array);
export_proto(shape_`'rtype_kind);

void
shape_`'rtype_kind (rtype * const restrict ret, 
	const rtype * const restrict array)
{
  int n;
  index_type stride;

  stride = ret->dim[0].stride;

  for (n = 0; n < GFC_DESCRIPTOR_RANK (array); n++)
    {
      ret->data[n * stride] =
        array->dim[n].ubound + 1 - array->dim[n].lbound;
    }
}

#endif
