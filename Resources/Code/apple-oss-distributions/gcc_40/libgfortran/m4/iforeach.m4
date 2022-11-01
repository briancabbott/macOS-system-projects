dnl Support macro file for intrinsic functions.
dnl Contains the generic sections of the array functions.
dnl This file is part of the GNU Fortran 95 Runtime Library (libgfortran)
dnl Distributed under the GNU LGPL.  See COPYING for details.
define(START_FOREACH_FUNCTION,
`void
`__'name`'rtype_qual`_'atype_code (rtype * retarray, atype *array)
{
  index_type count[GFC_MAX_DIMENSIONS];
  index_type extent[GFC_MAX_DIMENSIONS];
  index_type sstride[GFC_MAX_DIMENSIONS];
  index_type dstride;
  atype_name *base;
  rtype_name *dest;
  index_type rank;
  index_type n;

  rank = GFC_DESCRIPTOR_RANK (array);
  assert (rank > 0);
  assert (GFC_DESCRIPTOR_RANK (retarray) == 1);
  assert (retarray->dim[0].ubound + 1 - retarray->dim[0].lbound == rank);
  if (array->dim[0].stride == 0)
    array->dim[0].stride = 1;
  if (retarray->dim[0].stride == 0)
    retarray->dim[0].stride = 1;

  dstride = retarray->dim[0].stride;
  dest = retarray->data;
  for (n = 0; n < rank; n++)
    {
      sstride[n] = array->dim[n].stride;
      extent[n] = array->dim[n].ubound + 1 - array->dim[n].lbound;
      count[n] = 0;
      if (extent[n] <= 0)
	{
	  /* Set the return value.  */
	  for (n = 0; n < rank; n++)
	    dest[n * dstride] = 0;
	  return;
	}
    }

  base = array->data;

  /* Initialize the return value.  */
  for (n = 0; n < rank; n++)
    dest[n * dstride] = 1;
  {
')dnl
define(START_FOREACH_BLOCK,
`  while (base)
    {
      {
        /* Implementation start.  */
')dnl
define(FINISH_FOREACH_FUNCTION,
`        /* Implementation end.  */
      }
      /* Advance to the next element.  */
      count[0]++;
      base += sstride[0];
      n = 0;
      while (count[n] == extent[n])
        {
          /* When we get to the end of a dimension, reset it and increment
             the next dimension.  */
          count[n] = 0;
          /* We could precalculate these products, but this is a less
             frequently used path so proabably not worth it.  */
          base -= sstride[n] * extent[n];
          n++;
          if (n == rank)
            {
              /* Break out of the loop.  */
              base = NULL;
              break;
            }
          else
            {
              count[n]++;
              base += sstride[n];
            }
        }
    }
  }
}')dnl
define(START_MASKED_FOREACH_FUNCTION,
`void
`__m'name`'rtype_qual`_'atype_code (rtype * retarray, atype *array, gfc_array_l4 * mask)
{
  index_type count[GFC_MAX_DIMENSIONS];
  index_type extent[GFC_MAX_DIMENSIONS];
  index_type sstride[GFC_MAX_DIMENSIONS];
  index_type mstride[GFC_MAX_DIMENSIONS];
  index_type dstride;
  rtype_name *dest;
  atype_name *base;
  GFC_LOGICAL_4 *mbase;
  int rank;
  index_type n;

  rank = GFC_DESCRIPTOR_RANK (array);
  assert (rank > 0);
  assert (GFC_DESCRIPTOR_RANK (retarray) == 1);
  assert (retarray->dim[0].ubound + 1 - retarray->dim[0].lbound == rank);
  assert (GFC_DESCRIPTOR_RANK (mask) == rank);

  if (array->dim[0].stride == 0)
    array->dim[0].stride = 1;
  if (retarray->dim[0].stride == 0)
    retarray->dim[0].stride = 1;
  if (retarray->dim[0].stride == 0)
    retarray->dim[0].stride = 1;

  dstride = retarray->dim[0].stride;
  dest = retarray->data;
  for (n = 0; n < rank; n++)
    {
      sstride[n] = array->dim[n].stride;
      mstride[n] = mask->dim[n].stride;
      extent[n] = array->dim[n].ubound + 1 - array->dim[n].lbound;
      count[n] = 0;
      if (extent[n] <= 0)
	{
	  /* Set the return value.  */
	  for (n = 0; n < rank; n++)
	    dest[n * dstride] = 0;
	  return;
	}
    }

  base = array->data;
  mbase = mask->data;

  if (GFC_DESCRIPTOR_SIZE (mask) != 4)
    {
      /* This allows the same loop to be used for all logical types.  */
      assert (GFC_DESCRIPTOR_SIZE (mask) == 8);
      for (n = 0; n < rank; n++)
        mstride[n] <<= 1;
      mbase = (GFOR_POINTER_L8_TO_L4 (mbase));
    }


  /* Initialize the return value.  */
  for (n = 0; n < rank; n++)
    dest[n * dstride] = 1;
  {
')dnl
define(START_MASKED_FOREACH_BLOCK, `START_FOREACH_BLOCK')dnl
define(FINISH_MASKED_FOREACH_FUNCTION,
`        /* Implementation end.  */
      }
      /* Advance to the next element.  */
      count[0]++;
      base += sstride[0];
      mbase += mstride[0];
      n = 0;
      while (count[n] == extent[n])
        {
          /* When we get to the end of a dimension, reset it and increment
             the next dimension.  */
          count[n] = 0;
          /* We could precalculate these products, but this is a less
             frequently used path so proabably not worth it.  */
          base -= sstride[n] * extent[n];
          mbase -= mstride[n] * extent[n];
          n++;
          if (n == rank)
            {
              /* Break out of the loop.  */
              base = NULL;
              break;
            }
          else
            {
              count[n]++;
              base += sstride[n];
              mbase += mstride[n];
            }
        }
    }
  }
}')dnl
define(FOREACH_FUNCTION,
`START_FOREACH_FUNCTION
$1
START_FOREACH_BLOCK
$2
FINISH_FOREACH_FUNCTION')dnl
define(MASKED_FOREACH_FUNCTION,
`START_MASKED_FOREACH_FUNCTION
$1
START_MASKED_FOREACH_BLOCK
$2
FINISH_MASKED_FOREACH_FUNCTION')dnl
