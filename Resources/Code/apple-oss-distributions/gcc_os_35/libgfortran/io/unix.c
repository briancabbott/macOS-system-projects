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

/* Unix stream I/O module */

#include "config.h"
#include <stdlib.h>
#include <limits.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#include <string.h>
#include <errno.h>

#include "libgfortran.h"
#include "io.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifndef MAP_FAILED
#define MAP_FAILED ((void *) -1)
#endif

#ifndef PROT_READ
#define PROT_READ 1
#endif

#ifndef PROT_WRITE
#define PROT_WRITE 2
#endif

/* This implementation of stream I/O is based on the paper:
 *
 *  "Exploiting the advantages of mapped files for stream I/O",
 *  O. Krieger, M. Stumm and R. Umrau, "Proceedings of the 1992 Winter
 *  USENIX conference", p. 27-42.
 *
 * It differs in a number of ways from the version described in the
 * paper.  First of all, threads are not an issue during I/O and we
 * also don't have to worry about having multiple regions, since
 * fortran's I/O model only allows you to be one place at a time.
 *
 * On the other hand, we have to be able to writing at the end of a
 * stream, read from the start of a stream or read and write blocks of
 * bytes from an arbitrary position.  After opening a file, a pointer
 * to a stream structure is returned, which is used to handle file
 * accesses until the file is closed.
 *
 * salloc_at_r(stream, len, where)-- Given a stream pointer, return a
 * pointer to a block of memory that mirror the file at position
 * 'where' that is 'len' bytes long.  The len integer is updated to
 * reflect how many bytes were actually read.  The only reason for a
 * short read is end of file.  The file pointer is updated.  The
 * pointer is valid until the next call to salloc_*.
 *
 * salloc_at_w(stream, len, where)-- Given the stream pointer, returns
 * a pointer to a block of memory that is updated to reflect the state
 * of the file.  The length of the buffer is always equal to that
 * requested.  The buffer must be completely set by the caller.  When
 * data has been written, the sfree() function must be called to
 * indicate that the caller is done writing data to the buffer.  This
 * may or may not cause a physical write.
 *
 * Short forms of these are salloc_r() and salloc_w() which drop the
 * 'where' parameter and use the current file pointer. */


#define BUFFER_SIZE 8192

typedef struct
{
  stream st;

  int fd;
  gfc_offset buffer_offset;	/* File offset of the start of the buffer */
  gfc_offset physical_offset;	/* Current physical file offset */
  gfc_offset logical_offset;	/* Current logical file offset */
  gfc_offset dirty_offset;	/* Start of modified bytes in buffer */
  gfc_offset file_length;	/* Length of the file, -1 if not seekable. */

  char *buffer;
  int len;			/* Physical length of the current buffer */
  int active;			/* Length of valid bytes in the buffer */

  int prot;
  int ndirty;			/* Dirty bytes starting at dirty_offset */

  unsigned unbuffered:1, mmaped:1;

  char small_buffer[BUFFER_SIZE];

}
unix_stream;

/*move_pos_offset()--  Move the record pointer right or left
 *relative to current position */

int
move_pos_offset (stream* st, int pos_off)
{
  unix_stream * str = (unix_stream*)st;
  if (pos_off < 0)
    {
      str->active  += pos_off;
      if (str->active < 0)
         str->active = 0;

      str->logical_offset  += pos_off;

      if (str->dirty_offset+str->ndirty > str->logical_offset)
        {
          if (str->ndirty +  pos_off > 0)
            str->ndirty += pos_off ;
          else
            {
              str->dirty_offset +=  pos_off + pos_off;
              str->ndirty = 0 ;
            }
        }

    return pos_off ;
  }
  return 0 ;
}


/* fix_fd()-- Given a file descriptor, make sure it is not one of the
 * standard descriptors, returning a non-standard descriptor.  If the
 * user specifies that system errors should go to standard output,
 * then closes standard output, we don't want the system errors to a
 * file that has been given file descriptor 1 or 0.  We want to send
 * the error to the invalid descriptor. */

static int
fix_fd (int fd)
{
  int input, output, error;

  input = output = error = 0;

/* Unix allocates the lowest descriptors first, so a loop is not
 * required, but this order is. */

  if (fd == STDIN_FILENO)
    {
      fd = dup (fd);
      input = 1;
    }
  if (fd == STDOUT_FILENO)
    {
      fd = dup (fd);
      output = 1;
    }
  if (fd == STDERR_FILENO)
    {
      fd = dup (fd);
      error = 1;
    }

  if (input)
    close (STDIN_FILENO);
  if (output)
    close (STDOUT_FILENO);
  if (error)
    close (STDERR_FILENO);

  return fd;
}


/* write()-- Write a buffer to a descriptor, allowing for short writes */

static int
writen (int fd, char *buffer, int len)
{
  int n, n0;

  n0 = len;

  while (len > 0)
    {
      n = write (fd, buffer, len);
      if (n < 0)
	return n;

      buffer += n;
      len -= n;
    }

  return n0;
}


#if 0
/* readn()-- Read bytes into a buffer, allowing for short reads.  If
 * fewer than len bytes are returned, it is because we've hit the end
 * of file. */

static int
readn (int fd, char *buffer, int len)
{
  int nread, n;

  nread = 0;

  while (len > 0)
    {
      n = read (fd, buffer, len);
      if (n < 0)
	return n;

      if (n == 0)
	return nread;

      buffer += n;
      nread += n;
      len -= n;
    }

  return nread;
}
#endif


/* get_oserror()-- Get the most recent operating system error.  For
 * unix, this is errno. */

const char *
get_oserror (void)
{

  return strerror (errno);
}


/* sys_exit()-- Terminate the program with an exit code */

void
sys_exit (int code)
{

  exit (code);
}



/*********************************************************************
    File descriptor stream functions
*********************************************************************/

/* fd_flush()-- Write bytes that need to be written */

static try
fd_flush (unix_stream * s)
{

  if (s->ndirty == 0)
    return SUCCESS;;

  if (s->physical_offset != s->dirty_offset &&
      lseek (s->fd, s->dirty_offset, SEEK_SET) < 0)
    return FAILURE;

  if (writen (s->fd, s->buffer + (s->dirty_offset - s->buffer_offset),
	      s->ndirty) < 0)
    return FAILURE;

  s->physical_offset = s->dirty_offset + s->ndirty;

  /* don't increment file_length if the file is non-seekable */
  if (s->file_length != -1 && s->physical_offset > s->file_length)
    s->file_length = s->physical_offset;
  s->ndirty = 0;

  return SUCCESS;
}


/* fd_alloc()-- Arrange a buffer such that the salloc() request can be
 * satisfied.  This subroutine gets the buffer ready for whatever is
 * to come next. */

static void
fd_alloc (unix_stream * s, gfc_offset where, int *len)
{
  char *new_buffer;
  int n, read_len;

  if (*len <= BUFFER_SIZE)
    {
      new_buffer = s->small_buffer;
      read_len = BUFFER_SIZE;
    }
  else
    {
      new_buffer = get_mem (*len);
      read_len = *len;
    }

  /* Salvage bytes currently within the buffer.  This is important for
   * devices that cannot seek. */

  if (s->buffer != NULL && s->buffer_offset <= where &&
      where <= s->buffer_offset + s->active)
    {

      n = s->active - (where - s->buffer_offset);
      memmove (new_buffer, s->buffer + (where - s->buffer_offset), n);

      s->active = n;
    }
  else
    {				/* new buffer starts off empty */
      s->active = 0;
    }

  s->buffer_offset = where;

  /* free the old buffer if necessary */

  if (s->buffer != NULL && s->buffer != s->small_buffer)
    free_mem (s->buffer);

  s->buffer = new_buffer;
  s->len = read_len;
  s->mmaped = 0;
}


/* fd_alloc_r_at()-- Allocate a stream buffer for reading.  Either
 * we've already buffered the data or we need to load it.  Returns
 * NULL on I/O error. */

static char *
fd_alloc_r_at (unix_stream * s, int *len, gfc_offset where)
{
  gfc_offset m;
  int n;

  if (where == -1)
    where = s->logical_offset;

  if (s->buffer != NULL && s->buffer_offset <= where &&
      where + *len <= s->buffer_offset + s->active)
    {

      /* Return a position within the current buffer */

      s->logical_offset = where + *len;
      return s->buffer + where - s->buffer_offset;
    }

  fd_alloc (s, where, len);

  m = where + s->active;

  if (s->physical_offset != m && lseek (s->fd, m, SEEK_SET) < 0)
    return NULL;

  n = read (s->fd, s->buffer + s->active, s->len - s->active);
  if (n < 0)
    return NULL;

  s->physical_offset = where + n;

  s->active += n;
  if (s->active < *len)
    *len = s->active;		/* Bytes actually available */

  s->logical_offset = where + *len;

  return s->buffer;
}


/* fd_alloc_w_at()-- Allocate a stream buffer for writing.  Either
 * we've already buffered the data or we need to load it. */

static char *
fd_alloc_w_at (unix_stream * s, int *len, gfc_offset where)
{
  gfc_offset n;

  if (where == -1)
    where = s->logical_offset;

  if (s->buffer == NULL || s->buffer_offset > where ||
      where + *len > s->buffer_offset + s->len)
    {

      if (fd_flush (s) == FAILURE)
	return NULL;
      fd_alloc (s, where, len);
    }

  /* Return a position within the current buffer */
  if (s->ndirty == 0 
      || where > s->dirty_offset + s->ndirty    
      || s->dirty_offset > where + *len)
    {  /* Discontiguous blocks, start with a clean buffer.  */  
        /* Flush the buffer.  */  
       if (s->ndirty != 0)    
         fd_flush (s);  
       s->dirty_offset = where;  
       s->ndirty = *len;
    }
  else
    {  
      gfc_offset start;  /* Merge with the existing data.  */  
      if (where < s->dirty_offset)    
        start = where;  
      else    
        start = s->dirty_offset;  
      if (where + *len > s->dirty_offset + s->ndirty)    
        s->ndirty = where + *len - start;  
      else    
        s->ndirty = s->dirty_offset + s->ndirty - start;  
        s->dirty_offset = start;
    }

  s->logical_offset = where + *len;

  n = s->logical_offset - s->buffer_offset;
  if (n > s->active)
    s->active = n;

  return s->buffer + where - s->buffer_offset;
}


static try
fd_sfree (unix_stream * s)
{

  if (s->ndirty != 0 &&
      (s->buffer != s->small_buffer || options.all_unbuffered ||
       s->unbuffered))
    return fd_flush (s);

  return SUCCESS;
}


static int
fd_seek (unix_stream * s, gfc_offset offset)
{

  s->physical_offset = s->logical_offset = offset;

  return (lseek (s->fd, offset, SEEK_SET) < 0) ? FAILURE : SUCCESS;
}


/* truncate_file()-- Given a unit, truncate the file at the current
 * position.  Sets the physical location to the new end of the file.
 * Returns nonzero on error. */

static try
fd_truncate (unix_stream * s)
{

  if (lseek (s->fd, s->logical_offset, SEEK_SET) == -1)
    return FAILURE;

  /* non-seekable files, like terminals and fifo's fail the lseek.
     the fd is a regular file at this point */

  if (ftruncate (s->fd, s->logical_offset))
   {
    return FAILURE;
   }

  s->physical_offset = s->file_length = s->logical_offset;

  return SUCCESS;
}


static try
fd_close (unix_stream * s)
{

  if (fd_flush (s) == FAILURE)
    return FAILURE;

  if (s->buffer != NULL && s->buffer != s->small_buffer)
    free_mem (s->buffer);

  if (close (s->fd) < 0)
    return FAILURE;

  free_mem (s);

  return SUCCESS;
}


static void
fd_open (unix_stream * s)
{

  if (isatty (s->fd))
    s->unbuffered = 1;

  s->st.alloc_r_at = (void *) fd_alloc_r_at;
  s->st.alloc_w_at = (void *) fd_alloc_w_at;
  s->st.sfree = (void *) fd_sfree;
  s->st.close = (void *) fd_close;
  s->st.seek = (void *) fd_seek;
  s->st.truncate = (void *) fd_truncate;

  s->buffer = NULL;
}


/*********************************************************************
    mmap stream functions

 Because mmap() is not capable of extending a file, we have to keep
 track of how long the file is.  We also have to be able to detect end
 of file conditions.  If there are multiple writers to the file (which
 can only happen outside the current program), things will get
 confused.  Then again, things will get confused anyway.

*********************************************************************/

#if HAVE_MMAP

static int page_size, page_mask;

/* mmap_flush()-- Deletes a memory mapping if something is mapped. */

static try
mmap_flush (unix_stream * s)
{

  if (!s->mmaped)
    return fd_flush (s);

  if (s->buffer == NULL)
    return SUCCESS;

  if (munmap (s->buffer, s->active))
    return FAILURE;

  s->buffer = NULL;
  s->active = 0;

  return SUCCESS;
}


/* mmap_alloc()-- mmap() a section of the file.  The whole section is
 * guaranteed to be mappable. */

static try
mmap_alloc (unix_stream * s, gfc_offset where, int *len)
{
  gfc_offset offset;
  int length;
  char *p;

  if (mmap_flush (s) == FAILURE)
    return FAILURE;

  offset = where & page_mask;	/* Round down to the next page */

  length = ((where - offset) & page_mask) + 2 * page_size;

  p = mmap (NULL, length, s->prot, MAP_SHARED, s->fd, offset);
  if (p == (char *) MAP_FAILED)
    return FAILURE;

  s->mmaped = 1;
  s->buffer = p;
  s->buffer_offset = offset;
  s->active = length;

  return SUCCESS;
}


static char *
mmap_alloc_r_at (unix_stream * s, int *len, gfc_offset where)
{
  gfc_offset m;

  if (where == -1)
    where = s->logical_offset;

  m = where + *len;

  if ((s->buffer == NULL || s->buffer_offset > where ||
       m > s->buffer_offset + s->active) &&
      mmap_alloc (s, where, len) == FAILURE)
    return NULL;

  if (m > s->file_length)
    {
      *len = s->file_length - s->logical_offset;
      s->logical_offset = s->file_length;
    }
  else
    s->logical_offset = m;

  return s->buffer + (where - s->buffer_offset);
}


static char *
mmap_alloc_w_at (unix_stream * s, int *len, gfc_offset where)
{
  if (where == -1)
    where = s->logical_offset;

  /* If we're extending the file, we have to use file descriptor
   * methods. */

  if (where + *len > s->file_length)
    {
      if (s->mmaped)
	mmap_flush (s);
      return fd_alloc_w_at (s, len, where);
    }

  if ((s->buffer == NULL || s->buffer_offset > where ||
       where + *len > s->buffer_offset + s->active) &&
      mmap_alloc (s, where, len) == FAILURE)
    return NULL;

  s->logical_offset = where + *len;

  return s->buffer + where - s->buffer_offset;
}


static int
mmap_seek (unix_stream * s, gfc_offset offset)
{

  s->logical_offset = offset;
  return SUCCESS;
}


static try
mmap_close (unix_stream * s)
{
  try t;

  t = mmap_flush (s);

  if (close (s->fd) < 0)
    t = FAILURE;
  free_mem (s);

  return t;
}


static try
mmap_sfree (unix_stream * s)
{

  return SUCCESS;
}


/* mmap_open()-- mmap_specific open.  If the particular file cannot be
 * mmap()-ed, we fall back to the file descriptor functions. */

static try
mmap_open (unix_stream * s)
{
  char *p;
  int i;

  page_size = getpagesize ();
  page_mask = ~0;

  p = mmap (0, page_size, s->prot, MAP_SHARED, s->fd, 0);
  if (p == (char *) MAP_FAILED)
    {
      fd_open (s);
      return SUCCESS;
    }

  munmap (p, page_size);

  i = page_size >> 1;
  while (i != 0)
    {
      page_mask <<= 1;
      i >>= 1;
    }

  s->st.alloc_r_at = (void *) mmap_alloc_r_at;
  s->st.alloc_w_at = (void *) mmap_alloc_w_at;
  s->st.sfree = (void *) mmap_sfree;
  s->st.close = (void *) mmap_close;
  s->st.seek = (void *) mmap_seek;
  s->st.truncate = (void *) fd_truncate;

  if (lseek (s->fd, s->file_length, SEEK_SET) < 0)
    return FAILURE;

  return SUCCESS;
}

#endif


/*********************************************************************
  memory stream functions - These are used for internal files

  The idea here is that a single stream structure is created and all
  requests must be satisfied from it.  The location and size of the
  buffer is the character variable supplied to the READ or WRITE
  statement.

*********************************************************************/


static char *
mem_alloc_r_at (unix_stream * s, int *len, gfc_offset where)
{
  gfc_offset n;

  if (where == -1)
    where = s->logical_offset;

  if (where < s->buffer_offset || where > s->buffer_offset + s->active)
    return NULL;

  s->logical_offset = where + *len;

  n = s->buffer_offset + s->active - where;
  if (*len > n)
    *len = n;

  return s->buffer + (where - s->buffer_offset);
}


static char *
mem_alloc_w_at (unix_stream * s, int *len, gfc_offset where)
{
  gfc_offset m;

  if (where == -1)
    where = s->logical_offset;

  m = where + *len;

  if (where < s->buffer_offset || m > s->buffer_offset + s->active)
    return NULL;

  s->logical_offset = m;

  return s->buffer + (where - s->buffer_offset);
}


static int
mem_seek (unix_stream * s, gfc_offset offset)
{

  if (offset > s->file_length)
    {
      errno = ESPIPE;
      return FAILURE;
    }

  s->logical_offset = offset;
  return SUCCESS;
}


static int
mem_truncate (unix_stream * s)
{

  return SUCCESS;
}


static try
mem_close (unix_stream * s)
{

  return SUCCESS;
}


static try
mem_sfree (unix_stream * s)
{

  return SUCCESS;
}



/*********************************************************************
  Public functions -- A reimplementation of this module needs to
  define functional equivalents of the following.
*********************************************************************/

/* empty_internal_buffer()-- Zero the buffer of Internal file */

void
empty_internal_buffer(stream *strm)
{
   unix_stream * s = (unix_stream *) strm;
   memset(s->buffer, ' ', s->file_length);
}

/* open_internal()-- Returns a stream structure from an internal file */

stream *
open_internal (char *base, int length)
{
  unix_stream *s;

  s = get_mem (sizeof (unix_stream));

  s->buffer = base;
  s->buffer_offset = 0;

  s->logical_offset = 0;
  s->active = s->file_length = length;

  s->st.alloc_r_at = (void *) mem_alloc_r_at;
  s->st.alloc_w_at = (void *) mem_alloc_w_at;
  s->st.sfree = (void *) mem_sfree;
  s->st.close = (void *) mem_close;
  s->st.seek = (void *) mem_seek;
  s->st.truncate = (void *) mem_truncate;

  return (stream *) s;
}


/* fd_to_stream()-- Given an open file descriptor, build a stream
 * around it. */

static stream *
fd_to_stream (int fd, int prot)
{
  struct stat statbuf;
  unix_stream *s;

  s = get_mem (sizeof (unix_stream));

  s->fd = fd;
  s->buffer_offset = 0;
  s->physical_offset = 0;
  s->logical_offset = 0;
  s->prot = prot;

  /* Get the current length of the file. */

  fstat (fd, &statbuf);
  s->file_length = S_ISREG (statbuf.st_mode) ? statbuf.st_size : -1;

#if HAVE_MMAP
  mmap_open (s);
#else
  fd_open (s);
#endif

  return (stream *) s;
}


/* unpack_filename()-- Given a fortran string and a pointer to a
 * buffer that is PATH_MAX characters, convert the fortran string to a
 * C string in the buffer.  Returns nonzero if this is not possible.  */

static int
unpack_filename (char *cstring, const char *fstring, int len)
{

  len = fstrlen (fstring, len);
  if (len >= PATH_MAX)
    return 1;

  memmove (cstring, fstring, len);
  cstring[len] = '\0';

  return 0;
}


/* tempfile()-- Generate a temporary filename for a scratch file and
 * open it.  mkstemp() opens the file for reading and writing, but the
 * library mode prevents anything that is not allowed.  The descriptor
 * is returns, which is less than zero on error.  The template is
 * pointed to by ioparm.file, which is copied into the unit structure
 * and freed later. */

static int
tempfile (void)
{
  const char *tempdir;
  char *template;
  int fd;

  tempdir = getenv ("GFORTRAN_TMPDIR");
  if (tempdir == NULL)
    tempdir = getenv ("TMP");
  if (tempdir == NULL)
    tempdir = DEFAULT_TEMPDIR;

  template = get_mem (strlen (tempdir) + 20);

  st_sprintf (template, "%s/gfortantmpXXXXXX", tempdir);

  fd = mkstemp (template);

  if (fd < 0)
    free_mem (template);
  else
    {
      ioparm.file = template;
      ioparm.file_len = strlen (template);	/* Don't include trailing nul */
    }

  return fd;
}


/* regular_file()-- Open a regular file.  Returns the descriptor, which is less than zero on error. */

static int
regular_file (unit_action action, unit_status status)
{
  char path[PATH_MAX + 1];
  struct stat statbuf;
  int mode;

  if (unpack_filename (path, ioparm.file, ioparm.file_len))
    {
      errno = ENOENT;		/* Fake an OS error */
      return -1;
    }

  mode = 0;

  switch (action)
    {
    case ACTION_READ:
      mode = O_RDONLY;
      break;

    case ACTION_WRITE:
      mode = O_WRONLY;
      break;

    case ACTION_READWRITE:
      mode = O_RDWR;
      break;

    default:
      internal_error ("regular_file(): Bad action");
    }

  switch (status)
    {
    case STATUS_NEW:
      mode |= O_CREAT | O_EXCL;
      break;

    case STATUS_OLD:		/* file must exist, so check for its existence */
      if (stat (path, &statbuf) < 0)
	return -1;
      break;

    case STATUS_UNKNOWN:
    case STATUS_SCRATCH:
      mode |= O_CREAT;
      break;

    case STATUS_REPLACE:
        mode |= O_CREAT | O_TRUNC;
      break;

    default:
      internal_error ("regular_file(): Bad status");
    }

  // mode |= O_LARGEFILE;

  return open (path, mode,
	       S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
}


/* open_external()-- Open an external file, unix specific version.
 * Returns NULL on operating system error. */

stream *
open_external (unit_action action, unit_status status)
{
  int fd, prot;

  fd =
    (status == STATUS_SCRATCH) ? tempfile () : regular_file (action, status);

  if (fd < 0)
    return NULL;
  fd = fix_fd (fd);

  switch (action)
    {
    case ACTION_READ:
      prot = PROT_READ;
      break;

    case ACTION_WRITE:
      prot = PROT_WRITE;
      break;

    case ACTION_READWRITE:
      prot = PROT_READ | PROT_WRITE;
      break;

    default:
      internal_error ("open_external(): Bad action");
    }

  /* If this is a scratch file, we can unlink it now and the file will
   * go away when it is closed. */

  if (status == STATUS_SCRATCH)
    unlink (ioparm.file);

  return fd_to_stream (fd, prot);
}


/* input_stream()-- Return a stream pointer to the default input stream.
 * Called on initialization. */

stream *
input_stream (void)
{

  return fd_to_stream (STDIN_FILENO, PROT_READ);
}


/* output_stream()-- Return a stream pointer to the default input stream.
 * Called on initialization. */

stream *
output_stream (void)
{

  return fd_to_stream (STDOUT_FILENO, PROT_WRITE);
}


/* init_error_stream()-- Return a pointer to the error stream.  This
 * subroutine is called when the stream is needed, rather than at
 * initialization.  We want to work even if memory has been seriously
 * corrupted. */

stream *
init_error_stream (void)
{
  static unix_stream error;

  memset (&error, '\0', sizeof (error));

  error.fd = options.use_stderr ? STDERR_FILENO : STDOUT_FILENO;

  error.st.alloc_w_at = (void *) fd_alloc_w_at;
  error.st.sfree = (void *) fd_sfree;

  error.unbuffered = 1;
  error.buffer = error.small_buffer;

  return (stream *) & error;
}


/* compare_file_filename()-- Given an open stream and a fortran string
 * that is a filename, figure out if the file is the same as the
 * filename. */

int
compare_file_filename (stream * s, const char *name, int len)
{
  char path[PATH_MAX + 1];
  struct stat st1, st2;

  if (unpack_filename (path, name, len))
    return 0;			/* Can't be the same */

  /* If the filename doesn't exist, then there is no match with the
   * existing file. */

  if (stat (path, &st1) < 0)
    return 0;

  fstat (((unix_stream *) s)->fd, &st2);

  return (st1.st_dev == st2.st_dev) && (st1.st_ino == st2.st_ino);
}


/* find_file0()-- Recursive work function for find_file() */

static gfc_unit *
find_file0 (gfc_unit * u, struct stat *st1)
{
  struct stat st2;
  gfc_unit *v;

  if (u == NULL)
    return NULL;

  if (fstat (((unix_stream *) u->s)->fd, &st2) >= 0 &&
      st1->st_dev == st2.st_dev && st1->st_ino == st2.st_ino)
    return u;

  v = find_file0 (u->left, st1);
  if (v != NULL)
    return v;

  v = find_file0 (u->right, st1);
  if (v != NULL)
    return v;

  return NULL;
}


/* find_file()-- Take the current filename and see if there is a unit
 * that has the file already open.  Returns a pointer to the unit if so. */

gfc_unit *
find_file (void)
{
  char path[PATH_MAX + 1];
  struct stat statbuf;

  if (unpack_filename (path, ioparm.file, ioparm.file_len))
    return NULL;

  if (stat (path, &statbuf) < 0)
    return NULL;

  return find_file0 (g.unit_root, &statbuf);
}


/* stream_at_bof()-- Returns nonzero if the stream is at the beginning
 * of the file. */

int
stream_at_bof (stream * s)
{
  unix_stream *us;

  us = (unix_stream *) s;

  if (!us->mmaped)
    return 0;			/* File is not seekable */

  return us->logical_offset == 0;
}


/* stream_at_eof()-- Returns nonzero if the stream is at the beginning
 * of the file. */

int
stream_at_eof (stream * s)
{
  unix_stream *us;

  us = (unix_stream *) s;

  if (!us->mmaped)
    return 0;			/* File is not seekable */

  return us->logical_offset == us->dirty_offset;
}


/* delete_file()-- Given a unit structure, delete the file associated
 * with the unit.  Returns nonzero if something went wrong. */

int
delete_file (gfc_unit * u)
{
  char path[PATH_MAX + 1];

  if (unpack_filename (path, u->file, u->file_len))
    {				/* Shouldn't be possible */
      errno = ENOENT;
      return 1;
    }

  return unlink (path);
}


/* file_exists()-- Returns nonzero if the current filename exists on
 * the system */

int
file_exists (void)
{
  char path[PATH_MAX + 1];
  struct stat statbuf;

  if (unpack_filename (path, ioparm.file, ioparm.file_len))
    return 0;

  if (stat (path, &statbuf) < 0)
    return 0;

  return 1;
}



static const char *yes = "YES", *no = "NO", *unknown = "UNKNOWN";

/* inquire_sequential()-- Given a fortran string, determine if the
 * file is suitable for sequential access.  Returns a C-style
 * string. */

const char *
inquire_sequential (const char *string, int len)
{
  char path[PATH_MAX + 1];
  struct stat statbuf;

  if (string == NULL ||
      unpack_filename (path, string, len) || stat (path, &statbuf) < 0)
    return unknown;

  if (S_ISREG (statbuf.st_mode) ||
      S_ISCHR (statbuf.st_mode) || S_ISFIFO (statbuf.st_mode))
    return yes;

  if (S_ISDIR (statbuf.st_mode) || S_ISBLK (statbuf.st_mode))
    return no;

  return unknown;
}


/* inquire_direct()-- Given a fortran string, determine if the file is
 * suitable for direct access.  Returns a C-style string. */

const char *
inquire_direct (const char *string, int len)
{
  char path[PATH_MAX + 1];
  struct stat statbuf;

  if (string == NULL ||
      unpack_filename (path, string, len) || stat (path, &statbuf) < 0)
    return unknown;

  if (S_ISREG (statbuf.st_mode) || S_ISBLK (statbuf.st_mode))
    return yes;

  if (S_ISDIR (statbuf.st_mode) ||
      S_ISCHR (statbuf.st_mode) || S_ISFIFO (statbuf.st_mode))
    return no;

  return unknown;
}


/* inquire_formatted()-- Given a fortran string, determine if the file
 * is suitable for formatted form.  Returns a C-style string. */

const char *
inquire_formatted (const char *string, int len)
{
  char path[PATH_MAX + 1];
  struct stat statbuf;

  if (string == NULL ||
      unpack_filename (path, string, len) || stat (path, &statbuf) < 0)
    return unknown;

  if (S_ISREG (statbuf.st_mode) ||
      S_ISBLK (statbuf.st_mode) ||
      S_ISCHR (statbuf.st_mode) || S_ISFIFO (statbuf.st_mode))
    return yes;

  if (S_ISDIR (statbuf.st_mode))
    return no;

  return unknown;
}


/* inquire_unformatted()-- Given a fortran string, determine if the file
 * is suitable for unformatted form.  Returns a C-style string. */

const char *
inquire_unformatted (const char *string, int len)
{

  return inquire_formatted (string, len);
}


/* inquire_access()-- Given a fortran string, determine if the file is
 * suitable for access. */

static const char *
inquire_access (const char *string, int len, int mode)
{
  char path[PATH_MAX + 1];

  if (string == NULL || unpack_filename (path, string, len) ||
      access (path, mode) < 0)
    return no;

  return yes;
}


/* inquire_read()-- Given a fortran string, determine if the file is
 * suitable for READ access. */

const char *
inquire_read (const char *string, int len)
{

  return inquire_access (string, len, R_OK);
}


/* inquire_write()-- Given a fortran string, determine if the file is
 * suitable for READ access. */

const char *
inquire_write (const char *string, int len)
{

  return inquire_access (string, len, W_OK);
}


/* inquire_readwrite()-- Given a fortran string, determine if the file is
 * suitable for read and write access. */

const char *
inquire_readwrite (const char *string, int len)
{

  return inquire_access (string, len, R_OK | W_OK);
}


/* file_length()-- Return the file length in bytes, -1 if unknown */

gfc_offset
file_length (stream * s)
{

  return ((unix_stream *) s)->file_length;
}


/* file_position()-- Return the current position of the file */

gfc_offset
file_position (stream * s)
{

  return ((unix_stream *) s)->logical_offset;
}


/* is_seekable()-- Return nonzero if the stream is seekable, zero if
 * it is not */

int
is_seekable (stream * s)
{
  /* by convention, if file_length == -1, the file is not seekable
     note that a mmapped file is always seekable, an fd_ file may
     or may not be. */
  return ((unix_stream *) s)->file_length!=-1;
}

try
flush (stream *s)
{
  return fd_flush( (unix_stream *) s);
}


/* How files are stored:  This is an operating-system specific issue,
   and therefore belongs here.  There are three cases to consider.

   Direct Access:
      Records are written as block of bytes corresponding to the record
      length of the file.  This goes for both formatted and unformatted
      records.  Positioning is done explicitly for each data transfer,
      so positioning is not much of an issue.

   Sequential Formatted:
      Records are separated by newline characters.  The newline character
      is prohibited from appearing in a string.  If it does, this will be
      messed up on the next read.  End of file is also the end of a record.

   Sequential Unformatted:
      In this case, we are merely copying bytes to and from main storage,
      yet we need to keep track of varying record lengths.  We adopt
      the solution used by f2c.  Each record contains a pair of length
      markers:

        Length of record n in bytes
        Data of record n
        Length of record n in bytes

        Length of record n+1 in bytes
        Data of record n+1
        Length of record n+1 in bytes

     The length is stored at the end of a record to allow backspacing to the
     previous record.  Between data transfer statements, the file pointer
     is left pointing to the first length of the current record.

     ENDFILE records are never explicitly stored.

*/
