/* Language-dependent hooks for C++.
   Copyright 2001 Free Software Foundation, Inc.
   Contributed by Alexandre Oliva  <aoliva@redhat.com>

This file is part of GNU CC.

GNU CC is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU CC is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU CC; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "config.h"
#include "system.h"
#include "tree.h"
#include "cp-tree.h"
#include "c-common.h"
#include "toplev.h"
#include "langhooks.h"
#include "langhooks-def.h"

/* APPLE LOCAL PFE */
#ifdef PFE
#include "pfe/pfe.h"
#include "pfe/cp-freeze-thaw.h"
#endif

/* APPLE LOCAL Objective-C++  */
static void lang_init_options PARAMS ((void));

/* APPLE LOCAL Objective-C++: made the following extern in cp-tree.h.  */
/* static HOST_WIDE_INT cxx_get_alias_set PARAMS ((tree)); */

/* APPLE LOCAL Objective-C++  */
#include "cp-lang.h"

/* Each front end provides its own hooks, for toplev.c.  */
const struct lang_hooks lang_hooks = LANG_HOOKS_INITIALIZER;

/* APPLE LOCAL Objective-C++ -- moved cxx_get_alias_set() to decl2.c. */

static void
lang_init_options ()
{
  cxx_init_options (clk_cplusplus);
}

/* APPLE LOCAL begin Objective-C++ */
void
objc_clear_super_receiver ()
{
}
/* APPLE LOCAL end Objective-C++ */
