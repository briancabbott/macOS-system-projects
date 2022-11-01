/* Manages interpreters for gdb.
   Copyright 2000 Free Software Foundation, Inc.
   Written by Jim Ingham <jingham@apple.com> of Apple Computer, Inc.

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
   Boston, MA 02111-1307, USA. */

/* This is just a first cut at separating out the "interpreter" functions
   of gdb into self-contained modules.  There are a couple of open areas that
   need to be sorted out:

   1) The interpreter explicitly contains a UI_OUT, and can insert itself
   into the event loop, but it doesn't explicitly contain hooks for readline.
   I did this because it seems to me many interpreters won't want to use
   the readline command interface, and it is probably simpler to just let
   them take over the input in their resume proc.  

   2) The event loop insertion is probably wrong.  I just inserted a 
   do_one_event alongside gdb's do_one_event.  This probably will lead
   to one or the other event loop getting starved.  It would be better
   to provide conversion functions for the gdb file handlers, and when
   an interpreter starts up, it grabs all the gdb created file handlers
   and inserts them into its select.  This is more complicated, however,
   and I have run out of time for now.
*/

#include "defs.h"
#include "gdbcmd.h"
#include "ui-out.h"
#include "event-loop.h"
#include "event-top.h"
#include "interpreter.h"

/* Functions local to this file. */
static void initialize_interps (void);

static void set_interpreter_cmd (char *args, int from_tty, 
				 struct cmd_list_element *c);
static void list_interpreter_cmd (char *args, int from_tty);
static void do_set_interpreter (int not_an_fd);

/* The magic initialization routine for this module. */

void _initialize_interpreter (void);

/* Variables local to this file: */

static struct gdb_interpreter *interp_list = NULL;
static struct gdb_interpreter *current = NULL;

static int initialized = 0;

/* gdb_new_interpreter - This allocates space for a new interpreter,
 * fills the fields from the inputs, and returns a pointer to the
 * interpreter.  I might be better to have a version that just takes
 * the name, data & uiout, and then a bunch of set routines for the 
 * procs.  Otherwise the argument list for this might get out of hand.
 */

struct gdb_interpreter *
gdb_new_interpreter (char *name, 
		     void *data, 
		     struct ui_out *uiout,
		     interp_init_ftype init_proc, 
		     interp_resume_ftype  resume_proc,
		     interp_do_one_event_ftype do_one_event_proc,
		     interp_suspend_ftype suspend_proc, 
		     interp_delete_ftype delete_proc,
		     interp_exec_ftype exec_proc,
		     interp_prompt_ftype prompt_proc,
		     interp_complete_ftype complete_proc)
{
  struct gdb_interpreter *new_interp;
  
  new_interp = (struct gdb_interpreter *) xmalloc (sizeof (struct gdb_interpreter));
  
  new_interp->name = xstrdup (name); 
  new_interp->data = data;
  new_interp->interpreter_out = uiout;
  new_interp->quiet_p = 0;
  new_interp->init_proc = init_proc; 
  new_interp->resume_proc = resume_proc; 
  new_interp->do_one_event_proc = do_one_event_proc;
  new_interp->suspend_proc = suspend_proc; 
  new_interp->delete_proc = delete_proc;
  new_interp->exec_proc   = exec_proc;
  new_interp->prompt_proc = prompt_proc;
  new_interp->complete_proc = complete_proc;
  new_interp->inited = 0;

  return new_interp;  
}

/*
 * gdb_add_interpreter
 * 
 * Add interpreter INTERP to the gdb interpreter list.  If an
 * interpreter of the same name is already on the list, then
 * the new one is NOT added, and the function returns 0.  Otherwise
 * it returns 1.
 */

int 
gdb_add_interpreter (struct gdb_interpreter *interp)
{
  if (!initialized)
    initialize_interps ();

  if (gdb_lookup_interpreter (interp->name) != NULL) 
      return 0;

  interp->next = interp_list;
  interp_list = interp;
  
  return 1;
}

/*
 * gdb_delete_interpreter
 *
 * Looks for the interpreter INTERP in the interpreter list.  If it exists,
 * runs the delete_proc, and if this is successful, the INTERP is deleted from
 * the interpreter list and the function returns 1.  If the delete_proc fails, the
 * function returns -1 and the interpreter is NOT removed from the list.  If the
 * interp is not found, 0 is returned.
 */
  
int
gdb_delete_interpreter (struct gdb_interpreter *interp)
{
  struct gdb_interpreter *cur_ptr, *prev_ptr;
  
  if (!initialized)
    {
      ui_out_message (uiout, 0,
		      "You can't delete an interp before you have added one!");
      return -1;
    }

  if (interp_list == NULL)
    {
      ui_out_message (uiout, 0, "No interpreters to delete.");
      return -1;
    }

  if (interp_list->next == NULL) 
    {
      ui_out_message (uiout, 0,
		      "You can't delete gdb's only intepreter.");
      return -1;
    }

  for (cur_ptr = interp_list, prev_ptr = NULL; 
       cur_ptr != NULL; 
       prev_ptr = cur_ptr, cur_ptr = cur_ptr->next)
    {
      if (cur_ptr == interp) 
        {
	  /* Can't currently delete the console interpreter... */
	  if (strcmp (interp->name, "console") == 0)
	    {
	      ui_out_message (uiout, 0,
			      "You can't delete the console interpreter.");
	      return -1;
	    }

	  /* If the interpreter is the current interpreter, switch
	     back to the console interpreter */

	  if (interp == current)
	    {
	      gdb_set_interpreter (gdb_lookup_interpreter ("console"));
	    }

          /* Don't delete the interpreter if its delete proc fails */
          
          if ((interp->delete_proc != NULL)
              && (!interp->delete_proc (interp->data))) 
	      return -1;
            
	  if (cur_ptr == interp_list)
            interp_list = cur_ptr->next;
          else
            prev_ptr->next = cur_ptr->next;
            
          break;
        }
    }
    
  if (cur_ptr == NULL)
    return 0;
  else
    return 1;
}

/*
 * gdb_set_interpreter
 *
 * This sets the current interpreter to be INTERP.  If INTERP has not
 * been initialized, then this will also run the init proc.  If the
 * init proc is successful, return 1, if it fails, set the old
 * interpreter back in place and return 0.  If we can't restore the
 * old interpreter, then raise an internal error, since we are in
 * pretty bad shape at this point.  
*/
 
int 
gdb_set_interpreter (struct gdb_interpreter *interp)
{
  struct gdb_interpreter *old_interp = current;
  int first_time = 0;
  

  char buffer[64];

  if (current != NULL)
    {
      /* APPLE LOCAL: Don't do this, you can't be sure there are no
	 continuations from the enclosing interpreter which should
	 really be run when that interpreter is in force. */
#if 0
     do_all_continuations ();
#endif
     ui_out_flush (uiout);
      if (current->suspend_proc &&
          !current->suspend_proc (current->data))
	{
	  error ("Could not suspend interpreter \"%s\"\n",
		 current->name);
	}
    }
  else
    {
    	first_time = 1;
    }
    
  current = interp;

  /* We use interpreter_p for the "set interpreter" variable, so we need
     to make sure we have a malloc'ed copy for the set command to free. */
  if (interpreter_p != NULL && strcmp (current->name, interpreter_p) != 0)
    {
      xfree (interpreter_p);

      interpreter_p = xstrdup (current->name);
    }

  uiout = interp->interpreter_out;

  /* Run the init proc.  If it fails, try to restore the old interp. */
  
  if (!interp->inited)
    {
      if (interp->init_proc != NULL)
	{
          if (!interp->init_proc (interp->data))
	    {
	      if (!gdb_set_interpreter (old_interp)) 
		internal_error ("Failed to initialize new interp \"%s\" %s",
				interp->name,
				"and could not restore old interp!\n");
	      return 0;
	    }
	  else
	    {
	      interp->inited = 1;
	    }
	}
      else
	{
	  interp->inited = 1;
	}
    }

  if (interp->resume_proc != NULL 
	  && (!interp->resume_proc (interp->data)))
    {
      if (!gdb_set_interpreter (old_interp)) 
	internal_error ("Failed to initialize new interp \"%s\" %s",
			interp->name,
			"and could not restore old interp!\n");
      return 0;
    }

  /* Finally, put up the new prompt to show that we are indeed here. 
     Also, display_gdb_prompt for the console does some readline magic
     which is needed for the console interpreter, at least... */

  if (!first_time)
    {
      if (!gdb_interpreter_is_quiet (interp))
	{
	  sprintf (buffer, "Switching to interpreter \"%.24s\".\n", 
		   interp->name);
	  ui_out_text (uiout, buffer);
	}
      display_gdb_prompt (NULL);
    }

  return 1;
}

/*
 * gdb_lookup_interpreter - Looks up the interpreter for NAME.  If
 * no such interpreter exists, return NULL, otherwise return a pointer
 * to the interpreter. 
 */

struct gdb_interpreter *
gdb_lookup_interpreter(char *name)
{
  struct gdb_interpreter *interp;

  if (name == NULL || strlen(name) == 0)
    return NULL;

  for (interp = interp_list; interp != NULL; interp = interp->next)
    {
      if (strcmp (interp->name, name) == 0)
      	return interp;
    }
  
  return NULL;
} 

/*
 * gdb_current_interpreter - Returns the current interpreter.
 */

struct gdb_interpreter *
gdb_current_interpreter()
{
  return current;
}

struct ui_out *
gdb_interpreter_ui_out (struct gdb_interpreter *interp)
{
  return interp->interpreter_out;
}

/*
 * gdb_current_interpreter_is -- returns true if the current interp is 
 *   the passed in name.
 */
int 
gdb_current_interpreter_is_named(char *interp_name)
{
    struct gdb_interpreter *current_interp = gdb_current_interpreter();
    
    if (current_interp)
        return (strcmp(current_interp->name, interp_name) == 0);
        
    return 0;
}

/*
 * gdb_interpreter_display_prompt - This is called in display_gdb_prompt.
 * If the current interpreter defines a prompt_proc, then that proc is 
 * run.  If the proc returns a non-zero value, display_gdb_prompt will
 * return without itself displaying the prompt.
 */

int
gdb_interpreter_display_prompt (char *new_prompt)
{
  if (current->prompt_proc == NULL)
    return 0;
  else
    return current->prompt_proc (current->data, new_prompt);
}

int
gdb_interpreter_is_quiet (struct gdb_interpreter *interp)
{
  if (interp != NULL)
    return interp->quiet_p;
  else
    return current->quiet_p;
}

int
gdb_interpreter_set_quiet (struct gdb_interpreter *interp, int quiet)
{
  int old_val = interp->quiet_p;
  interp->quiet_p = quiet;
  return old_val;
}

/* gdb_interpreter_exec - This executes COMMAND_STR in the current 
   interpreter. */

int
gdb_interpreter_exec (char *command_str)
{
  if (current->exec_proc != NULL)
    {
      return current->exec_proc (current->data, command_str);
    }

  return 0;
}

int
gdb_interpreter_complete (struct gdb_interpreter *interp, 
				     char *word, char *command_buffer, int cursor)
{
  if (interp->complete_proc != NULL)
    {
      return interp->complete_proc (interp->data, word, command_buffer, cursor);
    }
  
  return 0;
}

int
interpreter_do_one_event ()
{
  if (current->do_one_event_proc == NULL)
    return 0;
    
  return current->do_one_event_proc (current->data);
}

/* clear_interpreter_hooks - A convenience routine that nulls out all the
 * common command hooks.  Use it when removing your interpreter in its 
 * suspend proc.
 */

void 
clear_interpreter_hooks ()
{
  init_ui_hook = 0;
  print_frame_info_listing_hook = 0;
  print_frame_more_info_hook = 0;
  query_hook = 0;
  warning_hook = 0;
  create_breakpoint_hook = 0;
  delete_breakpoint_hook = 0;
  modify_breakpoint_hook = 0;
  interactive_hook = 0;
  registers_changed_hook = 0;
  readline_begin_hook = 0;
  readline_hook = 0;
  readline_end_hook = 0;
  register_changed_hook = 0;
  memory_changed_hook = 0;
  context_hook = 0;
  target_wait_hook = 0;
  call_command_hook = 0;
  error_hook = 0;
  error_begin_hook = 0;
  command_loop_hook = 0; 
}

/*
 * initialize_interps - This is a lazy init routine, called the first time
 * the interpreter module is used.  I put it here just in case, but I haven't
 * thought of a use for it yet.  I will probably bag it soon, since I don't
 * think it will be necessary.
 */

static void
initialize_interps (void)
{
  initialized = 1;
  /* Don't know if anything needs to be done here... */    
}

/* set_interpreter_cmd - This implements "set interpreter foo". */

static void 
set_interpreter_cmd (char *args, int from_tty, struct cmd_list_element * c)
{
  struct gdb_interpreter *interp_ptr;

  dont_repeat ();

  if (cmd_type (c) != set_cmd)
    return;

  interp_ptr = gdb_lookup_interpreter (interpreter_p);
  if (interp_ptr != NULL)
    {
      if (!gdb_set_interpreter (interp_ptr))
	error ("\nCould not switch to interpreter \"%s\", %s%s\".\n", 
	       interp_ptr->name, "reverting to interpreter \"",
	       current->name);
    }
  else
    {
      char *bad_name = interpreter_p;
      interpreter_p = xstrdup (current->name);
      error ("Could not find interpreter \"%s\".", bad_name);
    }
}

/* list_interpreter_cmd - This implements "info interpreters".
 */

void
list_interpreter_cmd (char *args, int from_tty)
{
  struct gdb_interpreter *interp_ptr;
  struct cleanup *list_cleanup;

  list_cleanup = make_cleanup_ui_out_list_begin_end (uiout, "interpreters");
  for (interp_ptr = interp_list; interp_ptr != NULL; 
       interp_ptr = interp_ptr->next)
    {
      ui_out_text (uiout, "  * ");
      ui_out_field_string (uiout, "interpreter", interp_ptr->name);
      ui_out_text (uiout, "\n");
    }
  do_cleanups (list_cleanup);
}

void
interpreter_exec_cmd (char *args, int from_tty)
{
  struct gdb_interpreter *old_interp, *interp_to_use;
  char **prules = NULL;
  char **trule = NULL;
  unsigned int nrules;
  unsigned int i;
  int old_quiet;

  prules = buildargv (args);
  if (prules == NULL) {
    error ("unable to parse arguments");
  }

  nrules = 0;
  if (prules != NULL) {
    for (trule = prules; *trule != NULL; trule++) {
      nrules++;
    }
  }
  
  if (nrules < 2)
    error ("usage: interpreter-exec <interpreter> [ <command> ... ]");

  old_interp = gdb_current_interpreter ();

  interp_to_use = gdb_lookup_interpreter (prules[0]);
  if (interp_to_use == NULL)
    error ("Could not find interpreter \"%s\".", prules[0]);
  
  old_quiet = gdb_interpreter_set_quiet (interp_to_use, 1);
  
  if (! gdb_set_interpreter (interp_to_use))
    error ("Could not switch to interpreter \"%s\".", prules[0]);
  
  for (i = 1; i < nrules; i++) {
    if (! gdb_interpreter_exec (prules[i])) {
      gdb_set_interpreter (old_interp);
      gdb_interpreter_set_quiet (interp_to_use, old_quiet);
      error ("interpreter-exec: mi_interpreter_execute: error in command: \"%s\".", prules[i]);
      break;
    }
  }
  
  gdb_set_interpreter (old_interp);
  gdb_interpreter_set_quiet (interp_to_use, old_quiet);
}

/* _initialize_interpreter - This just adds the "set interpreter" and
 * "info interpreters" commands.
 */

void
_initialize_interpreter (void)
{
  struct cmd_list_element *c;

  c = add_set_cmd ("interpreter", class_support,
		   var_string,
		   &interpreter_p,
		   "Set the interpreter for gdb.",
		   &setlist);
  set_cmd_sfunc (c, set_interpreter_cmd);
  add_show_from_set(c, &showlist);

  add_cmd ("interpreters", class_support,
	       list_interpreter_cmd,
	   "List the interpreters currently available in gdb.",
	       &infolist);

  add_cmd ("interpreter-exec", class_support,
	       interpreter_exec_cmd,
	   "List the interpreters currently available in gdb.",
	       &cmdlist);
}
