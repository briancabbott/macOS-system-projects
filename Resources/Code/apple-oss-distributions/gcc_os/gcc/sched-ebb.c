/* Instruction scheduling pass.
   Copyright (C) 1992, 1993, 1994, 1995, 1996, 1997, 1998,
   1999, 2000, 2001, 2002 Free Software Foundation, Inc.
   Contributed by Michael Tiemann (tiemann@cygnus.com) Enhanced by,
   and currently maintained by, Jim Wilson (wilson@cygnus.com)

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

#include "config.h"
#include "system.h"
#include "toplev.h"
#include "rtl.h"
#include "tm_p.h"
#include "hard-reg-set.h"
#include "basic-block.h"
#include "regs.h"
#include "function.h"
#include "flags.h"
#include "insn-config.h"
#include "insn-attr.h"
#include "except.h"
#include "toplev.h"
#include "recog.h"
#include "cfglayout.h"
#include "sched-int.h"
#include "target.h"         /* APPLE LOCAL - 3.4 scheduler update */

/* The number of insns to be scheduled in total.  */
static int target_n_insns;
/* The number of insns scheduled so far.  */
static int sched_n_insns;

/* Implementations of the sched_info functions for region scheduling.  */
static void init_ready_list PARAMS ((struct ready_list *));
static int can_schedule_ready_p PARAMS ((rtx));
static int new_ready PARAMS ((rtx));
static int schedule_more_p PARAMS ((void));
static const char *ebb_print_insn PARAMS ((rtx, int));
static int rank PARAMS ((rtx, rtx));
static int contributes_to_priority PARAMS ((rtx, rtx));
static void compute_jump_reg_dependencies PARAMS ((rtx, regset));
/* APPLE LOCAL begin - 3.4 scheduler update */
static basic_block schedule_ebb PARAMS ((rtx, rtx));
static basic_block fix_basic_block_boundaries PARAMS ((basic_block, basic_block, rtx, rtx));
static void add_missing_bbs PARAMS ((rtx, basic_block, basic_block));
/* APPLE LOCAL end - 3.4 scheduler update */

/* Return nonzero if there are more insns that should be scheduled.  */

static int
schedule_more_p ()
{
  return sched_n_insns < target_n_insns;
}

/* Add all insns that are initially ready to the ready list READY.  Called
   once before scheduling a set of insns.  */

static void
init_ready_list (ready)
     struct ready_list *ready;
{
  rtx prev_head = current_sched_info->prev_head;
  rtx next_tail = current_sched_info->next_tail;
  rtx insn;

  target_n_insns = 0;
  sched_n_insns = 0;

#if 0
  /* Print debugging information.  */
  if (sched_verbose >= 5)
    debug_dependencies ();
#endif

  /* Initialize ready list with all 'ready' insns in target block.
     Count number of insns in the target block being scheduled.  */
  for (insn = NEXT_INSN (prev_head); insn != next_tail; insn = NEXT_INSN (insn))
    {
      /* APPLE LOCAL begin - 3.4 scheduler update */
      if (INSN_DEP_COUNT (insn) == 0)
	ready_add (ready, insn);
      target_n_insns++;
      /* APPLE LOCAL end - 3.4 scheduler update */
    }
}

/* Called after taking INSN from the ready list.  Returns nonzero if this
   insn can be scheduled, nonzero if we should silently discard it.  */

static int
can_schedule_ready_p (insn)
     rtx insn ATTRIBUTE_UNUSED;
{
  sched_n_insns++;
  return 1;
}

/* Called after INSN has all its dependencies resolved.  Return nonzero
   if it should be moved to the ready list or the queue, or zero if we
   should silently discard it.  */
static int
new_ready (next)
     rtx next ATTRIBUTE_UNUSED;
{
  return 1;
}

/* Return a string that contains the insn uid and optionally anything else
   necessary to identify this insn in an output.  It's valid to use a
   static buffer for this.  The ALIGNED parameter should cause the string
   to be formatted so that multiple output lines will line up nicely.  */

static const char *
ebb_print_insn (insn, aligned)
     rtx insn;
     int aligned ATTRIBUTE_UNUSED;
{
  static char tmp[80];

  sprintf (tmp, "%4d", INSN_UID (insn));
  return tmp;
}

/* Compare priority of two insns.  Return a positive number if the second
   insn is to be preferred for scheduling, and a negative one if the first
   is to be preferred.  Zero if they are equally good.  */

static int
rank (insn1, insn2)
     rtx insn1, insn2;  /* APPLE LOCAL - 3.4 scheduler update */
{
  /* APPLE LOCAL begin - 3.4 scheduler update */
  basic_block bb1 = BLOCK_FOR_INSN (insn1);
  basic_block bb2 = BLOCK_FOR_INSN (insn2);

  if (bb1->count > bb2->count
      || bb1->frequency > bb2->frequency)
    return -1;
  if (bb1->count < bb2->count
      || bb1->frequency < bb2->frequency)
    return 1;
  /* APPLE LOCAL end - 3.4 scheduler update */
  return 0;
}

/* NEXT is an instruction that depends on INSN (a backward dependence);
   return nonzero if we should include this dependence in priority
   calculations.  */

static int
contributes_to_priority (next, insn)
     rtx next ATTRIBUTE_UNUSED, insn ATTRIBUTE_UNUSED;
{
  return 1;
}

/* INSN is a JUMP_INSN.  Store the set of registers that must be considered
   to be set by this jump in SET.  */

static void
compute_jump_reg_dependencies (insn, set)
     rtx insn;
     regset set;
{
  basic_block b = BLOCK_FOR_INSN (insn);
  edge e;
  for (e = b->succ; e; e = e->succ_next)
    if ((e->flags & EDGE_FALLTHRU) == 0)
      {
	bitmap_operation (set, set, e->dest->global_live_at_start,
			  BITMAP_IOR);
      }
}

/* Used in schedule_insns to initialize current_sched_info for scheduling
   regions (or single basic blocks).  */

static struct sched_info ebb_sched_info =
{
  init_ready_list,
  can_schedule_ready_p,
  schedule_more_p,
  new_ready,
  rank,
  ebb_print_insn,
  contributes_to_priority,
  compute_jump_reg_dependencies,

  NULL, NULL,
  NULL, NULL,
  0, 1
};

/* APPLE LOCAL begin - 3.4 scheduler update */
/* It is possible that ebb scheduling elliminated some blocks.
   Place blocks from FIRST to LAST before BEFORE.  */

static void
add_missing_bbs (before, first, last)
     rtx before;
     basic_block first, last;
{
  for (; last != first->prev_bb; last = last->prev_bb)
    {
      before = emit_note_before (NOTE_INSN_BASIC_BLOCK, before);
      NOTE_BASIC_BLOCK (before) = last;
      last->head = before;
      last->end = before;
      update_bb_for_insn (last);
    }
}

/* Fixup the CFG after EBB scheduling.  Re-recognize the basic
   block boundaries in between HEAD and TAIL and update basic block
   structures between BB and LAST.  */

static basic_block
fix_basic_block_boundaries (bb, last, head, tail)
     basic_block bb, last;
     rtx head, tail;
{
  rtx insn = head;
  rtx last_inside = bb->head;
  rtx aftertail = NEXT_INSN (tail);

  head = bb->head;

  for (; insn != aftertail; insn = NEXT_INSN (insn))
    {
      if (GET_CODE (insn) == CODE_LABEL)
	abort ();
      /* Create new basic blocks just before first insn.  */
      if (inside_basic_block_p (insn))
	{
	  if (!last_inside)
	    {
	      rtx note;

	      /* Re-emit the basic block note for newly found BB header.  */
	      if (GET_CODE (insn) == CODE_LABEL)
		{
		  note = emit_note_after (NOTE_INSN_BASIC_BLOCK, insn);
		  head = insn;
		  last_inside = note;
		}
	      else
		{
		  note = emit_note_before (NOTE_INSN_BASIC_BLOCK, insn);
		  head = note;
		  last_inside = insn;
		}
	    }
	  else
	    last_inside = insn;
	}
      /* Control flow instruction terminate basic block.  It is possible
	 that we've elliminated some basic blocks (made them empty).
	 Find the proper basic block using BLOCK_FOR_INSN and arrange things in
	 a sensible way by inserting empty basic blocks as needed.  */
      if (control_flow_insn_p (insn) || (insn == tail && last_inside))
	{
	  basic_block curr_bb = BLOCK_FOR_INSN (insn);
	  rtx note;

	  if (!control_flow_insn_p (insn))
	    curr_bb = last;
	  if (bb == last->next_bb)
	    {
	      edge f;
	      rtx h;

	      /* An obscure special case, where we do have partially dead
	         instruction scheduled after last control flow instruction.
	         In this case we can create new basic block.  It is
	         always exactly one basic block last in the sequence.  Handle
	         it by splitting the edge and repositioning the block.
	         This is somewhat hackish, but at least avoid cut&paste 

	         Safter sollution can be to bring the code into sequence,
	         do the split and re-emit it back in case this will ever
	         trigger problem.  */
	      f = bb->prev_bb->succ;
	      while (f && !(f->flags & EDGE_FALLTHRU))
		f = f->succ_next;

	      if (f)
		{
		  last = curr_bb = split_edge (f);
		  h = curr_bb->head;
		  curr_bb->head = head;
		  curr_bb->end = insn;
		  /* Edge splitting created missplaced BASIC_BLOCK note, kill
		     it.  */
		  delete_insn (h);
		}
	      /* It may happen that code got moved past unconditional jump in
	         case the code is completely dead.  Kill it.  */
	      else
		{
		  rtx next = next_nonnote_insn (insn);
		  delete_insn_chain (head, insn);
		  /* We keep some notes in the way that may split barrier from the
		     jump.  */
		  if (GET_CODE (next) == BARRIER)
		     {
		       emit_barrier_after (prev_nonnote_insn (head));
		       delete_insn (next);
		     }
		  insn = NULL;
		}
	    }
	  else
	    {
	      curr_bb->head = head;
	      curr_bb->end = insn;
	      add_missing_bbs (curr_bb->head, bb, curr_bb->prev_bb);
	    }
	  note = GET_CODE (head) == CODE_LABEL ? NEXT_INSN (head) : head;
	  NOTE_BASIC_BLOCK (note) = curr_bb;
	  update_bb_for_insn (curr_bb);
	  bb = curr_bb->next_bb;
	  last_inside = NULL;
	  if (!insn)
	     break;
	}
    }
  add_missing_bbs (last->next_bb->head, bb, last);
  return bb->prev_bb;
}
/* APPLE LOCAL end - 3.4 scheduler update */

/* Schedule a single extended basic block, defined by the boundaries HEAD
   and TAIL.  */

static basic_block             /* APPLE LOCAL - 3.4 scheduler update */
schedule_ebb (head, tail)
     rtx head, tail;
{
  int n_insns;
  /* APPLE LOCAL begin - 3.4 scheduler update */
  basic_block b;
  struct deps tmp_deps;
  basic_block first_bb = BLOCK_FOR_INSN (head);
  basic_block last_bb = BLOCK_FOR_INSN (tail);

  if (no_real_insns_p (head, tail))
    return BLOCK_FOR_INSN (tail);
  /* APPLE LOCAL end - 3.4 scheduler update */

  init_deps_global ();

  /* Compute LOG_LINKS.  */
  init_deps (&tmp_deps);
  sched_analyze (&tmp_deps, head, tail);
  free_deps (&tmp_deps);

  /* Compute INSN_DEPEND.  */
  compute_forward_dependences (head, tail);

  /* APPLE LOCAL begin - 3.4 scheduler update */
  if (targetm.sched.dependencies_evaluation_hook)
    targetm.sched.dependencies_evaluation_hook (head, tail);
  /* APPLE LOCAL end - 3.4 scheduler update */
  
  /* Set priorities.  */
  n_insns = set_priorities (head, tail);

  current_sched_info->prev_head = PREV_INSN (head);
  current_sched_info->next_tail = NEXT_INSN (tail);

  if (write_symbols != NO_DEBUG)
    {
      save_line_notes (0, head, tail);
      rm_line_notes (head, tail);
    }

  /* rm_other_notes only removes notes which are _inside_ the
     block---that is, it won't remove notes before the first real insn
     or after the last real insn of the block.  So if the first insn
     has a REG_SAVE_NOTE which would otherwise be emitted before the
     insn, it is redundant with the note before the start of the
     block, and so we have to take it out.  */
  if (INSN_P (head))
    {
      rtx note;

      for (note = REG_NOTES (head); note; note = XEXP (note, 1))
	if (REG_NOTE_KIND (note) == REG_SAVE_NOTE)
	  {
	    remove_note (head, note);
	    note = XEXP (note, 1);
	    remove_note (head, note);
	  }
    }

  /* Remove remaining note insns from the block, save them in
     note_list.  These notes are restored at the end of
     schedule_block ().  */
  rm_other_notes (head, tail);

  current_sched_info->queue_must_finish_empty = 1;

  schedule_block (-1, n_insns);

  /* Sanity check: verify that all region insns were scheduled.  */
  if (sched_n_insns != n_insns)
    abort ();
  head = current_sched_info->head;
  tail = current_sched_info->tail;

  if (write_symbols != NO_DEBUG)
    restore_line_notes (head, tail);
  /* APPLE LOCAL begin - 3.4 scheduler update */
  b = fix_basic_block_boundaries (first_bb, last_bb, head, tail);

  finish_deps_global ();
  return b;
  /* APPLE LOCAL end - 3.4 scheduler update */
}

/* The one entry point in this file.  DUMP_FILE is the dump file for
   this pass.  */

void
schedule_ebbs (dump_file)
     FILE *dump_file;
{
  basic_block bb;

  /* Taking care of this degenerate case makes the rest of
     this code simpler.  */
  if (n_basic_blocks == 0)
    return;

  sched_init (dump_file);

  current_sched_info = &ebb_sched_info;

  allocate_reg_life_data ();
  compute_bb_for_insn ();

  /* Schedule every region in the subroutine.  */
  FOR_EACH_BB (bb)
    {
      rtx head = bb->head;
      rtx tail;

      for (;;)
	{
	  edge e;
	  tail = bb->end;
	  if (bb->next_bb == EXIT_BLOCK_PTR
	      || GET_CODE (bb->next_bb->head) == CODE_LABEL)
	    break;
	  for (e = bb->succ; e; e = e->succ_next)
	    if ((e->flags & EDGE_FALLTHRU) != 0)
	      break;
	  if (! e)
	    break;
	  /* APPLE LOCAL begin - 3.4 scheduler update */
	  if (e->probability < REG_BR_PROB_BASE / 2)
	    break;
	  /* APPLE LOCAL end - 3.4 scheduler update */
	  bb = bb->next_bb;
	}

      /* Blah.  We should fix the rest of the code not to get confused by
	 a note or two.  */
      while (head != tail)
	{
	  if (GET_CODE (head) == NOTE)
	    head = NEXT_INSN (head);
	  else if (GET_CODE (tail) == NOTE)
	    tail = PREV_INSN (tail);
	  else if (GET_CODE (head) == CODE_LABEL)
	    head = NEXT_INSN (head);
	  else
	    break;
	}

      bb = schedule_ebb (head, tail); /* APPLE LOCAL - 3.4 scheduler update */
    }

  /* Updating life info can be done by local propagation over the modified
     superblocks.  */

  /* Reposition the prologue and epilogue notes in case we moved the
     prologue/epilogue insns.  */
  if (reload_completed)
    reposition_prologue_and_epilogue_notes (get_insns ());

  if (write_symbols != NO_DEBUG)
    rm_redundant_line_notes ();

  sched_finish ();
}
