/* Copyright (C) 2002, 2003, 2004 Free Software Foundation, Inc.
   Contributed by Andy Vaught

This file is part of the GNU Fortran 95 runtime library (libgfortran).

Libgfortran is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Libgfortran is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Libgfortran; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include <string.h>
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include "libgfortran.h"
#include "io.h"


#define star_fill(p, n) memset(p, '*', n)


typedef enum
{ SIGN_NONE, SIGN_MINUS, SIGN_PLUS }
sign_t;


void
write_a (fnode * f, const char *source, int len)
{
  int wlen;
  char *p;

  wlen = f->u.string.length < 0 ? len : f->u.string.length;

  p = write_block (wlen);
  if (p == NULL)
    return;

  if (wlen < len)
    memcpy (p, source, wlen);
  else
    {
      memset (p, ' ', wlen - len);
      memcpy (p + wlen - len, source, len);
    }
}

static int64_t
extract_int (const void *p, int len)
{
  int64_t i = 0;

  if (p == NULL)
    return i;

  switch (len)
    {
    case 1:
      i = *((const int8_t *) p);
      break;
    case 2:
      i = *((const int16_t *) p);
      break;
    case 4:
      i = *((const int32_t *) p);
      break;
    case 8:
      i = *((const int64_t *) p);
      break;
    default:
      internal_error ("bad integer kind");
    }

  return i;
}

static double
extract_real (const void *p, int len)
{
  double i = 0.0;
  switch (len)
    {
    case 4:
      i = *((const float *) p);
      break;
    case 8:
      i = *((const double *) p);
      break;
    default:
      internal_error ("bad real kind");
    }
  return i;

}


/* Given a flag that indicate if a value is negative or not, return a
   sign_t that gives the sign that we need to produce.  */

static sign_t
calculate_sign (int negative_flag)
{
  sign_t s = SIGN_NONE;

  if (negative_flag)
    s = SIGN_MINUS;
  else
    switch (g.sign_status)
      {
      case SIGN_SP:
	s = SIGN_PLUS;
	break;
      case SIGN_SS:
	s = SIGN_NONE;
	break;
      case SIGN_S:
	s = options.optional_plus ? SIGN_PLUS : SIGN_NONE;
	break;
      }

  return s;
}


/* Returns the value of 10**d.  */

static double
calculate_exp (int d)
{
  int i;
  double r = 1.0;

  for (i = 0; i< (d >= 0 ? d : -d); i++)
    r *= 10;

  r = (d >= 0) ? r : 1.0 / r;

  return r;
}


/* Generate corresponding I/O format for FMT_G output.
   The rules to translate FMT_G to FMT_E or FMT_F from DEC fortran
   LRM (table 11-2, Chapter 11, "I/O Formatting", P11-25) is:

   Data Magnitude                              Equivalent Conversion
   0< m < 0.1-0.5*10**(-d-1)                   Ew.d[Ee]
   m = 0                                       F(w-n).(d-1), n' '
   0.1-0.5*10**(-d-1)<= m < 1-0.5*10**(-d)     F(w-n).d, n' '
   1-0.5*10**(-d)<= m < 10-0.5*10**(-d+1)      F(w-n).(d-1), n' '
   10-0.5*10**(-d+1)<= m < 100-0.5*10**(-d+2)  F(w-n).(d-2), n' '
   ................                           ..........
   10**(d-1)-0.5*10**(-1)<= m <10**d-0.5       F(w-n).0,n(' ')
   m >= 10**d-0.5                              Ew.d[Ee]

   notes: for Gw.d ,  n' ' means 4 blanks
          for Gw.dEe, n' ' means e+2 blanks  */

static fnode *
calculate_G_format (fnode *f, double value, int len, int *num_blank)
{
  int e = f->u.real.e;
  int d = f->u.real.d;
  int w = f->u.real.w;
  fnode *newf;
  double m, exp_d;
  int low, high, mid;
  int ubound, lbound;

  newf = get_mem (sizeof (fnode));

  /* Absolute value.  */
  m = (value > 0.0) ? value : -value;

  /* In case of the two data magnitude ranges,
     generate E editing, Ew.d[Ee].  */
  exp_d = calculate_exp (d);
  if ((m > 0.0 && m < 0.1 - 0.05 / (double) exp_d)
      || (m >= (double) exp_d - 0.5 ))
    {
      newf->format = FMT_E;
      newf->u.real.w = w;
      newf->u.real.d = d;
      newf->u.real.e = e;
      *num_blank = 0;
      return newf;
    }

  /* Use binary search to find the data magnitude range.  */
  mid = 0;
  low = 0;
  high = d + 1;
  lbound = 0;
  ubound = d + 1;

  while (low <= high)
    {
      double temp;
      mid = (low + high) / 2;

      /* 0.1 * 10**mid - 0.5 * 10**(mid-d-1)  */
      temp = 0.1 * calculate_exp (mid) - 0.5 * calculate_exp (mid - d - 1);

      if (m < temp)
        {
          ubound = mid;
          if (ubound == lbound + 1)
            break;
          high = mid - 1;
        }
      else if (m > temp)
        {
          lbound = mid;
          if (ubound == lbound + 1)
            {
              mid ++;
              break;
            }
          low = mid + 1;
        }
      else
        break;
    }

  /* Pad with blanks where the exponent would be.  */
  if (e < 0)
    *num_blank = 4;
  else
    *num_blank = e + 2;

  /* Generate the F editing. F(w-n).(-(mid-d-1)), n' '.  */
  newf->format = FMT_F;
  newf->u.real.w = f->u.real.w - *num_blank;

  /* Special case.  */
  if (m == 0.0)
    newf->u.real.d = d - 1;
  else
    newf->u.real.d = - (mid - d - 1);

  /* For F editing, the scale factor is ignored.  */
  g.scale_factor = 0;
  return newf;
}


/* Output a real number according to its format which is FMT_G free.  */

static void
output_float (fnode *f, double value, int len)
{
  /* This must be large enough to accurately hold any value.  */ 
  char buffer[32];
  char *out;
  char *digits;
  int e;
  char expchar;
  format_token ft;
  int w;
  int d;
  int edigits;
  int ndigits;
  /* Number of digits before the decimal point.  */
  int nbefore;
  /* Number of zeros after the decimal point.  */
  int nzero;
  /* Number of digits after the decimal point.  */
  int nafter;
  int leadzero;
  int nblanks;
  int i;
  sign_t sign;

  ft = f->format;
  w = f->u.real.w;
  d = f->u.real.d;

  /* We should always know the field width and precision.  */
  if (d < 0)
    internal_error ("Uspecified precision");

  /* Use sprintf to print the number in the format +D.DDDDe+ddd
     For an N digit exponent, this gives us (32-6)-N digits after the
     decimal point, plus another one before the decimal point.  */
  sign = calculate_sign (value < 0.0);
  if (value < 0)
    value = -value;

  /* Printf always prints at least two exponent digits.  */
  if (value == 0)
    edigits = 2;
  else
    {
      edigits = 1 + (int) log10 (fabs(log10 (value)));
      if (edigits < 2)
	edigits = 2;
    }
  
  if (ft == FMT_F || ft == FMT_EN
      || ((ft == FMT_D || ft == FMT_E) && g.scale_factor != 0))
    {
      /* Always convert at full precision to avoid double rounding.  */
      ndigits = 27 - edigits;
    }
  else
    {
      /* We know the number of digits, so can let printf do the rounding
	 for us.  */
      if (ft == FMT_ES)
	ndigits = d + 1;
      else
	ndigits = d;
      if (ndigits > 27 - edigits)
	ndigits = 27 - edigits;
    }

  sprintf (buffer, "%+-#31.*e", ndigits - 1, value);
  
  /* Check the resulting string has punctuation in the correct places.  */
  if (buffer[2] != '.' || buffer[ndigits + 2] != 'e')
      internal_error ("printf is broken");

  /* Read the exponent back in.  */
  e = atoi (&buffer[ndigits + 3]) + 1;

  /* Make sure zero comes out as 0.0e0.  */
  if (value == 0.0)
    e = 0;

  /* Normalize the fractional component.  */
  buffer[2] = buffer[1];
  digits = &buffer[2];

  /* Figure out where to place the decimal point.  */
  switch (ft)
    {
    case FMT_F:
      nbefore = e + g.scale_factor;
      if (nbefore < 0)
	{
	  nzero = -nbefore;
	  if (nzero > d)
	    nzero = d;
	  nafter = d - nzero;
	  nbefore = 0;
	}
      else
	{
	  nzero = 0;
	  nafter = d;
	}
      expchar = 0;
      break;

    case FMT_E:
    case FMT_D:
      i = g.scale_factor;
      e -= i;
      if (i < 0)
	{
	  nbefore = 0;
	  nzero = -i;
	  nafter = d + i;
	}
      else if (i > 0)
	{
	  nbefore = i;
	  nzero = 0;
	  nafter = (d - i) + 1;
	}
      else /* i == 0 */
	{
	  nbefore = 0;
	  nzero = 0;
	  nafter = d;
	}

      if (ft = FMT_E)
	expchar = 'E';
      else
	expchar = 'D';
      break;

    case FMT_EN:
      /* The exponent must be a multiple of three, with 1-3 digits before
	 the decimal point.  */
      e--;
      if (e >= 0)
	nbefore = e % 3;
      else
	{
	  nbefore = (-e) % 3;
	  if (nbefore != 0)
	    nbefore = 3 - nbefore;
	}
      e -= nbefore;
      nbefore++;
      nzero = 0;
      nafter = d;
      expchar = 'E';
      break;

    case FMT_ES:
      e--;
      nbefore = 1;
      nzero = 0;
      nafter = d;
      expchar = 'E';
      break;

    default:
      /* Should never happen.  */
      internal_error ("Unexpected format token");
    }

  /* Round the value.  */
  if (nbefore + nafter == 0)
    ndigits = 0;
  else if (nbefore + nafter < ndigits)
    {
      ndigits = nbefore + nafter;
      i = ndigits;
      if (digits[i] >= '5')
	{
	  /* Propagate the carry.  */
	  for (i--; i >= 0; i--)
	    {
	      if (digits[i] != '9')
		{
		  digits[i]++;
		  break;
		}
	      digits[i] = '0';
	    }

	  if (i < 0)
	    {
	      /* The carry overflowed.  Fortunately we have some spare space
		 at the start of the buffer.  We may discard some digits, but
		 this is ok because we already know they are zero.  */
	      digits--;
	      digits[0] = '1';
	      if (ft == FMT_F)
		{
		  if (nzero > 0)
		    {
		      nzero--;
		      nafter++;
		    }
		  else
		    nbefore++;
		}
	      else if (ft == FMT_EN)
		{
		  nbefore++;
		  if (nbefore == 4)
		    {
		      nbefore = 1;
		      e += 3;
		    }
		}
	      else
		e++;
	    }
	}
    }

  /* Calculate the format of the exponent field.  */
  if (expchar)
    {
      edigits = 1;
      for (i = abs (e); i >= 10; i /= 10)
	edigits++;
      
      if (f->u.real.e < 0)
	{
	  /* Width not specified.  Must be no more than 3 digits.  */
	  if (e > 999 || e < -999)
	    edigits = -1;
	  else
	    {
	      edigits = 4;
	      if (e > 99 || e < -99)
		expchar = ' ';
	    }
	}
      else
	{
	  /* Exponent width specified, check it is wide enough.  */
	  if (edigits > f->u.real.e)
	    edigits = -1;
	  else
	    edigits = f->u.real.e + 2;
	}
    }
  else
    edigits = 0;

  /* Pick a field size if none was specified.  */
  if (w <= 0)
    w = nbefore + nzero + nafter + 2;

  /* Create the ouput buffer.  */
  out = write_block (w);
  if (out == NULL)
    return;

  /* Zero values always output as positive, even if the value was negative
     before rounding.  */
  for (i = 0; i < ndigits; i++)
    {
      if (digits[i] != '0')
	break;
    }
  if (i == ndigits)
    sign = calculate_sign (0);

  /* Work out how much padding is needed.  */
  nblanks = w - (nbefore + nzero + nafter + edigits + 1);
  if (sign != SIGN_NONE)
    nblanks--;
  
  /* Check the value fits in the specified field width.  */
  if (nblanks < 0 || edigits == -1)
    {
      star_fill (out, w);
      return;
    }

  /* See if we have space for a zero before the decimal point.  */
  if (nbefore == 0 && nblanks > 0)
    {
      leadzero = 1;
      nblanks--;
    }
  else
    leadzero = 0;

  /* Padd to full field width.  */
  if (nblanks > 0)
    {
      memset (out, ' ', nblanks);
      out += nblanks;
    }

  /* Output the initial sign (if any).  */
  if (sign == SIGN_PLUS)
    *(out++) = '+';
  else if (sign == SIGN_MINUS)
    *(out++) = '-';

  /* Output an optional leading zero.  */
  if (leadzero)
    *(out++) = '0';

  /* Output the part before the decimal point, padding with zeros.  */
  if (nbefore > 0)
    {
      if (nbefore > ndigits)
	i = ndigits;
      else
	i = nbefore;

      memcpy (out, digits, i);
      while (i < nbefore)
	out[i++] = '0';

      digits += i;
      ndigits -= i;
      out += nbefore;
    }
  /* Output the decimal point.  */
  *(out++) = '.';

  /* Output leading zeros after the decimal point.  */
  if (nzero > 0)
    {
      for (i = 0; i < nzero; i++)
	*(out++) = '0';
    }

  /* Output digits after the decimal point, padding with zeros.  */
  if (nafter > 0)
    {
      if (nafter > ndigits)
	i = ndigits;
      else
	i = nafter;

      memcpy (out, digits, i);
      while (i < nafter)
	out[i++] = '0';

      digits += i;
      ndigits -= i;
      out += nafter;
    }
  
  /* Output the exponent.  */
  if (expchar)
    {
      if (expchar != ' ')
	{
	  *(out++) = expchar;
	  edigits--;
	}
      snprintf (buffer, 32, "%+0*d", edigits, e);
      memcpy (out, buffer, edigits);
    }
}


void
write_l (fnode * f, char *source, int len)
{
  char *p;
  int64_t n;

  p = write_block (f->u.w);
  if (p == NULL)
    return;

  memset (p, ' ', f->u.w - 1);
  n = extract_int (source, len);
  p[f->u.w - 1] = (n) ? 'T' : 'F';
}

/* Output a real number according to its format.  */

static void
write_float (fnode *f, const char *source, int len)
{
  double n;
  int nb =0, res;
  char * p, fin;
  fnode *f2 = NULL;

  n = extract_real (source, len);

  if (f->format != FMT_B && f->format != FMT_O && f->format != FMT_Z)
   {
     res = finite (n);
     if (res == 0)
       {
         nb =  f->u.real.w;
         p = write_block (nb);
         if (nb < 3)
         {
             memset (p, '*',nb);
             return;
         }

         memset(p, ' ', nb);
         res = !isnan (n); 
         if (res != 0)
         {
            if (signbit(n))   
               fin = '-';
            else
               fin = '+';

            if (nb > 7)
               memcpy(p + nb - 8, "Infinity", 8); 
            else
               memcpy(p + nb - 3, "Inf", 3);
            if (nb < 8 && nb > 3)
               p[nb - 4] = fin;
            else if (nb > 8)
               p[nb - 9] = fin; 
          }
         else
             memcpy(p + nb - 3, "NaN", 3);
         return;
       }
   }

  if (f->format != FMT_G)
    {
      output_float (f, n, len);
    }
  else
    {
      f2 = calculate_G_format(f, n, len, &nb);
      output_float (f2, n, len);
      if (f2 != NULL)
        free_mem(f2);

      if (nb > 0)
        {
          p = write_block (nb);
          memset (p, ' ', nb);
        }
    }
}


static void
write_int (fnode *f, const char *source, int len, char *(*conv) (uint64_t))
{
  uint32_t ns =0;
  uint64_t n = 0;
  int w, m, digits, nzero, nblank;
  char *p, *q;

  w = f->u.integer.w;
  m = f->u.integer.m;

  n = extract_int (source, len);

  /* Special case:  */

  if (m == 0 && n == 0)
    {
      if (w == 0)
        w = 1;

      p = write_block (w);
      if (p == NULL)
        return;

      memset (p, ' ', w);
      goto done;
    }


  if (len < 8)
     {
       ns = n;
       q = conv (ns);
     }
  else
      q = conv (n);

  digits = strlen (q);

  /* Select a width if none was specified.  The idea here is to always
     print something.  */

  if (w == 0)
    w = ((digits < m) ? m : digits);

  p = write_block (w);
  if (p == NULL)
    return;

  nzero = 0;
  if (digits < m)
    nzero = m - digits;

  /* See if things will work.  */

  nblank = w - (nzero + digits);

  if (nblank < 0)
    {
      star_fill (p, w);
      goto done;
    }

  memset (p, ' ', nblank);
  p += nblank;

  memset (p, '0', nzero);
  p += nzero;

  memcpy (p, q, digits);

done:
  return;
}

static void
write_decimal (fnode *f, const char *source, int len, char *(*conv) (int64_t))
{
  int64_t n = 0;
  int w, m, digits, nsign, nzero, nblank;
  char *p, *q;
  sign_t sign;

  w = f->u.integer.w;
  m = f->u.integer.m;

  n = extract_int (source, len);

  /* Special case:  */

  if (m == 0 && n == 0)
    {
      if (w == 0)
        w = 1;

      p = write_block (w);
      if (p == NULL)
        return;

      memset (p, ' ', w);
      goto done;
    }

  sign = calculate_sign (n < 0);
  if (n < 0)
    n = -n;

  nsign = sign == SIGN_NONE ? 0 : 1;
  q = conv (n);

  digits = strlen (q);

  /* Select a width if none was specified.  The idea here is to always
     print something.  */

  if (w == 0)
    w = ((digits < m) ? m : digits) + nsign;

  p = write_block (w);
  if (p == NULL)
    return;

  nzero = 0;
  if (digits < m)
    nzero = m - digits;

  /* See if things will work.  */

  nblank = w - (nsign + nzero + digits);

  if (nblank < 0)
    {
      star_fill (p, w);
      goto done;
    }

  memset (p, ' ', nblank);
  p += nblank;

  switch (sign)
    {
    case SIGN_PLUS:
      *p++ = '+';
      break;
    case SIGN_MINUS:
      *p++ = '-';
      break;
    case SIGN_NONE:
      break;
    }

  memset (p, '0', nzero);
  p += nzero;

  memcpy (p, q, digits);

done:
  return;
}


/* Convert unsigned octal to ascii.  */

static char *
otoa (uint64_t n)
{
  char *p;

  if (n == 0)
    {
      scratch[0] = '0';
      scratch[1] = '\0';
      return scratch;
    }

  p = scratch + sizeof (SCRATCH_SIZE) - 1;
  *p-- = '\0';

  while (n != 0)
    {
      *p = '0' + (n & 7);
      p -- ;
      n >>= 3;
    }

  return ++p;
}


/* Convert unsigned binary to ascii.  */

static char *
btoa (uint64_t n)
{
  char *p;

  if (n == 0)
    {
      scratch[0] = '0';
      scratch[1] = '\0';
      return scratch;
    }

  p = scratch + sizeof (SCRATCH_SIZE) - 1;
  *p-- = '\0';

  while (n != 0)
    {
      *p-- = '0' + (n & 1);
      n >>= 1;
    }

  return ++p;
}


void
write_i (fnode * f, const char *p, int len)
{

  write_decimal (f, p, len, (void *) itoa);
}


void
write_b (fnode * f, const char *p, int len)
{

  write_int (f, p, len, btoa);
}


void
write_o (fnode * f, const char *p, int len)
{

  write_int (f, p, len, otoa);
}

void
write_z (fnode * f, const char *p, int len)
{

  write_int (f, p, len, xtoa);
}


void
write_d (fnode *f, const char *p, int len)
{

  write_float (f, p, len);
}


void
write_e (fnode *f, const char *p, int len)
{

  write_float (f, p, len);
}


void
write_f (fnode *f, const char *p, int len)
{

  write_float (f, p, len);
}


void
write_en (fnode *f, const char *p, int len)
{

  write_float (f, p, len);
}


void
write_es (fnode *f, const char *p, int len)
{

  write_float (f, p, len);
}


/* Take care of the X/TR descriptor.  */

void
write_x (fnode * f)
{
  char *p;

  p = write_block (f->u.n);
  if (p == NULL)
    return;

  memset (p, ' ', f->u.n);
}


/* List-directed writing.  */


/* Write a single character to the output.  Returns nonzero if
   something goes wrong.  */

static int
write_char (char c)
{
  char *p;

  p = write_block (1);
  if (p == NULL)
    return 1;

  *p = c;

  return 0;
}


/* Write a list-directed logical value.  */

static void
write_logical (const char *source, int length)
{
  write_char (extract_int (source, length) ? 'T' : 'F');
}


/* Write a list-directed integer value.  */

static void
write_integer (const char *source, int length)
{
  char *p;
  const char *q;
  int digits;
  int width;

  q = itoa (extract_int (source, length));

  switch (length)
    {
    case 1:
      width = 4;
      break;

    case 2:
      width = 6;
      break;

    case 4:
      width = 11;
      break;

    case 8:
      width = 20;
      break;

    default:
      width = 0;
      break;
    }

  digits = strlen (q);

  if(width < digits )
    width = digits ;
  p = write_block (width) ;

  memset(p ,' ', width - digits) ;
  memcpy (p + width - digits, q, digits);
}


/* Write a list-directed string.  We have to worry about delimiting
   the strings if the file has been opened in that mode.  */

static void
write_character (const char *source, int length)
{
  int i, extra;
  char *p, d;

  switch (current_unit->flags.delim)
    {
    case DELIM_APOSTROPHE:
      d = '\'';
      break;
    case DELIM_QUOTE:
      d = '"';
      break;
    default:
      d = ' ';
      break;
    }

  if (d == ' ')
    extra = 0;
  else
    {
      extra = 2;

      for (i = 0; i < length; i++)
	if (source[i] == d)
	  extra++;
    }

  p = write_block (length + extra);
  if (p == NULL)
    return;

  if (d == ' ')
    memcpy (p, source, length);
  else
    {
      *p++ = d;

      for (i = 0; i < length; i++)
	{
	  *p++ = source[i];
	  if (source[i] == d)
	    *p++ = d;
	}

      *p = d;
    }
}


/* Output a real number with default format.
   This is 1PG14.7E2 for REAL(4) and 1PG23.15E3 for REAL(8).  */

static void
write_real (const char *source, int length)
{
  fnode f ;
  int org_scale = g.scale_factor;
  f.format = FMT_G;
  g.scale_factor = 1;
  if (length < 8)
    {
      f.u.real.w = 14;
      f.u.real.d = 7;
      f.u.real.e = 2;
    }
  else
    {
      f.u.real.w = 23;
      f.u.real.d = 15;
      f.u.real.e = 3;
    }
  write_float (&f, source , length);
  g.scale_factor = org_scale;
}


static void
write_complex (const char *source, int len)
{

  if (write_char ('('))
    return;
  write_real (source, len);

  if (write_char (','))
    return;
  write_real (source + len, len);

  write_char (')');
}


/* Write the separator between items.  */

static void
write_separator (void)
{
  char *p;

  p = write_block (options.separator_len);
  if (p == NULL)
    return;

  memcpy (p, options.separator, options.separator_len);
}


/* Write an item with list formatting.
   TODO: handle skipping to the next record correctly, particularly
   with strings.  */

void
list_formatted_write (bt type, void *p, int len)
{
  static int char_flag;

  if (current_unit == NULL)
    return;

  if (g.first_item)
    {
      g.first_item = 0;
      char_flag = 0;
      write_char (' ');
    }
  else
    {
      if (type != BT_CHARACTER || !char_flag ||
	  current_unit->flags.delim != DELIM_NONE)
	write_separator ();
    }

  switch (type)
    {
    case BT_INTEGER:
      write_integer (p, len);
      break;
    case BT_LOGICAL:
      write_logical (p, len);
      break;
    case BT_CHARACTER:
      write_character (p, len);
      break;
    case BT_REAL:
      write_real (p, len);
      break;
    case BT_COMPLEX:
      write_complex (p, len);
      break;
    default:
      internal_error ("list_formatted_write(): Bad type");
    }

  char_flag = (type == BT_CHARACTER);
}

void
namelist_write (void)
{
   namelist_info * t1, *t2;
   int len,num;
   void * p;

   num = 0;
   write_character("&",1);
   write_character (ioparm.namelist_name, ioparm.namelist_name_len);
   write_character("\n",1);

   if (ionml != NULL)
     {
       t1 = ionml;
       while (t1 != NULL)
        {
          num ++;
          t2 = t1;
          t1 = t1->next;
          if (t2->var_name)
            {
              write_character(t2->var_name, strlen(t2->var_name));
              write_character("=",1);
            }
          len = t2->len;
          p = t2->mem_pos;
          switch (t2->type)
            {
            case BT_INTEGER:
              write_integer (p, len);
              break;
            case BT_LOGICAL:
              write_logical (p, len);
              break;
            case BT_CHARACTER:
              write_character (p, t2->string_length);
              break;
            case BT_REAL:
              write_real (p, len);
              break;
            case BT_COMPLEX:
              write_complex (p, len);
              break;
            default:
              internal_error ("Bad type for namelist write");
            }
         write_character(",",1);
         if (num > 5)
           {
              num = 0;
              write_character("\n",1);
           }
        }
     }
     write_character("/",1);

}
