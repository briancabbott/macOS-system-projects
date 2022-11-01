/* Copyright (C) 2002-2003 Free Software Foundation, Inc.
   Contributed by Paul Brook

This file is part of the GNU Fortran 95 runtime library (libgfor).

Libgfor is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

Libgfor is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with libgfor; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include <string.h>

#include "libgfortran.h"


/* Compare a C-style string with a fortran style string in a case-insensitive
   manner.  Used for decoding string options to various statements.  Returns
   zero if not equal, nonzero if equal.  */

static int
compare0 (const char *s1, int s1_len, const char *s2)
{
  int i;

  if (strncasecmp (s1, s2, s1_len) != 0)
    return 0;

  /* The rest of s1 needs to be blanks for equality.  */

  for (i = strlen (s2); i < s1_len; i++)
    if (s1[i] != ' ')
      return 0;

  return 1;
}


/* Given a fortran string, return its length exclusive of the trailing
   spaces.  */
int
fstrlen (const char *string, int len)
{

  for (len--; len >= 0; len--)
    if (string[len] != ' ')
      break;

  return len + 1;
}



void
fstrcpy (char *dest, int destlen, const char *src, int srclen)
{

  if (srclen >= destlen)
    {
      /* This will truncate if too long.  */
      memcpy (dest, src, destlen);
    }
  else
    {
      memcpy (dest, src, srclen);
      /* Pad with spaces.  */
      memset (&dest[srclen], ' ', destlen - srclen);
    }
}


void
cf_strcpy (char *dest, int dest_len, const char *src)
{
  int src_len;

  src_len = strlen (src);

  if (src_len >= dest_len)
    {
      /* This will truncate if too long.  */
      memcpy (dest, src, dest_len);
    }
  else
    {
      memcpy (dest, src, src_len);
      /* Pad with spaces.  */
      memset (&dest[src_len], ' ', dest_len - src_len);
    }
}


/* Given a fortran string and an array of st_option structures, search through
   the array to find a match.  If the option is not found, we generate an error
   if no default is provided.  */

int
find_option (const char *s1, int s1_len, st_option * opts,
	     const char *error_message)
{

  for (; opts->name; opts++)
    if (compare0 (s1, s1_len, opts->name))
      return opts->value;

  generate_error (ERROR_BAD_OPTION, error_message);

  return -1;
}

