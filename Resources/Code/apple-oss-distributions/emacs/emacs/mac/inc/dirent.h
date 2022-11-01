/* Replacement dirent.h file for building GNU Emacs on the Macintosh.
   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004,
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

/* Contributed by Andrew Choi (akochoi@mac.com).  */

#ifndef _DIRENT_H
#define _DIRENT_H

/* for definition of FSSpec */
#include <Files.h>

/* for definition of ino_t */
#include <sys/types.h>

struct dirent {
  ino_t d_ino;
  char *d_name;
};

typedef struct DIR {
  long dir_id;
  short vol_ref_num;
  long current_index;
  int getting_volumes;  /* true if this DIR struct refers to the root directory */
} DIR;

extern DIR *opendir(const char *);
extern int closedir(DIR *);
extern struct dirent *readdir(DIR *);

#endif /* _DIRENT_H */

/* arch-tag: ec3116df-70f9-4a4a-b6d0-1858aaa9ea22
   (do not change this comment) */
