/* Exported functions from alias.c
   Copyright (C) 2004 Free Software Foundation, Inc.

This file is part of GCC.

GCC is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free
Software Foundation; either version 2, or (at your option) any later
version.

GCC is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or
FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received a copy of the GNU General Public License
along with GCC; see the file COPYING.  If not, write to the Free
Software Foundation, 59 Temple Place - Suite 330, Boston, MA
02111-1307, USA.  */

#ifndef GCC_ALIAS_H
#define GCC_ALIAS_H

extern HOST_WIDE_INT new_alias_set (void);
extern HOST_WIDE_INT get_varargs_alias_set (void);
extern HOST_WIDE_INT get_frame_alias_set (void);
extern void record_base_value (unsigned int, rtx, int);
extern bool component_uses_parent_alias_set (tree);

#endif /* GCC_ALIAS_H */
