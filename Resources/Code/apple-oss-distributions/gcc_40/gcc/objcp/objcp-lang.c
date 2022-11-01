/* APPLE LOCAL file Objective-C++ */
/* Language-dependent hooks for Objective-C++.
   Copyright 2001, 2002, 2004 Free Software Foundation, Inc.
   Contributed by Ziemowit Laski  <zlaski@apple.com>

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

#include "config.h"
#include "system.h"
#include "coretypes.h"
#include "tm.h"
#include "tree.h"
#include "cp-tree.h"
#include "c-common.h"
#include "toplev.h"
#include "objc-act.h"
#include "langhooks.h"
#include "langhooks-def.h"
#include "diagnostic.h"
#include "cxx-pretty-print.h"
#include "debug.h"
#include "cp-objcp-common.h"

enum c_language_kind c_language = clk_objcxx;

/* Lang hooks common to C++ and ObjC++ are declared in cp/cp-objcp-common.h;
   consequently, there should be very few hooks below.  */

#undef LANG_HOOKS_NAME
#define LANG_HOOKS_NAME "GNU Objective-C++"
#undef LANG_HOOKS_INIT
#define LANG_HOOKS_INIT objc_init
#undef LANG_HOOKS_DECL_PRINTABLE_NAME
#define LANG_HOOKS_DECL_PRINTABLE_NAME	objc_printable_name
#undef LANG_HOOKS_TYPES_COMPATIBLE_P
#define LANG_HOOKS_TYPES_COMPATIBLE_P objc_types_compatible_p
/* APPLE LOCAL begin Radar 4015820 FSF candidate */
#undef LANG_HOOKS_GIMPLIFY_EXPR 
#define LANG_HOOKS_GIMPLIFY_EXPR objc_gimplify_expr
/* APPLE LOCAL end Radar 4105820 FSF candidate */
/* APPLE LOCAL begin Radar 3926484 FSF candidate */
#undef LANG_HOOKS_GET_CALLEE_FNDECL
#define LANG_HOOKS_GET_CALLEE_FNDECL	objc_get_callee_fndecl
/* APPLE LOCAL end Radar 3926484 FSF candidate */
/* Each front end provides its own lang hook initializer.  */
const struct lang_hooks lang_hooks = LANG_HOOKS_INITIALIZER;

/* Tree code classes.  */

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) TYPE,

const enum tree_code_class tree_code_type[] = {
#include "tree.def"
  tcc_exceptional,
#include "c-common.def"
  tcc_exceptional,
#include "cp-tree.def"
  tcc_exceptional,
#include "objc-tree.def"
};
#undef DEFTREECODE

/* Table indexed by tree code giving number of expression
   operands beyond the fixed part of the node structure.
   Not used for types or decls.  */

#define DEFTREECODE(SYM, NAME, TYPE, LENGTH) LENGTH,

const unsigned char tree_code_length[] = {
#include "tree.def"
  0,
#include "c-common.def"
  0,
#include "cp-tree.def"
  0,
#include "objc-tree.def"
};
#undef DEFTREECODE

/* Names of tree components.
   Used for printing out the tree and error messages.  */
#define DEFTREECODE(SYM, NAME, TYPE, LEN) NAME,

const char *const tree_code_name[] = {
#include "tree.def"
  "@@dummy",
#include "c-common.def"
  "@@dummy",
#include "cp-tree.def"
  "@@dummy",
#include "objc-tree.def"
};
#undef DEFTREECODE

/* Lang hook routines common to C++ and ObjC++ appear in cp/cp-objcp-common.c;
   there should be very few (if any) routines below.  */

tree
objcp_tsubst_copy_and_build (tree t, tree args, tsubst_flags_t complain, 
			     tree in_decl, bool function_p ATTRIBUTE_UNUSED)
{
#define RECURSE(NODE) \
  tsubst_copy_and_build (NODE, args, complain, in_decl, /*function_p=*/false)

  /* The following two can only occur in Objective-C++.  */

  switch ((int) TREE_CODE (t))
    {
    case MESSAGE_SEND_EXPR:
      return objc_finish_message_expr
	(RECURSE (TREE_OPERAND (t, 0)),
	 TREE_OPERAND (t, 1),  /* No need to expand the selector.  */
	 RECURSE (TREE_OPERAND (t, 2)));

    case CLASS_REFERENCE_EXPR:
      return objc_get_class_reference
	(RECURSE (TREE_OPERAND (t, 0)));

    default:
      break;
    }

  /* Fall back to C++ processing.  */
  return NULL_TREE;

#undef RECURSE
}

void
finish_file (void)
{
  objc_finish_file ();
}

#include "gtype-objcp.h"
