/* Replacement utime.h file for building GNU Emacs on the Macintosh.
   Copyright (C) 2000, 2001, 2002, 2003, 2004, 2005,
      2006, 2007  Free Software Foundation, Inc.

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

#ifndef _UTIME_H_
#define _UTIME_H_

#include <time.h>

#define _mac_unix_epoch_offset_  (365L * 4L) * 24L * 60L * 60L

struct utimbuf {
	time_t actime;					/* access time (ignored on the Mac) */
	time_t modtime;					/* modification time */
};

int utime(const char *path, const struct utimbuf *buf);

#endif

/* arch-tag: 52dc3f6b-6122-4568-8f09-a5a56de6a324
   (do not change this comment) */
