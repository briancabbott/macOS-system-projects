/* Definitions of target machine for GNU compiler.
   Renesas H8/300 version generating coff
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.
   Contributed by Steve Chamberlain (sac@cygnus.com),
   Jim Wilson (wilson@cygnus.com), and Doug Evans (dje@cygnus.com).

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GCC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#ifndef GCC_H8300_COFF_H
#define GCC_H8300_COFF_H

#define SDB_DEBUGGING_INFO 1
#define SDB_DELIM	"\n"

/* Override definition in dbxcoff.h.  */
/* Generate a blank trailing N_SO to mark the end of the .o file, since
   we can't depend upon the linker to mark .o file boundaries with
   embedded stabs.  */

#undef DBX_OUTPUT_MAIN_SOURCE_FILE_END
#define DBX_OUTPUT_MAIN_SOURCE_FILE_END(FILE, FILENAME)			\
  fprintf (FILE,							\
	   "\t.text\n.stabs \"\",%d,0,0,.Letext\n.Letext:\n", N_SO)

/* This is how to output an assembler line
   that says to advance the location counter by SIZE bytes.  */

#define ASM_OUTPUT_IDENT(FILE, NAME)			\
  fprintf (FILE, "%s\"%s\"\n", IDENT_ASM_OP, NAME)

#define IDENT_ASM_OP "\t.ident\t"
#define INIT_SECTION_ASM_OP "\t.section .init"
#define READONLY_DATA_SECTION_ASM_OP "\t.section .rodata"

/* Switch into a generic section.  */
#define TARGET_ASM_NAMED_SECTION h8300_asm_named_section

/* A bit-field declared as `int' forces `int' alignment for the struct.  */
#define PCC_BITFIELD_TYPE_MATTERS  0

#endif /* h8300/coff.h */
