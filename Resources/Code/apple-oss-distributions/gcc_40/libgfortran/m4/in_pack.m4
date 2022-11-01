`/* Helper function for repacking arrays.
   Copyright 2003 Free Software Foundation, Inc.
   Contributed by Paul Brook <paul@nowt.org>

This file is part of the GNU Fortran 95 runtime library (libgfortran).

Libgfor is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

Ligbfor is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with libgfortran; see the file COPYING.LIB.  If not,
write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include <stdlib.h>
#include <assert.h>
#include "libgfortran.h"'
include(iparm.m4)dnl

/* Allocates a block of memory with internal_malloc if the array needs
   repacking.  */

dnl Only the kind (ie size) is used to name the function.
rtype_name *
`internal_pack_'rtype_kind (rtype * source)
{
  index_type count[GFC_MAX_DIMENSIONS - 1];
  index_type extent[GFC_MAX_DIMENSIONS - 1];
  index_type stride[GFC_MAX_DIMENSIONS - 1];
  index_type stride0;
  index_type dim;
  index_type ssize;
  const rtype_name *src;
  rtype_name *dest;
  rtype_name *destptr;
  int n;
  int packed;

  if (source->dim[0].stride == 0)
    {
      source->dim[0].stride = 1;
      return source->data;
    }

  dim = GFC_DESCRIPTOR_RANK (source);
  ssize = 1;
  packed = 1;
  for (n = 0; n < dim; n++)
    {
      count[n] = 0;
      stride[n] = source->dim[n].stride;
      extent[n] = source->dim[n].ubound + 1 - source->dim[n].lbound;
      if (extent[n] <= 0)
        {
          /* Do nothing.  */
          packed = 1;
          break;
        }

      if (ssize != stride[n])
        packed = 0;

      ssize *= extent[n];
    }

  if (packed)
    return source->data;

  /* Allocate storage for the destination.  */
  destptr = (rtype_name *)internal_malloc_size (ssize * rtype_kind);
  dest = destptr;
  src = source->data;
  stride0 = stride[0];


  while (src)
    {
      /* Copy the data.  */
      *(dest++) = *src;
      /* Advance to the next element.  */
      src += stride0;
      count[0]++;
      /* Advance to the next source element.  */
      n = 0;
      while (count[n] == extent[n])
        {
          /* When we get to the end of a dimension, reset it and increment
             the next dimension.  */
          count[n] = 0;
          /* We could precalculate these products, but this is a less
             frequently used path so proabably not worth it.  */
          src -= stride[n] * extent[n];
          n++;
          if (n == dim)
            {
              src = NULL;
              break;
            }
          else
            {
              count[n]++;
              src += stride[n];
            }
        }
    }
  return destptr;
}

