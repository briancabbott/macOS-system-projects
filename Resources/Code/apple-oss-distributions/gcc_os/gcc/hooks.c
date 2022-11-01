/* General-purpose hooks.
   Copyright (C) 2002, 2003 Free Software Foundation, Inc.

This program is free software; you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by the
Free Software Foundation; either version 2, or (at your option) any
later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

 In other words, you are welcome to use, share and improve this program.
 You are forbidden to forbid anyone else to use, share and improve
 what you give them.   Help stamp out software-hoarding!  */

/* This file contains generic hooks that can be used as defaults for
   target or language-dependent hook initializers.  */

#include "config.h"
#include "system.h"
#include "hooks.h"

/* Generic hook that does absolutely zappo.  */
void
hook_void_void ()
{
}

/* Generic hook that takes no arguments and returns false.  */
bool
hook_bool_void_false ()
{
  return false;
}

/* Generic hook that takes (tree, int) and does nothing.  */
void
hook_void_tree_int (a, b)
     tree a ATTRIBUTE_UNUSED;
     int b ATTRIBUTE_UNUSED;
{
}

/* Generic hook that takes (FILE *, const char *) and does nothing.  */
void
hook_void_FILEptr_constcharptr (a, b)
     FILE *a ATTRIBUTE_UNUSED;
     const char *b ATTRIBUTE_UNUSED;
{
}

/* Used for the TARGET_ASM_CAN_OUTPUT_MI_THUNK hook.  */
bool
hook_bool_tree_hwi_hwi_tree_false (a, b, c, d)
     tree a ATTRIBUTE_UNUSED;
     HOST_WIDE_INT b ATTRIBUTE_UNUSED;
     HOST_WIDE_INT c ATTRIBUTE_UNUSED;
     tree d ATTRIBUTE_UNUSED;
{
  return false;
}

bool
hook_bool_tree_hwi_hwi_tree_true (a, b, c, d)
     tree a ATTRIBUTE_UNUSED;
     HOST_WIDE_INT b ATTRIBUTE_UNUSED;
     HOST_WIDE_INT c ATTRIBUTE_UNUSED;
     tree d ATTRIBUTE_UNUSED;
{
  return true;
}

bool
default_can_output_mi_thunk_no_vcall (a, b, c, d)
     tree a ATTRIBUTE_UNUSED;
     HOST_WIDE_INT b ATTRIBUTE_UNUSED;
     HOST_WIDE_INT c;
     tree d ATTRIBUTE_UNUSED;
{
  return c == 0;
}

/* ??? Used for comp_type_attributes, which ought to return bool.  */
int
hook_int_tree_tree_1 (a, b)
     tree a ATTRIBUTE_UNUSED;
     tree b ATTRIBUTE_UNUSED;
{
  return 1;
}

/* APPLE LOCAL begin - 3.4 scheduler update */
int
hook_int_rtx_0 (a)
     rtx a ATTRIBUTE_UNUSED;
{
  return 0;
}
/* APPLE LOCAL end - 3.4 scheduler update */

void
hook_void_tree (a)
     tree a ATTRIBUTE_UNUSED;
{
}

void
hook_void_tree_treeptr (a, b)
     tree a ATTRIBUTE_UNUSED;
     tree *b ATTRIBUTE_UNUSED;
{
}

bool
hook_bool_tree_false (a)
     tree a ATTRIBUTE_UNUSED;
{
  return false;
}

/* APPLE LOCAL begin - 3.4 scheduler update */
bool
hook_bool_tree_tree_false (a, b)
     tree a ATTRIBUTE_UNUSED;
     tree b ATTRIBUTE_UNUSED;
{
  return false;
}

bool
hook_bool_rtx_false (a)
     rtx a ATTRIBUTE_UNUSED;
{
  return false;
}

bool
hook_bool_rtx_int_int_intp_false (a, b, c, d)
     rtx a ATTRIBUTE_UNUSED;
     int b ATTRIBUTE_UNUSED;
     int c ATTRIBUTE_UNUSED;
     int *d ATTRIBUTE_UNUSED;
{
  return false;
}

/* Generic hook that takes an rtx and returns it.  */
rtx
hook_rtx_rtx_identity (x)
     rtx x;
{
  return x;
}

/* APPLE LOCAL end - 3.4 scheduler update */
/* APPLE LOCAL begin pch */

/* Generic hook that takes a size_t and returns NULL.  */
void *
hook_voidp_size_t_null (size_t a ATTRIBUTE_UNUSED)
{
  return NULL;
}

/* Generic hook that takes a size_t and a pointer and returns false.  */
bool
hook_bool_voidp_size_t_false (void * a ATTRIBUTE_UNUSED,
			      size_t b ATTRIBUTE_UNUSED)
{
  return false;
}
/* APPLE LOCAL end pch */
