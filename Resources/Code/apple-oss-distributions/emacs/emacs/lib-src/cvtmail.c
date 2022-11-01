/* Copyright (C) 1985, 1994, 2001, 2002, 2003, 2004,
                 2005, 2006, 2007  Free Software Foundation, Inc.

This file is part of GNU Emacs.

GNU Emacs is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Emacs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

/* cvtmail:
 * Program to convert oldstyle goslings emacs mail directories into
 * gnu-rmail format.  Program expects a directory called Messages to
 * exist in your home directory, containing individual mail messages in
 * separate files in the standard gosling emacs mail reader format.
 *
 * Program takes one argument: an output file.  This file will contain
 * all the messages in Messages directory, in berkeley mail format.
 * If no output file is mentioned, messages are put in ~/OMAIL.
 *
 * In order to get rmail to read the messages, the resulting file must
 * be mv'ed to ~/mbox, and then have rmail invoked on them.
 *
 * Author: Larry Kolodney, 1985
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#ifndef HAVE_STDLIB_H
char *getenv ();
#endif

char *xmalloc __P ((unsigned));
char *xrealloc __P ((char *, unsigned));
void skip_to_lf __P ((FILE *));
void sysfail __P ((char *));

int
main (argc, argv)
     int argc;
     char *argv[];
{
  char *hd;
  char *md;
  char *mdd;
  char *mfile;
  char *cf;
  int cflen;
  FILE *mddf;
  FILE *mfilef;
  FILE *cff;
  char pre[10];
  char name[14];
  int c;

  hd = (char *) getenv ("HOME");

  md = (char *) xmalloc (strlen (hd) + 10);
  strcpy (md, hd);
  strcat (md, "/Messages");

  mdd = (char *) xmalloc (strlen (md) + 11);
  strcpy (mdd, md);
  strcat (mdd, "/Directory");

  cflen = 100;
  cf = (char *) xmalloc (cflen);

  mddf = fopen (mdd, "r");
  if (!mddf)
    sysfail (mdd);
  if (argc > 1)
    mfile = argv[1];
  else
    {
      mfile = (char *) xmalloc (strlen (hd) + 7);
      strcpy (mfile, hd);
      strcat (mfile, "/OMAIL");
    }
  mfilef = fopen (mfile, "w");
  if (!mfilef)
    sysfail (mfile);

  skip_to_lf (mddf);
  while (fscanf (mddf, "%4c%14[0123456789]", pre, name) != EOF)
    {
      if (cflen < strlen (md) + strlen (name) + 2)
	{
	  cflen = strlen (md) + strlen (name) + 2;
	  cf = (char *) xrealloc (cf, cflen);
	}
      strcpy (cf, md);
      strcat (cf,"/");
      strcat (cf, name);
      cff = fopen (cf, "r");
      if (!cff)
	perror (cf);
      else
	{
	  while ((c = getc(cff)) != EOF)
	    putc (c, mfilef);
	  putc ('\n', mfilef);
	  skip_to_lf (mddf);
	  fclose (cff);
	}
    }
  fclose (mddf);
  fclose (mfilef);
  return EXIT_SUCCESS;
}

void
skip_to_lf (stream)
     FILE *stream;
{
  register int c;
  while ((c = getc(stream)) != EOF && c != '\n')
    ;
}


void
error (s1, s2)
     char *s1, *s2;
{
  fprintf (stderr, "cvtmail: ");
  fprintf (stderr, s1, s2);
  fprintf (stderr, "\n");
}

/* Print error message and exit.  */

void
fatal (s1, s2)
     char *s1, *s2;
{
  error (s1, s2);
  exit (EXIT_FAILURE);
}

void
sysfail (s)
     char *s;
{
  fprintf (stderr, "cvtmail: ");
  perror (s);
  exit (EXIT_FAILURE);
}

char *
xmalloc (size)
     unsigned size;
{
  char *result = (char *) malloc (size);
  if (!result)
    fatal ("virtual memory exhausted", 0);
  return result;
}

char *
xrealloc (ptr, size)
     char *ptr;
     unsigned size;
{
  char *result = (char *) realloc (ptr, size);
  if (!result)
    fatal ("virtual memory exhausted", 0);
  return result;
}

/* arch-tag: b93c25a9-9012-44f1-b78b-9cc7aed44a7a
   (do not change this comment) */

/* cvtmail.c ends here */
