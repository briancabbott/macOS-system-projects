/* Generic implementation of the RESHAPE intrinsic
   Copyright 2002 Free Software Foundation, Inc.
   Contributed by Paul Brook <paul@nowt.org>

This file is part of the GNU Fortran 95 runtime library (libgfor).

Libgfor is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

Ligbfor is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with libgfor; see the file COPYING.LIB.  If not,
write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "libgfortran.h"

void
__unpack1 (const gfc_array_char * ret, const gfc_array_char * vector,
    const gfc_array_l4 * mask, const gfc_array_char * field)
{
  /* r.* indicates the return array.  */
  index_type rstride[GFC_MAX_DIMENSIONS];
  index_type rstride0;
  char *rptr;
  /* v.* indicates the vector array.  */
  index_type vstride0;
  char *vptr;
  /* f.* indicates the field array.  */
  index_type fstride[GFC_MAX_DIMENSIONS];
  index_type fstride0;
  const char *fptr;
  /* m.* indicates the mask array.  */
  index_type mstride[GFC_MAX_DIMENSIONS];
  index_type mstride0;
  const GFC_LOGICAL_4 *mptr;

  index_type count[GFC_MAX_DIMENSIONS];
  index_type extent[GFC_MAX_DIMENSIONS];
  index_type n;
  index_type dim;
  index_type size;
  index_type fsize;

  size = GFC_DESCRIPTOR_SIZE (ret);
  /* A field element size of 0 actually means this is a scalar.  */
  fsize = GFC_DESCRIPTOR_SIZE (field);
  dim = GFC_DESCRIPTOR_RANK (ret);
  for (n = 0; n < dim; n++)
    {
      count[n] = 0;
      extent[n] = ret->dim[n].ubound + 1 - ret->dim[n].lbound;
      rstride[n] = ret->dim[n].stride * size;
      fstride[n] = field->dim[n].stride * fsize;
      mstride[n] = mask->dim[n].stride;
    }
  if (rstride[0] == 0)
    rstride[0] = size;
  if (fstride[0] == 0)
    fstride[0] = fsize;
  if (mstride[0] == 0)
    mstride[0] = 1;

  vstride0 = vector->dim[0].stride * size;
  if (vstride0 == 0)
    vstride0 = size;
  rstride0 = rstride[0];
  fstride0 = fstride[0];
  mstride0 = mstride[0];
  rptr = ret->data;
  fptr = field->data;
  mptr = mask->data;
  vptr = vector->data;


  /* Use the same loop for both logical types. */
  if (GFC_DESCRIPTOR_SIZE (mask) != 4)
    {
      if (GFC_DESCRIPTOR_SIZE (mask) != 8)
        runtime_error ("Funny sized logical array");
      for (n = 0; n < dim; n++)
        mstride[n] <<= 1;
      mstride0 <<= 1;
      mptr = GFOR_POINTER_L8_TO_L4 (mptr);
    }

  while (rptr)
    {
      if (*mptr)
        {
          /* From vector.  */
          memcpy (rptr, vptr, size);
          vptr += vstride0;
        }
      else
        {
          /* From field.  */
          memcpy (rptr, fptr, size);
        }
      /* Advance to the next element.  */
      rptr += rstride0;
      fptr += fstride0;
      mptr += mstride0;
      count[0]++;
      n = 0;
      while (count[n] == extent[n])
        {
          /* When we get to the end of a dimension, reset it and increment
             the next dimension.  */
          count[n] = 0;
          /* We could precalculate these products, but this is a less
             frequently used path so proabably not worth it.  */
          rptr -= rstride[n] * extent[n];
          fptr -= fstride[n] * extent[n];
          mptr -= mstride[n] * extent[n];
          n++;
          if (n >= dim)
            {
              /* Break out of the loop.  */
              rptr = NULL;
              break;
            }
          else
            {
              count[n]++;
              rptr += rstride[n];
              fptr += fstride[n];
              mptr += mstride[n];
            }
        }
    }
}

void
__unpack0 (const gfc_array_char * ret, const gfc_array_char * vector,
    const gfc_array_l4 * mask, char * field)
{
  gfc_array_char tmp;

  tmp.dtype = 0;
  tmp.data = field;
  __unpack1 (ret, vector, mask, &tmp);
}

