/* Perform type resolution on the various stuctures.
   Copyright (C) 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.
   Contributed by Andy Vaught

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
Software Foundation, 59 Temple Place - Suite 330,Boston, MA
02111-1307, USA.  */


#include "config.h"
#include "system.h"
#include "gfortran.h"
#include "arith.h"  /* For gfc_compare_expr().  */


/* Stack to push the current if we descend into a block during
   resolution.  See resolve_branch() and resolve_code().  */

typedef struct code_stack
{
  struct gfc_code *head, *current;
  struct code_stack *prev;
}
code_stack;

static code_stack *cs_base = NULL;


/* Nonzero if we're inside a FORALL block */

static int forall_flag;

/* Resolve types of formal argument lists.  These have to be done early so that
   the formal argument lists of module procedures can be copied to the
   containing module before the individual procedures are resolved
   individually.  We also resolve argument lists of procedures in interface
   blocks because they are self-contained scoping units.

   Since a dummy argument cannot be a non-dummy procedure, the only
   resort left for untyped names are the IMPLICIT types.  */

static void
resolve_formal_arglist (gfc_symbol * proc)
{
  gfc_formal_arglist *f;
  gfc_symbol *sym;
  int i;

  /* TODO: Procedures whose return character length parameter is not constant
     or assumed must also have explicit interfaces.  */
  if (proc->result != NULL)
    sym = proc->result;
  else
    sym = proc;

  if (gfc_elemental (proc)
      || sym->attr.pointer || sym->attr.allocatable
      || (sym->as && sym->as->rank > 0))
    proc->attr.always_explicit = 1;

  for (f = proc->formal; f; f = f->next)
    {
      sym = f->sym;

      if (sym == NULL)
	{
          /* Alternate return placeholder.  */
	  if (gfc_elemental (proc))
	    gfc_error ("Alternate return specifier in elemental subroutine "
		       "'%s' at %L is not allowed", proc->name,
		       &proc->declared_at);
          if (proc->attr.function)
            gfc_error ("Alternate return specifier in function "
                       "'%s' at %L is not allowed", proc->name,
                       &proc->declared_at);
	  continue;
	}

      if (sym->attr.if_source != IFSRC_UNKNOWN)
	resolve_formal_arglist (sym);

      if (sym->attr.subroutine || sym->attr.external || sym->attr.intrinsic)
	{
	  if (gfc_pure (proc) && !gfc_pure (sym))
	    {
	      gfc_error
		("Dummy procedure '%s' of PURE procedure at %L must also "
		 "be PURE", sym->name, &sym->declared_at);
	      continue;
	    }

	  if (gfc_elemental (proc))
	    {
	      gfc_error
		("Dummy procedure at %L not allowed in ELEMENTAL procedure",
		 &sym->declared_at);
	      continue;
	    }

	  continue;
	}

      if (sym->ts.type == BT_UNKNOWN)
	{
	  if (!sym->attr.function || sym->result == sym)
	    gfc_set_default_type (sym, 1, sym->ns);
	  else
	    {
              /* Set the type of the RESULT, then copy.  */
	      if (sym->result->ts.type == BT_UNKNOWN)
		gfc_set_default_type (sym->result, 1, sym->result->ns);

	      sym->ts = sym->result->ts;
	      if (sym->as == NULL)
		sym->as = gfc_copy_array_spec (sym->result->as);
	    }
	}

      gfc_resolve_array_spec (sym->as, 0);

      /* We can't tell if an array with dimension (:) is assumed or deferred
         shape until we know if it has the pointer or allocatable attributes.
      */
      if (sym->as && sym->as->rank > 0 && sym->as->type == AS_DEFERRED
          && !(sym->attr.pointer || sym->attr.allocatable))
        {
          sym->as->type = AS_ASSUMED_SHAPE;
          for (i = 0; i < sym->as->rank; i++)
            sym->as->lower[i] = gfc_int_expr (1);
        }

      if ((sym->as && sym->as->rank > 0 && sym->as->type == AS_ASSUMED_SHAPE)
          || sym->attr.pointer || sym->attr.allocatable || sym->attr.target
          || sym->attr.optional)
        proc->attr.always_explicit = 1;

      /* If the flavor is unknown at this point, it has to be a variable.
         A procedure specification would have already set the type.  */

      if (sym->attr.flavor == FL_UNKNOWN)
	gfc_add_flavor (&sym->attr, FL_VARIABLE, sym->name, &sym->declared_at);

      if (gfc_pure (proc))
	{
	  if (proc->attr.function && !sym->attr.pointer
              && sym->attr.flavor != FL_PROCEDURE
	      && sym->attr.intent != INTENT_IN)

	    gfc_error ("Argument '%s' of pure function '%s' at %L must be "
		       "INTENT(IN)", sym->name, proc->name,
		       &sym->declared_at);

	  if (proc->attr.subroutine && !sym->attr.pointer
	      && sym->attr.intent == INTENT_UNKNOWN)

	    gfc_error
	      ("Argument '%s' of pure subroutine '%s' at %L must have "
	       "its INTENT specified", sym->name, proc->name,
	       &sym->declared_at);
	}


      if (gfc_elemental (proc))
	{
	  if (sym->as != NULL)
	    {
	      gfc_error
		("Argument '%s' of elemental procedure at %L must be scalar",
		 sym->name, &sym->declared_at);
	      continue;
	    }

	  if (sym->attr.pointer)
	    {
	      gfc_error
		("Argument '%s' of elemental procedure at %L cannot have "
		 "the POINTER attribute", sym->name, &sym->declared_at);
	      continue;
	    }
	}

      /* Each dummy shall be specified to be scalar.  */
      if (proc->attr.proc == PROC_ST_FUNCTION)
        {
          if (sym->as != NULL)
            {
              gfc_error
                ("Argument '%s' of statement function at %L must be scalar",
                 sym->name, &sym->declared_at);
              continue;
            }

          if (sym->ts.type == BT_CHARACTER)
            {
              gfc_charlen *cl = sym->ts.cl;
              if (!cl || !cl->length || cl->length->expr_type != EXPR_CONSTANT)
                {
                  gfc_error
                    ("Character-valued argument '%s' of statement function at "
                     "%L must has constant length",
                     sym->name, &sym->declared_at);
                  continue;
                }
            }
        }
    }
}


/* Work function called when searching for symbols that have argument lists
   associated with them.  */

static void
find_arglists (gfc_symbol * sym)
{

  if (sym->attr.if_source == IFSRC_UNKNOWN || sym->ns != gfc_current_ns)
    return;

  resolve_formal_arglist (sym);
}


/* Given a namespace, resolve all formal argument lists within the namespace.
 */

static void
resolve_formal_arglists (gfc_namespace * ns)
{

  if (ns == NULL)
    return;

  gfc_traverse_ns (ns, find_arglists);
}


static void
resolve_contained_fntype (gfc_symbol * sym, gfc_namespace * ns)
{
  try t;
  
  /* If this namespace is not a function, ignore it.  */
  if (! sym
      || !(sym->attr.function
	   || sym->attr.flavor == FL_VARIABLE))
    return;

  /* Try to find out of what the return type is.  */
  if (sym->result != NULL)
    sym = sym->result;

  if (sym->ts.type == BT_UNKNOWN)
    {
      t = gfc_set_default_type (sym, 0, ns);

      if (t == FAILURE)
	gfc_error ("Contained function '%s' at %L has no IMPLICIT type",
		    sym->name, &sym->declared_at); /* FIXME */
    }
}


/* Add NEW_ARGS to the formal argument list of PROC, taking care not to
   introduce duplicates.  */

static void
merge_argument_lists (gfc_symbol *proc, gfc_formal_arglist *new_args)
{
  gfc_formal_arglist *f, *new_arglist;
  gfc_symbol *new_sym;

  for (; new_args != NULL; new_args = new_args->next)
    {
      new_sym = new_args->sym;
      /* See if ths arg is already in the formal argument list.  */
      for (f = proc->formal; f; f = f->next)
	{
	  if (new_sym == f->sym)
	    break;
	}

      if (f)
	continue;

      /* Add a new argument.  Argument order is not important.  */
      new_arglist = gfc_get_formal_arglist ();
      new_arglist->sym = new_sym;
      new_arglist->next = proc->formal;
      proc->formal  = new_arglist;
    }
}


/* Resolve alternate entry points.  If a symbol has multiple entry points we
   create a new master symbol for the main routine, and turn the existing
   symbol into an entry point.  */

static void
resolve_entries (gfc_namespace * ns)
{
  gfc_namespace *old_ns;
  gfc_code *c;
  gfc_symbol *proc;
  gfc_entry_list *el;
  char name[GFC_MAX_SYMBOL_LEN + 1];
  static int master_count = 0;

  if (ns->proc_name == NULL)
    return;

  /* No need to do anything if this procedure doesn't have alternate entry
     points.  */
  if (!ns->entries)
    return;

  /* We may already have resolved alternate entry points.  */
  if (ns->proc_name->attr.entry_master)
    return;

  /* If this isn't a procedure something has gone horribly wrong.  */
  gcc_assert (ns->proc_name->attr.flavor == FL_PROCEDURE);
  
  /* Remember the current namespace.  */
  old_ns = gfc_current_ns;

  gfc_current_ns = ns;

  /* Add the main entry point to the list of entry points.  */
  el = gfc_get_entry_list ();
  el->sym = ns->proc_name;
  el->id = 0;
  el->next = ns->entries;
  ns->entries = el;
  ns->proc_name->attr.entry = 1;

  /* Add an entry statement for it.  */
  c = gfc_get_code ();
  c->op = EXEC_ENTRY;
  c->ext.entry = el;
  c->next = ns->code;
  ns->code = c;

  /* Create a new symbol for the master function.  */
  /* Give the internal function a unique name (within this file).
     Also include the function name so the user has some hope of figuring
     out what is going on.  */
  snprintf (name, GFC_MAX_SYMBOL_LEN, "master.%d.%s",
	    master_count++, ns->proc_name->name);
  name[GFC_MAX_SYMBOL_LEN] = '\0';
  gfc_get_ha_symbol (name, &proc);
  gcc_assert (proc != NULL);

  gfc_add_procedure (&proc->attr, PROC_INTERNAL, proc->name, NULL);
  if (ns->proc_name->attr.subroutine)
    gfc_add_subroutine (&proc->attr, proc->name, NULL);
  else
    {
      gfc_add_function (&proc->attr, proc->name, NULL);
      gfc_internal_error ("TODO: Functions with alternate entry points");
    }
  proc->attr.access = ACCESS_PRIVATE;
  proc->attr.entry_master = 1;

  /* Merge all the entry point arguments.  */
  for (el = ns->entries; el; el = el->next)
    merge_argument_lists (proc, el->sym->formal);

  /* Use the master function for the function body.  */
  ns->proc_name = proc;

  /* Finalize the new symbols.  */
  gfc_commit_symbols ();

  /* Restore the original namespace.  */
  gfc_current_ns = old_ns;
}


/* Resolve contained function types.  Because contained functions can call one
   another, they have to be worked out before any of the contained procedures
   can be resolved.

   The good news is that if a function doesn't already have a type, the only
   way it can get one is through an IMPLICIT type or a RESULT variable, because
   by definition contained functions are contained namespace they're contained
   in, not in a sibling or parent namespace.  */

static void
resolve_contained_functions (gfc_namespace * ns)
{
  gfc_namespace *child;
  gfc_entry_list *el;

  resolve_formal_arglists (ns);

  for (child = ns->contained; child; child = child->sibling)
    {
      /* Resolve alternate entry points first.  */
      resolve_entries (child); 

      /* Then check function return types.  */
      resolve_contained_fntype (child->proc_name, child);
      for (el = child->entries; el; el = el->next)
	resolve_contained_fntype (el->sym, child);
    }
}


/* Resolve all of the elements of a structure constructor and make sure that
   the types are correct.  */

static try
resolve_structure_cons (gfc_expr * expr)
{
  gfc_constructor *cons;
  gfc_component *comp;
  try t;

  t = SUCCESS;
  cons = expr->value.constructor;
  /* A constructor may have references if it is the result of substituting a
     parameter variable.  In this case we just pull out the component we
     want.  */
  if (expr->ref)
    comp = expr->ref->u.c.sym->components;
  else
    comp = expr->ts.derived->components;

  for (; comp; comp = comp->next, cons = cons->next)
    {
      if (! cons->expr)
	{
	  t = FAILURE;
	  continue;
	}

      if (gfc_resolve_expr (cons->expr) == FAILURE)
	{
	  t = FAILURE;
	  continue;
	}

      /* If we don't have the right type, try to convert it.  */

      if (!gfc_compare_types (&cons->expr->ts, &comp->ts)
	  && gfc_convert_type (cons->expr, &comp->ts, 1) == FAILURE)
	t = FAILURE;
    }

  return t;
}



/****************** Expression name resolution ******************/

/* Returns 0 if a symbol was not declared with a type or
   attribute declaration statement, nonzero otherwise.  */

static int
was_declared (gfc_symbol * sym)
{
  symbol_attribute a;

  a = sym->attr;

  if (!a.implicit_type && sym->ts.type != BT_UNKNOWN)
    return 1;

  if (a.allocatable || a.dimension || a.dummy || a.external || a.intrinsic
      || a.optional || a.pointer || a.save || a.target
      || a.access != ACCESS_UNKNOWN || a.intent != INTENT_UNKNOWN)
    return 1;

  return 0;
}


/* Determine if a symbol is generic or not.  */

static int
generic_sym (gfc_symbol * sym)
{
  gfc_symbol *s;

  if (sym->attr.generic ||
      (sym->attr.intrinsic && gfc_generic_intrinsic (sym->name)))
    return 1;

  if (was_declared (sym) || sym->ns->parent == NULL)
    return 0;

  gfc_find_symbol (sym->name, sym->ns->parent, 1, &s);

  return (s == NULL) ? 0 : generic_sym (s);
}


/* Determine if a symbol is specific or not.  */

static int
specific_sym (gfc_symbol * sym)
{
  gfc_symbol *s;

  if (sym->attr.if_source == IFSRC_IFBODY
      || sym->attr.proc == PROC_MODULE
      || sym->attr.proc == PROC_INTERNAL
      || sym->attr.proc == PROC_ST_FUNCTION
      || (sym->attr.intrinsic &&
	  gfc_specific_intrinsic (sym->name))
      || sym->attr.external)
    return 1;

  if (was_declared (sym) || sym->ns->parent == NULL)
    return 0;

  gfc_find_symbol (sym->name, sym->ns->parent, 1, &s);

  return (s == NULL) ? 0 : specific_sym (s);
}


/* Figure out if the procedure is specific, generic or unknown.  */

typedef enum
{ PTYPE_GENERIC = 1, PTYPE_SPECIFIC, PTYPE_UNKNOWN }
proc_type;

static proc_type
procedure_kind (gfc_symbol * sym)
{

  if (generic_sym (sym))
    return PTYPE_GENERIC;

  if (specific_sym (sym))
    return PTYPE_SPECIFIC;

  return PTYPE_UNKNOWN;
}


/* Resolve an actual argument list.  Most of the time, this is just
   resolving the expressions in the list.
   The exception is that we sometimes have to decide whether arguments
   that look like procedure arguments are really simple variable
   references.  */

static try
resolve_actual_arglist (gfc_actual_arglist * arg)
{
  gfc_symbol *sym;
  gfc_symtree *parent_st;
  gfc_expr *e;

  for (; arg; arg = arg->next)
    {

      e = arg->expr;
      if (e == NULL)
        {
          /* Check the label is a valid branching target.  */
          if (arg->label)
            {
              if (arg->label->defined == ST_LABEL_UNKNOWN)
                {
                  gfc_error ("Label %d referenced at %L is never defined",
                             arg->label->value, &arg->label->where);
                  return FAILURE;
                }
            }
          continue;
        }

      if (e->ts.type != BT_PROCEDURE)
	{
	  if (gfc_resolve_expr (e) != SUCCESS)
	    return FAILURE;
	  continue;
	}

      /* See if the expression node should really be a variable
	 reference.  */

      sym = e->symtree->n.sym;

      if (sym->attr.flavor == FL_PROCEDURE
	  || sym->attr.intrinsic
	  || sym->attr.external)
	{

	  /* If the symbol is the function that names the current (or
	     parent) scope, then we really have a variable reference.  */

	  if (sym->attr.function && sym->result == sym
	      && (sym->ns->proc_name == sym
		  || (sym->ns->parent != NULL
		      && sym->ns->parent->proc_name == sym)))
	    goto got_variable;

	  continue;
	}

      /* See if the name is a module procedure in a parent unit.  */

      if (was_declared (sym) || sym->ns->parent == NULL)
	goto got_variable;

      if (gfc_find_sym_tree (sym->name, sym->ns->parent, 1, &parent_st))
	{
	  gfc_error ("Symbol '%s' at %L is ambiguous", sym->name, &e->where);
	  return FAILURE;
	}

      if (parent_st == NULL)
	goto got_variable;

      sym = parent_st->n.sym;
      e->symtree = parent_st;		/* Point to the right thing.  */

      if (sym->attr.flavor == FL_PROCEDURE
	  || sym->attr.intrinsic
	  || sym->attr.external)
	{
	  continue;
	}

    got_variable:
      e->expr_type = EXPR_VARIABLE;
      e->ts = sym->ts;
      if (sym->as != NULL)
	{
	  e->rank = sym->as->rank;
	  e->ref = gfc_get_ref ();
	  e->ref->type = REF_ARRAY;
	  e->ref->u.ar.type = AR_FULL;
	  e->ref->u.ar.as = sym->as;
	}
    }

  return SUCCESS;
}


/************* Function resolution *************/

/* Resolve a function call known to be generic.
   Section 14.1.2.4.1.  */

static match
resolve_generic_f0 (gfc_expr * expr, gfc_symbol * sym)
{
  gfc_symbol *s;

  if (sym->attr.generic)
    {
      s =
	gfc_search_interface (sym->generic, 0, &expr->value.function.actual);
      if (s != NULL)
	{
	  expr->value.function.name = s->name;
	  expr->value.function.esym = s;
	  expr->ts = s->ts;
	  if (s->as != NULL)
	    expr->rank = s->as->rank;
	  return MATCH_YES;
	}

      /* TODO: Need to search for elemental references in generic interface */
    }

  if (sym->attr.intrinsic)
    return gfc_intrinsic_func_interface (expr, 0);

  return MATCH_NO;
}


static try
resolve_generic_f (gfc_expr * expr)
{
  gfc_symbol *sym;
  match m;

  sym = expr->symtree->n.sym;

  for (;;)
    {
      m = resolve_generic_f0 (expr, sym);
      if (m == MATCH_YES)
	return SUCCESS;
      else if (m == MATCH_ERROR)
	return FAILURE;

generic:
      if (sym->ns->parent == NULL)
	break;
      gfc_find_symbol (sym->name, sym->ns->parent, 1, &sym);

      if (sym == NULL)
	break;
      if (!generic_sym (sym))
	goto generic;
    }

  /* Last ditch attempt.  */

  if (!gfc_generic_intrinsic (expr->symtree->n.sym->name))
    {
      gfc_error ("Generic function '%s' at %L is not an intrinsic function",
		 expr->symtree->n.sym->name, &expr->where);
      return FAILURE;
    }

  m = gfc_intrinsic_func_interface (expr, 0);
  if (m == MATCH_YES)
    return SUCCESS;
  if (m == MATCH_NO)
    gfc_error
      ("Generic function '%s' at %L is not consistent with a specific "
       "intrinsic interface", expr->symtree->n.sym->name, &expr->where);

  return FAILURE;
}


/* Resolve a function call known to be specific.  */

static match
resolve_specific_f0 (gfc_symbol * sym, gfc_expr * expr)
{
  match m;

  if (sym->attr.external || sym->attr.if_source == IFSRC_IFBODY)
    {
      if (sym->attr.dummy)
	{
	  sym->attr.proc = PROC_DUMMY;
	  goto found;
	}

      sym->attr.proc = PROC_EXTERNAL;
      goto found;
    }

  if (sym->attr.proc == PROC_MODULE
      || sym->attr.proc == PROC_ST_FUNCTION
      || sym->attr.proc == PROC_INTERNAL)
    goto found;

  if (sym->attr.intrinsic)
    {
      m = gfc_intrinsic_func_interface (expr, 1);
      if (m == MATCH_YES)
	return MATCH_YES;
      if (m == MATCH_NO)
	gfc_error
	  ("Function '%s' at %L is INTRINSIC but is not compatible with "
	   "an intrinsic", sym->name, &expr->where);

      return MATCH_ERROR;
    }

  return MATCH_NO;

found:
  gfc_procedure_use (sym, &expr->value.function.actual, &expr->where);

  expr->ts = sym->ts;
  expr->value.function.name = sym->name;
  expr->value.function.esym = sym;
  if (sym->as != NULL)
    expr->rank = sym->as->rank;

  return MATCH_YES;
}


static try
resolve_specific_f (gfc_expr * expr)
{
  gfc_symbol *sym;
  match m;

  sym = expr->symtree->n.sym;

  for (;;)
    {
      m = resolve_specific_f0 (sym, expr);
      if (m == MATCH_YES)
	return SUCCESS;
      if (m == MATCH_ERROR)
	return FAILURE;

      if (sym->ns->parent == NULL)
	break;

      gfc_find_symbol (sym->name, sym->ns->parent, 1, &sym);

      if (sym == NULL)
	break;
    }

  gfc_error ("Unable to resolve the specific function '%s' at %L",
	     expr->symtree->n.sym->name, &expr->where);

  return SUCCESS;
}


/* Resolve a procedure call not known to be generic nor specific.  */

static try
resolve_unknown_f (gfc_expr * expr)
{
  gfc_symbol *sym;
  gfc_typespec *ts;

  sym = expr->symtree->n.sym;

  if (sym->attr.dummy)
    {
      sym->attr.proc = PROC_DUMMY;
      expr->value.function.name = sym->name;
      goto set_type;
    }

  /* See if we have an intrinsic function reference.  */

  if (gfc_intrinsic_name (sym->name, 0))
    {
      if (gfc_intrinsic_func_interface (expr, 1) == MATCH_YES)
	return SUCCESS;
      return FAILURE;
    }

  /* The reference is to an external name.  */

  sym->attr.proc = PROC_EXTERNAL;
  expr->value.function.name = sym->name;
  expr->value.function.esym = expr->symtree->n.sym;

  if (sym->as != NULL)
    expr->rank = sym->as->rank;

  /* Type of the expression is either the type of the symbol or the
     default type of the symbol.  */

set_type:
  gfc_procedure_use (sym, &expr->value.function.actual, &expr->where);

  if (sym->ts.type != BT_UNKNOWN)
    expr->ts = sym->ts;
  else
    {
      ts = gfc_get_default_type (sym, sym->ns);

      if (ts->type == BT_UNKNOWN)
	{
	  gfc_error ("Function '%s' at %L has no implicit type",
		     sym->name, &expr->where);
	  return FAILURE;
	}
      else
	expr->ts = *ts;
    }

  return SUCCESS;
}


/* Figure out if a function reference is pure or not.  Also set the name
   of the function for a potential error message.  Return nonzero if the
   function is PURE, zero if not.  */

static int
pure_function (gfc_expr * e, const char **name)
{
  int pure;

  if (e->value.function.esym)
    {
      pure = gfc_pure (e->value.function.esym);
      *name = e->value.function.esym->name;
    }
  else if (e->value.function.isym)
    {
      pure = e->value.function.isym->pure
	|| e->value.function.isym->elemental;
      *name = e->value.function.isym->name;
    }
  else
    {
      /* Implicit functions are not pure.  */
      pure = 0;
      *name = e->value.function.name;
    }

  return pure;
}


/* Resolve a function call, which means resolving the arguments, then figuring
   out which entity the name refers to.  */
/* TODO: Check procedure arguments so that an INTENT(IN) isn't passed
   to INTENT(OUT) or INTENT(INOUT).  */

static try
resolve_function (gfc_expr * expr)
{
  gfc_actual_arglist *arg;
  const char *name;
  try t;

  if (resolve_actual_arglist (expr->value.function.actual) == FAILURE)
    return FAILURE;

/* See if function is already resolved.  */

  if (expr->value.function.name != NULL)
    {
      if (expr->ts.type == BT_UNKNOWN)
	expr->ts = expr->symtree->n.sym->ts;
      t = SUCCESS;
    }
  else
    {
      /* Apply the rules of section 14.1.2.  */

      switch (procedure_kind (expr->symtree->n.sym))
	{
	case PTYPE_GENERIC:
	  t = resolve_generic_f (expr);
	  break;

	case PTYPE_SPECIFIC:
	  t = resolve_specific_f (expr);
	  break;

	case PTYPE_UNKNOWN:
	  t = resolve_unknown_f (expr);
	  break;

	default:
	  gfc_internal_error ("resolve_function(): bad function type");
	}
    }

  /* If the expression is still a function (it might have simplified),
     then we check to see if we are calling an elemental function.  */

  if (expr->expr_type != EXPR_FUNCTION)
    return t;

  if (expr->value.function.actual != NULL
      && ((expr->value.function.esym != NULL
	   && expr->value.function.esym->attr.elemental)
	  || (expr->value.function.isym != NULL
	      && expr->value.function.isym->elemental)))
    {

      /* The rank of an elemental is the rank of its array argument(s).  */

      for (arg = expr->value.function.actual; arg; arg = arg->next)
	{
	  if (arg->expr != NULL && arg->expr->rank > 0)
	    {
	      expr->rank = arg->expr->rank;
	      break;
	    }
	}
    }

  if (!pure_function (expr, &name))
    {
      if (forall_flag)
	{
	  gfc_error
	    ("Function reference to '%s' at %L is inside a FORALL block",
	     name, &expr->where);
	  t = FAILURE;
	}
      else if (gfc_pure (NULL))
	{
	  gfc_error ("Function reference to '%s' at %L is to a non-PURE "
		     "procedure within a PURE procedure", name, &expr->where);
	  t = FAILURE;
	}
    }

  return t;
}


/************* Subroutine resolution *************/

static void
pure_subroutine (gfc_code * c, gfc_symbol * sym)
{

  if (gfc_pure (sym))
    return;

  if (forall_flag)
    gfc_error ("Subroutine call to '%s' in FORALL block at %L is not PURE",
	       sym->name, &c->loc);
  else if (gfc_pure (NULL))
    gfc_error ("Subroutine call to '%s' at %L is not PURE", sym->name,
	       &c->loc);
}


static match
resolve_generic_s0 (gfc_code * c, gfc_symbol * sym)
{
  gfc_symbol *s;

  if (sym->attr.generic)
    {
      s = gfc_search_interface (sym->generic, 1, &c->ext.actual);
      if (s != NULL)
	{
          c->resolved_sym = s;
	  pure_subroutine (c, s);
	  return MATCH_YES;
	}

      /* TODO: Need to search for elemental references in generic interface.  */
    }

  if (sym->attr.intrinsic)
    return gfc_intrinsic_sub_interface (c, 0);

  return MATCH_NO;
}


static try
resolve_generic_s (gfc_code * c)
{
  gfc_symbol *sym;
  match m;

  sym = c->symtree->n.sym;

  m = resolve_generic_s0 (c, sym);
  if (m == MATCH_YES)
    return SUCCESS;
  if (m == MATCH_ERROR)
    return FAILURE;

  if (sym->ns->parent != NULL)
    {
      gfc_find_symbol (sym->name, sym->ns->parent, 1, &sym);
      if (sym != NULL)
	{
	  m = resolve_generic_s0 (c, sym);
	  if (m == MATCH_YES)
	    return SUCCESS;
	  if (m == MATCH_ERROR)
	    return FAILURE;
	}
    }

  /* Last ditch attempt.  */

  if (!gfc_generic_intrinsic (sym->name))
    {
      gfc_error
	("Generic subroutine '%s' at %L is not an intrinsic subroutine",
	 sym->name, &c->loc);
      return FAILURE;
    }

  m = gfc_intrinsic_sub_interface (c, 0);
  if (m == MATCH_YES)
    return SUCCESS;
  if (m == MATCH_NO)
    gfc_error ("Generic subroutine '%s' at %L is not consistent with an "
	       "intrinsic subroutine interface", sym->name, &c->loc);

  return FAILURE;
}


/* Resolve a subroutine call known to be specific.  */

static match
resolve_specific_s0 (gfc_code * c, gfc_symbol * sym)
{
  match m;

  if (sym->attr.external || sym->attr.if_source == IFSRC_IFBODY)
    {
      if (sym->attr.dummy)
	{
	  sym->attr.proc = PROC_DUMMY;
	  goto found;
	}

      sym->attr.proc = PROC_EXTERNAL;
      goto found;
    }

  if (sym->attr.proc == PROC_MODULE || sym->attr.proc == PROC_INTERNAL)
    goto found;

  if (sym->attr.intrinsic)
    {
      m = gfc_intrinsic_sub_interface (c, 1);
      if (m == MATCH_YES)
	return MATCH_YES;
      if (m == MATCH_NO)
	gfc_error ("Subroutine '%s' at %L is INTRINSIC but is not compatible "
		   "with an intrinsic", sym->name, &c->loc);

      return MATCH_ERROR;
    }

  return MATCH_NO;

found:
  gfc_procedure_use (sym, &c->ext.actual, &c->loc);

  c->resolved_sym = sym;
  pure_subroutine (c, sym);

  return MATCH_YES;
}


static try
resolve_specific_s (gfc_code * c)
{
  gfc_symbol *sym;
  match m;

  sym = c->symtree->n.sym;

  m = resolve_specific_s0 (c, sym);
  if (m == MATCH_YES)
    return SUCCESS;
  if (m == MATCH_ERROR)
    return FAILURE;

  gfc_find_symbol (sym->name, sym->ns->parent, 1, &sym);

  if (sym != NULL)
    {
      m = resolve_specific_s0 (c, sym);
      if (m == MATCH_YES)
	return SUCCESS;
      if (m == MATCH_ERROR)
	return FAILURE;
    }

  gfc_error ("Unable to resolve the specific subroutine '%s' at %L",
	     sym->name, &c->loc);

  return FAILURE;
}


/* Resolve a subroutine call not known to be generic nor specific.  */

static try
resolve_unknown_s (gfc_code * c)
{
  gfc_symbol *sym;

  sym = c->symtree->n.sym;

  if (sym->attr.dummy)
    {
      sym->attr.proc = PROC_DUMMY;
      goto found;
    }

  /* See if we have an intrinsic function reference.  */

  if (gfc_intrinsic_name (sym->name, 1))
    {
      if (gfc_intrinsic_sub_interface (c, 1) == MATCH_YES)
	return SUCCESS;
      return FAILURE;
    }

  /* The reference is to an external name.  */

found:
  gfc_procedure_use (sym, &c->ext.actual, &c->loc);

  c->resolved_sym = sym;

  pure_subroutine (c, sym);

  return SUCCESS;
}


/* Resolve a subroutine call.  Although it was tempting to use the same code
   for functions, subroutines and functions are stored differently and this
   makes things awkward.  */

static try
resolve_call (gfc_code * c)
{
  try t;

  if (resolve_actual_arglist (c->ext.actual) == FAILURE)
    return FAILURE;

  if (c->resolved_sym != NULL)
    return SUCCESS;

  switch (procedure_kind (c->symtree->n.sym))
    {
    case PTYPE_GENERIC:
      t = resolve_generic_s (c);
      break;

    case PTYPE_SPECIFIC:
      t = resolve_specific_s (c);
      break;

    case PTYPE_UNKNOWN:
      t = resolve_unknown_s (c);
      break;

    default:
      gfc_internal_error ("resolve_subroutine(): bad function type");
    }

  return t;
}

/* Compare the shapes of two arrays that have non-NULL shapes.  If both
   op1->shape and op2->shape are non-NULL return SUCCESS if their shapes
   match.  If both op1->shape and op2->shape are non-NULL return FAILURE
   if their shapes do not match.  If either op1->shape or op2->shape is
   NULL, return SUCCESS.  */

static try
compare_shapes (gfc_expr * op1, gfc_expr * op2)
{
  try t;
  int i;

  t = SUCCESS;
		  
  if (op1->shape != NULL && op2->shape != NULL)
    {
      for (i = 0; i < op1->rank; i++)
	{
	  if (mpz_cmp (op1->shape[i], op2->shape[i]) != 0)
	   {
	     gfc_error ("Shapes for operands at %L and %L are not conformable",
			 &op1->where, &op2->where);
	     t = FAILURE;
	     break;
	   }
	}
    }

  return t;
}

/* Resolve an operator expression node.  This can involve replacing the
   operation with a user defined function call.  */

static try
resolve_operator (gfc_expr * e)
{
  gfc_expr *op1, *op2;
  char msg[200];
  try t;

  /* Resolve all subnodes-- give them types.  */

  switch (e->value.op.operator)
    {
    default:
      if (gfc_resolve_expr (e->value.op.op2) == FAILURE)
	return FAILURE;

    /* Fall through...  */

    case INTRINSIC_NOT:
    case INTRINSIC_UPLUS:
    case INTRINSIC_UMINUS:
      if (gfc_resolve_expr (e->value.op.op1) == FAILURE)
	return FAILURE;
      break;
    }

  /* Typecheck the new node.  */

  op1 = e->value.op.op1;
  op2 = e->value.op.op2;

  switch (e->value.op.operator)
    {
    case INTRINSIC_UPLUS:
    case INTRINSIC_UMINUS:
      if (op1->ts.type == BT_INTEGER
	  || op1->ts.type == BT_REAL
	  || op1->ts.type == BT_COMPLEX)
	{
	  e->ts = op1->ts;
	  break;
	}

      sprintf (msg, "Operand of unary numeric operator '%s' at %%L is %s",
	       gfc_op2string (e->value.op.operator), gfc_typename (&e->ts));
      goto bad_op;

    case INTRINSIC_PLUS:
    case INTRINSIC_MINUS:
    case INTRINSIC_TIMES:
    case INTRINSIC_DIVIDE:
    case INTRINSIC_POWER:
      if (gfc_numeric_ts (&op1->ts) && gfc_numeric_ts (&op2->ts))
	{
	  gfc_type_convert_binary (e);
	  break;
	}

      sprintf (msg,
	       "Operands of binary numeric operator '%s' at %%L are %s/%s",
	       gfc_op2string (e->value.op.operator), gfc_typename (&op1->ts),
	       gfc_typename (&op2->ts));
      goto bad_op;

    case INTRINSIC_CONCAT:
      if (op1->ts.type == BT_CHARACTER && op2->ts.type == BT_CHARACTER)
	{
	  e->ts.type = BT_CHARACTER;
	  e->ts.kind = op1->ts.kind;
	  break;
	}

      sprintf (msg,
	       "Operands of string concatenation operator at %%L are %s/%s",
	       gfc_typename (&op1->ts), gfc_typename (&op2->ts));
      goto bad_op;

    case INTRINSIC_AND:
    case INTRINSIC_OR:
    case INTRINSIC_EQV:
    case INTRINSIC_NEQV:
      if (op1->ts.type == BT_LOGICAL && op2->ts.type == BT_LOGICAL)
	{
	  e->ts.type = BT_LOGICAL;
	  e->ts.kind = gfc_kind_max (op1, op2);
          if (op1->ts.kind < e->ts.kind)
            gfc_convert_type (op1, &e->ts, 2);
          else if (op2->ts.kind < e->ts.kind)
            gfc_convert_type (op2, &e->ts, 2);
	  break;
	}

      sprintf (msg, "Operands of logical operator '%s' at %%L are %s/%s",
	       gfc_op2string (e->value.op.operator), gfc_typename (&op1->ts),
	       gfc_typename (&op2->ts));

      goto bad_op;

    case INTRINSIC_NOT:
      if (op1->ts.type == BT_LOGICAL)
	{
	  e->ts.type = BT_LOGICAL;
	  e->ts.kind = op1->ts.kind;
	  break;
	}

      sprintf (msg, "Operand of .NOT. operator at %%L is %s",
	       gfc_typename (&op1->ts));
      goto bad_op;

    case INTRINSIC_GT:
    case INTRINSIC_GE:
    case INTRINSIC_LT:
    case INTRINSIC_LE:
      if (op1->ts.type == BT_COMPLEX || op2->ts.type == BT_COMPLEX)
	{
	  strcpy (msg, "COMPLEX quantities cannot be compared at %L");
	  goto bad_op;
	}

      /* Fall through...  */

    case INTRINSIC_EQ:
    case INTRINSIC_NE:
      if (op1->ts.type == BT_CHARACTER && op2->ts.type == BT_CHARACTER)
	{
	  e->ts.type = BT_LOGICAL;
	  e->ts.kind = gfc_default_logical_kind;
	  break;
	}

      if (gfc_numeric_ts (&op1->ts) && gfc_numeric_ts (&op2->ts))
	{
	  gfc_type_convert_binary (e);

	  e->ts.type = BT_LOGICAL;
	  e->ts.kind = gfc_default_logical_kind;
	  break;
	}

      sprintf (msg, "Operands of comparison operator '%s' at %%L are %s/%s",
	       gfc_op2string (e->value.op.operator), gfc_typename (&op1->ts),
	       gfc_typename (&op2->ts));

      goto bad_op;

    case INTRINSIC_USER:
      if (op2 == NULL)
	sprintf (msg, "Operand of user operator '%s' at %%L is %s",
		 e->value.op.uop->name, gfc_typename (&op1->ts));
      else
	sprintf (msg, "Operands of user operator '%s' at %%L are %s/%s",
		 e->value.op.uop->name, gfc_typename (&op1->ts),
		 gfc_typename (&op2->ts));

      goto bad_op;

    default:
      gfc_internal_error ("resolve_operator(): Bad intrinsic");
    }

  /* Deal with arrayness of an operand through an operator.  */

  t = SUCCESS;

  switch (e->value.op.operator)
    {
    case INTRINSIC_PLUS:
    case INTRINSIC_MINUS:
    case INTRINSIC_TIMES:
    case INTRINSIC_DIVIDE:
    case INTRINSIC_POWER:
    case INTRINSIC_CONCAT:
    case INTRINSIC_AND:
    case INTRINSIC_OR:
    case INTRINSIC_EQV:
    case INTRINSIC_NEQV:
    case INTRINSIC_EQ:
    case INTRINSIC_NE:
    case INTRINSIC_GT:
    case INTRINSIC_GE:
    case INTRINSIC_LT:
    case INTRINSIC_LE:

      if (op1->rank == 0 && op2->rank == 0)
	e->rank = 0;

      if (op1->rank == 0 && op2->rank != 0)
	{
	  e->rank = op2->rank;

	  if (e->shape == NULL)
	    e->shape = gfc_copy_shape (op2->shape, op2->rank);
	}

      if (op1->rank != 0 && op2->rank == 0)
	{
	  e->rank = op1->rank;

	  if (e->shape == NULL)
	    e->shape = gfc_copy_shape (op1->shape, op1->rank);
	}

      if (op1->rank != 0 && op2->rank != 0)
	{
	  if (op1->rank == op2->rank)
	    {
	      e->rank = op1->rank;
	      if (e->shape == NULL)
		{
		  t = compare_shapes(op1, op2);
		  if (t == FAILURE)
		    e->shape = NULL;
		  else
		e->shape = gfc_copy_shape (op1->shape, op1->rank);
		}
	    }
	  else
	    {
	      gfc_error ("Inconsistent ranks for operator at %L and %L",
			 &op1->where, &op2->where);
	      t = FAILURE;

              /* Allow higher level expressions to work.  */
	      e->rank = 0;
	    }
	}

      break;

    case INTRINSIC_NOT:
    case INTRINSIC_UPLUS:
    case INTRINSIC_UMINUS:
      e->rank = op1->rank;

      if (e->shape == NULL)
	e->shape = gfc_copy_shape (op1->shape, op1->rank);

      /* Simply copy arrayness attribute */
      break;

    default:
      break;
    }

  /* Attempt to simplify the expression.  */
  if (t == SUCCESS)
    t = gfc_simplify_expr (e, 0);
  return t;

bad_op:

  if (gfc_extend_expr (e) == SUCCESS)
    return SUCCESS;

  gfc_error (msg, &e->where);

  return FAILURE;
}


/************** Array resolution subroutines **************/


typedef enum
{ CMP_LT, CMP_EQ, CMP_GT, CMP_UNKNOWN }
comparison;

/* Compare two integer expressions.  */

static comparison
compare_bound (gfc_expr * a, gfc_expr * b)
{
  int i;

  if (a == NULL || a->expr_type != EXPR_CONSTANT
      || b == NULL || b->expr_type != EXPR_CONSTANT)
    return CMP_UNKNOWN;

  if (a->ts.type != BT_INTEGER || b->ts.type != BT_INTEGER)
    gfc_internal_error ("compare_bound(): Bad expression");

  i = mpz_cmp (a->value.integer, b->value.integer);

  if (i < 0)
    return CMP_LT;
  if (i > 0)
    return CMP_GT;
  return CMP_EQ;
}


/* Compare an integer expression with an integer.  */

static comparison
compare_bound_int (gfc_expr * a, int b)
{
  int i;

  if (a == NULL || a->expr_type != EXPR_CONSTANT)
    return CMP_UNKNOWN;

  if (a->ts.type != BT_INTEGER)
    gfc_internal_error ("compare_bound_int(): Bad expression");

  i = mpz_cmp_si (a->value.integer, b);

  if (i < 0)
    return CMP_LT;
  if (i > 0)
    return CMP_GT;
  return CMP_EQ;
}


/* Compare a single dimension of an array reference to the array
   specification.  */

static try
check_dimension (int i, gfc_array_ref * ar, gfc_array_spec * as)
{

/* Given start, end and stride values, calculate the minimum and
   maximum referenced indexes.  */

  switch (ar->type)
    {
    case AR_FULL:
      break;

    case AR_ELEMENT:
      if (compare_bound (ar->start[i], as->lower[i]) == CMP_LT)
	goto bound;
      if (compare_bound (ar->start[i], as->upper[i]) == CMP_GT)
	goto bound;

      break;

    case AR_SECTION:
      if (compare_bound_int (ar->stride[i], 0) == CMP_EQ)
	{
	  gfc_error ("Illegal stride of zero at %L", &ar->c_where[i]);
	  return FAILURE;
	}

      if (compare_bound (ar->start[i], as->lower[i]) == CMP_LT)
	goto bound;
      if (compare_bound (ar->start[i], as->upper[i]) == CMP_GT)
	goto bound;

      /* TODO: Possibly, we could warn about end[i] being out-of-bound although
         it is legal (see 6.2.2.3.1).  */

      break;

    default:
      gfc_internal_error ("check_dimension(): Bad array reference");
    }

  return SUCCESS;

bound:
  gfc_warning ("Array reference at %L is out of bounds", &ar->c_where[i]);
  return SUCCESS;
}


/* Compare an array reference with an array specification.  */

static try
compare_spec_to_ref (gfc_array_ref * ar)
{
  gfc_array_spec *as;
  int i;

  as = ar->as;
  i = as->rank - 1;
  /* TODO: Full array sections are only allowed as actual parameters.  */
  if (as->type == AS_ASSUMED_SIZE
      && (/*ar->type == AR_FULL
          ||*/ (ar->type == AR_SECTION
              && ar->dimen_type[i] == DIMEN_RANGE && ar->end[i] == NULL)))
    {
      gfc_error ("Rightmost upper bound of assumed size array section"
                 " not specified at %L", &ar->where);
      return FAILURE;
    }

  if (ar->type == AR_FULL)
    return SUCCESS;

  if (as->rank != ar->dimen)
    {
      gfc_error ("Rank mismatch in array reference at %L (%d/%d)",
		 &ar->where, ar->dimen, as->rank);
      return FAILURE;
    }

  for (i = 0; i < as->rank; i++)
    if (check_dimension (i, ar, as) == FAILURE)
      return FAILURE;

  return SUCCESS;
}


/* Resolve one part of an array index.  */

try
gfc_resolve_index (gfc_expr * index, int check_scalar)
{
  gfc_typespec ts;

  if (index == NULL)
    return SUCCESS;

  if (gfc_resolve_expr (index) == FAILURE)
    return FAILURE;

  if (check_scalar && index->rank != 0)
    {
      gfc_error ("Array index at %L must be scalar", &index->where);
      return FAILURE;
    }

  if (index->ts.type != BT_INTEGER && index->ts.type != BT_REAL)
    {
      gfc_error ("Array index at %L must be of INTEGER type",
		 &index->where);
      return FAILURE;
    }

  if (index->ts.type == BT_REAL)
    if (gfc_notify_std (GFC_STD_GNU, "Extension: REAL array index at %L",
			&index->where) == FAILURE)
      return FAILURE;

  if (index->ts.kind != gfc_index_integer_kind
      || index->ts.type != BT_INTEGER)
    {
      ts.type = BT_INTEGER;
      ts.kind = gfc_index_integer_kind;

      gfc_convert_type_warn (index, &ts, 2, 0);
    }

  return SUCCESS;
}


/* Given an expression that contains array references, update those array
   references to point to the right array specifications.  While this is
   filled in during matching, this information is difficult to save and load
   in a module, so we take care of it here.

   The idea here is that the original array reference comes from the
   base symbol.  We traverse the list of reference structures, setting
   the stored reference to references.  Component references can
   provide an additional array specification.  */

static void
find_array_spec (gfc_expr * e)
{
  gfc_array_spec *as;
  gfc_component *c;
  gfc_ref *ref;

  as = e->symtree->n.sym->as;
  c = e->symtree->n.sym->components;

  for (ref = e->ref; ref; ref = ref->next)
    switch (ref->type)
      {
      case REF_ARRAY:
	if (as == NULL)
	  gfc_internal_error ("find_array_spec(): Missing spec");

	ref->u.ar.as = as;
	as = NULL;
	break;

      case REF_COMPONENT:
	for (; c; c = c->next)
	  if (c == ref->u.c.component)
	    break;

	if (c == NULL)
	  gfc_internal_error ("find_array_spec(): Component not found");

	if (c->dimension)
	  {
	    if (as != NULL)
	      gfc_internal_error ("find_array_spec(): unused as(1)");
	    as = c->as;
	  }

	c = c->ts.derived->components;
	break;

      case REF_SUBSTRING:
	break;
      }

  if (as != NULL)
    gfc_internal_error ("find_array_spec(): unused as(2)");
}


/* Resolve an array reference.  */

static try
resolve_array_ref (gfc_array_ref * ar)
{
  int i, check_scalar;

  for (i = 0; i < ar->dimen; i++)
    {
      check_scalar = ar->dimen_type[i] == DIMEN_RANGE;

      if (gfc_resolve_index (ar->start[i], check_scalar) == FAILURE)
	return FAILURE;
      if (gfc_resolve_index (ar->end[i], check_scalar) == FAILURE)
	return FAILURE;
      if (gfc_resolve_index (ar->stride[i], check_scalar) == FAILURE)
	return FAILURE;

      if (ar->dimen_type[i] == DIMEN_UNKNOWN)
	switch (ar->start[i]->rank)
	  {
	  case 0:
	    ar->dimen_type[i] = DIMEN_ELEMENT;
	    break;

	  case 1:
	    ar->dimen_type[i] = DIMEN_VECTOR;
	    break;

	  default:
	    gfc_error ("Array index at %L is an array of rank %d",
		       &ar->c_where[i], ar->start[i]->rank);
	    return FAILURE;
	  }
    }

  /* If the reference type is unknown, figure out what kind it is.  */

  if (ar->type == AR_UNKNOWN)
    {
      ar->type = AR_ELEMENT;
      for (i = 0; i < ar->dimen; i++)
	if (ar->dimen_type[i] == DIMEN_RANGE
	    || ar->dimen_type[i] == DIMEN_VECTOR)
	  {
	    ar->type = AR_SECTION;
	    break;
	  }
    }

  if (compare_spec_to_ref (ar) == FAILURE)
    return FAILURE;

  return SUCCESS;
}


static try
resolve_substring (gfc_ref * ref)
{

  if (ref->u.ss.start != NULL)
    {
      if (gfc_resolve_expr (ref->u.ss.start) == FAILURE)
	return FAILURE;

      if (ref->u.ss.start->ts.type != BT_INTEGER)
	{
	  gfc_error ("Substring start index at %L must be of type INTEGER",
		     &ref->u.ss.start->where);
	  return FAILURE;
	}

      if (ref->u.ss.start->rank != 0)
	{
	  gfc_error ("Substring start index at %L must be scalar",
		     &ref->u.ss.start->where);
	  return FAILURE;
	}

      if (compare_bound_int (ref->u.ss.start, 1) == CMP_LT)
	{
	  gfc_error ("Substring start index at %L is less than one",
		     &ref->u.ss.start->where);
	  return FAILURE;
	}
    }

  if (ref->u.ss.end != NULL)
    {
      if (gfc_resolve_expr (ref->u.ss.end) == FAILURE)
	return FAILURE;

      if (ref->u.ss.end->ts.type != BT_INTEGER)
	{
	  gfc_error ("Substring end index at %L must be of type INTEGER",
		     &ref->u.ss.end->where);
	  return FAILURE;
	}

      if (ref->u.ss.end->rank != 0)
	{
	  gfc_error ("Substring end index at %L must be scalar",
		     &ref->u.ss.end->where);
	  return FAILURE;
	}

      if (ref->u.ss.length != NULL
	  && compare_bound (ref->u.ss.end, ref->u.ss.length->length) == CMP_GT)
	{
	  gfc_error ("Substring end index at %L is out of bounds",
		     &ref->u.ss.start->where);
	  return FAILURE;
	}
    }

  return SUCCESS;
}


/* Resolve subtype references.  */

static try
resolve_ref (gfc_expr * expr)
{
  int current_part_dimension, n_components, seen_part_dimension;
  gfc_ref *ref;

  for (ref = expr->ref; ref; ref = ref->next)
    if (ref->type == REF_ARRAY && ref->u.ar.as == NULL)
      {
	find_array_spec (expr);
	break;
      }

  for (ref = expr->ref; ref; ref = ref->next)
    switch (ref->type)
      {
      case REF_ARRAY:
	if (resolve_array_ref (&ref->u.ar) == FAILURE)
	  return FAILURE;
	break;

      case REF_COMPONENT:
	break;

      case REF_SUBSTRING:
	resolve_substring (ref);
	break;
      }

  /* Check constraints on part references.  */

  current_part_dimension = 0;
  seen_part_dimension = 0;
  n_components = 0;

  for (ref = expr->ref; ref; ref = ref->next)
    {
      switch (ref->type)
	{
	case REF_ARRAY:
	  switch (ref->u.ar.type)
	    {
	    case AR_FULL:
	    case AR_SECTION:
	      current_part_dimension = 1;
	      break;

	    case AR_ELEMENT:
	      current_part_dimension = 0;
	      break;

	    case AR_UNKNOWN:
	      gfc_internal_error ("resolve_ref(): Bad array reference");
	    }

	  break;

	case REF_COMPONENT:
	  if ((current_part_dimension || seen_part_dimension)
	      && ref->u.c.component->pointer)
	    {
	      gfc_error
		("Component to the right of a part reference with nonzero "
		 "rank must not have the POINTER attribute at %L",
		 &expr->where);
	      return FAILURE;
	    }

	  n_components++;
	  break;

	case REF_SUBSTRING:
	  break;
	}

      if (((ref->type == REF_COMPONENT && n_components > 1)
	   || ref->next == NULL)
          && current_part_dimension
	  && seen_part_dimension)
	{

	  gfc_error ("Two or more part references with nonzero rank must "
		     "not be specified at %L", &expr->where);
	  return FAILURE;
	}

      if (ref->type == REF_COMPONENT)
	{
	  if (current_part_dimension)
	    seen_part_dimension = 1;

          /* reset to make sure */
	  current_part_dimension = 0;
	}
    }

  return SUCCESS;
}


/* Given an expression, determine its shape.  This is easier than it sounds.
   Leaves the shape array NULL if it is not possible to determine the shape.  */

static void
expression_shape (gfc_expr * e)
{
  mpz_t array[GFC_MAX_DIMENSIONS];
  int i;

  if (e->rank == 0 || e->shape != NULL)
    return;

  for (i = 0; i < e->rank; i++)
    if (gfc_array_dimen_size (e, i, &array[i]) == FAILURE)
      goto fail;

  e->shape = gfc_get_shape (e->rank);

  memcpy (e->shape, array, e->rank * sizeof (mpz_t));

  return;

fail:
  for (i--; i >= 0; i--)
    mpz_clear (array[i]);
}


/* Given a variable expression node, compute the rank of the expression by
   examining the base symbol and any reference structures it may have.  */

static void
expression_rank (gfc_expr * e)
{
  gfc_ref *ref;
  int i, rank;

  if (e->ref == NULL)
    {
      if (e->expr_type == EXPR_ARRAY)
	goto done;
      /* Constructors can have a rank different from one via RESHAPE().  */

      if (e->symtree == NULL)
	{
	  e->rank = 0;
	  goto done;
	}

      e->rank = (e->symtree->n.sym->as == NULL)
                  ? 0 : e->symtree->n.sym->as->rank;
      goto done;
    }

  rank = 0;

  for (ref = e->ref; ref; ref = ref->next)
    {
      if (ref->type != REF_ARRAY)
	continue;

      if (ref->u.ar.type == AR_FULL)
	{
	  rank = ref->u.ar.as->rank;
	  break;
	}

      if (ref->u.ar.type == AR_SECTION)
	{
          /* Figure out the rank of the section.  */
	  if (rank != 0)
	    gfc_internal_error ("expression_rank(): Two array specs");

	  for (i = 0; i < ref->u.ar.dimen; i++)
	    if (ref->u.ar.dimen_type[i] == DIMEN_RANGE
		|| ref->u.ar.dimen_type[i] == DIMEN_VECTOR)
	      rank++;

	  break;
	}
    }

  e->rank = rank;

done:
  expression_shape (e);
}


/* Resolve a variable expression.  */

static try
resolve_variable (gfc_expr * e)
{
  gfc_symbol *sym;

  if (e->ref && resolve_ref (e) == FAILURE)
    return FAILURE;

  sym = e->symtree->n.sym;
  if (sym->attr.flavor == FL_PROCEDURE && !sym->attr.function)
    {
      e->ts.type = BT_PROCEDURE;
      return SUCCESS;
    }

  if (sym->ts.type != BT_UNKNOWN)
    gfc_variable_attr (e, &e->ts);
  else
    {
      /* Must be a simple variable reference.  */
      if (gfc_set_default_type (sym, 1, NULL) == FAILURE)
	return FAILURE;
      e->ts = sym->ts;
    }

  return SUCCESS;
}


/* Resolve an expression.  That is, make sure that types of operands agree
   with their operators, intrinsic operators are converted to function calls
   for overloaded types and unresolved function references are resolved.  */

try
gfc_resolve_expr (gfc_expr * e)
{
  try t;

  if (e == NULL)
    return SUCCESS;

  switch (e->expr_type)
    {
    case EXPR_OP:
      t = resolve_operator (e);
      break;

    case EXPR_FUNCTION:
      t = resolve_function (e);
      break;

    case EXPR_VARIABLE:
      t = resolve_variable (e);
      if (t == SUCCESS)
	expression_rank (e);
      break;

    case EXPR_SUBSTRING:
      t = resolve_ref (e);
      break;

    case EXPR_CONSTANT:
    case EXPR_NULL:
      t = SUCCESS;
      break;

    case EXPR_ARRAY:
      t = FAILURE;
      if (resolve_ref (e) == FAILURE)
	break;

      t = gfc_resolve_array_constructor (e);
      /* Also try to expand a constructor.  */
      if (t == SUCCESS)
	{
	  expression_rank (e);
	  gfc_expand_constructor (e);
	}

      break;

    case EXPR_STRUCTURE:
      t = resolve_ref (e);
      if (t == FAILURE)
	break;

      t = resolve_structure_cons (e);
      if (t == FAILURE)
	break;

      t = gfc_simplify_expr (e, 0);
      break;

    default:
      gfc_internal_error ("gfc_resolve_expr(): Bad expression type");
    }

  return t;
}


/* Resolve an expression from an iterator.  They must be scalar and have
   INTEGER or (optionally) REAL type.  */

static try
gfc_resolve_iterator_expr (gfc_expr * expr, bool real_ok, const char * name)
{
  if (gfc_resolve_expr (expr) == FAILURE)
    return FAILURE;

  if (expr->rank != 0)
    {
      gfc_error ("%s at %L must be a scalar", name, &expr->where);
      return FAILURE;
    }

  if (!(expr->ts.type == BT_INTEGER
	|| (expr->ts.type == BT_REAL && real_ok)))
    {
      gfc_error ("%s at %L must be INTEGER%s",
		 name,
		 &expr->where,
		 real_ok ? " or REAL" : "");
      return FAILURE;
    }
  return SUCCESS;
}


/* Resolve the expressions in an iterator structure.  If REAL_OK is
   false allow only INTEGER type iterators, otherwise allow REAL types.  */

try
gfc_resolve_iterator (gfc_iterator * iter, bool real_ok)
{

  if (iter->var->ts.type == BT_REAL)
    gfc_notify_std (GFC_STD_F95_DEL,
		    "Obsolete: REAL DO loop iterator at %L",
		    &iter->var->where);

  if (gfc_resolve_iterator_expr (iter->var, real_ok, "Loop variable")
      == FAILURE)
    return FAILURE;

  if (gfc_pure (NULL) && gfc_impure_variable (iter->var->symtree->n.sym))
    {
      gfc_error ("Cannot assign to loop variable in PURE procedure at %L",
		 &iter->var->where);
      return FAILURE;
    }

  if (gfc_resolve_iterator_expr (iter->start, real_ok,
				 "Start expression in DO loop") == FAILURE)
    return FAILURE;

  if (gfc_resolve_iterator_expr (iter->end, real_ok,
				 "End expression in DO loop") == FAILURE)
    return FAILURE;

  if (gfc_resolve_iterator_expr (iter->step, real_ok,
				 "Step expression in DO loop") == FAILURE)
    return FAILURE;

  if (iter->step->expr_type == EXPR_CONSTANT)
    {
      if ((iter->step->ts.type == BT_INTEGER
	   && mpz_cmp_ui (iter->step->value.integer, 0) == 0)
	  || (iter->step->ts.type == BT_REAL
	      && mpfr_sgn (iter->step->value.real) == 0))
	{
	  gfc_error ("Step expression in DO loop at %L cannot be zero",
		     &iter->step->where);
	  return FAILURE;
	}
    }

  /* Convert start, end, and step to the same type as var.  */
  if (iter->start->ts.kind != iter->var->ts.kind
      || iter->start->ts.type != iter->var->ts.type)
    gfc_convert_type (iter->start, &iter->var->ts, 2);

  if (iter->end->ts.kind != iter->var->ts.kind
      || iter->end->ts.type != iter->var->ts.type)
    gfc_convert_type (iter->end, &iter->var->ts, 2);

  if (iter->step->ts.kind != iter->var->ts.kind
      || iter->step->ts.type != iter->var->ts.type)
    gfc_convert_type (iter->step, &iter->var->ts, 2);

  return SUCCESS;
}


/* Resolve a list of FORALL iterators.  */

static void
resolve_forall_iterators (gfc_forall_iterator * iter)
{

  while (iter)
    {
      if (gfc_resolve_expr (iter->var) == SUCCESS
	  && iter->var->ts.type != BT_INTEGER)
	gfc_error ("FORALL Iteration variable at %L must be INTEGER",
		   &iter->var->where);

      if (gfc_resolve_expr (iter->start) == SUCCESS
	  && iter->start->ts.type != BT_INTEGER)
	gfc_error ("FORALL start expression at %L must be INTEGER",
		   &iter->start->where);
      if (iter->var->ts.kind != iter->start->ts.kind)
	gfc_convert_type (iter->start, &iter->var->ts, 2);

      if (gfc_resolve_expr (iter->end) == SUCCESS
	  && iter->end->ts.type != BT_INTEGER)
	gfc_error ("FORALL end expression at %L must be INTEGER",
		   &iter->end->where);
      if (iter->var->ts.kind != iter->end->ts.kind)
	gfc_convert_type (iter->end, &iter->var->ts, 2);

      if (gfc_resolve_expr (iter->stride) == SUCCESS
	  && iter->stride->ts.type != BT_INTEGER)
	gfc_error ("FORALL Stride expression at %L must be INTEGER",
		   &iter->stride->where);
      if (iter->var->ts.kind != iter->stride->ts.kind)
	gfc_convert_type (iter->stride, &iter->var->ts, 2);

      iter = iter->next;
    }
}


/* Given a pointer to a symbol that is a derived type, see if any components
   have the POINTER attribute.  The search is recursive if necessary.
   Returns zero if no pointer components are found, nonzero otherwise.  */

static int
derived_pointer (gfc_symbol * sym)
{
  gfc_component *c;

  for (c = sym->components; c; c = c->next)
    {
      if (c->pointer)
	return 1;

      if (c->ts.type == BT_DERIVED && derived_pointer (c->ts.derived))
	return 1;
    }

  return 0;
}


/* Resolve the argument of a deallocate expression.  The expression must be
   a pointer or a full array.  */

static try
resolve_deallocate_expr (gfc_expr * e)
{
  symbol_attribute attr;
  int allocatable;
  gfc_ref *ref;

  if (gfc_resolve_expr (e) == FAILURE)
    return FAILURE;

  attr = gfc_expr_attr (e);
  if (attr.pointer)
    return SUCCESS;

  if (e->expr_type != EXPR_VARIABLE)
    goto bad;

  allocatable = e->symtree->n.sym->attr.allocatable;
  for (ref = e->ref; ref; ref = ref->next)
    switch (ref->type)
      {
      case REF_ARRAY:
	if (ref->u.ar.type != AR_FULL)
	  allocatable = 0;
	break;

      case REF_COMPONENT:
	allocatable = (ref->u.c.component->as != NULL
		       && ref->u.c.component->as->type == AS_DEFERRED);
	break;

      case REF_SUBSTRING:
	allocatable = 0;
	break;
      }

  if (allocatable == 0)
    {
    bad:
      gfc_error ("Expression in DEALLOCATE statement at %L must be "
		 "ALLOCATABLE or a POINTER", &e->where);
    }

  return SUCCESS;
}


/* Resolve the expression in an ALLOCATE statement, doing the additional
   checks to see whether the expression is OK or not.  The expression must
   have a trailing array reference that gives the size of the array.  */

static try
resolve_allocate_expr (gfc_expr * e)
{
  int i, pointer, allocatable, dimension;
  symbol_attribute attr;
  gfc_ref *ref, *ref2;
  gfc_array_ref *ar;

  if (gfc_resolve_expr (e) == FAILURE)
    return FAILURE;

  /* Make sure the expression is allocatable or a pointer.  If it is
     pointer, the next-to-last reference must be a pointer.  */

  ref2 = NULL;

  if (e->expr_type != EXPR_VARIABLE)
    {
      allocatable = 0;

      attr = gfc_expr_attr (e);
      pointer = attr.pointer;
      dimension = attr.dimension;

    }
  else
    {
      allocatable = e->symtree->n.sym->attr.allocatable;
      pointer = e->symtree->n.sym->attr.pointer;
      dimension = e->symtree->n.sym->attr.dimension;

      for (ref = e->ref; ref; ref2 = ref, ref = ref->next)
	switch (ref->type)
	  {
	  case REF_ARRAY:
	    if (ref->next != NULL)
	      pointer = 0;
	    break;

	  case REF_COMPONENT:
	    allocatable = (ref->u.c.component->as != NULL
			   && ref->u.c.component->as->type == AS_DEFERRED);

	    pointer = ref->u.c.component->pointer;
	    dimension = ref->u.c.component->dimension;
	    break;

	  case REF_SUBSTRING:
	    allocatable = 0;
	    pointer = 0;
	    break;
	  }
    }

  if (allocatable == 0 && pointer == 0)
    {
      gfc_error ("Expression in ALLOCATE statement at %L must be "
		 "ALLOCATABLE or a POINTER", &e->where);
      return FAILURE;
    }

  if (pointer && dimension == 0)
    return SUCCESS;

  /* Make sure the next-to-last reference node is an array specification.  */

  if (ref2 == NULL || ref2->type != REF_ARRAY || ref2->u.ar.type == AR_FULL)
    {
      gfc_error ("Array specification required in ALLOCATE statement "
		 "at %L", &e->where);
      return FAILURE;
    }

  if (ref2->u.ar.type == AR_ELEMENT)
    return SUCCESS;

  /* Make sure that the array section reference makes sense in the
    context of an ALLOCATE specification.  */

  ar = &ref2->u.ar;

  for (i = 0; i < ar->dimen; i++)
    switch (ar->dimen_type[i])
      {
      case DIMEN_ELEMENT:
	break;

      case DIMEN_RANGE:
	if (ar->start[i] != NULL
	    && ar->end[i] != NULL
	    && ar->stride[i] == NULL)
	  break;

	/* Fall Through...  */

      case DIMEN_UNKNOWN:
      case DIMEN_VECTOR:
	gfc_error ("Bad array specification in ALLOCATE statement at %L",
		   &e->where);
	return FAILURE;
      }

  return SUCCESS;
}


/************ SELECT CASE resolution subroutines ************/

/* Callback function for our mergesort variant.  Determines interval
   overlaps for CASEs. Return <0 if op1 < op2, 0 for overlap, >0 for
   op1 > op2.  Assumes we're not dealing with the default case.  
   We have op1 = (:L), (K:L) or (K:) and op2 = (:N), (M:N) or (M:).
   There are nine situations to check.  */

static int
compare_cases (const gfc_case * op1, const gfc_case * op2)
{
  int retval;

  if (op1->low == NULL) /* op1 = (:L)  */
    {
      /* op2 = (:N), so overlap.  */
      retval = 0;
      /* op2 = (M:) or (M:N),  L < M  */
      if (op2->low != NULL
	  && gfc_compare_expr (op1->high, op2->low) < 0)
	retval = -1;
    }
  else if (op1->high == NULL) /* op1 = (K:)  */
    {
      /* op2 = (M:), so overlap.  */
      retval = 0;
      /* op2 = (:N) or (M:N), K > N  */
      if (op2->high != NULL
	  && gfc_compare_expr (op1->low, op2->high) > 0)
	retval = 1;
    }
  else /* op1 = (K:L)  */
    {
      if (op2->low == NULL)       /* op2 = (:N), K > N  */
	retval = (gfc_compare_expr (op1->low, op2->high) > 0) ? 1 : 0;
      else if (op2->high == NULL) /* op2 = (M:), L < M  */
	retval = (gfc_compare_expr (op1->high, op2->low) < 0) ? -1 : 0;
      else                        /* op2 = (M:N)  */
        {
	  retval =  0;
          /* L < M  */
	  if (gfc_compare_expr (op1->high, op2->low) < 0)
	    retval =  -1;
          /* K > N  */
	  else if (gfc_compare_expr (op1->low, op2->high) > 0)
	    retval =  1;
	}
    }

  return retval;
}


/* Merge-sort a double linked case list, detecting overlap in the
   process.  LIST is the head of the double linked case list before it
   is sorted.  Returns the head of the sorted list if we don't see any
   overlap, or NULL otherwise.  */

static gfc_case *
check_case_overlap (gfc_case * list)
{
  gfc_case *p, *q, *e, *tail;
  int insize, nmerges, psize, qsize, cmp, overlap_seen;

  /* If the passed list was empty, return immediately.  */
  if (!list)
    return NULL;

  overlap_seen = 0;
  insize = 1;

  /* Loop unconditionally.  The only exit from this loop is a return
     statement, when we've finished sorting the case list.  */
  for (;;)
    {
      p = list;
      list = NULL;
      tail = NULL;

      /* Count the number of merges we do in this pass.  */
      nmerges = 0;

      /* Loop while there exists a merge to be done.  */
      while (p)
	{
	  int i;

	  /* Count this merge.  */
	  nmerges++;

	  /* Cut the list in two pieces by stepping INSIZE places
             forward in the list, starting from P.  */
	  psize = 0;
	  q = p;
	  for (i = 0; i < insize; i++)
	    {
	      psize++;
	      q = q->right;
	      if (!q)
		break;
	    }
	  qsize = insize;

	  /* Now we have two lists.  Merge them!  */
	  while (psize > 0 || (qsize > 0 && q != NULL))
	    {

	      /* See from which the next case to merge comes from.  */
	      if (psize == 0)
		{
		  /* P is empty so the next case must come from Q.  */
		  e = q;
		  q = q->right;
		  qsize--;
		}
	      else if (qsize == 0 || q == NULL)
		{
		  /* Q is empty.  */
		  e = p;
		  p = p->right;
		  psize--;
		}
	      else
		{
		  cmp = compare_cases (p, q);
		  if (cmp < 0)
		    {
		      /* The whole case range for P is less than the
                         one for Q.  */
		      e = p;
		      p = p->right;
		      psize--;
		    }
		  else if (cmp > 0)
		    {
		      /* The whole case range for Q is greater than
                         the case range for P.  */
		      e = q;
		      q = q->right;
		      qsize--;
		    }
		  else
		    {
		      /* The cases overlap, or they are the same
			 element in the list.  Either way, we must
			 issue an error and get the next case from P.  */
		      /* FIXME: Sort P and Q by line number.  */
		      gfc_error ("CASE label at %L overlaps with CASE "
				 "label at %L", &p->where, &q->where);
		      overlap_seen = 1;
		      e = p;
		      p = p->right;
		      psize--;
		    }
		}

		/* Add the next element to the merged list.  */
	      if (tail)
		tail->right = e;
	      else
		list = e;
	      e->left = tail;
	      tail = e;
	    }

	  /* P has now stepped INSIZE places along, and so has Q.  So
             they're the same.  */
	  p = q;
	}
      tail->right = NULL;

      /* If we have done only one merge or none at all, we've
         finished sorting the cases.  */
      if (nmerges <= 1)
        {
	  if (!overlap_seen)
	    return list;
	  else
	    return NULL;
	}

      /* Otherwise repeat, merging lists twice the size.  */
      insize *= 2;
    }
}


/* Check to see if an expression is suitable for use in a CASE statement.
   Makes sure that all case expressions are scalar constants of the same
   type.  Return FAILURE if anything is wrong.  */

static try
validate_case_label_expr (gfc_expr * e, gfc_expr * case_expr)
{
  if (e == NULL) return SUCCESS;

  if (e->ts.type != case_expr->ts.type)
    {
      gfc_error ("Expression in CASE statement at %L must be of type %s",
		 &e->where, gfc_basic_typename (case_expr->ts.type));
      return FAILURE;
    }

  /* C805 (R808) For a given case-construct, each case-value shall be of
     the same type as case-expr.  For character type, length differences
     are allowed, but the kind type parameters shall be the same.  */

  if (case_expr->ts.type == BT_CHARACTER && e->ts.kind != case_expr->ts.kind)
    {
      gfc_error("Expression in CASE statement at %L must be kind %d",
                &e->where, case_expr->ts.kind);
      return FAILURE;
    }

  /* Convert the case value kind to that of case expression kind, if needed.
     FIXME:  Should a warning be issued?  */
  if (e->ts.kind != case_expr->ts.kind)
    gfc_convert_type_warn (e, &case_expr->ts, 2, 0);

  if (e->rank != 0)
    {
      gfc_error ("Expression in CASE statement at %L must be scalar",
		 &e->where);
      return FAILURE;
    }

  return SUCCESS;
}


/* Given a completely parsed select statement, we:

     - Validate all expressions and code within the SELECT.
     - Make sure that the selection expression is not of the wrong type.
     - Make sure that no case ranges overlap.
     - Eliminate unreachable cases and unreachable code resulting from
       removing case labels.

   The standard does allow unreachable cases, e.g. CASE (5:3).  But
   they are a hassle for code generation, and to prevent that, we just
   cut them out here.  This is not necessary for overlapping cases
   because they are illegal and we never even try to generate code.

   We have the additional caveat that a SELECT construct could have
   been a computed GOTO in the source code. Fortunately we can fairly
   easily work around that here: The case_expr for a "real" SELECT CASE
   is in code->expr1, but for a computed GOTO it is in code->expr2. All
   we have to do is make sure that the case_expr is a scalar integer
   expression.  */

static void
resolve_select (gfc_code * code)
{
  gfc_code *body;
  gfc_expr *case_expr;
  gfc_case *cp, *default_case, *tail, *head;
  int seen_unreachable;
  int ncases;
  bt type;
  try t;

  if (code->expr == NULL)
    {
      /* This was actually a computed GOTO statement.  */
      case_expr = code->expr2;
      if (case_expr->ts.type != BT_INTEGER
	  || case_expr->rank != 0)
	gfc_error ("Selection expression in computed GOTO statement "
		   "at %L must be a scalar integer expression",
		   &case_expr->where);

      /* Further checking is not necessary because this SELECT was built
	 by the compiler, so it should always be OK.  Just move the
	 case_expr from expr2 to expr so that we can handle computed
	 GOTOs as normal SELECTs from here on.  */
      code->expr = code->expr2;
      code->expr2 = NULL;
      return;
    }

  case_expr = code->expr;

  type = case_expr->ts.type;
  if (type != BT_LOGICAL && type != BT_INTEGER && type != BT_CHARACTER)
    {
      gfc_error ("Argument of SELECT statement at %L cannot be %s",
		 &case_expr->where, gfc_typename (&case_expr->ts));

      /* Punt. Going on here just produce more garbage error messages.  */
      return;
    }

  if (case_expr->rank != 0)
    {
      gfc_error ("Argument of SELECT statement at %L must be a scalar "
		 "expression", &case_expr->where);

      /* Punt.  */
      return;
    }

  /* PR 19168 has a long discussion concerning a mismatch of the kinds
     of the SELECT CASE expression and its CASE values.  Walk the lists
     of case values, and if we find a mismatch, promote case_expr to
     the appropriate kind.  */

  if (type == BT_LOGICAL || type == BT_INTEGER)
    {
      for (body = code->block; body; body = body->block)
	{
	  /* Walk the case label list.  */
	  for (cp = body->ext.case_list; cp; cp = cp->next)
	    {
	      /* Intercept the DEFAULT case.  It does not have a kind.  */
	      if (cp->low == NULL && cp->high == NULL)
		continue;

	      /* Unreachable case ranges are discarded, so ignore.  */	
	      if (cp->low != NULL && cp->high != NULL
		  && cp->low != cp->high
		  && gfc_compare_expr (cp->low, cp->high) > 0)
		continue;

	      /* FIXME: Should a warning be issued?  */
	      if (cp->low != NULL
		  && case_expr->ts.kind != gfc_kind_max(case_expr, cp->low))
		gfc_convert_type_warn (case_expr, &cp->low->ts, 2, 0);

	      if (cp->high != NULL
		  && case_expr->ts.kind != gfc_kind_max(case_expr, cp->high))
 		gfc_convert_type_warn (case_expr, &cp->high->ts, 2, 0);
	    }
	 }
    }

  /* Assume there is no DEFAULT case.  */
  default_case = NULL;
  head = tail = NULL;
  ncases = 0;

  for (body = code->block; body; body = body->block)
    {
      /* Assume the CASE list is OK, and all CASE labels can be matched.  */
      t = SUCCESS;
      seen_unreachable = 0;

      /* Walk the case label list, making sure that all case labels
         are legal.  */
      for (cp = body->ext.case_list; cp; cp = cp->next)
	{
	  /* Count the number of cases in the whole construct.  */
	  ncases++;

	  /* Intercept the DEFAULT case.  */
	  if (cp->low == NULL && cp->high == NULL)
	    {
	      if (default_case != NULL)
	        {
		  gfc_error ("The DEFAULT CASE at %L cannot be followed "
			     "by a second DEFAULT CASE at %L",
			     &default_case->where, &cp->where);
		  t = FAILURE;
		  break;
		}
	      else
		{
		  default_case = cp;
		  continue;
		}
	    }

	  /* Deal with single value cases and case ranges.  Errors are
             issued from the validation function.  */
	  if(validate_case_label_expr (cp->low, case_expr) != SUCCESS
	     || validate_case_label_expr (cp->high, case_expr) != SUCCESS)
	    {
	      t = FAILURE;
	      break;
	    }

	  if (type == BT_LOGICAL
	      && ((cp->low == NULL || cp->high == NULL)
		  || cp->low != cp->high))
	    {
	      gfc_error
		("Logical range in CASE statement at %L is not allowed",
		 &cp->low->where);
	      t = FAILURE;
	      break;
	    }

	  if (cp->low != NULL && cp->high != NULL
	      && cp->low != cp->high
	      && gfc_compare_expr (cp->low, cp->high) > 0)
	    {
	      if (gfc_option.warn_surprising)
	        gfc_warning ("Range specification at %L can never "
			     "be matched", &cp->where);

	      cp->unreachable = 1;
	      seen_unreachable = 1;
	    }
	  else
	    {
	      /* If the case range can be matched, it can also overlap with
		 other cases.  To make sure it does not, we put it in a
		 double linked list here.  We sort that with a merge sort
		 later on to detect any overlapping cases.  */
	      if (!head)
	        {
		  head = tail = cp;
		  head->right = head->left = NULL;
		}
	      else
	        {
		  tail->right = cp;
		  tail->right->left = tail;
		  tail = tail->right;
		  tail->right = NULL;
		}
	    }
	}

      /* It there was a failure in the previous case label, give up
	 for this case label list.  Continue with the next block.  */
      if (t == FAILURE)
	continue;

      /* See if any case labels that are unreachable have been seen.
	 If so, we eliminate them.  This is a bit of a kludge because
	 the case lists for a single case statement (label) is a
	 single forward linked lists.  */
      if (seen_unreachable)
      {
	/* Advance until the first case in the list is reachable.  */
	while (body->ext.case_list != NULL
	       && body->ext.case_list->unreachable)
	  {
	    gfc_case *n = body->ext.case_list;
	    body->ext.case_list = body->ext.case_list->next;
	    n->next = NULL;
	    gfc_free_case_list (n);
	  }

	/* Strip all other unreachable cases.  */
	if (body->ext.case_list)
	  {
	    for (cp = body->ext.case_list; cp->next; cp = cp->next)
	      {
		if (cp->next->unreachable)
		  {
		    gfc_case *n = cp->next;
		    cp->next = cp->next->next;
		    n->next = NULL;
		    gfc_free_case_list (n);
		  }
	      }
	  }
      }
    }

  /* See if there were overlapping cases.  If the check returns NULL,
     there was overlap.  In that case we don't do anything.  If head
     is non-NULL, we prepend the DEFAULT case.  The sorted list can
     then used during code generation for SELECT CASE constructs with
     a case expression of a CHARACTER type.  */
  if (head)
    {
      head = check_case_overlap (head);

      /* Prepend the default_case if it is there.  */
      if (head != NULL && default_case)
	{
	  default_case->left = NULL;
	  default_case->right = head;
	  head->left = default_case;
	}
    }

  /* Eliminate dead blocks that may be the result if we've seen
     unreachable case labels for a block.  */
  for (body = code; body && body->block; body = body->block)
    {
      if (body->block->ext.case_list == NULL)
        {
	  /* Cut the unreachable block from the code chain.  */
	  gfc_code *c = body->block;
	  body->block = c->block;

	  /* Kill the dead block, but not the blocks below it.  */
	  c->block = NULL;
	  gfc_free_statements (c);
        }
    }

  /* More than two cases is legal but insane for logical selects.
     Issue a warning for it.  */
  if (gfc_option.warn_surprising && type == BT_LOGICAL
      && ncases > 2)
    gfc_warning ("Logical SELECT CASE block at %L has more that two cases",
		 &code->loc);
}


/* Resolve a transfer statement. This is making sure that:
   -- a derived type being transferred has only non-pointer components
   -- a derived type being transferred doesn't have private components
   -- we're not trying to transfer a whole assumed size array.  */

static void
resolve_transfer (gfc_code * code)
{
  gfc_typespec *ts;
  gfc_symbol *sym;
  gfc_ref *ref;
  gfc_expr *exp;

  exp = code->expr;

  if (exp->expr_type != EXPR_VARIABLE)
    return;

  sym = exp->symtree->n.sym;
  ts = &sym->ts;

  /* Go to actual component transferred.  */
  for (ref = code->expr->ref; ref; ref = ref->next)
    if (ref->type == REF_COMPONENT)
      ts = &ref->u.c.component->ts;

  if (ts->type == BT_DERIVED)
    {
      /* Check that transferred derived type doesn't contain POINTER
	 components.  */
      if (derived_pointer (ts->derived))
	{
	  gfc_error ("Data transfer element at %L cannot have "
		     "POINTER components", &code->loc);
	  return;
	}

      if (ts->derived->component_access == ACCESS_PRIVATE)
	{
	  gfc_error ("Data transfer element at %L cannot have "
		     "PRIVATE components",&code->loc);
	  return;
	}
    }

  if (sym->as != NULL && sym->as->type == AS_ASSUMED_SIZE
      && exp->ref->type == REF_ARRAY && exp->ref->u.ar.type == AR_FULL)
    {
      gfc_error ("Data transfer element at %L cannot be a full reference to "
		 "an assumed-size array", &code->loc);
      return;
    }
}


/*********** Toplevel code resolution subroutines ***********/

/* Given a branch to a label and a namespace, if the branch is conforming.
   The code node described where the branch is located.  */

static void
resolve_branch (gfc_st_label * label, gfc_code * code)
{
  gfc_code *block, *found;
  code_stack *stack;
  gfc_st_label *lp;

  if (label == NULL)
    return;
  lp = label;

  /* Step one: is this a valid branching target?  */

  if (lp->defined == ST_LABEL_UNKNOWN)
    {
      gfc_error ("Label %d referenced at %L is never defined", lp->value,
		 &lp->where);
      return;
    }

  if (lp->defined != ST_LABEL_TARGET)
    {
      gfc_error ("Statement at %L is not a valid branch target statement "
		 "for the branch statement at %L", &lp->where, &code->loc);
      return;
    }

  /* Step two: make sure this branch is not a branch to itself ;-)  */

  if (code->here == label)
    {
      gfc_warning ("Branch at %L causes an infinite loop", &code->loc);
      return;
    }

  /* Step three: Try to find the label in the parse tree. To do this,
     we traverse the tree block-by-block: first the block that
     contains this GOTO, then the block that it is nested in, etc.  We
     can ignore other blocks because branching into another block is
     not allowed.  */

  found = NULL;

  for (stack = cs_base; stack; stack = stack->prev)
    {
      for (block = stack->head; block; block = block->next)
	{
	  if (block->here == label)
	    {
	      found = block;
	      break;
	    }
	}

      if (found)
	break;
    }

  if (found == NULL)
    {
      /* still nothing, so illegal.  */
      gfc_error_now ("Label at %L is not in the same block as the "
		     "GOTO statement at %L", &lp->where, &code->loc);
      return;
    }

  /* Step four: Make sure that the branching target is legal if
     the statement is an END {SELECT,DO,IF}.  */

  if (found->op == EXEC_NOP)
    {
      for (stack = cs_base; stack; stack = stack->prev)
	if (stack->current->next == found)
	  break;

      if (stack == NULL)
	gfc_notify_std (GFC_STD_F95_DEL,
			"Obsolete: GOTO at %L jumps to END of construct at %L",
			&code->loc, &found->loc);
    }
}


/* Check whether EXPR1 has the same shape as EXPR2.  */

static try
resolve_where_shape (gfc_expr *expr1, gfc_expr *expr2)
{
  mpz_t shape[GFC_MAX_DIMENSIONS];
  mpz_t shape2[GFC_MAX_DIMENSIONS];
  try result = FAILURE;
  int i;

  /* Compare the rank.  */
  if (expr1->rank != expr2->rank)
    return result;

  /* Compare the size of each dimension.  */
  for (i=0; i<expr1->rank; i++)
    {
      if (gfc_array_dimen_size (expr1, i, &shape[i]) == FAILURE)
        goto ignore;

      if (gfc_array_dimen_size (expr2, i, &shape2[i]) == FAILURE)
        goto ignore;

      if (mpz_cmp (shape[i], shape2[i]))
        goto over;
    }

  /* When either of the two expression is an assumed size array, we
     ignore the comparison of dimension sizes.  */
ignore:
  result = SUCCESS;

over:
  for (i--; i>=0; i--)
    {
      mpz_clear (shape[i]);
      mpz_clear (shape2[i]);
    }
  return result;
}


/* Check whether a WHERE assignment target or a WHERE mask expression
   has the same shape as the outmost WHERE mask expression.  */

static void
resolve_where (gfc_code *code, gfc_expr *mask)
{
  gfc_code *cblock;
  gfc_code *cnext;
  gfc_expr *e = NULL;

  cblock = code->block;

  /* Store the first WHERE mask-expr of the WHERE statement or construct.
     In case of nested WHERE, only the outmost one is stored.  */
  if (mask == NULL) /* outmost WHERE */
    e = cblock->expr;
  else /* inner WHERE */
    e = mask;

  while (cblock)
    {
      if (cblock->expr)
        {
          /* Check if the mask-expr has a consistent shape with the
             outmost WHERE mask-expr.  */
          if (resolve_where_shape (cblock->expr, e) == FAILURE)
            gfc_error ("WHERE mask at %L has inconsistent shape",
                       &cblock->expr->where);
         }

      /* the assignment statement of a WHERE statement, or the first
         statement in where-body-construct of a WHERE construct */
      cnext = cblock->next;
      while (cnext)
        {
          switch (cnext->op)
            {
            /* WHERE assignment statement */
            case EXEC_ASSIGN:

              /* Check shape consistent for WHERE assignment target.  */
              if (e && resolve_where_shape (cnext->expr, e) == FAILURE)
               gfc_error ("WHERE assignment target at %L has "
                          "inconsistent shape", &cnext->expr->where);
              break;

            /* WHERE or WHERE construct is part of a where-body-construct */
            case EXEC_WHERE:
              resolve_where (cnext, e);
              break;

            default:
              gfc_error ("Unsupported statement inside WHERE at %L",
                         &cnext->loc);
            }
         /* the next statement within the same where-body-construct */
         cnext = cnext->next;
       }
    /* the next masked-elsewhere-stmt, elsewhere-stmt, or end-where-stmt */
    cblock = cblock->block;
  }
}


/* Check whether the FORALL index appears in the expression or not.  */

static try
gfc_find_forall_index (gfc_expr *expr, gfc_symbol *symbol)
{
  gfc_array_ref ar;
  gfc_ref *tmp;
  gfc_actual_arglist *args;
  int i;

  switch (expr->expr_type)
    {
    case EXPR_VARIABLE:
      gcc_assert (expr->symtree->n.sym);

      /* A scalar assignment  */
      if (!expr->ref)
        {
          if (expr->symtree->n.sym == symbol)
            return SUCCESS;
          else
            return FAILURE;
        }

      /* the expr is array ref, substring or struct component.  */
      tmp = expr->ref;
      while (tmp != NULL)
        {
          switch (tmp->type)
            {
            case  REF_ARRAY:
              /* Check if the symbol appears in the array subscript.  */
              ar = tmp->u.ar;
              for (i = 0; i < GFC_MAX_DIMENSIONS; i++)
                {
                  if (ar.start[i])
                    if (gfc_find_forall_index (ar.start[i], symbol) == SUCCESS)
                      return SUCCESS;

                  if (ar.end[i])
                    if (gfc_find_forall_index (ar.end[i], symbol) == SUCCESS)
                      return SUCCESS;

                  if (ar.stride[i])
                    if (gfc_find_forall_index (ar.stride[i], symbol) == SUCCESS)
                      return SUCCESS;
                }  /* end for  */
              break;

            case REF_SUBSTRING:
              if (expr->symtree->n.sym == symbol)
                return SUCCESS;
              tmp = expr->ref;
              /* Check if the symbol appears in the substring section.  */
              if (gfc_find_forall_index (tmp->u.ss.start, symbol) == SUCCESS)
                return SUCCESS;
              if (gfc_find_forall_index (tmp->u.ss.end, symbol) == SUCCESS)
                return SUCCESS;
              break;

            case REF_COMPONENT:
              break;

            default:
              gfc_error("expresion reference type error at %L", &expr->where);
            }
          tmp = tmp->next;
        }
      break;

    /* If the expression is a function call, then check if the symbol
       appears in the actual arglist of the function.  */
    case EXPR_FUNCTION:
      for (args = expr->value.function.actual; args; args = args->next)
        {
          if (gfc_find_forall_index(args->expr,symbol) == SUCCESS)
            return SUCCESS;
        }
      break;

    /* It seems not to happen.  */
    case EXPR_SUBSTRING:
      if (expr->ref)
        {
          tmp = expr->ref;
          gcc_assert (expr->ref->type == REF_SUBSTRING);
          if (gfc_find_forall_index (tmp->u.ss.start, symbol) == SUCCESS)
            return SUCCESS;
          if (gfc_find_forall_index (tmp->u.ss.end, symbol) == SUCCESS)
            return SUCCESS;
        }
      break;

    /* It seems not to happen.  */
    case EXPR_STRUCTURE:
    case EXPR_ARRAY:
      gfc_error ("Unsupported statement while finding forall index in "
                 "expression");
      break;

    case EXPR_OP:
      /* Find the FORALL index in the first operand.  */
      if (expr->value.op.op1)
	{
	  if (gfc_find_forall_index (expr->value.op.op1, symbol) == SUCCESS)
	    return SUCCESS;
	}

      /* Find the FORALL index in the second operand.  */
      if (expr->value.op.op2)
	{
	  if (gfc_find_forall_index (expr->value.op.op2, symbol) == SUCCESS)
	    return SUCCESS;
	}
      break;

    default:
      break;
    }

  return FAILURE;
}


/* Resolve assignment in FORALL construct.
   NVAR is the number of FORALL index variables, and VAR_EXPR records the
   FORALL index variables.  */

static void
gfc_resolve_assign_in_forall (gfc_code *code, int nvar, gfc_expr **var_expr)
{
  int n;

  for (n = 0; n < nvar; n++)
    {
      gfc_symbol *forall_index;

      forall_index = var_expr[n]->symtree->n.sym;

      /* Check whether the assignment target is one of the FORALL index
         variable.  */
      if ((code->expr->expr_type == EXPR_VARIABLE)
          && (code->expr->symtree->n.sym == forall_index))
        gfc_error ("Assignment to a FORALL index variable at %L",
                   &code->expr->where);
      else
        {
          /* If one of the FORALL index variables doesn't appear in the
             assignment target, then there will be a many-to-one
             assignment.  */
          if (gfc_find_forall_index (code->expr, forall_index) == FAILURE)
            gfc_error ("The FORALL with index '%s' cause more than one "
                       "assignment to this object at %L",
                       var_expr[n]->symtree->name, &code->expr->where);
        }
    }
}


/* Resolve WHERE statement in FORALL construct.  */

static void
gfc_resolve_where_code_in_forall (gfc_code *code, int nvar, gfc_expr **var_expr){
  gfc_code *cblock;
  gfc_code *cnext;

  cblock = code->block;
  while (cblock)
    {
      /* the assignment statement of a WHERE statement, or the first
         statement in where-body-construct of a WHERE construct */
      cnext = cblock->next;
      while (cnext)
        {
          switch (cnext->op)
            {
            /* WHERE assignment statement */
            case EXEC_ASSIGN:
              gfc_resolve_assign_in_forall (cnext, nvar, var_expr);
              break;

            /* WHERE or WHERE construct is part of a where-body-construct */
            case EXEC_WHERE:
              gfc_resolve_where_code_in_forall (cnext, nvar, var_expr);
              break;

            default:
              gfc_error ("Unsupported statement inside WHERE at %L",
                         &cnext->loc);
            }
          /* the next statement within the same where-body-construct */
          cnext = cnext->next;
        }
      /* the next masked-elsewhere-stmt, elsewhere-stmt, or end-where-stmt */
      cblock = cblock->block;
    }
}


/* Traverse the FORALL body to check whether the following errors exist:
   1. For assignment, check if a many-to-one assignment happens.
   2. For WHERE statement, check the WHERE body to see if there is any
      many-to-one assignment.  */

static void
gfc_resolve_forall_body (gfc_code *code, int nvar, gfc_expr **var_expr)
{
  gfc_code *c;

  c = code->block->next;
  while (c)
    {
      switch (c->op)
        {
        case EXEC_ASSIGN:
        case EXEC_POINTER_ASSIGN:
          gfc_resolve_assign_in_forall (c, nvar, var_expr);
          break;

        /* Because the resolve_blocks() will handle the nested FORALL,
           there is no need to handle it here.  */
        case EXEC_FORALL:
          break;
        case EXEC_WHERE:
          gfc_resolve_where_code_in_forall(c, nvar, var_expr);
          break;
        default:
          break;
        }
      /* The next statement in the FORALL body.  */
      c = c->next;
    }
}


/* Given a FORALL construct, first resolve the FORALL iterator, then call
   gfc_resolve_forall_body to resolve the FORALL body.  */

static void resolve_blocks (gfc_code *, gfc_namespace *);

static void
gfc_resolve_forall (gfc_code *code, gfc_namespace *ns, int forall_save)
{
  static gfc_expr **var_expr;
  static int total_var = 0;
  static int nvar = 0;
  gfc_forall_iterator *fa;
  gfc_symbol *forall_index;
  gfc_code *next;
  int i;

  /* Start to resolve a FORALL construct   */
  if (forall_save == 0)
    {
      /* Count the total number of FORALL index in the nested FORALL
         construct in order to allocate the VAR_EXPR with proper size.  */
      next = code;
      while ((next != NULL) && (next->op == EXEC_FORALL))
        {
          for (fa = next->ext.forall_iterator; fa; fa = fa->next)
            total_var ++;
          next = next->block->next;
        }

      /* Allocate VAR_EXPR with NUMBER_OF_FORALL_INDEX elements.  */
      var_expr = (gfc_expr **) gfc_getmem (total_var * sizeof (gfc_expr *));
    }

  /* The information about FORALL iterator, including FORALL index start, end
     and stride. The FORALL index can not appear in start, end or stride.  */
  for (fa = code->ext.forall_iterator; fa; fa = fa->next)
    {
      /* Check if any outer FORALL index name is the same as the current
         one.  */
      for (i = 0; i < nvar; i++)
        {
          if (fa->var->symtree->n.sym == var_expr[i]->symtree->n.sym)
            {
              gfc_error ("An outer FORALL construct already has an index "
                         "with this name %L", &fa->var->where);
            }
        }

      /* Record the current FORALL index.  */
      var_expr[nvar] = gfc_copy_expr (fa->var);

      forall_index = fa->var->symtree->n.sym;

      /* Check if the FORALL index appears in start, end or stride.  */
      if (gfc_find_forall_index (fa->start, forall_index) == SUCCESS)
        gfc_error ("A FORALL index must not appear in a limit or stride "
                   "expression in the same FORALL at %L", &fa->start->where);
      if (gfc_find_forall_index (fa->end, forall_index) == SUCCESS)
        gfc_error ("A FORALL index must not appear in a limit or stride "
                   "expression in the same FORALL at %L", &fa->end->where);
      if (gfc_find_forall_index (fa->stride, forall_index) == SUCCESS)
        gfc_error ("A FORALL index must not appear in a limit or stride "
                   "expression in the same FORALL at %L", &fa->stride->where);
      nvar++;
    }

  /* Resolve the FORALL body.  */
  gfc_resolve_forall_body (code, nvar, var_expr);

  /* May call gfc_resolve_forall to resolve the inner FORALL loop.  */
  resolve_blocks (code->block, ns);

  /* Free VAR_EXPR after the whole FORALL construct resolved.  */
  for (i = 0; i < total_var; i++)
    gfc_free_expr (var_expr[i]);

  /* Reset the counters.  */
  total_var = 0;
  nvar = 0;
}


/* Resolve lists of blocks found in IF, SELECT CASE, WHERE, FORALL ,GOTO and
   DO code nodes.  */

static void resolve_code (gfc_code *, gfc_namespace *);

static void
resolve_blocks (gfc_code * b, gfc_namespace * ns)
{
  try t;

  for (; b; b = b->block)
    {
      t = gfc_resolve_expr (b->expr);
      if (gfc_resolve_expr (b->expr2) == FAILURE)
	t = FAILURE;

      switch (b->op)
	{
	case EXEC_IF:
	  if (t == SUCCESS && b->expr != NULL
	      && (b->expr->ts.type != BT_LOGICAL || b->expr->rank != 0))
	    gfc_error
	      ("ELSE IF clause at %L requires a scalar LOGICAL expression",
	       &b->expr->where);
	  break;

	case EXEC_WHERE:
	  if (t == SUCCESS
	      && b->expr != NULL
	      && (b->expr->ts.type != BT_LOGICAL
		  || b->expr->rank == 0))
	    gfc_error
	      ("WHERE/ELSEWHERE clause at %L requires a LOGICAL array",
	       &b->expr->where);
	  break;

        case EXEC_GOTO:
          resolve_branch (b->label, b);
          break;

	case EXEC_SELECT:
	case EXEC_FORALL:
	case EXEC_DO:
	case EXEC_DO_WHILE:
	  break;

	default:
	  gfc_internal_error ("resolve_block(): Bad block type");
	}

      resolve_code (b->next, ns);
    }
}


/* Given a block of code, recursively resolve everything pointed to by this
   code block.  */

static void
resolve_code (gfc_code * code, gfc_namespace * ns)
{
  int forall_save = 0;
  code_stack frame;
  gfc_alloc *a;
  try t;

  frame.prev = cs_base;
  frame.head = code;
  cs_base = &frame;

  for (; code; code = code->next)
    {
      frame.current = code;

      if (code->op == EXEC_FORALL)
	{
	  forall_save = forall_flag;
	  forall_flag = 1;
          gfc_resolve_forall (code, ns, forall_save);
        }
      else
        resolve_blocks (code->block, ns);

      if (code->op == EXEC_FORALL)
	forall_flag = forall_save;

      t = gfc_resolve_expr (code->expr);
      if (gfc_resolve_expr (code->expr2) == FAILURE)
	t = FAILURE;

      switch (code->op)
	{
	case EXEC_NOP:
	case EXEC_CYCLE:
	case EXEC_PAUSE:
	case EXEC_STOP:
	case EXEC_EXIT:
	case EXEC_CONTINUE:
	case EXEC_DT_END:
	case EXEC_ENTRY:
	  break;

	case EXEC_WHERE:
	  resolve_where (code, NULL);
	  break;

	case EXEC_GOTO:
          if (code->expr != NULL)
	    {
	      if (code->expr->ts.type != BT_INTEGER)
		gfc_error ("ASSIGNED GOTO statement at %L requires an INTEGER "
                       "variable", &code->expr->where);
	      else if (code->expr->symtree->n.sym->attr.assign != 1)
		gfc_error ("Variable '%s' has not been assigned a target label "
			"at %L", code->expr->symtree->n.sym->name,
			&code->expr->where);
	    }
	  else
            resolve_branch (code->label, code);
	  break;

	case EXEC_RETURN:
	  if (code->expr != NULL && code->expr->ts.type != BT_INTEGER)
	    gfc_error ("Alternate RETURN statement at %L requires an INTEGER "
		       "return specifier", &code->expr->where);
	  break;

	case EXEC_ASSIGN:
	  if (t == FAILURE)
	    break;

	  if (gfc_extend_assign (code, ns) == SUCCESS)
	    goto call;

	  if (gfc_pure (NULL))
	    {
	      if (gfc_impure_variable (code->expr->symtree->n.sym))
		{
		  gfc_error
		    ("Cannot assign to variable '%s' in PURE procedure at %L",
		     code->expr->symtree->n.sym->name, &code->expr->where);
		  break;
		}

	      if (code->expr2->ts.type == BT_DERIVED
		  && derived_pointer (code->expr2->ts.derived))
		{
		  gfc_error
		    ("Right side of assignment at %L is a derived type "
		     "containing a POINTER in a PURE procedure",
		     &code->expr2->where);
		  break;
		}
	    }

	  gfc_check_assign (code->expr, code->expr2, 1);
	  break;

	case EXEC_LABEL_ASSIGN:
          if (code->label->defined == ST_LABEL_UNKNOWN)
            gfc_error ("Label %d referenced at %L is never defined",
                       code->label->value, &code->label->where);
          if (t == SUCCESS
	      && (code->expr->expr_type != EXPR_VARIABLE
		  || code->expr->symtree->n.sym->ts.type != BT_INTEGER
		  || code->expr->symtree->n.sym->ts.kind 
		        != gfc_default_integer_kind
		  || code->expr->symtree->n.sym->as != NULL))
	    gfc_error ("ASSIGN statement at %L requires a scalar "
		       "default INTEGER variable", &code->expr->where);
	  break;

	case EXEC_POINTER_ASSIGN:
	  if (t == FAILURE)
	    break;

	  gfc_check_pointer_assign (code->expr, code->expr2);
	  break;

	case EXEC_ARITHMETIC_IF:
	  if (t == SUCCESS
	      && code->expr->ts.type != BT_INTEGER
	      && code->expr->ts.type != BT_REAL)
	    gfc_error ("Arithmetic IF statement at %L requires a numeric "
		       "expression", &code->expr->where);

	  resolve_branch (code->label, code);
	  resolve_branch (code->label2, code);
	  resolve_branch (code->label3, code);
	  break;

	case EXEC_IF:
	  if (t == SUCCESS && code->expr != NULL
	      && (code->expr->ts.type != BT_LOGICAL
		  || code->expr->rank != 0))
	    gfc_error ("IF clause at %L requires a scalar LOGICAL expression",
		       &code->expr->where);
	  break;

	case EXEC_CALL:
	call:
	  resolve_call (code);
	  break;

	case EXEC_SELECT:
	  /* Select is complicated. Also, a SELECT construct could be
	     a transformed computed GOTO.  */
	  resolve_select (code);
	  break;

	case EXEC_DO:
	  if (code->ext.iterator != NULL)
	    gfc_resolve_iterator (code->ext.iterator, true);
	  break;

	case EXEC_DO_WHILE:
	  if (code->expr == NULL)
	    gfc_internal_error ("resolve_code(): No expression on DO WHILE");
	  if (t == SUCCESS
	      && (code->expr->rank != 0
		  || code->expr->ts.type != BT_LOGICAL))
	    gfc_error ("Exit condition of DO WHILE loop at %L must be "
		       "a scalar LOGICAL expression", &code->expr->where);
	  break;

	case EXEC_ALLOCATE:
	  if (t == SUCCESS && code->expr != NULL
	      && code->expr->ts.type != BT_INTEGER)
	    gfc_error ("STAT tag in ALLOCATE statement at %L must be "
		       "of type INTEGER", &code->expr->where);

	  for (a = code->ext.alloc_list; a; a = a->next)
	    resolve_allocate_expr (a->expr);

	  break;

	case EXEC_DEALLOCATE:
	  if (t == SUCCESS && code->expr != NULL
	      && code->expr->ts.type != BT_INTEGER)
	    gfc_error
	      ("STAT tag in DEALLOCATE statement at %L must be of type "
	       "INTEGER", &code->expr->where);

	  for (a = code->ext.alloc_list; a; a = a->next)
	    resolve_deallocate_expr (a->expr);

	  break;

	case EXEC_OPEN:
	  if (gfc_resolve_open (code->ext.open) == FAILURE)
	    break;

	  resolve_branch (code->ext.open->err, code);
	  break;

	case EXEC_CLOSE:
	  if (gfc_resolve_close (code->ext.close) == FAILURE)
	    break;

	  resolve_branch (code->ext.close->err, code);
	  break;

	case EXEC_BACKSPACE:
	case EXEC_ENDFILE:
	case EXEC_REWIND:
	  if (gfc_resolve_filepos (code->ext.filepos) == FAILURE)
	    break;

	  resolve_branch (code->ext.filepos->err, code);
	  break;

	case EXEC_INQUIRE:
	  if (gfc_resolve_inquire (code->ext.inquire) == FAILURE)
	      break;

	  resolve_branch (code->ext.inquire->err, code);
	  break;

	case EXEC_IOLENGTH:
	  gcc_assert (code->ext.inquire != NULL);
	  if (gfc_resolve_inquire (code->ext.inquire) == FAILURE)
	    break;

	  resolve_branch (code->ext.inquire->err, code);
	  break;

	case EXEC_READ:
	case EXEC_WRITE:
	  if (gfc_resolve_dt (code->ext.dt) == FAILURE)
	    break;

	  resolve_branch (code->ext.dt->err, code);
	  resolve_branch (code->ext.dt->end, code);
	  resolve_branch (code->ext.dt->eor, code);
	  break;

	case EXEC_TRANSFER:
	  resolve_transfer (code);
	  break;

	case EXEC_FORALL:
	  resolve_forall_iterators (code->ext.forall_iterator);

	  if (code->expr != NULL && code->expr->ts.type != BT_LOGICAL)
	    gfc_error
	      ("FORALL mask clause at %L requires a LOGICAL expression",
	       &code->expr->where);
	  break;

	default:
	  gfc_internal_error ("resolve_code(): Bad statement code");
	}
    }

  cs_base = frame.prev;
}


/* Resolve initial values and make sure they are compatible with
   the variable.  */

static void
resolve_values (gfc_symbol * sym)
{

  if (sym->value == NULL)
    return;

  if (gfc_resolve_expr (sym->value) == FAILURE)
    return;

  gfc_check_assign_symbol (sym, sym->value);
}


/* Do anything necessary to resolve a symbol.  Right now, we just
   assume that an otherwise unknown symbol is a variable.  This sort
   of thing commonly happens for symbols in module.  */

static void
resolve_symbol (gfc_symbol * sym)
{
  /* Zero if we are checking a formal namespace.  */
  static int formal_ns_flag = 1;
  int formal_ns_save, check_constant, mp_flag;
  int i;
  const char *whynot;
  gfc_namelist *nl;

  if (sym->attr.flavor == FL_UNKNOWN)
    {
      if (sym->attr.external == 0 && sym->attr.intrinsic == 0)
	sym->attr.flavor = FL_VARIABLE;
      else
	{
	  sym->attr.flavor = FL_PROCEDURE;
	  if (sym->attr.dimension)
	    sym->attr.function = 1;
	}
    }

  /* Symbols that are module procedures with results (functions) have
     the types and array specification copied for type checking in
     procedures that call them, as well as for saving to a module
     file.  These symbols can't stand the scrutiny that their results
     can.  */
  mp_flag = (sym->result != NULL && sym->result != sym);

  /* Assign default type to symbols that need one and don't have one.  */
  if (sym->ts.type == BT_UNKNOWN)
    {
      if (sym->attr.flavor == FL_VARIABLE || sym->attr.flavor == FL_PARAMETER)
	gfc_set_default_type (sym, 1, NULL);

      if (sym->attr.flavor == FL_PROCEDURE && sym->attr.function)
	{
	  if (!mp_flag)
	    gfc_set_default_type (sym, 0, NULL);
	  else
	    {
              /* Result may be in another namespace.  */
	      resolve_symbol (sym->result);

	      sym->ts = sym->result->ts;
	      sym->as = gfc_copy_array_spec (sym->result->as);
	    }
	}
    }

  /* Assumed size arrays and assumed shape arrays must be dummy
     arguments.  */ 

  if (sym->as != NULL
      && (sym->as->type == AS_ASSUMED_SIZE
	  || sym->as->type == AS_ASSUMED_SHAPE)
      && sym->attr.dummy == 0)
    {
      gfc_error ("Assumed %s array at %L must be a dummy argument",
		 sym->as->type == AS_ASSUMED_SIZE ? "size" : "shape",
                 &sym->declared_at);
      return;
    }

  /* A parameter array's shape needs to be constant.  */

  if (sym->attr.flavor == FL_PARAMETER && sym->as != NULL 
      && !gfc_is_compile_time_shape (sym->as))
    {
      gfc_error ("Parameter array '%s' at %L cannot be automatic "
		 "or assumed shape", sym->name, &sym->declared_at);
	  return;
    }

  /* Make sure that character string variables with assumed length are
     dummy arguments.  */

  if (sym->attr.flavor == FL_VARIABLE && !sym->attr.result
      && sym->ts.type == BT_CHARACTER
      && sym->ts.cl->length == NULL && sym->attr.dummy == 0)
    {
      gfc_error ("Entity with assumed character length at %L must be a "
		 "dummy argument or a PARAMETER", &sym->declared_at);
      return;
    }

  /* Make sure a parameter that has been implicitly typed still
     matches the implicit type, since PARAMETER statements can precede
     IMPLICIT statements.  */

  if (sym->attr.flavor == FL_PARAMETER
      && sym->attr.implicit_type
      && !gfc_compare_types (&sym->ts, gfc_get_default_type (sym, sym->ns)))
    gfc_error ("Implicitly typed PARAMETER '%s' at %L doesn't match a "
	       "later IMPLICIT type", sym->name, &sym->declared_at);

  /* Make sure the types of derived parameters are consistent.  This
     type checking is deferred until resolution because the type may
     refer to a derived type from the host.  */

  if (sym->attr.flavor == FL_PARAMETER
      && sym->ts.type == BT_DERIVED
      && !gfc_compare_types (&sym->ts, &sym->value->ts))
    gfc_error ("Incompatible derived type in PARAMETER at %L",
	       &sym->value->where);

  /* Make sure symbols with known intent or optional are really dummy
     variable.  Because of ENTRY statement, this has to be deferred
     until resolution time.  */

  if (! sym->attr.dummy
      && (sym->attr.optional
	  || sym->attr.intent != INTENT_UNKNOWN))
    {
      gfc_error ("Symbol at %L is not a DUMMY variable", &sym->declared_at);
      return;
    }

  if (sym->attr.proc == PROC_ST_FUNCTION)
    {
      if (sym->ts.type == BT_CHARACTER)
        {
          gfc_charlen *cl = sym->ts.cl;
          if (!cl || !cl->length || cl->length->expr_type != EXPR_CONSTANT)
            {
              gfc_error ("Character-valued statement function '%s' at %L must "
                         "have constant length", sym->name, &sym->declared_at);
              return;
            }
        }
    }

  /* Constraints on deferred shape variable.  */
  if (sym->attr.flavor == FL_VARIABLE
      || (sym->attr.flavor == FL_PROCEDURE
	  && sym->attr.function))
    {
      if (sym->as == NULL || sym->as->type != AS_DEFERRED)
	{
	  if (sym->attr.allocatable)
	    {
	      if (sym->attr.dimension)
		gfc_error ("Allocatable array at %L must have a deferred shape",
			   &sym->declared_at);
	      else
		gfc_error ("Object at %L may not be ALLOCATABLE",
			   &sym->declared_at);
	      return;
	    }

	  if (sym->attr.pointer && sym->attr.dimension)
	    {
	      gfc_error ("Pointer to array at %L must have a deferred shape",
			 &sym->declared_at);
	      return;
	    }

	}
      else
	{
	  if (!mp_flag && !sym->attr.allocatable
	      && !sym->attr.pointer && !sym->attr.dummy)
	    {
	      gfc_error ("Array at %L cannot have a deferred shape",
			 &sym->declared_at);
	      return;
	    }
	}
    }

  switch (sym->attr.flavor)
    {
    case FL_VARIABLE:
      /* Can the sybol have an initializer?  */
      whynot = NULL;
      if (sym->attr.allocatable)
	whynot = "Allocatable";
      else if (sym->attr.external)
	whynot = "External";
      else if (sym->attr.dummy)
	whynot = "Dummy";
      else if (sym->attr.intrinsic)
	whynot = "Intrinsic";
      else if (sym->attr.result)
	whynot = "Function Result";
      else if (sym->attr.dimension && !sym->attr.pointer)
	{
	  /* Don't allow initialization of automatic arrays.  */
	  for (i = 0; i < sym->as->rank; i++)
	    {
	      if (sym->as->lower[i] == NULL
		  || sym->as->lower[i]->expr_type != EXPR_CONSTANT
		  || sym->as->upper[i] == NULL
		  || sym->as->upper[i]->expr_type != EXPR_CONSTANT)
		{
		  whynot = "Automatic array";
		  break;
		}
	    }
	}

      /* Reject illegal initializers.  */
      if (sym->value && whynot)
	{
	  gfc_error ("%s '%s' at %L cannot have an initializer",
		     whynot, sym->name, &sym->declared_at);
	  return;
	}

      /* Assign default initializer.  */
      if (sym->ts.type == BT_DERIVED && !(sym->value || whynot))
	sym->value = gfc_default_initializer (&sym->ts);
      break;

    case FL_NAMELIST:
      /* Reject PRIVATE objects in a PUBLIC namelist.  */
      if (gfc_check_access(sym->attr.access, sym->ns->default_access))
	{
	  for (nl = sym->namelist; nl; nl = nl->next)
	    {
	      if (!gfc_check_access(nl->sym->attr.access,
				    nl->sym->ns->default_access))
		gfc_error ("PRIVATE symbol '%s' cannot be member of "
			   "PUBLIC namelist at %L", nl->sym->name,
			   &sym->declared_at);
	    }
	}
      break;

    default:
      break;
    }


  /* Make sure that intrinsic exist */
  if (sym->attr.intrinsic
      && ! gfc_intrinsic_name(sym->name, 0)
      && ! gfc_intrinsic_name(sym->name, 1))
    gfc_error("Intrinsic at %L does not exist", &sym->declared_at);

  /* Resolve array specifier. Check as well some constraints
     on COMMON blocks.  */

  check_constant = sym->attr.in_common && !sym->attr.pointer;
  gfc_resolve_array_spec (sym->as, check_constant);

  /* Resolve formal namespaces.  */

  if (formal_ns_flag && sym != NULL && sym->formal_ns != NULL)
    {
      formal_ns_save = formal_ns_flag;
      formal_ns_flag = 0;
      gfc_resolve (sym->formal_ns);
      formal_ns_flag = formal_ns_save;
    }
}



/************* Resolve DATA statements *************/

static struct
{
  gfc_data_value *vnode;
  unsigned int left;
}
values;


/* Advance the values structure to point to the next value in the data list.  */

static try
next_data_value (void)
{
  while (values.left == 0)
    {
      if (values.vnode->next == NULL)
	return FAILURE;

      values.vnode = values.vnode->next;
      values.left = values.vnode->repeat;
    }

  return SUCCESS;
}


static try
check_data_variable (gfc_data_variable * var, locus * where)
{
  gfc_expr *e;
  mpz_t size;
  mpz_t offset;
  try t;
  ar_type mark = AR_UNKNOWN;
  int i;
  mpz_t section_index[GFC_MAX_DIMENSIONS];
  gfc_ref *ref;
  gfc_array_ref *ar;

  if (gfc_resolve_expr (var->expr) == FAILURE)
    return FAILURE;

  ar = NULL;
  mpz_init_set_si (offset, 0);
  e = var->expr;

  if (e->expr_type != EXPR_VARIABLE)
    gfc_internal_error ("check_data_variable(): Bad expression");

  if (e->rank == 0)
    {
      mpz_init_set_ui (size, 1);
      ref = NULL;
    }
  else
    {
      ref = e->ref;

      /* Find the array section reference.  */
      for (ref = e->ref; ref; ref = ref->next)
	{
	  if (ref->type != REF_ARRAY)
	    continue;
	  if (ref->u.ar.type == AR_ELEMENT)
	    continue;
	  break;
	}
      gcc_assert (ref);

      /* Set marks according to the reference pattern.  */
      switch (ref->u.ar.type)
	{
	case AR_FULL:
	  mark = AR_FULL;
	  break;

	case AR_SECTION:
          ar = &ref->u.ar;
          /* Get the start position of array section.  */
          gfc_get_section_index (ar, section_index, &offset);
          mark = AR_SECTION;
	  break;

	default:
	  gcc_unreachable ();
	}

      if (gfc_array_size (e, &size) == FAILURE)
	{
	  gfc_error ("Nonconstant array section at %L in DATA statement",
		     &e->where);
	  mpz_clear (offset);
	  return FAILURE;
	}
    }

  t = SUCCESS;

  while (mpz_cmp_ui (size, 0) > 0)
    {
      if (next_data_value () == FAILURE)
	{
	  gfc_error ("DATA statement at %L has more variables than values",
		     where);
	  t = FAILURE;
	  break;
	}

      t = gfc_check_assign (var->expr, values.vnode->expr, 0);
      if (t == FAILURE)
	break;

      /* If we have more than one element left in the repeat count,
	 and we have more than one element left in the target variable,
	 then create a range assignment.  */
      /* ??? Only done for full arrays for now, since array sections
	 seem tricky.  */
      if (mark == AR_FULL && ref && ref->next == NULL
	  && values.left > 1 && mpz_cmp_ui (size, 1) > 0)
	{
	  mpz_t range;

	  if (mpz_cmp_ui (size, values.left) >= 0)
	    {
	      mpz_init_set_ui (range, values.left);
	      mpz_sub_ui (size, size, values.left);
	      values.left = 0;
	    }
	  else
	    {
	      mpz_init_set (range, size);
	      values.left -= mpz_get_ui (size);
	      mpz_set_ui (size, 0);
	    }

	  gfc_assign_data_value_range (var->expr, values.vnode->expr,
				       offset, range);

	  mpz_add (offset, offset, range);
	  mpz_clear (range);
	}

      /* Assign initial value to symbol.  */
      else
	{
	  values.left -= 1;
	  mpz_sub_ui (size, size, 1);

	  gfc_assign_data_value (var->expr, values.vnode->expr, offset);

	  if (mark == AR_FULL)
	    mpz_add_ui (offset, offset, 1);

	  /* Modify the array section indexes and recalculate the offset
	     for next element.  */
	  else if (mark == AR_SECTION)
	    gfc_advance_section (section_index, ar, &offset);
	}
    }

  if (mark == AR_SECTION)
    {
      for (i = 0; i < ar->dimen; i++)
        mpz_clear (section_index[i]);
    }

  mpz_clear (size);
  mpz_clear (offset);

  return t;
}


static try traverse_data_var (gfc_data_variable *, locus *);

/* Iterate over a list of elements in a DATA statement.  */

static try
traverse_data_list (gfc_data_variable * var, locus * where)
{
  mpz_t trip;
  iterator_stack frame;
  gfc_expr *e;

  mpz_init (frame.value);

  mpz_init_set (trip, var->iter.end->value.integer);
  mpz_sub (trip, trip, var->iter.start->value.integer);
  mpz_add (trip, trip, var->iter.step->value.integer);

  mpz_div (trip, trip, var->iter.step->value.integer);

  mpz_set (frame.value, var->iter.start->value.integer);

  frame.prev = iter_stack;
  frame.variable = var->iter.var->symtree;
  iter_stack = &frame;

  while (mpz_cmp_ui (trip, 0) > 0)
    {
      if (traverse_data_var (var->list, where) == FAILURE)
	{
	  mpz_clear (trip);
	  return FAILURE;
	}

      e = gfc_copy_expr (var->expr);
      if (gfc_simplify_expr (e, 1) == FAILURE)
        {
          gfc_free_expr (e);
          return FAILURE;
        }

      mpz_add (frame.value, frame.value, var->iter.step->value.integer);

      mpz_sub_ui (trip, trip, 1);
    }

  mpz_clear (trip);
  mpz_clear (frame.value);

  iter_stack = frame.prev;
  return SUCCESS;
}


/* Type resolve variables in the variable list of a DATA statement.  */

static try
traverse_data_var (gfc_data_variable * var, locus * where)
{
  try t;

  for (; var; var = var->next)
    {
      if (var->expr == NULL)
	t = traverse_data_list (var, where);
      else
	t = check_data_variable (var, where);

      if (t == FAILURE)
	return FAILURE;
    }

  return SUCCESS;
}


/* Resolve the expressions and iterators associated with a data statement.
   This is separate from the assignment checking because data lists should
   only be resolved once.  */

static try
resolve_data_variables (gfc_data_variable * d)
{
  for (; d; d = d->next)
    {
      if (d->list == NULL)
	{
	  if (gfc_resolve_expr (d->expr) == FAILURE)
	    return FAILURE;
	}
      else
	{
	  if (gfc_resolve_iterator (&d->iter, false) == FAILURE)
	    return FAILURE;

	  if (d->iter.start->expr_type != EXPR_CONSTANT
	      || d->iter.end->expr_type != EXPR_CONSTANT
	      || d->iter.step->expr_type != EXPR_CONSTANT)
	    gfc_internal_error ("resolve_data_variables(): Bad iterator");

	  if (resolve_data_variables (d->list) == FAILURE)
	    return FAILURE;
	}
    }

  return SUCCESS;
}


/* Resolve a single DATA statement.  We implement this by storing a pointer to
   the value list into static variables, and then recursively traversing the
   variables list, expanding iterators and such.  */

static void
resolve_data (gfc_data * d)
{
  if (resolve_data_variables (d->var) == FAILURE)
    return;

  values.vnode = d->value;
  values.left = (d->value == NULL) ? 0 : d->value->repeat;

  if (traverse_data_var (d->var, &d->where) == FAILURE)
    return;

  /* At this point, we better not have any values left.  */

  if (next_data_value () == SUCCESS)
    gfc_error ("DATA statement at %L has more values than variables",
	       &d->where);
}


/* Determines if a variable is not 'pure', ie not assignable within a pure
   procedure.  Returns zero if assignment is OK, nonzero if there is a problem.
 */

int
gfc_impure_variable (gfc_symbol * sym)
{
  if (sym->attr.use_assoc || sym->attr.in_common)
    return 1;

  if (sym->ns != gfc_current_ns)
    return !sym->attr.function;

  /* TODO: Check storage association through EQUIVALENCE statements */

  return 0;
}


/* Test whether a symbol is pure or not.  For a NULL pointer, checks the
   symbol of the current procedure.  */

int
gfc_pure (gfc_symbol * sym)
{
  symbol_attribute attr;

  if (sym == NULL)
    sym = gfc_current_ns->proc_name;
  if (sym == NULL)
    return 0;

  attr = sym->attr;

  return attr.flavor == FL_PROCEDURE && (attr.pure || attr.elemental);
}


/* Test whether the current procedure is elemental or not.  */

int
gfc_elemental (gfc_symbol * sym)
{
  symbol_attribute attr;

  if (sym == NULL)
    sym = gfc_current_ns->proc_name;
  if (sym == NULL)
    return 0;
  attr = sym->attr;

  return attr.flavor == FL_PROCEDURE && attr.elemental;
}


/* Warn about unused labels.  */

static void
warn_unused_label (gfc_namespace * ns)
{
  gfc_st_label *l;

  l = ns->st_labels;
  if (l == NULL)
    return;

  while (l->next)
    l = l->next;

  for (; l; l = l->prev)
    {
      if (l->defined == ST_LABEL_UNKNOWN)
	continue;

      switch (l->referenced)
	{
	case ST_LABEL_UNKNOWN:
	  gfc_warning ("Label %d at %L defined but not used", l->value,
		       &l->where);
	  break;

	case ST_LABEL_BAD_TARGET:
	  gfc_warning ("Label %d at %L defined but cannot be used", l->value,
		       &l->where);
	  break;

	default:
	  break;
	}
    }
}


/* Resolve derived type EQUIVALENCE object.  */

static try
resolve_equivalence_derived (gfc_symbol *derived, gfc_symbol *sym, gfc_expr *e)
{
  gfc_symbol *d;
  gfc_component *c = derived->components;

  if (!derived)
    return SUCCESS;

  /* Shall not be an object of nonsequence derived type.  */
  if (!derived->attr.sequence)
    {
      gfc_error ("Derived type variable '%s' at %L must have SEQUENCE "
                 "attribute to be an EQUIVALENCE object", sym->name, &e->where);
      return FAILURE;
    }

  for (; c ; c = c->next)
    {
      d = c->ts.derived;
      if (d && (resolve_equivalence_derived (c->ts.derived, sym, e) == FAILURE))
        return FAILURE;
        
      /* Shall not be an object of sequence derived type containing a pointer
         in the structure.  */
      if (c->pointer)
        {
          gfc_error ("Derived type variable '%s' at %L has pointer componet(s) "
                     "cannot be an EQUIVALENCE object", sym->name, &e->where);
          return FAILURE;
        }
    }
  return SUCCESS;
}


/* Resolve equivalence object. 
   An EQUIVALENCE object shall not be a dummy argument, a pointer, an
   allocatable array, an object of nonsequence derived type, an object of
   sequence derived type containing a pointer at any level of component
   selection, an automatic object, a function name, an entry name, a result
   name, a named constant, a structure component, or a subobject of any of
   the preceding objects.  */

static void
resolve_equivalence (gfc_equiv *eq)
{
  gfc_symbol *sym;
  gfc_symbol *derived;
  gfc_expr *e;
  gfc_ref *r;

  for (; eq; eq = eq->eq)
    {
      e = eq->expr;
      if (gfc_resolve_expr (e) == FAILURE)
        continue;

      sym = e->symtree->n.sym;
     
      /* Shall not be a dummy argument.  */
      if (sym->attr.dummy)
        {
          gfc_error ("Dummy argument '%s' at %L cannot be an EQUIVALENCE "
                     "object", sym->name, &e->where);
          continue;
        }

      /* Shall not be an allocatable array.  */
      if (sym->attr.allocatable)
        {
          gfc_error ("Allocatable array '%s' at %L cannot be an EQUIVALENCE "
                     "object", sym->name, &e->where);
          continue;
        }

      /* Shall not be a pointer.  */
      if (sym->attr.pointer)
        {
          gfc_error ("Pointer '%s' at %L cannot be an EQUIVALENCE object",
                     sym->name, &e->where);
          continue;
        }
      
      /* Shall not be a function name, ...  */
      if (sym->attr.function || sym->attr.result || sym->attr.entry
          || sym->attr.subroutine)
        {
          gfc_error ("Entity '%s' at %L cannot be an EQUIVALENCE object",
                     sym->name, &e->where);
          continue;
        }

      /* Shall not be a named constant.  */      
      if (e->expr_type == EXPR_CONSTANT)
        {
          gfc_error ("Named constant '%s' at %L cannot be an EQUIVALENCE "
                     "object", sym->name, &e->where);
          continue;
        }

      derived = e->ts.derived;
      if (derived && resolve_equivalence_derived (derived, sym, e) == FAILURE)
        continue;

      if (!e->ref)
        continue;

      /* Shall not be an automatic array.  */
      if (e->ref->type == REF_ARRAY
          && gfc_resolve_array_spec (e->ref->u.ar.as, 1) == FAILURE)
        {
          gfc_error ("Array '%s' at %L with non-constant bounds cannot be "
                     "an EQUIVALENCE object", sym->name, &e->where);
          continue;
        }

      /* Shall not be a structure component.  */
      r = e->ref;
      while (r)
        {
          if (r->type == REF_COMPONENT)
            {
              gfc_error ("Structure component '%s' at %L cannot be an "
                         "EQUIVALENCE object",
                         r->u.c.component->name, &e->where);
              break;
            }
          r = r->next;
        }
    }    
}      
      
      
/* This function is called after a complete program unit has been compiled.
   Its purpose is to examine all of the expressions associated with a program
   unit, assign types to all intermediate expressions, make sure that all
   assignments are to compatible types and figure out which names refer to
   which functions or subroutines.  */

void
gfc_resolve (gfc_namespace * ns)
{
  gfc_namespace *old_ns, *n;
  gfc_charlen *cl;
  gfc_data *d;
  gfc_equiv *eq;

  old_ns = gfc_current_ns;
  gfc_current_ns = ns;

  resolve_entries (ns);

  resolve_contained_functions (ns);

  gfc_traverse_ns (ns, resolve_symbol);

  for (n = ns->contained; n; n = n->sibling)
    {
      if (gfc_pure (ns->proc_name) && !gfc_pure (n->proc_name))
	gfc_error ("Contained procedure '%s' at %L of a PURE procedure must "
		   "also be PURE", n->proc_name->name,
		   &n->proc_name->declared_at);

      gfc_resolve (n);
    }

  forall_flag = 0;
  gfc_check_interfaces (ns);

  for (cl = ns->cl_list; cl; cl = cl->next)
    {
      if (cl->length == NULL || gfc_resolve_expr (cl->length) == FAILURE)
	continue;

      if (gfc_simplify_expr (cl->length, 0) == FAILURE)
	continue;

      if (gfc_specification_expr (cl->length) == FAILURE)
	continue;
    }

  gfc_traverse_ns (ns, resolve_values);

  if (ns->save_all)
    gfc_save_all (ns);

  iter_stack = NULL;
  for (d = ns->data; d; d = d->next)
    resolve_data (d);

  iter_stack = NULL;
  gfc_traverse_ns (ns, gfc_formalize_init_value);

  for (eq = ns->equiv; eq; eq = eq->next)
    resolve_equivalence (eq);

  cs_base = NULL;
  resolve_code (ns->code, ns);

  /* Warn about unused labels.  */
  if (gfc_option.warn_unused_labels)
    warn_unused_label (ns);

  gfc_current_ns = old_ns;
}
