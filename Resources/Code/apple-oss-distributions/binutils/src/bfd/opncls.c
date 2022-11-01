/* opncls.c -- open and close a BFD.
   Copyright 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997, 1998, 2000,
   2001, 2002
   Free Software Foundation, Inc.

   Written by Cygnus Support.

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "bfd.h"
#include "sysdep.h"
#include "objalloc.h"
#include "libbfd.h"

#if USE_MMAP 

#if HAVE_MMAP || HAVE_MPROTECT || HAVE_MADVISE
#include <sys/types.h>
#include <sys/mman.h>
#endif

#if HAVE_MMAP || HAVE_MPROTECT || HAVE_MADVISE
#include <sys/types.h>
#include <sys/mman.h>
#endif

#undef MAP_SHARED
#define MAP_SHARED MAP_PRIVATE

#ifndef MAP_FILE
#define MAP_FILE 0
#endif

#endif /* USE_MMAP */
 
#ifndef S_IXUSR
#define S_IXUSR 0100	/* Execute by owner.  */
#endif
#ifndef S_IXGRP
#define S_IXGRP 0010	/* Execute by group.  */
#endif
#ifndef S_IXOTH
#define S_IXOTH 0001	/* Execute by others.  */
#endif

/* Counter used to initialize the bfd identifier.  */

static unsigned int _bfd_id_counter = 0;

/* fdopen is a loser -- we should use stdio exclusively.  Unfortunately
   if we do that we can't use fcntl.  */

/* Return a new BFD.  All BFD's are allocated through this routine.  */

bfd *
_bfd_new_bfd ()
{
  bfd *nbfd;

  nbfd = (bfd *) bfd_zmalloc ((bfd_size_type) sizeof (bfd));
  if (nbfd == NULL)
    return NULL;

  nbfd->id = _bfd_id_counter++;

  nbfd->memory = (PTR) objalloc_create ();
  if (nbfd->memory == NULL)
    {
      bfd_set_error (bfd_error_no_memory);
      free (nbfd);
      return NULL;
    }

  nbfd->arch_info = &bfd_default_arch_struct;

  nbfd->direction = no_direction;
  nbfd->iostream = NULL;
  nbfd->where = 0;
  if (!bfd_hash_table_init_n (&nbfd->section_htab,
			      bfd_section_hash_newfunc,
			      251))
    {
      free (nbfd);
      return NULL;
    }
  nbfd->sections = (asection *) NULL;
  nbfd->section_tail = &nbfd->sections;
  nbfd->format = bfd_unknown;
  nbfd->my_archive = (bfd *) NULL;
  nbfd->origin = 0;
  nbfd->opened_once = FALSE;
  nbfd->output_has_begun = FALSE;
  nbfd->section_count = 0;
  nbfd->usrdata = (PTR) NULL;
  nbfd->cacheable = FALSE;
  nbfd->flags = BFD_NO_FLAGS;
  nbfd->mtime_set = FALSE;

  return nbfd;
}

/* Allocate a new BFD as a member of archive OBFD.  */

bfd *
_bfd_new_bfd_contained_in (obfd)
     bfd *obfd;
{
  bfd *nbfd;

  nbfd = _bfd_new_bfd ();
  if (nbfd == NULL)
    return NULL;
  nbfd->xvec = obfd->xvec;
  nbfd->my_archive = obfd;
  nbfd->direction = read_direction;
  nbfd->target_defaulted = obfd->target_defaulted;
  return nbfd;
}

/* Delete a BFD.  */

void
_bfd_delete_bfd (abfd)
     bfd *abfd;
{
  bfd_hash_table_free (&abfd->section_htab);
  objalloc_free ((struct objalloc *) abfd->memory);
  memset (abfd, '\0', sizeof (bfd));
  free (abfd);
}

/*
SECTION
	Opening and closing BFDs

*/

/*
FUNCTION
	bfd_openr

SYNOPSIS
	bfd *bfd_openr(const char *filename, const char *target);

DESCRIPTION
	Open the file @var{filename} (using <<fopen>>) with the target
	@var{target}.  Return a pointer to the created BFD.

	Calls <<bfd_find_target>>, so @var{target} is interpreted as by
	that function.

	If <<NULL>> is returned then an error has occured.   Possible errors
	are <<bfd_error_no_memory>>, <<bfd_error_invalid_target>> or
	<<system_call>> error.
*/

bfd *
bfd_openr (filename, target)
     const char *filename;
     const char *target;
{
  bfd *nbfd;
  const bfd_target *target_vec;

  nbfd = _bfd_new_bfd ();
  if (nbfd == NULL)
    return NULL;

  target_vec = bfd_find_target (target, nbfd);
  if (target_vec == NULL)
    {
      _bfd_delete_bfd (nbfd);
      return NULL;
    }

  nbfd->filename = filename;
  nbfd->direction = read_direction;

  if (bfd_open_file (nbfd) == NULL)
    {
      /* File didn't exist, or some such.  */
      bfd_set_error (bfd_error_system_call);
      _bfd_delete_bfd (nbfd);
      return NULL;
    }

  return nbfd;
}

/*
FUNCTION
	bfd_mmap_file

SYNOPSIS
        bfd_boolean bfd_mmap_file(bfd *abfd, void *addr);

DESCRIPTION
	Cause future file accesses to be done via mmap rather than via read/write.
*/

bfd_boolean bfd_mmap_file (abfd, addr)
     bfd *abfd ATTRIBUTE_UNUSED;
     void *addr ATTRIBUTE_UNUSED;
{
#if USE_MMAP 
  struct bfd_in_memory *mem;
  struct stat statbuf;
  FILE *fp;
  int fd;
  unsigned int prot;
  unsigned int flags;

  BFD_ASSERT ((abfd->flags & BFD_IN_MEMORY) == 0);

  abfd->mtime = bfd_get_mtime (abfd);
  abfd->mtime_set = 1;

  mem = bfd_alloc (abfd, sizeof (struct bfd_in_memory));
  if (mem == NULL) {
    return FALSE; 
  }

  fp = bfd_cache_lookup (abfd);
  fd = fileno (fp);

  if (fstat (fd, &statbuf) != 0) {
    bfd_set_error (bfd_error_system_call);
    return FALSE;
  }
  mem->size = statbuf.st_size;

  switch (abfd->direction) {
  case no_direction:
    prot = 0;
    flags = MAP_FILE | MAP_SHARED;
    break;
  case read_direction:
    prot = PROT_READ;
    flags = MAP_FILE | MAP_SHARED;
    break;
  case write_direction:
  case both_direction:
    prot = PROT_READ | PROT_WRITE;
    flags = MAP_FILE | MAP_SHARED;
    break;
  default:
    abort ();
  }

  if (addr != ((void *) -1)) {
    flags |= MAP_FIXED;
  } else {
    addr = NULL;
  }
  
  mem->buffer = mmap (addr, mem->size, prot, flags, fd, 0);
  if ((caddr_t) mem->buffer == (caddr_t) -1) {
    bfd_set_error (bfd_error_system_call);
    return FALSE;
  }
  
  BFD_ASSERT ((abfd->flags & BFD_IN_MEMORY) == 0);
  bfd_cache_close (abfd);
  abfd->iostream = mem;
  abfd->flags |= BFD_IN_MEMORY;

  return TRUE;

#else /* ! USE_MMAP */
  bfd_set_error (bfd_error_system_call);
  return FALSE;
#endif /* USE_MMAP */
}

/*
FUNCTION
	bfd_funopenr

SYNOPSIS
        bfd *bfd_funopenr(const char *filename, const char *target, struct bfd_io_functions *fdata);

DESCRIPTION
	No description available.
*/

bfd *
bfd_funopenr (filename, target, fdata)
     const char *filename;
     const char *target;
     struct bfd_io_functions *fdata;
{
  bfd *nbfd;
  const bfd_target *target_vec;
  struct bfd_io_functions *fun;

  nbfd = _bfd_new_bfd ();
  if (nbfd == NULL)
    return NULL;

  fun = bfd_alloc (nbfd, sizeof (struct bfd_io_functions));
  if (fun == NULL)
    return NULL; 

  target_vec = bfd_find_target (target, nbfd);
  if (target_vec == NULL) 
    {
      objalloc_free ((struct objalloc *) nbfd->memory);
      free (nbfd);
      bfd_set_error (bfd_error_invalid_target);
      return NULL;
    }

  *fun = *fdata;

  nbfd->filename = filename;
  nbfd->direction = read_direction;
  nbfd->iostream = fun;
  nbfd->flags |= BFD_IO_FUNCS;
  
  return nbfd;
}


/*
FUNCTION
	bfd_memopenr

SYNOPSIS
        bfd *bfd_memopenr(const char *filename, const char *target, unsigned char *addr, bfd_size_type len);

DESCRIPTION
	Treat the block of memory at address @var{addr} as if it were a file of
	length @var{len}, using the target @var{taarget}.  Return a pointer to
	the created BFD.

	Calls <<bfd_find_target>>, so @var{target} is interpreted as by
	that function.

	If <<NULL>> is returned then an error has occured.   Possible errors
	are <<bfd_error_no_memory>>, <<bfd_error_invalid_target>> or <<system_call>> error.
*/

bfd *
bfd_memopenr (filename, target, addr, len)
     const char *filename;
     const char *target;
     unsigned char *addr;
     bfd_size_type len;
{
  bfd *nbfd;
  const bfd_target *target_vec;
  struct bfd_in_memory *mem;

  nbfd = _bfd_new_bfd ();
  if (nbfd == NULL)
    return NULL;

  mem = bfd_alloc (nbfd, sizeof (struct bfd_in_memory));
  if (mem == NULL)
    return NULL; 

  target_vec = bfd_find_target (target, nbfd);
  if (target_vec == NULL) 
    {
      objalloc_free ((struct objalloc *) nbfd->memory);
      free (nbfd);
      bfd_set_error (bfd_error_invalid_target);
      return NULL;
    }

  mem->buffer = addr;
  mem->size = len;

  nbfd->filename = filename;
  nbfd->direction = read_direction;
  nbfd->iostream = mem;
  nbfd->flags |= BFD_IN_MEMORY;

  return nbfd;
}

/* Don't try to `optimize' this function:

   o - We lock using stack space so that interrupting the locking
       won't cause a storage leak.
   o - We open the file stream last, since we don't want to have to
       close it if anything goes wrong.  Closing the stream means closing
       the file descriptor too, even though we didn't open it.  */
/*
FUNCTION
	bfd_fdopenr

SYNOPSIS
	bfd *bfd_fdopenr(const char *filename, const char *target, int fd);

DESCRIPTION
	<<bfd_fdopenr>> is to <<bfd_fopenr>> much like <<fdopen>> is to
	<<fopen>>.  It opens a BFD on a file already described by the
	@var{fd} supplied.

	When the file is later <<bfd_close>>d, the file descriptor will
	be closed.  If the caller desires that this file descriptor be
	cached by BFD (opened as needed, closed as needed to free
	descriptors for other opens), with the supplied @var{fd} used as
	an initial file descriptor (but subject to closure at any time),
	call bfd_set_cacheable(bfd, 1) on the returned BFD.  The default
	is to assume no cacheing; the file descriptor will remain open
	until <<bfd_close>>, and will not be affected by BFD operations
	on other files.

	Possible errors are <<bfd_error_no_memory>>,
	<<bfd_error_invalid_target>> and <<bfd_error_system_call>>.
*/

bfd *
bfd_fdopenr (filename, target, fd)
     const char *filename;
     const char *target;
     int fd;
{
  bfd *nbfd;
  const bfd_target *target_vec;
  int fdflags;

  bfd_set_error (bfd_error_system_call);
#if ! defined(HAVE_FCNTL) || ! defined(F_GETFL)
  fdflags = O_RDWR;			/* Assume full access.  */
#else
  fdflags = fcntl (fd, F_GETFL, NULL);
#endif
  if (fdflags == -1)
    return NULL;

  nbfd = _bfd_new_bfd ();
  if (nbfd == NULL)
    return NULL;

  target_vec = bfd_find_target (target, nbfd);
  if (target_vec == NULL)
    {
      _bfd_delete_bfd (nbfd);
      return NULL;
    }

#ifndef HAVE_FDOPEN
  nbfd->iostream = (PTR) fopen (filename, FOPEN_RB);
#else
  /* (O_ACCMODE) parens are to avoid Ultrix header file bug.  */
  switch (fdflags & (O_ACCMODE))
    {
    case O_RDONLY: nbfd->iostream = (PTR) fdopen (fd, FOPEN_RB);   break;
    case O_WRONLY: nbfd->iostream = (PTR) fdopen (fd, FOPEN_RUB);  break;
    case O_RDWR:   nbfd->iostream = (PTR) fdopen (fd, FOPEN_RUB);  break;
    default: abort ();
    }
#endif

  if (nbfd->iostream == NULL)
    {
      _bfd_delete_bfd (nbfd);
      return NULL;
    }

  /* OK, put everything where it belongs.  */

  nbfd->filename = filename;

  /* As a special case we allow a FD open for read/write to
     be written through, although doing so requires that we end
     the previous clause with a preposition.  */
  /* (O_ACCMODE) parens are to avoid Ultrix header file bug.  */
  switch (fdflags & (O_ACCMODE))
    {
    case O_RDONLY: nbfd->direction = read_direction; break;
    case O_WRONLY: nbfd->direction = write_direction; break;
    case O_RDWR: nbfd->direction = both_direction; break;
    default: abort ();
    }

  if (! bfd_cache_init (nbfd))
    {
      _bfd_delete_bfd (nbfd);
      return NULL;
    }
  nbfd->opened_once = TRUE;

  return nbfd;
}

/*
FUNCTION
	bfd_openstreamr

SYNOPSIS
	bfd *bfd_openstreamr(const char *, const char *, PTR);

DESCRIPTION

	Open a BFD for read access on an existing stdio stream.  When
	the BFD is passed to <<bfd_close>>, the stream will be closed.
*/

bfd *
bfd_openstreamr (filename, target, streamarg)
     const char *filename;
     const char *target;
     PTR streamarg;
{
  FILE *stream = (FILE *) streamarg;
  bfd *nbfd;
  const bfd_target *target_vec;

  nbfd = _bfd_new_bfd ();
  if (nbfd == NULL)
    return NULL;

  target_vec = bfd_find_target (target, nbfd);
  if (target_vec == NULL)
    {
      _bfd_delete_bfd (nbfd);
      return NULL;
    }

  nbfd->iostream = (PTR) stream;
  nbfd->filename = filename;
  nbfd->direction = read_direction;

  if (! bfd_cache_init (nbfd))
    {
      _bfd_delete_bfd (nbfd);
      return NULL;
    }

  return nbfd;
}

/* bfd_openw -- open for writing.
   Returns a pointer to a freshly-allocated BFD on success, or NULL.

   See comment by bfd_fdopenr before you try to modify this function.  */

/*
FUNCTION
	bfd_openw

SYNOPSIS
	bfd *bfd_openw(const char *filename, const char *target);

DESCRIPTION
	Create a BFD, associated with file @var{filename}, using the
	file format @var{target}, and return a pointer to it.

	Possible errors are <<bfd_error_system_call>>, <<bfd_error_no_memory>>,
	<<bfd_error_invalid_target>>.
*/

bfd *
bfd_openw (filename, target)
     const char *filename;
     const char *target;
{
  bfd *nbfd;
  const bfd_target *target_vec;

  /* nbfd has to point to head of malloc'ed block so that bfd_close may
     reclaim it correctly.  */
  nbfd = _bfd_new_bfd ();
  if (nbfd == NULL)
    return NULL;

  target_vec = bfd_find_target (target, nbfd);
  if (target_vec == NULL)
    {
      _bfd_delete_bfd (nbfd);
      return NULL;
    }

  nbfd->filename = filename;
  nbfd->direction = write_direction;

  if (bfd_open_file (nbfd) == NULL)
    {
      /* File not writeable, etc.  */
      bfd_set_error (bfd_error_system_call);
      _bfd_delete_bfd (nbfd);
      return NULL;
  }

  return nbfd;
}

/*

FUNCTION
	bfd_close

SYNOPSIS
	bfd_boolean bfd_close (bfd *abfd);

DESCRIPTION

	Close a BFD. If the BFD was open for writing, then pending
	operations are completed and the file written out and closed.
	If the created file is executable, then <<chmod>> is called
	to mark it as such.

	All memory attached to the BFD is released.

	The file descriptor associated with the BFD is closed (even
	if it was passed in to BFD by <<bfd_fdopenr>>).

RETURNS
	<<TRUE>> is returned if all is ok, otherwise <<FALSE>>.
*/


bfd_boolean
bfd_close (abfd)
     bfd *abfd;
{
  bfd_boolean ret = TRUE;

  if (bfd_write_p (abfd))
    {
      if (! BFD_SEND_FMT (abfd, _bfd_write_contents, (abfd)))
	return FALSE;
    }

  if (! BFD_SEND (abfd, _close_and_cleanup, (abfd)))
    return FALSE;

  ret = _bfd_io_close (abfd);
  _bfd_delete_bfd (abfd);

  return ret;
}

/*
FUNCTION
	bfd_close_all_done

SYNOPSIS
	bfd_boolean bfd_close_all_done (bfd *);

DESCRIPTION
	Close a BFD.  Differs from <<bfd_close>> since it does not
	complete any pending operations.  This routine would be used
	if the application had just used BFD for swapping and didn't
	want to use any of the writing code.

	If the created file is executable, then <<chmod>> is called
	to mark it as such.

	All memory attached to the BFD is released.

RETURNS
	<<TRUE>> is returned if all is ok, otherwise <<FALSE>>.
*/

bfd_boolean
bfd_close_all_done (abfd)
     bfd *abfd;
{
  bfd_boolean ret;

  ret = _bfd_io_close (abfd);

  /* If the file was open for writing and is now executable,
     make it so.  */
  if (ret
      && abfd->direction == write_direction
      && abfd->flags & EXEC_P)
    {
      struct stat buf;

      if (stat (abfd->filename, &buf) == 0)
	{
	  unsigned int mask = umask (0);

	  umask (mask);
	  chmod (abfd->filename,
		 (0777
		  & (buf.st_mode | ((S_IXUSR | S_IXGRP | S_IXOTH) &~ mask))));
	}
    }

  _bfd_delete_bfd (abfd);

  return ret;
}

/*
FUNCTION
	bfd_create

SYNOPSIS
	bfd *bfd_create(const char *filename, bfd *templ);

DESCRIPTION
	Create a new BFD in the manner of <<bfd_openw>>, but without
	opening a file. The new BFD takes the target from the target
	used by @var{template}. The format is always set to <<bfd_object>>.
*/

bfd *
bfd_create (filename, templ)
     const char *filename;
     bfd *templ;
{
  bfd *nbfd;

  nbfd = _bfd_new_bfd ();
  if (nbfd == NULL)
    return NULL;
  nbfd->filename = filename;
  if (templ)
    nbfd->xvec = templ->xvec;
  nbfd->direction = no_direction;
  bfd_set_format (nbfd, bfd_object);

  return nbfd;
}

/*
FUNCTION
	bfd_make_writable

SYNOPSIS
	bfd_boolean bfd_make_writable (bfd *abfd);

DESCRIPTION
	Takes a BFD as created by <<bfd_create>> and converts it
	into one like as returned by <<bfd_openw>>.  It does this
	by converting the BFD to BFD_IN_MEMORY.  It's assumed that
	you will call <<bfd_make_readable>> on this bfd later.

RETURNS
	<<TRUE>> is returned if all is ok, otherwise <<FALSE>>.
*/

bfd_boolean
bfd_make_writable(abfd)
     bfd *abfd;
{
  struct bfd_in_memory *bim;

  if (abfd->direction != no_direction)
    {
      bfd_set_error (bfd_error_invalid_operation);
      return FALSE;
    }

  bim = ((struct bfd_in_memory *)
	 bfd_malloc ((bfd_size_type) sizeof (struct bfd_in_memory)));
  abfd->iostream = (PTR) bim;
  /* bfd_bwrite will grow these as needed.  */
  bim->size = 0;
  bim->buffer = 0;

  abfd->flags |= BFD_IN_MEMORY;
  abfd->direction = write_direction;
  abfd->where = 0;

  return TRUE;
}

/*
FUNCTION
	bfd_make_readable

SYNOPSIS
	bfd_boolean bfd_make_readable (bfd *abfd);

DESCRIPTION
	Takes a BFD as created by <<bfd_create>> and
	<<bfd_make_writable>> and converts it into one like as
	returned by <<bfd_openr>>.  It does this by writing the
	contents out to the memory buffer, then reversing the
	direction.

RETURNS
	<<TRUE>> is returned if all is ok, otherwise <<FALSE>>.  */

bfd_boolean
bfd_make_readable(abfd)
     bfd *abfd;
{
  if (abfd->direction != write_direction || !(abfd->flags & BFD_IN_MEMORY))
    {
      bfd_set_error (bfd_error_invalid_operation);
      return FALSE;
    }

  if (! BFD_SEND_FMT (abfd, _bfd_write_contents, (abfd)))
    return FALSE;

  if (! BFD_SEND (abfd, _close_and_cleanup, (abfd)))
    return FALSE;


  abfd->arch_info = &bfd_default_arch_struct;

  abfd->where = 0;
  abfd->format = bfd_unknown;
  abfd->my_archive = (bfd *) NULL;
  abfd->origin = 0;
  abfd->opened_once = FALSE;
  abfd->output_has_begun = FALSE;
  abfd->section_count = 0;
  abfd->usrdata = (PTR) NULL;
  abfd->cacheable = FALSE;
  abfd->flags = BFD_IN_MEMORY;
  abfd->mtime_set = FALSE;

  abfd->target_defaulted = TRUE;
  abfd->direction = read_direction;
  abfd->sections = 0;
  abfd->symcount = 0;
  abfd->outsymbols = 0;
  abfd->tdata.any = 0;

  bfd_section_list_clear (abfd);
  bfd_check_format (abfd, bfd_object);

  return TRUE;
}

/*
INTERNAL_FUNCTION
	bfd_alloc

SYNOPSIS
	PTR bfd_alloc (bfd *abfd, size_t wanted);

DESCRIPTION
	Allocate a block of @var{wanted} bytes of memory attached to
	<<abfd>> and return a pointer to it.
*/


PTR
bfd_alloc (abfd, size)
     bfd *abfd;
     bfd_size_type size;
{
  PTR ret;

  if (size != (unsigned long) size)
    {
      bfd_set_error (bfd_error_no_memory);
      return NULL;
    }

  ret = objalloc_alloc (abfd->memory, (unsigned long) size);
  if (ret == NULL)
    bfd_set_error (bfd_error_no_memory);
  return ret;
}

PTR
bfd_zalloc (abfd, size)
     bfd *abfd;
     bfd_size_type size;
{
  PTR res;

  res = bfd_alloc (abfd, size);
  if (res)
    memset (res, 0, (size_t) size);
  return res;
}

/* Free a block allocated for a BFD.
   Note:  Also frees all more recently allocated blocks!  */

void
bfd_release (abfd, block)
     bfd *abfd;
     PTR block;
{
  objalloc_free_block ((struct objalloc *) abfd->memory, block);
}
