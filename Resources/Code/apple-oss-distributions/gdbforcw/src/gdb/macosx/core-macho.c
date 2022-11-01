/* Mac OS X support for GDB, the GNU debugger.
   Copyright 1997, 1998, 1999, 2000, 2001, 2002
   Free Software Foundation, Inc.

   Contributed by Apple Computer, Inc.

   This file is part of GDB.

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
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#if defined (TARGET_POWERPC)
#include "ppc-macosx-thread-status.h"
#include "ppc-macosx-regs.h"
#elif defined (TARGET_I386)
#include "i386-macosx-thread-status.h"
#include "i386-macosx-tdep.h"
#else
#error "unsupported architecture"
#endif

#include "defs.h"
#include "gdb_string.h"
#include "frame.h"
#include "inferior.h"
#include "symtab.h"
#include "command.h"
#include "bfd.h"
#include "target.h"
#include "gdbcore.h"
#include "gdbthread.h"
#include "regcache.h"
#include "bfd.h"

#include <errno.h>
#include <signal.h>
#include <fcntl.h>

struct target_ops macho_core_ops;

static struct sec *
lookup_section (bfd *abfd, unsigned int n)
{
  struct bfd_section *sect = NULL;

  CHECK_FATAL (abfd != NULL);
  CHECK_FATAL (n < bfd_count_sections (abfd));

  sect = abfd->sections;

  while (n-- > 0)
    sect = sect->next;

  return sect;
}

static void
check_thread (bfd *abfd, asection *asect, unsigned int num)
{
  const char *sname = bfd_section_name (abfd, asect);
  unsigned int i;

#if defined (TARGET_POWERPC)
  const char *expected = "LC_THREAD.PPC_THREAD_STATE.";
#elif defined (TARGET_I386)
  const char *expected = "LC_THREAD.i386_THREAD_STATE.";
#else
#error "unsupported architecture"
#endif

  if (strncmp (sname, expected, strlen (expected)) != 0)
    {
      return;
    }

  i = strtol (sname + strlen (expected), NULL, 0);

  add_thread (ptid_build (1, i, num));
  if (ptid_equal (inferior_ptid, null_ptid))
    {
      inferior_ptid = ptid_build (1, i, num);
    }
}

static void
core_close_1 (void *arg)
{
  char *name;

  if (core_bfd == NULL)
    {
      return;
    }

  name = bfd_get_filename (core_bfd);
  if (!bfd_close (core_bfd))
    {
      warning ("Unable to close \"%s\": %s", name,
               bfd_errmsg (bfd_get_error ()));
    }

  core_bfd = NULL;
  inferior_ptid = null_ptid;

#ifdef CLEAR_SOLIB
  CLEAR_SOLIB ();
#endif

  if (macho_core_ops.to_sections)
    {
      xfree (macho_core_ops.to_sections);
      macho_core_ops.to_sections = NULL;
      macho_core_ops.to_sections_end = NULL;
    }
}

static void
core_close (int quitting)
{
  core_close_1 (NULL);
}

static void
core_open (char *filename, int from_tty)
{
  const char *p;
  int siggy;
  struct cleanup *old_chain;
  char *temp;
  bfd *temp_bfd;
  int ontop;
  int scratch_chan;
  struct bfd_section *sect;
  unsigned int i;

  target_preopen (from_tty);
  if (!filename)
    {
      error (core_bfd ?
             "No core file specified.  (Use `detach' to stop debugging a core file.)"
             : "No core file specified.");
    }

  filename = tilde_expand (filename);
  if (filename[0] != '/')
    {
      temp = concat (current_directory, "/", filename, NULL);
      xfree (filename);
      filename = temp;
    }

  old_chain = make_cleanup (free, filename);

  scratch_chan = open (filename, write_files ? O_RDWR : O_RDONLY, 0);
  if (scratch_chan < 0)
    perror_with_name (filename);

  temp_bfd = bfd_fdopenr (filename, gnutarget, scratch_chan);
  if (temp_bfd == NULL)
    perror_with_name (filename);

  if (bfd_check_format (temp_bfd, bfd_core) == 0)
    {
      /* Do it after the err msg */
      /* FIXME: should be checking for errors from bfd_close (for one thing,
         on error it does not free all the storage associated with the
         bfd).  */
      make_cleanup_bfd_close (temp_bfd);
      error ("\"%s\" is not a core dump: %s",
             filename, bfd_errmsg (bfd_get_error ()));
    }

  /* Looks semi-reasonable.  Toss the old core file and work on the new.  */

  discard_cleanups (old_chain); /* Don't free filename any more */
  unpush_target (&macho_core_ops);
  core_bfd = temp_bfd;
  old_chain = make_cleanup (core_close_1, core_bfd);

  validate_files ();

  /* Find the data section */
  if (build_section_table (core_bfd, &macho_core_ops.to_sections,
                           &macho_core_ops.to_sections_end))
    error ("\"%s\": Can't find sections: %s",
           bfd_get_filename (core_bfd), bfd_errmsg (bfd_get_error ()));

  ontop = !push_target (&macho_core_ops);
  discard_cleanups (old_chain);

  p = bfd_core_file_failing_command (core_bfd);
  if (p)
    printf_filtered ("Core was generated by `%s'.\n", p);

  siggy = bfd_core_file_failing_signal (core_bfd);
  if (siggy > 0)
    printf_filtered ("Program terminated with signal %d, %s.\n", siggy,
                     target_signal_to_string (target_signal_from_host
                                              (siggy)));

  /* Build up thread list from BFD sections. */

  init_thread_list ();

  inferior_ptid = null_ptid;
  i = 0;

  for (sect = core_bfd->sections; sect != NULL; i++, sect = sect->next)
    check_thread (core_bfd, sect, i);

  CHECK_FATAL (i == core_bfd->section_count);

  if (ptid_equal (inferior_ptid, null_ptid))
    {
      error ("Core file contained no thread-specific data\n");
    }

  if (ontop)
    {
      /* Fetch all registers from core file.  */
      target_fetch_registers (-1);

      /* Now, set up the frame cache, and print the top of stack.  */
      flush_cached_frames ();
      select_frame (get_current_frame ());
      print_stack_frame (deprecated_selected_frame,
                         frame_relative_level (deprecated_selected_frame), 1);
    }
  else
    {
      warning
        ("you won't be able to access this core file until you terminate\n"
         "your %s; do ``info files''", target_longname);
    }
}

static void
core_detach (char *args, int from_tty)
{
  if (args)
    error ("Too many arguments");
  unpush_target (&macho_core_ops);
  reinit_frame_cache ();
  if (from_tty)
    printf_filtered ("No core file now.\n");
}

static void
core_fetch_section_registers (asection *sec, int regno)
{
  unsigned size;
  unsigned char *regs;

  size = bfd_section_size (core_bfd, sec);
  regs = (unsigned char *) alloca (size);
  if (regs == NULL)
    {
      fprintf_filtered (gdb_stderr,
                        "Unable to allocate space to read registers\n");
    }
  if (bfd_get_section_contents (core_bfd, sec, regs, (file_ptr) 0, size) != 1)
    {
      fprintf_filtered (gdb_stderr,
                        "Unable to read register data from core file\n");
    }

#if defined (TARGET_POWERPC)
  ppc_macosx_fetch_gp_registers ((gdb_ppc_thread_state_t *) regs);
#elif defined (TARGET_I386)
  i386_macosx_fetch_gp_registers ((gdb_i386_thread_state_t *) regs);
#else
#error "unsupported architecture"
#endif
}

static void
core_fetch_registers (int regno)
{
  asection *sec = lookup_section (core_bfd, ptid_get_tid (inferior_ptid));
  CHECK (sec != NULL);
  core_fetch_section_registers (sec, regno);
}

static void
core_files_info (struct target_ops *t)
{
  print_section_info (t, core_bfd);
}

static char *
macosx_core_ptid_to_str (ptid_t pid)
{
  static char buf[128];
  sprintf (buf, "core thread %lu", ptid_get_lwp (pid));
  return buf;
}

static int
core_thread_alive (ptid_t pid)
{
  return 1;
}

static void
core_prepare_to_store (void)
{
}

static void
core_store_registers (int regno)
{
}

static void
init_macho_core_ops ()
{
  macho_core_ops.to_shortname = "core-macho";
  macho_core_ops.to_longname = "Mach-O core dump file";
  macho_core_ops.to_doc =
    "Use a core file as a target.  Specify the filename of the core file.";
  macho_core_ops.to_open = core_open;
  macho_core_ops.to_close = core_close;
  macho_core_ops.to_attach = find_default_attach;
  macho_core_ops.to_detach = core_detach;
  macho_core_ops.to_fetch_registers = core_fetch_registers;
  macho_core_ops.to_prepare_to_store = core_prepare_to_store;
  macho_core_ops.to_store_registers = core_store_registers;
  macho_core_ops.to_xfer_memory = xfer_memory;
  macho_core_ops.to_files_info = core_files_info;
  macho_core_ops.to_create_inferior = find_default_create_inferior;
  macho_core_ops.to_pid_to_str = macosx_core_ptid_to_str;
  macho_core_ops.to_stratum = core_stratum;
  macho_core_ops.to_has_all_memory = 0;
  macho_core_ops.to_has_memory = 1;
  macho_core_ops.to_has_stack = 1;
  macho_core_ops.to_has_registers = 1;
  macho_core_ops.to_has_execution = 0;
  macho_core_ops.to_thread_alive = core_thread_alive;
  macho_core_ops.to_magic = OPS_MAGIC;
};

void
_initialize_core_macho ()
{
  init_macho_core_ops ();
  add_target (&macho_core_ops);
}
