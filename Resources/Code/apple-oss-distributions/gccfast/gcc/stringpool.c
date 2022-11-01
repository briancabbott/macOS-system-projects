/* String pool for GCC.
   Copyright (C) 2000, 2001, 2002, 2004 Free Software Foundation, Inc.

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

/* String text, identifier text and identifier node allocator.  Strings
   allocated by ggc_alloc_string are stored in an obstack which is
   never shrunk.  Identifiers are uniquely stored in a hash table.

   We have our own private hash table implementation.  libiberty's
   hashtab.c is not used because it requires 100% average space
   overhead per string, which is unacceptable.  Also, this algorithm
   is faster.  */

#include "config.h"
#include "system.h"
#include "ggc.h"
#include "tree.h"
#include "hashtable.h"
/* APPLE LOCAL begin speed up pch reading */
#include "cpplib.h"
/* APPLE LOCAL end speed up pch reading */

/* The "" allocated string.  */
const char empty_string[] = "";

/* Character strings, each containing a single decimal digit.
   Written this way to save space.  */
const char digit_vector[] = {
  '0', 0, '1', 0, '2', 0, '3', 0, '4', 0,
  '5', 0, '6', 0, '7', 0, '8', 0, '9', 0
};

struct ht *ident_hash;
static struct obstack string_stack;

static hashnode alloc_node PARAMS ((hash_table *));
static int mark_ident PARAMS ((struct cpp_reader *, hashnode, const PTR));

/* Initialize the string pool.  */
void
init_stringpool ()
{
  /* Create with 16K (2^14) entries.  */
  ident_hash = ht_create (14);
  ident_hash->alloc_node = alloc_node;
  ident_hash->alloc_subobject = ggc_alloc;
  gcc_obstack_init (&string_stack);
}

/* Allocate a hash node.  */
static hashnode
alloc_node (table)
     hash_table *table ATTRIBUTE_UNUSED;
{
  return GCC_IDENT_TO_HT_IDENT (make_node (IDENTIFIER_NODE));
}

/* Allocate and return a string constant of length LENGTH, containing
   CONTENTS.  If LENGTH is -1, CONTENTS is assumed to be a
   nul-terminated string, and the length is calculated using strlen.
   If the same string constant has been allocated before, that copy is
   returned this time too.  */

const char *
ggc_alloc_string (contents, length)
     const char *contents;
     int length;
{
  if (length == -1)
    length = strlen (contents);

  if (length == 0)
    return empty_string;
  if (length == 1 && ISDIGIT (contents[0]))
    return digit_string (contents[0] - '0');

  obstack_grow0 (&string_stack, contents, length);
  return obstack_finish (&string_stack);
}

/* Return an IDENTIFIER_NODE whose name is TEXT (a null-terminated string).
   If an identifier with that name has previously been referred to,
   the same node is returned this time.  */

tree
get_identifier (text)
     const char *text;
{
  hashnode ht_node = ht_lookup (ident_hash,
				(const unsigned char *) text,
				strlen (text), HT_ALLOC);

  /* ht_node can't be NULL here.  */
  return HT_IDENT_TO_GCC_IDENT (ht_node);
}

/* Identical to get_identifier, except that the length is assumed
   known.  */

tree
get_identifier_with_length (text, length)
     const char *text;
     unsigned int length;
{
  hashnode ht_node = ht_lookup (ident_hash,
				(const unsigned char *) text,
				length, HT_ALLOC);

  /* ht_node can't be NULL here.  */
  return HT_IDENT_TO_GCC_IDENT (ht_node);
}


/* If an identifier with the name TEXT (a null-terminated string) has
   previously been referred to, return that node; otherwise return
   NULL_TREE.  */

tree
maybe_get_identifier (text)
     const char *text;
{
  hashnode ht_node;

  ht_node = ht_lookup (ident_hash, (const unsigned char *) text,
		       strlen (text), HT_NO_INSERT);
  if (ht_node)
    return HT_IDENT_TO_GCC_IDENT (ht_node);

  return NULL_TREE;
}

/* Report some basic statistics about the string pool.  */

void
stringpool_statistics ()
{
  ht_dump_statistics (ident_hash);
}

/* Mark an identifier for GC.  */

static int
mark_ident (pfile, h, v)
     struct cpp_reader *pfile ATTRIBUTE_UNUSED;
     hashnode h;
     const PTR v ATTRIBUTE_UNUSED;
{
  gt_ggc_m_9tree_node (HT_IDENT_TO_GCC_IDENT (h));
  return 1;
}

/* Mark the trees hanging off the identifier node for GGC.  These are
   handled specially (not using gengtype) because of the special
   treatment for strings.  */

void
ggc_mark_stringpool ()
{
  ht_forall (ident_hash, mark_ident, NULL);
}

/* Strings are _not_ GCed, but this routine exists so that a separate
   roots table isn't needed for the few global variables that refer
   to strings.  */

void
gt_ggc_m_S (x)
     void *x ATTRIBUTE_UNUSED;
{
}

/* Pointer-walking routine for strings (not very interesting, since
   strings don't contain pointers).  */

void
gt_pch_p_S (obj, x, op, cookie)
     void *obj ATTRIBUTE_UNUSED;
     void *x ATTRIBUTE_UNUSED;
     gt_pointer_operator op ATTRIBUTE_UNUSED;
     void *cookie ATTRIBUTE_UNUSED;
{
}

/* PCH pointer-walking routine for strings.  */

void
gt_pch_n_S (x)
     const void *x;
{
  gt_pch_note_object ((void *)x, (void *)x, &gt_pch_p_S);
}

/* Handle saving and restoring the string pool for PCH.  */

struct string_pool_data GTY(())
{
  struct ht_identifier * * 
    GTY((length ("%h.nslots"),
	 nested_ptr (union tree_node, "%h ? GCC_IDENT_TO_HT_IDENT (%h) : NULL",
		     "%h ? HT_IDENT_TO_GCC_IDENT (%h) : NULL")))
    entries;
  unsigned int nslots;
  unsigned int nelements;
};

static GTY(()) struct string_pool_data * spd;

void
gt_pch_save_stringpool ()
{
  spd = ggc_alloc (sizeof (*spd));
  spd->nslots = ident_hash->nslots;
  spd->nelements = ident_hash->nelements;
  spd->entries = ggc_alloc (sizeof (spd->entries[0]) * spd->nslots);
  memcpy (spd->entries, ident_hash->entries,
	  spd->nslots * sizeof (spd->entries[0]));
}

void
gt_pch_fixup_stringpool ()
{
}

void
gt_pch_restore_stringpool ()
{
  ht_load (ident_hash, spd->entries, spd->nslots, spd->nelements, false);
  spd = NULL;
}

#include "gt-stringpool.h"
