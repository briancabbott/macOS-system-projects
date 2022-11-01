/* Evaluator for GNU Emacs Lisp interpreter.
   Copyright (C) 1985, 1986, 1987, 1993, 1994, 1995, 1999, 2000, 2001,
                 2002, 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

This file is part of GNU Emacs.

GNU Emacs is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Emacs is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */


#include <config.h>
#include "lisp.h"
#include "blockinput.h"
#include "commands.h"
#include "keyboard.h"
#include "dispextern.h"
#include <setjmp.h>

#if HAVE_X_WINDOWS
#include "xterm.h"
#endif

/* This definition is duplicated in alloc.c and keyboard.c */
/* Putting it in lisp.h makes cc bomb out! */

struct backtrace
{
  struct backtrace *next;
  Lisp_Object *function;
  Lisp_Object *args;	/* Points to vector of args. */
  int nargs;		/* Length of vector.
			   If nargs is UNEVALLED, args points to slot holding
			   list of unevalled args */
  char evalargs;
  /* Nonzero means call value of debugger when done with this operation. */
  char debug_on_exit;
};

struct backtrace *backtrace_list;

/* This structure helps implement the `catch' and `throw' control
   structure.  A struct catchtag contains all the information needed
   to restore the state of the interpreter after a non-local jump.

   Handlers for error conditions (represented by `struct handler'
   structures) just point to a catch tag to do the cleanup required
   for their jumps.

   catchtag structures are chained together in the C calling stack;
   the `next' member points to the next outer catchtag.

   A call like (throw TAG VAL) searches for a catchtag whose `tag'
   member is TAG, and then unbinds to it.  The `val' member is used to
   hold VAL while the stack is unwound; `val' is returned as the value
   of the catch form.

   All the other members are concerned with restoring the interpreter
   state.  */

struct catchtag
{
  Lisp_Object tag;
  Lisp_Object val;
  struct catchtag *next;
  struct gcpro *gcpro;
  jmp_buf jmp;
  struct backtrace *backlist;
  struct handler *handlerlist;
  int lisp_eval_depth;
  int pdlcount;
  int poll_suppress_count;
  int interrupt_input_blocked;
  struct byte_stack *byte_stack;
};

struct catchtag *catchlist;

#ifdef DEBUG_GCPRO
/* Count levels of GCPRO to detect failure to UNGCPRO.  */
int gcpro_level;
#endif

Lisp_Object Qautoload, Qmacro, Qexit, Qinteractive, Qcommandp, Qdefun;
Lisp_Object Qinhibit_quit, Vinhibit_quit, Vquit_flag;
Lisp_Object Qand_rest, Qand_optional;
Lisp_Object Qdebug_on_error;
Lisp_Object Qdeclare;

/* This holds either the symbol `run-hooks' or nil.
   It is nil at an early stage of startup, and when Emacs
   is shutting down.  */

Lisp_Object Vrun_hooks;

/* Non-nil means record all fset's and provide's, to be undone
   if the file being autoloaded is not fully loaded.
   They are recorded by being consed onto the front of Vautoload_queue:
   (FUN . ODEF) for a defun, (0 . OFEATURES) for a provide.  */

Lisp_Object Vautoload_queue;

/* Current number of specbindings allocated in specpdl.  */

int specpdl_size;

/* Pointer to beginning of specpdl.  */

struct specbinding *specpdl;

/* Pointer to first unused element in specpdl.  */

struct specbinding *specpdl_ptr;

/* Maximum size allowed for specpdl allocation */

EMACS_INT max_specpdl_size;

/* Depth in Lisp evaluations and function calls.  */

int lisp_eval_depth;

/* Maximum allowed depth in Lisp evaluations and function calls.  */

EMACS_INT max_lisp_eval_depth;

/* Nonzero means enter debugger before next function call */

int debug_on_next_call;

/* Non-zero means debugger may continue.  This is zero when the
   debugger is called during redisplay, where it might not be safe to
   continue the interrupted redisplay. */

int debugger_may_continue;

/* List of conditions (non-nil atom means all) which cause a backtrace
   if an error is handled by the command loop's error handler.  */

Lisp_Object Vstack_trace_on_error;

/* List of conditions (non-nil atom means all) which enter the debugger
   if an error is handled by the command loop's error handler.  */

Lisp_Object Vdebug_on_error;

/* List of conditions and regexps specifying error messages which
   do not enter the debugger even if Vdebug_on_error says they should.  */

Lisp_Object Vdebug_ignored_errors;

/* Non-nil means call the debugger even if the error will be handled.  */

Lisp_Object Vdebug_on_signal;

/* Hook for edebug to use.  */

Lisp_Object Vsignal_hook_function;

/* Nonzero means enter debugger if a quit signal
   is handled by the command loop's error handler. */

int debug_on_quit;

/* The value of num_nonmacro_input_events as of the last time we
   started to enter the debugger.  If we decide to enter the debugger
   again when this is still equal to num_nonmacro_input_events, then we
   know that the debugger itself has an error, and we should just
   signal the error instead of entering an infinite loop of debugger
   invocations.  */

int when_entered_debugger;

Lisp_Object Vdebugger;

/* The function from which the last `signal' was called.  Set in
   Fsignal.  */

Lisp_Object Vsignaling_function;

/* Set to non-zero while processing X events.  Checked in Feval to
   make sure the Lisp interpreter isn't called from a signal handler,
   which is unsafe because the interpreter isn't reentrant.  */

int handling_signal;

/* Function to process declarations in defmacro forms.  */

Lisp_Object Vmacro_declaration_function;

extern Lisp_Object Qrisky_local_variable;

static Lisp_Object funcall_lambda P_ ((Lisp_Object, int, Lisp_Object*));
static void unwind_to_catch P_ ((struct catchtag *, Lisp_Object)) NO_RETURN;

#if __GNUC__
/* "gcc -O3" enables automatic function inlining, which optimizes out
   the arguments for the invocations of these functions, whereas they
   expect these values on the stack.  */
Lisp_Object apply1 () __attribute__((noinline));
Lisp_Object call2 () __attribute__((noinline));
#endif

void
init_eval_once ()
{
  specpdl_size = 50;
  specpdl = (struct specbinding *) xmalloc (specpdl_size * sizeof (struct specbinding));
  specpdl_ptr = specpdl;
  /* Don't forget to update docs (lispref node "Local Variables").  */
  max_specpdl_size = 1000;
  max_lisp_eval_depth = 300;

  Vrun_hooks = Qnil;
}

void
init_eval ()
{
  specpdl_ptr = specpdl;
  catchlist = 0;
  handlerlist = 0;
  backtrace_list = 0;
  Vquit_flag = Qnil;
  debug_on_next_call = 0;
  lisp_eval_depth = 0;
#ifdef DEBUG_GCPRO
  gcpro_level = 0;
#endif
  /* This is less than the initial value of num_nonmacro_input_events.  */
  when_entered_debugger = -1;
}

/* unwind-protect function used by call_debugger.  */

static Lisp_Object
restore_stack_limits (data)
     Lisp_Object data;
{
  max_specpdl_size = XINT (XCAR (data));
  max_lisp_eval_depth = XINT (XCDR (data));
  return Qnil;
}

/* Call the Lisp debugger, giving it argument ARG.  */

Lisp_Object
call_debugger (arg)
     Lisp_Object arg;
{
  int debug_while_redisplaying;
  int count = SPECPDL_INDEX ();
  Lisp_Object val;
  int old_max = max_specpdl_size;

  /* Temporarily bump up the stack limits,
     so the debugger won't run out of stack.  */

  max_specpdl_size += 1;
  record_unwind_protect (restore_stack_limits,
			 Fcons (make_number (old_max),
				make_number (max_lisp_eval_depth)));
  max_specpdl_size = old_max;

  if (lisp_eval_depth + 40 > max_lisp_eval_depth)
    max_lisp_eval_depth = lisp_eval_depth + 40;

  if (SPECPDL_INDEX () + 100 > max_specpdl_size)
    max_specpdl_size = SPECPDL_INDEX () + 100;

#ifdef HAVE_X_WINDOWS
  if (display_hourglass_p)
    cancel_hourglass ();
#endif

  debug_on_next_call = 0;
  when_entered_debugger = num_nonmacro_input_events;

  /* Resetting redisplaying_p to 0 makes sure that debug output is
     displayed if the debugger is invoked during redisplay.  */
  debug_while_redisplaying = redisplaying_p;
  redisplaying_p = 0;
  specbind (intern ("debugger-may-continue"),
	    debug_while_redisplaying ? Qnil : Qt);
  specbind (Qinhibit_redisplay, Qnil);
  specbind (Qdebug_on_error, Qnil);

#if 0 /* Binding this prevents execution of Lisp code during
	 redisplay, which necessarily leads to display problems.  */
  specbind (Qinhibit_eval_during_redisplay, Qt);
#endif

  val = apply1 (Vdebugger, arg);

  /* Interrupting redisplay and resuming it later is not safe under
     all circumstances.  So, when the debugger returns, abort the
     interrupted redisplay by going back to the top-level.  */
  if (debug_while_redisplaying)
    Ftop_level ();

  return unbind_to (count, val);
}

void
do_debug_on_call (code)
     Lisp_Object code;
{
  debug_on_next_call = 0;
  backtrace_list->debug_on_exit = 1;
  call_debugger (Fcons (code, Qnil));
}

/* NOTE!!! Every function that can call EVAL must protect its args
   and temporaries from garbage collection while it needs them.
   The definition of `For' shows what you have to do.  */

DEFUN ("or", For, Sor, 0, UNEVALLED, 0,
       doc: /* Eval args until one of them yields non-nil, then return that value.
The remaining args are not evalled at all.
If all args return nil, return nil.
usage: (or CONDITIONS ...)  */)
     (args)
     Lisp_Object args;
{
  register Lisp_Object val = Qnil;
  struct gcpro gcpro1;

  GCPRO1 (args);

  while (CONSP (args))
    {
      val = Feval (XCAR (args));
      if (!NILP (val))
	break;
      args = XCDR (args);
    }

  UNGCPRO;
  return val;
}

DEFUN ("and", Fand, Sand, 0, UNEVALLED, 0,
       doc: /* Eval args until one of them yields nil, then return nil.
The remaining args are not evalled at all.
If no arg yields nil, return the last arg's value.
usage: (and CONDITIONS ...)  */)
     (args)
     Lisp_Object args;
{
  register Lisp_Object val = Qt;
  struct gcpro gcpro1;

  GCPRO1 (args);

  while (CONSP (args))
    {
      val = Feval (XCAR (args));
      if (NILP (val))
	break;
      args = XCDR (args);
    }

  UNGCPRO;
  return val;
}

DEFUN ("if", Fif, Sif, 2, UNEVALLED, 0,
       doc: /* If COND yields non-nil, do THEN, else do ELSE...
Returns the value of THEN or the value of the last of the ELSE's.
THEN must be one expression, but ELSE... can be zero or more expressions.
If COND yields nil, and there are no ELSE's, the value is nil.
usage: (if COND THEN ELSE...)  */)
     (args)
     Lisp_Object args;
{
  register Lisp_Object cond;
  struct gcpro gcpro1;

  GCPRO1 (args);
  cond = Feval (Fcar (args));
  UNGCPRO;

  if (!NILP (cond))
    return Feval (Fcar (Fcdr (args)));
  return Fprogn (Fcdr (Fcdr (args)));
}

DEFUN ("cond", Fcond, Scond, 0, UNEVALLED, 0,
       doc: /* Try each clause until one succeeds.
Each clause looks like (CONDITION BODY...).  CONDITION is evaluated
and, if the value is non-nil, this clause succeeds:
then the expressions in BODY are evaluated and the last one's
value is the value of the cond-form.
If no clause succeeds, cond returns nil.
If a clause has one element, as in (CONDITION),
CONDITION's value if non-nil is returned from the cond-form.
usage: (cond CLAUSES...)  */)
     (args)
     Lisp_Object args;
{
  register Lisp_Object clause, val;
  struct gcpro gcpro1;

  val = Qnil;
  GCPRO1 (args);
  while (!NILP (args))
    {
      clause = Fcar (args);
      val = Feval (Fcar (clause));
      if (!NILP (val))
	{
	  if (!EQ (XCDR (clause), Qnil))
	    val = Fprogn (XCDR (clause));
	  break;
	}
      args = XCDR (args);
    }
  UNGCPRO;

  return val;
}

DEFUN ("progn", Fprogn, Sprogn, 0, UNEVALLED, 0,
       doc: /* Eval BODY forms sequentially and return value of last one.
usage: (progn BODY ...)  */)
     (args)
     Lisp_Object args;
{
  register Lisp_Object val = Qnil;
  struct gcpro gcpro1;

  GCPRO1 (args);

  while (CONSP (args))
    {
      val = Feval (XCAR (args));
      args = XCDR (args);
    }

  UNGCPRO;
  return val;
}

DEFUN ("prog1", Fprog1, Sprog1, 1, UNEVALLED, 0,
       doc: /* Eval FIRST and BODY sequentially; value from FIRST.
The value of FIRST is saved during the evaluation of the remaining args,
whose values are discarded.
usage: (prog1 FIRST BODY...)  */)
     (args)
     Lisp_Object args;
{
  Lisp_Object val;
  register Lisp_Object args_left;
  struct gcpro gcpro1, gcpro2;
  register int argnum = 0;

  if (NILP(args))
    return Qnil;

  args_left = args;
  val = Qnil;
  GCPRO2 (args, val);

  do
    {
      if (!(argnum++))
        val = Feval (Fcar (args_left));
      else
	Feval (Fcar (args_left));
      args_left = Fcdr (args_left);
    }
  while (!NILP(args_left));

  UNGCPRO;
  return val;
}

DEFUN ("prog2", Fprog2, Sprog2, 2, UNEVALLED, 0,
       doc: /* Eval FORM1, FORM2 and BODY sequentially; value from FORM2.
The value of FORM2 is saved during the evaluation of the
remaining args, whose values are discarded.
usage: (prog2 FORM1 FORM2 BODY...)  */)
     (args)
     Lisp_Object args;
{
  Lisp_Object val;
  register Lisp_Object args_left;
  struct gcpro gcpro1, gcpro2;
  register int argnum = -1;

  val = Qnil;

  if (NILP (args))
    return Qnil;

  args_left = args;
  val = Qnil;
  GCPRO2 (args, val);

  do
    {
      if (!(argnum++))
        val = Feval (Fcar (args_left));
      else
	Feval (Fcar (args_left));
      args_left = Fcdr (args_left);
    }
  while (!NILP (args_left));

  UNGCPRO;
  return val;
}

DEFUN ("setq", Fsetq, Ssetq, 0, UNEVALLED, 0,
       doc: /* Set each SYM to the value of its VAL.
The symbols SYM are variables; they are literal (not evaluated).
The values VAL are expressions; they are evaluated.
Thus, (setq x (1+ y)) sets `x' to the value of `(1+ y)'.
The second VAL is not computed until after the first SYM is set, and so on;
each VAL can use the new value of variables set earlier in the `setq'.
The return value of the `setq' form is the value of the last VAL.
usage: (setq SYM VAL SYM VAL ...)  */)
     (args)
     Lisp_Object args;
{
  register Lisp_Object args_left;
  register Lisp_Object val, sym;
  struct gcpro gcpro1;

  if (NILP(args))
    return Qnil;

  args_left = args;
  GCPRO1 (args);

  do
    {
      val = Feval (Fcar (Fcdr (args_left)));
      sym = Fcar (args_left);
      Fset (sym, val);
      args_left = Fcdr (Fcdr (args_left));
    }
  while (!NILP(args_left));

  UNGCPRO;
  return val;
}

DEFUN ("quote", Fquote, Squote, 1, UNEVALLED, 0,
       doc: /* Return the argument, without evaluating it.  `(quote x)' yields `x'.
usage: (quote ARG)  */)
     (args)
     Lisp_Object args;
{
  return Fcar (args);
}

DEFUN ("function", Ffunction, Sfunction, 1, UNEVALLED, 0,
       doc: /* Like `quote', but preferred for objects which are functions.
In byte compilation, `function' causes its argument to be compiled.
`quote' cannot do that.
usage: (function ARG)  */)
     (args)
     Lisp_Object args;
{
  return Fcar (args);
}


DEFUN ("interactive-p", Finteractive_p, Sinteractive_p, 0, 0, 0,
       doc: /* Return t if the function was run directly by user input.
This means that the function was called with `call-interactively'
\(which includes being called as the binding of a key)
and input is currently coming from the keyboard (not in keyboard macro),
and Emacs is not running in batch mode (`noninteractive' is nil).

The only known proper use of `interactive-p' is in deciding whether to
display a helpful message, or how to display it.  If you're thinking
of using it for any other purpose, it is quite likely that you're
making a mistake.  Think: what do you want to do when the command is
called from a keyboard macro?

If you want to test whether your function was called with
`call-interactively', the way to do that is by adding an extra
optional argument, and making the `interactive' spec specify non-nil
unconditionally for that argument.  (`p' is a good way to do this.)  */)
     ()
{
  return (INTERACTIVE && interactive_p (1)) ? Qt : Qnil;
}


DEFUN ("called-interactively-p", Fcalled_interactively_p, Scalled_interactively_p, 0, 0, 0,
       doc: /* Return t if the function using this was called with `call-interactively'.
This is used for implementing advice and other function-modifying
features of Emacs.

The cleanest way to test whether your function was called with
`call-interactively' is by adding an extra optional argument,
and making the `interactive' spec specify non-nil unconditionally
for that argument.  (`p' is a good way to do this.)  */)
     ()
{
  return interactive_p (1) ? Qt : Qnil;
}


/*  Return 1 if function in which this appears was called using
    call-interactively.

    EXCLUDE_SUBRS_P non-zero means always return 0 if the function
    called is a built-in.  */

int
interactive_p (exclude_subrs_p)
     int exclude_subrs_p;
{
  struct backtrace *btp;
  Lisp_Object fun;

  btp = backtrace_list;

  /* If this isn't a byte-compiled function, there may be a frame at
     the top for Finteractive_p.  If so, skip it.  */
  fun = Findirect_function (*btp->function, Qnil);
  if (SUBRP (fun) && (XSUBR (fun) == &Sinteractive_p
		      || XSUBR (fun) == &Scalled_interactively_p))
    btp = btp->next;

  /* If we're running an Emacs 18-style byte-compiled function, there
     may be a frame for Fbytecode at the top level.  In any version of
     Emacs there can be Fbytecode frames for subexpressions evaluated
     inside catch and condition-case.  Skip past them.

     If this isn't a byte-compiled function, then we may now be
     looking at several frames for special forms.  Skip past them.  */
  while (btp
	 && (EQ (*btp->function, Qbytecode)
	     || btp->nargs == UNEVALLED))
    btp = btp->next;

  /* btp now points at the frame of the innermost function that isn't
     a special form, ignoring frames for Finteractive_p and/or
     Fbytecode at the top.  If this frame is for a built-in function
     (such as load or eval-region) return nil.  */
  fun = Findirect_function (*btp->function, Qnil);
  if (exclude_subrs_p && SUBRP (fun))
    return 0;

  /* btp points to the frame of a Lisp function that called interactive-p.
     Return t if that function was called interactively.  */
  if (btp && btp->next && EQ (*btp->next->function, Qcall_interactively))
    return 1;
  return 0;
}


DEFUN ("defun", Fdefun, Sdefun, 2, UNEVALLED, 0,
       doc: /* Define NAME as a function.
The definition is (lambda ARGLIST [DOCSTRING] BODY...).
See also the function `interactive'.
usage: (defun NAME ARGLIST [DOCSTRING] BODY...)  */)
     (args)
     Lisp_Object args;
{
  register Lisp_Object fn_name;
  register Lisp_Object defn;

  fn_name = Fcar (args);
  CHECK_SYMBOL (fn_name);
  defn = Fcons (Qlambda, Fcdr (args));
  if (!NILP (Vpurify_flag))
    defn = Fpurecopy (defn);
  if (CONSP (XSYMBOL (fn_name)->function)
      && EQ (XCAR (XSYMBOL (fn_name)->function), Qautoload))
    LOADHIST_ATTACH (Fcons (Qt, fn_name));
  Ffset (fn_name, defn);
  LOADHIST_ATTACH (Fcons (Qdefun, fn_name));
  return fn_name;
}

DEFUN ("defmacro", Fdefmacro, Sdefmacro, 2, UNEVALLED, 0,
       doc: /* Define NAME as a macro.
The actual definition looks like
 (macro lambda ARGLIST [DOCSTRING] [DECL] BODY...).
When the macro is called, as in (NAME ARGS...),
the function (lambda ARGLIST BODY...) is applied to
the list ARGS... as it appears in the expression,
and the result should be a form to be evaluated instead of the original.

DECL is a declaration, optional, which can specify how to indent
calls to this macro and how Edebug should handle it.  It looks like this:
  (declare SPECS...)
The elements can look like this:
  (indent INDENT)
	Set NAME's `lisp-indent-function' property to INDENT.

  (debug DEBUG)
	Set NAME's `edebug-form-spec' property to DEBUG.  (This is
	equivalent to writing a `def-edebug-spec' for the macro.)
usage: (defmacro NAME ARGLIST [DOCSTRING] [DECL] BODY...)  */)
     (args)
     Lisp_Object args;
{
  register Lisp_Object fn_name;
  register Lisp_Object defn;
  Lisp_Object lambda_list, doc, tail;

  fn_name = Fcar (args);
  CHECK_SYMBOL (fn_name);
  lambda_list = Fcar (Fcdr (args));
  tail = Fcdr (Fcdr (args));

  doc = Qnil;
  if (STRINGP (Fcar (tail)))
    {
      doc = XCAR (tail);
      tail = XCDR (tail);
    }

  while (CONSP (Fcar (tail))
	 && EQ (Fcar (Fcar (tail)), Qdeclare))
    {
      if (!NILP (Vmacro_declaration_function))
	{
	  struct gcpro gcpro1;
	  GCPRO1 (args);
	  call2 (Vmacro_declaration_function, fn_name, Fcar (tail));
	  UNGCPRO;
	}

      tail = Fcdr (tail);
    }

  if (NILP (doc))
    tail = Fcons (lambda_list, tail);
  else
    tail = Fcons (lambda_list, Fcons (doc, tail));
  defn = Fcons (Qmacro, Fcons (Qlambda, tail));

  if (!NILP (Vpurify_flag))
    defn = Fpurecopy (defn);
  if (CONSP (XSYMBOL (fn_name)->function)
      && EQ (XCAR (XSYMBOL (fn_name)->function), Qautoload))
    LOADHIST_ATTACH (Fcons (Qt, fn_name));
  Ffset (fn_name, defn);
  LOADHIST_ATTACH (Fcons (Qdefun, fn_name));
  return fn_name;
}


DEFUN ("defvaralias", Fdefvaralias, Sdefvaralias, 2, 3, 0,
       doc: /* Make NEW-ALIAS a variable alias for symbol BASE-VARIABLE.
Aliased variables always have the same value; setting one sets the other.
Third arg DOCSTRING, if non-nil, is documentation for NEW-ALIAS.  If it is
 omitted or nil, NEW-ALIAS gets the documentation string of BASE-VARIABLE,
 or of the variable at the end of the chain of aliases, if BASE-VARIABLE is
 itself an alias.
The return value is BASE-VARIABLE.  */)
     (new_alias, base_variable, docstring)
     Lisp_Object new_alias, base_variable, docstring;
{
  struct Lisp_Symbol *sym;

  CHECK_SYMBOL (new_alias);
  CHECK_SYMBOL (base_variable);

  if (SYMBOL_CONSTANT_P (new_alias))
    error ("Cannot make a constant an alias");

  sym = XSYMBOL (new_alias);
  sym->indirect_variable = 1;
  sym->value = base_variable;
  sym->constant = SYMBOL_CONSTANT_P (base_variable);
  LOADHIST_ATTACH (new_alias);
  if (!NILP (docstring))
    Fput (new_alias, Qvariable_documentation, docstring);
  else
    Fput (new_alias, Qvariable_documentation, Qnil);

  return base_variable;
}


DEFUN ("defvar", Fdefvar, Sdefvar, 1, UNEVALLED, 0,
       doc: /* Define SYMBOL as a variable, and return SYMBOL.
You are not required to define a variable in order to use it,
but the definition can supply documentation and an initial value
in a way that tags can recognize.

INITVALUE is evaluated, and used to set SYMBOL, only if SYMBOL's value is void.
If SYMBOL is buffer-local, its default value is what is set;
 buffer-local values are not affected.
INITVALUE and DOCSTRING are optional.
If DOCSTRING starts with *, this variable is identified as a user option.
 This means that M-x set-variable recognizes it.
 See also `user-variable-p'.
If INITVALUE is missing, SYMBOL's value is not set.

If SYMBOL has a local binding, then this form affects the local
binding.  This is usually not what you want.  Thus, if you need to
load a file defining variables, with this form or with `defconst' or
`defcustom', you should always load that file _outside_ any bindings
for these variables.  \(`defconst' and `defcustom' behave similarly in
this respect.)
usage: (defvar SYMBOL &optional INITVALUE DOCSTRING)  */)
     (args)
     Lisp_Object args;
{
  register Lisp_Object sym, tem, tail;

  sym = Fcar (args);
  tail = Fcdr (args);
  if (!NILP (Fcdr (Fcdr (tail))))
    error ("Too many arguments");

  tem = Fdefault_boundp (sym);
  if (!NILP (tail))
    {
      if (SYMBOL_CONSTANT_P (sym))
	{
	  /* For upward compatibility, allow (defvar :foo (quote :foo)).  */
	  Lisp_Object tem = Fcar (tail);
	  if (! (CONSP (tem)
		 && EQ (XCAR (tem), Qquote)
		 && CONSP (XCDR (tem))
		 && EQ (XCAR (XCDR (tem)), sym)))
	    error ("Constant symbol `%s' specified in defvar",
		   SDATA (SYMBOL_NAME (sym)));
	}

      if (NILP (tem))
	Fset_default (sym, Feval (Fcar (tail)));
      else
	{ /* Check if there is really a global binding rather than just a let
	     binding that shadows the global unboundness of the var.  */
	  volatile struct specbinding *pdl = specpdl_ptr;
	  while (--pdl >= specpdl)
	    {
	      if (EQ (pdl->symbol, sym) && !pdl->func
		  && EQ (pdl->old_value, Qunbound))
		{
		  message_with_string ("Warning: defvar ignored because %s is let-bound",
				       SYMBOL_NAME (sym), 1);
		  break;
		}
	    }
	}
      tail = Fcdr (tail);
      tem = Fcar (tail);
      if (!NILP (tem))
	{
	  if (!NILP (Vpurify_flag))
	    tem = Fpurecopy (tem);
	  Fput (sym, Qvariable_documentation, tem);
	}
      LOADHIST_ATTACH (sym);
    }
  else
    /* Simple (defvar <var>) should not count as a definition at all.
       It could get in the way of other definitions, and unloading this
       package could try to make the variable unbound.  */
    ;

  return sym;
}

DEFUN ("defconst", Fdefconst, Sdefconst, 2, UNEVALLED, 0,
       doc: /* Define SYMBOL as a constant variable.
The intent is that neither programs nor users should ever change this value.
Always sets the value of SYMBOL to the result of evalling INITVALUE.
If SYMBOL is buffer-local, its default value is what is set;
 buffer-local values are not affected.
DOCSTRING is optional.

If SYMBOL has a local binding, then this form sets the local binding's
value.  However, you should normally not make local bindings for
variables defined with this form.
usage: (defconst SYMBOL INITVALUE [DOCSTRING])  */)
     (args)
     Lisp_Object args;
{
  register Lisp_Object sym, tem;

  sym = Fcar (args);
  if (!NILP (Fcdr (Fcdr (Fcdr (args)))))
    error ("Too many arguments");

  tem = Feval (Fcar (Fcdr (args)));
  if (!NILP (Vpurify_flag))
    tem = Fpurecopy (tem);
  Fset_default (sym, tem);
  tem = Fcar (Fcdr (Fcdr (args)));
  if (!NILP (tem))
    {
      if (!NILP (Vpurify_flag))
	tem = Fpurecopy (tem);
      Fput (sym, Qvariable_documentation, tem);
    }
  Fput (sym, Qrisky_local_variable, Qt);
  LOADHIST_ATTACH (sym);
  return sym;
}

/* Error handler used in Fuser_variable_p.  */
static Lisp_Object
user_variable_p_eh (ignore)
     Lisp_Object ignore;
{
  return Qnil;
}

DEFUN ("user-variable-p", Fuser_variable_p, Suser_variable_p, 1, 1, 0,
       doc: /* Return t if VARIABLE is intended to be set and modified by users.
\(The alternative is a variable used internally in a Lisp program.)
A variable is a user variable if
\(1) the first character of its documentation is `*', or
\(2) it is customizable (its property list contains a non-nil value
    of `standard-value' or `custom-autoload'), or
\(3) it is an alias for another user variable.
Return nil if VARIABLE is an alias and there is a loop in the
chain of symbols.  */)
     (variable)
     Lisp_Object variable;
{
  Lisp_Object documentation;

  if (!SYMBOLP (variable))
      return Qnil;

  /* If indirect and there's an alias loop, don't check anything else.  */
  if (XSYMBOL (variable)->indirect_variable
      && NILP (internal_condition_case_1 (indirect_variable, variable,
                                          Qt, user_variable_p_eh)))
    return Qnil;

  while (1)
    {
      documentation = Fget (variable, Qvariable_documentation);
      if (INTEGERP (documentation) && XINT (documentation) < 0)
        return Qt;
      if (STRINGP (documentation)
          && ((unsigned char) SREF (documentation, 0) == '*'))
        return Qt;
      /* If it is (STRING . INTEGER), a negative integer means a user variable.  */
      if (CONSP (documentation)
          && STRINGP (XCAR (documentation))
          && INTEGERP (XCDR (documentation))
          && XINT (XCDR (documentation)) < 0)
        return Qt;
      /* Customizable?  See `custom-variable-p'.  */
      if ((!NILP (Fget (variable, intern ("standard-value"))))
          || (!NILP (Fget (variable, intern ("custom-autoload")))))
        return Qt;

      if (!XSYMBOL (variable)->indirect_variable)
        return Qnil;

      /* An indirect variable?  Let's follow the chain.  */
      variable = XSYMBOL (variable)->value;
    }
}

DEFUN ("let*", FletX, SletX, 1, UNEVALLED, 0,
       doc: /* Bind variables according to VARLIST then eval BODY.
The value of the last form in BODY is returned.
Each element of VARLIST is a symbol (which is bound to nil)
or a list (SYMBOL VALUEFORM) (which binds SYMBOL to the value of VALUEFORM).
Each VALUEFORM can refer to the symbols already bound by this VARLIST.
usage: (let* VARLIST BODY...)  */)
     (args)
     Lisp_Object args;
{
  Lisp_Object varlist, val, elt;
  int count = SPECPDL_INDEX ();
  struct gcpro gcpro1, gcpro2, gcpro3;

  GCPRO3 (args, elt, varlist);

  varlist = Fcar (args);
  while (!NILP (varlist))
    {
      QUIT;
      elt = Fcar (varlist);
      if (SYMBOLP (elt))
	specbind (elt, Qnil);
      else if (! NILP (Fcdr (Fcdr (elt))))
	signal_error ("`let' bindings can have only one value-form", elt);
      else
	{
	  val = Feval (Fcar (Fcdr (elt)));
	  specbind (Fcar (elt), val);
	}
      varlist = Fcdr (varlist);
    }
  UNGCPRO;
  val = Fprogn (Fcdr (args));
  return unbind_to (count, val);
}

DEFUN ("let", Flet, Slet, 1, UNEVALLED, 0,
       doc: /* Bind variables according to VARLIST then eval BODY.
The value of the last form in BODY is returned.
Each element of VARLIST is a symbol (which is bound to nil)
or a list (SYMBOL VALUEFORM) (which binds SYMBOL to the value of VALUEFORM).
All the VALUEFORMs are evalled before any symbols are bound.
usage: (let VARLIST BODY...)  */)
     (args)
     Lisp_Object args;
{
  Lisp_Object *temps, tem;
  register Lisp_Object elt, varlist;
  int count = SPECPDL_INDEX ();
  register int argnum;
  struct gcpro gcpro1, gcpro2;

  varlist = Fcar (args);

  /* Make space to hold the values to give the bound variables */
  elt = Flength (varlist);
  temps = (Lisp_Object *) alloca (XFASTINT (elt) * sizeof (Lisp_Object));

  /* Compute the values and store them in `temps' */

  GCPRO2 (args, *temps);
  gcpro2.nvars = 0;

  for (argnum = 0; !NILP (varlist); varlist = Fcdr (varlist))
    {
      QUIT;
      elt = Fcar (varlist);
      if (SYMBOLP (elt))
	temps [argnum++] = Qnil;
      else if (! NILP (Fcdr (Fcdr (elt))))
	signal_error ("`let' bindings can have only one value-form", elt);
      else
	temps [argnum++] = Feval (Fcar (Fcdr (elt)));
      gcpro2.nvars = argnum;
    }
  UNGCPRO;

  varlist = Fcar (args);
  for (argnum = 0; !NILP (varlist); varlist = Fcdr (varlist))
    {
      elt = Fcar (varlist);
      tem = temps[argnum++];
      if (SYMBOLP (elt))
	specbind (elt, tem);
      else
	specbind (Fcar (elt), tem);
    }

  elt = Fprogn (Fcdr (args));
  return unbind_to (count, elt);
}

DEFUN ("while", Fwhile, Swhile, 1, UNEVALLED, 0,
       doc: /* If TEST yields non-nil, eval BODY... and repeat.
The order of execution is thus TEST, BODY, TEST, BODY and so on
until TEST returns nil.
usage: (while TEST BODY...)  */)
     (args)
     Lisp_Object args;
{
  Lisp_Object test, body;
  struct gcpro gcpro1, gcpro2;

  GCPRO2 (test, body);

  test = Fcar (args);
  body = Fcdr (args);
  while (!NILP (Feval (test)))
    {
      QUIT;
      Fprogn (body);
    }

  UNGCPRO;
  return Qnil;
}

DEFUN ("macroexpand", Fmacroexpand, Smacroexpand, 1, 2, 0,
       doc: /* Return result of expanding macros at top level of FORM.
If FORM is not a macro call, it is returned unchanged.
Otherwise, the macro is expanded and the expansion is considered
in place of FORM.  When a non-macro-call results, it is returned.

The second optional arg ENVIRONMENT specifies an environment of macro
definitions to shadow the loaded ones for use in file byte-compilation.  */)
     (form, environment)
     Lisp_Object form;
     Lisp_Object environment;
{
  /* With cleanups from Hallvard Furuseth.  */
  register Lisp_Object expander, sym, def, tem;

  while (1)
    {
      /* Come back here each time we expand a macro call,
	 in case it expands into another macro call.  */
      if (!CONSP (form))
	break;
      /* Set SYM, give DEF and TEM right values in case SYM is not a symbol. */
      def = sym = XCAR (form);
      tem = Qnil;
      /* Trace symbols aliases to other symbols
	 until we get a symbol that is not an alias.  */
      while (SYMBOLP (def))
	{
	  QUIT;
	  sym = def;
	  tem = Fassq (sym, environment);
	  if (NILP (tem))
	    {
	      def = XSYMBOL (sym)->function;
	      if (!EQ (def, Qunbound))
		continue;
	    }
	  break;
	}
      /* Right now TEM is the result from SYM in ENVIRONMENT,
	 and if TEM is nil then DEF is SYM's function definition.  */
      if (NILP (tem))
	{
	  /* SYM is not mentioned in ENVIRONMENT.
	     Look at its function definition.  */
	  if (EQ (def, Qunbound) || !CONSP (def))
	    /* Not defined or definition not suitable */
	    break;
	  if (EQ (XCAR (def), Qautoload))
	    {
	      /* Autoloading function: will it be a macro when loaded?  */
	      tem = Fnth (make_number (4), def);
	      if (EQ (tem, Qt) || EQ (tem, Qmacro))
		/* Yes, load it and try again.  */
		{
		  struct gcpro gcpro1;
		  GCPRO1 (form);
		  do_autoload (def, sym);
		  UNGCPRO;
		  continue;
		}
	      else
		break;
	    }
	  else if (!EQ (XCAR (def), Qmacro))
	    break;
	  else expander = XCDR (def);
	}
      else
	{
	  expander = XCDR (tem);
	  if (NILP (expander))
	    break;
	}
      form = apply1 (expander, XCDR (form));
    }
  return form;
}

DEFUN ("catch", Fcatch, Scatch, 1, UNEVALLED, 0,
       doc: /* Eval BODY allowing nonlocal exits using `throw'.
TAG is evalled to get the tag to use; it must not be nil.

Then the BODY is executed.
Within BODY, a call to `throw' with the same TAG exits BODY and this `catch'.
If no throw happens, `catch' returns the value of the last BODY form.
If a throw happens, it specifies the value to return from `catch'.
usage: (catch TAG BODY...)  */)
     (args)
     Lisp_Object args;
{
  register Lisp_Object tag;
  struct gcpro gcpro1;

  GCPRO1 (args);
  tag = Feval (Fcar (args));
  UNGCPRO;
  return internal_catch (tag, Fprogn, Fcdr (args));
}

/* Set up a catch, then call C function FUNC on argument ARG.
   FUNC should return a Lisp_Object.
   This is how catches are done from within C code. */

Lisp_Object
internal_catch (tag, func, arg)
     Lisp_Object tag;
     Lisp_Object (*func) ();
     Lisp_Object arg;
{
  /* This structure is made part of the chain `catchlist'.  */
  volatile struct catchtag c;

  /* Fill in the components of c, and put it on the list.  */
  c.next = catchlist;
  c.tag = tag;
  c.val = Qnil;
  c.backlist = backtrace_list;
  c.handlerlist = handlerlist;
  c.lisp_eval_depth = lisp_eval_depth;
  c.pdlcount = SPECPDL_INDEX ();
  c.poll_suppress_count = poll_suppress_count;
  c.interrupt_input_blocked = interrupt_input_blocked;
  c.gcpro = gcprolist;
  c.byte_stack = byte_stack_list;
  catchlist = &c;

  /* Call FUNC.  */
  if (! _setjmp (c.jmp))
    c.val = (*func) (arg);

  /* Throw works by a longjmp that comes right here.  */
  catchlist = c.next;
  return c.val;
}

/* Unwind the specbind, catch, and handler stacks back to CATCH, and
   jump to that CATCH, returning VALUE as the value of that catch.

   This is the guts Fthrow and Fsignal; they differ only in the way
   they choose the catch tag to throw to.  A catch tag for a
   condition-case form has a TAG of Qnil.

   Before each catch is discarded, unbind all special bindings and
   execute all unwind-protect clauses made above that catch.  Unwind
   the handler stack as we go, so that the proper handlers are in
   effect for each unwind-protect clause we run.  At the end, restore
   some static info saved in CATCH, and longjmp to the location
   specified in the

   This is used for correct unwinding in Fthrow and Fsignal.  */

static void
unwind_to_catch (catch, value)
     struct catchtag *catch;
     Lisp_Object value;
{
  register int last_time;

  /* Save the value in the tag.  */
  catch->val = value;

  /* Restore certain special C variables.  */
  set_poll_suppress_count (catch->poll_suppress_count);
  UNBLOCK_INPUT_TO (catch->interrupt_input_blocked);
  handling_signal = 0;
  immediate_quit = 0;

  do
    {
      last_time = catchlist == catch;

      /* Unwind the specpdl stack, and then restore the proper set of
         handlers.  */
      unbind_to (catchlist->pdlcount, Qnil);
      handlerlist = catchlist->handlerlist;
      catchlist = catchlist->next;
    }
  while (! last_time);

#if HAVE_X_WINDOWS
  /* If x_catch_errors was done, turn it off now.
     (First we give unbind_to a chance to do that.)  */
  x_fully_uncatch_errors ();
#endif

  byte_stack_list = catch->byte_stack;
  gcprolist = catch->gcpro;
#ifdef DEBUG_GCPRO
  if (gcprolist != 0)
    gcpro_level = gcprolist->level + 1;
  else
    gcpro_level = 0;
#endif
  backtrace_list = catch->backlist;
  lisp_eval_depth = catch->lisp_eval_depth;

  _longjmp (catch->jmp, 1);
}

DEFUN ("throw", Fthrow, Sthrow, 2, 2, 0,
       doc: /* Throw to the catch for TAG and return VALUE from it.
Both TAG and VALUE are evalled.  */)
     (tag, value)
     register Lisp_Object tag, value;
{
  register struct catchtag *c;

  if (!NILP (tag))
    for (c = catchlist; c; c = c->next)
      {
	if (EQ (c->tag, tag))
	  unwind_to_catch (c, value);
      }
  xsignal2 (Qno_catch, tag, value);
}


DEFUN ("unwind-protect", Funwind_protect, Sunwind_protect, 1, UNEVALLED, 0,
       doc: /* Do BODYFORM, protecting with UNWINDFORMS.
If BODYFORM completes normally, its value is returned
after executing the UNWINDFORMS.
If BODYFORM exits nonlocally, the UNWINDFORMS are executed anyway.
usage: (unwind-protect BODYFORM UNWINDFORMS...)  */)
     (args)
     Lisp_Object args;
{
  Lisp_Object val;
  int count = SPECPDL_INDEX ();

  record_unwind_protect (Fprogn, Fcdr (args));
  val = Feval (Fcar (args));
  return unbind_to (count, val);
}

/* Chain of condition handlers currently in effect.
   The elements of this chain are contained in the stack frames
   of Fcondition_case and internal_condition_case.
   When an error is signaled (by calling Fsignal, below),
   this chain is searched for an element that applies.  */

struct handler *handlerlist;

DEFUN ("condition-case", Fcondition_case, Scondition_case, 2, UNEVALLED, 0,
       doc: /* Regain control when an error is signaled.
Executes BODYFORM and returns its value if no error happens.
Each element of HANDLERS looks like (CONDITION-NAME BODY...)
where the BODY is made of Lisp expressions.

A handler is applicable to an error
if CONDITION-NAME is one of the error's condition names.
If an error happens, the first applicable handler is run.

The car of a handler may be a list of condition names
instead of a single condition name.

When a handler handles an error,
control returns to the condition-case and the handler BODY... is executed
with VAR bound to (SIGNALED-CONDITIONS . SIGNAL-DATA).
VAR may be nil; then you do not get access to the signal information.

The value of the last BODY form is returned from the condition-case.
See also the function `signal' for more info.
usage: (condition-case VAR BODYFORM &rest HANDLERS)  */)
     (args)
     Lisp_Object args;
{
  register Lisp_Object bodyform, handlers;
  volatile Lisp_Object var;

  var      = Fcar (args);
  bodyform = Fcar (Fcdr (args));
  handlers = Fcdr (Fcdr (args));

  return internal_lisp_condition_case (var, bodyform, handlers);
}

/* Like Fcondition_case, but the args are separate
   rather than passed in a list.  Used by Fbyte_code.  */

Lisp_Object
internal_lisp_condition_case (var, bodyform, handlers)
     volatile Lisp_Object var;
     Lisp_Object bodyform, handlers;
{
  Lisp_Object val;
  volatile struct catchtag c;
  volatile struct handler h;

  CHECK_SYMBOL (var);

  for (val = handlers; CONSP (val); val = XCDR (val))
    {
      Lisp_Object tem;
      tem = XCAR (val);
      if (! (NILP (tem)
	     || (CONSP (tem)
		 && (SYMBOLP (XCAR (tem))
		     || CONSP (XCAR (tem))))))
	error ("Invalid condition handler", tem);
    }

  c.tag = Qnil;
  c.val = Qnil;
  c.backlist = backtrace_list;
  c.handlerlist = handlerlist;
  c.lisp_eval_depth = lisp_eval_depth;
  c.pdlcount = SPECPDL_INDEX ();
  c.poll_suppress_count = poll_suppress_count;
  c.interrupt_input_blocked = interrupt_input_blocked;
  c.gcpro = gcprolist;
  c.byte_stack = byte_stack_list;
  if (_setjmp (c.jmp))
    {
      if (!NILP (h.var))
        specbind (h.var, c.val);
      val = Fprogn (Fcdr (h.chosen_clause));

      /* Note that this just undoes the binding of h.var; whoever
	 longjumped to us unwound the stack to c.pdlcount before
	 throwing. */
      unbind_to (c.pdlcount, Qnil);
      return val;
    }
  c.next = catchlist;
  catchlist = &c;

  h.var = var;
  h.handler = handlers;
  h.next = handlerlist;
  h.tag = &c;
  handlerlist = &h;

  val = Feval (bodyform);
  catchlist = c.next;
  handlerlist = h.next;
  return val;
}

/* Call the function BFUN with no arguments, catching errors within it
   according to HANDLERS.  If there is an error, call HFUN with
   one argument which is the data that describes the error:
   (SIGNALNAME . DATA)

   HANDLERS can be a list of conditions to catch.
   If HANDLERS is Qt, catch all errors.
   If HANDLERS is Qerror, catch all errors
   but allow the debugger to run if that is enabled.  */

Lisp_Object
internal_condition_case (bfun, handlers, hfun)
     Lisp_Object (*bfun) ();
     Lisp_Object handlers;
     Lisp_Object (*hfun) ();
{
  Lisp_Object val;
  volatile struct catchtag c;
  volatile struct handler h;

  /* Since Fsignal will close off all calls to x_catch_errors,
     we will get the wrong results if some are not closed now.  */
#if HAVE_X_WINDOWS
  if (x_catching_errors ())
    abort ();
#endif

  c.tag = Qnil;
  c.val = Qnil;
  c.backlist = backtrace_list;
  c.handlerlist = handlerlist;
  c.lisp_eval_depth = lisp_eval_depth;
  c.pdlcount = SPECPDL_INDEX ();
  c.poll_suppress_count = poll_suppress_count;
  c.interrupt_input_blocked = interrupt_input_blocked;
  c.gcpro = gcprolist;
  c.byte_stack = byte_stack_list;
  if (_setjmp (c.jmp))
    {
      return (*hfun) (c.val);
    }
  c.next = catchlist;
  catchlist = &c;
  h.handler = handlers;
  h.var = Qnil;
  h.next = handlerlist;
  h.tag = &c;
  handlerlist = &h;

  val = (*bfun) ();
  catchlist = c.next;
  handlerlist = h.next;
  return val;
}

/* Like internal_condition_case but call BFUN with ARG as its argument.  */

Lisp_Object
internal_condition_case_1 (bfun, arg, handlers, hfun)
     Lisp_Object (*bfun) ();
     Lisp_Object arg;
     Lisp_Object handlers;
     Lisp_Object (*hfun) ();
{
  Lisp_Object val;
  volatile struct catchtag c;
  volatile struct handler h;

  /* Since Fsignal will close off all calls to x_catch_errors,
     we will get the wrong results if some are not closed now.  */
#if HAVE_X_WINDOWS
  if (x_catching_errors ())
    abort ();
#endif

  c.tag = Qnil;
  c.val = Qnil;
  c.backlist = backtrace_list;
  c.handlerlist = handlerlist;
  c.lisp_eval_depth = lisp_eval_depth;
  c.pdlcount = SPECPDL_INDEX ();
  c.poll_suppress_count = poll_suppress_count;
  c.interrupt_input_blocked = interrupt_input_blocked;
  c.gcpro = gcprolist;
  c.byte_stack = byte_stack_list;
  if (_setjmp (c.jmp))
    {
      return (*hfun) (c.val);
    }
  c.next = catchlist;
  catchlist = &c;
  h.handler = handlers;
  h.var = Qnil;
  h.next = handlerlist;
  h.tag = &c;
  handlerlist = &h;

  val = (*bfun) (arg);
  catchlist = c.next;
  handlerlist = h.next;
  return val;
}


/* Like internal_condition_case but call BFUN with NARGS as first,
   and ARGS as second argument.  */

Lisp_Object
internal_condition_case_2 (bfun, nargs, args, handlers, hfun)
     Lisp_Object (*bfun) ();
     int nargs;
     Lisp_Object *args;
     Lisp_Object handlers;
     Lisp_Object (*hfun) ();
{
  Lisp_Object val;
  volatile struct catchtag c;
  volatile struct handler h;

  /* Since Fsignal will close off all calls to x_catch_errors,
     we will get the wrong results if some are not closed now.  */
#if HAVE_X_WINDOWS
  if (x_catching_errors ())
    abort ();
#endif

  c.tag = Qnil;
  c.val = Qnil;
  c.backlist = backtrace_list;
  c.handlerlist = handlerlist;
  c.lisp_eval_depth = lisp_eval_depth;
  c.pdlcount = SPECPDL_INDEX ();
  c.poll_suppress_count = poll_suppress_count;
  c.interrupt_input_blocked = interrupt_input_blocked;
  c.gcpro = gcprolist;
  c.byte_stack = byte_stack_list;
  if (_setjmp (c.jmp))
    {
      return (*hfun) (c.val);
    }
  c.next = catchlist;
  catchlist = &c;
  h.handler = handlers;
  h.var = Qnil;
  h.next = handlerlist;
  h.tag = &c;
  handlerlist = &h;

  val = (*bfun) (nargs, args);
  catchlist = c.next;
  handlerlist = h.next;
  return val;
}


static Lisp_Object find_handler_clause P_ ((Lisp_Object, Lisp_Object,
					    Lisp_Object, Lisp_Object,
					    Lisp_Object *));

DEFUN ("signal", Fsignal, Ssignal, 2, 2, 0,
       doc: /* Signal an error.  Args are ERROR-SYMBOL and associated DATA.
This function does not return.

An error symbol is a symbol with an `error-conditions' property
that is a list of condition names.
A handler for any of those names will get to handle this signal.
The symbol `error' should normally be one of them.

DATA should be a list.  Its elements are printed as part of the error message.
See Info anchor `(elisp)Definition of signal' for some details on how this
error message is constructed.
If the signal is handled, DATA is made available to the handler.
See also the function `condition-case'.  */)
     (error_symbol, data)
     Lisp_Object error_symbol, data;
{
  /* When memory is full, ERROR-SYMBOL is nil,
     and DATA is (REAL-ERROR-SYMBOL . REAL-DATA).
     That is a special case--don't do this in other situations.  */
  register struct handler *allhandlers = handlerlist;
  Lisp_Object conditions;
  extern int gc_in_progress;
  extern int waiting_for_input;
  Lisp_Object debugger_value;
  Lisp_Object string;
  Lisp_Object real_error_symbol;
  struct backtrace *bp;

  immediate_quit = handling_signal = 0;
  abort_on_gc = 0;
  if (gc_in_progress || waiting_for_input)
    abort ();

  if (NILP (error_symbol))
    real_error_symbol = Fcar (data);
  else
    real_error_symbol = error_symbol;

#if 0 /* rms: I don't know why this was here,
	 but it is surely wrong for an error that is handled.  */
#ifdef HAVE_X_WINDOWS
  if (display_hourglass_p)
    cancel_hourglass ();
#endif
#endif

  /* This hook is used by edebug.  */
  if (! NILP (Vsignal_hook_function)
      && ! NILP (error_symbol))
    {
      /* Edebug takes care of restoring these variables when it exits.  */
      if (lisp_eval_depth + 20 > max_lisp_eval_depth)
	max_lisp_eval_depth = lisp_eval_depth + 20;

      if (SPECPDL_INDEX () + 40 > max_specpdl_size)
	max_specpdl_size = SPECPDL_INDEX () + 40;

      call2 (Vsignal_hook_function, error_symbol, data);
    }

  conditions = Fget (real_error_symbol, Qerror_conditions);

  /* Remember from where signal was called.  Skip over the frame for
     `signal' itself.  If a frame for `error' follows, skip that,
     too.  Don't do this when ERROR_SYMBOL is nil, because that
     is a memory-full error.  */
  Vsignaling_function = Qnil;
  if (backtrace_list && !NILP (error_symbol))
    {
      bp = backtrace_list->next;
      if (bp && bp->function && EQ (*bp->function, Qerror))
	bp = bp->next;
      if (bp && bp->function)
	Vsignaling_function = *bp->function;
    }

  for (; handlerlist; handlerlist = handlerlist->next)
    {
      register Lisp_Object clause;

      clause = find_handler_clause (handlerlist->handler, conditions,
				    error_symbol, data, &debugger_value);

      if (EQ (clause, Qlambda))
	{
	  /* We can't return values to code which signaled an error, but we
	     can continue code which has signaled a quit.  */
	  if (EQ (real_error_symbol, Qquit))
	    return Qnil;
	  else
	    error ("Cannot return from the debugger in an error");
	}

      if (!NILP (clause))
	{
	  Lisp_Object unwind_data;
	  struct handler *h = handlerlist;

	  handlerlist = allhandlers;

	  if (NILP (error_symbol))
	    unwind_data = data;
	  else
	    unwind_data = Fcons (error_symbol, data);
	  h->chosen_clause = clause;
	  unwind_to_catch (h->tag, unwind_data);
	}
    }

  handlerlist = allhandlers;
  /* If no handler is present now, try to run the debugger,
     and if that fails, throw to top level.  */
  find_handler_clause (Qerror, conditions, error_symbol, data, &debugger_value);
  if (catchlist != 0)
    Fthrow (Qtop_level, Qt);

  if (! NILP (error_symbol))
    data = Fcons (error_symbol, data);

  string = Ferror_message_string (data);
  fatal ("%s", SDATA (string), 0);
}

/* Internal version of Fsignal that never returns.
   Used for anything but Qquit (which can return from Fsignal).  */

void
xsignal (error_symbol, data)
     Lisp_Object error_symbol, data;
{
  Fsignal (error_symbol, data);
  abort ();
}

/* Like xsignal, but takes 0, 1, 2, or 3 args instead of a list.  */

void
xsignal0 (error_symbol)
     Lisp_Object error_symbol;
{
  xsignal (error_symbol, Qnil);
}

void
xsignal1 (error_symbol, arg)
     Lisp_Object error_symbol, arg;
{
  xsignal (error_symbol, list1 (arg));
}

void
xsignal2 (error_symbol, arg1, arg2)
     Lisp_Object error_symbol, arg1, arg2;
{
  xsignal (error_symbol, list2 (arg1, arg2));
}

void
xsignal3 (error_symbol, arg1, arg2, arg3)
     Lisp_Object error_symbol, arg1, arg2, arg3;
{
  xsignal (error_symbol, list3 (arg1, arg2, arg3));
}

/* Signal `error' with message S, and additional arg ARG.
   If ARG is not a genuine list, make it a one-element list.  */

void
signal_error (s, arg)
     char *s;
     Lisp_Object arg;
{
  Lisp_Object tortoise, hare;

  hare = tortoise = arg;
  while (CONSP (hare))
    {
      hare = XCDR (hare);
      if (!CONSP (hare))
	break;

      hare = XCDR (hare);
      tortoise = XCDR (tortoise);

      if (EQ (hare, tortoise))
	break;
    }

  if (!NILP (hare))
    arg = Fcons (arg, Qnil);	/* Make it a list.  */

  xsignal (Qerror, Fcons (build_string (s), arg));
}


/* Return nonzero iff LIST is a non-nil atom or
   a list containing one of CONDITIONS.  */

static int
wants_debugger (list, conditions)
     Lisp_Object list, conditions;
{
  if (NILP (list))
    return 0;
  if (! CONSP (list))
    return 1;

  while (CONSP (conditions))
    {
      Lisp_Object this, tail;
      this = XCAR (conditions);
      for (tail = list; CONSP (tail); tail = XCDR (tail))
	if (EQ (XCAR (tail), this))
	  return 1;
      conditions = XCDR (conditions);
    }
  return 0;
}

/* Return 1 if an error with condition-symbols CONDITIONS,
   and described by SIGNAL-DATA, should skip the debugger
   according to debugger-ignored-errors.  */

static int
skip_debugger (conditions, data)
     Lisp_Object conditions, data;
{
  Lisp_Object tail;
  int first_string = 1;
  Lisp_Object error_message;

  error_message = Qnil;
  for (tail = Vdebug_ignored_errors; CONSP (tail); tail = XCDR (tail))
    {
      if (STRINGP (XCAR (tail)))
	{
	  if (first_string)
	    {
	      error_message = Ferror_message_string (data);
	      first_string = 0;
	    }

	  if (fast_string_match (XCAR (tail), error_message) >= 0)
	    return 1;
	}
      else
	{
	  Lisp_Object contail;

	  for (contail = conditions; CONSP (contail); contail = XCDR (contail))
	    if (EQ (XCAR (tail), XCAR (contail)))
	      return 1;
	}
    }

  return 0;
}

/* Value of Qlambda means we have called debugger and user has continued.
   There are two ways to pass SIG and DATA:
    = SIG is the error symbol, and DATA is the rest of the data.
    = SIG is nil, and DATA is (SYMBOL . REST-OF-DATA).
       This is for memory-full errors only.

   Store value returned from debugger into *DEBUGGER_VALUE_PTR.

   We need to increase max_specpdl_size temporarily around
   anything we do that can push on the specpdl, so as not to get
   a second error here in case we're handling specpdl overflow.  */

static Lisp_Object
find_handler_clause (handlers, conditions, sig, data, debugger_value_ptr)
     Lisp_Object handlers, conditions, sig, data;
     Lisp_Object *debugger_value_ptr;
{
  register Lisp_Object h;
  register Lisp_Object tem;

  if (EQ (handlers, Qt))  /* t is used by handlers for all conditions, set up by C code.  */
    return Qt;
  /* error is used similarly, but means print an error message
     and run the debugger if that is enabled.  */
  if (EQ (handlers, Qerror)
      || !NILP (Vdebug_on_signal)) /* This says call debugger even if
				      there is a handler.  */
    {
      int debugger_called = 0;
      Lisp_Object sig_symbol, combined_data;
      /* This is set to 1 if we are handling a memory-full error,
	 because these must not run the debugger.
	 (There is no room in memory to do that!)  */
      int no_debugger = 0;

      if (NILP (sig))
	{
	  combined_data = data;
	  sig_symbol = Fcar (data);
	  no_debugger = 1;
	}
      else
	{
	  combined_data = Fcons (sig, data);
	  sig_symbol = sig;
	}

      if (wants_debugger (Vstack_trace_on_error, conditions))
	{
	  max_specpdl_size++;
#ifdef PROTOTYPES
	  internal_with_output_to_temp_buffer ("*Backtrace*",
					       (Lisp_Object (*) (Lisp_Object)) Fbacktrace,
					       Qnil);
#else
	  internal_with_output_to_temp_buffer ("*Backtrace*",
					       Fbacktrace, Qnil);
#endif
	  max_specpdl_size--;
	}
      if (! no_debugger
	  /* Don't try to run the debugger with interrupts blocked.
	     The editing loop would return anyway.  */
	  && ! INPUT_BLOCKED_P
	  && (EQ (sig_symbol, Qquit)
	      ? debug_on_quit
	      : wants_debugger (Vdebug_on_error, conditions))
	  && ! skip_debugger (conditions, combined_data)
	  && when_entered_debugger < num_nonmacro_input_events)
	{
	  *debugger_value_ptr
	    = call_debugger (Fcons (Qerror,
				    Fcons (combined_data, Qnil)));
	  debugger_called = 1;
	}
      /* If there is no handler, return saying whether we ran the debugger.  */
      if (EQ (handlers, Qerror))
	{
	  if (debugger_called)
	    return Qlambda;
	  return Qt;
	}
    }
  for (h = handlers; CONSP (h); h = Fcdr (h))
    {
      Lisp_Object handler, condit;

      handler = Fcar (h);
      if (!CONSP (handler))
	continue;
      condit = Fcar (handler);
      /* Handle a single condition name in handler HANDLER.  */
      if (SYMBOLP (condit))
	{
	  tem = Fmemq (Fcar (handler), conditions);
	  if (!NILP (tem))
	    return handler;
	}
      /* Handle a list of condition names in handler HANDLER.  */
      else if (CONSP (condit))
	{
	  while (CONSP (condit))
	    {
	      tem = Fmemq (Fcar (condit), conditions);
	      if (!NILP (tem))
		return handler;
	      condit = XCDR (condit);
	    }
	}
    }
  return Qnil;
}

/* dump an error message; called like printf */

/* VARARGS 1 */
void
error (m, a1, a2, a3)
     char *m;
     char *a1, *a2, *a3;
{
  char buf[200];
  int size = 200;
  int mlen;
  char *buffer = buf;
  char *args[3];
  int allocated = 0;
  Lisp_Object string;

  args[0] = a1;
  args[1] = a2;
  args[2] = a3;

  mlen = strlen (m);

  while (1)
    {
      int used = doprnt (buffer, size, m, m + mlen, 3, args);
      if (used < size)
	break;
      size *= 2;
      if (allocated)
	buffer = (char *) xrealloc (buffer, size);
      else
	{
	  buffer = (char *) xmalloc (size);
	  allocated = 1;
	}
    }

  string = build_string (buffer);
  if (allocated)
    xfree (buffer);

  xsignal1 (Qerror, string);
}

DEFUN ("commandp", Fcommandp, Scommandp, 1, 2, 0,
       doc: /* Non-nil if FUNCTION makes provisions for interactive calling.
This means it contains a description for how to read arguments to give it.
The value is nil for an invalid function or a symbol with no function
definition.

Interactively callable functions include strings and vectors (treated
as keyboard macros), lambda-expressions that contain a top-level call
to `interactive', autoload definitions made by `autoload' with non-nil
fourth argument, and some of the built-in functions of Lisp.

Also, a symbol satisfies `commandp' if its function definition does so.

If the optional argument FOR-CALL-INTERACTIVELY is non-nil,
then strings and vectors are not accepted.  */)
     (function, for_call_interactively)
     Lisp_Object function, for_call_interactively;
{
  register Lisp_Object fun;
  register Lisp_Object funcar;

  fun = function;

  fun = indirect_function (fun);
  if (EQ (fun, Qunbound))
    return Qnil;

  /* Emacs primitives are interactive if their DEFUN specifies an
     interactive spec.  */
  if (SUBRP (fun))
    {
      if (XSUBR (fun)->prompt)
	return Qt;
      else
	return Qnil;
    }

  /* Bytecode objects are interactive if they are long enough to
     have an element whose index is COMPILED_INTERACTIVE, which is
     where the interactive spec is stored.  */
  else if (COMPILEDP (fun))
    return ((ASIZE (fun) & PSEUDOVECTOR_SIZE_MASK) > COMPILED_INTERACTIVE
	    ? Qt : Qnil);

  /* Strings and vectors are keyboard macros.  */
  if (NILP (for_call_interactively) && (STRINGP (fun) || VECTORP (fun)))
    return Qt;

  /* Lists may represent commands.  */
  if (!CONSP (fun))
    return Qnil;
  funcar = XCAR (fun);
  if (EQ (funcar, Qlambda))
    return Fassq (Qinteractive, Fcdr (XCDR (fun)));
  if (EQ (funcar, Qautoload))
    return Fcar (Fcdr (Fcdr (XCDR (fun))));
  else
    return Qnil;
}

/* ARGSUSED */
DEFUN ("autoload", Fautoload, Sautoload, 2, 5, 0,
       doc: /* Define FUNCTION to autoload from FILE.
FUNCTION is a symbol; FILE is a file name string to pass to `load'.
Third arg DOCSTRING is documentation for the function.
Fourth arg INTERACTIVE if non-nil says function can be called interactively.
Fifth arg TYPE indicates the type of the object:
   nil or omitted says FUNCTION is a function,
   `keymap' says FUNCTION is really a keymap, and
   `macro' or t says FUNCTION is really a macro.
Third through fifth args give info about the real definition.
They default to nil.
If FUNCTION is already defined other than as an autoload,
this does nothing and returns nil.  */)
     (function, file, docstring, interactive, type)
     Lisp_Object function, file, docstring, interactive, type;
{
#ifdef NO_ARG_ARRAY
  Lisp_Object args[4];
#endif

  CHECK_SYMBOL (function);
  CHECK_STRING (file);

  /* If function is defined and not as an autoload, don't override */
  if (!EQ (XSYMBOL (function)->function, Qunbound)
      && !(CONSP (XSYMBOL (function)->function)
	   && EQ (XCAR (XSYMBOL (function)->function), Qautoload)))
    return Qnil;

  if (NILP (Vpurify_flag))
    /* Only add entries after dumping, because the ones before are
       not useful and else we get loads of them from the loaddefs.el.  */
    LOADHIST_ATTACH (Fcons (Qautoload, function));

#ifdef NO_ARG_ARRAY
  args[0] = file;
  args[1] = docstring;
  args[2] = interactive;
  args[3] = type;

  return Ffset (function, Fcons (Qautoload, Flist (4, &args[0])));
#else /* NO_ARG_ARRAY */
  return Ffset (function, Fcons (Qautoload, Flist (4, &file)));
#endif /* not NO_ARG_ARRAY */
}

Lisp_Object
un_autoload (oldqueue)
     Lisp_Object oldqueue;
{
  register Lisp_Object queue, first, second;

  /* Queue to unwind is current value of Vautoload_queue.
     oldqueue is the shadowed value to leave in Vautoload_queue.  */
  queue = Vautoload_queue;
  Vautoload_queue = oldqueue;
  while (CONSP (queue))
    {
      first = XCAR (queue);
      second = Fcdr (first);
      first = Fcar (first);
      if (EQ (first, make_number (0)))
	Vfeatures = second;
      else
	Ffset (first, second);
      queue = XCDR (queue);
    }
  return Qnil;
}

/* Load an autoloaded function.
   FUNNAME is the symbol which is the function's name.
   FUNDEF is the autoload definition (a list).  */

void
do_autoload (fundef, funname)
     Lisp_Object fundef, funname;
{
  int count = SPECPDL_INDEX ();
  Lisp_Object fun, queue, first, second;
  struct gcpro gcpro1, gcpro2, gcpro3;

  /* This is to make sure that loadup.el gives a clear picture
     of what files are preloaded and when.  */
  if (! NILP (Vpurify_flag))
    error ("Attempt to autoload %s while preparing to dump",
	   SDATA (SYMBOL_NAME (funname)));

  fun = funname;
  CHECK_SYMBOL (funname);
  GCPRO3 (fun, funname, fundef);

  /* Preserve the match data.  */
  record_unwind_save_match_data ();

  /* Value saved here is to be restored into Vautoload_queue.  */
  record_unwind_protect (un_autoload, Vautoload_queue);
  Vautoload_queue = Qt;
  Fload (Fcar (Fcdr (fundef)), Qnil, noninteractive ? Qt : Qnil, Qnil, Qt);

  /* Save the old autoloads, in case we ever do an unload.  */
  queue = Vautoload_queue;
  while (CONSP (queue))
    {
      first = XCAR (queue);
      second = Fcdr (first);
      first = Fcar (first);

      if (SYMBOLP (first) && CONSP (second) && EQ (XCAR (second), Qautoload))
	Fput (first, Qautoload, (XCDR (second)));

      queue = XCDR (queue);
    }

  /* Once loading finishes, don't undo it.  */
  Vautoload_queue = Qt;
  unbind_to (count, Qnil);

  fun = Findirect_function (fun, Qnil);

  if (!NILP (Fequal (fun, fundef)))
    error ("Autoloading failed to define function %s",
	   SDATA (SYMBOL_NAME (funname)));
  UNGCPRO;
}


DEFUN ("eval", Feval, Seval, 1, 1, 0,
       doc: /* Evaluate FORM and return its value.  */)
     (form)
     Lisp_Object form;
{
  Lisp_Object fun, val, original_fun, original_args;
  Lisp_Object funcar;
  struct backtrace backtrace;
  struct gcpro gcpro1, gcpro2, gcpro3;

  if (handling_signal)
    abort ();

  if (SYMBOLP (form))
    return Fsymbol_value (form);
  if (!CONSP (form))
    return form;

  QUIT;
  if ((consing_since_gc > gc_cons_threshold
       && consing_since_gc > gc_relative_threshold)
      ||
      (!NILP (Vmemory_full) && consing_since_gc > memory_full_cons_threshold))
    {
      GCPRO1 (form);
      Fgarbage_collect ();
      UNGCPRO;
    }

  if (++lisp_eval_depth > max_lisp_eval_depth)
    {
      if (max_lisp_eval_depth < 100)
	max_lisp_eval_depth = 100;
      if (lisp_eval_depth > max_lisp_eval_depth)
	error ("Lisp nesting exceeds `max-lisp-eval-depth'");
    }

  original_fun = Fcar (form);
  original_args = Fcdr (form);

  backtrace.next = backtrace_list;
  backtrace_list = &backtrace;
  backtrace.function = &original_fun; /* This also protects them from gc */
  backtrace.args = &original_args;
  backtrace.nargs = UNEVALLED;
  backtrace.evalargs = 1;
  backtrace.debug_on_exit = 0;

  if (debug_on_next_call)
    do_debug_on_call (Qt);

  /* At this point, only original_fun and original_args
     have values that will be used below */
 retry:

  /* Optimize for no indirection.  */
  fun = original_fun;
  if (SYMBOLP (fun) && !EQ (fun, Qunbound)
      && (fun = XSYMBOL (fun)->function, SYMBOLP (fun)))
    fun = indirect_function (fun);

  if (SUBRP (fun))
    {
      Lisp_Object numargs;
      Lisp_Object argvals[8];
      Lisp_Object args_left;
      register int i, maxargs;

      args_left = original_args;
      numargs = Flength (args_left);

      CHECK_CONS_LIST ();

      if (XINT (numargs) < XSUBR (fun)->min_args ||
	  (XSUBR (fun)->max_args >= 0 && XSUBR (fun)->max_args < XINT (numargs)))
	xsignal2 (Qwrong_number_of_arguments, original_fun, numargs);

      if (XSUBR (fun)->max_args == UNEVALLED)
	{
	  backtrace.evalargs = 0;
	  val = (*XSUBR (fun)->function) (args_left);
	  goto done;
	}

      if (XSUBR (fun)->max_args == MANY)
	{
	  /* Pass a vector of evaluated arguments */
	  Lisp_Object *vals;
	  register int argnum = 0;

	  vals = (Lisp_Object *) alloca (XINT (numargs) * sizeof (Lisp_Object));

	  GCPRO3 (args_left, fun, fun);
	  gcpro3.var = vals;
	  gcpro3.nvars = 0;

	  while (!NILP (args_left))
	    {
	      vals[argnum++] = Feval (Fcar (args_left));
	      args_left = Fcdr (args_left);
	      gcpro3.nvars = argnum;
	    }

	  backtrace.args = vals;
	  backtrace.nargs = XINT (numargs);

	  val = (*XSUBR (fun)->function) (XINT (numargs), vals);
	  UNGCPRO;
	  goto done;
	}

      GCPRO3 (args_left, fun, fun);
      gcpro3.var = argvals;
      gcpro3.nvars = 0;

      maxargs = XSUBR (fun)->max_args;
      for (i = 0; i < maxargs; args_left = Fcdr (args_left))
	{
	  argvals[i] = Feval (Fcar (args_left));
	  gcpro3.nvars = ++i;
	}

      UNGCPRO;

      backtrace.args = argvals;
      backtrace.nargs = XINT (numargs);

      switch (i)
	{
	case 0:
	  val = (*XSUBR (fun)->function) ();
	  goto done;
	case 1:
	  val = (*XSUBR (fun)->function) (argvals[0]);
	  goto done;
	case 2:
	  val = (*XSUBR (fun)->function) (argvals[0], argvals[1]);
	  goto done;
	case 3:
	  val = (*XSUBR (fun)->function) (argvals[0], argvals[1],
					  argvals[2]);
	  goto done;
	case 4:
	  val = (*XSUBR (fun)->function) (argvals[0], argvals[1],
					  argvals[2], argvals[3]);
	  goto done;
	case 5:
	  val = (*XSUBR (fun)->function) (argvals[0], argvals[1], argvals[2],
					  argvals[3], argvals[4]);
	  goto done;
	case 6:
	  val = (*XSUBR (fun)->function) (argvals[0], argvals[1], argvals[2],
					  argvals[3], argvals[4], argvals[5]);
	  goto done;
	case 7:
	  val = (*XSUBR (fun)->function) (argvals[0], argvals[1], argvals[2],
					  argvals[3], argvals[4], argvals[5],
					  argvals[6]);
	  goto done;

	case 8:
	  val = (*XSUBR (fun)->function) (argvals[0], argvals[1], argvals[2],
					  argvals[3], argvals[4], argvals[5],
					  argvals[6], argvals[7]);
	  goto done;

	default:
	  /* Someone has created a subr that takes more arguments than
	     is supported by this code.  We need to either rewrite the
	     subr to use a different argument protocol, or add more
	     cases to this switch.  */
	  abort ();
	}
    }
  if (COMPILEDP (fun))
    val = apply_lambda (fun, original_args, 1);
  else
    {
      if (EQ (fun, Qunbound))
	xsignal1 (Qvoid_function, original_fun);
      if (!CONSP (fun))
	xsignal1 (Qinvalid_function, original_fun);
      funcar = XCAR (fun);
      if (!SYMBOLP (funcar))
	xsignal1 (Qinvalid_function, original_fun);
      if (EQ (funcar, Qautoload))
	{
	  do_autoload (fun, original_fun);
	  goto retry;
	}
      if (EQ (funcar, Qmacro))
	val = Feval (apply1 (Fcdr (fun), original_args));
      else if (EQ (funcar, Qlambda))
	val = apply_lambda (fun, original_args, 1);
      else
	xsignal1 (Qinvalid_function, original_fun);
    }
 done:
  CHECK_CONS_LIST ();

  lisp_eval_depth--;
  if (backtrace.debug_on_exit)
    val = call_debugger (Fcons (Qexit, Fcons (val, Qnil)));
  backtrace_list = backtrace.next;

  return val;
}

DEFUN ("apply", Fapply, Sapply, 2, MANY, 0,
       doc: /* Call FUNCTION with our remaining args, using our last arg as list of args.
Then return the value FUNCTION returns.
Thus, (apply '+ 1 2 '(3 4)) returns 10.
usage: (apply FUNCTION &rest ARGUMENTS)  */)
     (nargs, args)
     int nargs;
     Lisp_Object *args;
{
  register int i, numargs;
  register Lisp_Object spread_arg;
  register Lisp_Object *funcall_args;
  Lisp_Object fun;
  struct gcpro gcpro1;

  fun = args [0];
  funcall_args = 0;
  spread_arg = args [nargs - 1];
  CHECK_LIST (spread_arg);

  numargs = XINT (Flength (spread_arg));

  if (numargs == 0)
    return Ffuncall (nargs - 1, args);
  else if (numargs == 1)
    {
      args [nargs - 1] = XCAR (spread_arg);
      return Ffuncall (nargs, args);
    }

  numargs += nargs - 2;

  /* Optimize for no indirection.  */
  if (SYMBOLP (fun) && !EQ (fun, Qunbound)
      && (fun = XSYMBOL (fun)->function, SYMBOLP (fun)))
    fun = indirect_function (fun);
  if (EQ (fun, Qunbound))
    {
      /* Let funcall get the error */
      fun = args[0];
      goto funcall;
    }

  if (SUBRP (fun))
    {
      if (numargs < XSUBR (fun)->min_args
	  || (XSUBR (fun)->max_args >= 0 && XSUBR (fun)->max_args < numargs))
	goto funcall;		/* Let funcall get the error */
      else if (XSUBR (fun)->max_args > numargs)
	{
	  /* Avoid making funcall cons up a yet another new vector of arguments
	     by explicitly supplying nil's for optional values */
	  funcall_args = (Lisp_Object *) alloca ((1 + XSUBR (fun)->max_args)
						 * sizeof (Lisp_Object));
	  for (i = numargs; i < XSUBR (fun)->max_args;)
	    funcall_args[++i] = Qnil;
	  GCPRO1 (*funcall_args);
	  gcpro1.nvars = 1 + XSUBR (fun)->max_args;
	}
    }
 funcall:
  /* We add 1 to numargs because funcall_args includes the
     function itself as well as its arguments.  */
  if (!funcall_args)
    {
      funcall_args = (Lisp_Object *) alloca ((1 + numargs)
					     * sizeof (Lisp_Object));
      GCPRO1 (*funcall_args);
      gcpro1.nvars = 1 + numargs;
    }

  bcopy (args, funcall_args, nargs * sizeof (Lisp_Object));
  /* Spread the last arg we got.  Its first element goes in
     the slot that it used to occupy, hence this value of I.  */
  i = nargs - 1;
  while (!NILP (spread_arg))
    {
      funcall_args [i++] = XCAR (spread_arg);
      spread_arg = XCDR (spread_arg);
    }

  /* By convention, the caller needs to gcpro Ffuncall's args.  */
  RETURN_UNGCPRO (Ffuncall (gcpro1.nvars, funcall_args));
}

/* Run hook variables in various ways.  */

enum run_hooks_condition {to_completion, until_success, until_failure};
static Lisp_Object run_hook_with_args P_ ((int, Lisp_Object *,
					   enum run_hooks_condition));

DEFUN ("run-hooks", Frun_hooks, Srun_hooks, 0, MANY, 0,
       doc: /* Run each hook in HOOKS.
Each argument should be a symbol, a hook variable.
These symbols are processed in the order specified.
If a hook symbol has a non-nil value, that value may be a function
or a list of functions to be called to run the hook.
If the value is a function, it is called with no arguments.
If it is a list, the elements are called, in order, with no arguments.

Major modes should not use this function directly to run their mode
hook; they should use `run-mode-hooks' instead.

Do not use `make-local-variable' to make a hook variable buffer-local.
Instead, use `add-hook' and specify t for the LOCAL argument.
usage: (run-hooks &rest HOOKS)  */)
     (nargs, args)
     int nargs;
     Lisp_Object *args;
{
  Lisp_Object hook[1];
  register int i;

  for (i = 0; i < nargs; i++)
    {
      hook[0] = args[i];
      run_hook_with_args (1, hook, to_completion);
    }

  return Qnil;
}

DEFUN ("run-hook-with-args", Frun_hook_with_args,
       Srun_hook_with_args, 1, MANY, 0,
       doc: /* Run HOOK with the specified arguments ARGS.
HOOK should be a symbol, a hook variable.  If HOOK has a non-nil
value, that value may be a function or a list of functions to be
called to run the hook.  If the value is a function, it is called with
the given arguments and its return value is returned.  If it is a list
of functions, those functions are called, in order,
with the given arguments ARGS.
It is best not to depend on the value returned by `run-hook-with-args',
as that may change.

Do not use `make-local-variable' to make a hook variable buffer-local.
Instead, use `add-hook' and specify t for the LOCAL argument.
usage: (run-hook-with-args HOOK &rest ARGS)  */)
     (nargs, args)
     int nargs;
     Lisp_Object *args;
{
  return run_hook_with_args (nargs, args, to_completion);
}

DEFUN ("run-hook-with-args-until-success", Frun_hook_with_args_until_success,
       Srun_hook_with_args_until_success, 1, MANY, 0,
       doc: /* Run HOOK with the specified arguments ARGS.
HOOK should be a symbol, a hook variable.  If HOOK has a non-nil
value, that value may be a function or a list of functions to be
called to run the hook.  If the value is a function, it is called with
the given arguments and its return value is returned.
If it is a list of functions, those functions are called, in order,
with the given arguments ARGS, until one of them
returns a non-nil value.  Then we return that value.
However, if they all return nil, we return nil.

Do not use `make-local-variable' to make a hook variable buffer-local.
Instead, use `add-hook' and specify t for the LOCAL argument.
usage: (run-hook-with-args-until-success HOOK &rest ARGS)  */)
     (nargs, args)
     int nargs;
     Lisp_Object *args;
{
  return run_hook_with_args (nargs, args, until_success);
}

DEFUN ("run-hook-with-args-until-failure", Frun_hook_with_args_until_failure,
       Srun_hook_with_args_until_failure, 1, MANY, 0,
       doc: /* Run HOOK with the specified arguments ARGS.
HOOK should be a symbol, a hook variable.  If HOOK has a non-nil
value, that value may be a function or a list of functions to be
called to run the hook.  If the value is a function, it is called with
the given arguments and its return value is returned.
If it is a list of functions, those functions are called, in order,
with the given arguments ARGS, until one of them returns nil.
Then we return nil.  However, if they all return non-nil, we return non-nil.

Do not use `make-local-variable' to make a hook variable buffer-local.
Instead, use `add-hook' and specify t for the LOCAL argument.
usage: (run-hook-with-args-until-failure HOOK &rest ARGS)  */)
     (nargs, args)
     int nargs;
     Lisp_Object *args;
{
  return run_hook_with_args (nargs, args, until_failure);
}

/* ARGS[0] should be a hook symbol.
   Call each of the functions in the hook value, passing each of them
   as arguments all the rest of ARGS (all NARGS - 1 elements).
   COND specifies a condition to test after each call
   to decide whether to stop.
   The caller (or its caller, etc) must gcpro all of ARGS,
   except that it isn't necessary to gcpro ARGS[0].  */

static Lisp_Object
run_hook_with_args (nargs, args, cond)
     int nargs;
     Lisp_Object *args;
     enum run_hooks_condition cond;
{
  Lisp_Object sym, val, ret;
  Lisp_Object globals;
  struct gcpro gcpro1, gcpro2, gcpro3;

  /* If we are dying or still initializing,
     don't do anything--it would probably crash if we tried.  */
  if (NILP (Vrun_hooks))
    return Qnil;

  sym = args[0];
  val = find_symbol_value (sym);
  ret = (cond == until_failure ? Qt : Qnil);

  if (EQ (val, Qunbound) || NILP (val))
    return ret;
  else if (!CONSP (val) || EQ (XCAR (val), Qlambda))
    {
      args[0] = val;
      return Ffuncall (nargs, args);
    }
  else
    {
      globals = Qnil;
      GCPRO3 (sym, val, globals);

      for (;
	   CONSP (val) && ((cond == to_completion)
			   || (cond == until_success ? NILP (ret)
			       : !NILP (ret)));
	   val = XCDR (val))
	{
	  if (EQ (XCAR (val), Qt))
	    {
	      /* t indicates this hook has a local binding;
		 it means to run the global binding too.  */

	      for (globals = Fdefault_value (sym);
		   CONSP (globals) && ((cond == to_completion)
				       || (cond == until_success ? NILP (ret)
					   : !NILP (ret)));
		   globals = XCDR (globals))
		{
		  args[0] = XCAR (globals);
		  /* In a global value, t should not occur.  If it does, we
		     must ignore it to avoid an endless loop.  */
		  if (!EQ (args[0], Qt))
		    ret = Ffuncall (nargs, args);
		}
	    }
	  else
	    {
	      args[0] = XCAR (val);
	      ret = Ffuncall (nargs, args);
	    }
	}

      UNGCPRO;
      return ret;
    }
}

/* Run a hook symbol ARGS[0], but use FUNLIST instead of the actual
   present value of that symbol.
   Call each element of FUNLIST,
   passing each of them the rest of ARGS.
   The caller (or its caller, etc) must gcpro all of ARGS,
   except that it isn't necessary to gcpro ARGS[0].  */

Lisp_Object
run_hook_list_with_args (funlist, nargs, args)
     Lisp_Object funlist;
     int nargs;
     Lisp_Object *args;
{
  Lisp_Object sym;
  Lisp_Object val;
  Lisp_Object globals;
  struct gcpro gcpro1, gcpro2, gcpro3;

  sym = args[0];
  globals = Qnil;
  GCPRO3 (sym, val, globals);

  for (val = funlist; CONSP (val); val = XCDR (val))
    {
      if (EQ (XCAR (val), Qt))
	{
	  /* t indicates this hook has a local binding;
	     it means to run the global binding too.  */

	  for (globals = Fdefault_value (sym);
	       CONSP (globals);
	       globals = XCDR (globals))
	    {
	      args[0] = XCAR (globals);
	      /* In a global value, t should not occur.  If it does, we
		 must ignore it to avoid an endless loop.  */
	      if (!EQ (args[0], Qt))
		Ffuncall (nargs, args);
	    }
	}
      else
	{
	  args[0] = XCAR (val);
	  Ffuncall (nargs, args);
	}
    }
  UNGCPRO;
  return Qnil;
}

/* Run the hook HOOK, giving each function the two args ARG1 and ARG2.  */

void
run_hook_with_args_2 (hook, arg1, arg2)
     Lisp_Object hook, arg1, arg2;
{
  Lisp_Object temp[3];
  temp[0] = hook;
  temp[1] = arg1;
  temp[2] = arg2;

  Frun_hook_with_args (3, temp);
}

/* Apply fn to arg */
Lisp_Object
apply1 (fn, arg)
     Lisp_Object fn, arg;
{
  struct gcpro gcpro1;

  GCPRO1 (fn);
  if (NILP (arg))
    RETURN_UNGCPRO (Ffuncall (1, &fn));
  gcpro1.nvars = 2;
#ifdef NO_ARG_ARRAY
  {
    Lisp_Object args[2];
    args[0] = fn;
    args[1] = arg;
    gcpro1.var = args;
    RETURN_UNGCPRO (Fapply (2, args));
  }
#else /* not NO_ARG_ARRAY */
  RETURN_UNGCPRO (Fapply (2, &fn));
#endif /* not NO_ARG_ARRAY */
}

/* Call function fn on no arguments */
Lisp_Object
call0 (fn)
     Lisp_Object fn;
{
  struct gcpro gcpro1;

  GCPRO1 (fn);
  RETURN_UNGCPRO (Ffuncall (1, &fn));
}

/* Call function fn with 1 argument arg1 */
/* ARGSUSED */
Lisp_Object
call1 (fn, arg1)
     Lisp_Object fn, arg1;
{
  struct gcpro gcpro1;
#ifdef NO_ARG_ARRAY
  Lisp_Object args[2];

  args[0] = fn;
  args[1] = arg1;
  GCPRO1 (args[0]);
  gcpro1.nvars = 2;
  RETURN_UNGCPRO (Ffuncall (2, args));
#else /* not NO_ARG_ARRAY */
  GCPRO1 (fn);
  gcpro1.nvars = 2;
  RETURN_UNGCPRO (Ffuncall (2, &fn));
#endif /* not NO_ARG_ARRAY */
}

/* Call function fn with 2 arguments arg1, arg2 */
/* ARGSUSED */
Lisp_Object
call2 (fn, arg1, arg2)
     Lisp_Object fn, arg1, arg2;
{
  struct gcpro gcpro1;
#ifdef NO_ARG_ARRAY
  Lisp_Object args[3];
  args[0] = fn;
  args[1] = arg1;
  args[2] = arg2;
  GCPRO1 (args[0]);
  gcpro1.nvars = 3;
  RETURN_UNGCPRO (Ffuncall (3, args));
#else /* not NO_ARG_ARRAY */
  GCPRO1 (fn);
  gcpro1.nvars = 3;
  RETURN_UNGCPRO (Ffuncall (3, &fn));
#endif /* not NO_ARG_ARRAY */
}

/* Call function fn with 3 arguments arg1, arg2, arg3 */
/* ARGSUSED */
Lisp_Object
call3 (fn, arg1, arg2, arg3)
     Lisp_Object fn, arg1, arg2, arg3;
{
  struct gcpro gcpro1;
#ifdef NO_ARG_ARRAY
  Lisp_Object args[4];
  args[0] = fn;
  args[1] = arg1;
  args[2] = arg2;
  args[3] = arg3;
  GCPRO1 (args[0]);
  gcpro1.nvars = 4;
  RETURN_UNGCPRO (Ffuncall (4, args));
#else /* not NO_ARG_ARRAY */
  GCPRO1 (fn);
  gcpro1.nvars = 4;
  RETURN_UNGCPRO (Ffuncall (4, &fn));
#endif /* not NO_ARG_ARRAY */
}

/* Call function fn with 4 arguments arg1, arg2, arg3, arg4 */
/* ARGSUSED */
Lisp_Object
call4 (fn, arg1, arg2, arg3, arg4)
     Lisp_Object fn, arg1, arg2, arg3, arg4;
{
  struct gcpro gcpro1;
#ifdef NO_ARG_ARRAY
  Lisp_Object args[5];
  args[0] = fn;
  args[1] = arg1;
  args[2] = arg2;
  args[3] = arg3;
  args[4] = arg4;
  GCPRO1 (args[0]);
  gcpro1.nvars = 5;
  RETURN_UNGCPRO (Ffuncall (5, args));
#else /* not NO_ARG_ARRAY */
  GCPRO1 (fn);
  gcpro1.nvars = 5;
  RETURN_UNGCPRO (Ffuncall (5, &fn));
#endif /* not NO_ARG_ARRAY */
}

/* Call function fn with 5 arguments arg1, arg2, arg3, arg4, arg5 */
/* ARGSUSED */
Lisp_Object
call5 (fn, arg1, arg2, arg3, arg4, arg5)
     Lisp_Object fn, arg1, arg2, arg3, arg4, arg5;
{
  struct gcpro gcpro1;
#ifdef NO_ARG_ARRAY
  Lisp_Object args[6];
  args[0] = fn;
  args[1] = arg1;
  args[2] = arg2;
  args[3] = arg3;
  args[4] = arg4;
  args[5] = arg5;
  GCPRO1 (args[0]);
  gcpro1.nvars = 6;
  RETURN_UNGCPRO (Ffuncall (6, args));
#else /* not NO_ARG_ARRAY */
  GCPRO1 (fn);
  gcpro1.nvars = 6;
  RETURN_UNGCPRO (Ffuncall (6, &fn));
#endif /* not NO_ARG_ARRAY */
}

/* Call function fn with 6 arguments arg1, arg2, arg3, arg4, arg5, arg6 */
/* ARGSUSED */
Lisp_Object
call6 (fn, arg1, arg2, arg3, arg4, arg5, arg6)
     Lisp_Object fn, arg1, arg2, arg3, arg4, arg5, arg6;
{
  struct gcpro gcpro1;
#ifdef NO_ARG_ARRAY
  Lisp_Object args[7];
  args[0] = fn;
  args[1] = arg1;
  args[2] = arg2;
  args[3] = arg3;
  args[4] = arg4;
  args[5] = arg5;
  args[6] = arg6;
  GCPRO1 (args[0]);
  gcpro1.nvars = 7;
  RETURN_UNGCPRO (Ffuncall (7, args));
#else /* not NO_ARG_ARRAY */
  GCPRO1 (fn);
  gcpro1.nvars = 7;
  RETURN_UNGCPRO (Ffuncall (7, &fn));
#endif /* not NO_ARG_ARRAY */
}

/* The caller should GCPRO all the elements of ARGS.  */

DEFUN ("funcall", Ffuncall, Sfuncall, 1, MANY, 0,
       doc: /* Call first argument as a function, passing remaining arguments to it.
Return the value that function returns.
Thus, (funcall 'cons 'x 'y) returns (x . y).
usage: (funcall FUNCTION &rest ARGUMENTS)  */)
     (nargs, args)
     int nargs;
     Lisp_Object *args;
{
  Lisp_Object fun, original_fun;
  Lisp_Object funcar;
  int numargs = nargs - 1;
  Lisp_Object lisp_numargs;
  Lisp_Object val;
  struct backtrace backtrace;
  register Lisp_Object *internal_args;
  register int i;

  QUIT;
  if ((consing_since_gc > gc_cons_threshold
       && consing_since_gc > gc_relative_threshold)
      ||
      (!NILP (Vmemory_full) && consing_since_gc > memory_full_cons_threshold))
    Fgarbage_collect ();

  if (++lisp_eval_depth > max_lisp_eval_depth)
    {
      if (max_lisp_eval_depth < 100)
	max_lisp_eval_depth = 100;
      if (lisp_eval_depth > max_lisp_eval_depth)
	error ("Lisp nesting exceeds `max-lisp-eval-depth'");
    }

  backtrace.next = backtrace_list;
  backtrace_list = &backtrace;
  backtrace.function = &args[0];
  backtrace.args = &args[1];
  backtrace.nargs = nargs - 1;
  backtrace.evalargs = 0;
  backtrace.debug_on_exit = 0;

  if (debug_on_next_call)
    do_debug_on_call (Qlambda);

  CHECK_CONS_LIST ();

  original_fun = args[0];

 retry:

  /* Optimize for no indirection.  */
  fun = original_fun;
  if (SYMBOLP (fun) && !EQ (fun, Qunbound)
      && (fun = XSYMBOL (fun)->function, SYMBOLP (fun)))
    fun = indirect_function (fun);

  if (SUBRP (fun))
    {
       if (numargs < XSUBR (fun)->min_args
	  || (XSUBR (fun)->max_args >= 0 && XSUBR (fun)->max_args < numargs))
	{
	  XSETFASTINT (lisp_numargs, numargs);
	  xsignal2 (Qwrong_number_of_arguments, original_fun, lisp_numargs);
	}

      if (XSUBR (fun)->max_args == UNEVALLED)
	xsignal1 (Qinvalid_function, original_fun);

      if (XSUBR (fun)->max_args == MANY)
	{
	  val = (*XSUBR (fun)->function) (numargs, args + 1);
	  goto done;
	}

      if (XSUBR (fun)->max_args > numargs)
	{
	  internal_args = (Lisp_Object *) alloca (XSUBR (fun)->max_args * sizeof (Lisp_Object));
	  bcopy (args + 1, internal_args, numargs * sizeof (Lisp_Object));
	  for (i = numargs; i < XSUBR (fun)->max_args; i++)
	    internal_args[i] = Qnil;
	}
      else
	internal_args = args + 1;
      switch (XSUBR (fun)->max_args)
	{
	case 0:
	  val = (*XSUBR (fun)->function) ();
	  goto done;
	case 1:
	  val = (*XSUBR (fun)->function) (internal_args[0]);
	  goto done;
	case 2:
	  val = (*XSUBR (fun)->function) (internal_args[0], internal_args[1]);
	  goto done;
	case 3:
	  val = (*XSUBR (fun)->function) (internal_args[0], internal_args[1],
					  internal_args[2]);
	  goto done;
	case 4:
	  val = (*XSUBR (fun)->function) (internal_args[0], internal_args[1],
					  internal_args[2], internal_args[3]);
	  goto done;
	case 5:
	  val = (*XSUBR (fun)->function) (internal_args[0], internal_args[1],
					  internal_args[2], internal_args[3],
					  internal_args[4]);
	  goto done;
	case 6:
	  val = (*XSUBR (fun)->function) (internal_args[0], internal_args[1],
					  internal_args[2], internal_args[3],
					  internal_args[4], internal_args[5]);
	  goto done;
	case 7:
	  val = (*XSUBR (fun)->function) (internal_args[0], internal_args[1],
					  internal_args[2], internal_args[3],
					  internal_args[4], internal_args[5],
					  internal_args[6]);
	  goto done;

	case 8:
	  val = (*XSUBR (fun)->function) (internal_args[0], internal_args[1],
					  internal_args[2], internal_args[3],
					  internal_args[4], internal_args[5],
					  internal_args[6], internal_args[7]);
	  goto done;

	default:

	  /* If a subr takes more than 8 arguments without using MANY
	     or UNEVALLED, we need to extend this function to support it.
	     Until this is done, there is no way to call the function.  */
	  abort ();
	}
    }
  if (COMPILEDP (fun))
    val = funcall_lambda (fun, numargs, args + 1);
  else
    {
      if (EQ (fun, Qunbound))
	xsignal1 (Qvoid_function, original_fun);
      if (!CONSP (fun))
	xsignal1 (Qinvalid_function, original_fun);
      funcar = XCAR (fun);
      if (!SYMBOLP (funcar))
	xsignal1 (Qinvalid_function, original_fun);
      if (EQ (funcar, Qlambda))
	val = funcall_lambda (fun, numargs, args + 1);
      else if (EQ (funcar, Qautoload))
	{
	  do_autoload (fun, original_fun);
	  CHECK_CONS_LIST ();
	  goto retry;
	}
      else
	xsignal1 (Qinvalid_function, original_fun);
    }
 done:
  CHECK_CONS_LIST ();
  lisp_eval_depth--;
  if (backtrace.debug_on_exit)
    val = call_debugger (Fcons (Qexit, Fcons (val, Qnil)));
  backtrace_list = backtrace.next;
  return val;
}

Lisp_Object
apply_lambda (fun, args, eval_flag)
     Lisp_Object fun, args;
     int eval_flag;
{
  Lisp_Object args_left;
  Lisp_Object numargs;
  register Lisp_Object *arg_vector;
  struct gcpro gcpro1, gcpro2, gcpro3;
  register int i;
  register Lisp_Object tem;

  numargs = Flength (args);
  arg_vector = (Lisp_Object *) alloca (XINT (numargs) * sizeof (Lisp_Object));
  args_left = args;

  GCPRO3 (*arg_vector, args_left, fun);
  gcpro1.nvars = 0;

  for (i = 0; i < XINT (numargs);)
    {
      tem = Fcar (args_left), args_left = Fcdr (args_left);
      if (eval_flag) tem = Feval (tem);
      arg_vector[i++] = tem;
      gcpro1.nvars = i;
    }

  UNGCPRO;

  if (eval_flag)
    {
      backtrace_list->args = arg_vector;
      backtrace_list->nargs = i;
    }
  backtrace_list->evalargs = 0;
  tem = funcall_lambda (fun, XINT (numargs), arg_vector);

  /* Do the debug-on-exit now, while arg_vector still exists.  */
  if (backtrace_list->debug_on_exit)
    tem = call_debugger (Fcons (Qexit, Fcons (tem, Qnil)));
  /* Don't do it again when we return to eval.  */
  backtrace_list->debug_on_exit = 0;
  return tem;
}

/* Apply a Lisp function FUN to the NARGS evaluated arguments in ARG_VECTOR
   and return the result of evaluation.
   FUN must be either a lambda-expression or a compiled-code object.  */

static Lisp_Object
funcall_lambda (fun, nargs, arg_vector)
     Lisp_Object fun;
     int nargs;
     register Lisp_Object *arg_vector;
{
  Lisp_Object val, syms_left, next;
  int count = SPECPDL_INDEX ();
  int i, optional, rest;

  if (CONSP (fun))
    {
      syms_left = XCDR (fun);
      if (CONSP (syms_left))
	syms_left = XCAR (syms_left);
      else
	xsignal1 (Qinvalid_function, fun);
    }
  else if (COMPILEDP (fun))
    syms_left = AREF (fun, COMPILED_ARGLIST);
  else
    abort ();

  i = optional = rest = 0;
  for (; CONSP (syms_left); syms_left = XCDR (syms_left))
    {
      QUIT;

      next = XCAR (syms_left);
      if (!SYMBOLP (next))
	xsignal1 (Qinvalid_function, fun);

      if (EQ (next, Qand_rest))
	rest = 1;
      else if (EQ (next, Qand_optional))
	optional = 1;
      else if (rest)
	{
	  specbind (next, Flist (nargs - i, &arg_vector[i]));
	  i = nargs;
	}
      else if (i < nargs)
	specbind (next, arg_vector[i++]);
      else if (!optional)
	xsignal2 (Qwrong_number_of_arguments, fun, make_number (nargs));
      else
	specbind (next, Qnil);
    }

  if (!NILP (syms_left))
    xsignal1 (Qinvalid_function, fun);
  else if (i < nargs)
    xsignal2 (Qwrong_number_of_arguments, fun, make_number (nargs));

  if (CONSP (fun))
    val = Fprogn (XCDR (XCDR (fun)));
  else
    {
      /* If we have not actually read the bytecode string
	 and constants vector yet, fetch them from the file.  */
      if (CONSP (AREF (fun, COMPILED_BYTECODE)))
	Ffetch_bytecode (fun);
      val = Fbyte_code (AREF (fun, COMPILED_BYTECODE),
			AREF (fun, COMPILED_CONSTANTS),
			AREF (fun, COMPILED_STACK_DEPTH));
    }

  return unbind_to (count, val);
}

DEFUN ("fetch-bytecode", Ffetch_bytecode, Sfetch_bytecode,
       1, 1, 0,
       doc: /* If byte-compiled OBJECT is lazy-loaded, fetch it now.  */)
     (object)
     Lisp_Object object;
{
  Lisp_Object tem;

  if (COMPILEDP (object) && CONSP (AREF (object, COMPILED_BYTECODE)))
    {
      tem = read_doc_string (AREF (object, COMPILED_BYTECODE));
      if (!CONSP (tem))
	{
	  tem = AREF (object, COMPILED_BYTECODE);
	  if (CONSP (tem) && STRINGP (XCAR (tem)))
	    error ("Invalid byte code in %s", SDATA (XCAR (tem)));
	  else
	    error ("Invalid byte code");
	}
      AREF (object, COMPILED_BYTECODE) = XCAR (tem);
      AREF (object, COMPILED_CONSTANTS) = XCDR (tem);
    }
  return object;
}

void
grow_specpdl ()
{
  register int count = SPECPDL_INDEX ();
  if (specpdl_size >= max_specpdl_size)
    {
      if (max_specpdl_size < 400)
	max_specpdl_size = 400;
      if (specpdl_size >= max_specpdl_size)
	signal_error ("Variable binding depth exceeds max-specpdl-size", Qnil);
    }
  specpdl_size *= 2;
  if (specpdl_size > max_specpdl_size)
    specpdl_size = max_specpdl_size;
  specpdl = (struct specbinding *) xrealloc (specpdl, specpdl_size * sizeof (struct specbinding));
  specpdl_ptr = specpdl + count;
}

void
specbind (symbol, value)
     Lisp_Object symbol, value;
{
  Lisp_Object ovalue;
  Lisp_Object valcontents;

  CHECK_SYMBOL (symbol);
  if (specpdl_ptr == specpdl + specpdl_size)
    grow_specpdl ();

  /* The most common case is that of a non-constant symbol with a
     trivial value.  Make that as fast as we can.  */
  valcontents = SYMBOL_VALUE (symbol);
  if (!MISCP (valcontents) && !SYMBOL_CONSTANT_P (symbol))
    {
      specpdl_ptr->symbol = symbol;
      specpdl_ptr->old_value = valcontents;
      specpdl_ptr->func = NULL;
      ++specpdl_ptr;
      SET_SYMBOL_VALUE (symbol, value);
    }
  else
    {
      Lisp_Object valcontents;

      ovalue = find_symbol_value (symbol);
      specpdl_ptr->func = 0;
      specpdl_ptr->old_value = ovalue;

      valcontents = XSYMBOL (symbol)->value;

      if (BUFFER_LOCAL_VALUEP (valcontents)
	  || SOME_BUFFER_LOCAL_VALUEP (valcontents)
	  || BUFFER_OBJFWDP (valcontents))
	{
	  Lisp_Object where, current_buffer;

	  current_buffer = Fcurrent_buffer ();

	  /* For a local variable, record both the symbol and which
	     buffer's or frame's value we are saving.  */
	  if (!NILP (Flocal_variable_p (symbol, Qnil)))
	    where = current_buffer;
	  else if (!BUFFER_OBJFWDP (valcontents)
		   && XBUFFER_LOCAL_VALUE (valcontents)->found_for_frame)
	    where = XBUFFER_LOCAL_VALUE (valcontents)->frame;
	  else
	    where = Qnil;

	  /* We're not using the `unused' slot in the specbinding
	     structure because this would mean we have to do more
	     work for simple variables.  */
	  specpdl_ptr->symbol = Fcons (symbol, Fcons (where, current_buffer));

	  /* If SYMBOL is a per-buffer variable which doesn't have a
	     buffer-local value here, make the `let' change the global
	     value by changing the value of SYMBOL in all buffers not
	     having their own value.  This is consistent with what
	     happens with other buffer-local variables.  */
	  if (NILP (where)
	      && BUFFER_OBJFWDP (valcontents))
	    {
	      ++specpdl_ptr;
	      Fset_default (symbol, value);
	      return;
	    }
	}
      else
	specpdl_ptr->symbol = symbol;

      specpdl_ptr++;
      if (BUFFER_OBJFWDP (ovalue) || KBOARD_OBJFWDP (ovalue))
	store_symval_forwarding (symbol, ovalue, value, NULL);
      else
	set_internal (symbol, value, 0, 1);
    }
}

void
record_unwind_protect (function, arg)
     Lisp_Object (*function) P_ ((Lisp_Object));
     Lisp_Object arg;
{
  eassert (!handling_signal);

  if (specpdl_ptr == specpdl + specpdl_size)
    grow_specpdl ();
  specpdl_ptr->func = function;
  specpdl_ptr->symbol = Qnil;
  specpdl_ptr->old_value = arg;
  specpdl_ptr++;
}

Lisp_Object
unbind_to (count, value)
     int count;
     Lisp_Object value;
{
  Lisp_Object quitf = Vquit_flag;
  struct gcpro gcpro1, gcpro2;

  GCPRO2 (value, quitf);
  Vquit_flag = Qnil;

  while (specpdl_ptr != specpdl + count)
    {
      /* Copy the binding, and decrement specpdl_ptr, before we do
	 the work to unbind it.  We decrement first
	 so that an error in unbinding won't try to unbind
	 the same entry again, and we copy the binding first
	 in case more bindings are made during some of the code we run.  */

      struct specbinding this_binding;
      this_binding = *--specpdl_ptr;

      if (this_binding.func != 0)
	(*this_binding.func) (this_binding.old_value);
      /* If the symbol is a list, it is really (SYMBOL WHERE
	 . CURRENT-BUFFER) where WHERE is either nil, a buffer, or a
	 frame.  If WHERE is a buffer or frame, this indicates we
	 bound a variable that had a buffer-local or frame-local
	 binding.  WHERE nil means that the variable had the default
	 value when it was bound.  CURRENT-BUFFER is the buffer that
         was current when the variable was bound.  */
      else if (CONSP (this_binding.symbol))
	{
	  Lisp_Object symbol, where;

	  symbol = XCAR (this_binding.symbol);
	  where = XCAR (XCDR (this_binding.symbol));

	  if (NILP (where))
	    Fset_default (symbol, this_binding.old_value);
	  else if (BUFFERP (where))
	    set_internal (symbol, this_binding.old_value, XBUFFER (where), 1);
	  else
	    set_internal (symbol, this_binding.old_value, NULL, 1);
	}
      else
	{
	  /* If variable has a trivial value (no forwarding), we can
	     just set it.  No need to check for constant symbols here,
	     since that was already done by specbind.  */
	  if (!MISCP (SYMBOL_VALUE (this_binding.symbol)))
	    SET_SYMBOL_VALUE (this_binding.symbol, this_binding.old_value);
	  else
	    set_internal (this_binding.symbol, this_binding.old_value, 0, 1);
	}
    }

  if (NILP (Vquit_flag) && !NILP (quitf))
    Vquit_flag = quitf;

  UNGCPRO;
  return value;
}

DEFUN ("backtrace-debug", Fbacktrace_debug, Sbacktrace_debug, 2, 2, 0,
       doc: /* Set the debug-on-exit flag of eval frame LEVEL levels down to FLAG.
The debugger is entered when that frame exits, if the flag is non-nil.  */)
     (level, flag)
     Lisp_Object level, flag;
{
  register struct backtrace *backlist = backtrace_list;
  register int i;

  CHECK_NUMBER (level);

  for (i = 0; backlist && i < XINT (level); i++)
    {
      backlist = backlist->next;
    }

  if (backlist)
    backlist->debug_on_exit = !NILP (flag);

  return flag;
}

DEFUN ("backtrace", Fbacktrace, Sbacktrace, 0, 0, "",
       doc: /* Print a trace of Lisp function calls currently active.
Output stream used is value of `standard-output'.  */)
     ()
{
  register struct backtrace *backlist = backtrace_list;
  register int i;
  Lisp_Object tail;
  Lisp_Object tem;
  extern Lisp_Object Vprint_level;
  struct gcpro gcpro1;

  XSETFASTINT (Vprint_level, 3);

  tail = Qnil;
  GCPRO1 (tail);

  while (backlist)
    {
      write_string (backlist->debug_on_exit ? "* " : "  ", 2);
      if (backlist->nargs == UNEVALLED)
	{
	  Fprin1 (Fcons (*backlist->function, *backlist->args), Qnil);
	  write_string ("\n", -1);
	}
      else
	{
	  tem = *backlist->function;
	  Fprin1 (tem, Qnil);	/* This can QUIT */
	  write_string ("(", -1);
	  if (backlist->nargs == MANY)
	    {
	      for (tail = *backlist->args, i = 0;
		   !NILP (tail);
		   tail = Fcdr (tail), i++)
		{
		  if (i) write_string (" ", -1);
		  Fprin1 (Fcar (tail), Qnil);
		}
	    }
	  else
	    {
	      for (i = 0; i < backlist->nargs; i++)
		{
		  if (i) write_string (" ", -1);
		  Fprin1 (backlist->args[i], Qnil);
		}
	    }
	  write_string (")\n", -1);
	}
      backlist = backlist->next;
    }

  Vprint_level = Qnil;
  UNGCPRO;
  return Qnil;
}

DEFUN ("backtrace-frame", Fbacktrace_frame, Sbacktrace_frame, 1, 1, NULL,
       doc: /* Return the function and arguments NFRAMES up from current execution point.
If that frame has not evaluated the arguments yet (or is a special form),
the value is (nil FUNCTION ARG-FORMS...).
If that frame has evaluated its arguments and called its function already,
the value is (t FUNCTION ARG-VALUES...).
A &rest arg is represented as the tail of the list ARG-VALUES.
FUNCTION is whatever was supplied as car of evaluated list,
or a lambda expression for macro calls.
If NFRAMES is more than the number of frames, the value is nil.  */)
     (nframes)
     Lisp_Object nframes;
{
  register struct backtrace *backlist = backtrace_list;
  register int i;
  Lisp_Object tem;

  CHECK_NATNUM (nframes);

  /* Find the frame requested.  */
  for (i = 0; backlist && i < XFASTINT (nframes); i++)
    backlist = backlist->next;

  if (!backlist)
    return Qnil;
  if (backlist->nargs == UNEVALLED)
    return Fcons (Qnil, Fcons (*backlist->function, *backlist->args));
  else
    {
      if (backlist->nargs == MANY)
	tem = *backlist->args;
      else
	tem = Flist (backlist->nargs, backlist->args);

      return Fcons (Qt, Fcons (*backlist->function, tem));
    }
}


void
mark_backtrace ()
{
  register struct backtrace *backlist;
  register int i;

  for (backlist = backtrace_list; backlist; backlist = backlist->next)
    {
      mark_object (*backlist->function);

      if (backlist->nargs == UNEVALLED || backlist->nargs == MANY)
	i = 0;
      else
	i = backlist->nargs - 1;
      for (; i >= 0; i--)
	mark_object (backlist->args[i]);
    }
}

void
syms_of_eval ()
{
  DEFVAR_INT ("max-specpdl-size", &max_specpdl_size,
	      doc: /* *Limit on number of Lisp variable bindings and `unwind-protect's.
If Lisp code tries to increase the total number past this amount,
an error is signaled.
You can safely use a value considerably larger than the default value,
if that proves inconveniently small.  However, if you increase it too far,
Emacs could run out of memory trying to make the stack bigger.  */);

  DEFVAR_INT ("max-lisp-eval-depth", &max_lisp_eval_depth,
	      doc: /* *Limit on depth in `eval', `apply' and `funcall' before error.

This limit serves to catch infinite recursions for you before they cause
actual stack overflow in C, which would be fatal for Emacs.
You can safely make it considerably larger than its default value,
if that proves inconveniently small.  However, if you increase it too far,
Emacs could overflow the real C stack, and crash.  */);

  DEFVAR_LISP ("quit-flag", &Vquit_flag,
	       doc: /* Non-nil causes `eval' to abort, unless `inhibit-quit' is non-nil.
If the value is t, that means do an ordinary quit.
If the value equals `throw-on-input', that means quit by throwing
to the tag specified in `throw-on-input'; it's for handling `while-no-input'.
Typing C-g sets `quit-flag' to t, regardless of `inhibit-quit',
but `inhibit-quit' non-nil prevents anything from taking notice of that.  */);
  Vquit_flag = Qnil;

  DEFVAR_LISP ("inhibit-quit", &Vinhibit_quit,
	       doc: /* Non-nil inhibits C-g quitting from happening immediately.
Note that `quit-flag' will still be set by typing C-g,
so a quit will be signaled as soon as `inhibit-quit' is nil.
To prevent this happening, set `quit-flag' to nil
before making `inhibit-quit' nil.  */);
  Vinhibit_quit = Qnil;

  Qinhibit_quit = intern ("inhibit-quit");
  staticpro (&Qinhibit_quit);

  Qautoload = intern ("autoload");
  staticpro (&Qautoload);

  Qdebug_on_error = intern ("debug-on-error");
  staticpro (&Qdebug_on_error);

  Qmacro = intern ("macro");
  staticpro (&Qmacro);

  Qdeclare = intern ("declare");
  staticpro (&Qdeclare);

  /* Note that the process handling also uses Qexit, but we don't want
     to staticpro it twice, so we just do it here.  */
  Qexit = intern ("exit");
  staticpro (&Qexit);

  Qinteractive = intern ("interactive");
  staticpro (&Qinteractive);

  Qcommandp = intern ("commandp");
  staticpro (&Qcommandp);

  Qdefun = intern ("defun");
  staticpro (&Qdefun);

  Qand_rest = intern ("&rest");
  staticpro (&Qand_rest);

  Qand_optional = intern ("&optional");
  staticpro (&Qand_optional);

  DEFVAR_LISP ("stack-trace-on-error", &Vstack_trace_on_error,
	       doc: /* *Non-nil means errors display a backtrace buffer.
More precisely, this happens for any error that is handled
by the editor command loop.
If the value is a list, an error only means to display a backtrace
if one of its condition symbols appears in the list.  */);
  Vstack_trace_on_error = Qnil;

  DEFVAR_LISP ("debug-on-error", &Vdebug_on_error,
	       doc: /* *Non-nil means enter debugger if an error is signaled.
Does not apply to errors handled by `condition-case' or those
matched by `debug-ignored-errors'.
If the value is a list, an error only means to enter the debugger
if one of its condition symbols appears in the list.
When you evaluate an expression interactively, this variable
is temporarily non-nil if `eval-expression-debug-on-error' is non-nil.
See also variable `debug-on-quit'.  */);
  Vdebug_on_error = Qnil;

  DEFVAR_LISP ("debug-ignored-errors", &Vdebug_ignored_errors,
    doc: /* *List of errors for which the debugger should not be called.
Each element may be a condition-name or a regexp that matches error messages.
If any element applies to a given error, that error skips the debugger
and just returns to top level.
This overrides the variable `debug-on-error'.
It does not apply to errors handled by `condition-case'.  */);
  Vdebug_ignored_errors = Qnil;

  DEFVAR_BOOL ("debug-on-quit", &debug_on_quit,
    doc: /* *Non-nil means enter debugger if quit is signaled (C-g, for example).
Does not apply if quit is handled by a `condition-case'.  */);
  debug_on_quit = 0;

  DEFVAR_BOOL ("debug-on-next-call", &debug_on_next_call,
	       doc: /* Non-nil means enter debugger before next `eval', `apply' or `funcall'.  */);

  DEFVAR_BOOL ("debugger-may-continue", &debugger_may_continue,
	       doc: /* Non-nil means debugger may continue execution.
This is nil when the debugger is called under circumstances where it
might not be safe to continue.  */);
  debugger_may_continue = 1;

  DEFVAR_LISP ("debugger", &Vdebugger,
	       doc: /* Function to call to invoke debugger.
If due to frame exit, args are `exit' and the value being returned;
 this function's value will be returned instead of that.
If due to error, args are `error' and a list of the args to `signal'.
If due to `apply' or `funcall' entry, one arg, `lambda'.
If due to `eval' entry, one arg, t.  */);
  Vdebugger = Qnil;

  DEFVAR_LISP ("signal-hook-function", &Vsignal_hook_function,
	       doc: /* If non-nil, this is a function for `signal' to call.
It receives the same arguments that `signal' was given.
The Edebug package uses this to regain control.  */);
  Vsignal_hook_function = Qnil;

  DEFVAR_LISP ("debug-on-signal", &Vdebug_on_signal,
	       doc: /* *Non-nil means call the debugger regardless of condition handlers.
Note that `debug-on-error', `debug-on-quit' and friends
still determine whether to handle the particular condition.  */);
  Vdebug_on_signal = Qnil;

  DEFVAR_LISP ("macro-declaration-function", &Vmacro_declaration_function,
	       doc: /* Function to process declarations in a macro definition.
The function will be called with two args MACRO and DECL.
MACRO is the name of the macro being defined.
DECL is a list `(declare ...)' containing the declarations.
The value the function returns is not used.  */);
  Vmacro_declaration_function = Qnil;

  Vrun_hooks = intern ("run-hooks");
  staticpro (&Vrun_hooks);

  staticpro (&Vautoload_queue);
  Vautoload_queue = Qnil;
  staticpro (&Vsignaling_function);
  Vsignaling_function = Qnil;

  defsubr (&Sor);
  defsubr (&Sand);
  defsubr (&Sif);
  defsubr (&Scond);
  defsubr (&Sprogn);
  defsubr (&Sprog1);
  defsubr (&Sprog2);
  defsubr (&Ssetq);
  defsubr (&Squote);
  defsubr (&Sfunction);
  defsubr (&Sdefun);
  defsubr (&Sdefmacro);
  defsubr (&Sdefvar);
  defsubr (&Sdefvaralias);
  defsubr (&Sdefconst);
  defsubr (&Suser_variable_p);
  defsubr (&Slet);
  defsubr (&SletX);
  defsubr (&Swhile);
  defsubr (&Smacroexpand);
  defsubr (&Scatch);
  defsubr (&Sthrow);
  defsubr (&Sunwind_protect);
  defsubr (&Scondition_case);
  defsubr (&Ssignal);
  defsubr (&Sinteractive_p);
  defsubr (&Scalled_interactively_p);
  defsubr (&Scommandp);
  defsubr (&Sautoload);
  defsubr (&Seval);
  defsubr (&Sapply);
  defsubr (&Sfuncall);
  defsubr (&Srun_hooks);
  defsubr (&Srun_hook_with_args);
  defsubr (&Srun_hook_with_args_until_success);
  defsubr (&Srun_hook_with_args_until_failure);
  defsubr (&Sfetch_bytecode);
  defsubr (&Sbacktrace_debug);
  defsubr (&Sbacktrace);
  defsubr (&Sbacktrace_frame);
}

/* arch-tag: 014a07aa-33ab-4a8f-a3d2-ee8a4a9ff7fb
   (do not change this comment) */
