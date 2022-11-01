/* Functions for generic Darwin as target machine for GNU C compiler.
   Copyright (C) 1989, 1990, 1991, 1992, 1993, 2000, 2001, 2002
   Free Software Foundation, Inc.
   Contributed by Apple Computer Inc.

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
#include "rtl.h"
#include "regs.h"
#include "hard-reg-set.h"
#include "real.h"
#include "insn-config.h"
#include "conditions.h"
#include "insn-flags.h"
#include "output.h"
#include "insn-attr.h"
#include "flags.h"
#include "tree.h"
#include "expr.h"
#include "reload.h"
#include "function.h"
#include "ggc.h"
#include "langhooks.h"
#include "tm_p.h"

extern void machopic_output_stub PARAMS ((FILE *, const char *, const char *));

static int machopic_data_defined_p PARAMS ((const char *));
static void update_non_lazy_ptrs PARAMS ((const char *));
static void update_stubs PARAMS ((const char *));

/* APPLE LOCAL prototypes  */
static tree machopic_non_lazy_ptr_list_entry PARAMS ((const char*, int));
static tree machopic_stub_list_entry PARAMS ((const char *));

/* APPLE LOCAL begin coalescing  */
void
make_decl_coalesced (decl, private_extern_p)
     tree decl;
     int private_extern_p;      /* 0 for global, 1 for private extern */
{
  int no_toc_p = 1;             /* Don't add to table of contents */
#if 0
  const char *decl_name = IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (decl));
#endif
  static const char *const names[4] = {
	"__TEXT,__textcoal,coalesced",
	"__TEXT,__textcoal_nt,coalesced,no_toc",
	"__DATA,__datacoal,coalesced",
	"__DATA,__datacoal_nt,coalesced,no_toc",
  };
  const char *sec;
  int idx;

  /* Do nothing if coalescing is disabled.  */
  if (!COALESCING_ENABLED_P())
    return;

  /* We *do* need to mark these *INTERNAL* functions coalesced: though
     these pseudo-functions themselves will never appear, their cloned
     descendants need to be marked coalesced too.  */
#if 0
  /* Don't touch anything with " *INTERNAL" in its name.  */
  if (strstr (decl_name, " *INTERNAL") != NULL)
    return;
#endif

  DECL_COALESCED (decl) = 1;
  if (private_extern_p)
    DECL_PRIVATE_EXTERN (decl) = 1;
  TREE_PUBLIC (decl) = 1;

  idx = 0;
  if (TREE_CODE (decl) != FUNCTION_DECL)
    idx = 2;
  sec = names[idx + (no_toc_p ? 1 : 0)];

  DECL_SECTION_NAME (decl) = build_string (strlen (sec), sec);
}
/* APPLE LOCAL end coalescing  */

/* APPLE LOCAL begin backport 3721776 fix from FSF mainline. */
/* Nonzero if the user passes the -mone-byte-bool switch, which forces
   sizeof(bool) to be 1. */
const char *darwin_one_byte_bool = 0;
/* APPLE LOCAL end backport 3721776 fix from FSF mainline. */

int
name_needs_quotes (name)
     const char *name;
{
  int c;
  while ((c = *name++) != '\0')
    /* APPLE LOCAL Objective-C++ */
    if (! ISIDNUM (c) && c != '.' && c != '$')
      return 1;
  return 0;
}

/* 
 * flag_pic = 1 ... generate only indirections
 * flag_pic = 2 ... generate indirections and pure code
 */

/* This module assumes that (const (symbol_ref "foo")) is a legal pic
   reference, which will not be changed.  */

static GTY(()) tree machopic_defined_list;

enum machopic_addr_class
machopic_classify_ident (ident)
     tree ident;
{
  const char *name = IDENTIFIER_POINTER (ident);
  int lprefix = (((name[0] == '*' || name[0] == '&')
		  && (name[1] == 'L' || (name[1] == '"' && name[2] == 'L')))
		 || (   name[0] == '_' 
		     && name[1] == 'O' 
		     && name[2] == 'B' 
		     && name[3] == 'J'
		     && name[4] == 'C'
		     && name[5] == '_'));
  tree temp;

  if (name[0] != '!')
    {
      /* Here if no special encoding to be found.  */
      if (lprefix)
	{
	  const char *name = IDENTIFIER_POINTER (ident);
	  int len = strlen (name);

	  if ((len > 5 && !strcmp (name + len - 5, "$stub"))
	      || (len > 6 && !strcmp (name + len - 6, "$stub\"")))
	    return MACHOPIC_DEFINED_FUNCTION;
	  return MACHOPIC_DEFINED_DATA;
	}

      for (temp = machopic_defined_list;
	   temp != NULL_TREE;
	   temp = TREE_CHAIN (temp))
	{
	  if (ident == TREE_VALUE (temp))
	    return MACHOPIC_DEFINED_DATA;
	}

      if (TREE_ASM_WRITTEN (ident))
	return MACHOPIC_DEFINED_DATA;

      return MACHOPIC_UNDEFINED;
    }

  else if (name[1] == 'D')
    return MACHOPIC_DEFINED_DATA;

  else if (name[1] == 'T')
    return MACHOPIC_DEFINED_FUNCTION;

  /* It is possible that someone is holding a "stale" name, which has
     since been defined.  See if there is a "defined" name (i.e,
     different from NAME only in having a '!D_' or a '!T_' instead of
     a '!d_' or '!t_' prefix) in the identifier hash tables.  If so, say
     that this identifier is defined.  */
  else if (name[1] == 'd' || name[1] == 't')
    {
      char *new_name;
      new_name = (char *)alloca (strlen (name) + 1);
      strcpy (new_name, name);
      new_name[1] = (name[1] == 'd') ? 'D' : 'T';
      if (maybe_get_identifier (new_name) != NULL)
	return  (name[1] == 'd') ? MACHOPIC_DEFINED_DATA
				 : MACHOPIC_DEFINED_FUNCTION;
    }

  for (temp = machopic_defined_list; temp != NULL_TREE; temp = TREE_CHAIN (temp))
    {
      if (ident == TREE_VALUE (temp))
	{
	  if (name[1] == 'T')
	    return MACHOPIC_DEFINED_FUNCTION;
	  else
	    return MACHOPIC_DEFINED_DATA;
	}
    }
  
  if (name[1] == 't' || name[1] == 'T')
    {
      if (lprefix)
	return MACHOPIC_DEFINED_FUNCTION;
      else
	return MACHOPIC_UNDEFINED_FUNCTION;
    }
  else
    {
      if (lprefix)
	return MACHOPIC_DEFINED_DATA;
      else
	return MACHOPIC_UNDEFINED_DATA;
    }
}

     
enum machopic_addr_class
machopic_classify_name (name)
     const char *name;
{
  return machopic_classify_ident (get_identifier (name));
}

int
machopic_ident_defined_p (ident)
     tree ident;
{
  switch (machopic_classify_ident (ident))
    {
    case MACHOPIC_UNDEFINED:
    case MACHOPIC_UNDEFINED_DATA:
    case MACHOPIC_UNDEFINED_FUNCTION:
      return 0;
    default:
      return 1;
    }
}

/* APPLE LOCAL BEGIN fix-and-continue mrs  */
/* Determine, based upon only the name, which instances should be
   rebound to the newly loaded file, and which instances should be
   bind to the previously loaded file.  This might turn out to be
   very hard to do based upon only the name, for now, it might be
   enough.  */

static int
indirect_data (name)
     const char *name;
{
  int lprefix;

  if (flag_indirect_data == 0)
    return 0;

  name = darwin_strip_name_encoding (name);
  lprefix = (((name[0] == '*' || name[0] == '&')
	      && (name[1] == 'L' || (name[1] == '"' && name[2] == 'L')))
	     || (name[0] == '_' 
		 && name[1] == 'O' 
		 && name[2] == 'B' 
		 && name[3] == 'J'
		 && name[4] == 'C'
		 && name[5] == '_'));

  /* Symbols that begin with 'L' are for compiler generated local
     things of the translation unit.  */
  return ! lprefix;
}
/* APPLE LOCAL END fix-and-continue mrs  */

static int
machopic_data_defined_p (name)
     const char *name;
{
  /* APPLE LOCAL BEGIN fix-and-continue mrs  */
  if (indirect_data (name))
    return 0;
  /* APPLE LOCAL END fix-and-continue mrs  */

  switch (machopic_classify_ident (get_identifier (name)))
    {
    case MACHOPIC_DEFINED_DATA:
      return 1;
    default:
      return 0;
    }
}

int
machopic_name_defined_p (name)
     const char *name;
{
  return machopic_ident_defined_p (get_identifier (name));
}

void
machopic_define_ident (ident)
     tree ident;
{
  if (!machopic_ident_defined_p (ident))
    machopic_defined_list = 
      tree_cons (NULL_TREE, ident, machopic_defined_list);
}

void
machopic_define_name (name)
     const char *name;
{
  machopic_define_ident (get_identifier (name));
}

/* This is a static to make inline functions work.  The rtx
   representing the PIC base symbol always points to here.  

   FIXME: The rest of the compiler doesn't expect strings to change.  */

static GTY(()) char * function_base;
static GTY(()) const char * function_base_func_name;
static GTY(()) int current_pic_label_num;

const char *
machopic_function_base_name ()
{
  const char *current_name;

  /* APPLE LOCAL  dynamic-no-pic  */
  if (MACHO_DYNAMIC_NO_PIC_P ())
    abort ();
  current_name = 
    IDENTIFIER_POINTER (DECL_ASSEMBLER_NAME (current_function_decl));

  if (function_base_func_name != current_name)
    {
      /* APPLE LOCAL deep branch prediction pic-base */
      if (cfun)
	current_function_uses_pic_offset_table = 1;

      /* Save mucho space and time.  Some of the C++ mangled names are over
	 700 characters long!  Note that we produce a label containing a '-'
	 if the function we're compiling is an Objective-C method, as evinced
	 by the incredibly scientific test below.  This is because code in
	 rs6000.c makes the same ugly test when loading the PIC reg.  */
 
      /* It's hard to describe just how ugly this is.  The reason for
         the '%011d' is that after a PCH load, we can't change the
         size of the string, because PCH will have uniqued it and
         allocated it in the string pool.  */
      if (function_base == NULL)
	function_base = ggc_alloc_string ("", sizeof ("*\"L12345678901$pb\""));

      ++current_pic_label_num;
      if (*current_name == '+' || *current_name == '-')
	sprintf (function_base, "*\"L-%010d$pb\"", current_pic_label_num);
      else
	sprintf (function_base, "*\"L%011d$pb\"", current_pic_label_num);

      function_base_func_name = current_name;
    }

  return function_base;
}

static GTY(()) tree machopic_non_lazy_pointers;

/* Return a non-lazy pointer name corresponding to the given name,
   either by finding it in our list of pointer names, or by generating
   a new one.  */

/* APPLE LOCAL weak import */
static tree
machopic_non_lazy_ptr_list_entry (name, create_p)
     const char *name;
     int create_p;
{
  tree temp, ident = (create_p) ? get_identifier (name) : NULL;
  
  for (temp = machopic_non_lazy_pointers;
       temp != NULL_TREE; 
       temp = TREE_CHAIN (temp))
    {
      if (ident == TREE_VALUE (temp))
	return temp;
    }

  name = darwin_strip_name_encoding (name);

  /* Try again, but comparing names this time.  */
  for (temp = machopic_non_lazy_pointers;
       temp != NULL_TREE; 
       temp = TREE_CHAIN (temp))
    {
      if (TREE_VALUE (temp))
	{
	  const char *temp_name = IDENTIFIER_POINTER (TREE_VALUE (temp));
	  temp_name = darwin_strip_name_encoding (temp_name);
	  if (strcmp (name, temp_name) == 0)
	    return temp;
	}
    }

  if (create_p) {
    char *buffer;
    tree ptr_name;

    buffer = alloca (strlen (name) + 20);

    strcpy (buffer, "&L");
    if (name[0] == '*')
      strcat (buffer, name+1);
    else
      {
	strcat (buffer, "_");
	strcat (buffer, name);
      }
      
    strcat (buffer, "$non_lazy_ptr");
    ptr_name = get_identifier (buffer);

    machopic_non_lazy_pointers 
      = tree_cons (ptr_name, ident, machopic_non_lazy_pointers);

    TREE_USED (machopic_non_lazy_pointers) = 0;

    return machopic_non_lazy_pointers;
  }

  return NULL;
}

/* Was the variable NAME ever referenced?  */
int
machopic_var_referred_to_p (name)
     const char *name;
{
  return (machopic_non_lazy_ptr_list_entry (name, /*create:*/ 0) != NULL);
}

const char *
machopic_non_lazy_ptr_name (name)
     const char *name;
{
    return IDENTIFIER_POINTER (TREE_PURPOSE 
		(machopic_non_lazy_ptr_list_entry (name, /*create:*/ 1)));
}

static GTY(()) tree machopic_stubs;

/* Return the name of the stub corresponding to the given name,
   generating a new stub name if necessary.  */

/* APPLE LOCAL weak import */
/* machopic_stub_list_entry separated from machopic_stub_name */
static tree
machopic_stub_list_entry (name)
     const char *name;
{
  tree temp, ident = get_identifier (name);
  const char *tname;

  for (temp = machopic_stubs;
       temp != NULL_TREE; 
       temp = TREE_CHAIN (temp))
    {
      if (ident == TREE_VALUE (temp))
	return temp;
      tname = IDENTIFIER_POINTER (TREE_VALUE (temp));
      if (strcmp (name, tname) == 0)
	return temp;

      /* APPLE LOCAL Stripped encodings ('!T_' and '!t_') should match.  */
      if (name [0] == '!' && tname[0] == '!'
	  && strcmp (name + 4, tname + 4) == 0)
	return temp;

      /* A library call name might not be section-encoded yet, so try
	 it against a stripped name.  */
      if (name[0] != '!'
	  && tname[0] == '!'
	  && strcmp (name, tname + 4) == 0)
	return temp;
    }

  name = darwin_strip_name_encoding (name);

  {
    char *buffer;
    tree ptr_name;
    int needs_quotes = name_needs_quotes (name);

    buffer = alloca (strlen (name) + 20);

    if (needs_quotes)
      strcpy (buffer, "&\"L");
    else
      strcpy (buffer, "&L");
    if (name[0] == '*')
      {
	strcat (buffer, name+1);
      }
    else
      {
	strcat (buffer, "_");
	strcat (buffer, name);
      }

    if (needs_quotes)
      strcat (buffer, "$stub\"");
    else
      strcat (buffer, "$stub");
    ptr_name = get_identifier (buffer);

    machopic_stubs = tree_cons (ptr_name, ident, machopic_stubs);
    TREE_USED (machopic_stubs) = 0;

    return machopic_stubs;
  }
}

const char * 
machopic_stub_name (name)
     const char *name;
{
  return IDENTIFIER_POINTER (TREE_PURPOSE (machopic_stub_list_entry (name)));
}

void
machopic_validate_stub_or_non_lazy_ptr (name, validate_stub)
     const char *name;
     int validate_stub;
{
  const char *real_name;
  tree temp, ident = get_identifier (name), id2;

    for (temp = (validate_stub ? machopic_stubs : machopic_non_lazy_pointers);
         temp != NULL_TREE;
         temp = TREE_CHAIN (temp))
      if (ident == TREE_PURPOSE (temp))
	{
	  /* Mark both the stub or non-lazy pointer as well as the
	     original symbol as being referenced.  */
          TREE_USED (temp) = 1;
	  if (TREE_CODE (TREE_VALUE (temp)) == IDENTIFIER_NODE)
	    TREE_SYMBOL_REFERENCED (TREE_VALUE (temp)) = 1;
	  real_name = IDENTIFIER_POINTER (TREE_VALUE (temp));
	  real_name = darwin_strip_name_encoding (real_name);
	  id2 = maybe_get_identifier (real_name);
	  if (id2)
	    TREE_SYMBOL_REFERENCED (id2) = 1;
	}
}

/* Transform ORIG, which may be any data source, to the corresponding
   source using indirections.  */

rtx
machopic_indirect_data_reference (orig, reg)
     rtx orig, reg;
{
  rtx ptr_ref = orig;

  if (! MACHOPIC_INDIRECT)
    return orig;

  /* APPLE LOCAL begin dynamic-no-pic  */
  switch (GET_CODE (orig))
    {
    case SYMBOL_REF:
      {
	const char *name;
	int defined;
	/* APPLE LOCAL weak import */
	tree sym;

	name = XSTR (orig, 0);
	defined = machopic_data_defined_p (name);

	if (defined && MACHO_DYNAMIC_NO_PIC_P ())
	  {
#if defined (TARGET_TOC)
	    emit_insn (gen_macho_high (reg, orig));  
	    emit_insn (gen_macho_low (reg, reg, orig));
#else /* defined (TARGET_TOC) */
#if defined (TARGET_386)
	    return orig;
#else /* defined (TARGET_386) */
	    /* some other cpu -- writeme!  */
	    abort ();
#endif /* defined (TARGET_386) */
#endif /* defined (TARGET_TOC) */
	    return reg;
	  }
	else if (defined)
	  {
#if defined (TARGET_TOC) || defined (HAVE_lo_sum)
	    rtx pic_base = gen_rtx (SYMBOL_REF, Pmode, 
				    machopic_function_base_name ());
	    rtx offset = gen_rtx (CONST, Pmode,
				  gen_rtx (MINUS, Pmode, orig, pic_base));
#endif

#if defined (TARGET_TOC) /* i.e., PowerPC */
	    rtx hi_sum_reg = reg;

	    if (reg == NULL)
	      abort ();

	    emit_insn (gen_rtx (SET, Pmode, hi_sum_reg,
				gen_rtx (PLUS, Pmode, pic_offset_table_rtx,
					 gen_rtx (HIGH, Pmode, offset))));
	    emit_insn (gen_rtx (SET, Pmode, reg,
				gen_rtx (LO_SUM, Pmode, hi_sum_reg, offset)));

	    orig = reg;
#else
#if defined (HAVE_lo_sum)
	    if (reg == 0) abort ();

	    emit_insn (gen_rtx (SET, VOIDmode, reg,
				gen_rtx (HIGH, Pmode, offset)));
	    emit_insn (gen_rtx (SET, VOIDmode, reg,
				gen_rtx (LO_SUM, Pmode, reg, offset)));
	    /* APPLE LOCAL ? */
	    emit_insn (gen_rtx (USE, VOIDmode, pic_offset_table_rtx));

	    orig = gen_rtx (PLUS, Pmode, pic_offset_table_rtx, reg);
#endif
#endif
	    return orig;
	  }

	/* APPLE LOCAL weak import */
	sym = machopic_non_lazy_ptr_list_entry (name, /*create:*/ 1);
	IDENTIFIER_WEAK_IMPORT (TREE_PURPOSE (sym)) =
	  IDENTIFIER_WEAK_IMPORT (TREE_VALUE (sym)) =
	  SYMBOL_REF_WEAK_IMPORT (orig);

	ptr_ref = gen_rtx (SYMBOL_REF, Pmode,
			   IDENTIFIER_POINTER (TREE_PURPOSE (sym)));

	ptr_ref = gen_rtx_MEM (Pmode, ptr_ref);
	RTX_UNCHANGING_P (ptr_ref) = 1;

#ifdef TARGET_386
	if (reg && TARGET_DYNAMIC_NO_PIC)
	  {
	    emit_insn (gen_rtx (SET, Pmode, reg, ptr_ref));
	    ptr_ref = reg;
	  }
#endif	/* TARGET_386 */
	return ptr_ref;
      }
      break;

    case CONST:
      {
	rtx base, result;

	/* If "(const (plus ...", walk the PLUS and return that result.
	   PLUS processing (below) will restore the "(const ..." if
	   appropriate.  */
	if (GET_CODE (XEXP (orig, 0)) == PLUS)
	  return machopic_indirect_data_reference (XEXP (orig, 0), reg);
	else 
	  return orig;
      }
      break;

    case MEM:
      {
	XEXP (ptr_ref, 0) = machopic_indirect_data_reference (XEXP (orig, 0), reg);
	return ptr_ref;
      }
      break;

    case PLUS:
      {
	rtx base, result;

	/* When the target is i386, this code prevents crashes due to the
	   compiler's ignorance on how to move the PIC base register to
	   other registers.  (The reload phase sometimes introduces such
	   insns.)  */
	if (GET_CODE (XEXP (orig, 0)) == REG
	    && REGNO (XEXP (orig, 0)) == PIC_OFFSET_TABLE_REGNUM
#ifdef TARGET_386
	    /* Prevent the same register from being erroneously used
	       as both the base and index registers.  */
	    && GET_CODE (XEXP (orig, 1)) == CONST
#endif
	    && reg)
	  {
	    emit_move_insn (reg, XEXP (orig, 0));
	    XEXP (ptr_ref, 0) = reg;
	    return ptr_ref;
	  }

	/* Legitimize both operands of the PLUS.  */
	base = machopic_indirect_data_reference (XEXP (orig, 0), reg);
	orig = machopic_indirect_data_reference (XEXP (orig, 1),
						 (base == reg ? 0 : reg));
	if (MACHOPIC_INDIRECT && GET_CODE (orig) == CONST_INT)
	  result = plus_constant (base, INTVAL (orig));
	else
	  result = gen_rtx (PLUS, Pmode, base, orig);

	if (MACHOPIC_JUST_INDIRECT && GET_CODE (base) == MEM)
	  {
	    if (reg)
	      {
		emit_move_insn (reg, result);
		result = reg;
	      }
	    else
	      result = force_reg (GET_MODE (result), result);
	  }
	return result;
      }
      break;

    default:
      break;
    }	/* End switch (GET_CODE (orig)) */
  /* APPLE LOCAL end dynamic-no-pic */
  return ptr_ref;
}

/* Transform TARGET (a MEM), which is a function call target, to the
   corresponding symbol_stub if necessary.  Return a new MEM.  */

rtx
machopic_indirect_call_target (target)
     rtx target;
{
  if (GET_CODE (target) != MEM)
    return target;

  if (MACHOPIC_INDIRECT && GET_CODE (XEXP (target, 0)) == SYMBOL_REF)
    { 
      enum machine_mode mode = GET_MODE (XEXP (target, 0));
      const char *name = XSTR (XEXP (target, 0), 0);

      /* If the name is already defined, we need do nothing.  */
      if (name[0] == '!' && name[1] == 'T')
	return target;

      if (!machopic_name_defined_p (name))
	{
	  /* APPLE LOCAL weak import */
	  tree stub = machopic_stub_list_entry (name);
	  IDENTIFIER_WEAK_IMPORT (TREE_PURPOSE (stub)) = 
	    IDENTIFIER_WEAK_IMPORT (TREE_VALUE (stub)) =
	      SYMBOL_REF_WEAK_IMPORT (XEXP (target, 0));

	  XEXP (target, 0) = gen_rtx (SYMBOL_REF, mode, 
		IDENTIFIER_POINTER (TREE_PURPOSE (stub)));
	  RTX_UNCHANGING_P (target) = 1;
	} 
    }

  return target;
}

rtx
machopic_legitimize_pic_address (orig, mode, reg)
     rtx orig, reg;
     enum machine_mode mode;
{
  rtx pic_ref = orig;

  /* APPLE LOCAL  dynamic-no-pic  */
  if (! MACHOPIC_INDIRECT)
    return orig;

  /* First handle a simple SYMBOL_REF or LABEL_REF */
  if (GET_CODE (orig) == LABEL_REF
      || (GET_CODE (orig) == SYMBOL_REF
	  ))
    {
      /* addr(foo) = &func+(foo-func) */
      rtx pic_base;

      orig = machopic_indirect_data_reference (orig, reg);

      if (GET_CODE (orig) == PLUS 
	  && GET_CODE (XEXP (orig, 0)) == REG)
	{
	  if (reg == 0)
	    return force_reg (mode, orig);

	  emit_move_insn (reg, orig);
	  return reg;
	}  

      /* APPLE LOCAL  dynamic-no-pic  */
      if (MACHO_DYNAMIC_NO_PIC_P ())
	pic_base = CONST0_RTX (Pmode);
      else
	pic_base = gen_rtx (SYMBOL_REF, Pmode, machopic_function_base_name ());

      if (GET_CODE (orig) == MEM)
	{
	  if (reg == 0)
	    {
	      if (reload_in_progress)
		abort ();
	      else
		reg = gen_reg_rtx (Pmode);
	    }
	
#ifdef HAVE_lo_sum
	  /* APPLE LOCAL  dynamic-no-pic  */
	  if (MACHO_DYNAMIC_NO_PIC_P ()
	      && (GET_CODE (XEXP (orig, 0)) == SYMBOL_REF
		  || GET_CODE (XEXP (orig, 0)) == LABEL_REF))
	    {
#if defined (TARGET_TOC)	/* ppc  */
	      rtx temp_reg = (no_new_pseudos) ? reg : gen_reg_rtx (Pmode);
	      rtx asym = XEXP (orig, 0);
	      rtx mem;

	      emit_insn (gen_macho_high (temp_reg, asym));
	      mem = gen_rtx_MEM (GET_MODE (orig),
				 gen_rtx (LO_SUM, Pmode, temp_reg, asym));
	      RTX_UNCHANGING_P (mem) = 1;
	      emit_insn (gen_rtx (SET, VOIDmode, reg, mem));
#else
	      /* Some other CPU -- WriteMe!  */
	      abort ();
#endif
	      pic_ref = reg;
	    }
	  else
	  if (GET_CODE (XEXP (orig, 0)) == SYMBOL_REF 
	      || GET_CODE (XEXP (orig, 0)) == LABEL_REF)
	    {
	      rtx offset = gen_rtx (CONST, Pmode,
				    gen_rtx (MINUS, Pmode,
					     XEXP (orig, 0), pic_base));
#if defined (TARGET_TOC) /* i.e., PowerPC */
	      /* Generating a new reg may expose opportunities for
		 common subexpression elimination.  */
              rtx hi_sum_reg =
		(reload_in_progress ? reg : gen_reg_rtx (SImode));

	      rtx mem;	/* dbj */
	      rtx insn; /* dbj */
	      emit_insn (gen_rtx (SET, Pmode, hi_sum_reg,
			   /* APPLE LOCAL  dynamic-no-pic  */
			   (MACHO_DYNAMIC_NO_PIC_P ())
				? gen_rtx (HIGH, Pmode, offset)
				: gen_rtx (PLUS, Pmode,
					   pic_offset_table_rtx,
					   gen_rtx (HIGH, Pmode, offset))));
	      /* APPLE LOCAL dbj */
	      mem =		  gen_rtx (MEM, GET_MODE (orig),
					   gen_rtx (LO_SUM, Pmode, 
						    hi_sum_reg, offset));
	      RTX_UNCHANGING_P (mem) = 1;
	      insn = emit_insn (gen_rtx (SET, VOIDmode, reg, mem));
	      REG_NOTES (insn) = gen_rtx_EXPR_LIST (REG_EQUAL, pic_ref, REG_NOTES (insn));
	      /* end APPLE LOCAL dbj */

	      pic_ref = reg;

#else
	      emit_insn (gen_rtx (USE, VOIDmode, pic_offset_table_rtx));

	      emit_insn (gen_rtx (SET, VOIDmode, reg,
				  gen_rtx (HIGH, Pmode, 
					   gen_rtx (CONST, Pmode, offset))));
	      emit_insn (gen_rtx (SET, VOIDmode, reg,
				  gen_rtx (LO_SUM, Pmode, reg, 
					   gen_rtx (CONST, Pmode, offset))));
	      pic_ref = gen_rtx (PLUS, Pmode,
				 pic_offset_table_rtx, reg);
#endif
	    }
	  else
#endif  /* HAVE_lo_sum */
	    {
	      rtx pic = pic_offset_table_rtx;
	      if (GET_CODE (pic) != REG)
		{
		  emit_move_insn (reg, pic);
		  pic = reg;
		}

	      pic_ref = gen_rtx (PLUS, Pmode,
				 pic, 
				 gen_rtx (CONST, Pmode, 
					  gen_rtx (MINUS, Pmode,
						   XEXP (orig, 0), 
						   pic_base)));
	    }
	  
#if !defined (TARGET_TOC)
	  emit_move_insn (reg, pic_ref);
	  pic_ref = gen_rtx (MEM, GET_MODE (orig), reg);
#endif
	  RTX_UNCHANGING_P (pic_ref) = 1;
	}
      else
	{

#ifdef HAVE_lo_sum
	  if (GET_CODE (orig) == SYMBOL_REF 
	      || GET_CODE (orig) == LABEL_REF)
	    {
	      rtx offset = gen_rtx (CONST, Pmode,
				    gen_rtx (MINUS, Pmode, orig, pic_base));
#if defined (TARGET_TOC) /* i.e., PowerPC */
              rtx hi_sum_reg;

	      if (reg == 0)
		{
		  if (reload_in_progress)
		    abort ();
		  else
		    reg = gen_reg_rtx (SImode);
		}
	
	      hi_sum_reg = reg;

	      emit_insn (gen_rtx (SET, Pmode, hi_sum_reg,
			   /* APPLE LOCAL  dynamic-no-pic  */
			   (MACHO_DYNAMIC_NO_PIC_P ())
				? gen_rtx (HIGH, Pmode, offset)
				: gen_rtx (PLUS, Pmode,
					   pic_offset_table_rtx,
					   gen_rtx (HIGH, Pmode, offset))));
	      emit_insn (gen_rtx (SET, VOIDmode, reg,
				  gen_rtx (LO_SUM, Pmode,
					   hi_sum_reg, offset)));
	      pic_ref = reg;
	      RTX_UNCHANGING_P (pic_ref) = 1;
#else
	      emit_insn (gen_rtx (SET, VOIDmode, reg,
				  gen_rtx (HIGH, Pmode, offset)));
	      emit_insn (gen_rtx (SET, VOIDmode, reg,
				  gen_rtx (LO_SUM, Pmode, reg, offset)));
	      pic_ref = gen_rtx (PLUS, Pmode,
				 pic_offset_table_rtx, reg);
	      RTX_UNCHANGING_P (pic_ref) = 1;
#endif
	    }
	  else
#endif  /*  HAVE_lo_sum  */
	    {
	      if (GET_CODE (orig) == REG)
		{
		  return orig;
		}
	      else
		{
		  rtx pic = pic_offset_table_rtx;
		  if (GET_CODE (pic) != REG)
		    {
		      emit_move_insn (reg, pic);
		      pic = reg;
		    }
#if 0
		  emit_insn (gen_rtx (USE, VOIDmode,
				      pic_offset_table_rtx));
#endif
		  pic_ref = gen_rtx (PLUS, Pmode,
				     pic,
				     gen_rtx (CONST, Pmode, 
					      gen_rtx (MINUS, Pmode,
						       orig, pic_base)));
		}
	    }
	}

      if (GET_CODE (pic_ref) != REG)
        {
          if (reg != 0)
            {
              emit_move_insn (reg, pic_ref);
              return reg;
            }
          else
            {
              return force_reg (mode, pic_ref);
            }
        }
      else
        {
          return pic_ref;
        }
    }

  else if (GET_CODE (orig) == SYMBOL_REF)
    return orig;

  else if (GET_CODE (orig) == PLUS
	   && (GET_CODE (XEXP (orig, 0)) == MEM
	       || GET_CODE (XEXP (orig, 0)) == SYMBOL_REF
	       || GET_CODE (XEXP (orig, 0)) == LABEL_REF)
	   && XEXP (orig, 0) != pic_offset_table_rtx
	   && GET_CODE (XEXP (orig, 1)) != REG)
    
    {
      rtx base;
      int is_complex = (GET_CODE (XEXP (orig, 0)) == MEM);

      base = machopic_legitimize_pic_address (XEXP (orig, 0), Pmode, reg);
      orig = machopic_legitimize_pic_address (XEXP (orig, 1),
					      Pmode, (base == reg ? 0 : reg));
      if (GET_CODE (orig) == CONST_INT)
	{
	  pic_ref = plus_constant (base, INTVAL (orig));
	  is_complex = 1;
	}
      /* APPLE LOCAL begin 3769675 */
      /* There are three addends present; try to construct
	 (plus (plus reg reg) const).  */
      else if (GET_CODE (orig) == PLUS
	       && GET_CODE (XEXP (orig, 0)) == REG)
	pic_ref = gen_rtx (PLUS, Pmode, gen_rtx (PLUS, Pmode, base, XEXP (orig, 0)), XEXP (orig, 1));
      /* APPLE LOCAL end 3769675 */
      else
	pic_ref = gen_rtx (PLUS, Pmode, base, orig);

      if (RTX_UNCHANGING_P (base) && RTX_UNCHANGING_P (orig))
	RTX_UNCHANGING_P (pic_ref) = 1;

      /* APPLE LOCAL begin gen ADD */
#ifdef MASK_80387
      {
	rtx mem, other;

	if (GET_CODE (orig) == MEM) {
	    mem = orig; other = base;
	    /* Swap the kids only if there is only one MEM, and it's on the right.  */
	    if (GET_CODE (base) != MEM) {
		XEXP (pic_ref, 0) = orig;
		XEXP (pic_ref, 1) = base;
	      }
	  }
	else if (GET_CODE (base) == MEM) {
	    mem = base; other = orig;
	  } else
	    mem = other = NULL_RTX;
     
	/* Both kids are MEMs.  */
	if (other && GET_CODE (other) == MEM)
	  other = force_reg (GET_MODE (other), other);

	/* The x86 can't post-index a MEM; emit an ADD instruction to handle this.  */
	if (mem && GET_CODE (mem) == MEM) {
	  if ( ! reload_in_progress) {
	    rtx set = gen_rtx_SET (VOIDmode, reg, pic_ref);
	    rtx clobber_cc = gen_rtx_CLOBBER (VOIDmode, gen_rtx_REG (CCmode, FLAGS_REG));
	    pic_ref = gen_rtx_PARALLEL (VOIDmode, gen_rtvec (2, set, clobber_cc));
	    emit_insn (pic_ref);
	    pic_ref = reg;
	    is_complex = 0;
	  }
	}
      }
#endif
      /* APPLE LOCAL end gen ADD */

      if (reg && is_complex)
	{
	  emit_move_insn (reg, pic_ref);
	  pic_ref = reg;
	}
      /* Likewise, should we set special REG_NOTEs here?  */
    }

  else if (GET_CODE (orig) == CONST)
    {
      return machopic_legitimize_pic_address (XEXP (orig, 0), Pmode, reg);
    }

  else if (GET_CODE (orig) == MEM
	   && GET_CODE (XEXP (orig, 0)) == SYMBOL_REF)
    {
      /* APPLE LOCAL use new pseudo for temp; reusing reg confuses PRE */
      rtx tempreg = reg;
      rtx addr;
      if ( !no_new_pseudos )
	tempreg = gen_reg_rtx (Pmode);
      addr = machopic_legitimize_pic_address (XEXP (orig, 0), Pmode, tempreg);

      addr = gen_rtx (MEM, GET_MODE (orig), addr);
      RTX_UNCHANGING_P (addr) = RTX_UNCHANGING_P (orig);
      emit_move_insn (reg, addr);
      pic_ref = reg;
    }

  return pic_ref;
}


void
machopic_finish (asm_out_file)
     FILE *asm_out_file;
{
  tree temp;

  for (temp = machopic_stubs;
       temp != NULL_TREE;
       temp = TREE_CHAIN (temp))
    {
      const char *sym_name = IDENTIFIER_POINTER (TREE_VALUE (temp));
      const char *stub_name = IDENTIFIER_POINTER (TREE_PURPOSE (temp));
      char *sym;
      char *stub;

      if (! TREE_USED (temp))
	continue;

      /* APPLE LOCAL remove a stub tweak */

      sym_name = darwin_strip_name_encoding (sym_name);

      sym = alloca (strlen (sym_name) + 2);
      if (sym_name[0] == '*' || sym_name[0] == '&')
	strcpy (sym, sym_name + 1);
      else if (sym_name[0] == '-'
	       || sym_name[0] == '+'
	       || sym_name[0] == '"'
	       || name_needs_quotes (sym_name))
	strcpy (sym, sym_name);	  
      else
	sym[0] = '_', strcpy (sym + 1, sym_name);

      stub = alloca (strlen (stub_name) + 2);
      if (stub_name[0] == '*' || stub_name[0] == '&')
	strcpy (stub, stub_name + 1);
      else
	stub[0] = '_', strcpy (stub + 1, stub_name);

      /* APPLE LOCAL weak import */
      if ( IDENTIFIER_WEAK_IMPORT (TREE_VALUE (temp)))
	{
	  fprintf (asm_out_file, "\t.weak_reference ");
	  assemble_name (asm_out_file, sym_name); 
	  fprintf (asm_out_file, "\n");
	}

      machopic_output_stub (asm_out_file, sym, stub);
    }

  for (temp = machopic_non_lazy_pointers;
       temp != NULL_TREE; 
       temp = TREE_CHAIN (temp))
    {
      const char *const sym_name = IDENTIFIER_POINTER (TREE_VALUE (temp));
      const char *const lazy_name = IDENTIFIER_POINTER (TREE_PURPOSE (temp));

      if (! TREE_USED (temp))
	continue;

      /* APPLE LOCAL fix-and-continue mrs  */
      if (! indirect_data (sym_name)
	  && (machopic_ident_defined_p (TREE_VALUE (temp))
	      /* APPLE LOCAL private extern */
	      || (sym_name[0] == '!' && sym_name[2] == 'p')))
	{
	  data_section ();
	  assemble_align (GET_MODE_ALIGNMENT (Pmode));
	  assemble_label (lazy_name);
	  assemble_integer (gen_rtx (SYMBOL_REF, Pmode, sym_name),
			    GET_MODE_SIZE (Pmode),
			    GET_MODE_ALIGNMENT (Pmode), 1);
	}
      else
	{
	  /* APPLE LOCAL fix-and-continue mrs  */
	  rtx init = const0_rtx;

	  /* APPLE LOCAL weak import */
	  if ( IDENTIFIER_WEAK_IMPORT (TREE_VALUE (temp)))
	    {
	      fprintf (asm_out_file, "\t.weak_reference ");
	      assemble_name (asm_out_file, sym_name); 
	      fprintf (asm_out_file, "\n");
	    }

	  machopic_nl_symbol_ptr_section ();
	  assemble_name (asm_out_file, lazy_name); 
	  fprintf (asm_out_file, ":\n");

	  fprintf (asm_out_file, "\t.indirect_symbol ");
	  assemble_name (asm_out_file, sym_name); 
	  fprintf (asm_out_file, "\n");

	  /* APPLE LOCAL BEGIN fix-and-continue mrs  */
	  if (sym_name[3] == 's'
	      && machopic_ident_defined_p (TREE_VALUE (temp)))
	    init = gen_rtx (SYMBOL_REF, Pmode, sym_name);

	  assemble_integer (init, GET_MODE_SIZE (Pmode),
			    GET_MODE_ALIGNMENT (Pmode), 1);
	  /* APPLE LOCAL END fix-and-continue mrs  */
	}
    }
}

int 
machopic_operand_p (op)
     rtx op;
{
  if (MACHOPIC_JUST_INDIRECT)
    {
      while (GET_CODE (op) == CONST)
	op = XEXP (op, 0);

      if (GET_CODE (op) == SYMBOL_REF)
	return machopic_name_defined_p (XSTR (op, 0));
      else
	return 0;
    }

  while (GET_CODE (op) == CONST)
    op = XEXP (op, 0);

  if (GET_CODE (op) == MINUS
      && GET_CODE (XEXP (op, 0)) == SYMBOL_REF
      && GET_CODE (XEXP (op, 1)) == SYMBOL_REF
      && machopic_name_defined_p (XSTR (XEXP (op, 0), 0))
      && machopic_name_defined_p (XSTR (XEXP (op, 1), 0)))
      return 1;

  return 0;
}

/* This function records whether a given name corresponds to a defined
   or undefined function or variable, for machopic_classify_ident to
   use later.  */

void
darwin_encode_section_info (decl, first)
     tree decl;
     int first ATTRIBUTE_UNUSED;
{
  char code = '\0';
  int defined = 0;
  rtx sym_ref;
  const char *orig_str;
  char *new_str;
  size_t len, new_len;

  if ((TREE_CODE (decl) == FUNCTION_DECL
       || TREE_CODE (decl) == VAR_DECL)
      && !DECL_EXTERNAL (decl)
      /* APPLE LOCAL  coalescing  */
#ifdef DECL_IS_COALESCED_OR_WEAK
      && ! DECL_IS_COALESCED_OR_WEAK (decl)
#endif
      && ((TREE_STATIC (decl)
	   && (!DECL_COMMON (decl) || !TREE_PUBLIC (decl)))
	  || (DECL_INITIAL (decl)
	      && DECL_INITIAL (decl) != error_mark_node)))
    defined = 1;
  /* APPLE LOCAL fix OBJC codegen */
  if (TREE_CODE (decl) == VAR_DECL)
    {
      sym_ref = XEXP (DECL_RTL (decl), 0);
      orig_str = XSTR (sym_ref, 0);
      if (  orig_str[0] == '_'
	 && orig_str[1] == 'O' 
	 && orig_str[2] == 'B' 
	 && orig_str[3] == 'J'
	 && orig_str[4] == 'C'
	 && orig_str[5] == '_')
	defined = 1;
    }

  if (TREE_CODE (decl) == FUNCTION_DECL)
    code = (defined ? 'T' : 't');
  else if (TREE_CODE (decl) == VAR_DECL)
    code = (defined ? 'D' : 'd');

  if (code == '\0')
    return;

  sym_ref = XEXP (DECL_RTL (decl), 0);
  orig_str = XSTR (sym_ref, 0);
  len = strlen (orig_str) + 1;

  if (orig_str[0] == '!')
    {
      /* Already encoded; see if we need to change it.  */
      if (code == orig_str[1])
	return;
      /* Yes, tweak a copy of the name and put it in a new string.  */
      new_str = alloca (len);
      memcpy (new_str, orig_str, len);
      new_str[1] = code;
      XSTR (sym_ref, 0) = ggc_alloc_string (new_str, len);
    }
  else
    {
      /* Add the encoding.  */
      new_len = len + 4;
      new_str = alloca (new_len);
      new_str[0] = '!';
      new_str[1] = code;
      new_str[2] = '_';
      /* APPLE LOCAL private extern */
      if (DECL_PRIVATE_EXTERN (decl))
	new_str[2] = 'p';
      new_str[3] = '_';
      /* APPLE LOCAL BEGIN fix-and-continue mrs  */
      if (indirect_data (orig_str)
	  && ! TREE_PUBLIC (decl))
	new_str[3] = 's';
      /* APPLE LOCAL END fix-and-continue mrs  */
      memcpy (new_str + 4, orig_str, len);
      XSTR (sym_ref, 0) = ggc_alloc_string (new_str, new_len);
    }
  /* The non-lazy pointer list may have captured references to the
     old encoded name, change them.  */
  if (TREE_CODE (decl) == VAR_DECL)
    update_non_lazy_ptrs (XSTR (sym_ref, 0));
  else
    update_stubs (XSTR (sym_ref, 0));
}

/* Undo the effects of the above.  */

const char *
darwin_strip_name_encoding (str)
     const char *str;
{
  return str[0] == '!' ? str + 4 : str;
}

/* Scan the list of non-lazy pointers and update any recorded names whose
   stripped name matches the argument.  */

static void
update_non_lazy_ptrs (name)
     const char *name;
{
  const char *name1, *name2;
  tree temp;

  name1 = darwin_strip_name_encoding (name);

  for (temp = machopic_non_lazy_pointers;
       temp != NULL_TREE; 
       temp = TREE_CHAIN (temp))
    {
      const char *sym_name = IDENTIFIER_POINTER (TREE_VALUE (temp));

      if (*sym_name == '!')
	{
	  name2 = darwin_strip_name_encoding (sym_name);
	  if (strcmp (name1, name2) == 0)
	    {
	      IDENTIFIER_POINTER (TREE_VALUE (temp)) = name;
	      break;
	    }
	}
    }
}

/* APPLE LOCAL remove machopic_output_possible_stub_label */

/* Scan the list of stubs and update any recorded names whose
   stripped name matches the argument.  */

static void
update_stubs (name)
     const char *name;
{
  const char *name1, *name2;
  tree temp;

  name1 = darwin_strip_name_encoding (name);

  for (temp = machopic_stubs;
       temp != NULL_TREE; 
       temp = TREE_CHAIN (temp))
    {
      const char *sym_name = IDENTIFIER_POINTER (TREE_VALUE (temp));

      if (*sym_name == '!')
	{
	  name2 = darwin_strip_name_encoding (sym_name);
	  if (strcmp (name1, name2) == 0)
	    {
	      IDENTIFIER_POINTER (TREE_VALUE (temp)) = name;
	      break;
	    }
	}
    }
}

void
machopic_select_section (exp, reloc, align)
     tree exp;
     int reloc;
     unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED;
{
  if (TREE_CODE (exp) == STRING_CST)
    {
      if (flag_writable_strings)
	data_section ();
      else if (TREE_STRING_LENGTH (exp) !=
	       strlen (TREE_STRING_POINTER (exp)) + 1)
	readonly_data_section ();
      else
	cstring_section ();
    }
  else if (TREE_CODE (exp) == INTEGER_CST
	   || TREE_CODE (exp) == REAL_CST)
    {
      tree size = TYPE_SIZE (TREE_TYPE (exp));

      if (TREE_CODE (size) == INTEGER_CST &&
	  TREE_INT_CST_LOW (size) == 4 &&
	  TREE_INT_CST_HIGH (size) == 0)
	literal4_section ();
      else if (TREE_CODE (size) == INTEGER_CST &&
	       TREE_INT_CST_LOW (size) == 8 &&
	       TREE_INT_CST_HIGH (size) == 0)
	literal8_section ();
      else
	readonly_data_section ();
    }
  else if (TREE_CODE (exp) == CONSTRUCTOR
	   && TREE_TYPE (exp)
	   && TREE_CODE (TREE_TYPE (exp)) == RECORD_TYPE
	   && TYPE_NAME (TREE_TYPE (exp)))
    {
      /* APPLE LOCAL begin constant strings */
      extern int flag_next_runtime;
      extern const char *constant_string_class_name;
      /* APPLE LOCAL end constant strings */
      tree name = TYPE_NAME (TREE_TYPE (exp));
      if (TREE_CODE (name) == TYPE_DECL)
	name = DECL_NAME (name);
      /* APPLE LOCAL begin constant strings */
      if (constant_string_class_name
	  && !strcmp (IDENTIFIER_POINTER (name),
		      constant_string_class_name))
	{
	  if (flag_next_runtime)
	    objc_constant_string_object_section ();
	  else
	    objc_string_object_section ();
	}
      /* APPLE LOCAL end constant strings */
      if (!strcmp (IDENTIFIER_POINTER (name), "NSConstantString"))
	objc_constant_string_object_section ();
      else if (!strcmp (IDENTIFIER_POINTER (name), "NXConstantString"))
	objc_string_object_section ();
      else if (TREE_READONLY (exp) || TREE_CONSTANT (exp))
	{
	  /* APPLE LOCAL dynamic-no-pic */
	  if (TREE_SIDE_EFFECTS (exp) || (MACHOPIC_INDIRECT && reloc))
	    const_data_section ();
	  else
	    readonly_data_section ();
	}
      else
	data_section ();
    }
  /* APPLE LOCAL begin constant cfstrings */
  else if (TREE_CODE (exp) == CONSTRUCTOR
	   && TREE_TYPE (exp)
	   && TREE_CODE (TREE_TYPE (exp)) == ARRAY_TYPE
	   && TREE_OPERAND (exp, 1))
    {
      tree name = TREE_OPERAND (exp, 1);
      if (TREE_CODE (name) == TREE_LIST && TREE_VALUE (name)
	  && TREE_CODE (TREE_VALUE (name)) == NOP_EXPR
	  && TREE_OPERAND (TREE_VALUE (name), 0)
	  && TREE_OPERAND (TREE_OPERAND (TREE_VALUE (name), 0), 0))
	name = TREE_OPERAND (TREE_OPERAND (TREE_VALUE (name), 0), 0);
      if (TREE_CODE (name) == VAR_DECL
	  && !strcmp (IDENTIFIER_POINTER (DECL_NAME (name)),
		      "__CFConstantStringClassReference"))
	cfstring_constant_object_section ();
      else if (TREE_READONLY (exp) || TREE_CONSTANT (exp))
	{
	  /* APPLE LOCAL dynamic-no-pic  */
	  if (TREE_SIDE_EFFECTS (exp) || (MACHOPIC_INDIRECT && reloc))
	    const_data_section ();
	  else
	    readonly_data_section ();
	}
      else
	data_section ();
    }
  /* APPLE LOCAL end constant cfstrings */
  else if (TREE_CODE (exp) == VAR_DECL &&
	   DECL_NAME (exp) &&
	   TREE_CODE (DECL_NAME (exp)) == IDENTIFIER_NODE &&
	   IDENTIFIER_POINTER (DECL_NAME (exp)) &&
	   !strncmp (IDENTIFIER_POINTER (DECL_NAME (exp)), "_OBJC_", 6))
    {
      const char *name = IDENTIFIER_POINTER (DECL_NAME (exp));

      if (!strncmp (name, "_OBJC_CLASS_METHODS_", 20))
	objc_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_INSTANCE_METHODS_", 23))
	objc_inst_meth_section ();
      else if (!strncmp (name, "_OBJC_CATEGORY_CLASS_METHODS_", 20))
	objc_cat_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_CATEGORY_INSTANCE_METHODS_", 23))
	objc_cat_inst_meth_section ();
      else if (!strncmp (name, "_OBJC_CLASS_VARIABLES_", 22))
	objc_class_vars_section ();
      else if (!strncmp (name, "_OBJC_INSTANCE_VARIABLES_", 25))
	objc_instance_vars_section ();
      else if (!strncmp (name, "_OBJC_CLASS_PROTOCOLS_", 22))
	objc_cat_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_CLASS_NAME_", 17))
	objc_class_names_section ();
      else if (!strncmp (name, "_OBJC_METH_VAR_NAME_", 20))
	objc_meth_var_names_section ();
      else if (!strncmp (name, "_OBJC_METH_VAR_TYPE_", 20))
	objc_meth_var_types_section ();
      else if (!strncmp (name, "_OBJC_CLASS_REFERENCES", 22))
	objc_cls_refs_section ();
      else if (!strncmp (name, "_OBJC_CLASS_", 12))
	objc_class_section ();
      else if (!strncmp (name, "_OBJC_METACLASS_", 16))
	objc_meta_class_section ();
      else if (!strncmp (name, "_OBJC_CATEGORY_", 15))
	objc_category_section ();
      else if (!strncmp (name, "_OBJC_SELECTOR_REFERENCES", 25))
	objc_selector_refs_section ();
      else if (!strncmp (name, "_OBJC_SELECTOR_FIXUP", 20))
	objc_selector_fixup_section ();
      else if (!strncmp (name, "_OBJC_SYMBOLS", 13))
	objc_symbols_section ();
      else if (!strncmp (name, "_OBJC_MODULES", 13))
	objc_module_info_section ();
      /* APPLE LOCAL begin fix and continue */
      else if (!strncmp (name, "_OBJC_IMAGE_INFO", 16))
	objc_image_info_section ();
      /* APPLE LOCAL end fix and continue */
      else if (!strncmp (name, "_OBJC_PROTOCOL_INSTANCE_METHODS_", 32))
	objc_cat_inst_meth_section ();
      else if (!strncmp (name, "_OBJC_PROTOCOL_CLASS_METHODS_", 29))
	objc_cat_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_PROTOCOL_REFS_", 20))
	objc_cat_cls_meth_section ();
      else if (!strncmp (name, "_OBJC_PROTOCOL_", 15))
	objc_protocol_section ();
      else if ((TREE_READONLY (exp) || TREE_CONSTANT (exp))
	       && !TREE_SIDE_EFFECTS (exp))
	{
	  /* APPLE LOCAL dynamic-no-pic */
	  if (MACHOPIC_INDIRECT && reloc)
	    const_data_section ();
	  else
	    readonly_data_section ();
	}
      else
	data_section ();
    }
  /* APPLE LOCAL begin darwin_set_section_for_var_p  */
  else if (darwin_set_section_for_var_p (exp, reloc, align))
    ;
  /* APPLE LOCAL end darwin_set_section_for_var_p  */
  else if (TREE_READONLY (exp) || TREE_CONSTANT (exp))
    {
      /* APPLE LOCAL dynamic-no-pic */
      if (TREE_SIDE_EFFECTS (exp) || (MACHOPIC_INDIRECT && reloc))
	const_data_section ();
      else
	readonly_data_section ();
    }
  else
    data_section ();
}

/* This can be called with address expressions as "rtx".
   They must go in "const".  */

void
machopic_select_rtx_section (mode, x, align)
     enum machine_mode mode;
     rtx x;
     unsigned HOST_WIDE_INT align ATTRIBUTE_UNUSED;
{
  if (GET_MODE_SIZE (mode) == 8)
    literal8_section ();
  else if (GET_MODE_SIZE (mode) == 4
	   && (GET_CODE (x) == CONST_INT
	       || GET_CODE (x) == CONST_DOUBLE))
    literal4_section ();
  else
    /* APPLE LOCAL begin const_data */
    /* Go in "const_data" instead (although "const" will work in some
       cases where the relocation can be resolved at static link
       time). */
    const_data_section ();
    /* APPLE LOCAL end const_data */
}

void
machopic_asm_out_constructor (symbol, priority)
     rtx symbol;
     int priority ATTRIBUTE_UNUSED;
{
  /* APPLE LOCAL  dynamic-no-pic  */
  if (MACHOPIC_INDIRECT)
    mod_init_section ();
  else
    constructor_section ();
  assemble_align (POINTER_SIZE);
  assemble_integer (symbol, POINTER_SIZE / BITS_PER_UNIT, POINTER_SIZE, 1);

  /* APPLE LOCAL  dynamic-no-pic  */
  if (! MACHOPIC_INDIRECT)
    fprintf (asm_out_file, ".reference .constructors_used\n");
}

void
machopic_asm_out_destructor (symbol, priority)
     rtx symbol;
     int priority ATTRIBUTE_UNUSED;
{
  /* APPLE LOCAL  dynamic-no-pic  */
  if (MACHOPIC_INDIRECT)
    mod_term_section ();
  else
    destructor_section ();
  assemble_align (POINTER_SIZE);
  assemble_integer (symbol, POINTER_SIZE / BITS_PER_UNIT, POINTER_SIZE, 1);

  /* APPLE LOCAL  dynamic-no-pic  */
  if (! MACHOPIC_INDIRECT)
    fprintf (asm_out_file, ".reference .destructors_used\n");
}

void
darwin_globalize_label (stream, name)
     FILE *stream;
     const char *name;
{
  if (!!strncmp (name, "_OBJC_", 6))
    default_globalize_label (stream, name);
}

/* APPLE LOCAL begin assembly "abort" directive  */
/* This can be called instead of EXIT.  It will emit a '.abort' directive
   into any existing assembly file, causing assembly to immediately abort,
   thus preventing the assembler from spewing out numerous, irrelevant
   error messages.  */

void
abort_assembly_and_exit (status)
    int status;
{
  /* If we're aborting, get the assembler to abort, too.  */
  if (status == FATAL_EXIT_CODE && asm_out_file != 0)
    fprintf (asm_out_file, "\n.abort\n");

  exit (status);
}
/* APPLE LOCAL end assembly "abort" directive  */

/* APPLE LOCAL coalescing  */
void
darwin_asm_named_section (name, flags)
     const char *name;
     unsigned int flags ATTRIBUTE_UNUSED;
{
  fprintf (asm_out_file, ".section %s\n", name);
}

unsigned int
darwin_section_type_flags (decl, name, reloc)
     tree decl;
     const char *name;
     int reloc;
{
  unsigned int flags = default_section_type_flags (decl, name, reloc);
 
  /* Weak or coalesced variables live in a writable section.  */
  if (decl != 0 && TREE_CODE (decl) != FUNCTION_DECL
      && DECL_IS_COALESCED_OR_WEAK (decl))
    flags |= SECTION_WRITE;
  
  return flags;
}              
/* APPLE LOCAL  end coalescing  */

/* APPLE LOCAL begin double destructor turly 20020214  */
#include "c-common.h"

extern int warning (const char *, ...);

/* Handle __attribute__ ((apple_kext_compatibility)).
   This only applies to darwin kexts for 295 compatibility -- it shrinks the
   vtable for classes with this attribute (and their descendants) by not
   outputting the new 3.0 nondeleting destructor.  This means that such
   objects CANNOT be allocated on the stack or as globals UNLESS they have
   a completely empty `operator delete'.
   Luckily, this fits in with the Darwin kext model.
   
   This attribute also disables gcc3's potential overlaying of derived
   class data members on the padding at the end of the base class.  */

tree
darwin_handle_odd_attribute (node, name, args, flags, no_add_attrs)
     tree *node;
     tree name;
     tree args ATTRIBUTE_UNUSED;
     int flags ATTRIBUTE_UNUSED;
     bool *no_add_attrs;
{
  if (! POSSIBLY_COMPILING_APPLE_KEXT_P ())
    {
      warning ("`%s' 2.95 vtable-compatability attribute applies "
	       "only when compiling a kext", IDENTIFIER_POINTER (name));

      *no_add_attrs = true;
    }
  else if (TREE_CODE (*node) != RECORD_TYPE)
    {
      warning ("`%s' 2.95 vtable-compatability attribute applies "
	       "only to C++ classes", IDENTIFIER_POINTER (name));

      *no_add_attrs = true;
    }

  return NULL_TREE;
}
/* APPLE LOCAL end  double destructor turly 20020214  */

/* APPLE LOCAL begin XJR */
tree
darwin_handle_objc_gc_attribute (node, name, args, flags, no_add_attrs)
     tree *node;
     tree name;
     tree args ATTRIBUTE_UNUSED;
     int flags ATTRIBUTE_UNUSED;
     bool *no_add_attrs;
{

  return NULL_TREE;
}
/* APPLE LOCAL end XJR */

/* APPLE LOCAL begin darwin_set_section_for_var_p  turly 20020226  */

/* This is specifically for any initialised static class constants
   which may be output by the C++ front end at the end of compilation. 
   SELECT_SECTION () macro won't do because these are VAR_DECLs, not
   STRING_CSTs or INTEGER_CSTs.  And by putting 'em in appropriate
   sections, we save space.  */

extern void cstring_section (void),
	    literal4_section (void), literal8_section (void);
int
darwin_set_section_for_var_p (exp, reloc, align)
     tree exp;
     int reloc;
     int align;
{
  if (!reloc && TREE_CODE (exp) == VAR_DECL
      && DECL_ALIGN (exp) == align 
      && TREE_READONLY (exp) && DECL_INITIAL (exp))
    {
      /* Put constant string vars in ".cstring" section.  */

      if (! flag_writable_strings
	  && TREE_CODE (TREE_TYPE (exp)) == ARRAY_TYPE
	  && TREE_CODE (TREE_TYPE (TREE_TYPE (exp))) == INTEGER_TYPE
	  && integer_onep (TYPE_SIZE_UNIT (TREE_TYPE (TREE_TYPE (exp))))
	  && TREE_CODE (DECL_INITIAL (exp)) == STRING_CST)
	{

	  /* Compare string length with actual number of characters
	     the compiler will write out (which is not necessarily
	     TREE_STRING_LENGTH, in the case of a constant array of
	     characters that is not null-terminated).   Select appropriate
	     section accordingly. */

	  if (MIN ( (unsigned) TREE_STRING_LENGTH (DECL_INITIAL(exp)),
		    int_size_in_bytes (TREE_TYPE (exp)))
	      == strlen (TREE_STRING_POINTER (DECL_INITIAL (exp))) + 1)
	    {
	      cstring_section ();
	      return 1;
	    }
	  else
	    {
	      const_section ();
	      return 1;
	    }
	}
     else
      if (TREE_READONLY (TREE_TYPE (exp)) 
	  && ((TREE_CODE (TREE_TYPE (exp)) == INTEGER_TYPE
	       && TREE_CODE (DECL_INITIAL (exp)) == INTEGER_CST)
	      || (TREE_CODE (TREE_TYPE (exp)) == REAL_TYPE
	  	  && TREE_CODE (DECL_INITIAL (exp)) == REAL_CST))
	  && TREE_CODE (TYPE_SIZE_UNIT (TREE_TYPE (DECL_INITIAL (exp))))
		== INTEGER_CST)
	{
	  tree size = TYPE_SIZE_UNIT (TREE_TYPE (DECL_INITIAL (exp)));
	  if (TREE_INT_CST_HIGH (size) != 0)
	    return 0;

	  /* Put integer and float consts in the literal4|8 sections.  */

	  if (TREE_INT_CST_LOW (size) == 4)
	    {
	      literal4_section ();
	      return 1;
	    }
	  else if (TREE_INT_CST_LOW (size) == 8)
	    {
	      literal8_section ();                                
	      return 1;
	    }
	}
    }
  return 0;
}
/* APPLE LOCAL end darwin_set_section_for_var_p  turly 20020226  */

/* APPLE LOCAL begin coalescing */
/* Generate a PC-relative reference to a Mach-O non-lazy-symbol.  */ 
void
darwin_non_lazy_pcrel (FILE *file, rtx addr)
{
  const char *str;
  const char *nlp_name;

  if (GET_CODE (addr) != SYMBOL_REF)
    abort ();

  str = darwin_strip_name_encoding (XSTR (addr, 0));
  nlp_name = machopic_non_lazy_ptr_name (str);
  fputs ("\t.long\t", file);
  ASM_OUTPUT_LABELREF (file, nlp_name);
  fputs ("-.", file);
}
/* APPLE LOCAL end coalescing */

/* APPLE LOCAL begin CW asm blocks */
/* Assume labels like L_foo$stub etc in CW-style inline code are
   intended to be taken as literal labels, and return the identifier,
   otherwise return NULL signifying that we have no special
   knowledge.  */
tree
darwin_cw_asm_special_label (id)
     tree id;
{
  const char *name = IDENTIFIER_POINTER (id);

  if (name[0] == 'L')
    {
      int len = strlen (name);

      if ((len > 5 && strcmp (name + len - 5, "$stub") == 0)
	  || (len > 9 && strcmp (name + len - 9, "$lazy_ptr") == 0)
	  || (len > 13 && strcmp (name + len - 13, "$non_lazy_ptr") == 0))
	return id;
    }

  return NULL_TREE;
}
/* APPLE LOCAL end CW asm blocks */

/* Output a difference of two labels that will be an assembly time
   constant if the two labels are local.  (.long lab1-lab2 will be
   very different if lab1 is at the boundary between two sections; it
   will be relocated according to the second section, not the first,
   so one ends up with a difference between labels in different
   sections, which is bad in the dwarf2 eh context for instance.)  */

static int darwin_dwarf_label_counter;

void
darwin_asm_output_dwarf_delta (file, size, lab1, lab2)
     FILE *file;
     int size ATTRIBUTE_UNUSED;
     const char *lab1, *lab2;
{
/* APPLE LOCAL begin coalescing */
  int islocaldiff = (lab1[0] == '*' && lab1[1] == 'L'
		     && lab2[0] == '*' && lab2[1] == 'L');
/* APPLE LOCAL end coalescing */

  if (islocaldiff)
    fprintf (file, "\t.set L$set$%d,", darwin_dwarf_label_counter);
  else
    fprintf (file, "\t%s\t", ".long");
  assemble_name (file, lab1);
  fprintf (file, "-");
  assemble_name (file, lab2);
  if (islocaldiff)
    fprintf (file, "\n\t.long L$set$%d", darwin_dwarf_label_counter++);
}

/* APPLE LOCAL begin deep branch prediction pic-base */
void
darwin_file_start (void)
{
#ifdef ASM_FILE_START
  ASM_FILE_START (asm_out_file);
#endif
}

void
darwin_file_end (void)
{
#ifdef ASM_FILE_END
  ASM_FILE_END (asm_out_file);
#endif
  machopic_finish (asm_out_file);
  if (strcmp (lang_hooks.name, "GNU C++") == 0)
    {
      constructor_section ();
      destructor_section ();
      ASM_OUTPUT_ALIGN (asm_out_file, 1);
    }
  /* APPLE LOCAL radar 3563020 */
  fprintf (asm_out_file, "\t.subsections_via_symbols\n");
}
/* APPLE LOCAL end deep branch prediction pic-base */

#include "gt-darwin.h"
