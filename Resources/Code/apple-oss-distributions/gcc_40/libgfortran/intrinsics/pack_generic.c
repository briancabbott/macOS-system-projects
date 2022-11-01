/* Generic implementation of the PACK intrinsic
   Copyright (C) 2002, 2004 Free Software Foundation, Inc.
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

/* PACK is specified as follows:

   13.14.80 PACK (ARRAY, MASK, [VECTOR])
   
   Description: Pack an array into an array of rank one under the
   control of a mask.

   Class: Transformational fucntion.

   Arguments:
      ARRAY   may be of any type. It shall not be scalar.
      MASK    shall be of type LOGICAL. It shall be conformable with ARRAY.
      VECTOR  (optional) shall be of the same type and type parameters
              as ARRAY. VECTOR shall have at least as many elements as
              there are true elements in MASK. If MASK is a scalar
              with the value true, VECTOR shall have at least as many 
              elements as there are in ARRAY.

   Result Characteristics: The result is an array of rank one with the
   same type and type parameters as ARRAY. If VECTOR is present, the
   result size is that of VECTOR; otherwise, the result size is the
   number /t/ of true elements in MASK unless MASK is scalar with the
   value true, in which case the result size is the size of ARRAY.

   Result Value: Element /i/ of the result is the element of ARRAY
   that corresponds to the /i/th true element of MASK, taking elements
   in array element order, for /i/ = 1, 2, ..., /t/. If VECTOR is
   present and has size /n/ > /t/, element /i/ of the result has the
   value VECTOR(/i/), for /i/ = /t/ + 1, ..., /n/.

   Examples: The nonzero elements of an array M with the value
   | 0 0 0 |
   | 9 0 0 | may be "gathered" by the function PACK. The result of
   | 0 0 7 |
   PACK (M, MASK = M.NE.0) is [9,7] and the result of PACK (M, M.NE.0,
   VECTOR = (/ 2,4,6,8,10,12 /)) is [9,7,6,8,10,12].  

There are two variants of the PACK intrinsic: one, where MASK is
array valued, and the other one where MASK is scalar.  */

void
__pack (gfc_array_char * ret, const gfc_array_char * array,
	const gfc_array_l4 * mask, const gfc_array_char * vector)
{
  /* r.* indicates the return array.  */
  index_type rstride0;
  char *rptr;
  /* s.* indicates the source array.  */
  index_type sstride[GFC_MAX_DIMENSIONS];
  index_type sstride0;
  const char *sptr;
  /* m.* indicates the mask array.  */
  index_type mstride[GFC_MAX_DIMENSIONS];
  index_type mstride0;
  const GFC_LOGICAL_4 *mptr;

  index_type count[GFC_MAX_DIMENSIONS];
  index_type extent[GFC_MAX_DIMENSIONS];
  index_type n;
  index_type dim;
  index_type size;
  index_type nelem;

  size = GFC_DESCRIPTOR_SIZE (array);
  dim = GFC_DESCRIPTOR_RANK (array);
  for (n = 0; n < dim; n++)
    {
      count[n] = 0;
      extent[n] = array->dim[n].ubound + 1 - array->dim[n].lbound;
      sstride[n] = array->dim[n].stride * size;
      mstride[n] = mask->dim[n].stride;
    }
  if (sstride[0] == 0)
    sstride[0] = size;
  if (mstride[0] == 0)
    mstride[0] = 1;

  sptr = array->data;
  mptr = mask->data;

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

  if (ret->data == NULL)
    {
      /* Allocate the memory for the result.  */
      int total;

      if (vector != NULL) 
	{ 

	  /* The return array will have as many
	     elements as there are in VECTOR.  */ 
	  total = vector->dim[0].ubound + 1 - vector->dim[0].lbound; 
	} 
      else 
	{ 
	  /* We have to count the true elements in MASK.  */ 

	  /* TODO: We could speed up pack easily in the case of only
	     few .TRUE. entries in MASK, by keeping track of where we
	     would be in the source array during the initial traversal
	     of MASK, and caching the pointers to those elements. Then,
	     supposed the number of elements is small enough, we would
	     only have to traverse the list, and copy those elements
	     into the result array. In the case of datatypes which fit
	     in one of the integer types we could also cache the
	     value instead of a pointer to it. 
	     This approach might be bad from the point of view of
	     cache behavior in the case where our cache is not big
	     enough to hold all elements that have to be copied.  */

	  const GFC_LOGICAL_4 *m = mptr;

	  total = 0;

	  while (m)
	    {
	      /* Test this element.  */
	      if (*m)
		total++;

	      /* Advance to the next element.  */
	      m += mstride[0];
	      count[0]++;
	      n = 0;
	      while (count[n] == extent[n])
		{
		  /* When we get to the end of a dimension, reset it
		     and increment the next dimension.  */
		  count[n] = 0;
		  /* We could precalculate this product, but this is a
		     less frequently used path so proabably not worth
		     it.  */
		  m -= mstride[n] * extent[n];
		  n++;
		  if (n >= dim)
		    {
		      /* Break out of the loop.  */
		      m = NULL;
		      break;
		    }
		  else
		    {
		      count[n]++;
		      mptr += mstride[n];
		    }
		}
	    }
	}
      
      /* Setup the array descriptor.  */
      ret->dim[0].lbound = 0;
      ret->dim[0].ubound = total - 1;
      ret->dim[0].stride = 1;

      ret->data = internal_malloc (size * total);
      ret->base = 0;

      if (total == 0)
	/* In this case, nothing remains to be done.  */
	return;
    }

  rstride0 = ret->dim[0].stride * size;
  if (rstride0 == 0)
    rstride0 = size;
  sstride0 = sstride[0];
  mstride0 = mstride[0];
  rptr = ret->data;

  while (sptr)
    {
      /* Test this element.  */
      if (*mptr)
        {
          /* Add it.  */
          memcpy (rptr, sptr, size);
          rptr += rstride0;
        }
      /* Advance to the next element.  */
      sptr += sstride0;
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
          sptr -= sstride[n] * extent[n];
          mptr -= mstride[n] * extent[n];
          n++;
          if (n >= dim)
            {
              /* Break out of the loop.  */
              sptr = NULL;
              break;
            }
          else
            {
              count[n]++;
              sptr += sstride[n];
              mptr += mstride[n];
            }
        }
    }

  /* Add any remaining elements from VECTOR.  */
  if (vector)
    {
      n = vector->dim[0].ubound + 1 - vector->dim[0].lbound;
      nelem = ((rptr - ret->data) / rstride0);
      if (n > nelem)
        {
          sstride0 = vector->dim[0].stride * size;
          if (sstride0 == 0)
            sstride0 = size;

          sptr = vector->data + sstride0 * nelem;
          n -= nelem;
          while (n--)
            {
              memcpy (rptr, sptr, size);
              rptr += rstride0;
              sptr += sstride0;
            }
        }
    }
}

void
__pack_s (gfc_array_char * ret, const gfc_array_char * array,
	  const GFC_LOGICAL_4 * mask, const gfc_array_char * vector)
{
  /* r.* indicates the return array.  */
  index_type rstride0;
  char *rptr;
  /* s.* indicates the source array.  */
  index_type sstride[GFC_MAX_DIMENSIONS];
  index_type sstride0;
  const char *sptr;

  index_type count[GFC_MAX_DIMENSIONS];
  index_type extent[GFC_MAX_DIMENSIONS];
  index_type n;
  index_type dim;
  index_type size;
  index_type nelem;

  size = GFC_DESCRIPTOR_SIZE (array);
  dim = GFC_DESCRIPTOR_RANK (array);
  for (n = 0; n < dim; n++)
    {
      count[n] = 0;
      extent[n] = array->dim[n].ubound + 1 - array->dim[n].lbound;
      sstride[n] = array->dim[n].stride * size;
    }
  if (sstride[0] == 0)
    sstride[0] = size;

  sstride0 = sstride[0];
  sptr = array->data;

  if (ret->data == NULL)
    {
      /* Allocate the memory for the result.  */
      int total;

      if (vector != NULL)
	{
	  /* The return array will have as many elements as there are
	     in vector.  */
	  total = vector->dim[0].ubound + 1 - vector->dim[0].lbound;
	}
      else
	{
	  if (*mask)
	    {
	      /* The result array will have as many elements as the input
		 array.  */
	      total = extent[0];
	      for (n = 1; n < dim; n++)
		total *= extent[n];
	    }
	  else
	    {
	      /* The result array will be empty.  */
	      ret->dim[0].lbound = 0;
	      ret->dim[0].ubound = -1;
	      ret->dim[0].stride = 1;
	      ret->data = internal_malloc (0);
	      ret->base = 0;
	      
	      return;
	    }
	}

      /* Setup the array descriptor.  */
      ret->dim[0].lbound = 0;
      ret->dim[0].ubound = total - 1;
      ret->dim[0].stride = 1;

      ret->data = internal_malloc (size * total);
      ret->base = 0;
    }

  rstride0 = ret->dim[0].stride * size;
  if (rstride0 == 0)
    rstride0 = size;
  rptr = ret->data;

  /* The remaining possibilities are now: 
       If MASK is .TRUE., we have to copy the source array into the
     result array. We then have to fill it up with elements from VECTOR.
       If MASK is .FALSE., we have to copy VECTOR into the result
     array. If VECTOR were not present we would have already returned.  */

  if (*mask)
    {
      while (sptr)
	{
	  /* Add this element.  */
	  memcpy (rptr, sptr, size);
	  rptr += rstride0;

	  /* Advance to the next element.  */
	  sptr += sstride0;
	  count[0]++;
	  n = 0;
	  while (count[n] == extent[n])
	    {
	      /* When we get to the end of a dimension, reset it and
		 increment the next dimension.  */
	      count[n] = 0;
	      /* We could precalculate these products, but this is a
		 less frequently used path so proabably not worth it.  */
	      sptr -= sstride[n] * extent[n];
	      n++;
	      if (n >= dim)
		{
		  /* Break out of the loop.  */
		  sptr = NULL;
		  break;
		}
	      else
		{
		  count[n]++;
		  sptr += sstride[n];
		}
	    }
	}
    }
  
  /* Add any remaining elements from VECTOR.  */
  if (vector)
    {
      n = vector->dim[0].ubound + 1 - vector->dim[0].lbound;
      nelem = ((rptr - ret->data) / rstride0);
      if (n > nelem)
        {
          sstride0 = vector->dim[0].stride * size;
          if (sstride0 == 0)
            sstride0 = size;

          sptr = vector->data + sstride0 * nelem;
          n -= nelem;
          while (n--)
            {
              memcpy (rptr, sptr, size);
              rptr += rstride0;
              sptr += sstride0;
            }
        }
    }
}
