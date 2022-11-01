dnl Support macro file for intrinsic functions.
dnl Contains the generic sections of the array functions.
dnl This file is part of the GNU Fortran 95 Runtime Library (libgfortran)
dnl Distributed under the GNU GPL with exception.  See COPYING for details.
dnl
dnl Pass the implementation for a single section as the parameter to
dnl {MASK_}ARRAY_FUNCTION.
dnl The variables base, delta, and len describe the input section.
dnl For masked section the mask is described by mbase and mdelta.
dnl These should not be modified. The result should be stored in *dest.
dnl The names count, extent, sstride, dstride, base, dest, rank, dim
dnl retarray, array, pdim and mstride should not be used.
dnl The variable n is declared as index_type and may be used.
dnl Other variable declarations may be placed at the start of the code,
dnl The types of the array parameter and the return value are
dnl atype_name and rtype_name respectively.
dnl Execution should be allowed to continue to the end of the block.
dnl You should not return or break from the inner loop of the implementation.
dnl Care should also be taken to avoid using the names defined in iparm.m4
define(START_ARRAY_FUNCTION,
`
extern void name`'rtype_qual`_'atype_code (rtype * const restrict, 
	atype * const restrict, const index_type * const restrict);
export_proto(name`'rtype_qual`_'atype_code);

void
name`'rtype_qual`_'atype_code (rtype * const restrict retarray, 
	atype * const restrict array, 
	const index_type * const restrict pdim)
{
  index_type count[GFC_MAX_DIMENSIONS];
  index_type extent[GFC_MAX_DIMENSIONS];
  index_type sstride[GFC_MAX_DIMENSIONS];
  index_type dstride[GFC_MAX_DIMENSIONS];
  const atype_name * restrict base;
  rtype_name * restrict dest;
  index_type rank;
  index_type n;
  index_type len;
  index_type delta;
  index_type dim;

  /* Make dim zero based to avoid confusion.  */
  dim = (*pdim) - 1;
  rank = GFC_DESCRIPTOR_RANK (array) - 1;

  len = array->dim[dim].ubound + 1 - array->dim[dim].lbound;
  delta = array->dim[dim].stride;

  for (n = 0; n < dim; n++)
    {
      sstride[n] = array->dim[n].stride;
      extent[n] = array->dim[n].ubound + 1 - array->dim[n].lbound;

      if (extent[n] < 0)
	extent[n] = 0;
    }
  for (n = dim; n < rank; n++)
    {
      sstride[n] = array->dim[n + 1].stride;
      extent[n] =
        array->dim[n + 1].ubound + 1 - array->dim[n + 1].lbound;

      if (extent[n] < 0)
	extent[n] = 0;
    }

  if (retarray->data == NULL)
    {
      size_t alloc_size;

      for (n = 0; n < rank; n++)
        {
          retarray->dim[n].lbound = 0;
          retarray->dim[n].ubound = extent[n]-1;
          if (n == 0)
            retarray->dim[n].stride = 1;
          else
            retarray->dim[n].stride = retarray->dim[n-1].stride * extent[n-1];
        }

      retarray->offset = 0;
      retarray->dtype = (array->dtype & ~GFC_DTYPE_RANK_MASK) | rank;

      alloc_size = sizeof (rtype_name) * retarray->dim[rank-1].stride
    		   * extent[rank-1];

      if (alloc_size == 0)
	{
	  /* Make sure we have a zero-sized array.  */
	  retarray->dim[0].lbound = 0;
	  retarray->dim[0].ubound = -1;
	  return;
	}
      else
	retarray->data = internal_malloc_size (alloc_size);
    }
  else
    {
      if (rank != GFC_DESCRIPTOR_RANK (retarray))
	runtime_error ("rank of return array incorrect");
    }

  for (n = 0; n < rank; n++)
    {
      count[n] = 0;
      dstride[n] = retarray->dim[n].stride;
      if (extent[n] <= 0)
        len = 0;
    }

  base = array->data;
  dest = retarray->data;

  while (base)
    {
      const atype_name * restrict src;
      rtype_name result;
      src = base;
      {
')dnl
define(START_ARRAY_BLOCK,
`        if (len <= 0)
	  *dest = '$1`;
	else
	  {
	    for (n = 0; n < len; n++, src += delta)
	      {
')dnl
define(FINISH_ARRAY_FUNCTION,
    `          }
	    *dest = result;
	  }
      }
      /* Advance to the next element.  */
      count[0]++;
      base += sstride[0];
      dest += dstride[0];
      n = 0;
      while (count[n] == extent[n])
        {
          /* When we get to the end of a dimension, reset it and increment
             the next dimension.  */
          count[n] = 0;
          /* We could precalculate these products, but this is a less
             frequently used path so probably not worth it.  */
          base -= sstride[n] * extent[n];
          dest -= dstride[n] * extent[n];
          n++;
          if (n == rank)
            {
              /* Break out of the look.  */
              base = NULL;
              break;
            }
          else
            {
              count[n]++;
              base += sstride[n];
              dest += dstride[n];
            }
        }
    }
}')dnl
define(START_MASKED_ARRAY_FUNCTION,
`
extern void `m'name`'rtype_qual`_'atype_code (rtype * const restrict, 
	atype * const restrict, const index_type * const restrict,
	gfc_array_l4 * const restrict);
export_proto(`m'name`'rtype_qual`_'atype_code);

void
`m'name`'rtype_qual`_'atype_code (rtype * const restrict retarray, 
	atype * const restrict array, 
	const index_type * const restrict pdim, 
	gfc_array_l4 * const restrict mask)
{
  index_type count[GFC_MAX_DIMENSIONS];
  index_type extent[GFC_MAX_DIMENSIONS];
  index_type sstride[GFC_MAX_DIMENSIONS];
  index_type dstride[GFC_MAX_DIMENSIONS];
  index_type mstride[GFC_MAX_DIMENSIONS];
  rtype_name * restrict dest;
  const atype_name * restrict base;
  const GFC_LOGICAL_4 * restrict mbase;
  int rank;
  int dim;
  index_type n;
  index_type len;
  index_type delta;
  index_type mdelta;

  dim = (*pdim) - 1;
  rank = GFC_DESCRIPTOR_RANK (array) - 1;

  len = array->dim[dim].ubound + 1 - array->dim[dim].lbound;
  if (len <= 0)
    return;
  delta = array->dim[dim].stride;
  mdelta = mask->dim[dim].stride;

  for (n = 0; n < dim; n++)
    {
      sstride[n] = array->dim[n].stride;
      mstride[n] = mask->dim[n].stride;
      extent[n] = array->dim[n].ubound + 1 - array->dim[n].lbound;

      if (extent[n] < 0)
	extent[n] = 0;

    }
  for (n = dim; n < rank; n++)
    {
      sstride[n] = array->dim[n + 1].stride;
      mstride[n] = mask->dim[n + 1].stride;
      extent[n] =
        array->dim[n + 1].ubound + 1 - array->dim[n + 1].lbound;

      if (extent[n] < 0)
	extent[n] = 0;
    }

  if (retarray->data == NULL)
    {
      size_t alloc_size;

      for (n = 0; n < rank; n++)
        {
          retarray->dim[n].lbound = 0;
          retarray->dim[n].ubound = extent[n]-1;
          if (n == 0)
            retarray->dim[n].stride = 1;
          else
            retarray->dim[n].stride = retarray->dim[n-1].stride * extent[n-1];
        }

      alloc_size = sizeof (rtype_name) * retarray->dim[rank-1].stride
    		   * extent[rank-1];

      retarray->offset = 0;
      retarray->dtype = (array->dtype & ~GFC_DTYPE_RANK_MASK) | rank;

      if (alloc_size == 0)
	{
	  /* Make sure we have a zero-sized array.  */
	  retarray->dim[0].lbound = 0;
	  retarray->dim[0].ubound = -1;
	  return;
	}
      else
	retarray->data = internal_malloc_size (alloc_size);

    }
  else
    {
      if (rank != GFC_DESCRIPTOR_RANK (retarray))
	runtime_error ("rank of return array incorrect");
    }

  for (n = 0; n < rank; n++)
    {
      count[n] = 0;
      dstride[n] = retarray->dim[n].stride;
      if (extent[n] <= 0)
        return;
    }

  dest = retarray->data;
  base = array->data;
  mbase = mask->data;

  if (GFC_DESCRIPTOR_SIZE (mask) != 4)
    {
      /* This allows the same loop to be used for all logical types.  */
      assert (GFC_DESCRIPTOR_SIZE (mask) == 8);
      for (n = 0; n < rank; n++)
        mstride[n] <<= 1;
      mdelta <<= 1;
      mbase = (GFOR_POINTER_L8_TO_L4 (mbase));
    }

  while (base)
    {
      const atype_name * restrict src;
      const GFC_LOGICAL_4 * restrict msrc;
      rtype_name result;
      src = base;
      msrc = mbase;
      {
')dnl
define(START_MASKED_ARRAY_BLOCK,
`        if (len <= 0)
	  *dest = '$1`;
	else
	  {
	    for (n = 0; n < len; n++, src += delta, msrc += mdelta)
	      {
')dnl
define(FINISH_MASKED_ARRAY_FUNCTION,
`              }
	    *dest = result;
	  }
      }
      /* Advance to the next element.  */
      count[0]++;
      base += sstride[0];
      mbase += mstride[0];
      dest += dstride[0];
      n = 0;
      while (count[n] == extent[n])
        {
          /* When we get to the end of a dimension, reset it and increment
             the next dimension.  */
          count[n] = 0;
          /* We could precalculate these products, but this is a less
             frequently used path so probably not worth it.  */
          base -= sstride[n] * extent[n];
          mbase -= mstride[n] * extent[n];
          dest -= dstride[n] * extent[n];
          n++;
          if (n == rank)
            {
              /* Break out of the look.  */
              base = NULL;
              break;
            }
          else
            {
              count[n]++;
              base += sstride[n];
              mbase += mstride[n];
              dest += dstride[n];
            }
        }
    }
}')dnl
define(SCALAR_ARRAY_FUNCTION,
`
extern void `s'name`'rtype_qual`_'atype_code (rtype * const restrict, 
	atype * const restrict, const index_type * const restrict,
	GFC_LOGICAL_4 *);
export_proto(`s'name`'rtype_qual`_'atype_code);

void
`s'name`'rtype_qual`_'atype_code (rtype * const restrict retarray, 
	atype * const restrict array, 
	const index_type * const restrict pdim, 
	GFC_LOGICAL_4 * mask)
{
  index_type rank;
  index_type n;
  index_type dstride;
  rtype_name *dest;

  if (*mask)
    {
      name`'rtype_qual`_'atype_code (retarray, array, pdim);
      return;
    }
    rank = GFC_DESCRIPTOR_RANK (array);
  if (rank <= 0)
    runtime_error ("Rank of array needs to be > 0");

  if (retarray->data == NULL)
    {
      retarray->dim[0].lbound = 0;
      retarray->dim[0].ubound = rank-1;
      retarray->dim[0].stride = 1;
      retarray->dtype = (retarray->dtype & ~GFC_DTYPE_RANK_MASK) | 1;
      retarray->offset = 0;
      retarray->data = internal_malloc_size (sizeof (rtype_name) * rank);
    }
  else
    {
      if (GFC_DESCRIPTOR_RANK (retarray) != 1)
	runtime_error ("rank of return array does not equal 1");

      if (retarray->dim[0].ubound + 1 - retarray->dim[0].lbound != rank)
        runtime_error ("dimension of return array incorrect");
    }

    dstride = retarray->dim[0].stride;
    dest = retarray->data;

    for (n = 0; n < rank; n++)
      dest[n * dstride] = $1 ;
}')dnl
define(ARRAY_FUNCTION,
`START_ARRAY_FUNCTION
$2
START_ARRAY_BLOCK($1)
$3
FINISH_ARRAY_FUNCTION')dnl
define(MASKED_ARRAY_FUNCTION,
`START_MASKED_ARRAY_FUNCTION
$2
START_MASKED_ARRAY_BLOCK($1)
$3
FINISH_MASKED_ARRAY_FUNCTION')dnl
