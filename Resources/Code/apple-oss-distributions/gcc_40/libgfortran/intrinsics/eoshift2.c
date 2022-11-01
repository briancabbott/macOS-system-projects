/* Generic implementation of the EOSHIFT intrinsic
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

static const char zeros[16] =
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/* TODO: make this work for large shifts when
   sizeof(int) < sizeof (index_type).  */

static void
__eoshift2 (gfc_array_char * ret, const gfc_array_char * array,
    int shift, const gfc_array_char * bound, int which)
{
  /* r.* indicates the return array.  */
  index_type rstride[GFC_MAX_DIMENSIONS - 1];
  index_type rstride0;
  index_type roffset;
  char *rptr;
  char *dest;
  /* s.* indicates the source array.  */
  index_type sstride[GFC_MAX_DIMENSIONS - 1];
  index_type sstride0;
  index_type soffset;
  const char *sptr;
  const char *src;
  /* b.* indicates the bound array.  */
  index_type bstride[GFC_MAX_DIMENSIONS - 1];
  index_type bstride0;
  const char *bptr;

  index_type count[GFC_MAX_DIMENSIONS - 1];
  index_type extent[GFC_MAX_DIMENSIONS - 1];
  index_type dim;
  index_type size;
  index_type len;
  index_type n;

  size = GFC_DESCRIPTOR_SIZE (ret);

  if (ret->data == NULL)
    {
      int i;

      ret->data = internal_malloc (size * size0 ((array_t *)array));
      ret->base = 0;
      ret->dtype = array->dtype;
      for (i = 0; i < GFC_DESCRIPTOR_RANK (array); i++)
        {
          ret->dim[i].lbound = 0;
          ret->dim[i].ubound = array->dim[i].ubound - array->dim[i].lbound;

          if (i == 0)
            ret->dim[i].stride = 1;
          else
            ret->dim[i].stride = (ret->dim[i-1].ubound + 1) * ret->dim[i-1].stride;
        }
    }

  which = which - 1;

  extent[0] = 1;
  count[0] = 0;
  size = GFC_DESCRIPTOR_SIZE (array);
  n = 0;
  for (dim = 0; dim < GFC_DESCRIPTOR_RANK (array); dim++)
    {
      if (dim == which)
        {
          roffset = ret->dim[dim].stride * size;
          if (roffset == 0)
            roffset = size;
          soffset = array->dim[dim].stride * size;
          if (soffset == 0)
            soffset = size;
          len = array->dim[dim].ubound + 1 - array->dim[dim].lbound;
        }
      else
        {
          count[n] = 0;
          extent[n] = array->dim[dim].ubound + 1 - array->dim[dim].lbound;
          rstride[n] = ret->dim[dim].stride * size;
          sstride[n] = array->dim[dim].stride * size;
          if (bound)
            bstride[n] = bound->dim[n].stride * size;
          else
            bstride[n] = 0;
          n++;
        }
    }
  if (sstride[0] == 0)
    sstride[0] = size;
  if (rstride[0] == 0)
    rstride[0] = size;
  if (bound && bstride[0] == 0)
    bstride[0] = size;

  dim = GFC_DESCRIPTOR_RANK (array);
  rstride0 = rstride[0];
  sstride0 = sstride[0];
  bstride0 = bstride[0];
  rptr = ret->data;
  sptr = array->data;
  if (bound)
    bptr = bound->data;
  else
    bptr = zeros;

  if (shift > 0)
    len = len - shift;
  else
    len = len + shift;

  while (rptr)
    {
      /* Do the shift for this dimension.  */
      if (shift > 0)
        {
          src = &sptr[shift * soffset];
          dest = rptr;
        }
      else
        {
          src = sptr;
          dest = &rptr[-shift * roffset];
        }
      for (n = 0; n < len; n++)
        {
          memcpy (dest, src, size);
          dest += roffset;
          src += soffset;
        }
      if (shift >= 0)
        {
          n = shift;
        }
      else
        {
          dest = rptr;
          n = -shift;
        }

      while (n--)
        {
          memcpy (dest, bptr, size);
          dest += roffset;
        }

      /* Advance to the next section.  */
      rptr += rstride0;
      sptr += sstride0;
      bptr += bstride0;
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
          sptr -= sstride[n] * extent[n];
          bptr -= bstride[n] * extent[n];
          n++;
          if (n >= dim - 1)
            {
              /* Break out of the loop.  */
              rptr = NULL;
              break;
            }
          else
            {
              count[n]++;
              rptr += rstride[n];
              sptr += sstride[n];
              bptr += bstride[n];
            }
        }
    }
}


void
__eoshift2_4 (gfc_array_char * ret, const gfc_array_char * array,
    const GFC_INTEGER_4 * pshift, const gfc_array_char * bound,
    const GFC_INTEGER_4 * pdim)
{
  __eoshift2 (ret, array, *pshift, bound, pdim ? *pdim : 1);
}


void
__eoshift2_8 (gfc_array_char * ret, const gfc_array_char * array,
    const GFC_INTEGER_8 * pshift, const gfc_array_char * bound,
    const GFC_INTEGER_8 * pdim)
{
  __eoshift2 (ret, array, *pshift, bound, pdim ? *pdim : 1);
}

