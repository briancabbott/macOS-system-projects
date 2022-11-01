/* Window creation, deletion and examination for GNU Emacs.
   Does not include redisplay.
   Copyright (C) 1985, 1986, 1987, 1993, 1994, 1995, 1996, 1997, 1998, 2000,
                 2001, 2002, 2003, 2004, 2005, 2006, 2007 Free Software Foundation, Inc.

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
#include "buffer.h"
#include "keyboard.h"
#include "keymap.h"
#include "frame.h"
#include "window.h"
#include "commands.h"
#include "indent.h"
#include "termchar.h"
#include "disptab.h"
#include "dispextern.h"
#include "blockinput.h"
#include "intervals.h"

#ifdef HAVE_X_WINDOWS
#include "xterm.h"
#endif	/* HAVE_X_WINDOWS */
#ifdef WINDOWSNT
#include "w32term.h"
#endif
#ifdef MSDOS
#include "msdos.h"
#endif
#ifdef MAC_OS
#include "macterm.h"
#endif


Lisp_Object Qwindowp, Qwindow_live_p, Qwindow_configuration_p;
Lisp_Object Qscroll_up, Qscroll_down;
Lisp_Object Qwindow_size_fixed;
extern Lisp_Object Qleft_margin, Qright_margin;

static int displayed_window_lines P_ ((struct window *));
static struct window *decode_window P_ ((Lisp_Object));
static int count_windows P_ ((struct window *));
static int get_leaf_windows P_ ((struct window *, struct window **, int));
static void window_scroll P_ ((Lisp_Object, int, int, int));
static void window_scroll_pixel_based P_ ((Lisp_Object, int, int, int));
static void window_scroll_line_based P_ ((Lisp_Object, int, int, int));
static int window_min_size_1 P_ ((struct window *, int));
static int window_min_size P_ ((struct window *, int, int, int *));
static void size_window P_ ((Lisp_Object, int, int, int, int, int));
static int freeze_window_start P_ ((struct window *, void *));
static int window_fixed_size_p P_ ((struct window *, int, int));
static void enlarge_window P_ ((Lisp_Object, int, int));
static Lisp_Object window_list P_ ((void));
static int add_window_to_list P_ ((struct window *, void *));
static int candidate_window_p P_ ((Lisp_Object, Lisp_Object, Lisp_Object,
				   Lisp_Object));
static Lisp_Object next_window P_ ((Lisp_Object, Lisp_Object,
				    Lisp_Object, int));
static void decode_next_window_args P_ ((Lisp_Object *, Lisp_Object *,
					 Lisp_Object *));
static int foreach_window_1 P_ ((struct window *,
				 int (* fn) (struct window *, void *),
				 void *));
static Lisp_Object window_list_1 P_ ((Lisp_Object, Lisp_Object, Lisp_Object));

/* This is the window in which the terminal's cursor should
   be left when nothing is being done with it.  This must
   always be a leaf window, and its buffer is selected by
   the top level editing loop at the end of each command.

   This value is always the same as
   FRAME_SELECTED_WINDOW (selected_frame).  */

Lisp_Object selected_window;

/* A list of all windows for use by next_window and Fwindow_list.
   Functions creating or deleting windows should invalidate this cache
   by setting it to nil.  */

Lisp_Object Vwindow_list;

/* The mini-buffer window of the selected frame.
   Note that you cannot test for mini-bufferness of an arbitrary window
   by comparing against this; but you can test for mini-bufferness of
   the selected window.  */

Lisp_Object minibuf_window;

/* Non-nil means it is the window whose mode line should be
   shown as the selected window when the minibuffer is selected.  */

Lisp_Object minibuf_selected_window;

/* Non-nil means it is the window for C-M-v to scroll
   when the mini-buffer is selected.  */

Lisp_Object Vminibuf_scroll_window;

/* Non-nil means this is the buffer whose window C-M-v should scroll.  */

Lisp_Object Vother_window_scroll_buffer;

/* Non-nil means it's function to call to display temp buffers.  */

Lisp_Object Vtemp_buffer_show_function;

/* Non-zero means line and page scrolling on tall lines (with images)
   does partial scrolling by modifying window-vscroll.  */

int auto_window_vscroll_p;

/* Non-zero means to use mode-line-inactive face in all windows but the
   selected-window and the minibuffer-scroll-window when the
   minibuffer is active.  */
int mode_line_in_non_selected_windows;

/* If a window gets smaller than either of these, it is removed. */

EMACS_INT window_min_height;
EMACS_INT window_min_width;

/* Nonzero implies Fdisplay_buffer should create windows. */

int pop_up_windows;

/* Nonzero implies make new frames for Fdisplay_buffer.  */

int pop_up_frames;

/* Nonzero means reuse existing frames for displaying buffers.  */

int display_buffer_reuse_frames;

/* Non-nil means use this function instead of default */

Lisp_Object Vpop_up_frame_function;

/* Function to call to handle Fdisplay_buffer.  */

Lisp_Object Vdisplay_buffer_function;

/* Non-nil means that Fdisplay_buffer should even the heights of windows.  */

Lisp_Object Veven_window_heights;

/* List of buffer *names* for buffers that should have their own frames.  */

Lisp_Object Vspecial_display_buffer_names;

/* List of regexps for buffer names that should have their own frames.  */

Lisp_Object Vspecial_display_regexps;

/* Function to pop up a special frame.  */

Lisp_Object Vspecial_display_function;

/* List of buffer *names* for buffers to appear in selected window.  */

Lisp_Object Vsame_window_buffer_names;

/* List of regexps for buffer names to appear in selected window.  */

Lisp_Object Vsame_window_regexps;

/* Hook run at end of temp_output_buffer_show.  */

Lisp_Object Qtemp_buffer_show_hook;

/* Fdisplay_buffer always splits the largest window
   if that window is more than this high.  */

EMACS_INT split_height_threshold;

/* Number of lines of continuity in scrolling by screenfuls.  */

EMACS_INT next_screen_context_lines;

/* Incremented for each window created.  */

static int sequence_number;

/* Nonzero after init_window_once has finished.  */

static int window_initialized;

/* Hook to run when window config changes.  */

Lisp_Object Qwindow_configuration_change_hook;
Lisp_Object Vwindow_configuration_change_hook;

/* Non-nil means scroll commands try to put point
   at the same screen height as previously.  */

Lisp_Object Vscroll_preserve_screen_position;

/* Incremented by 1 whenever a window is deleted.  */

int window_deletion_count;

/* Used by the function window_scroll_pixel_based */

static int window_scroll_pixel_based_preserve_y;

#if 0 /* This isn't used anywhere.  */
/* Nonzero means we can split a frame even if it is "unsplittable".  */
static int inhibit_frame_unsplittable;
#endif /* 0 */

extern EMACS_INT scroll_margin;

extern Lisp_Object Qwindow_scroll_functions, Vwindow_scroll_functions;

DEFUN ("windowp", Fwindowp, Swindowp, 1, 1, 0,
       doc: /* Returns t if OBJECT is a window.  */)
     (object)
     Lisp_Object object;
{
  return WINDOWP (object) ? Qt : Qnil;
}

DEFUN ("window-live-p", Fwindow_live_p, Swindow_live_p, 1, 1, 0,
       doc: /* Returns t if OBJECT is a window which is currently visible.  */)
     (object)
     Lisp_Object object;
{
  return WINDOW_LIVE_P (object) ? Qt : Qnil;
}

Lisp_Object
make_window ()
{
  Lisp_Object val;
  register struct window *p;

  p = allocate_window ();
  ++sequence_number;
  XSETFASTINT (p->sequence_number, sequence_number);
  XSETFASTINT (p->left_col, 0);
  XSETFASTINT (p->top_line, 0);
  XSETFASTINT (p->total_lines, 0);
  XSETFASTINT (p->total_cols, 0);
  XSETFASTINT (p->hscroll, 0);
  XSETFASTINT (p->min_hscroll, 0);
  p->orig_top_line = p->orig_total_lines = Qnil;
  p->start = Fmake_marker ();
  p->pointm = Fmake_marker ();
  XSETFASTINT (p->use_time, 0);
  p->frame = Qnil;
  p->display_table = Qnil;
  p->dedicated = Qnil;
  p->pseudo_window_p = 0;
  bzero (&p->cursor, sizeof (p->cursor));
  bzero (&p->last_cursor, sizeof (p->last_cursor));
  bzero (&p->phys_cursor, sizeof (p->phys_cursor));
  p->desired_matrix = p->current_matrix = 0;
  p->nrows_scale_factor = p->ncols_scale_factor = 1;
  p->phys_cursor_type = -1;
  p->phys_cursor_width = -1;
  p->must_be_updated_p = 0;
  XSETFASTINT (p->window_end_vpos, 0);
  XSETFASTINT (p->window_end_pos, 0);
  p->window_end_valid = Qnil;
  p->vscroll = 0;
  XSETWINDOW (val, p);
  XSETFASTINT (p->last_point, 0);
  p->frozen_window_start_p = 0;
  p->last_cursor_off_p = p->cursor_off_p = 0;
  p->left_margin_cols = Qnil;
  p->right_margin_cols = Qnil;
  p->left_fringe_width = Qnil;
  p->right_fringe_width = Qnil;
  p->fringes_outside_margins = Qnil;
  p->scroll_bar_width = Qnil;
  p->vertical_scroll_bar_type = Qt;

  Vwindow_list = Qnil;
  return val;
}

DEFUN ("selected-window", Fselected_window, Sselected_window, 0, 0, 0,
       doc: /* Return the window that the cursor now appears in and commands apply to.  */)
     ()
{
  return selected_window;
}

DEFUN ("minibuffer-window", Fminibuffer_window, Sminibuffer_window, 0, 1, 0,
       doc: /* Return the window used now for minibuffers.
If the optional argument FRAME is specified, return the minibuffer window
used by that frame.  */)
     (frame)
    Lisp_Object frame;
{
  if (NILP (frame))
    frame = selected_frame;
  CHECK_LIVE_FRAME (frame);
  return FRAME_MINIBUF_WINDOW (XFRAME (frame));
}

DEFUN ("window-minibuffer-p", Fwindow_minibuffer_p, Swindow_minibuffer_p, 0, 1, 0,
       doc: /* Returns non-nil if WINDOW is a minibuffer window.
WINDOW defaults to the selected window.  */)
     (window)
     Lisp_Object window;
{
  struct window *w = decode_window (window);
  return MINI_WINDOW_P (w) ? Qt : Qnil;
}


DEFUN ("pos-visible-in-window-p", Fpos_visible_in_window_p,
       Spos_visible_in_window_p, 0, 3, 0,
       doc: /* Return non-nil if position POS is currently on the frame in WINDOW.
Return nil if that position is scrolled vertically out of view.
If a character is only partially visible, nil is returned, unless the
optional argument PARTIALLY is non-nil.
If POS is only out of view because of horizontal scrolling, return non-nil.
If POS is t, it specifies the position of the last visible glyph in WINDOW.
POS defaults to point in WINDOW; WINDOW defaults to the selected window.

If POS is visible, return t if PARTIALLY is nil; if PARTIALLY is non-nil,
return value is a list of 2 or 6 elements (X Y [RTOP RBOT ROWH VPOS]),
where X and Y are the pixel coordinates relative to the top left corner
of the window.  The remaining elements are omitted if the character after
POS is fully visible; otherwise, RTOP and RBOT are the number of pixels
off-window at the top and bottom of the row, ROWH is the height of the
display row, and VPOS is the row number (0-based) containing POS.  */)
     (pos, window, partially)
     Lisp_Object pos, window, partially;
{
  register struct window *w;
  register int posint;
  register struct buffer *buf;
  struct text_pos top;
  Lisp_Object in_window = Qnil;
  int rtop, rbot, rowh, vpos, fully_p = 1;
  int x, y;

  w = decode_window (window);
  buf = XBUFFER (w->buffer);
  SET_TEXT_POS_FROM_MARKER (top, w->start);

  if (EQ (pos, Qt))
    posint = -1;
  else if (!NILP (pos))
    {
      CHECK_NUMBER_COERCE_MARKER (pos);
      posint = XINT (pos);
    }
  else if (w == XWINDOW (selected_window))
    posint = PT;
  else
    posint = XMARKER (w->pointm)->charpos;

  /* If position is above window start or outside buffer boundaries,
     or if window start is out of range, position is not visible.  */
  if ((EQ (pos, Qt)
       || (posint >= CHARPOS (top) && posint <= BUF_ZV (buf)))
      && CHARPOS (top) >= BUF_BEGV (buf)
      && CHARPOS (top) <= BUF_ZV (buf)
      && pos_visible_p (w, posint, &x, &y, &rtop, &rbot, &rowh, &vpos)
      && (fully_p = !rtop && !rbot, (!NILP (partially) || fully_p)))
    in_window = Qt;

  if (!NILP (in_window) && !NILP (partially))
    {
      Lisp_Object part = Qnil;
      if (!fully_p)
	part = list4 (make_number (rtop), make_number (rbot),
			make_number (rowh), make_number (vpos));
      in_window = Fcons (make_number (x),
			 Fcons (make_number (y), part));
    }

  return in_window;
}

DEFUN ("window-line-height", Fwindow_line_height,
       Swindow_line_height, 0, 2, 0,
       doc: /* Return height in pixels of text line LINE in window WINDOW.
If WINDOW is nil or omitted, use selected window.

Return height of current line if LINE is omitted or nil.  Return height of
header or mode line if LINE is `header-line' and `mode-line'.
Otherwise, LINE is a text line number starting from 0.  A negative number
counts from the end of the window.

Value is a list (HEIGHT VPOS YPOS OFFBOT), where HEIGHT is the height
in pixels of the visible part of the line, VPOS and YPOS are the
vertical position in lines and pixels of the line, relative to the top
of the first text line, and OFFBOT is the number of off-window pixels at
the bottom of the text line.  If there are off-window pixels at the top
of the (first) text line, YPOS is negative.

Return nil if window display is not up-to-date.  In that case, use
`pos-visible-in-window-p' to obtain the information.  */)
     (line, window)
     Lisp_Object line, window;
{
  register struct window *w;
  register struct buffer *b;
  struct glyph_row *row, *end_row;
  int max_y, crop, i, n;

  w = decode_window (window);

  if (noninteractive
      || w->pseudo_window_p)
    return Qnil;

  CHECK_BUFFER (w->buffer);
  b = XBUFFER (w->buffer);

  /* Fail if current matrix is not up-to-date.  */
  if (NILP (w->window_end_valid)
      || current_buffer->clip_changed
      || current_buffer->prevent_redisplay_optimizations_p
      || XFASTINT (w->last_modified) < BUF_MODIFF (b)
      || XFASTINT (w->last_overlay_modified) < BUF_OVERLAY_MODIFF (b))
    return Qnil;

  if (NILP (line))
    {
      i = w->cursor.vpos;
      if (i < 0 || i >= w->current_matrix->nrows
	  || (row = MATRIX_ROW (w->current_matrix, i), !row->enabled_p))
	return Qnil;
      max_y = window_text_bottom_y (w);
      goto found_row;
    }

  if (EQ (line, Qheader_line))
    {
      if (!WINDOW_WANTS_HEADER_LINE_P (w))
	return Qnil;
      row = MATRIX_HEADER_LINE_ROW (w->current_matrix);
      if (!row->enabled_p)
	return Qnil;
      return list4 (make_number (row->height),
		    make_number (0), make_number (0),
		    make_number (0));
    }

  if (EQ (line, Qmode_line))
    {
      row = MATRIX_MODE_LINE_ROW (w->current_matrix);
      if (!row->enabled_p)
	return Qnil;
      return list4 (make_number (row->height),
		    make_number (0), /* not accurate */
		    make_number (WINDOW_HEADER_LINE_HEIGHT (w)
				 + window_text_bottom_y (w)),
		    make_number (0));
    }

  CHECK_NUMBER (line);
  n = XINT (line);

  row = MATRIX_FIRST_TEXT_ROW (w->current_matrix);
  end_row = MATRIX_BOTTOM_TEXT_ROW (w->current_matrix, w);
  max_y = window_text_bottom_y (w);
  i = 0;

  while ((n < 0 || i < n)
	 && row <= end_row && row->enabled_p
	 && row->y + row->height < max_y)
    row++, i++;

  if (row > end_row || !row->enabled_p)
    return Qnil;

  if (++n < 0)
    {
      if (-n > i)
	return Qnil;
      row += n;
      i += n;
    }

 found_row:
  crop = max (0, (row->y + row->height) - max_y);
  return list4 (make_number (row->height + min (0, row->y) - crop),
		make_number (i),
		make_number (row->y),
		make_number (crop));
}



static struct window *
decode_window (window)
     register Lisp_Object window;
{
  if (NILP (window))
    return XWINDOW (selected_window);

  CHECK_LIVE_WINDOW (window);
  return XWINDOW (window);
}

static struct window *
decode_any_window (window)
     register Lisp_Object window;
{
  if (NILP (window))
    return XWINDOW (selected_window);

  CHECK_WINDOW (window);
  return XWINDOW (window);
}

DEFUN ("window-buffer", Fwindow_buffer, Swindow_buffer, 0, 1, 0,
       doc: /* Return the buffer that WINDOW is displaying.
WINDOW defaults to the selected window.  */)
     (window)
     Lisp_Object window;
{
  return decode_window (window)->buffer;
}

DEFUN ("window-height", Fwindow_height, Swindow_height, 0, 1, 0,
       doc: /* Return the number of lines in WINDOW (including its mode line).
WINDOW defaults to the selected window.  */)
     (window)
     Lisp_Object window;
{
  return decode_any_window (window)->total_lines;
}

DEFUN ("window-width", Fwindow_width, Swindow_width, 0, 1, 0,
       doc: /* Return the number of display columns in WINDOW.
This is the width that is usable columns available for text in WINDOW.
If you want to find out how many columns WINDOW takes up,
use  (let ((edges (window-edges))) (- (nth 2 edges) (nth 0 edges))).  */)
     (window)
     Lisp_Object window;
{
  return make_number (window_box_text_cols (decode_any_window (window)));
}

DEFUN ("window-hscroll", Fwindow_hscroll, Swindow_hscroll, 0, 1, 0,
       doc: /* Return the number of columns by which WINDOW is scrolled from left margin.
WINDOW defaults to the selected window.  */)
     (window)
     Lisp_Object window;
{
  return decode_window (window)->hscroll;
}

DEFUN ("set-window-hscroll", Fset_window_hscroll, Sset_window_hscroll, 2, 2, 0,
       doc: /* Set number of columns WINDOW is scrolled from left margin to NCOL.
Return NCOL.  NCOL should be zero or positive.

Note that if `automatic-hscrolling' is non-nil, you cannot scroll the
window so that the location of point moves off-window.  */)
     (window, ncol)
     Lisp_Object window, ncol;
{
  struct window *w = decode_window (window);
  int hscroll;

  CHECK_NUMBER (ncol);
  hscroll = max (0, XINT (ncol));

  /* Prevent redisplay shortcuts when changing the hscroll.  */
  if (XINT (w->hscroll) != hscroll)
    XBUFFER (w->buffer)->prevent_redisplay_optimizations_p = 1;

  w->hscroll = make_number (hscroll);
  return ncol;
}

DEFUN ("window-redisplay-end-trigger", Fwindow_redisplay_end_trigger,
       Swindow_redisplay_end_trigger, 0, 1, 0,
       doc: /* Return WINDOW's redisplay end trigger value.
WINDOW defaults to the selected window.
See `set-window-redisplay-end-trigger' for more information.  */)
     (window)
     Lisp_Object window;
{
  return decode_window (window)->redisplay_end_trigger;
}

DEFUN ("set-window-redisplay-end-trigger", Fset_window_redisplay_end_trigger,
       Sset_window_redisplay_end_trigger, 2, 2, 0,
       doc: /* Set WINDOW's redisplay end trigger value to VALUE.
VALUE should be a buffer position (typically a marker) or nil.
If it is a buffer position, then if redisplay in WINDOW reaches a position
beyond VALUE, the functions in `redisplay-end-trigger-functions' are called
with two arguments: WINDOW, and the end trigger value.
Afterwards the end-trigger value is reset to nil.  */)
     (window, value)
     register Lisp_Object window, value;
{
  register struct window *w;

  w = decode_window (window);
  w->redisplay_end_trigger = value;
  return value;
}

DEFUN ("window-edges", Fwindow_edges, Swindow_edges, 0, 1, 0,
       doc: /* Return a list of the edge coordinates of WINDOW.
\(LEFT TOP RIGHT BOTTOM), all relative to 0, 0 at top left corner of frame.
RIGHT is one more than the rightmost column occupied by WINDOW,
and BOTTOM is one more than the bottommost row occupied by WINDOW.
The edges include the space used by the window's scroll bar,
display margins, fringes, header line, and mode line, if it has them.
To get the edges of the actual text area, use `window-inside-edges'.  */)
     (window)
     Lisp_Object window;
{
  register struct window *w = decode_any_window (window);

  return Fcons (make_number (WINDOW_LEFT_EDGE_COL (w)),
	 Fcons (make_number (WINDOW_TOP_EDGE_LINE (w)),
	 Fcons (make_number (WINDOW_RIGHT_EDGE_COL (w)),
	 Fcons (make_number (WINDOW_BOTTOM_EDGE_LINE (w)),
		Qnil))));
}

DEFUN ("window-pixel-edges", Fwindow_pixel_edges, Swindow_pixel_edges, 0, 1, 0,
       doc: /* Return a list of the edge pixel coordinates of WINDOW.
\(LEFT TOP RIGHT BOTTOM), all relative to 0, 0 at top left corner of frame.
RIGHT is one more than the rightmost x position occupied by WINDOW,
and BOTTOM is one more than the bottommost y position occupied by WINDOW.
The pixel edges include the space used by the window's scroll bar,
display margins, fringes, header line, and mode line, if it has them.
To get the edges of the actual text area, use `window-inside-pixel-edges'.  */)
     (window)
     Lisp_Object window;
{
  register struct window *w = decode_any_window (window);

  return Fcons (make_number (WINDOW_LEFT_EDGE_X (w)),
	 Fcons (make_number (WINDOW_TOP_EDGE_Y (w)),
	 Fcons (make_number (WINDOW_RIGHT_EDGE_X (w)),
	 Fcons (make_number (WINDOW_BOTTOM_EDGE_Y (w)),
		Qnil))));
}

DEFUN ("window-inside-edges", Fwindow_inside_edges, Swindow_inside_edges, 0, 1, 0,
       doc: /* Return a list of the edge coordinates of WINDOW.
\(LEFT TOP RIGHT BOTTOM), all relative to 0, 0 at top left corner of frame.
RIGHT is one more than the rightmost column used by text in WINDOW,
and BOTTOM is one more than the bottommost row used by text in WINDOW.
The inside edges do not include the space used by the window's scroll bar,
display margins, fringes, header line, and/or mode line.  */)
     (window)
     Lisp_Object window;
{
  register struct window *w = decode_any_window (window);

  return list4 (make_number (WINDOW_BOX_LEFT_EDGE_COL (w)
			     + WINDOW_LEFT_MARGIN_COLS (w)
			     + WINDOW_LEFT_FRINGE_COLS (w)),
		make_number (WINDOW_TOP_EDGE_LINE (w)
			     + WINDOW_HEADER_LINE_LINES (w)),
		make_number (WINDOW_BOX_RIGHT_EDGE_COL (w)
			     - WINDOW_RIGHT_MARGIN_COLS (w)
			     - WINDOW_RIGHT_FRINGE_COLS (w)),
		make_number (WINDOW_BOTTOM_EDGE_LINE (w)
			     - WINDOW_MODE_LINE_LINES (w)));
}

DEFUN ("window-inside-pixel-edges", Fwindow_inside_pixel_edges, Swindow_inside_pixel_edges, 0, 1, 0,
       doc: /* Return a list of the edge pixel coordinates of WINDOW.
\(LEFT TOP RIGHT BOTTOM), all relative to 0, 0 at top left corner of frame.
RIGHT is one more than the rightmost x position used by text in WINDOW,
and BOTTOM is one more than the bottommost y position used by text in WINDOW.
The inside edges do not include the space used by the window's scroll bar,
display margins, fringes, header line, and/or mode line.  */)
     (window)
     Lisp_Object window;
{
  register struct window *w = decode_any_window (window);

  return list4 (make_number (WINDOW_BOX_LEFT_EDGE_X (w)
			     + WINDOW_LEFT_MARGIN_WIDTH (w)
			     + WINDOW_LEFT_FRINGE_WIDTH (w)),
		make_number (WINDOW_TOP_EDGE_Y (w)
			     + WINDOW_HEADER_LINE_HEIGHT (w)),
		make_number (WINDOW_BOX_RIGHT_EDGE_X (w)
			     - WINDOW_RIGHT_MARGIN_WIDTH (w)
			     - WINDOW_RIGHT_FRINGE_WIDTH (w)),
		make_number (WINDOW_BOTTOM_EDGE_Y (w)
			     - WINDOW_MODE_LINE_HEIGHT (w)));
}

/* Test if the character at column *X, row *Y is within window W.
   If it is not, return ON_NOTHING;
   if it is in the window's text area,
      set *x and *y to its location relative to the upper left corner
         of the window, and
      return ON_TEXT;
   if it is on the window's modeline, return ON_MODE_LINE;
   if it is on the border between the window and its right sibling,
      return ON_VERTICAL_BORDER.
   if it is on a scroll bar,
      return ON_SCROLL_BAR.
   if it is on the window's top line, return ON_HEADER_LINE;
   if it is in left or right fringe of the window,
      return ON_LEFT_FRINGE or ON_RIGHT_FRINGE, and convert *X and *Y
      to window-relative coordinates;
   if it is in the marginal area to the left/right of the window,
      return ON_LEFT_MARGIN or ON_RIGHT_MARGIN, and convert *X and *Y
      to window-relative coordinates.

   X and Y are frame relative pixel coordinates.  */

static enum window_part
coordinates_in_window (w, x, y)
     register struct window *w;
     register int *x, *y;
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  int left_x, right_x, top_y, bottom_y;
  enum window_part part;
  int ux = FRAME_COLUMN_WIDTH (f);
  int x0 = WINDOW_LEFT_EDGE_X (w);
  int x1 = WINDOW_RIGHT_EDGE_X (w);
  /* The width of the area where the vertical line can be dragged.
     (Between mode lines for instance.  */
  int grabbable_width = ux;
  int lmargin_width, rmargin_width, text_left, text_right;

  /* In what's below, we subtract 1 when computing right_x because we
     want the rightmost pixel, which is given by left_pixel+width-1.  */
  if (w->pseudo_window_p)
    {
      left_x = 0;
      right_x = WINDOW_TOTAL_WIDTH (w) - 1;
      top_y = WINDOW_TOP_EDGE_Y (w);
      bottom_y = WINDOW_BOTTOM_EDGE_Y (w);
    }
  else
    {
      left_x = WINDOW_BOX_LEFT_EDGE_X (w);
      right_x = WINDOW_BOX_RIGHT_EDGE_X (w) - 1;
      top_y = WINDOW_TOP_EDGE_Y (w);
      bottom_y = WINDOW_BOTTOM_EDGE_Y (w);
    }

  /* Outside any interesting row?  */
  if (*y < top_y || *y >= bottom_y)
    return ON_NOTHING;

  /* On the mode line or header line?  If it's near the start of
     the mode or header line of window that's has a horizontal
     sibling, say it's on the vertical line.  That's to be able
     to resize windows horizontally in case we're using toolkit
     scroll bars.  */

  if (WINDOW_WANTS_MODELINE_P (w)
      && *y >= bottom_y - CURRENT_MODE_LINE_HEIGHT (w))
    {
      part = ON_MODE_LINE;

    header_vertical_border_check:
      /* We're somewhere on the mode line.  We consider the place
	 between mode lines of horizontally adjacent mode lines
	 as the vertical border.    If scroll bars on the left,
	 return the right window.  */
      if (WINDOW_HAS_VERTICAL_SCROLL_BAR_ON_LEFT (w)
	  || WINDOW_RIGHTMOST_P (w))
	{
	  if (!WINDOW_LEFTMOST_P (w) && abs (*x - x0) < grabbable_width)
	    {
	      /* Convert X and Y to window relative coordinates.
		 Vertical border is at the left edge of window.  */
	      *x = max (0, *x - x0);
	      *y -= top_y;
	      return ON_VERTICAL_BORDER;
	    }
	}
      else
	{
	  if (abs (*x - x1) < grabbable_width)
	    {
	      /* Convert X and Y to window relative coordinates.
		 Vertical border is at the right edge of window.  */
	      *x = min (x1, *x) - x0;
	      *y -= top_y;
	      return ON_VERTICAL_BORDER;
	    }
	}

      if (*x < x0 || *x >= x1)
	return ON_NOTHING;

      /* Convert X and Y to window relative coordinates.
	 Mode line starts at left edge of window.  */
      *x -= x0;
      *y -= top_y;
      return part;
    }

  if (WINDOW_WANTS_HEADER_LINE_P (w)
      && *y < top_y + CURRENT_HEADER_LINE_HEIGHT (w))
    {
      part = ON_HEADER_LINE;
      goto header_vertical_border_check;
    }

  if (*x < x0 || *x >= x1)
    return ON_NOTHING;

  /* Outside any interesting column?  */
  if (*x < left_x || *x > right_x)
    {
      *y -= top_y;
      return ON_SCROLL_BAR;
    }

  lmargin_width = window_box_width (w, LEFT_MARGIN_AREA);
  rmargin_width = window_box_width (w, RIGHT_MARGIN_AREA);

  text_left = window_box_left (w, TEXT_AREA);
  text_right = text_left + window_box_width (w, TEXT_AREA);

  if (FRAME_WINDOW_P (f))
    {
      if (!w->pseudo_window_p
	  && !WINDOW_HAS_VERTICAL_SCROLL_BAR (w)
	  && !WINDOW_RIGHTMOST_P (w)
	  && (abs (*x - right_x) < grabbable_width))
	{
	  /* Convert X and Y to window relative coordinates.
	     Vertical border is at the right edge of window.  */
	  *x = min (right_x, *x) - left_x;
	  *y -= top_y;
	  return ON_VERTICAL_BORDER;
	}
    }
  else
    {
      /* Need to say "*x > right_x" rather than >=, since on character
	 terminals, the vertical line's x coordinate is right_x.  */
      if (!w->pseudo_window_p
	  && !WINDOW_RIGHTMOST_P (w)
	  && *x > right_x - ux)
	{
	  /* On the border on the right side of the window?  Assume that
	     this area begins at RIGHT_X minus a canonical char width.  */
	  *x = min (right_x, *x) - left_x;
	  *y -= top_y;
	  return ON_VERTICAL_BORDER;
	}
    }

  if (*x < text_left)
    {
      if (lmargin_width > 0
	  && (WINDOW_HAS_FRINGES_OUTSIDE_MARGINS (w)
	      ? (*x >= left_x + WINDOW_LEFT_FRINGE_WIDTH (w))
	      : (*x < left_x + lmargin_width)))
	{
	  *x -= left_x;
	  if (WINDOW_HAS_FRINGES_OUTSIDE_MARGINS (w))
	    *x -= WINDOW_LEFT_FRINGE_WIDTH (w);
	  *y -= top_y;
	  return ON_LEFT_MARGIN;
	}

      /* Convert X and Y to window-relative pixel coordinates.  */
      *x -= left_x;
      *y -= top_y;
      return ON_LEFT_FRINGE;
    }

  if (*x >= text_right)
    {
      if (rmargin_width > 0
	  && (WINDOW_HAS_FRINGES_OUTSIDE_MARGINS (w)
	      ? (*x < right_x - WINDOW_RIGHT_FRINGE_WIDTH (w))
	      : (*x >= right_x - rmargin_width)))
	{
	  *x -= right_x - rmargin_width;
	  if (WINDOW_HAS_FRINGES_OUTSIDE_MARGINS (w))
	    *x += WINDOW_RIGHT_FRINGE_WIDTH (w);
	  *y -= top_y;
	  return ON_RIGHT_MARGIN;
	}

      /* Convert X and Y to window-relative pixel coordinates.  */
      *x -= left_x + WINDOW_LEFT_FRINGE_WIDTH (w);
      *y -= top_y;
      return ON_RIGHT_FRINGE;
    }

  /* Everything special ruled out - must be on text area */
  *x -= text_left;
  *y -= top_y;
  return ON_TEXT;
}


DEFUN ("coordinates-in-window-p", Fcoordinates_in_window_p,
       Scoordinates_in_window_p, 2, 2, 0,
       doc: /* Return non-nil if COORDINATES are in WINDOW.
COORDINATES is a cons of the form (X . Y), X and Y being distances
measured in characters from the upper-left corner of the frame.
\(0 .  0) denotes the character in the upper left corner of the
frame.
If COORDINATES are in the text portion of WINDOW,
   the coordinates relative to the window are returned.
If they are in the mode line of WINDOW, `mode-line' is returned.
If they are in the top mode line of WINDOW, `header-line' is returned.
If they are in the left fringe of WINDOW, `left-fringe' is returned.
If they are in the right fringe of WINDOW, `right-fringe' is returned.
If they are on the border between WINDOW and its right sibling,
  `vertical-line' is returned.
If they are in the windows's left or right marginal areas, `left-margin'\n\
  or `right-margin' is returned.  */)
     (coordinates, window)
     register Lisp_Object coordinates, window;
{
  struct window *w;
  struct frame *f;
  int x, y;
  Lisp_Object lx, ly;

  CHECK_WINDOW (window);
  w = XWINDOW (window);
  f = XFRAME (w->frame);
  CHECK_CONS (coordinates);
  lx = Fcar (coordinates);
  ly = Fcdr (coordinates);
  CHECK_NUMBER_OR_FLOAT (lx);
  CHECK_NUMBER_OR_FLOAT (ly);
  x = FRAME_PIXEL_X_FROM_CANON_X (f, lx) + FRAME_INTERNAL_BORDER_WIDTH (f);
  y = FRAME_PIXEL_Y_FROM_CANON_Y (f, ly) + FRAME_INTERNAL_BORDER_WIDTH (f);

  switch (coordinates_in_window (w, &x, &y))
    {
    case ON_NOTHING:
      return Qnil;

    case ON_TEXT:
      /* X and Y are now window relative pixel coordinates.  Convert
	 them to canonical char units before returning them.  */
      return Fcons (FRAME_CANON_X_FROM_PIXEL_X (f, x),
		    FRAME_CANON_Y_FROM_PIXEL_Y (f, y));

    case ON_MODE_LINE:
      return Qmode_line;

    case ON_VERTICAL_BORDER:
      return Qvertical_line;

    case ON_HEADER_LINE:
      return Qheader_line;

    case ON_LEFT_FRINGE:
      return Qleft_fringe;

    case ON_RIGHT_FRINGE:
      return Qright_fringe;

    case ON_LEFT_MARGIN:
      return Qleft_margin;

    case ON_RIGHT_MARGIN:
      return Qright_margin;

    case ON_SCROLL_BAR:
      /* Historically we are supposed to return nil in this case.  */
      return Qnil;

    default:
      abort ();
    }
}


/* Callback for foreach_window, used in window_from_coordinates.
   Check if window W contains coordinates specified by USER_DATA which
   is actually a pointer to a struct check_window_data CW.

   Check if window W contains coordinates *CW->x and *CW->y.  If it
   does, return W in *CW->window, as Lisp_Object, and return in
   *CW->part the part of the window under coordinates *X,*Y.  Return
   zero from this function to stop iterating over windows.  */

struct check_window_data
{
  Lisp_Object *window;
  int *x, *y;
  enum window_part *part;
};

static int
check_window_containing (w, user_data)
     struct window *w;
     void *user_data;
{
  struct check_window_data *cw = (struct check_window_data *) user_data;
  enum window_part found;
  int continue_p = 1;

  found = coordinates_in_window (w, cw->x, cw->y);
  if (found != ON_NOTHING)
    {
      *cw->part = found;
      XSETWINDOW (*cw->window, w);
      continue_p = 0;
    }

  return continue_p;
}


/* Find the window containing frame-relative pixel position X/Y and
   return it as a Lisp_Object.

   If X, Y is on one of the window's special `window_part' elements,
   set *PART to the id of that element, and return X and Y converted
   to window relative coordinates in WX and WY.

   If there is no window under X, Y return nil and leave *PART
   unmodified.  TOOL_BAR_P non-zero means detect tool-bar windows.

   This function was previously implemented with a loop cycling over
   windows with Fnext_window, and starting with the frame's selected
   window.  It turned out that this doesn't work with an
   implementation of next_window using Vwindow_list, because
   FRAME_SELECTED_WINDOW (F) is not always contained in the window
   tree of F when this function is called asynchronously from
   note_mouse_highlight.  The original loop didn't terminate in this
   case.  */

Lisp_Object
window_from_coordinates (f, x, y, part, wx, wy, tool_bar_p)
     struct frame *f;
     int x, y;
     enum window_part *part;
     int *wx, *wy;
     int tool_bar_p;
{
  Lisp_Object window;
  struct check_window_data cw;
  enum window_part dummy;

  if (part == 0)
    part = &dummy;

  window = Qnil;
  cw.window = &window, cw.x = &x, cw.y = &y; cw.part = part;
  foreach_window (f, check_window_containing, &cw);

  /* If not found above, see if it's in the tool bar window, if a tool
     bar exists.  */
  if (NILP (window)
      && tool_bar_p
      && WINDOWP (f->tool_bar_window)
      && WINDOW_TOTAL_LINES (XWINDOW (f->tool_bar_window)) > 0
      && (coordinates_in_window (XWINDOW (f->tool_bar_window), &x, &y)
	  != ON_NOTHING))
    {
      *part = ON_TEXT;
      window = f->tool_bar_window;
    }

  if (wx) *wx = x;
  if (wy) *wy = y;

  return window;
}

DEFUN ("window-at", Fwindow_at, Swindow_at, 2, 3, 0,
       doc: /* Return window containing coordinates X and Y on FRAME.
If omitted, FRAME defaults to the currently selected frame.
The top left corner of the frame is considered to be row 0,
column 0.  */)
     (x, y, frame)
     Lisp_Object x, y, frame;
{
  struct frame *f;

  if (NILP (frame))
    frame = selected_frame;
  CHECK_LIVE_FRAME (frame);
  f = XFRAME (frame);

  /* Check that arguments are integers or floats.  */
  CHECK_NUMBER_OR_FLOAT (x);
  CHECK_NUMBER_OR_FLOAT (y);

  return window_from_coordinates (f,
				  (FRAME_PIXEL_X_FROM_CANON_X (f, x)
				   + FRAME_INTERNAL_BORDER_WIDTH (f)),
				  (FRAME_PIXEL_Y_FROM_CANON_Y (f, y)
				   + FRAME_INTERNAL_BORDER_WIDTH (f)),
				  0, 0, 0, 0);
}

DEFUN ("window-point", Fwindow_point, Swindow_point, 0, 1, 0,
       doc: /* Return current value of point in WINDOW.
WINDOW defaults to the selected window.

For a nonselected window, this is the value point would have
if that window were selected.

Note that, when WINDOW is the selected window and its buffer
is also currently selected, the value returned is the same as (point).
It would be more strictly correct to return the `top-level' value
of point, outside of any save-excursion forms.
But that is hard to define.  */)
     (window)
     Lisp_Object window;
{
  register struct window *w = decode_window (window);

  if (w == XWINDOW (selected_window)
      && current_buffer == XBUFFER (w->buffer))
    return Fpoint ();
  return Fmarker_position (w->pointm);
}

DEFUN ("window-start", Fwindow_start, Swindow_start, 0, 1, 0,
       doc: /* Return position at which display currently starts in WINDOW.
WINDOW defaults to the selected window.
This is updated by redisplay or by calling `set-window-start'.  */)
     (window)
     Lisp_Object window;
{
  return Fmarker_position (decode_window (window)->start);
}

/* This is text temporarily removed from the doc string below.

This function returns nil if the position is not currently known.
That happens when redisplay is preempted and doesn't finish.
If in that case you want to compute where the end of the window would
have been if redisplay had finished, do this:
    (save-excursion
      (goto-char (window-start window))
      (vertical-motion (1- (window-height window)) window)
      (point))")  */

DEFUN ("window-end", Fwindow_end, Swindow_end, 0, 2, 0,
       doc: /* Return position at which display currently ends in WINDOW.
WINDOW defaults to the selected window.
This is updated by redisplay, when it runs to completion.
Simply changing the buffer text or setting `window-start'
does not update this value.
Return nil if there is no recorded value.  \(This can happen if the
last redisplay of WINDOW was preempted, and did not finish.)
If UPDATE is non-nil, compute the up-to-date position
if it isn't already recorded.  */)
     (window, update)
     Lisp_Object window, update;
{
  Lisp_Object value;
  struct window *w = decode_window (window);
  Lisp_Object buf;
  struct buffer *b;

  buf = w->buffer;
  CHECK_BUFFER (buf);
  b = XBUFFER (buf);

#if 0 /* This change broke some things.  We should make it later.  */
  /* If we don't know the end position, return nil.
     The user can compute it with vertical-motion if he wants to.
     It would be nicer to do it automatically,
     but that's so slow that it would probably bother people.  */
  if (NILP (w->window_end_valid))
    return Qnil;
#endif

  if (! NILP (update)
      && ! (! NILP (w->window_end_valid)
	    && XFASTINT (w->last_modified) >= BUF_MODIFF (b)
	    && XFASTINT (w->last_overlay_modified) >= BUF_OVERLAY_MODIFF (b))
      && !noninteractive)
    {
      struct text_pos startp;
      struct it it;
      struct buffer *old_buffer = NULL;

      /* Cannot use Fvertical_motion because that function doesn't
	 cope with variable-height lines.  */
      if (b != current_buffer)
	{
	  old_buffer = current_buffer;
	  set_buffer_internal (b);
	}

      /* In case W->start is out of the range, use something
         reasonable.  This situation occurred when loading a file with
         `-l' containing a call to `rmail' with subsequent other
         commands.  At the end, W->start happened to be BEG, while
         rmail had already narrowed the buffer.  */
      if (XMARKER (w->start)->charpos < BEGV)
	SET_TEXT_POS (startp, BEGV, BEGV_BYTE);
      else if (XMARKER (w->start)->charpos > ZV)
	SET_TEXT_POS (startp, ZV, ZV_BYTE);
      else
	SET_TEXT_POS_FROM_MARKER (startp, w->start);

      start_display (&it, w, startp);
      move_it_vertically (&it, window_box_height (w));
      if (it.current_y < it.last_visible_y)
	move_it_past_eol (&it);
      value = make_number (IT_CHARPOS (it));

      if (old_buffer)
	set_buffer_internal (old_buffer);
    }
  else
    XSETINT (value, BUF_Z (b) - XFASTINT (w->window_end_pos));

  return value;
}

DEFUN ("set-window-point", Fset_window_point, Sset_window_point, 2, 2, 0,
       doc: /* Make point value in WINDOW be at position POS in WINDOW's buffer.
Return POS.  */)
     (window, pos)
     Lisp_Object window, pos;
{
  register struct window *w = decode_window (window);

  CHECK_NUMBER_COERCE_MARKER (pos);
  if (w == XWINDOW (selected_window)
      && XBUFFER (w->buffer) == current_buffer)
    Fgoto_char (pos);
  else
    set_marker_restricted (w->pointm, pos, w->buffer);

  /* We have to make sure that redisplay updates the window to show
     the new value of point.  */
  if (!EQ (window, selected_window))
    ++windows_or_buffers_changed;

  return pos;
}

DEFUN ("set-window-start", Fset_window_start, Sset_window_start, 2, 3, 0,
       doc: /* Make display in WINDOW start at position POS in WINDOW's buffer.
Return POS.
Optional third arg NOFORCE non-nil inhibits next redisplay
from overriding motion of point in order to display at this exact start.  */)
     (window, pos, noforce)
     Lisp_Object window, pos, noforce;
{
  register struct window *w = decode_window (window);

  CHECK_NUMBER_COERCE_MARKER (pos);
  set_marker_restricted (w->start, pos, w->buffer);
  /* this is not right, but much easier than doing what is right. */
  w->start_at_line_beg = Qnil;
  if (NILP (noforce))
    w->force_start = Qt;
  w->update_mode_line = Qt;
  XSETFASTINT (w->last_modified, 0);
  XSETFASTINT (w->last_overlay_modified, 0);
  if (!EQ (window, selected_window))
    windows_or_buffers_changed++;

  return pos;
}

DEFUN ("window-dedicated-p", Fwindow_dedicated_p, Swindow_dedicated_p,
       1, 1, 0,
       doc: /* Return WINDOW's dedicated object, usually t or nil.
See also `set-window-dedicated-p'.  */)
     (window)
     Lisp_Object window;
{
  return decode_window (window)->dedicated;
}

DEFUN ("set-window-dedicated-p", Fset_window_dedicated_p,
       Sset_window_dedicated_p, 2, 2, 0,
       doc: /* Control whether WINDOW is dedicated to the buffer it displays.
If it is dedicated, Emacs will not automatically change
which buffer appears in it.
The second argument is the new value for the dedication flag;
non-nil means yes. */)
     (window, arg)
     Lisp_Object window, arg;
{
  register struct window *w = decode_window (window);

  w->dedicated = arg;

  return w->dedicated;
}

DEFUN ("window-display-table", Fwindow_display_table, Swindow_display_table,
       0, 1, 0,
       doc: /* Return the display-table that WINDOW is using.
WINDOW defaults to the selected window.  */)
     (window)
     Lisp_Object window;
{
  return decode_window (window)->display_table;
}

/* Get the display table for use on window W.  This is either W's
   display table or W's buffer's display table.  Ignore the specified
   tables if they are not valid; if no valid table is specified,
   return 0.  */

struct Lisp_Char_Table *
window_display_table (w)
     struct window *w;
{
  struct Lisp_Char_Table *dp = NULL;

  if (DISP_TABLE_P (w->display_table))
    dp = XCHAR_TABLE (w->display_table);
  else if (BUFFERP (w->buffer))
    {
      struct buffer *b = XBUFFER (w->buffer);

      if (DISP_TABLE_P (b->display_table))
	dp = XCHAR_TABLE (b->display_table);
      else if (DISP_TABLE_P (Vstandard_display_table))
	dp = XCHAR_TABLE (Vstandard_display_table);
    }

  return dp;
}

DEFUN ("set-window-display-table", Fset_window_display_table, Sset_window_display_table, 2, 2, 0,
       doc: /* Set WINDOW's display-table to TABLE.  */)
     (window, table)
     register Lisp_Object window, table;
{
  register struct window *w;

  w = decode_window (window);
  w->display_table = table;
  return table;
}

/* Record info on buffer window w is displaying
   when it is about to cease to display that buffer.  */
static void
unshow_buffer (w)
     register struct window *w;
{
  Lisp_Object buf;
  struct buffer *b;

  buf = w->buffer;
  b = XBUFFER (buf);
  if (b != XMARKER (w->pointm)->buffer)
    abort ();

#if 0
  if (w == XWINDOW (selected_window)
      || ! EQ (buf, XWINDOW (selected_window)->buffer))
    /* Do this except when the selected window's buffer
       is being removed from some other window.  */
#endif
    /* last_window_start records the start position that this buffer
       had in the last window to be disconnected from it.
       Now that this statement is unconditional,
       it is possible for the buffer to be displayed in the
       selected window, while last_window_start reflects another
       window which was recently showing the same buffer.
       Some people might say that might be a good thing.  Let's see.  */
    b->last_window_start = marker_position (w->start);

  /* Point in the selected window's buffer
     is actually stored in that buffer, and the window's pointm isn't used.
     So don't clobber point in that buffer.  */
  if (! EQ (buf, XWINDOW (selected_window)->buffer)
      /* This line helps to fix Horsley's testbug.el bug.  */
      && !(WINDOWP (b->last_selected_window)
	   && w != XWINDOW (b->last_selected_window)
	   && EQ (buf, XWINDOW (b->last_selected_window)->buffer)))
    temp_set_point_both (b,
			 clip_to_bounds (BUF_BEGV (b),
					 XMARKER (w->pointm)->charpos,
					 BUF_ZV (b)),
			 clip_to_bounds (BUF_BEGV_BYTE (b),
					 marker_byte_position (w->pointm),
					 BUF_ZV_BYTE (b)));

  if (WINDOWP (b->last_selected_window)
      && w == XWINDOW (b->last_selected_window))
    b->last_selected_window = Qnil;
}

/* Put replacement into the window structure in place of old. */
static void
replace_window (old, replacement)
     Lisp_Object old, replacement;
{
  register Lisp_Object tem;
  register struct window *o = XWINDOW (old), *p = XWINDOW (replacement);

  /* If OLD is its frame's root_window, then replacement is the new
     root_window for that frame.  */

  if (EQ (old, FRAME_ROOT_WINDOW (XFRAME (o->frame))))
    FRAME_ROOT_WINDOW (XFRAME (o->frame)) = replacement;

  p->left_col = o->left_col;
  p->top_line = o->top_line;
  p->total_cols = o->total_cols;
  p->total_lines = o->total_lines;
  p->desired_matrix = p->current_matrix = 0;
  p->vscroll = 0;
  bzero (&p->cursor, sizeof (p->cursor));
  bzero (&p->last_cursor, sizeof (p->last_cursor));
  bzero (&p->phys_cursor, sizeof (p->phys_cursor));
  p->phys_cursor_type = -1;
  p->phys_cursor_width = -1;
  p->must_be_updated_p = 0;
  p->pseudo_window_p = 0;
  XSETFASTINT (p->window_end_vpos, 0);
  XSETFASTINT (p->window_end_pos, 0);
  p->window_end_valid = Qnil;
  p->frozen_window_start_p = 0;
  p->orig_top_line = p->orig_total_lines = Qnil;

  p->next = tem = o->next;
  if (!NILP (tem))
    XWINDOW (tem)->prev = replacement;

  p->prev = tem = o->prev;
  if (!NILP (tem))
    XWINDOW (tem)->next = replacement;

  p->parent = tem = o->parent;
  if (!NILP (tem))
    {
      if (EQ (XWINDOW (tem)->vchild, old))
	XWINDOW (tem)->vchild = replacement;
      if (EQ (XWINDOW (tem)->hchild, old))
	XWINDOW (tem)->hchild = replacement;
    }

/*** Here, if replacement is a vertical combination
and so is its new parent, we should make replacement's
children be children of that parent instead.  ***/
}

DEFUN ("delete-window", Fdelete_window, Sdelete_window, 0, 1, "",
       doc: /* Remove WINDOW from the display.  Default is selected window.  */)
     (window)
     register Lisp_Object window;
{
  delete_window (window);

  if (! NILP (Vwindow_configuration_change_hook)
      && ! NILP (Vrun_hooks))
    call1 (Vrun_hooks, Qwindow_configuration_change_hook);

  return Qnil;
}

void
delete_window (window)
     register Lisp_Object window;
{
  register Lisp_Object tem, parent, sib;
  register struct window *p;
  register struct window *par;
  struct frame *f;

  /* Because this function is called by other C code on non-leaf
     windows, the CHECK_LIVE_WINDOW macro would choke inappropriately,
     so we can't decode_window here.  */
  if (NILP (window))
    window = selected_window;
  else
    CHECK_WINDOW (window);
  p = XWINDOW (window);

  /* It's a no-op to delete an already-deleted window.  */
  if (NILP (p->buffer)
      && NILP (p->hchild)
      && NILP (p->vchild))
    return;

  parent = p->parent;
  if (NILP (parent))
    error ("Attempt to delete minibuffer or sole ordinary window");
  par = XWINDOW (parent);

  windows_or_buffers_changed++;
  Vwindow_list = Qnil;
  f = XFRAME (WINDOW_FRAME (p));
  FRAME_WINDOW_SIZES_CHANGED (f) = 1;

  /* Are we trying to delete any frame's selected window?  */
  {
    Lisp_Object swindow, pwindow;

    /* See if the frame's selected window is either WINDOW
       or any subwindow of it, by finding all that window's parents
       and comparing each one with WINDOW.  */
    swindow = FRAME_SELECTED_WINDOW (f);

    while (1)
      {
	pwindow = swindow;
	while (!NILP (pwindow))
	  {
	    if (EQ (window, pwindow))
	      break;
	    pwindow = XWINDOW (pwindow)->parent;
	  }

	/* If the window being deleted is not a parent of SWINDOW,
	   then SWINDOW is ok as the new selected window.  */
	if (!EQ (window, pwindow))
	  break;
	/* Otherwise, try another window for SWINDOW.  */
	swindow = Fnext_window (swindow, Qlambda, Qnil);;

	/* If we get back to the frame's selected window,
	   it means there was no acceptable alternative,
	   so we cannot delete.  */
	if (EQ (swindow, FRAME_SELECTED_WINDOW (f)))
	  error ("Cannot delete window");
      }

    /* If we need to change SWINDOW, do it.  */
    if (! EQ (swindow, FRAME_SELECTED_WINDOW (f)))
      {
	/* If we're about to delete the selected window on the
	   selected frame, then we should use Fselect_window to select
	   the new window.  On the other hand, if we're about to
	   delete the selected window on any other frame, we shouldn't do
	   anything but set the frame's selected_window slot.  */
	if (EQ (FRAME_SELECTED_WINDOW (f), selected_window))
	  Fselect_window (swindow, Qnil);
	else
	  FRAME_SELECTED_WINDOW (f) = swindow;
      }
  }

  /* Now we know we can delete this one.  */
  window_deletion_count++;

  tem = p->buffer;
  /* tem is null for dummy parent windows
     (which have inferiors but not any contents themselves) */
  if (!NILP (tem))
    {
      unshow_buffer (p);
      unchain_marker (XMARKER (p->pointm));
      unchain_marker (XMARKER (p->start));
    }

  /* Free window glyph matrices.  It is sure that they are allocated
     again when ADJUST_GLYPHS is called.  Block input so that expose
     events and other events that access glyph matrices are not
     processed while we are changing them.  */
  BLOCK_INPUT;
  free_window_matrices (XWINDOW (FRAME_ROOT_WINDOW (f)));

  tem = p->next;
  if (!NILP (tem))
    XWINDOW (tem)->prev = p->prev;

  tem = p->prev;
  if (!NILP (tem))
    XWINDOW (tem)->next = p->next;

  if (EQ (window, par->hchild))
    par->hchild = p->next;
  if (EQ (window, par->vchild))
    par->vchild = p->next;

  /* Find one of our siblings to give our space to.  */
  sib = p->prev;
  if (NILP (sib))
    {
      /* If p gives its space to its next sibling, that sibling needs
	 to have its top/left side pulled back to where p's is.
	 set_window_{height,width} will re-position the sibling's
	 children.  */
      sib = p->next;
      XWINDOW (sib)->top_line = p->top_line;
      XWINDOW (sib)->left_col = p->left_col;
    }

  /* Stretch that sibling.  */
  if (!NILP (par->vchild))
    set_window_height (sib,
		       XFASTINT (XWINDOW (sib)->total_lines) + XFASTINT (p->total_lines),
		       1);
  if (!NILP (par->hchild))
    set_window_width (sib,
		      XFASTINT (XWINDOW (sib)->total_cols) + XFASTINT (p->total_cols),
		      1);

  /* If parent now has only one child,
     put the child into the parent's place.  */
  tem = par->hchild;
  if (NILP (tem))
    tem = par->vchild;
  if (NILP (XWINDOW (tem)->next)) {
    replace_window (parent, tem);
    par = XWINDOW (tem);
  }

  /* Since we may be deleting combination windows, we must make sure that
     not only p but all its children have been marked as deleted.  */
  if (! NILP (p->hchild))
    delete_all_subwindows (XWINDOW (p->hchild));
  else if (! NILP (p->vchild))
    delete_all_subwindows (XWINDOW (p->vchild));

  /* Mark this window as deleted.  */
  p->buffer = p->hchild = p->vchild = Qnil;

  if (! NILP (par->parent))
    par = XWINDOW (par->parent);

  /* Check if we have a v/hchild with a v/hchild.  In that case remove
     one of them.  */

  if (! NILP (par->vchild) && ! NILP (XWINDOW (par->vchild)->vchild))
    {
      p = XWINDOW (par->vchild);
      par->vchild = p->vchild;
      tem = p->vchild;
    }
  else if (! NILP (par->hchild) && ! NILP (XWINDOW (par->hchild)->hchild))
    {
      p = XWINDOW (par->hchild);
      par->hchild = p->hchild;
      tem = p->hchild;
    }
  else
    p = 0;

  if (p)
    {
      while (! NILP (tem)) {
        XWINDOW (tem)->parent = p->parent;
        if (NILP (XWINDOW (tem)->next))
          break;
        tem = XWINDOW (tem)->next;
      }
      if (! NILP (tem)) {
        /* The next of the v/hchild we are removing is now the next of the
           last child for the v/hchild:
           Before v/hchild -> v/hchild -> next1 -> next2
                    |
                     -> next3
           After:  v/hchild -> next1 -> next2 -> next3
        */
        XWINDOW (tem)->next = p->next;
        if (! NILP (p->next))
          XWINDOW (p->next)->prev = tem;
      }
      p->next = p->prev = p->vchild = p->hchild = p->buffer = Qnil;
    }


  /* Adjust glyph matrices. */
  adjust_glyphs (f);
  UNBLOCK_INPUT;
}



/***********************************************************************
			     Window List
 ***********************************************************************/

/* Add window W to *USER_DATA.  USER_DATA is actually a Lisp_Object
   pointer.  This is a callback function for foreach_window, used in
   function window_list.  */

static int
add_window_to_list (w, user_data)
     struct window *w;
     void *user_data;
{
  Lisp_Object *list = (Lisp_Object *) user_data;
  Lisp_Object window;
  XSETWINDOW (window, w);
  *list = Fcons (window, *list);
  return 1;
}


/* Return a list of all windows, for use by next_window.  If
   Vwindow_list is a list, return that list.  Otherwise, build a new
   list, cache it in Vwindow_list, and return that.  */

static Lisp_Object
window_list ()
{
  if (!CONSP (Vwindow_list))
    {
      Lisp_Object tail;

      Vwindow_list = Qnil;
      for (tail = Vframe_list; CONSP (tail); tail = XCDR (tail))
	{
	  Lisp_Object args[2];

	  /* We are visiting windows in canonical order, and add
	     new windows at the front of args[1], which means we
	     have to reverse this list at the end.  */
	  args[1] = Qnil;
	  foreach_window (XFRAME (XCAR (tail)), add_window_to_list, &args[1]);
	  args[0] = Vwindow_list;
	  args[1] = Fnreverse (args[1]);
	  Vwindow_list = Fnconc (2, args);
	}
    }

  return Vwindow_list;
}


/* Value is non-zero if WINDOW satisfies the constraints given by
   OWINDOW, MINIBUF and ALL_FRAMES.

   MINIBUF	t means WINDOW may be minibuffer windows.
		`lambda' means WINDOW may not be a minibuffer window.
		a window means a specific minibuffer window

   ALL_FRAMES	t means search all frames,
		nil means search just current frame,
		`visible' means search just visible frames,
		0 means search visible and iconified frames,
		a window means search the frame that window belongs to,
		a frame means consider windows on that frame, only.  */

static int
candidate_window_p (window, owindow, minibuf, all_frames)
     Lisp_Object window, owindow, minibuf, all_frames;
{
  struct window *w = XWINDOW (window);
  struct frame *f = XFRAME (w->frame);
  int candidate_p = 1;

  if (!BUFFERP (w->buffer))
    candidate_p = 0;
  else if (MINI_WINDOW_P (w)
           && (EQ (minibuf, Qlambda)
	       || (WINDOWP (minibuf) && !EQ (minibuf, window))))
    {
      /* If MINIBUF is `lambda' don't consider any mini-windows.
         If it is a window, consider only that one.  */
      candidate_p = 0;
    }
  else if (EQ (all_frames, Qt))
    candidate_p = 1;
  else if (NILP (all_frames))
    {
      xassert (WINDOWP (owindow));
      candidate_p = EQ (w->frame, XWINDOW (owindow)->frame);
    }
  else if (EQ (all_frames, Qvisible))
    {
      FRAME_SAMPLE_VISIBILITY (f);
      candidate_p = FRAME_VISIBLE_P (f);
    }
  else if (INTEGERP (all_frames) && XINT (all_frames) == 0)
    {
      FRAME_SAMPLE_VISIBILITY (f);
      candidate_p = FRAME_VISIBLE_P (f) || FRAME_ICONIFIED_P (f);
    }
  else if (WINDOWP (all_frames))
    candidate_p = (EQ (FRAME_MINIBUF_WINDOW (f), all_frames)
		   || EQ (XWINDOW (all_frames)->frame, w->frame)
		   || EQ (XWINDOW (all_frames)->frame, FRAME_FOCUS_FRAME (f)));
  else if (FRAMEP (all_frames))
    candidate_p = EQ (all_frames, w->frame);

  return candidate_p;
}


/* Decode arguments as allowed by Fnext_window, Fprevious_window, and
   Fwindow_list.  See there for the meaning of WINDOW, MINIBUF, and
   ALL_FRAMES.  */

static void
decode_next_window_args (window, minibuf, all_frames)
     Lisp_Object *window, *minibuf, *all_frames;
{
  if (NILP (*window))
    *window = selected_window;
  else
    CHECK_LIVE_WINDOW (*window);

  /* MINIBUF nil may or may not include minibuffers.  Decide if it
     does.  */
  if (NILP (*minibuf))
    *minibuf = minibuf_level ? minibuf_window : Qlambda;
  else if (!EQ (*minibuf, Qt))
    *minibuf = Qlambda;

  /* Now *MINIBUF can be t => count all minibuffer windows, `lambda'
     => count none of them, or a specific minibuffer window (the
     active one) to count.  */

  /* ALL_FRAMES nil doesn't specify which frames to include.  */
  if (NILP (*all_frames))
    *all_frames = (!EQ (*minibuf, Qlambda)
		   ? FRAME_MINIBUF_WINDOW (XFRAME (XWINDOW (*window)->frame))
		   : Qnil);
  else if (EQ (*all_frames, Qvisible))
    ;
  else if (EQ (*all_frames, make_number (0)))
    ;
  else if (FRAMEP (*all_frames))
    ;
  else if (!EQ (*all_frames, Qt))
    *all_frames = Qnil;

  /* Now *ALL_FRAMES is t meaning search all frames, nil meaning
     search just current frame, `visible' meaning search just visible
     frames, 0 meaning search visible and iconified frames, or a
     window, meaning search the frame that window belongs to, or a
     frame, meaning consider windows on that frame, only.  */
}


/* Return the next or previous window of WINDOW in canonical ordering
   of windows.  NEXT_P non-zero means return the next window.  See the
   documentation string of next-window for the meaning of MINIBUF and
   ALL_FRAMES.  */

static Lisp_Object
next_window (window, minibuf, all_frames, next_p)
     Lisp_Object window, minibuf, all_frames;
     int next_p;
{
  decode_next_window_args (&window, &minibuf, &all_frames);

  /* If ALL_FRAMES is a frame, and WINDOW isn't on that frame, just
     return the first window on the frame.  */
  if (FRAMEP (all_frames)
      && !EQ (all_frames, XWINDOW (window)->frame))
    return Fframe_first_window (all_frames);

  if (next_p)
    {
      Lisp_Object list;

      /* Find WINDOW in the list of all windows.  */
      list = Fmemq (window, window_list ());

      /* Scan forward from WINDOW to the end of the window list.  */
      if (CONSP (list))
	for (list = XCDR (list); CONSP (list); list = XCDR (list))
	  if (candidate_window_p (XCAR (list), window, minibuf, all_frames))
	    break;

      /* Scan from the start of the window list up to WINDOW.  */
      if (!CONSP (list))
	for (list = Vwindow_list;
	     CONSP (list) && !EQ (XCAR (list), window);
	     list = XCDR (list))
	  if (candidate_window_p (XCAR (list), window, minibuf, all_frames))
	    break;

      if (CONSP (list))
	window = XCAR (list);
    }
  else
    {
      Lisp_Object candidate, list;

      /* Scan through the list of windows for candidates.  If there are
	 candidate windows in front of WINDOW, the last one of these
	 is the one we want.  If there are candidates following WINDOW
	 in the list, again the last one of these is the one we want.  */
      candidate = Qnil;
      for (list = window_list (); CONSP (list); list = XCDR (list))
	{
	  if (EQ (XCAR (list), window))
	    {
	      if (WINDOWP (candidate))
		break;
	    }
	  else if (candidate_window_p (XCAR (list), window, minibuf,
				       all_frames))
	    candidate = XCAR (list);
	}

      if (WINDOWP (candidate))
	window = candidate;
    }

  return window;
}


DEFUN ("next-window", Fnext_window, Snext_window, 0, 3, 0,
       doc: /* Return next window after WINDOW in canonical ordering of windows.
If omitted, WINDOW defaults to the selected window.

Optional second arg MINIBUF t means count the minibuffer window even
if not active.  MINIBUF nil or omitted means count the minibuffer iff
it is active.  MINIBUF neither t nor nil means not to count the
minibuffer even if it is active.

Several frames may share a single minibuffer; if the minibuffer
counts, all windows on all frames that share that minibuffer count
too.  Therefore, `next-window' can be used to iterate through the
set of windows even when the minibuffer is on another frame.  If the
minibuffer does not count, only windows from WINDOW's frame count.

Optional third arg ALL-FRAMES t means include windows on all frames.
ALL-FRAMES nil or omitted means cycle within the frames as specified
above.  ALL-FRAMES = `visible' means include windows on all visible frames.
ALL-FRAMES = 0 means include windows on all visible and iconified frames.
If ALL-FRAMES is a frame, restrict search to windows on that frame.
Anything else means restrict to WINDOW's frame.

If you use consistent values for MINIBUF and ALL-FRAMES, you can use
`next-window' to iterate through the entire cycle of acceptable
windows, eventually ending up back at the window you started with.
`previous-window' traverses the same cycle, in the reverse order.  */)
     (window, minibuf, all_frames)
     Lisp_Object window, minibuf, all_frames;
{
  return next_window (window, minibuf, all_frames, 1);
}


DEFUN ("previous-window", Fprevious_window, Sprevious_window, 0, 3, 0,
       doc: /* Return the window preceding WINDOW in canonical ordering of windows.
If omitted, WINDOW defaults to the selected window.

Optional second arg MINIBUF t means count the minibuffer window even
if not active.  MINIBUF nil or omitted means count the minibuffer iff
it is active.  MINIBUF neither t nor nil means not to count the
minibuffer even if it is active.

Several frames may share a single minibuffer; if the minibuffer
counts, all windows on all frames that share that minibuffer count
too.  Therefore, `previous-window' can be used to iterate through
the set of windows even when the minibuffer is on another frame.  If
the minibuffer does not count, only windows from WINDOW's frame count

Optional third arg ALL-FRAMES t means include windows on all frames.
ALL-FRAMES nil or omitted means cycle within the frames as specified
above.  ALL-FRAMES = `visible' means include windows on all visible frames.
ALL-FRAMES = 0 means include windows on all visible and iconified frames.
If ALL-FRAMES is a frame, restrict search to windows on that frame.
Anything else means restrict to WINDOW's frame.

If you use consistent values for MINIBUF and ALL-FRAMES, you can use
`previous-window' to iterate through the entire cycle of acceptable
windows, eventually ending up back at the window you started with.
`next-window' traverses the same cycle, in the reverse order.  */)
     (window, minibuf, all_frames)
     Lisp_Object window, minibuf, all_frames;
{
  return next_window (window, minibuf, all_frames, 0);
}


DEFUN ("other-window", Fother_window, Sother_window, 1, 2, "p",
       doc: /* Select the ARG'th different window on this frame.
All windows on current frame are arranged in a cyclic order.
This command selects the window ARG steps away in that order.
A negative ARG moves in the opposite order.  The optional second
argument ALL-FRAMES has the same meaning as in `next-window', which see.  */)
     (arg, all_frames)
     Lisp_Object arg, all_frames;
{
  Lisp_Object window;
  int i;

  CHECK_NUMBER (arg);
  window = selected_window;

  for (i = XINT (arg); i > 0; --i)
    window = Fnext_window (window, Qnil, all_frames);
  for (; i < 0; ++i)
    window = Fprevious_window (window, Qnil, all_frames);

  Fselect_window (window, Qnil);
  return Qnil;
}


DEFUN ("window-list", Fwindow_list, Swindow_list, 0, 3, 0,
       doc: /* Return a list of windows on FRAME, starting with WINDOW.
FRAME nil or omitted means use the selected frame.
WINDOW nil or omitted means use the selected window.
MINIBUF t means include the minibuffer window, even if it isn't active.
MINIBUF nil or omitted means include the minibuffer window only
if it's active.
MINIBUF neither nil nor t means never include the minibuffer window.  */)
     (frame, minibuf, window)
     Lisp_Object frame, minibuf, window;
{
  if (NILP (window))
    window = FRAMEP (frame) ? XFRAME (frame)->selected_window : selected_window;
  CHECK_WINDOW (window);
  if (NILP (frame))
    frame = selected_frame;

  if (!EQ (frame, XWINDOW (window)->frame))
    error ("Window is on a different frame");

  return window_list_1 (window, minibuf, frame);
}


/* Return a list of windows in canonical ordering.  Arguments are like
   for `next-window'.  */

static Lisp_Object
window_list_1 (window, minibuf, all_frames)
     Lisp_Object window, minibuf, all_frames;
{
  Lisp_Object tail, list, rest;

  decode_next_window_args (&window, &minibuf, &all_frames);
  list = Qnil;

  for (tail = window_list (); CONSP (tail); tail = XCDR (tail))
    if (candidate_window_p (XCAR (tail), window, minibuf, all_frames))
      list = Fcons (XCAR (tail), list);

  /* Rotate the list to start with WINDOW.  */
  list = Fnreverse (list);
  rest = Fmemq (window, list);
  if (!NILP (rest) && !EQ (rest, list))
    {
      for (tail = list; !EQ (XCDR (tail), rest); tail = XCDR (tail))
	;
      XSETCDR (tail, Qnil);
      list = nconc2 (rest, list);
    }
  return list;
}



/* Look at all windows, performing an operation specified by TYPE
   with argument OBJ.
   If FRAMES is Qt, look at all frames;
                Qnil, look at just the selected frame;
		Qvisible, look at visible frames;
	        a frame, just look at windows on that frame.
   If MINI is non-zero, perform the operation on minibuffer windows too.  */

enum window_loop
{
  WINDOW_LOOP_UNUSED,
  GET_BUFFER_WINDOW,		/* Arg is buffer */
  GET_LRU_WINDOW,		/* Arg is t for full-width windows only */
  DELETE_OTHER_WINDOWS,		/* Arg is window not to delete */
  DELETE_BUFFER_WINDOWS,	/* Arg is buffer */
  GET_LARGEST_WINDOW,
  UNSHOW_BUFFER,		/* Arg is buffer */
  REDISPLAY_BUFFER_WINDOWS,	/* Arg is buffer */
  CHECK_ALL_WINDOWS
};

static Lisp_Object
window_loop (type, obj, mini, frames)
     enum window_loop type;
     Lisp_Object obj, frames;
     int mini;
{
  Lisp_Object window, windows, best_window, frame_arg;
  struct frame *f;
  struct gcpro gcpro1;

  /* If we're only looping through windows on a particular frame,
     frame points to that frame.  If we're looping through windows
     on all frames, frame is 0.  */
  if (FRAMEP (frames))
    f = XFRAME (frames);
  else if (NILP (frames))
    f = SELECTED_FRAME ();
  else
    f = NULL;

  if (f)
    frame_arg = Qlambda;
  else if (EQ (frames, make_number (0)))
    frame_arg = frames;
  else if (EQ (frames, Qvisible))
    frame_arg = frames;
  else
    frame_arg = Qt;

  /* frame_arg is Qlambda to stick to one frame,
     Qvisible to consider all visible frames,
     or Qt otherwise.  */

  /* Pick a window to start with.  */
  if (WINDOWP (obj))
    window = obj;
  else if (f)
    window = FRAME_SELECTED_WINDOW (f);
  else
    window = FRAME_SELECTED_WINDOW (SELECTED_FRAME ());

  windows = window_list_1 (window, mini ? Qt : Qnil, frame_arg);
  GCPRO1 (windows);
  best_window = Qnil;

  for (; CONSP (windows); windows = XCDR (windows))
    {
      struct window *w;

      window = XCAR (windows);
      w = XWINDOW (window);

      /* Note that we do not pay attention here to whether the frame
	 is visible, since Fwindow_list skips non-visible frames if
	 that is desired, under the control of frame_arg.  */
      if (!MINI_WINDOW_P (w)
	  /* For UNSHOW_BUFFER, we must always consider all windows.  */
	  || type == UNSHOW_BUFFER
	  || (mini && minibuf_level > 0))
	switch (type)
	  {
	  case GET_BUFFER_WINDOW:
	    if (EQ (w->buffer, obj)
		/* Don't find any minibuffer window
		   except the one that is currently in use.  */
		&& (MINI_WINDOW_P (w)
		    ? EQ (window, minibuf_window)
		    : 1))
	      {
		if (NILP (best_window))
		  best_window = window;
		else if (EQ (window, selected_window))
		  /* For compatibility with 20.x, prefer to return
		     selected-window.  */
		  best_window = window;
	      }
	    break;

	  case GET_LRU_WINDOW:
	    /* `obj' is an integer encoding a bitvector.
	       `obj & 1' means consider only full-width windows.
	       `obj & 2' means consider also dedicated windows. */
	    if (((XINT (obj) & 1) && !WINDOW_FULL_WIDTH_P (w))
		|| (!(XINT (obj) & 2) && !NILP (w->dedicated))
		/* Minibuffer windows are always ignored.  */
		|| MINI_WINDOW_P (w))
	      break;
	    if (NILP (best_window)
		|| (XFASTINT (XWINDOW (best_window)->use_time)
		    > XFASTINT (w->use_time)))
	      best_window = window;
	    break;

	  case DELETE_OTHER_WINDOWS:
	    if (!EQ (window, obj))
	      Fdelete_window (window);
	    break;

	  case DELETE_BUFFER_WINDOWS:
	    if (EQ (w->buffer, obj))
	      {
		struct frame *f = XFRAME (WINDOW_FRAME (w));

		/* If this window is dedicated, and in a frame of its own,
		   kill the frame.  */
		if (EQ (window, FRAME_ROOT_WINDOW (f))
		    && !NILP (w->dedicated)
		    && other_visible_frames (f))
		  {
		    /* Skip the other windows on this frame.
		       There might be one, the minibuffer!  */
		    while (CONSP (XCDR (windows))
			   && EQ (XWINDOW (XCAR (windows))->frame,
				  XWINDOW (XCAR (XCDR (windows)))->frame))
		      windows = XCDR (windows);

		    /* Now we can safely delete the frame.  */
		    Fdelete_frame (w->frame, Qnil);
		  }
		else if (NILP (w->parent))
		  {
		    /* If we're deleting the buffer displayed in the
		       only window on the frame, find a new buffer to
		       display there.  */
		    Lisp_Object buffer;
		    buffer = Fother_buffer (obj, Qnil, w->frame);
		    Fset_window_buffer (window, buffer, Qnil);
		    if (EQ (window, selected_window))
		      Fset_buffer (w->buffer);
		  }
		else
		  Fdelete_window (window);
	      }
	    break;

	  case GET_LARGEST_WINDOW:
	    { /* nil `obj' means to ignore dedicated windows.  */
	      /* Ignore dedicated windows and minibuffers.  */
	      if (MINI_WINDOW_P (w) || (NILP (obj) && !NILP (w->dedicated)))
		break;

	      if (NILP (best_window))
		best_window = window;
	      else
		{
		  struct window *b = XWINDOW (best_window);
		  if (XFASTINT (w->total_lines) * XFASTINT (w->total_cols)
		      > XFASTINT (b->total_lines) * XFASTINT (b->total_cols))
		    best_window = window;
		}
	    }
	    break;

	  case UNSHOW_BUFFER:
	    if (EQ (w->buffer, obj))
	      {
		Lisp_Object buffer;
		struct frame *f = XFRAME (w->frame);

		/* Find another buffer to show in this window.  */
		buffer = Fother_buffer (obj, Qnil, w->frame);

		/* If this window is dedicated, and in a frame of its own,
		   kill the frame.  */
		if (EQ (window, FRAME_ROOT_WINDOW (f))
		    && !NILP (w->dedicated)
		    && other_visible_frames (f))
		  {
		    /* Skip the other windows on this frame.
		       There might be one, the minibuffer!  */
		    while (CONSP (XCDR (windows))
			   && EQ (XWINDOW (XCAR (windows))->frame,
				  XWINDOW (XCAR (XCDR (windows)))->frame))
		      windows = XCDR (windows);

		    /* Now we can safely delete the frame.  */
		    Fdelete_frame (w->frame, Qnil);
		  }
		else if (!NILP (w->dedicated) && !NILP (w->parent))
		  {
		    Lisp_Object window;
		    XSETWINDOW (window, w);
		    /* If this window is dedicated and not the only window
		       in its frame, then kill it.  */
		    Fdelete_window (window);
		  }
		else
		  {
		    /* Otherwise show a different buffer in the window.  */
		    w->dedicated = Qnil;
		    Fset_window_buffer (window, buffer, Qnil);
		    if (EQ (window, selected_window))
		      Fset_buffer (w->buffer);
		  }
	      }
	    break;

	  case REDISPLAY_BUFFER_WINDOWS:
	    if (EQ (w->buffer, obj))
	      {
		mark_window_display_accurate (window, 0);
		w->update_mode_line = Qt;
		XBUFFER (obj)->prevent_redisplay_optimizations_p = 1;
		++update_mode_lines;
		best_window = window;
	      }
	    break;

	    /* Check for a window that has a killed buffer.  */
	  case CHECK_ALL_WINDOWS:
	    if (! NILP (w->buffer)
		&& NILP (XBUFFER (w->buffer)->name))
	      abort ();
	    break;

	  case WINDOW_LOOP_UNUSED:
	    break;
	  }
    }

  UNGCPRO;
  return best_window;
}

/* Used for debugging.  Abort if any window has a dead buffer.  */

void
check_all_windows ()
{
  window_loop (CHECK_ALL_WINDOWS, Qnil, 1, Qt);
}

DEFUN ("get-lru-window", Fget_lru_window, Sget_lru_window, 0, 2, 0,
       doc: /* Return the window least recently selected or used for display.
\(LRU means Least Recently Used.)

Return a full-width window if possible.
A minibuffer window is never a candidate.
A dedicated window is never a candidate, unless DEDICATED is non-nil,
  so if all windows are dedicated, the value is nil.
If optional argument FRAME is `visible', search all visible frames.
If FRAME is 0, search all visible and iconified frames.
If FRAME is t, search all frames.
If FRAME is nil, search only the selected frame.
If FRAME is a frame, search only that frame.  */)
  (frame, dedicated)
     Lisp_Object frame, dedicated;
{
  register Lisp_Object w;
  /* First try for a window that is full-width */
  w = window_loop (GET_LRU_WINDOW,
		   NILP (dedicated) ? make_number (1) : make_number (3),
		   0, frame);
  if (!NILP (w) && !EQ (w, selected_window))
    return w;
  /* If none of them, try the rest */
  return window_loop (GET_LRU_WINDOW,
		      NILP (dedicated) ? make_number (0) : make_number (2),
		      0, frame);
}

DEFUN ("get-largest-window", Fget_largest_window, Sget_largest_window, 0, 2, 0,
       doc: /* Return the largest window in area.
A minibuffer window is never a candidate.
A dedicated window is never a candidate unless DEDICATED is non-nil,
  so if all windows are dedicated, the value is nil.
If optional argument FRAME is `visible', search all visible frames.
If FRAME is 0, search all visible and iconified frames.
If FRAME is t, search all frames.
If FRAME is nil, search only the selected frame.
If FRAME is a frame, search only that frame.  */)
    (frame, dedicated)
     Lisp_Object frame, dedicated;
{
  return window_loop (GET_LARGEST_WINDOW, dedicated, 0,
		      frame);
}

DEFUN ("get-buffer-window", Fget_buffer_window, Sget_buffer_window, 1, 2, 0,
       doc: /* Return a window currently displaying BUFFER, or nil if none.
BUFFER can be a buffer or a buffer name.
If optional argument FRAME is `visible', search all visible frames.
If optional argument FRAME is 0, search all visible and iconified frames.
If FRAME is t, search all frames.
If FRAME is nil, search only the selected frame.
If FRAME is a frame, search only that frame.  */)
     (buffer, frame)
     Lisp_Object buffer, frame;
{
  buffer = Fget_buffer (buffer);
  if (BUFFERP (buffer))
    return window_loop (GET_BUFFER_WINDOW, buffer, 1, frame);
  else
    return Qnil;
}

DEFUN ("delete-other-windows", Fdelete_other_windows, Sdelete_other_windows,
       0, 1, "",
       doc: /* Make WINDOW (or the selected window) fill its frame.
Only the frame WINDOW is on is affected.
This function tries to reduce display jumps
by keeping the text previously visible in WINDOW
in the same place on the frame.  Doing this depends on
the value of (window-start WINDOW), so if calling this function
in a program gives strange scrolling, make sure the window-start
value is reasonable when this function is called.  */)
     (window)
     Lisp_Object window;
{
  struct window *w;
  int startpos;
  int top, new_top;

  if (NILP (window))
    window = selected_window;
  else
    CHECK_LIVE_WINDOW (window);
  w = XWINDOW (window);

  startpos = marker_position (w->start);
  top = WINDOW_TOP_EDGE_LINE (w) - FRAME_TOP_MARGIN (XFRAME (WINDOW_FRAME (w)));

  if (MINI_WINDOW_P (w) && top > 0)
    error ("Can't expand minibuffer to full frame");

  window_loop (DELETE_OTHER_WINDOWS, window, 0, WINDOW_FRAME (w));

  /* Try to minimize scrolling, by setting the window start to the point
     will cause the text at the old window start to be at the same place
     on the frame.  But don't try to do this if the window start is
     outside the visible portion (as might happen when the display is
     not current, due to typeahead).  */
  new_top = WINDOW_TOP_EDGE_LINE (w) - FRAME_TOP_MARGIN (XFRAME (WINDOW_FRAME (w)));
  if (new_top != top
      && startpos >= BUF_BEGV (XBUFFER (w->buffer))
      && startpos <= BUF_ZV (XBUFFER (w->buffer)))
    {
      struct position pos;
      struct buffer *obuf = current_buffer;

      Fset_buffer (w->buffer);
      /* This computation used to temporarily move point, but that can
	 have unwanted side effects due to text properties.  */
      pos = *vmotion (startpos, -top, w);

      set_marker_both (w->start, w->buffer, pos.bufpos, pos.bytepos);
      w->window_end_valid = Qnil;
      w->start_at_line_beg = ((pos.bytepos == BEGV_BYTE
			       || FETCH_BYTE (pos.bytepos - 1) == '\n') ? Qt
			      : Qnil);
      /* We need to do this, so that the window-scroll-functions
	 get called.  */
      w->optional_new_start = Qt;

      set_buffer_internal (obuf);
    }

  return Qnil;
}

DEFUN ("delete-windows-on", Fdelete_windows_on, Sdelete_windows_on,
       1, 2, "bDelete windows on (buffer): ",
       doc: /* Delete all windows showing BUFFER.
BUFFER must be a buffer or the name of an existing buffer.
Optional second argument FRAME controls which frames are affected.
If optional argument FRAME is `visible', search all visible frames.
If FRAME is 0, search all visible and iconified frames.
If FRAME is nil, search all frames.
If FRAME is t, search only the selected frame.
If FRAME is a frame, search only that frame.  */)
     (buffer, frame)
     Lisp_Object buffer, frame;
{
  /* FRAME uses t and nil to mean the opposite of what window_loop
     expects.  */
  if (NILP (frame))
    frame = Qt;
  else if (EQ (frame, Qt))
    frame = Qnil;

  if (!NILP (buffer))
    {
      buffer = Fget_buffer (buffer);
      CHECK_BUFFER (buffer);
      window_loop (DELETE_BUFFER_WINDOWS, buffer, 0, frame);
    }

  return Qnil;
}

DEFUN ("replace-buffer-in-windows", Freplace_buffer_in_windows,
       Sreplace_buffer_in_windows,
       1, 1, "bReplace buffer in windows: ",
       doc: /* Replace BUFFER with some other buffer in all windows showing it.
BUFFER may be a buffer or the name of an existing buffer.  */)
     (buffer)
     Lisp_Object buffer;
{
  if (!NILP (buffer))
    {
      buffer = Fget_buffer (buffer);
      CHECK_BUFFER (buffer);
      window_loop (UNSHOW_BUFFER, buffer, 0, Qt);
    }
  return Qnil;
}

/* Replace BUFFER with some other buffer in all windows
   of all frames, even those on other keyboards.  */

void
replace_buffer_in_all_windows (buffer)
     Lisp_Object buffer;
{
#ifdef MULTI_KBOARD
  Lisp_Object tail, frame;

  /* A single call to window_loop won't do the job
     because it only considers frames on the current keyboard.
     So loop manually over frames, and handle each one.  */
  FOR_EACH_FRAME (tail, frame)
    window_loop (UNSHOW_BUFFER, buffer, 1, frame);
#else
  window_loop (UNSHOW_BUFFER, buffer, 1, Qt);
#endif
}

/* Set the height of WINDOW and all its inferiors.  */

/* The smallest acceptable dimensions for a window.  Anything smaller
   might crash Emacs.  */

#define MIN_SAFE_WINDOW_WIDTH  (2)
#define MIN_SAFE_WINDOW_HEIGHT (1)

/* Make sure that window_min_height and window_min_width are
   not too small; if they are, set them to safe minima.  */

static void
check_min_window_sizes ()
{
  /* Smaller values might permit a crash.  */
  if (window_min_width < MIN_SAFE_WINDOW_WIDTH)
    window_min_width = MIN_SAFE_WINDOW_WIDTH;
  if (window_min_height < MIN_SAFE_WINDOW_HEIGHT)
    window_min_height = MIN_SAFE_WINDOW_HEIGHT;
}

/* If *ROWS or *COLS are too small a size for FRAME, set them to the
   minimum allowable size.  */

void
check_frame_size (frame, rows, cols)
     FRAME_PTR frame;
     int *rows, *cols;
{
  /* For height, we have to see:
     how many windows the frame has at minimum (one or two),
     and whether it has a menu bar or other special stuff at the top.  */
  int min_height
    = ((FRAME_MINIBUF_ONLY_P (frame) || ! FRAME_HAS_MINIBUF_P (frame))
       ? MIN_SAFE_WINDOW_HEIGHT
       : 2 * MIN_SAFE_WINDOW_HEIGHT);

  if (FRAME_TOP_MARGIN (frame) > 0)
    min_height += FRAME_TOP_MARGIN (frame);

  if (*rows < min_height)
    *rows = min_height;
  if (*cols  < MIN_SAFE_WINDOW_WIDTH)
    *cols = MIN_SAFE_WINDOW_WIDTH;
}


/* Value is non-zero if window W is fixed-size.  WIDTH_P non-zero means
   check if W's width can be changed, otherwise check W's height.
   CHECK_SIBLINGS_P non-zero means check resizablity of WINDOW's
   siblings, too.  If none of the siblings is resizable, WINDOW isn't
   either.  */

static int
window_fixed_size_p (w, width_p, check_siblings_p)
     struct window *w;
     int width_p, check_siblings_p;
{
  int fixed_p;
  struct window *c;

  if (!NILP (w->hchild))
    {
      c = XWINDOW (w->hchild);

      if (width_p)
	{
	  /* A horiz. combination is fixed-width if all of if its
	     children are.  */
	  while (c && window_fixed_size_p (c, width_p, 0))
	    c = WINDOWP (c->next) ? XWINDOW (c->next) : NULL;
	  fixed_p = c == NULL;
	}
      else
	{
	  /* A horiz. combination is fixed-height if one of if its
	     children is.  */
	  while (c && !window_fixed_size_p (c, width_p, 0))
	    c = WINDOWP (c->next) ? XWINDOW (c->next) : NULL;
	  fixed_p = c != NULL;
	}
    }
  else if (!NILP (w->vchild))
    {
      c = XWINDOW (w->vchild);

      if (width_p)
	{
	  /* A vert. combination is fixed-width if one of if its
	     children is.  */
	  while (c && !window_fixed_size_p (c, width_p, 0))
	    c = WINDOWP (c->next) ? XWINDOW (c->next) : NULL;
	  fixed_p = c != NULL;
	}
      else
	{
	  /* A vert. combination is fixed-height if all of if its
	     children are.  */
	  while (c && window_fixed_size_p (c, width_p, 0))
	    c = WINDOWP (c->next) ? XWINDOW (c->next) : NULL;
	  fixed_p = c == NULL;
	}
    }
  else if (BUFFERP (w->buffer))
    {
      struct buffer *old = current_buffer;
      Lisp_Object val;

      current_buffer = XBUFFER (w->buffer);
      val = find_symbol_value (Qwindow_size_fixed);
      current_buffer = old;

      fixed_p = 0;
      if (!EQ (val, Qunbound))
	{
	  fixed_p = !NILP (val);

	  if (fixed_p
	      && ((EQ (val, Qheight) && width_p)
		  || (EQ (val, Qwidth) && !width_p)))
	    fixed_p = 0;
	}

      /* Can't tell if this one is resizable without looking at
	 siblings.  If all siblings are fixed-size this one is too.  */
      if (!fixed_p && check_siblings_p && WINDOWP (w->parent))
	{
	  Lisp_Object child;

	  for (child = w->prev; !NILP (child); child = XWINDOW (child)->prev)
	    if (!window_fixed_size_p (XWINDOW (child), width_p, 0))
	      break;

	  if (NILP (child))
	    for (child = w->next; !NILP (child); child = XWINDOW (child)->next)
	      if (!window_fixed_size_p (XWINDOW (child), width_p, 0))
		break;

	  if (NILP (child))
	    fixed_p = 1;
	}
    }
  else
    fixed_p = 1;

  return fixed_p;
}


/* Return the minimum size of window W, not taking fixed-width windows
   into account.  WIDTH_P non-zero means return the minimum width,
   otherwise return the minimum height.  If W is a combination window,
   compute the minimum size from the minimum sizes of W's children.  */

static int
window_min_size_1 (w, width_p)
     struct window *w;
     int width_p;
{
  struct window *c;
  int size;

  if (!NILP (w->hchild))
    {
      c = XWINDOW (w->hchild);
      size = 0;

      if (width_p)
	{
	  /* The min width of a horizontal combination is
	     the sum of the min widths of its children.  */
	  while (c)
	    {
	      size += window_min_size_1 (c, width_p);
	      c = WINDOWP (c->next) ? XWINDOW (c->next) : NULL;
	    }
	}
      else
	{
	  /* The min height a horizontal combination equals
	     the maximum of all min height of its children.  */
	  while (c)
	    {
	      int min_size = window_min_size_1 (c, width_p);
	      size = max (min_size, size);
	      c = WINDOWP (c->next) ? XWINDOW (c->next) : NULL;
	    }
	}
    }
  else if (!NILP (w->vchild))
    {
      c = XWINDOW (w->vchild);
      size = 0;

      if (width_p)
	{
	  /* The min width of a vertical combination is
	     the maximum of the min widths of its children.  */
	  while (c)
	    {
	      int min_size = window_min_size_1 (c, width_p);
	      size = max (min_size, size);
	      c = WINDOWP (c->next) ? XWINDOW (c->next) : NULL;
	    }
	}
      else
	{
	  /* The min height of a vertical combination equals
	     the sum of the min height of its children.  */
	  while (c)
	    {
	      size += window_min_size_1 (c, width_p);
	      c = WINDOWP (c->next) ? XWINDOW (c->next) : NULL;
	    }
	}
    }
  else
    {
      if (width_p)
	size = max (window_min_width,
		    (MIN_SAFE_WINDOW_WIDTH
		     + WINDOW_FRINGE_COLS (w)
		     + WINDOW_SCROLL_BAR_COLS (w)));
      else
	{
	  if (MINI_WINDOW_P (w)
	      || (!WINDOW_WANTS_MODELINE_P (w)
		  && !WINDOW_WANTS_HEADER_LINE_P (w)))
	    size = 1;
	  else
	    size = window_min_height;
	}
    }

  return size;
}


/* Return the minimum size of window W, taking fixed-size windows into
   account.  WIDTH_P non-zero means return the minimum width,
   otherwise return the minimum height.  IGNORE_FIXED_P non-zero means
   ignore if W is fixed-size.  Set *FIXED to 1 if W is fixed-size
   unless FIXED is null.  */

static int
window_min_size (w, width_p, ignore_fixed_p, fixed)
     struct window *w;
     int width_p, ignore_fixed_p, *fixed;
{
  int size, fixed_p;

  if (ignore_fixed_p)
    fixed_p = 0;
  else
    fixed_p = window_fixed_size_p (w, width_p, 1);

  if (fixed)
    *fixed = fixed_p;

  if (fixed_p)
    size = width_p ? XFASTINT (w->total_cols) : XFASTINT (w->total_lines);
  else
    size = window_min_size_1 (w, width_p);

  return size;
}


/* Adjust the margins of window W if text area is too small.
   Return 1 if window width is ok after adjustment; 0 if window
   is still too narrow.  */

static int
adjust_window_margins (w)
     struct window *w;
{
  int box_cols = (WINDOW_TOTAL_COLS (w)
		  - WINDOW_FRINGE_COLS (w)
		  - WINDOW_SCROLL_BAR_COLS (w));
  int margin_cols = (WINDOW_LEFT_MARGIN_COLS (w)
		     + WINDOW_RIGHT_MARGIN_COLS (w));

  if (box_cols - margin_cols >= MIN_SAFE_WINDOW_WIDTH)
    return 1;

  if (margin_cols < 0 || box_cols < MIN_SAFE_WINDOW_WIDTH)
    return 0;

  /* Window's text area is too narrow, but reducing the window
     margins will fix that.  */
  margin_cols = box_cols - MIN_SAFE_WINDOW_WIDTH;
  if (WINDOW_RIGHT_MARGIN_COLS (w) > 0)
    {
      if (WINDOW_LEFT_MARGIN_COLS (w) > 0)
	w->left_margin_cols = w->right_margin_cols
	  = make_number (margin_cols/2);
      else
	w->right_margin_cols = make_number (margin_cols);
    }
  else
    w->left_margin_cols = make_number (margin_cols);
  return 1;
}

/* Calculate new sizes for windows in the list FORWARD when the window size
   goes from TOTAL to SIZE.  TOTAL must be greater than SIZE.
   The number of windows in FORWARD is NCHILDREN, and the number that
   can shrink is SHRINKABLE.
   The minimum size a window can have is MIN_SIZE.
   If we are shrinking fixed windows, RESIZE_FIXED_P is non-zero.
   If we are shrinking columns, WIDTH_P is non-zero, otherwise we are
   shrinking rows.

   This function returns an allocated array of new sizes that the caller
   must free.  The size -1 means the window is fixed and RESIZE_FIXED_P
   is zero.  Array index 0 refers to the first window in FORWARD, 1 to
   the second, and so on.

   This function tries to keep windows at least at the minimum size
   and resize other windows before it resizes any window to zero (i.e.
   delete that window).

   Windows are resized proportional to their size, so bigger windows
   shrink more than smaller windows.  */
static int *
shrink_windows (total, size, nchildren, shrinkable,
                min_size, resize_fixed_p, forward, width_p)
     int total, size, nchildren, shrinkable, min_size;
     int resize_fixed_p, width_p;
     Lisp_Object forward;
{
  int available_resize = 0;
  int *new_sizes;
  struct window *c;
  Lisp_Object child;
  int smallest = total;
  int total_removed = 0;
  int total_shrink = total - size;
  int i;

  new_sizes = xmalloc (sizeof (*new_sizes) * nchildren);

  for (i = 0, child = forward; !NILP (child); child = c->next, ++i)
    {
      int child_size;

      c = XWINDOW (child);
      child_size = width_p ? XINT (c->total_cols) : XINT (c->total_lines);

      if (! resize_fixed_p && window_fixed_size_p (c, width_p, 0))
        new_sizes[i] = -1;
      else
        {
          new_sizes[i] = child_size;
          if (child_size > min_size)
            available_resize += child_size - min_size;
        }
    }
  /* We might need to shrink some windows to zero.  Find the smallest
     windows and set them to 0 until we can fulfil the new size.  */

  while (shrinkable > 1 && size + available_resize < total)
    {
      for (i = 0; i < nchildren; ++i)
        if (new_sizes[i] > 0 && smallest > new_sizes[i])
          smallest = new_sizes[i];

      for (i = 0; i < nchildren; ++i)
        if (new_sizes[i] == smallest)
          {
            /* Resize this window down to zero.  */
            new_sizes[i] = 0;
            if (smallest > min_size)
              available_resize -= smallest - min_size;
            available_resize += smallest;
            --shrinkable;
            total_removed += smallest;

            /* We don't know what the smallest is now.  */
            smallest = total;

            /* Out of for, just remove one window at the time and
               check again if we have enough space.  */
            break;
          }
    }

  /* Now, calculate the new sizes.  Try to shrink each window
     proportional to its size.  */
  for (i = 0; i < nchildren; ++i)
    {
      if (new_sizes[i] > min_size)
        {
          int to_shrink = total_shrink*new_sizes[i]/total;
          if (new_sizes[i] - to_shrink < min_size)
            to_shrink = new_sizes[i] - min_size;
          new_sizes[i] -= to_shrink;
          total_removed += to_shrink;
        }
    }

  /* Any reminder due to rounding, we just subtract from windows
     that are left and still can be shrunk.  */
  while (total_shrink > total_removed)
    {
      int nonzero_sizes = 0;
      int nonzero_idx = -1;

      for (i = 0; i < nchildren; ++i)
        if (new_sizes[i] > 0)
          {
            ++nonzero_sizes;
            nonzero_idx = i;
          }

      for (i = 0; i < nchildren; ++i)
        if (new_sizes[i] > min_size)
          {
            --new_sizes[i];
            ++total_removed;

            /* Out of for, just shrink one window at the time and
               check again if we have enough space.  */
            break;
          }


      /* Special case, only one window left.  */
      if (nonzero_sizes == 1)
        break;
    }

  /* Any surplus due to rounding, we add to windows that are left.  */
  while (total_shrink < total_removed)
    {
      for (i = 0; i < nchildren; ++i)
        {
          if (new_sizes[i] != 0 && total_shrink < total_removed)
            {
              ++new_sizes[i];
              --total_removed;
              break;
            }
        }
    }

  return new_sizes;
}

/* Set WINDOW's height or width to SIZE.  WIDTH_P non-zero means set
   WINDOW's width.  Resize WINDOW's children, if any, so that they
   keep their proportionate size relative to WINDOW.

   If FIRST_ONLY is 1, change only the first of WINDOW's children when
   they are in series.  If LAST_ONLY is 1, change only the last of
   WINDOW's children when they are in series.

   Propagate WINDOW's top or left edge position to children.  Delete
   windows that become too small unless NODELETE_P is non-zero.

   If NODELETE_P is 2, that means we do delete windows that are
   too small, even if they were too small before!  */

static void
size_window (window, size, width_p, nodelete_p, first_only, last_only)
     Lisp_Object window;
     int size, width_p, nodelete_p;
     int first_only, last_only;
{
  struct window *w = XWINDOW (window);
  struct window *c;
  Lisp_Object child, *forward, *sideward;
  int old_size, min_size, safe_min_size;

  /* We test nodelete_p != 2 and nodelete_p != 1 below, so it
     seems like it's too soon to do this here.  ++KFS.  */
  if (nodelete_p == 2)
    nodelete_p = 0;

  check_min_window_sizes ();
  size = max (0, size);

  /* If the window has been "too small" at one point,
     don't delete it for being "too small" in the future.
     Preserve it as long as that is at all possible.  */
  if (width_p)
    {
      old_size = WINDOW_TOTAL_COLS (w);
      min_size = window_min_width;
      /* Ensure that there is room for the scroll bar and fringes!
         We may reduce display margins though.  */
      safe_min_size = (MIN_SAFE_WINDOW_WIDTH
		       + WINDOW_FRINGE_COLS (w)
		       + WINDOW_SCROLL_BAR_COLS (w));
    }
  else
    {
      old_size = XINT (w->total_lines);
      min_size = window_min_height;
      safe_min_size = MIN_SAFE_WINDOW_HEIGHT;
    }

  if (old_size < min_size && nodelete_p != 2)
    w->too_small_ok = Qt;

  /* Maybe delete WINDOW if it's too small.  */
  if (nodelete_p != 1 && !NILP (w->parent))
    {
      if (!MINI_WINDOW_P (w) && !NILP (w->too_small_ok))
	min_size = width_p ? MIN_SAFE_WINDOW_WIDTH : MIN_SAFE_WINDOW_HEIGHT;
      if (min_size < safe_min_size)
	min_size = safe_min_size;
      if (size < min_size)
	{
	  delete_window (window);
	  return;
	}
    }

  /* Set redisplay hints.  */
  w->last_modified = make_number (0);
  w->last_overlay_modified = make_number (0);
  windows_or_buffers_changed++;
  FRAME_WINDOW_SIZES_CHANGED (XFRAME (w->frame)) = 1;

  if (width_p)
    {
      sideward = &w->vchild;
      forward = &w->hchild;
      w->total_cols = make_number (size);
      adjust_window_margins (w);
    }
  else
    {
      sideward = &w->hchild;
      forward = &w->vchild;
      w->total_lines = make_number (size);
      w->orig_total_lines = Qnil;
    }

  if (!NILP (*sideward))
    {
      /* We have a chain of parallel siblings whose size should all change.  */
      for (child = *sideward; !NILP (child); child = c->next)
	{
	  c = XWINDOW (child);
	  if (width_p)
	    c->left_col = w->left_col;
	  else
	    c->top_line = w->top_line;
	  size_window (child, size, width_p, nodelete_p,
		       first_only, last_only);
	}
    }
  else if (!NILP (*forward) && last_only)
    {
      /* Change the last in a series of siblings.  */
      Lisp_Object last_child;
      int child_size;

      for (child = *forward; !NILP (child); child = c->next)
	{
	  c = XWINDOW (child);
	  last_child = child;
	}

      child_size = XINT (width_p ? c->total_cols : c->total_lines);
      size_window (last_child,
		   size - old_size + child_size,
		   width_p, nodelete_p, first_only, last_only);
    }
  else if (!NILP (*forward) && first_only)
    {
      /* Change the first in a series of siblings.  */
      int child_size;

      child = *forward;
      c = XWINDOW (child);

      if (width_p)
	c->left_col = w->left_col;
      else
	c->top_line = w->top_line;

      child_size = XINT (width_p ? c->total_cols : c->total_lines);
      size_window (child,
		   size - old_size + child_size,
		   width_p, nodelete_p, first_only, last_only);
    }
  else if (!NILP (*forward))
    {
      int fixed_size, each, extra, n;
      int resize_fixed_p, nfixed;
      int last_pos, first_pos, nchildren, total;
      int *new_sizes = NULL;

      /* Determine the fixed-size portion of this window, and the
	 number of child windows.  */
      fixed_size = nchildren = nfixed = total = 0;
      for (child = *forward; !NILP (child); child = c->next, ++nchildren)
	{
	  int child_size;

	  c = XWINDOW (child);
	  child_size = width_p ? XINT (c->total_cols) : XINT (c->total_lines);
	  total += child_size;

	  if (window_fixed_size_p (c, width_p, 0))
	    {
	      fixed_size += child_size;
	      ++nfixed;
	    }
	}

      /* If the new size is smaller than fixed_size, or if there
	 aren't any resizable windows, allow resizing fixed-size
	 windows.  */
      resize_fixed_p = nfixed == nchildren || size < fixed_size;

      /* Compute how many lines/columns to add/remove to each child.  The
	 value of extra takes care of rounding errors.  */
      n = resize_fixed_p ? nchildren : nchildren - nfixed;
      if (size < total && n > 1)
        new_sizes = shrink_windows (total, size, nchildren, n, min_size,
                                    resize_fixed_p, *forward, width_p);
      else
        {
          each = (size - total) / n;
          extra = (size - total) - n * each;
        }

      /* Compute new children heights and edge positions.  */
      first_pos = width_p ? XINT (w->left_col) : XINT (w->top_line);
      last_pos = first_pos;
      for (n = 0, child = *forward; !NILP (child); child = c->next, ++n)
	{
	  int new_size, old_size;

	  c = XWINDOW (child);
	  old_size = width_p ? XFASTINT (c->total_cols) : XFASTINT (c->total_lines);
	  new_size = old_size;

	  /* The top or left edge position of this child equals the
	     bottom or right edge of its predecessor.  */
	  if (width_p)
	    c->left_col = make_number (last_pos);
	  else
	    c->top_line = make_number (last_pos);

	  /* If this child can be resized, do it.  */
	  if (resize_fixed_p || !window_fixed_size_p (c, width_p, 0))
	    {
	      new_size = new_sizes ? new_sizes[n] : old_size + each + extra;
	      extra = 0;
	    }

	  /* Set new height.  Note that size_window also propagates
	     edge positions to children, so it's not a no-op if we
	     didn't change the child's size.  */
	  size_window (child, new_size, width_p, 1, first_only, last_only);

	  /* Remember the bottom/right edge position of this child; it
	     will be used to set the top/left edge of the next child.  */
          last_pos += new_size;
	}

      if (new_sizes) xfree (new_sizes);

      /* We should have covered the parent exactly with child windows.  */
      xassert (size == last_pos - first_pos);

      /* Now delete any children that became too small.  */
      if (!nodelete_p)
	for (child = *forward; !NILP (child); child = c->next)
	  {
	    int child_size;
	    c = XWINDOW (child);
	    child_size = width_p ? XINT (c->total_cols) : XINT (c->total_lines);
	    size_window (child, child_size, width_p, 2, first_only, last_only);
	  }
    }
}

/* Set WINDOW's height to HEIGHT, and recursively change the height of
   WINDOW's children.  NODELETE non-zero means don't delete windows
   that become too small in the process.  (The caller should check
   later and do so if appropriate.)  */

void
set_window_height (window, height, nodelete)
     Lisp_Object window;
     int height;
     int nodelete;
{
  size_window (window, height, 0, nodelete, 0, 0);
}


/* Set WINDOW's width to WIDTH, and recursively change the width of
   WINDOW's children.  NODELETE non-zero means don't delete windows
   that become too small in the process.  (The caller should check
   later and do so if appropriate.)  */

void
set_window_width (window, width, nodelete)
     Lisp_Object window;
     int width;
     int nodelete;
{
  size_window (window, width, 1, nodelete, 0, 0);
}

/* Change window heights in windows rooted in WINDOW by N lines.  */

void
change_window_heights (window, n)
     Lisp_Object window;
     int n;
{
  struct window *w = XWINDOW (window);

  XSETFASTINT (w->top_line, XFASTINT (w->top_line) + n);
  XSETFASTINT (w->total_lines, XFASTINT (w->total_lines) - n);

  if (INTEGERP (w->orig_top_line))
    XSETFASTINT (w->orig_top_line, XFASTINT (w->orig_top_line) + n);
  if (INTEGERP (w->orig_total_lines))
    XSETFASTINT (w->orig_total_lines, XFASTINT (w->orig_total_lines) - n);

  /* Handle just the top child in a vertical split.  */
  if (!NILP (w->vchild))
    change_window_heights (w->vchild, n);

  /* Adjust all children in a horizontal split.  */
  for (window = w->hchild; !NILP (window); window = w->next)
    {
      w = XWINDOW (window);
      change_window_heights (window, n);
    }
}


int window_select_count;

Lisp_Object
Fset_window_buffer_unwind (obuf)
     Lisp_Object obuf;
{
  Fset_buffer (obuf);
  return Qnil;
}

EXFUN (Fset_window_fringes, 4);
EXFUN (Fset_window_scroll_bars, 4);

/* Make WINDOW display BUFFER as its contents.  RUN_HOOKS_P non-zero
   means it's allowed to run hooks.  See make_frame for a case where
   it's not allowed.  KEEP_MARGINS_P non-zero means that the current
   margins, fringes, and scroll-bar settings of the window are not
   reset from the buffer's local settings.  */

void
set_window_buffer (window, buffer, run_hooks_p, keep_margins_p)
     Lisp_Object window, buffer;
     int run_hooks_p, keep_margins_p;
{
  struct window *w = XWINDOW (window);
  struct buffer *b = XBUFFER (buffer);
  int count = SPECPDL_INDEX ();

  w->buffer = buffer;

  if (EQ (window, selected_window))
    b->last_selected_window = window;

  /* Let redisplay errors through.  */
  b->display_error_modiff = 0;

  /* Update time stamps of buffer display.  */
  if (INTEGERP (b->display_count))
    XSETINT (b->display_count, XINT (b->display_count) + 1);
  b->display_time = Fcurrent_time ();

  XSETFASTINT (w->window_end_pos, 0);
  XSETFASTINT (w->window_end_vpos, 0);
  bzero (&w->last_cursor, sizeof w->last_cursor);
  w->window_end_valid = Qnil;
  w->hscroll = w->min_hscroll = make_number (0);
  w->vscroll = 0;
  set_marker_both (w->pointm, buffer, BUF_PT (b), BUF_PT_BYTE (b));
  set_marker_restricted (w->start,
			 make_number (b->last_window_start),
			 buffer);
  w->start_at_line_beg = Qnil;
  w->force_start = Qnil;
  XSETFASTINT (w->last_modified, 0);
  XSETFASTINT (w->last_overlay_modified, 0);
  windows_or_buffers_changed++;

  /* We must select BUFFER for running the window-scroll-functions.
     If WINDOW is selected, switch permanently.
     Otherwise, switch but go back to the ambient buffer afterward.  */
  if (EQ (window, selected_window))
    Fset_buffer (buffer);
  /* We can't check ! NILP (Vwindow_scroll_functions) here
     because that might itself be a local variable.  */
  else if (window_initialized)
    {
      record_unwind_protect (Fset_window_buffer_unwind, Fcurrent_buffer ());
      Fset_buffer (buffer);
    }

  if (!keep_margins_p)
    {
      /* Set left and right marginal area width etc. from buffer.  */

      /* This may call adjust_window_margins three times, so
	 temporarily disable window margins.  */
      Lisp_Object save_left = w->left_margin_cols;
      Lisp_Object save_right = w->right_margin_cols;

      w->left_margin_cols = w->right_margin_cols = Qnil;

      Fset_window_fringes (window,
			   b->left_fringe_width, b->right_fringe_width,
			   b->fringes_outside_margins);

      Fset_window_scroll_bars (window,
			       b->scroll_bar_width,
			       b->vertical_scroll_bar_type, Qnil);

      w->left_margin_cols = save_left;
      w->right_margin_cols = save_right;

      Fset_window_margins (window,
			   b->left_margin_cols, b->right_margin_cols);
    }

  if (run_hooks_p)
    {
      if (! NILP (Vwindow_scroll_functions))
	run_hook_with_args_2 (Qwindow_scroll_functions, window,
			      Fmarker_position (w->start));

      if (! NILP (Vwindow_configuration_change_hook)
	  && ! NILP (Vrun_hooks))
	call1 (Vrun_hooks, Qwindow_configuration_change_hook);
    }

  unbind_to (count, Qnil);
}


DEFUN ("set-window-buffer", Fset_window_buffer, Sset_window_buffer, 2, 3, 0,
       doc: /* Make WINDOW display BUFFER as its contents.
BUFFER can be a buffer or the name of an existing buffer.
Optional third arg KEEP-MARGINS non-nil means that WINDOW's current
display margins, fringe widths, and scroll bar settings are maintained;
the default is to reset these from BUFFER's local settings or the frame
defaults.

This function runs the hook `window-scroll-functions'.  */)
     (window, buffer, keep_margins)
     register Lisp_Object window, buffer, keep_margins;
{
  register Lisp_Object tem;
  register struct window *w = decode_window (window);

  XSETWINDOW (window, w);
  buffer = Fget_buffer (buffer);
  CHECK_BUFFER (buffer);

  if (NILP (XBUFFER (buffer)->name))
    error ("Attempt to display deleted buffer");

  tem = w->buffer;
  if (NILP (tem))
    error ("Window is deleted");
  else if (! EQ (tem, Qt))	/* w->buffer is t when the window
				   is first being set up.  */
    {
      if (!NILP (w->dedicated) && !EQ (tem, buffer))
	error ("Window is dedicated to `%s'",
	       SDATA (XBUFFER (tem)->name));

      unshow_buffer (w);
    }

  set_window_buffer (window, buffer, 1, !NILP (keep_margins));
  return Qnil;
}

/* Note that selected_window can be nil
   when this is called from Fset_window_configuration.  */

DEFUN ("select-window", Fselect_window, Sselect_window, 1, 2, 0,
       doc: /* Select WINDOW.  Most editing will apply to WINDOW's buffer.
If WINDOW is not already selected, make WINDOW's buffer current
and make WINDOW the frame's selected window.  Return WINDOW.
Optional second arg NORECORD non-nil means
do not put this buffer at the front of the list of recently selected ones.

Note that the main editor command loop
selects the buffer of the selected window before each command.  */)
     (window, norecord)
     register Lisp_Object window, norecord;
{
  register struct window *w;
  register struct window *ow;
  struct frame *sf;

  CHECK_LIVE_WINDOW (window);

  w = XWINDOW (window);
  w->frozen_window_start_p = 0;

  ++window_select_count;
  XSETFASTINT (w->use_time, window_select_count);
  if (EQ (window, selected_window))
    return window;

  /* Store the current buffer's actual point into the
     old selected window.  It belongs to that window,
     and when the window is not selected, must be in the window.  */
  if (!NILP (selected_window))
    {
      ow = XWINDOW (selected_window);
      if (! NILP (ow->buffer))
	set_marker_both (ow->pointm, ow->buffer,
			 BUF_PT (XBUFFER (ow->buffer)),
			 BUF_PT_BYTE (XBUFFER (ow->buffer)));
    }

  selected_window = window;
  sf = SELECTED_FRAME ();
  if (XFRAME (WINDOW_FRAME (w)) != sf)
    {
      XFRAME (WINDOW_FRAME (w))->selected_window = window;
      /* Use this rather than Fhandle_switch_frame
	 so that FRAME_FOCUS_FRAME is moved appropriately as we
	 move around in the state where a minibuffer in a separate
	 frame is active.  */
      Fselect_frame (WINDOW_FRAME (w));
    }
  else
    sf->selected_window = window;

  if (NILP (norecord))
    record_buffer (w->buffer);
  Fset_buffer (w->buffer);

  XBUFFER (w->buffer)->last_selected_window = window;

  /* Go to the point recorded in the window.
     This is important when the buffer is in more
     than one window.  It also matters when
     redisplay_window has altered point after scrolling,
     because it makes the change only in the window.  */
  {
    register int new_point = marker_position (w->pointm);
    if (new_point < BEGV)
      SET_PT (BEGV);
    else if (new_point > ZV)
      SET_PT (ZV);
    else
      SET_PT (new_point);
  }

  windows_or_buffers_changed++;
  return window;
}

static Lisp_Object
select_window_norecord (window)
     Lisp_Object window;
{
  return Fselect_window (window, Qt);
}

/* Deiconify the frame containing the window WINDOW,
   unless it is the selected frame;
   then return WINDOW.

   The reason for the exception for the selected frame
   is that it seems better not to change the selected frames visibility
   merely because of displaying a different buffer in it.
   The deiconification is useful when a buffer gets shown in
   another frame that you were not using lately.  */

static Lisp_Object
display_buffer_1 (window)
     Lisp_Object window;
{
  Lisp_Object frame = XWINDOW (window)->frame;
  FRAME_PTR f = XFRAME (frame);

  FRAME_SAMPLE_VISIBILITY (f);

  if (EQ (frame, selected_frame))
    ; /* Assume the selected frame is already visible enough.  */
  else if (minibuf_level > 0
	   && MINI_WINDOW_P (XWINDOW (selected_window))
	   && WINDOW_LIVE_P (minibuf_selected_window)
	   && EQ (frame, WINDOW_FRAME (XWINDOW (minibuf_selected_window))))
    ; /* Assume the frame from which we invoked the minibuffer is visible.  */
  else
    {
      if (FRAME_ICONIFIED_P (f))
	Fmake_frame_visible (frame);
      else if (FRAME_VISIBLE_P (f))
	Fraise_frame (frame);
    }

  return window;
}

DEFUN ("special-display-p", Fspecial_display_p, Sspecial_display_p, 1, 1, 0,
       doc: /* Returns non-nil if a buffer named BUFFER-NAME gets a special frame.
If the value is t, `display-buffer' or `pop-to-buffer' would create a
special frame for that buffer using the default frame parameters.

If the value is a list, it is a list of frame parameters that would be used
to make a frame for that buffer.
The variables `special-display-buffer-names'
and `special-display-regexps' control this.  */)
     (buffer_name)
     Lisp_Object buffer_name;
{
  Lisp_Object tem;

  CHECK_STRING (buffer_name);

  tem = Fmember (buffer_name, Vspecial_display_buffer_names);
  if (!NILP (tem))
    return Qt;

  tem = Fassoc (buffer_name, Vspecial_display_buffer_names);
  if (!NILP (tem))
    return XCDR (tem);

  for (tem = Vspecial_display_regexps; CONSP (tem); tem = XCDR (tem))
    {
      Lisp_Object car = XCAR (tem);
      if (STRINGP (car)
	  && fast_string_match (car, buffer_name) >= 0)
	return Qt;
      else if (CONSP (car)
	       && STRINGP (XCAR (car))
	       && fast_string_match (XCAR (car), buffer_name) >= 0)
	return XCDR (car);
    }
  return Qnil;
}

DEFUN ("same-window-p", Fsame_window_p, Ssame_window_p, 1, 1, 0,
       doc: /* Returns non-nil if a buffer named BUFFER-NAME would use the same window.
More precisely, if `display-buffer' or `pop-to-buffer' would display
that buffer in the selected window rather than (as usual) in some other window.
See `same-window-buffer-names' and `same-window-regexps'.  */)
     (buffer_name)
     Lisp_Object buffer_name;
{
  Lisp_Object tem;

  CHECK_STRING (buffer_name);

  tem = Fmember (buffer_name, Vsame_window_buffer_names);
  if (!NILP (tem))
    return Qt;

  tem = Fassoc (buffer_name, Vsame_window_buffer_names);
  if (!NILP (tem))
    return Qt;

  for (tem = Vsame_window_regexps; CONSP (tem); tem = XCDR (tem))
    {
      Lisp_Object car = XCAR (tem);
      if (STRINGP (car)
	  && fast_string_match (car, buffer_name) >= 0)
	return Qt;
      else if (CONSP (car)
	       && STRINGP (XCAR (car))
	       && fast_string_match (XCAR (car), buffer_name) >= 0)
	return Qt;
    }
  return Qnil;
}

/* Use B so the default is (other-buffer).  */
DEFUN ("display-buffer", Fdisplay_buffer, Sdisplay_buffer, 1, 3,
       "BDisplay buffer: \nP",
       doc: /* Make BUFFER appear in some window but don't select it.
BUFFER must be the name of an existing buffer, or, when called from Lisp,
a buffer.
If BUFFER is shown already in some window, just use that one,
unless the window is the selected window and the optional second
argument NOT-THIS-WINDOW is non-nil (interactively, with prefix arg).
If `pop-up-frames' is non-nil, make a new frame if no window shows BUFFER.
Returns the window displaying BUFFER.
If `display-buffer-reuse-frames' is non-nil, and another frame is currently
displaying BUFFER, then simply raise that frame.

The variables `special-display-buffer-names',
`special-display-regexps', `same-window-buffer-names', and
`same-window-regexps' customize how certain buffer names are handled.
The latter two take effect only if NOT-THIS-WINDOW is nil.

If optional argument FRAME is `visible', check all visible frames
for a window to use.
If FRAME is 0, check all visible and iconified frames.
If FRAME is t, check all frames.
If FRAME is a frame, check only that frame.
If FRAME is nil, check only the selected frame
 (actually the last nonminibuffer frame),
 unless `pop-up-frames' or `display-buffer-reuse-frames' is non-nil,
 which means search visible and iconified frames.

If a full-width window on a splittable frame is available to display
the buffer, it may be split, subject to the value of the variable
`split-height-threshold'.

If `even-window-heights' is non-nil, window heights will be evened out
if displaying the buffer causes two vertically adjacent windows to be
displayed.  */)
     (buffer, not_this_window, frame)
     register Lisp_Object buffer, not_this_window, frame;
{
  register Lisp_Object window, tem, swp;
  struct frame *f;

  swp = Qnil;
  buffer = Fget_buffer (buffer);
  CHECK_BUFFER (buffer);

  if (!NILP (Vdisplay_buffer_function))
    return call2 (Vdisplay_buffer_function, buffer, not_this_window);

  if (NILP (not_this_window)
      && XBUFFER (XWINDOW (selected_window)->buffer) == XBUFFER (buffer))
    return display_buffer_1 (selected_window);

  /* See if the user has specified this buffer should appear
     in the selected window.  */
  if (NILP (not_this_window))
    {
      swp = Fsame_window_p (XBUFFER (buffer)->name);
      if (!NILP (swp) && !no_switch_window (selected_window))
	{
	  Fswitch_to_buffer (buffer, Qnil);
	  return display_buffer_1 (selected_window);
	}
    }

  /* If the user wants pop-up-frames or display-buffer-reuse-frames,
     look for a window showing BUFFER on any visible or iconified frame.
     Otherwise search only the current frame.  */
  if (! NILP (frame))
    tem = frame;
  else if (pop_up_frames
	   || display_buffer_reuse_frames
	   || last_nonminibuf_frame == 0)
    XSETFASTINT (tem, 0);
  else
    XSETFRAME (tem, last_nonminibuf_frame);

  window = Fget_buffer_window (buffer, tem);
  if (!NILP (window)
      && (NILP (not_this_window) || !EQ (window, selected_window)))
    return display_buffer_1 (window);

  /* Certain buffer names get special handling.  */
  if (!NILP (Vspecial_display_function) && NILP (swp))
    {
      tem = Fspecial_display_p (XBUFFER (buffer)->name);
      if (EQ (tem, Qt))
	return call1 (Vspecial_display_function, buffer);
      if (CONSP (tem))
	return call2 (Vspecial_display_function, buffer, tem);
    }

  /* If there are no frames open that have more than a minibuffer,
     we need to create a new frame.  */
  if (pop_up_frames || last_nonminibuf_frame == 0)
    {
      window = Fframe_selected_window (call0 (Vpop_up_frame_function));
      Fset_window_buffer (window, buffer, Qnil);
      return display_buffer_1 (window);
    }

  f = SELECTED_FRAME ();
  if (pop_up_windows
      || FRAME_MINIBUF_ONLY_P (f)
      /* If the current frame is a special display frame,
	 don't try to reuse its windows.  */
      || !NILP (XWINDOW (FRAME_ROOT_WINDOW (f))->dedicated))
    {
      Lisp_Object frames;

      frames = Qnil;
      if (FRAME_MINIBUF_ONLY_P (f))
	XSETFRAME (frames, last_nonminibuf_frame);
      /* Don't try to create a window if we would get an error.  */
      if (split_height_threshold < window_min_height << 1)
	split_height_threshold = window_min_height << 1;

      /* Note that both Fget_largest_window and Fget_lru_window
	 ignore minibuffers and dedicated windows.
	 This means they can return nil.  */

      /* If the frame we would try to split cannot be split,
	 try other frames.  */
      if (FRAME_NO_SPLIT_P (NILP (frames) ? f : last_nonminibuf_frame))
	{
	  /* Try visible frames first.  */
	  window = Fget_largest_window (Qvisible, Qt);
	  /* If that didn't work, try iconified frames.  */
	  if (NILP (window))
	    window = Fget_largest_window (make_number (0), Qt);
#if 0     /* Don't try windows on other displays.  */
	  if (NILP (window))
	    window = Fget_largest_window (Qt, Qt);
#endif
	}
      else
	window = Fget_largest_window (frames, Qt);

      /* If we got a tall enough full-width window that can be split,
	 split it.  */
      if (!NILP (window)
	  && ! FRAME_NO_SPLIT_P (XFRAME (XWINDOW (window)->frame))
	  && window_height (window) >= split_height_threshold
	  && WINDOW_FULL_WIDTH_P (XWINDOW (window)))
	window = Fsplit_window (window, Qnil, Qnil);
      else
	{
	  Lisp_Object upper, lower, other;

	  window = Fget_lru_window (frames, Qt);
	  /* If the LRU window is selected, and big enough,
	     and can be split, split it.  */
	  if (!NILP (window)
	      && ! FRAME_NO_SPLIT_P (XFRAME (XWINDOW (window)->frame))
	      && (EQ (window, selected_window)
		  || EQ (XWINDOW (window)->parent, Qnil))
	      && window_height (window) >= window_min_height << 1)
	    window = Fsplit_window (window, Qnil, Qnil);
	  else
	    window = Fget_lru_window (frames, Qnil);
	  /* If Fget_lru_window returned nil, try other approaches.  */

	  /* Try visible frames first.  */
	  if (NILP (window))
	    window = Fget_buffer_window (buffer, Qvisible);
	  if (NILP (window))
	    window = Fget_largest_window (Qvisible, Qnil);
	  /* If that didn't work, try iconified frames.  */
	  if (NILP (window))
	    window = Fget_buffer_window (buffer, make_number (0));
	  if (NILP (window))
	    window = Fget_largest_window (make_number (0), Qnil);

#if 0     /* Don't try frames on other displays.  */
	  if (NILP (window))
	    window = Fget_buffer_window (buffer, Qt);
	  if (NILP (window))
	    window = Fget_largest_window (Qt, Qnil);
#endif
	  /* As a last resort, make a new frame.  */
	  if (NILP (window))
	    window = Fframe_selected_window (call0 (Vpop_up_frame_function));
	  /* If window appears above or below another,
	     even out their heights.  */
	  other = upper = lower = Qnil;
	  if (!NILP (XWINDOW (window)->prev))
	    other = upper = XWINDOW (window)->prev, lower = window;
	  if (!NILP (XWINDOW (window)->next))
	    other = lower = XWINDOW (window)->next, upper = window;
	  if (!NILP (other)
	      && !NILP (Veven_window_heights)
	      /* Check that OTHER and WINDOW are vertically arrayed.  */
	      && !EQ (XWINDOW (other)->top_line, XWINDOW (window)->top_line)
	      && (XFASTINT (XWINDOW (other)->total_lines)
		  > XFASTINT (XWINDOW (window)->total_lines)))
	    {
	      int total = (XFASTINT (XWINDOW (other)->total_lines)
			   + XFASTINT (XWINDOW (window)->total_lines));
	      enlarge_window (upper,
			      total / 2 - XFASTINT (XWINDOW (upper)->total_lines),
			      0);
	    }
	}
    }
  else
    window = Fget_lru_window (Qnil, Qnil);

  Fset_window_buffer (window, buffer, Qnil);
  return display_buffer_1 (window);
}


DEFUN ("force-window-update", Fforce_window_update, Sforce_window_update,
       0, 1, 0,
       doc: /* Force all windows to be updated on next redisplay.
If optional arg OBJECT is a window, force redisplay of that window only.
If OBJECT is a buffer or buffer name, force redisplay of all windows
displaying that buffer.  */)
     (object)
     Lisp_Object object;
{
  if (NILP (object))
    {
      windows_or_buffers_changed++;
      update_mode_lines++;
      return Qt;
    }

  if (WINDOWP (object))
    {
      struct window *w = XWINDOW (object);
      mark_window_display_accurate (object, 0);
      w->update_mode_line = Qt;
      if (BUFFERP (w->buffer))
	XBUFFER (w->buffer)->prevent_redisplay_optimizations_p = 1;
      ++update_mode_lines;
      return Qt;
    }

  if (STRINGP (object))
    object = Fget_buffer (object);
  if (BUFFERP (object) && !NILP (XBUFFER (object)->name))
    {
      /* Walk all windows looking for buffer, and force update
	 of each of those windows.  */

      object = window_loop (REDISPLAY_BUFFER_WINDOWS, object, 0, Qvisible);
      return NILP (object) ? Qnil : Qt;
    }

  /* If nothing suitable was found, just return.
     We could signal an error, but this feature will typically be used
     asynchronously in timers or process sentinels, so we don't.  */
  return Qnil;
}


void
temp_output_buffer_show (buf)
     register Lisp_Object buf;
{
  register struct buffer *old = current_buffer;
  register Lisp_Object window;
  register struct window *w;

  XBUFFER (buf)->directory = current_buffer->directory;

  Fset_buffer (buf);
  BUF_SAVE_MODIFF (XBUFFER (buf)) = MODIFF;
  BEGV = BEG;
  ZV = Z;
  SET_PT (BEG);
#if 0  /* rms: there should be no reason for this.  */
  XBUFFER (buf)->prevent_redisplay_optimizations_p = 1;
#endif
  set_buffer_internal (old);

  if (!NILP (Vtemp_buffer_show_function))
    call1 (Vtemp_buffer_show_function, buf);
  else
    {
      window = Fdisplay_buffer (buf, Qnil, Qnil);

      if (!EQ (XWINDOW (window)->frame, selected_frame))
	Fmake_frame_visible (WINDOW_FRAME (XWINDOW (window)));
      Vminibuf_scroll_window = window;
      w = XWINDOW (window);
      XSETFASTINT (w->hscroll, 0);
      XSETFASTINT (w->min_hscroll, 0);
      set_marker_restricted_both (w->start, buf, BEG, BEG);
      set_marker_restricted_both (w->pointm, buf, BEG, BEG);

      /* Run temp-buffer-show-hook, with the chosen window selected
	 and its buffer current.  */

      if (!NILP (Vrun_hooks)
	  && !NILP (Fboundp (Qtemp_buffer_show_hook))
	  && !NILP (Fsymbol_value (Qtemp_buffer_show_hook)))
	{
	  int count = SPECPDL_INDEX ();
	  Lisp_Object prev_window, prev_buffer;
	  prev_window = selected_window;
	  XSETBUFFER (prev_buffer, old);

	  /* Select the window that was chosen, for running the hook.
	     Note: Both Fselect_window and select_window_norecord may
	     set-buffer to the buffer displayed in the window,
	     so we need to save the current buffer.  --stef  */
	  record_unwind_protect (Fset_buffer, prev_buffer);
	  record_unwind_protect (select_window_norecord, prev_window);
	  Fselect_window (window, Qt);
	  Fset_buffer (w->buffer);
	  call1 (Vrun_hooks, Qtemp_buffer_show_hook);
	  unbind_to (count, Qnil);
	}
    }
}

static void
make_dummy_parent (window)
     Lisp_Object window;
{
  Lisp_Object new;
  register struct window *o, *p;
  int i;

  o = XWINDOW (window);
  p = allocate_window ();
  for (i = 0; i < VECSIZE (struct window); ++i)
    ((struct Lisp_Vector *) p)->contents[i]
      = ((struct Lisp_Vector *)o)->contents[i];
  XSETWINDOW (new, p);

  ++sequence_number;
  XSETFASTINT (p->sequence_number, sequence_number);

  /* Put new into window structure in place of window */
  replace_window (window, new);

  o->next = Qnil;
  o->prev = Qnil;
  o->vchild = Qnil;
  o->hchild = Qnil;
  o->parent = new;

  p->start = Qnil;
  p->pointm = Qnil;
  p->buffer = Qnil;
}

DEFUN ("split-window", Fsplit_window, Ssplit_window, 0, 3, "",
       doc: /* Split WINDOW, putting SIZE lines in the first of the pair.
WINDOW defaults to selected one and SIZE to half its size.
If optional third arg HORFLAG is non-nil, split side by side
and put SIZE columns in the first of the pair.  In that case,
SIZE includes that window's scroll bar, or the divider column to its right.
Interactively, all arguments are nil.

Returns the newly created window (which is the lower or rightmost one).
The upper or leftmost window is the original one, and remains selected
if it was selected before.

See Info node `(elisp)Splitting Windows' for more details and examples.*/)
     (window, size, horflag)
     Lisp_Object window, size, horflag;
{
  register Lisp_Object new;
  register struct window *o, *p;
  FRAME_PTR fo;
  register int size_int;

  if (NILP (window))
    window = selected_window;
  else
    CHECK_LIVE_WINDOW (window);

  o = XWINDOW (window);
  fo = XFRAME (WINDOW_FRAME (o));

  if (NILP (size))
    {
      if (!NILP (horflag))
	/* Calculate the size of the left-hand window, by dividing
	   the usable space in columns by two.
	   We round up, since the left-hand window may include
	   a dividing line, while the right-hand may not.  */
	size_int = (XFASTINT (o->total_cols) + 1) >> 1;
      else
	size_int = XFASTINT (o->total_lines) >> 1;
    }
  else
    {
      CHECK_NUMBER (size);
      size_int = XINT (size);
    }

  if (MINI_WINDOW_P (o))
    error ("Attempt to split minibuffer window");
  else if (window_fixed_size_p (o, !NILP (horflag), 0))
    error ("Attempt to split fixed-size window");

  check_min_window_sizes ();

  if (NILP (horflag))
    {
      if (size_int < window_min_height)
	error ("Window height %d too small (after splitting)", size_int);
      if (size_int + window_min_height > XFASTINT (o->total_lines))
	error ("Window height %d too small (after splitting)",
	       XFASTINT (o->total_lines) - size_int);
      if (NILP (o->parent)
	  || NILP (XWINDOW (o->parent)->vchild))
	{
	  make_dummy_parent (window);
	  new = o->parent;
	  XWINDOW (new)->vchild = window;
	}
    }
  else
    {
      if (size_int < window_min_width)
	error ("Window width %d too small (after splitting)", size_int);

      if (size_int + window_min_width > XFASTINT (o->total_cols))
	error ("Window width %d too small (after splitting)",
	       XFASTINT (o->total_cols) - size_int);
      if (NILP (o->parent)
	  || NILP (XWINDOW (o->parent)->hchild))
	{
	  make_dummy_parent (window);
	  new = o->parent;
	  XWINDOW (new)->hchild = window;
	}
    }

  /* Now we know that window's parent is a vertical combination
     if we are dividing vertically, or a horizontal combination
     if we are making side-by-side windows */

  windows_or_buffers_changed++;
  FRAME_WINDOW_SIZES_CHANGED (fo) = 1;
  new = make_window ();
  p = XWINDOW (new);

  p->frame = o->frame;
  p->next = o->next;
  if (!NILP (p->next))
    XWINDOW (p->next)->prev = new;
  p->prev = window;
  o->next = new;
  p->parent = o->parent;
  p->buffer = Qt;
  p->window_end_valid = Qnil;
  bzero (&p->last_cursor, sizeof p->last_cursor);

  /* Duplicate special geometry settings.  */

  p->left_margin_cols = o->left_margin_cols;
  p->right_margin_cols = o->right_margin_cols;
  p->left_fringe_width = o->left_fringe_width;
  p->right_fringe_width = o->right_fringe_width;
  p->fringes_outside_margins = o->fringes_outside_margins;
  p->scroll_bar_width = o->scroll_bar_width;
  p->vertical_scroll_bar_type = o->vertical_scroll_bar_type;

  /* Apportion the available frame space among the two new windows */

  if (!NILP (horflag))
    {
      p->total_lines = o->total_lines;
      p->top_line = o->top_line;
      XSETFASTINT (p->total_cols, XFASTINT (o->total_cols) - size_int);
      XSETFASTINT (o->total_cols, size_int);
      XSETFASTINT (p->left_col, XFASTINT (o->left_col) + size_int);
      adjust_window_margins (p);
      adjust_window_margins (o);
    }
  else
    {
      p->left_col = o->left_col;
      p->total_cols = o->total_cols;
      XSETFASTINT (p->total_lines, XFASTINT (o->total_lines) - size_int);
      XSETFASTINT (o->total_lines, size_int);
      XSETFASTINT (p->top_line, XFASTINT (o->top_line) + size_int);
    }

  /* Adjust glyph matrices.  */
  adjust_glyphs (fo);

  Fset_window_buffer (new, o->buffer, Qt);
  return new;
}

DEFUN ("enlarge-window", Fenlarge_window, Senlarge_window, 1, 2, "p",
       doc: /* Make current window ARG lines bigger.
From program, optional second arg non-nil means grow sideways ARG columns.
Interactively, if an argument is not given, make the window one line bigger.
If HORIZONTAL is non-nil, enlarge horizontally instead of vertically.
This function can delete windows, even the second window, if they get
too small.  */)
     (arg, horizontal)
     Lisp_Object arg, horizontal;
{
  CHECK_NUMBER (arg);
  enlarge_window (selected_window, XINT (arg), !NILP (horizontal));

  if (! NILP (Vwindow_configuration_change_hook))
    call1 (Vrun_hooks, Qwindow_configuration_change_hook);

  return Qnil;
}

DEFUN ("shrink-window", Fshrink_window, Sshrink_window, 1, 2, "p",
       doc: /* Make current window ARG lines smaller.
From program, optional second arg non-nil means shrink sideways arg columns.
Interactively, if an argument is not given, make the window one line smaller.  Only
siblings to the right or below are changed.  */)
     (arg, side)
     Lisp_Object arg, side;
{
  CHECK_NUMBER (arg);
  enlarge_window (selected_window, -XINT (arg), !NILP (side));

  if (! NILP (Vwindow_configuration_change_hook))
    call1 (Vrun_hooks, Qwindow_configuration_change_hook);

  return Qnil;
}

int
window_height (window)
     Lisp_Object window;
{
  register struct window *p = XWINDOW (window);
  return WINDOW_TOTAL_LINES (p);
}

int
window_width (window)
     Lisp_Object window;
{
  register struct window *p = XWINDOW (window);
  return WINDOW_TOTAL_COLS (p);
}


#define CURBEG(w) \
  *(horiz_flag ? &(XWINDOW (w)->left_col) : &(XWINDOW (w)->top_line))

#define CURSIZE(w) \
  *(horiz_flag ? &(XWINDOW (w)->total_cols) : &(XWINDOW (w)->total_lines))


/* Enlarge WINDOW by DELTA.
   HORIZ_FLAG nonzero means enlarge it horizontally;
   zero means do it vertically.

   Siblings of the selected window are resized to fulfill the size
   request.  If they become too small in the process, they will be
   deleted.  */

static void
enlarge_window (window, delta, horiz_flag)
     Lisp_Object window;
     int delta, horiz_flag;
{
  Lisp_Object parent, next, prev;
  struct window *p;
  Lisp_Object *sizep;
  int maximum;
  int (*sizefun) P_ ((Lisp_Object))
    = horiz_flag ? window_width : window_height;
  void (*setsizefun) P_ ((Lisp_Object, int, int))
    = (horiz_flag ? set_window_width : set_window_height);

  /* Check values of window_min_width and window_min_height for
     validity.  */
  check_min_window_sizes ();

  /* Give up if this window cannot be resized.  */
  if (window_fixed_size_p (XWINDOW (window), horiz_flag, 1))
    error ("Window is not resizable");

  /* Find the parent of the selected window.  */
  while (1)
    {
      p = XWINDOW (window);
      parent = p->parent;

      if (NILP (parent))
	{
	  if (horiz_flag)
	    error ("No other window to side of this one");
	  break;
	}

      if (horiz_flag
	  ? !NILP (XWINDOW (parent)->hchild)
	  : !NILP (XWINDOW (parent)->vchild))
	break;

      window = parent;
    }

  sizep = &CURSIZE (window);

  {
    register int maxdelta;

    /* Compute the maximum size increment this window can have.  */

    maxdelta = (!NILP (parent) ? (*sizefun) (parent) - XINT (*sizep)
		/* This is a main window followed by a minibuffer.  */
		: !NILP (p->next) ? ((*sizefun) (p->next)
				     - window_min_size (XWINDOW (p->next),
							horiz_flag, 0, 0))
		/* This is a minibuffer following a main window.  */
		: !NILP (p->prev) ? ((*sizefun) (p->prev)
				     - window_min_size (XWINDOW (p->prev),
							horiz_flag, 0, 0))
		/* This is a frame with only one window, a minibuffer-only
		   or a minibufferless frame.  */
		: (delta = 0));

    if (delta > maxdelta)
      /* This case traps trying to make the minibuffer
	 the full frame, or make the only window aside from the
	 minibuffer the full frame.  */
      delta = maxdelta;
  }

  if (XINT (*sizep) + delta < window_min_size (XWINDOW (window), horiz_flag, 0, 0))
    {
      delete_window (window);
      return;
    }

  if (delta == 0)
    return;

  /* Find the total we can get from other siblings without deleting them.  */
  maximum = 0;
  for (next = p->next; ! NILP (next); next = XWINDOW (next)->next)
    maximum += (*sizefun) (next) - window_min_size (XWINDOW (next),
						    horiz_flag, 0, 0);
  for (prev = p->prev; ! NILP (prev); prev = XWINDOW (prev)->prev)
    maximum += (*sizefun) (prev) - window_min_size (XWINDOW (prev),
						    horiz_flag, 0, 0);

  /* If we can get it all from them without deleting them, do so.  */
  if (delta <= maximum)
    {
      Lisp_Object first_unaffected;
      Lisp_Object first_affected;
      int fixed_p;

      next = p->next;
      prev = p->prev;
      first_affected = window;
      /* Look at one sibling at a time,
	 moving away from this window in both directions alternately,
	 and take as much as we can get without deleting that sibling.  */
      while (delta != 0
	     && (!NILP (next) || !NILP (prev)))
	{
	  if (! NILP (next))
	    {
	      int this_one = ((*sizefun) (next)
			      - window_min_size (XWINDOW (next),
						 horiz_flag, 0, &fixed_p));
	      if (!fixed_p)
		{
		  if (this_one > delta)
		    this_one = delta;

		  (*setsizefun) (next, (*sizefun) (next) - this_one, 0);
		  (*setsizefun) (window, XINT (*sizep) + this_one, 0);

		  delta -= this_one;
		}

	      next = XWINDOW (next)->next;
	    }

	  if (delta == 0)
	    break;

	  if (! NILP (prev))
	    {
	      int this_one = ((*sizefun) (prev)
			      - window_min_size (XWINDOW (prev),
						 horiz_flag, 0, &fixed_p));
	      if (!fixed_p)
		{
		  if (this_one > delta)
		    this_one = delta;

		  first_affected = prev;

		  (*setsizefun) (prev, (*sizefun) (prev) - this_one, 0);
		  (*setsizefun) (window, XINT (*sizep) + this_one, 0);

		  delta -= this_one;
		}

	      prev = XWINDOW (prev)->prev;
	    }
	}

      xassert (delta == 0);

      /* Now recalculate the edge positions of all the windows affected,
	 based on the new sizes.  */
      first_unaffected = next;
      prev = first_affected;
      for (next = XWINDOW (prev)->next; ! EQ (next, first_unaffected);
	   prev = next, next = XWINDOW (next)->next)
	{
	  XSETINT (CURBEG (next), XINT (CURBEG (prev)) + (*sizefun) (prev));
	  /* This does not change size of NEXT,
	     but it propagates the new top edge to its children */
	  (*setsizefun) (next, (*sizefun) (next), 0);
	}
    }
  else
    {
      register int delta1;
      register int opht = (*sizefun) (parent);

      if (opht <= XINT (*sizep) + delta)
	{
	  /* If trying to grow this window to or beyond size of the parent,
	     just delete all the sibling windows.  */
	  Lisp_Object start, tem, next;

	  start = XWINDOW (parent)->vchild;
	  if (NILP (start))
	    start = XWINDOW (parent)->hchild;

	  /* Delete any siblings that come after WINDOW.  */
	  tem = XWINDOW (window)->next;
	  while (! NILP (tem))
	    {
	      next = XWINDOW (tem)->next;
	      delete_window (tem);
	      tem = next;
	    }

	  /* Delete any siblings that come after WINDOW.
	     Note that if START is not WINDOW, then WINDOW still
	     Fhas siblings, so WINDOW has not yet replaced its parent.  */
	  tem = start;
	  while (! EQ (tem, window))
	    {
	      next = XWINDOW (tem)->next;
	      delete_window (tem);
	      tem = next;
	    }
	}
      else
	{
	  /* Otherwise, make delta1 just right so that if we add
	     delta1 lines to this window and to the parent, and then
	     shrink the parent back to its original size, the new
	     proportional size of this window will increase by delta.

	     The function size_window will compute the new height h'
	     of the window from delta1 as:

	     e = delta1/n
	     x = delta1 - delta1/n * n for the 1st resizable child
	     h' = h + e + x

	     where n is the number of children that can be resized.
	     We can ignore x by choosing a delta1 that is a multiple of
	     n.  We want the height of this window to come out as

	     h' = h + delta

	     So, delta1 must be

	     h + e = h + delta
	     delta1/n = delta
	     delta1 = n * delta.

	     The number of children n equals the number of resizable
	     children of this window + 1 because we know window itself
	     is resizable (otherwise we would have signalled an error).  */

	  struct window *w = XWINDOW (window);
	  Lisp_Object s;
	  int n = 1;

	  for (s = w->next; !NILP (s); s = XWINDOW (s)->next)
	    if (!window_fixed_size_p (XWINDOW (s), horiz_flag, 0))
	      ++n;
	  for (s = w->prev; !NILP (s); s = XWINDOW (s)->prev)
	    if (!window_fixed_size_p (XWINDOW (s), horiz_flag, 0))
	      ++n;

	  delta1 = n * delta;

	  /* Add delta1 lines or columns to this window, and to the parent,
	     keeping things consistent while not affecting siblings.  */
	  XSETINT (CURSIZE (parent), opht + delta1);
	  (*setsizefun) (window, XINT (*sizep) + delta1, 0);

	  /* Squeeze out delta1 lines or columns from our parent,
	     shriking this window and siblings proportionately.
	     This brings parent back to correct size.
	     Delta1 was calculated so this makes this window the desired size,
	     taking it all out of the siblings.  */
	  (*setsizefun) (parent, opht, 0);

	}
    }

  XSETFASTINT (p->last_modified, 0);
  XSETFASTINT (p->last_overlay_modified, 0);

  /* Adjust glyph matrices. */
  adjust_glyphs (XFRAME (WINDOW_FRAME (XWINDOW (window))));
}


/* Adjust the size of WINDOW by DELTA, moving only its trailing edge.
   HORIZ_FLAG nonzero means adjust the width, moving the right edge.
   zero means adjust the height, moving the bottom edge.

   Following siblings of the selected window are resized to fulfill
   the size request.  If they become too small in the process, they
   are not deleted; instead, we signal an error.  */

static void
adjust_window_trailing_edge (window, delta, horiz_flag)
     Lisp_Object window;
     int delta, horiz_flag;
{
  Lisp_Object parent, child;
  struct window *p;
  Lisp_Object old_config = Fcurrent_window_configuration (Qnil);
  int delcount = window_deletion_count;

  /* Check values of window_min_width and window_min_height for
     validity.  */
  check_min_window_sizes ();

  if (NILP (window))
    window = Fselected_window ();

  CHECK_WINDOW (window);

  /* Give up if this window cannot be resized.  */
  if (window_fixed_size_p (XWINDOW (window), horiz_flag, 1))
    error ("Window is not resizable");

  while (1)
    {
      Lisp_Object first_parallel = Qnil;

      if (NILP (window))
	{
	  /* This happens if WINDOW on the previous iteration was
	     at top level of the window tree.  */
	  Fset_window_configuration (old_config);
	  error ("Specified window edge is fixed");
	}

      p = XWINDOW (window);
      parent = p->parent;

      /* See if this level has windows in parallel in the specified
	 direction.  If so, set FIRST_PARALLEL to the first one.  */
      if (horiz_flag)
	{
	  if (! NILP (parent) && !NILP (XWINDOW (parent)->vchild))
	    first_parallel = XWINDOW (parent)->vchild;
	  else if (NILP (parent) && !NILP (p->next))
	    {
	      /* Handle the vertical chain of main window and minibuffer
		 which has no parent.  */
	      first_parallel = window;
	      while (! NILP (XWINDOW (first_parallel)->prev))
		first_parallel = XWINDOW (first_parallel)->prev;
	    }
	}
      else
	{
	  if (! NILP (parent) && !NILP (XWINDOW (parent)->hchild))
	    first_parallel = XWINDOW (parent)->hchild;
	}

      /* If this level's succession is in the desired dimension,
	 and this window is the last one, and there is no higher level,
	 its trailing edge is fixed.  */
      if (NILP (XWINDOW (window)->next) && NILP (first_parallel)
	  && NILP (parent))
	{
	  Fset_window_configuration (old_config);
	  error ("Specified window edge is fixed");
	}

      /* Don't make this window too small.  */
      if (XINT (CURSIZE (window)) + delta
	  < (horiz_flag ? window_min_width : window_min_height))
	{
	  Fset_window_configuration (old_config);
	  error ("Cannot adjust window size as specified");
	}

      /* Clear out some redisplay caches.  */
      XSETFASTINT (p->last_modified, 0);
      XSETFASTINT (p->last_overlay_modified, 0);

      /* Adjust this window's edge.  */
      XSETINT (CURSIZE (window),
	       XINT (CURSIZE (window)) + delta);

      /* If this window has following siblings in the desired dimension,
	 make them smaller, and exit the loop.

	 (If we reach the top of the tree and can never do this,
	 we will fail and report an error, above.)  */
      if (NILP (first_parallel))
	{
	  if (!NILP (p->next))
	    {
              /* This may happen for the minibuffer.  In that case
                 the window_deletion_count check below does not work.  */
              if (XINT (CURSIZE (p->next)) - delta <= 0)
                {
                  Fset_window_configuration (old_config);
                  error ("Cannot adjust window size as specified");
                }

	      XSETINT (CURBEG (p->next),
		       XINT (CURBEG (p->next)) + delta);
	      size_window (p->next, XINT (CURSIZE (p->next)) - delta,
			   horiz_flag, 0, 1, 0);
	      break;
	    }
	}
      else
	/* Here we have a chain of parallel siblings, in the other dimension.
	   Change the size of the other siblings.  */
	for (child = first_parallel;
	     ! NILP (child);
	     child = XWINDOW (child)->next)
	  if (! EQ (child, window))
	    size_window (child, XINT (CURSIZE (child)) + delta,
			 horiz_flag, 0, 0, 1);

      window = parent;
    }

  /* If we made a window so small it got deleted,
     we failed.  Report failure.  */
  if (delcount != window_deletion_count)
    {
      Fset_window_configuration (old_config);
      error ("Cannot adjust window size as specified");
    }

  /* Adjust glyph matrices. */
  adjust_glyphs (XFRAME (WINDOW_FRAME (XWINDOW (window))));
}

#undef CURBEG
#undef CURSIZE

DEFUN ("adjust-window-trailing-edge", Fadjust_window_trailing_edge,
       Sadjust_window_trailing_edge, 3, 3, 0,
       doc: /* Adjust the bottom or right edge of WINDOW by DELTA.
If HORIZONTAL is non-nil, that means adjust the width, moving the right edge.
Otherwise, adjust the height, moving the bottom edge.

Following siblings of the selected window are resized to fulfill
the size request.  If they become too small in the process, they
are not deleted; instead, we signal an error.  */)
  (window, delta, horizontal)
  Lisp_Object window, delta, horizontal;
{
  CHECK_NUMBER (delta);
  adjust_window_trailing_edge (window, XINT (delta), !NILP (horizontal));

  if (! NILP (Vwindow_configuration_change_hook))
    call1 (Vrun_hooks, Qwindow_configuration_change_hook);

  return Qnil;
}



/***********************************************************************
			Resizing Mini-Windows
 ***********************************************************************/

static void shrink_window_lowest_first P_ ((struct window *, int));

enum save_restore_action
{
    CHECK_ORIG_SIZES,
    SAVE_ORIG_SIZES,
    RESTORE_ORIG_SIZES
};

static int save_restore_orig_size P_ ((struct window *,
                                       enum save_restore_action));

/* Shrink windows rooted in window W to HEIGHT.  Take the space needed
   from lowest windows first.  */

static void
shrink_window_lowest_first (w, height)
     struct window *w;
     int height;
{
  struct window *c;
  Lisp_Object child;
  int old_height;

  xassert (!MINI_WINDOW_P (w));

  /* Set redisplay hints.  */
  XSETFASTINT (w->last_modified, 0);
  XSETFASTINT (w->last_overlay_modified, 0);
  windows_or_buffers_changed++;
  FRAME_WINDOW_SIZES_CHANGED (XFRAME (WINDOW_FRAME (w))) = 1;

  old_height = XFASTINT (w->total_lines);
  XSETFASTINT (w->total_lines, height);

  if (!NILP (w->hchild))
    {
      for (child = w->hchild; !NILP (child); child = c->next)
	{
	  c = XWINDOW (child);
	  c->top_line = w->top_line;
	  shrink_window_lowest_first (c, height);
	}
    }
  else if (!NILP (w->vchild))
    {
      Lisp_Object last_child;
      int delta = old_height - height;
      int last_top;

      last_child = Qnil;

      /* Find the last child.  We are taking space from lowest windows
	 first, so we iterate over children from the last child
	 backwards.  */
      for (child = w->vchild; !NILP (child); child = XWINDOW (child)->next)
	last_child = child;

      /* Assign new heights.  We leave only MIN_SAFE_WINDOW_HEIGHT.  */
      for (child = last_child; delta && !NILP (child); child = c->prev)
	{
	  int this_one;

	  c = XWINDOW (child);
	  this_one = XFASTINT (c->total_lines) - MIN_SAFE_WINDOW_HEIGHT;

	  if (this_one > delta)
	    this_one = delta;

	  shrink_window_lowest_first (c, XFASTINT (c->total_lines) - this_one);
	  delta -= this_one;
	}

      /* Compute new positions.  */
      last_top = XINT (w->top_line);
      for (child = w->vchild; !NILP (child); child = c->next)
	{
	  c = XWINDOW (child);
	  c->top_line = make_number (last_top);
	  shrink_window_lowest_first (c, XFASTINT (c->total_lines));
	  last_top += XFASTINT (c->total_lines);
	}
    }
}


/* Save, restore, or check positions and sizes in the window tree
   rooted at W.  ACTION says what to do.

   If ACTION is CHECK_ORIG_SIZES, check if orig_top_line and
   orig_total_lines members are valid for all windows in the window
   tree.  Value is non-zero if they are valid.

   If ACTION is SAVE_ORIG_SIZES, save members top and height in
   orig_top_line and orig_total_lines for all windows in the tree.

   If ACTION is RESTORE_ORIG_SIZES, restore top and height from values
   stored in orig_top_line and orig_total_lines for all windows.  */

static int
save_restore_orig_size (w, action)
     struct window *w;
     enum save_restore_action action;
{
  int success_p = 1;

  while (w)
    {
      if (!NILP (w->hchild))
	{
	  if (!save_restore_orig_size (XWINDOW (w->hchild), action))
	    success_p = 0;
	}
      else if (!NILP (w->vchild))
	{
	  if (!save_restore_orig_size (XWINDOW (w->vchild), action))
	    success_p = 0;
	}

      switch (action)
	{
	case CHECK_ORIG_SIZES:
	  if (!INTEGERP (w->orig_top_line) || !INTEGERP (w->orig_total_lines))
	    return 0;
	  break;

	case SAVE_ORIG_SIZES:
	  w->orig_top_line = w->top_line;
	  w->orig_total_lines = w->total_lines;
          XSETFASTINT (w->last_modified, 0);
          XSETFASTINT (w->last_overlay_modified, 0);
	  break;

	case RESTORE_ORIG_SIZES:
	  xassert (INTEGERP (w->orig_top_line) && INTEGERP (w->orig_total_lines));
	  w->top_line = w->orig_top_line;
	  w->total_lines = w->orig_total_lines;
	  w->orig_total_lines = w->orig_top_line = Qnil;
          XSETFASTINT (w->last_modified, 0);
          XSETFASTINT (w->last_overlay_modified, 0);
	  break;

	default:
	  abort ();
	}

      w = NILP (w->next) ? NULL : XWINDOW (w->next);
    }

  return success_p;
}


/* Grow mini-window W by DELTA lines, DELTA >= 0, or as much as we can
   without deleting other windows.  */

void
grow_mini_window (w, delta)
     struct window *w;
     int delta;
{
  struct frame *f = XFRAME (w->frame);
  struct window *root;

  xassert (MINI_WINDOW_P (w));
  xassert (delta >= 0);

  /* Check values of window_min_width and window_min_height for
     validity.  */
  check_min_window_sizes ();

  /* Compute how much we can enlarge the mini-window without deleting
     other windows.  */
  root = XWINDOW (FRAME_ROOT_WINDOW (f));
  if (delta)
    {
      int min_height = window_min_size (root, 0, 0, 0);
      if (XFASTINT (root->total_lines) - delta < min_height)
	/* Note that the root window may already be smaller than
	   min_height.  */
	delta = max (0, XFASTINT (root->total_lines) - min_height);
    }

  if (delta)
    {
      /* Save original window sizes and positions, if not already done.  */
      if (!save_restore_orig_size (root, CHECK_ORIG_SIZES))
	save_restore_orig_size (root, SAVE_ORIG_SIZES);

      /* Shrink other windows.  */
      shrink_window_lowest_first (root, XFASTINT (root->total_lines) - delta);

      /* Grow the mini-window.  */
      w->top_line = make_number (XFASTINT (root->top_line) + XFASTINT (root->total_lines));
      w->total_lines = make_number (XFASTINT (w->total_lines) + delta);
      XSETFASTINT (w->last_modified, 0);
      XSETFASTINT (w->last_overlay_modified, 0);

      adjust_glyphs (f);
    }
}


/* Shrink mini-window W.  If there is recorded info about window sizes
   before a call to grow_mini_window, restore recorded window sizes.
   Otherwise, if the mini-window is higher than 1 line, resize it to 1
   line.  */

void
shrink_mini_window (w)
     struct window *w;
{
  struct frame *f = XFRAME (w->frame);
  struct window *root = XWINDOW (FRAME_ROOT_WINDOW (f));

  if (save_restore_orig_size (root, CHECK_ORIG_SIZES))
    {
      save_restore_orig_size (root, RESTORE_ORIG_SIZES);
      adjust_glyphs (f);
      FRAME_WINDOW_SIZES_CHANGED (f) = 1;
      windows_or_buffers_changed = 1;
    }
  else if (XFASTINT (w->total_lines) > 1)
    {
      /* Distribute the additional lines of the mini-window
	 among the other windows.  */
      Lisp_Object window;
      XSETWINDOW (window, w);
      enlarge_window (window, 1 - XFASTINT (w->total_lines), 0);
    }
}



/* Mark window cursors off for all windows in the window tree rooted
   at W by setting their phys_cursor_on_p flag to zero.  Called from
   xterm.c, e.g. when a frame is cleared and thereby all cursors on
   the frame are cleared.  */

void
mark_window_cursors_off (w)
     struct window *w;
{
  while (w)
    {
      if (!NILP (w->hchild))
	mark_window_cursors_off (XWINDOW (w->hchild));
      else if (!NILP (w->vchild))
	mark_window_cursors_off (XWINDOW (w->vchild));
      else
	w->phys_cursor_on_p = 0;

      w = NILP (w->next) ? 0 : XWINDOW (w->next);
    }
}


/* Return number of lines of text (not counting mode lines) in W.  */

int
window_internal_height (w)
     struct window *w;
{
  int ht = XFASTINT (w->total_lines);

  if (!MINI_WINDOW_P (w))
    {
      if (!NILP (w->parent)
	  || !NILP (w->vchild)
	  || !NILP (w->hchild)
	  || !NILP (w->next)
	  || !NILP (w->prev)
	  || WINDOW_WANTS_MODELINE_P (w))
	--ht;

      if (WINDOW_WANTS_HEADER_LINE_P (w))
	--ht;
    }

  return ht;
}


/* Return the number of columns in W.
   Don't count columns occupied by scroll bars or the vertical bar
   separating W from the sibling to its right.  */

int
window_box_text_cols (w)
     struct window *w;
{
  struct frame *f = XFRAME (WINDOW_FRAME (w));
  int width = XINT (w->total_cols);

  if (WINDOW_HAS_VERTICAL_SCROLL_BAR (w))
    /* Scroll bars occupy a few columns.  */
    width -= WINDOW_CONFIG_SCROLL_BAR_COLS (w);
  else if (!FRAME_WINDOW_P (f)
	   && !WINDOW_RIGHTMOST_P (w) && !WINDOW_FULL_WIDTH_P (w))
    /* The column of `|' characters separating side-by-side windows
       occupies one column only.  */
    width -= 1;

  if (FRAME_WINDOW_P (f))
    /* On window-systems, fringes and display margins cannot be
       used for normal text.  */
    width -= (WINDOW_FRINGE_COLS (w)
	      + WINDOW_LEFT_MARGIN_COLS (w)
	      + WINDOW_RIGHT_MARGIN_COLS (w));

  return width;
}


/************************************************************************
			   Window Scrolling
 ***********************************************************************/

/* Scroll contents of window WINDOW up.  If WHOLE is non-zero, scroll
   N screen-fulls, which is defined as the height of the window minus
   next_screen_context_lines.  If WHOLE is zero, scroll up N lines
   instead.  Negative values of N mean scroll down.  NOERROR non-zero
   means don't signal an error if we try to move over BEGV or ZV,
   respectively.  */

static void
window_scroll (window, n, whole, noerror)
     Lisp_Object window;
     int n;
     int whole;
     int noerror;
{
  immediate_quit = 1;

  /* If we must, use the pixel-based version which is much slower than
     the line-based one but can handle varying line heights.  */
  if (FRAME_WINDOW_P (XFRAME (XWINDOW (window)->frame)))
    window_scroll_pixel_based (window, n, whole, noerror);
  else
    window_scroll_line_based (window, n, whole, noerror);

  immediate_quit = 0;
}


/* Implementation of window_scroll that works based on pixel line
   heights.  See the comment of window_scroll for parameter
   descriptions.  */

static void
window_scroll_pixel_based (window, n, whole, noerror)
     Lisp_Object window;
     int n;
     int whole;
     int noerror;
{
  struct it it;
  struct window *w = XWINDOW (window);
  struct text_pos start;
  int this_scroll_margin;
  /* True if we fiddled the window vscroll field without really scrolling.   */
  int vscrolled = 0;
  int x, y, rtop, rbot, rowh, vpos;

  SET_TEXT_POS_FROM_MARKER (start, w->start);

  /* If PT is not visible in WINDOW, move back one half of
     the screen.  Allow PT to be partially visible, otherwise
     something like (scroll-down 1) with PT in the line before
     the partially visible one would recenter. */

  if (!pos_visible_p (w, PT, &x, &y, &rtop, &rbot, &rowh, &vpos))
    {
      /* Move backward half the height of the window.  Performance note:
	 vmotion used here is about 10% faster, but would give wrong
	 results for variable height lines.  */
      init_iterator (&it, w, PT, PT_BYTE, NULL, DEFAULT_FACE_ID);
      it.current_y = it.last_visible_y;
      move_it_vertically_backward (&it, window_box_height (w) / 2);

      /* The function move_iterator_vertically may move over more than
	 the specified y-distance.  If it->w is small, e.g. a
	 mini-buffer window, we may end up in front of the window's
	 display area.  This is the case when Start displaying at the
	 start of the line containing PT in this case.  */
      if (it.current_y <= 0)
	{
	  init_iterator (&it, w, PT, PT_BYTE, NULL, DEFAULT_FACE_ID);
	  move_it_vertically_backward (&it, 0);
	  it.current_y = 0;
	}

      start = it.current.pos;
    }
  else if (auto_window_vscroll_p)
    {
      if (rtop || rbot)		/* partially visible */
	{
	  int px;
	  int dy = WINDOW_FRAME_LINE_HEIGHT (w);
	  if (whole)
	    dy = max ((window_box_height (w)
		       - next_screen_context_lines * dy),
		      dy);
	  dy *= n;

	  if (n < 0)
	    {
	      /* Only vscroll backwards if already vscrolled forwards.  */
	      if (w->vscroll < 0 && rtop > 0)
		{
		  px = max (0, -w->vscroll - min (rtop, -dy));
		  Fset_window_vscroll (window, make_number (px), Qt);
		  return;
		}
	    }
	  if (n > 0)
	    {
	      /* Do vscroll if already vscrolled or only display line.  */
	      if (rbot > 0 && (w->vscroll < 0 || vpos == 0))
		{
		  px = max (0, -w->vscroll + min (rbot, dy));
		  Fset_window_vscroll (window, make_number (px), Qt);
		  return;
		}

	      /* Maybe modify window start instead of scrolling.  */
	      if (rbot > 0 || w->vscroll < 0)
		{
		  int spos;

		  Fset_window_vscroll (window, make_number (0), Qt);
		  /* If there are other text lines above the current row,
		     move window start to current row.  Else to next row. */
		  if (rbot > 0)
		    spos = XINT (Fline_beginning_position (Qnil));
		  else
		    spos = min (XINT (Fline_end_position (Qnil)) + 1, ZV);
		  set_marker_restricted (w->start, make_number (spos),
					 w->buffer);
		  w->start_at_line_beg = Qt;
		  w->update_mode_line = Qt;
		  XSETFASTINT (w->last_modified, 0);
		  XSETFASTINT (w->last_overlay_modified, 0);
		  /* Set force_start so that redisplay_window will run the
		     window-scroll-functions.  */
		  w->force_start = Qt;
		  return;
		}
	    }
	}
      /* Cancel previous vscroll.  */
      Fset_window_vscroll (window, make_number (0), Qt);
    }

  /* If scroll_preserve_screen_position is non-nil, we try to set
     point in the same window line as it is now, so get that line.  */
  if (!NILP (Vscroll_preserve_screen_position))
    {
      /* We preserve the goal pixel coordinate across consecutive
	 calls to scroll-up or scroll-down.  This avoids the
	 possibility of point becoming "stuck" on a tall line when
	 scrolling by one line.  */
      if (window_scroll_pixel_based_preserve_y < 0
	  || (!EQ (current_kboard->Vlast_command, Qscroll_up)
	      && !EQ (current_kboard->Vlast_command, Qscroll_down)))
	{
	  start_display (&it, w, start);
	  move_it_to (&it, PT, -1, -1, -1, MOVE_TO_POS);
	  window_scroll_pixel_based_preserve_y = it.current_y;
	}
    }
  else
    window_scroll_pixel_based_preserve_y = -1;

  /* Move iterator it from start the specified distance forward or
     backward.  The result is the new window start.  */
  start_display (&it, w, start);
  if (whole)
    {
      int start_pos = IT_CHARPOS (it);
      int dy = WINDOW_FRAME_LINE_HEIGHT (w);
      dy = max ((window_box_height (w)
		 - next_screen_context_lines * dy),
		dy) * n;

      /* Note that move_it_vertically always moves the iterator to the
         start of a line.  So, if the last line doesn't have a newline,
	 we would end up at the start of the line ending at ZV.  */
      if (dy <= 0)
	{
	  move_it_vertically_backward (&it, -dy);
	  /* Ensure we actually do move, e.g. in case we are currently
	     looking at an image that is taller that the window height.  */
	  while (start_pos == IT_CHARPOS (it)
		 && start_pos > BEGV)
	    move_it_by_lines (&it, -1, 1);
	}
      else if (dy > 0)
	{
	  move_it_to (&it, ZV, -1, it.current_y + dy, -1,
		      MOVE_TO_POS | MOVE_TO_Y);
	  /* Ensure we actually do move, e.g. in case we are currently
	     looking at an image that is taller that the window height.  */
	  while (start_pos == IT_CHARPOS (it)
		 && start_pos < ZV)
	    move_it_by_lines (&it, 1, 1);
	}
    }
  else
    move_it_by_lines (&it, n, 1);

  /* We failed if we find ZV is already on the screen (scrolling up,
     means there's nothing past the end), or if we can't start any
     earlier (scrolling down, means there's nothing past the top).  */
  if ((n > 0 && IT_CHARPOS (it) == ZV)
      || (n < 0 && IT_CHARPOS (it) == CHARPOS (start)))
    {
      if (IT_CHARPOS (it) == ZV)
	{
	  if (it.current_y < it.last_visible_y
	      && (it.current_y + it.max_ascent + it.max_descent
		  > it.last_visible_y))
	    {
	      /* The last line was only partially visible, make it fully
		 visible.  */
	      w->vscroll = (it.last_visible_y
			    - it.current_y + it.max_ascent + it.max_descent);
	      adjust_glyphs (it.f);
	    }
	  else if (noerror)
	    return;
	  else if (n < 0)	/* could happen with empty buffers */
	    xsignal0 (Qbeginning_of_buffer);
	  else
	    xsignal0 (Qend_of_buffer);
	}
      else
	{
	  if (w->vscroll != 0)
	    /* The first line was only partially visible, make it fully
	       visible. */
	    w->vscroll = 0;
	  else if (noerror)
	    return;
	  else
	    xsignal0 (Qbeginning_of_buffer);
	}

      /* If control gets here, then we vscrolled.  */

      XBUFFER (w->buffer)->prevent_redisplay_optimizations_p = 1;

      /* Don't try to change the window start below.  */
      vscrolled = 1;
    }

  if (! vscrolled)
    {
      int pos = IT_CHARPOS (it);
      int bytepos;

      /* If in the middle of a multi-glyph character move forward to
	 the next character.  */
      if (in_display_vector_p (&it))
	{
	  ++pos;
	  move_it_to (&it, pos, -1, -1, -1, MOVE_TO_POS);
	}

      /* Set the window start, and set up the window for redisplay.  */
      set_marker_restricted (w->start, make_number (pos),
			     w->buffer);
      bytepos = XMARKER (w->start)->bytepos;
      w->start_at_line_beg = ((pos == BEGV || FETCH_BYTE (bytepos - 1) == '\n')
			      ? Qt : Qnil);
      w->update_mode_line = Qt;
      XSETFASTINT (w->last_modified, 0);
      XSETFASTINT (w->last_overlay_modified, 0);
      /* Set force_start so that redisplay_window will run the
	 window-scroll-functions.  */
      w->force_start = Qt;
    }

  /* The rest of this function uses current_y in a nonstandard way,
     not including the height of the header line if any.  */
  it.current_y = it.vpos = 0;

  /* Move PT out of scroll margins.
     This code wants current_y to be zero at the window start position
     even if there is a header line.  */
  this_scroll_margin = max (0, scroll_margin);
  this_scroll_margin = min (this_scroll_margin, XFASTINT (w->total_lines) / 4);
  this_scroll_margin *= FRAME_LINE_HEIGHT (it.f);

  if (n > 0)
    {
      /* We moved the window start towards ZV, so PT may be now
	 in the scroll margin at the top.  */
      move_it_to (&it, PT, -1, -1, -1, MOVE_TO_POS);
      if (IT_CHARPOS (it) == PT && it.current_y >= this_scroll_margin
          && (NILP (Vscroll_preserve_screen_position)
	      || EQ (Vscroll_preserve_screen_position, Qt)))
	/* We found PT at a legitimate height.  Leave it alone.  */
	;
      else if (window_scroll_pixel_based_preserve_y >= 0)
	{
	  /* If we have a header line, take account of it.
	     This is necessary because we set it.current_y to 0, above.  */
	  move_it_to (&it, -1, -1,
		      window_scroll_pixel_based_preserve_y
		      - (WINDOW_WANTS_HEADER_LINE_P (w) ? 1 : 0 ),
		      -1, MOVE_TO_Y);
	  SET_PT_BOTH (IT_CHARPOS (it), IT_BYTEPOS (it));
	}
      else
	{
	  while (it.current_y < this_scroll_margin)
	    {
	      int prev = it.current_y;
	      move_it_by_lines (&it, 1, 1);
	      if (prev == it.current_y)
		break;
	    }
	  SET_PT_BOTH (IT_CHARPOS (it), IT_BYTEPOS (it));
	}
    }
  else if (n < 0)
    {
      int charpos, bytepos;
      int partial_p;

      /* Save our position, for the
	 window_scroll_pixel_based_preserve_y case.  */
      charpos = IT_CHARPOS (it);
      bytepos = IT_BYTEPOS (it);

      /* We moved the window start towards BEGV, so PT may be now
	 in the scroll margin at the bottom.  */
      move_it_to (&it, PT, -1,
		  (it.last_visible_y - CURRENT_HEADER_LINE_HEIGHT (w)
		   - this_scroll_margin - 1),
		  -1,
		  MOVE_TO_POS | MOVE_TO_Y);

      /* Save our position, in case it's correct.  */
      charpos = IT_CHARPOS (it);
      bytepos = IT_BYTEPOS (it);

      /* See if point is on a partially visible line at the end.  */
      if (it.what == IT_EOB)
	partial_p = it.current_y + it.ascent + it.descent > it.last_visible_y;
      else
	{
	  move_it_by_lines (&it, 1, 1);
	  partial_p = it.current_y > it.last_visible_y;
	}

      if (charpos == PT && !partial_p
          && (NILP (Vscroll_preserve_screen_position)
	      || EQ (Vscroll_preserve_screen_position, Qt)))
	/* We found PT before we found the display margin, so PT is ok.  */
	;
      else if (window_scroll_pixel_based_preserve_y >= 0)
	{
	  SET_TEXT_POS_FROM_MARKER (start, w->start);
	  start_display (&it, w, start);
	  /* It would be wrong to subtract CURRENT_HEADER_LINE_HEIGHT
	     here because we called start_display again and did not
	     alter it.current_y this time.  */
	  move_it_to (&it, -1, -1, window_scroll_pixel_based_preserve_y, -1,
		      MOVE_TO_Y);
	  SET_PT_BOTH (IT_CHARPOS (it), IT_BYTEPOS (it));
	}
      else
	{
	  if (partial_p)
	    /* The last line was only partially visible, so back up two
	       lines to make sure we're on a fully visible line.  */
	    {
	      move_it_by_lines (&it, -2, 0);
	      SET_PT_BOTH (IT_CHARPOS (it), IT_BYTEPOS (it));
	    }
	  else
	    /* No, the position we saved is OK, so use it.  */
	    SET_PT_BOTH (charpos, bytepos);
	}
    }
}


/* Implementation of window_scroll that works based on screen lines.
   See the comment of window_scroll for parameter descriptions.  */

static void
window_scroll_line_based (window, n, whole, noerror)
     Lisp_Object window;
     int n;
     int whole;
     int noerror;
{
  register struct window *w = XWINDOW (window);
  register int opoint = PT, opoint_byte = PT_BYTE;
  register int pos, pos_byte;
  register int ht = window_internal_height (w);
  register Lisp_Object tem;
  int lose;
  Lisp_Object bolp;
  int startpos;
  struct position posit;
  int original_vpos;

  /* If scrolling screen-fulls, compute the number of lines to
     scroll from the window's height.  */
  if (whole)
    n *= max (1, ht - next_screen_context_lines);

  startpos = marker_position (w->start);

  posit = *compute_motion (startpos, 0, 0, 0,
			   PT, ht, 0,
			   -1, XINT (w->hscroll),
			   0, w);
  original_vpos = posit.vpos;

  XSETFASTINT (tem, PT);
  tem = Fpos_visible_in_window_p (tem, window, Qnil);

  if (NILP (tem))
    {
      Fvertical_motion (make_number (- (ht / 2)), window);
      startpos = PT;
    }

  SET_PT (startpos);
  lose = n < 0 && PT == BEGV;
  Fvertical_motion (make_number (n), window);
  pos = PT;
  pos_byte = PT_BYTE;
  bolp = Fbolp ();
  SET_PT_BOTH (opoint, opoint_byte);

  if (lose)
    {
      if (noerror)
	return;
      else
	xsignal0 (Qbeginning_of_buffer);
    }

  if (pos < ZV)
    {
      int this_scroll_margin = scroll_margin;

      /* Don't use a scroll margin that is negative or too large.  */
      if (this_scroll_margin < 0)
	this_scroll_margin = 0;

      if (XINT (w->total_lines) < 4 * scroll_margin)
	this_scroll_margin = XINT (w->total_lines) / 4;

      set_marker_restricted_both (w->start, w->buffer, pos, pos_byte);
      w->start_at_line_beg = bolp;
      w->update_mode_line = Qt;
      XSETFASTINT (w->last_modified, 0);
      XSETFASTINT (w->last_overlay_modified, 0);
      /* Set force_start so that redisplay_window will run
	 the window-scroll-functions.  */
      w->force_start = Qt;

      if (!NILP (Vscroll_preserve_screen_position)
	  && (whole || !EQ (Vscroll_preserve_screen_position, Qt)))
	{
	  SET_PT_BOTH (pos, pos_byte);
	  Fvertical_motion (make_number (original_vpos), window);
	}
      /* If we scrolled forward, put point enough lines down
	 that it is outside the scroll margin.  */
      else if (n > 0)
	{
	  int top_margin;

	  if (this_scroll_margin > 0)
	    {
	      SET_PT_BOTH (pos, pos_byte);
	      Fvertical_motion (make_number (this_scroll_margin), window);
	      top_margin = PT;
	    }
	  else
	    top_margin = pos;

	  if (top_margin <= opoint)
	    SET_PT_BOTH (opoint, opoint_byte);
	  else if (!NILP (Vscroll_preserve_screen_position))
	    {
	      SET_PT_BOTH (pos, pos_byte);
	      Fvertical_motion (make_number (original_vpos), window);
	    }
	  else
	    SET_PT (top_margin);
	}
      else if (n < 0)
	{
	  int bottom_margin;

	  /* If we scrolled backward, put point near the end of the window
	     but not within the scroll margin.  */
	  SET_PT_BOTH (pos, pos_byte);
	  tem = Fvertical_motion (make_number (ht - this_scroll_margin), window);
	  if (XFASTINT (tem) == ht - this_scroll_margin)
	    bottom_margin = PT;
	  else
	    bottom_margin = PT + 1;

	  if (bottom_margin > opoint)
	    SET_PT_BOTH (opoint, opoint_byte);
	  else
	    {
	      if (!NILP (Vscroll_preserve_screen_position))
		{
		  SET_PT_BOTH (pos, pos_byte);
		  Fvertical_motion (make_number (original_vpos), window);
		}
	      else
		Fvertical_motion (make_number (-1), window);
	    }
	}
    }
  else
    {
      if (noerror)
	return;
      else
	xsignal0 (Qend_of_buffer);
    }
}


/* Scroll selected_window up or down.  If N is nil, scroll a
   screen-full which is defined as the height of the window minus
   next_screen_context_lines.  If N is the symbol `-', scroll.
   DIRECTION may be 1 meaning to scroll down, or -1 meaning to scroll
   up.  This is the guts of Fscroll_up and Fscroll_down.  */

static void
scroll_command (n, direction)
     Lisp_Object n;
     int direction;
{
  int count = SPECPDL_INDEX ();

  xassert (abs (direction) == 1);

  /* If selected window's buffer isn't current, make it current for
     the moment.  But don't screw up if window_scroll gets an error.  */
  if (XBUFFER (XWINDOW (selected_window)->buffer) != current_buffer)
    {
      record_unwind_protect (save_excursion_restore, save_excursion_save ());
      Fset_buffer (XWINDOW (selected_window)->buffer);

      /* Make redisplay consider other windows than just selected_window.  */
      ++windows_or_buffers_changed;
    }

  if (NILP (n))
    window_scroll (selected_window, direction, 1, 0);
  else if (EQ (n, Qminus))
    window_scroll (selected_window, -direction, 1, 0);
  else
    {
      n = Fprefix_numeric_value (n);
      window_scroll (selected_window, XINT (n) * direction, 0, 0);
    }

  unbind_to (count, Qnil);
}

DEFUN ("scroll-up", Fscroll_up, Sscroll_up, 0, 1, "P",
       doc: /* Scroll text of current window upward ARG lines.
If ARG is omitted or nil, scroll upward by a near full screen.
A near full screen is `next-screen-context-lines' less than a full screen.
Negative ARG means scroll downward.
If ARG is the atom `-', scroll downward by nearly full screen.
When calling from a program, supply as argument a number, nil, or `-'.  */)
     (arg)
     Lisp_Object arg;
{
  scroll_command (arg, 1);
  return Qnil;
}

DEFUN ("scroll-down", Fscroll_down, Sscroll_down, 0, 1, "P",
       doc: /* Scroll text of current window down ARG lines.
If ARG is omitted or nil, scroll down by a near full screen.
A near full screen is `next-screen-context-lines' less than a full screen.
Negative ARG means scroll upward.
If ARG is the atom `-', scroll upward by nearly full screen.
When calling from a program, supply as argument a number, nil, or `-'.  */)
     (arg)
     Lisp_Object arg;
{
  scroll_command (arg, -1);
  return Qnil;
}

DEFUN ("other-window-for-scrolling", Fother_window_for_scrolling, Sother_window_for_scrolling, 0, 0, 0,
       doc: /* Return the other window for \"other window scroll\" commands.
If `other-window-scroll-buffer' is non-nil, a window
showing that buffer is used.
If in the minibuffer, `minibuffer-scroll-window' if non-nil
specifies the window.  This takes precedence over
`other-window-scroll-buffer'.  */)
     ()
{
  Lisp_Object window;

  if (MINI_WINDOW_P (XWINDOW (selected_window))
      && !NILP (Vminibuf_scroll_window))
    window = Vminibuf_scroll_window;
  /* If buffer is specified, scroll that buffer.  */
  else if (!NILP (Vother_window_scroll_buffer))
    {
      window = Fget_buffer_window (Vother_window_scroll_buffer, Qnil);
      if (NILP (window))
	window = Fdisplay_buffer (Vother_window_scroll_buffer, Qt, Qnil);
    }
  else
    {
      /* Nothing specified; look for a neighboring window on the same
	 frame.  */
      window = Fnext_window (selected_window, Qnil, Qnil);

      if (EQ (window, selected_window))
	/* That didn't get us anywhere; look for a window on another
           visible frame.  */
	do
	  window = Fnext_window (window, Qnil, Qt);
	while (! FRAME_VISIBLE_P (XFRAME (WINDOW_FRAME (XWINDOW (window))))
	       && ! EQ (window, selected_window));
    }

  CHECK_LIVE_WINDOW (window);

  if (EQ (window, selected_window))
    error ("There is no other window");

  return window;
}

DEFUN ("scroll-other-window", Fscroll_other_window, Sscroll_other_window, 0, 1, "P",
       doc: /* Scroll next window upward ARG lines; or near full screen if no ARG.
A near full screen is `next-screen-context-lines' less than a full screen.
The next window is the one below the current one; or the one at the top
if the current one is at the bottom.  Negative ARG means scroll downward.
If ARG is the atom `-', scroll downward by nearly full screen.
When calling from a program, supply as argument a number, nil, or `-'.

If `other-window-scroll-buffer' is non-nil, scroll the window
showing that buffer, popping the buffer up if necessary.
If in the minibuffer, `minibuffer-scroll-window' if non-nil
specifies the window to scroll.  This takes precedence over
`other-window-scroll-buffer'.  */)
     (arg)
     Lisp_Object arg;
{
  Lisp_Object window;
  struct window *w;
  int count = SPECPDL_INDEX ();

  window = Fother_window_for_scrolling ();
  w = XWINDOW (window);

  /* Don't screw up if window_scroll gets an error.  */
  record_unwind_protect (save_excursion_restore, save_excursion_save ());
  ++windows_or_buffers_changed;

  Fset_buffer (w->buffer);
  SET_PT (marker_position (w->pointm));

  if (NILP (arg))
    window_scroll (window, 1, 1, 1);
  else if (EQ (arg, Qminus))
    window_scroll (window, -1, 1, 1);
  else
    {
      if (CONSP (arg))
	arg = Fcar (arg);
      CHECK_NUMBER (arg);
      window_scroll (window, XINT (arg), 0, 1);
    }

  set_marker_both (w->pointm, Qnil, PT, PT_BYTE);
  unbind_to (count, Qnil);

  return Qnil;
}

DEFUN ("scroll-left", Fscroll_left, Sscroll_left, 0, 2, "P\np",
       doc: /* Scroll selected window display ARG columns left.
Default for ARG is window width minus 2.
Value is the total amount of leftward horizontal scrolling in
effect after the change.
If SET_MINIMUM is non-nil, the new scroll amount becomes the
lower bound for automatic scrolling, i.e. automatic scrolling
will not scroll a window to a column less than the value returned
by this function.  This happens in an interactive call.  */)
     (arg, set_minimum)
     register Lisp_Object arg, set_minimum;
{
  Lisp_Object result;
  int hscroll;
  struct window *w = XWINDOW (selected_window);

  if (NILP (arg))
    XSETFASTINT (arg, window_box_text_cols (w) - 2);
  else
    arg = Fprefix_numeric_value (arg);

  hscroll = XINT (w->hscroll) + XINT (arg);
  result = Fset_window_hscroll (selected_window, make_number (hscroll));

  if (!NILP (set_minimum))
    w->min_hscroll = w->hscroll;

  return result;
}

DEFUN ("scroll-right", Fscroll_right, Sscroll_right, 0, 2, "P\np",
       doc: /* Scroll selected window display ARG columns right.
Default for ARG is window width minus 2.
Value is the total amount of leftward horizontal scrolling in
effect after the change.
If SET_MINIMUM is non-nil, the new scroll amount becomes the
lower bound for automatic scrolling, i.e. automatic scrolling
will not scroll a window to a column less than the value returned
by this function.  This happens in an interactive call.  */)
     (arg, set_minimum)
     register Lisp_Object arg, set_minimum;
{
  Lisp_Object result;
  int hscroll;
  struct window *w = XWINDOW (selected_window);

  if (NILP (arg))
    XSETFASTINT (arg, window_box_text_cols (w) - 2);
  else
    arg = Fprefix_numeric_value (arg);

  hscroll = XINT (w->hscroll) - XINT (arg);
  result = Fset_window_hscroll (selected_window, make_number (hscroll));

  if (!NILP (set_minimum))
    w->min_hscroll = w->hscroll;

  return result;
}

DEFUN ("minibuffer-selected-window", Fminibuffer_selected_window, Sminibuffer_selected_window, 0, 0, 0,
       doc: /* Return the window which was selected when entering the minibuffer.
Returns nil, if current window is not a minibuffer window.  */)
     ()
{
  if (minibuf_level > 0
      && MINI_WINDOW_P (XWINDOW (selected_window))
      && WINDOW_LIVE_P (minibuf_selected_window))
    return minibuf_selected_window;

  return Qnil;
}

/* Value is the number of lines actually displayed in window W,
   as opposed to its height.  */

static int
displayed_window_lines (w)
     struct window *w;
{
  struct it it;
  struct text_pos start;
  int height = window_box_height (w);
  struct buffer *old_buffer;
  int bottom_y;

  if (XBUFFER (w->buffer) != current_buffer)
    {
      old_buffer = current_buffer;
      set_buffer_internal (XBUFFER (w->buffer));
    }
  else
    old_buffer = NULL;

  /* In case W->start is out of the accessible range, do something
     reasonable.  This happens in Info mode when Info-scroll-down
     calls (recenter -1) while W->start is 1.  */
  if (XMARKER (w->start)->charpos < BEGV)
    SET_TEXT_POS (start, BEGV, BEGV_BYTE);
  else if (XMARKER (w->start)->charpos > ZV)
    SET_TEXT_POS (start, ZV, ZV_BYTE);
  else
    SET_TEXT_POS_FROM_MARKER (start, w->start);

  start_display (&it, w, start);
  move_it_vertically (&it, height);
  bottom_y = line_bottom_y (&it);

  /* rms: On a non-window display,
     the value of it.vpos at the bottom of the screen
     seems to be 1 larger than window_box_height (w).
     This kludge fixes a bug whereby (move-to-window-line -1)
     when ZV is on the last screen line
     moves to the previous screen line instead of the last one.  */
  if (! FRAME_WINDOW_P (XFRAME (w->frame)))
    height++;

  /* Add in empty lines at the bottom of the window.  */
  if (bottom_y < height)
    {
      int uy = FRAME_LINE_HEIGHT (it.f);
      it.vpos += (height - bottom_y + uy - 1) / uy;
    }

  if (old_buffer)
    set_buffer_internal (old_buffer);

  return it.vpos;
}


DEFUN ("recenter", Frecenter, Srecenter, 0, 1, "P",
       doc: /* Center point in window and redisplay frame.
With prefix argument ARG, recenter putting point on screen line ARG
relative to the current window.  If ARG is negative, it counts up from the
bottom of the window.  (ARG should be less than the height of the window.)

If ARG is omitted or nil, erase the entire frame and then redraw with point
in the center of the current window.  If `auto-resize-tool-bars' is set to
`grow-only', this resets the tool-bar's height to the minimum height needed.

Just C-u as prefix means put point in the center of the window
and redisplay normally--don't erase and redraw the frame.  */)
     (arg)
     register Lisp_Object arg;
{
  struct window *w = XWINDOW (selected_window);
  struct buffer *buf = XBUFFER (w->buffer);
  struct buffer *obuf = current_buffer;
  int center_p = 0;
  int charpos, bytepos;
  int iarg;
  int this_scroll_margin;

  /* If redisplay is suppressed due to an error, try again.  */
  obuf->display_error_modiff = 0;

  if (NILP (arg))
    {
      int i;

      /* Invalidate pixel data calculated for all compositions.  */
      for (i = 0; i < n_compositions; i++)
	composition_table[i]->font = NULL;

      WINDOW_XFRAME (w)->minimize_tool_bar_window_p = 1;

      Fredraw_frame (WINDOW_FRAME (w));
      SET_FRAME_GARBAGED (WINDOW_XFRAME (w));
      center_p = 1;
    }
  else if (CONSP (arg)) /* Just C-u. */
    center_p = 1;
  else
    {
      arg = Fprefix_numeric_value (arg);
      CHECK_NUMBER (arg);
      iarg = XINT (arg);
    }

  set_buffer_internal (buf);

  /* Do this after making BUF current
     in case scroll_margin is buffer-local.  */
  this_scroll_margin = max (0, scroll_margin);
  this_scroll_margin = min (this_scroll_margin,
			    XFASTINT (w->total_lines) / 4);

  /* Handle centering on a graphical frame specially.  Such frames can
     have variable-height lines and centering point on the basis of
     line counts would lead to strange effects.  */
  if (FRAME_WINDOW_P (XFRAME (w->frame)))
    {
      if (center_p)
	{
	  struct it it;
	  struct text_pos pt;

	  SET_TEXT_POS (pt, PT, PT_BYTE);
	  start_display (&it, w, pt);
	  move_it_vertically_backward (&it, window_box_height (w) / 2);
	  charpos = IT_CHARPOS (it);
	  bytepos = IT_BYTEPOS (it);
	}
      else if (iarg < 0)
	{
	  struct it it;
	  struct text_pos pt;
	  int nlines = -iarg;
	  int extra_line_spacing;
	  int h = window_box_height (w);

	  iarg = - max (-iarg, this_scroll_margin);

	  SET_TEXT_POS (pt, PT, PT_BYTE);
	  start_display (&it, w, pt);

	  /* Be sure we have the exact height of the full line containing PT.  */
	  move_it_by_lines (&it, 0, 1);

	  /* The amount of pixels we have to move back is the window
	     height minus what's displayed in the line containing PT,
	     and the lines below.  */
	  it.current_y = 0;
	  it.vpos = 0;
	  move_it_by_lines (&it, nlines, 1);

	  if (it.vpos == nlines)
	    h -= it.current_y;
	  else
	    {
	      /* Last line has no newline */
	      h -= line_bottom_y (&it);
	      it.vpos++;
	    }

	  /* Don't reserve space for extra line spacing of last line.  */
	  extra_line_spacing = it.max_extra_line_spacing;

	  /* If we can't move down NLINES lines because we hit
	     the end of the buffer, count in some empty lines.  */
	  if (it.vpos < nlines)
	    {
	      nlines -= it.vpos;
	      extra_line_spacing = it.extra_line_spacing;
	      h -= nlines * (FRAME_LINE_HEIGHT (it.f) + extra_line_spacing);
	    }
	  if (h <= 0)
	    return Qnil;

	  /* Now find the new top line (starting position) of the window.  */
	  start_display (&it, w, pt);
	  it.current_y = 0;
	  move_it_vertically_backward (&it, h);

	  /* If extra line spacing is present, we may move too far
	     back.  This causes the last line to be only partially
	     visible (which triggers redisplay to recenter that line
	     in the middle), so move forward.
	     But ignore extra line spacing on last line, as it is not
	     considered to be part of the visible height of the line.
	  */
	  h += extra_line_spacing;
	  while (-it.current_y > h)
	    move_it_by_lines (&it, 1, 1);

	  charpos = IT_CHARPOS (it);
	  bytepos = IT_BYTEPOS (it);
	}
      else
	{
	  struct position pos;

	  iarg = max (iarg, this_scroll_margin);

	  pos = *vmotion (PT, -iarg, w);
	  charpos = pos.bufpos;
	  bytepos = pos.bytepos;
	}
    }
  else
    {
      struct position pos;
      int ht = window_internal_height (w);

      if (center_p)
	iarg = ht / 2;
      else if (iarg < 0)
	iarg += ht;

      /* Don't let it get into the margin at either top or bottom.  */
      iarg = max (iarg, this_scroll_margin);
      iarg = min (iarg, ht - this_scroll_margin - 1);

      pos = *vmotion (PT, - iarg, w);
      charpos = pos.bufpos;
      bytepos = pos.bytepos;
    }

  /* Set the new window start.  */
  set_marker_both (w->start, w->buffer, charpos, bytepos);
  w->window_end_valid = Qnil;

  w->optional_new_start = Qt;

  if (bytepos == BEGV_BYTE || FETCH_BYTE (bytepos - 1) == '\n')
    w->start_at_line_beg = Qt;
  else
    w->start_at_line_beg = Qnil;

  set_buffer_internal (obuf);
  return Qnil;
}


DEFUN ("window-text-height", Fwindow_text_height, Swindow_text_height,
       0, 1, 0,
       doc: /* Return the height in lines of the text display area of WINDOW.
WINDOW defaults to the selected window.
This doesn't include the mode-line (or header-line if any) or any
partial-height lines in the text display area.  */)
     (window)
     Lisp_Object window;
{
  struct window *w = decode_window (window);
  int pixel_height = window_box_height (w);
  int line_height = pixel_height / FRAME_LINE_HEIGHT (XFRAME (w->frame));
  return make_number (line_height);
}



DEFUN ("move-to-window-line", Fmove_to_window_line, Smove_to_window_line,
       1, 1, "P",
       doc: /* Position point relative to window.
With no argument, position point at center of window.
An argument specifies vertical position within the window;
zero means top of window, negative means relative to bottom of window.  */)
     (arg)
     Lisp_Object arg;
{
  struct window *w = XWINDOW (selected_window);
  int lines, start;
  Lisp_Object window;
#if 0
  int this_scroll_margin;
#endif

  window = selected_window;
  start = marker_position (w->start);
  if (start < BEGV || start > ZV)
    {
      int height = window_internal_height (w);
      Fvertical_motion (make_number (- (height / 2)), window);
      set_marker_both (w->start, w->buffer, PT, PT_BYTE);
      w->start_at_line_beg = Fbolp ();
      w->force_start = Qt;
    }
  else
    Fgoto_char (w->start);

  lines = displayed_window_lines (w);

#if 0
  this_scroll_margin = max (0, scroll_margin);
  this_scroll_margin = min (this_scroll_margin, lines / 4);
#endif

  if (NILP (arg))
    XSETFASTINT (arg, lines / 2);
  else
    {
      int iarg = XINT (Fprefix_numeric_value (arg));

      if (iarg < 0)
	iarg = iarg + lines;

#if 0  /* This code would prevent move-to-window-line from moving point
	  to a place inside the scroll margins (which would cause the
	  next redisplay to scroll).  I wrote this code, but then concluded
	  it is probably better not to install it.  However, it is here
	  inside #if 0 so as not to lose it.  -- rms.  */

      /* Don't let it get into the margin at either top or bottom.  */
      iarg = max (iarg, this_scroll_margin);
      iarg = min (iarg, lines - this_scroll_margin - 1);
#endif

      arg = make_number (iarg);
    }

  /* Skip past a partially visible first line.  */
  if (w->vscroll)
    XSETINT (arg, XINT (arg) + 1);

  return Fvertical_motion (arg, window);
}



/***********************************************************************
			 Window Configuration
 ***********************************************************************/

struct save_window_data
  {
    EMACS_INT size_from_Lisp_Vector_struct;
    struct Lisp_Vector *next_from_Lisp_Vector_struct;
    Lisp_Object frame_cols, frame_lines, frame_menu_bar_lines;
    Lisp_Object frame_tool_bar_lines;
    Lisp_Object selected_frame;
    Lisp_Object current_window;
    Lisp_Object current_buffer;
    Lisp_Object minibuf_scroll_window;
    Lisp_Object minibuf_selected_window;
    Lisp_Object root_window;
    Lisp_Object focus_frame;
    /* Record the values of window-min-width and window-min-height
       so that window sizes remain consistent with them.  */
    Lisp_Object min_width, min_height;
    /* A vector, each of whose elements is a struct saved_window
       for one window.  */
    Lisp_Object saved_windows;
  };

/* This is saved as a Lisp_Vector  */
struct saved_window
{
  /* these first two must agree with struct Lisp_Vector in lisp.h */
  EMACS_INT size_from_Lisp_Vector_struct;
  struct Lisp_Vector *next_from_Lisp_Vector_struct;

  Lisp_Object window;
  Lisp_Object buffer, start, pointm, mark;
  Lisp_Object left_col, top_line, total_cols, total_lines;
  Lisp_Object hscroll, min_hscroll;
  Lisp_Object parent, prev;
  Lisp_Object start_at_line_beg;
  Lisp_Object display_table;
  Lisp_Object orig_top_line, orig_total_lines;
  Lisp_Object left_margin_cols, right_margin_cols;
  Lisp_Object left_fringe_width, right_fringe_width, fringes_outside_margins;
  Lisp_Object scroll_bar_width, vertical_scroll_bar_type;
  Lisp_Object dedicated;
};

#define SAVED_WINDOW_N(swv,n) \
  ((struct saved_window *) (XVECTOR ((swv)->contents[(n)])))

DEFUN ("window-configuration-p", Fwindow_configuration_p, Swindow_configuration_p, 1, 1, 0,
       doc: /* Return t if OBJECT is a window-configuration object.  */)
     (object)
     Lisp_Object object;
{
  return WINDOW_CONFIGURATIONP (object) ? Qt : Qnil;
}

DEFUN ("window-configuration-frame", Fwindow_configuration_frame, Swindow_configuration_frame, 1, 1, 0,
       doc: /* Return the frame that CONFIG, a window-configuration object, is about.  */)
     (config)
     Lisp_Object config;
{
  register struct save_window_data *data;
  struct Lisp_Vector *saved_windows;

  CHECK_WINDOW_CONFIGURATION (config);

  data = (struct save_window_data *) XVECTOR (config);
  saved_windows = XVECTOR (data->saved_windows);
  return XWINDOW (SAVED_WINDOW_N (saved_windows, 0)->window)->frame;
}

DEFUN ("set-window-configuration", Fset_window_configuration,
       Sset_window_configuration, 1, 1, 0,
       doc: /* Set the configuration of windows and buffers as specified by CONFIGURATION.
CONFIGURATION must be a value previously returned
by `current-window-configuration' (which see).
If CONFIGURATION was made from a frame that is now deleted,
only frame-independent values can be restored.  In this case,
the return value is nil.  Otherwise the value is t.  */)
     (configuration)
     Lisp_Object configuration;
{
  register struct save_window_data *data;
  struct Lisp_Vector *saved_windows;
  Lisp_Object new_current_buffer;
  Lisp_Object frame;
  FRAME_PTR f;
  int old_point = -1;

  CHECK_WINDOW_CONFIGURATION (configuration);

  data = (struct save_window_data *) XVECTOR (configuration);
  saved_windows = XVECTOR (data->saved_windows);

  new_current_buffer = data->current_buffer;
  if (NILP (XBUFFER (new_current_buffer)->name))
    new_current_buffer = Qnil;
  else
    {
      if (XBUFFER (new_current_buffer) == current_buffer)
	/* The code further down "preserves point" by saving here PT in
	   old_point and then setting it later back into PT.  When the
	   current-selected-window and the final-selected-window both show
	   the current buffer, this suffers from the problem that the
	   current PT is the window-point of the current-selected-window,
	   while the final PT is the point of the final-selected-window, so
	   this copy from one PT to the other would end up moving the
	   window-point of the final-selected-window to the window-point of
	   the current-selected-window.  So we have to be careful which
	   point of the current-buffer we copy into old_point.  */
	if (EQ (XWINDOW (data->current_window)->buffer, new_current_buffer)
	    && WINDOWP (selected_window)
	    && EQ (XWINDOW (selected_window)->buffer, new_current_buffer)
	    && !EQ (selected_window, data->current_window))
	  old_point = XMARKER (XWINDOW (data->current_window)->pointm)->charpos;
	else
	  old_point = PT;
      else
	/* BUF_PT (XBUFFER (new_current_buffer)) gives us the position of
	   point in new_current_buffer as of the last time this buffer was
	   used.  This can be non-deterministic since it can be changed by
	   things like jit-lock by mere temporary selection of some random
	   window that happens to show this buffer.
	   So if possible we want this arbitrary choice of "which point" to
	   be the one from the to-be-selected-window so as to prevent this
	   window's cursor from being copied from another window.  */
	if (EQ (XWINDOW (data->current_window)->buffer, new_current_buffer)
	    /* If current_window = selected_window, its point is in BUF_PT.  */
	    && !EQ (selected_window, data->current_window))
	  old_point = XMARKER (XWINDOW (data->current_window)->pointm)->charpos;
	else
	  old_point = BUF_PT (XBUFFER (new_current_buffer));
    }

  frame = XWINDOW (SAVED_WINDOW_N (saved_windows, 0)->window)->frame;
  f = XFRAME (frame);

  /* If f is a dead frame, don't bother rebuilding its window tree.
     However, there is other stuff we should still try to do below.  */
  if (FRAME_LIVE_P (f))
    {
      register struct window *w;
      register struct saved_window *p;
      struct window *root_window;
      struct window **leaf_windows;
      int n_leaf_windows;
      int k, i, n;

      /* If the frame has been resized since this window configuration was
	 made, we change the frame to the size specified in the
	 configuration, restore the configuration, and then resize it
	 back.  We keep track of the prevailing height in these variables.  */
      int previous_frame_lines = FRAME_LINES (f);
      int previous_frame_cols =  FRAME_COLS  (f);
      int previous_frame_menu_bar_lines = FRAME_MENU_BAR_LINES (f);
      int previous_frame_tool_bar_lines = FRAME_TOOL_BAR_LINES (f);

      /* The mouse highlighting code could get screwed up
	 if it runs during this.  */
      BLOCK_INPUT;

      if (XFASTINT (data->frame_lines) != previous_frame_lines
	  || XFASTINT (data->frame_cols) != previous_frame_cols)
	change_frame_size (f, XFASTINT (data->frame_lines),
			   XFASTINT (data->frame_cols), 0, 0, 0);
#if defined (HAVE_WINDOW_SYSTEM) || defined (MSDOS)
      if (XFASTINT (data->frame_menu_bar_lines)
	  != previous_frame_menu_bar_lines)
	x_set_menu_bar_lines (f, data->frame_menu_bar_lines, make_number (0));
#ifdef HAVE_WINDOW_SYSTEM
      if (XFASTINT (data->frame_tool_bar_lines)
	  != previous_frame_tool_bar_lines)
	x_set_tool_bar_lines (f, data->frame_tool_bar_lines, make_number (0));
#endif
#endif

      /* "Swap out" point from the selected window's buffer
	 into the window itself.  (Normally the pointm of the selected
	 window holds garbage.)  We do this now, before
	 restoring the window contents, and prevent it from
	 being done later on when we select a new window.  */
      if (! NILP (XWINDOW (selected_window)->buffer))
	{
	  w = XWINDOW (selected_window);
	  set_marker_both (w->pointm,
			   w->buffer,
			   BUF_PT (XBUFFER (w->buffer)),
			   BUF_PT_BYTE (XBUFFER (w->buffer)));
	}

      windows_or_buffers_changed++;
      FRAME_WINDOW_SIZES_CHANGED (f) = 1;

      /* Problem: Freeing all matrices and later allocating them again
	 is a serious redisplay flickering problem.  What we would
	 really like to do is to free only those matrices not reused
	 below.   */
      root_window = XWINDOW (FRAME_ROOT_WINDOW (f));
      leaf_windows
	= (struct window **) alloca (count_windows (root_window)
				     * sizeof (struct window *));
      n_leaf_windows = get_leaf_windows (root_window, leaf_windows, 0);

      /* Temporarily avoid any problems with windows that are smaller
	 than they are supposed to be.  */
      window_min_height = 1;
      window_min_width = 1;

      /* Kludge Alert!
	 Mark all windows now on frame as "deleted".
	 Restoring the new configuration "undeletes" any that are in it.

	 Save their current buffers in their height fields, since we may
	 need it later, if a buffer saved in the configuration is now
	 dead.  */
      delete_all_subwindows (XWINDOW (FRAME_ROOT_WINDOW (f)));

      for (k = 0; k < saved_windows->size; k++)
	{
	  p = SAVED_WINDOW_N (saved_windows, k);
	  w = XWINDOW (p->window);
	  w->next = Qnil;

	  if (!NILP (p->parent))
	    w->parent = SAVED_WINDOW_N (saved_windows,
					XFASTINT (p->parent))->window;
	  else
	    w->parent = Qnil;

	  if (!NILP (p->prev))
	    {
	      w->prev = SAVED_WINDOW_N (saved_windows,
					XFASTINT (p->prev))->window;
	      XWINDOW (w->prev)->next = p->window;
	    }
	  else
	    {
	      w->prev = Qnil;
	      if (!NILP (w->parent))
		{
		  if (EQ (p->total_cols, XWINDOW (w->parent)->total_cols))
		    {
		      XWINDOW (w->parent)->vchild = p->window;
		      XWINDOW (w->parent)->hchild = Qnil;
		    }
		  else
		    {
		      XWINDOW (w->parent)->hchild = p->window;
		      XWINDOW (w->parent)->vchild = Qnil;
		    }
		}
	    }

	  /* If we squirreled away the buffer in the window's height,
	     restore it now.  */
	  if (BUFFERP (w->total_lines))
	    w->buffer = w->total_lines;
	  w->left_col = p->left_col;
	  w->top_line = p->top_line;
	  w->total_cols = p->total_cols;
	  w->total_lines = p->total_lines;
	  w->hscroll = p->hscroll;
	  w->min_hscroll = p->min_hscroll;
	  w->display_table = p->display_table;
	  w->orig_top_line = p->orig_top_line;
	  w->orig_total_lines = p->orig_total_lines;
	  w->left_margin_cols = p->left_margin_cols;
	  w->right_margin_cols = p->right_margin_cols;
	  w->left_fringe_width = p->left_fringe_width;
	  w->right_fringe_width = p->right_fringe_width;
	  w->fringes_outside_margins = p->fringes_outside_margins;
	  w->scroll_bar_width = p->scroll_bar_width;
	  w->vertical_scroll_bar_type = p->vertical_scroll_bar_type;
	  w->dedicated = p->dedicated;
	  XSETFASTINT (w->last_modified, 0);
	  XSETFASTINT (w->last_overlay_modified, 0);

	  /* Reinstall the saved buffer and pointers into it.  */
	  if (NILP (p->buffer))
	    w->buffer = p->buffer;
	  else
	    {
	      if (!NILP (XBUFFER (p->buffer)->name))
		/* If saved buffer is alive, install it.  */
		{
		  w->buffer = p->buffer;
		  w->start_at_line_beg = p->start_at_line_beg;
		  set_marker_restricted (w->start, p->start, w->buffer);
		  set_marker_restricted (w->pointm, p->pointm, w->buffer);
		  Fset_marker (XBUFFER (w->buffer)->mark,
			       p->mark, w->buffer);

		  /* As documented in Fcurrent_window_configuration, don't
		     restore the location of point in the buffer which was
		     current when the window configuration was recorded.  */
		  if (!EQ (p->buffer, new_current_buffer)
		      && XBUFFER (p->buffer) == current_buffer)
		    Fgoto_char (w->pointm);
		}
	      else if (NILP (w->buffer) || NILP (XBUFFER (w->buffer)->name))
		/* Else unless window has a live buffer, get one.  */
		{
		  w->buffer = Fcdr (Fcar (Vbuffer_alist));
		  /* This will set the markers to beginning of visible
		     range.  */
		  set_marker_restricted (w->start, make_number (0), w->buffer);
		  set_marker_restricted (w->pointm, make_number (0),w->buffer);
		  w->start_at_line_beg = Qt;
		}
	      else
		/* Keeping window's old buffer; make sure the markers
		   are real.  */
		{
		  /* Set window markers at start of visible range.  */
		  if (XMARKER (w->start)->buffer == 0)
		    set_marker_restricted (w->start, make_number (0),
					   w->buffer);
		  if (XMARKER (w->pointm)->buffer == 0)
		    set_marker_restricted_both (w->pointm, w->buffer,
						BUF_PT (XBUFFER (w->buffer)),
						BUF_PT_BYTE (XBUFFER (w->buffer)));
		  w->start_at_line_beg = Qt;
		}
	    }
	}

      FRAME_ROOT_WINDOW (f) = data->root_window;
      /* Prevent "swapping out point" in the old selected window
	 using the buffer that has been restored into it.
	 We already swapped out point that from that window's old buffer.  */
      selected_window = Qnil;

      /* Arrange *not* to restore point in the buffer that was
	 current when the window configuration was saved.  */
      if (EQ (XWINDOW (data->current_window)->buffer, new_current_buffer))
	set_marker_restricted (XWINDOW (data->current_window)->pointm,
			       make_number (old_point),
			       XWINDOW (data->current_window)->buffer);

      Fselect_window (data->current_window, Qnil);
      XBUFFER (XWINDOW (selected_window)->buffer)->last_selected_window
	= selected_window;

      if (NILP (data->focus_frame)
	  || (FRAMEP (data->focus_frame)
	      && FRAME_LIVE_P (XFRAME (data->focus_frame))))
	Fredirect_frame_focus (frame, data->focus_frame);

#if 0 /* I don't understand why this is needed, and it causes problems
         when the frame's old selected window has been deleted.  */
      if (f != selected_frame && FRAME_WINDOW_P (f))
	do_switch_frame (WINDOW_FRAME (XWINDOW (data->root_window)),
			 0, 0);
#endif

      /* Set the screen height to the value it had before this function.  */
      if (previous_frame_lines != FRAME_LINES (f)
	  || previous_frame_cols != FRAME_COLS (f))
	change_frame_size (f, previous_frame_lines, previous_frame_cols,
			   0, 0, 0);
#if defined (HAVE_WINDOW_SYSTEM) || defined (MSDOS)
      if (previous_frame_menu_bar_lines != FRAME_MENU_BAR_LINES (f))
	x_set_menu_bar_lines (f, make_number (previous_frame_menu_bar_lines),
			      make_number (0));
#ifdef HAVE_WINDOW_SYSTEM
      if (previous_frame_tool_bar_lines != FRAME_TOOL_BAR_LINES (f))
	x_set_tool_bar_lines (f, make_number (previous_frame_tool_bar_lines),
			      make_number (0));
#endif
#endif

      /* Now, free glyph matrices in windows that were not reused.  */
      for (i = n = 0; i < n_leaf_windows; ++i)
	{
	  if (NILP (leaf_windows[i]->buffer))
	    {
	      /* Assert it's not reused as a combination.  */
	      xassert (NILP (leaf_windows[i]->hchild)
		       && NILP (leaf_windows[i]->vchild));
	      free_window_matrices (leaf_windows[i]);
	    }
	  else if (EQ (leaf_windows[i]->buffer, new_current_buffer))
	    ++n;
	}

      adjust_glyphs (f);

      UNBLOCK_INPUT;

      /* Fselect_window will have made f the selected frame, so we
	 reselect the proper frame here.  Fhandle_switch_frame will change the
	 selected window too, but that doesn't make the call to
	 Fselect_window above totally superfluous; it still sets f's
	 selected window.  */
      if (FRAME_LIVE_P (XFRAME (data->selected_frame)))
	do_switch_frame (data->selected_frame, 0, 0);

      if (! NILP (Vwindow_configuration_change_hook)
	  && ! NILP (Vrun_hooks))
	call1 (Vrun_hooks, Qwindow_configuration_change_hook);
    }

  if (!NILP (new_current_buffer))
    Fset_buffer (new_current_buffer);

  /* Restore the minimum heights recorded in the configuration.  */
  window_min_height = XINT (data->min_height);
  window_min_width = XINT (data->min_width);

  Vminibuf_scroll_window = data->minibuf_scroll_window;
  minibuf_selected_window = data->minibuf_selected_window;

  return (FRAME_LIVE_P (f) ? Qt : Qnil);
}

/* Mark all windows now on frame as deleted
   by setting their buffers to nil.  */

void
delete_all_subwindows (w)
     register struct window *w;
{
  if (!NILP (w->next))
    delete_all_subwindows (XWINDOW (w->next));
  if (!NILP (w->vchild))
    delete_all_subwindows (XWINDOW (w->vchild));
  if (!NILP (w->hchild))
    delete_all_subwindows (XWINDOW (w->hchild));

  w->total_lines = w->buffer;       /* See Fset_window_configuration for excuse.  */

  if (!NILP (w->buffer))
    unshow_buffer (w);

  /* We set all three of these fields to nil, to make sure that we can
     distinguish this dead window from any live window.  Live leaf
     windows will have buffer set, and combination windows will have
     vchild or hchild set.  */
  w->buffer = Qnil;
  w->vchild = Qnil;
  w->hchild = Qnil;

  Vwindow_list = Qnil;
}

static int
count_windows (window)
     register struct window *window;
{
  register int count = 1;
  if (!NILP (window->next))
    count += count_windows (XWINDOW (window->next));
  if (!NILP (window->vchild))
    count += count_windows (XWINDOW (window->vchild));
  if (!NILP (window->hchild))
    count += count_windows (XWINDOW (window->hchild));
  return count;
}


/* Fill vector FLAT with leaf windows under W, starting at index I.
   Value is last index + 1.  */

static int
get_leaf_windows (w, flat, i)
     struct window *w;
     struct window **flat;
     int i;
{
  while (w)
    {
      if (!NILP (w->hchild))
	i = get_leaf_windows (XWINDOW (w->hchild), flat, i);
      else if (!NILP (w->vchild))
	i = get_leaf_windows (XWINDOW (w->vchild), flat, i);
      else
	flat[i++] = w;

      w = NILP (w->next) ? 0 : XWINDOW (w->next);
    }

  return i;
}


/* Return a pointer to the glyph W's physical cursor is on.  Value is
   null if W's current matrix is invalid, so that no meaningfull glyph
   can be returned.  */

struct glyph *
get_phys_cursor_glyph (w)
     struct window *w;
{
  struct glyph_row *row;
  struct glyph *glyph;

  if (w->phys_cursor.vpos >= 0
      && w->phys_cursor.vpos < w->current_matrix->nrows
      && (row = MATRIX_ROW (w->current_matrix, w->phys_cursor.vpos),
	  row->enabled_p)
      && row->used[TEXT_AREA] > w->phys_cursor.hpos)
    glyph = row->glyphs[TEXT_AREA] + w->phys_cursor.hpos;
  else
    glyph = NULL;

  return glyph;
}


static int
save_window_save (window, vector, i)
     Lisp_Object window;
     struct Lisp_Vector *vector;
     int i;
{
  register struct saved_window *p;
  register struct window *w;
  register Lisp_Object tem;

  for (;!NILP (window); window = w->next)
    {
      p = SAVED_WINDOW_N (vector, i);
      w = XWINDOW (window);

      XSETFASTINT (w->temslot, i); i++;
      p->window = window;
      p->buffer = w->buffer;
      p->left_col = w->left_col;
      p->top_line = w->top_line;
      p->total_cols = w->total_cols;
      p->total_lines = w->total_lines;
      p->hscroll = w->hscroll;
      p->min_hscroll = w->min_hscroll;
      p->display_table = w->display_table;
      p->orig_top_line = w->orig_top_line;
      p->orig_total_lines = w->orig_total_lines;
      p->left_margin_cols = w->left_margin_cols;
      p->right_margin_cols = w->right_margin_cols;
      p->left_fringe_width = w->left_fringe_width;
      p->right_fringe_width = w->right_fringe_width;
      p->fringes_outside_margins = w->fringes_outside_margins;
      p->scroll_bar_width = w->scroll_bar_width;
      p->vertical_scroll_bar_type = w->vertical_scroll_bar_type;
      p->dedicated = w->dedicated;
      if (!NILP (w->buffer))
	{
	  /* Save w's value of point in the window configuration.
	     If w is the selected window, then get the value of point
	     from the buffer; pointm is garbage in the selected window.  */
	  if (EQ (window, selected_window))
	    {
	      p->pointm = Fmake_marker ();
	      set_marker_both (p->pointm, w->buffer,
			       BUF_PT (XBUFFER (w->buffer)),
			       BUF_PT_BYTE (XBUFFER (w->buffer)));
	    }
	  else
	    p->pointm = Fcopy_marker (w->pointm, Qnil);

	  p->start = Fcopy_marker (w->start, Qnil);
	  p->start_at_line_beg = w->start_at_line_beg;

	  tem = XBUFFER (w->buffer)->mark;
	  p->mark = Fcopy_marker (tem, Qnil);
	}
      else
	{
	  p->pointm = Qnil;
	  p->start = Qnil;
	  p->mark = Qnil;
	  p->start_at_line_beg = Qnil;
	}

      if (NILP (w->parent))
	p->parent = Qnil;
      else
	p->parent = XWINDOW (w->parent)->temslot;

      if (NILP (w->prev))
	p->prev = Qnil;
      else
	p->prev = XWINDOW (w->prev)->temslot;

      if (!NILP (w->vchild))
	i = save_window_save (w->vchild, vector, i);
      if (!NILP (w->hchild))
	i = save_window_save (w->hchild, vector, i);
    }

  return i;
}

DEFUN ("current-window-configuration", Fcurrent_window_configuration,
       Scurrent_window_configuration, 0, 1, 0,
       doc: /* Return an object representing the current window configuration of FRAME.
If FRAME is nil or omitted, use the selected frame.
This describes the number of windows, their sizes and current buffers,
and for each displayed buffer, where display starts, and the positions of
point and mark.  An exception is made for point in the current buffer:
its value is -not- saved.
This also records the currently selected frame, and FRAME's focus
redirection (see `redirect-frame-focus').  */)
     (frame)
     Lisp_Object frame;
{
  register Lisp_Object tem;
  register int n_windows;
  register struct save_window_data *data;
  register struct Lisp_Vector *vec;
  register int i;
  FRAME_PTR f;

  if (NILP (frame))
    frame = selected_frame;
  CHECK_LIVE_FRAME (frame);
  f = XFRAME (frame);

  n_windows = count_windows (XWINDOW (FRAME_ROOT_WINDOW (f)));
  vec = allocate_other_vector (VECSIZE (struct save_window_data));
  data = (struct save_window_data *)vec;

  XSETFASTINT (data->frame_cols, FRAME_COLS (f));
  XSETFASTINT (data->frame_lines, FRAME_LINES (f));
  XSETFASTINT (data->frame_menu_bar_lines, FRAME_MENU_BAR_LINES (f));
  XSETFASTINT (data->frame_tool_bar_lines, FRAME_TOOL_BAR_LINES (f));
  data->selected_frame = selected_frame;
  data->current_window = FRAME_SELECTED_WINDOW (f);
  XSETBUFFER (data->current_buffer, current_buffer);
  data->minibuf_scroll_window = minibuf_level > 0 ? Vminibuf_scroll_window : Qnil;
  data->minibuf_selected_window = minibuf_level > 0 ? minibuf_selected_window : Qnil;
  data->root_window = FRAME_ROOT_WINDOW (f);
  data->focus_frame = FRAME_FOCUS_FRAME (f);
  XSETINT (data->min_height, window_min_height);
  XSETINT (data->min_width, window_min_width);
  tem = Fmake_vector (make_number (n_windows), Qnil);
  data->saved_windows = tem;
  for (i = 0; i < n_windows; i++)
    XVECTOR (tem)->contents[i]
      = Fmake_vector (make_number (VECSIZE (struct saved_window)), Qnil);
  save_window_save (FRAME_ROOT_WINDOW (f), XVECTOR (tem), 0);
  XSETWINDOW_CONFIGURATION (tem, data);
  return (tem);
}

DEFUN ("save-window-excursion", Fsave_window_excursion, Ssave_window_excursion,
       0, UNEVALLED, 0,
       doc: /* Execute BODY, preserving window sizes and contents.
Return the value of the last form in BODY.
Restore which buffer appears in which window, where display starts,
and the value of point and mark for each window.
Also restore the choice of selected window.
Also restore which buffer is current.
Does not restore the value of point in current buffer.
usage: (save-window-excursion BODY ...)  */)
     (args)
     Lisp_Object args;
{
  register Lisp_Object val;
  register int count = SPECPDL_INDEX ();

  record_unwind_protect (Fset_window_configuration,
			 Fcurrent_window_configuration (Qnil));
  val = Fprogn (args);
  return unbind_to (count, val);
}



/***********************************************************************
			    Window Split Tree
 ***********************************************************************/

static Lisp_Object
window_tree (w)
     struct window *w;
{
  Lisp_Object tail = Qnil;
  Lisp_Object result = Qnil;

  while (w)
    {
      Lisp_Object wn;

      XSETWINDOW (wn, w);
      if (!NILP (w->hchild))
	wn = Fcons (Qnil, Fcons (Fwindow_edges (wn),
				 window_tree (XWINDOW (w->hchild))));
      else if (!NILP (w->vchild))
	wn = Fcons (Qt, Fcons (Fwindow_edges (wn),
			       window_tree (XWINDOW (w->vchild))));

      if (NILP (result))
	{
	  result = tail = Fcons (wn, Qnil);
	}
      else
	{
	  XSETCDR (tail, Fcons (wn, Qnil));
	  tail = XCDR (tail);
	}

      w = NILP (w->next) ? 0 : XWINDOW (w->next);
    }

  return result;
}



DEFUN ("window-tree", Fwindow_tree, Swindow_tree,
       0, 1, 0,
       doc: /* Return the window tree for frame FRAME.

The return value is a list of the form (ROOT MINI), where ROOT
represents the window tree of the frame's root window, and MINI
is the frame's minibuffer window.

If the root window is not split, ROOT is the root window itself.
Otherwise, ROOT is a list (DIR EDGES W1 W2 ...) where DIR is nil for a
horizontal split, and t for a vertical split, EDGES gives the combined
size and position of the subwindows in the split, and the rest of the
elements are the subwindows in the split.  Each of the subwindows may
again be a window or a list representing a window split, and so on.
EDGES is a list \(LEFT TOP RIGHT BOTTOM) as returned by `window-edges'.

If FRAME is nil or omitted, return information on the currently
selected frame.  */)
     (frame)
     Lisp_Object frame;
{
  FRAME_PTR f;

  if (NILP (frame))
    frame = selected_frame;

  CHECK_FRAME (frame);
  f = XFRAME (frame);

  if (!FRAME_LIVE_P (f))
    return Qnil;

  return window_tree (XWINDOW (FRAME_ROOT_WINDOW (f)));
}


/***********************************************************************
			    Marginal Areas
 ***********************************************************************/

DEFUN ("set-window-margins", Fset_window_margins, Sset_window_margins,
       2, 3, 0,
       doc: /* Set width of marginal areas of window WINDOW.
If WINDOW is nil, set margins of the currently selected window.
Second arg LEFT-WIDTH specifies the number of character cells to
reserve for the left marginal area.  Optional third arg RIGHT-WIDTH
does the same for the right marginal area.  A nil width parameter
means no margin.  */)
     (window, left_width, right_width)
     Lisp_Object window, left_width, right_width;
{
  struct window *w = decode_window (window);

  /* Translate negative or zero widths to nil.
     Margins that are too wide have to be checked elsewhere.  */

  if (!NILP (left_width))
    {
      CHECK_NUMBER (left_width);
      if (XINT (left_width) <= 0)
	left_width = Qnil;
    }

  if (!NILP (right_width))
    {
      CHECK_NUMBER (right_width);
      if (XINT (right_width) <= 0)
	right_width = Qnil;
    }

  if (!EQ (w->left_margin_cols, left_width)
      || !EQ (w->right_margin_cols, right_width))
    {
      w->left_margin_cols = left_width;
      w->right_margin_cols = right_width;

      adjust_window_margins (w);

      ++windows_or_buffers_changed;
      adjust_glyphs (XFRAME (WINDOW_FRAME (w)));
    }

  return Qnil;
}


DEFUN ("window-margins", Fwindow_margins, Swindow_margins,
       0, 1, 0,
       doc: /* Get width of marginal areas of window WINDOW.
If WINDOW is omitted or nil, use the currently selected window.
Value is a cons of the form (LEFT-WIDTH . RIGHT-WIDTH).
If a marginal area does not exist, its width will be returned
as nil.  */)
     (window)
     Lisp_Object window;
{
  struct window *w = decode_window (window);
  return Fcons (w->left_margin_cols, w->right_margin_cols);
}



/***********************************************************************
			    Fringes
 ***********************************************************************/

DEFUN ("set-window-fringes", Fset_window_fringes, Sset_window_fringes,
       2, 4, 0,
       doc: /* Set the fringe widths of window WINDOW.
If WINDOW is nil, set the fringe widths of the currently selected
window.
Second arg LEFT-WIDTH specifies the number of pixels to reserve for
the left fringe.  Optional third arg RIGHT-WIDTH specifies the right
fringe width.  If a fringe width arg is nil, that means to use the
frame's default fringe width.  Default fringe widths can be set with
the command `set-fringe-style'.
If optional fourth arg OUTSIDE-MARGINS is non-nil, draw the fringes
outside of the display margins.  By default, fringes are drawn between
display marginal areas and the text area.  */)
     (window, left_width, right_width, outside_margins)
     Lisp_Object window, left_width, right_width, outside_margins;
{
  struct window *w = decode_window (window);

  if (!NILP (left_width))
    CHECK_NATNUM (left_width);
  if (!NILP (right_width))
    CHECK_NATNUM (right_width);

  /* Do nothing on a tty.  */
  if (FRAME_WINDOW_P (WINDOW_XFRAME (w))
      && (!EQ (w->left_fringe_width, left_width)
	  || !EQ (w->right_fringe_width, right_width)
	  || !EQ (w->fringes_outside_margins, outside_margins)))
    {
      w->left_fringe_width = left_width;
      w->right_fringe_width = right_width;
      w->fringes_outside_margins = outside_margins;

      adjust_window_margins (w);

      clear_glyph_matrix (w->current_matrix);
      w->window_end_valid = Qnil;

      ++windows_or_buffers_changed;
      adjust_glyphs (XFRAME (WINDOW_FRAME (w)));
    }

  return Qnil;
}


DEFUN ("window-fringes", Fwindow_fringes, Swindow_fringes,
       0, 1, 0,
       doc: /* Get width of fringes of window WINDOW.
If WINDOW is omitted or nil, use the currently selected window.
Value is a list of the form (LEFT-WIDTH RIGHT-WIDTH OUTSIDE-MARGINS).  */)
     (window)
     Lisp_Object window;
{
  struct window *w = decode_window (window);

  return Fcons (make_number (WINDOW_LEFT_FRINGE_WIDTH (w)),
		Fcons (make_number (WINDOW_RIGHT_FRINGE_WIDTH (w)),
		       Fcons ((WINDOW_HAS_FRINGES_OUTSIDE_MARGINS (w)
			       ? Qt : Qnil), Qnil)));
}



/***********************************************************************
			    Scroll bars
 ***********************************************************************/

DEFUN ("set-window-scroll-bars", Fset_window_scroll_bars, Sset_window_scroll_bars,
       2, 4, 0,
       doc: /* Set width and type of scroll bars of window WINDOW.
If window is nil, set scroll bars of the currently selected window.
Second parameter WIDTH specifies the pixel width for the scroll bar;
this is automatically adjusted to a multiple of the frame column width.
Third parameter VERTICAL-TYPE specifies the type of the vertical scroll
bar: left, right, or nil.
If WIDTH is nil, use the frame's scroll-bar width.
If VERTICAL-TYPE is t, use the frame's scroll-bar type.
Fourth parameter HORIZONTAL-TYPE is currently unused.  */)
     (window, width, vertical_type, horizontal_type)
     Lisp_Object window, width, vertical_type, horizontal_type;
{
  struct window *w = decode_window (window);

  if (!NILP (width))
    {
      CHECK_NATNUM (width);

      if (XINT (width) == 0)
	vertical_type = Qnil;
    }

  if (!(EQ (vertical_type, Qnil)
	|| EQ (vertical_type, Qleft)
	|| EQ (vertical_type, Qright)
	|| EQ (vertical_type, Qt)))
    error ("Invalid type of vertical scroll bar");

  if (!EQ (w->scroll_bar_width, width)
      || !EQ (w->vertical_scroll_bar_type, vertical_type))
    {
      w->scroll_bar_width = width;
      w->vertical_scroll_bar_type = vertical_type;

      adjust_window_margins (w);

      clear_glyph_matrix (w->current_matrix);
      w->window_end_valid = Qnil;

      ++windows_or_buffers_changed;
      adjust_glyphs (XFRAME (WINDOW_FRAME (w)));
    }

  return Qnil;
}


DEFUN ("window-scroll-bars", Fwindow_scroll_bars, Swindow_scroll_bars,
       0, 1, 0,
       doc: /* Get width and type of scroll bars of window WINDOW.
If WINDOW is omitted or nil, use the currently selected window.
Value is a list of the form (WIDTH COLS VERTICAL-TYPE HORIZONTAL-TYPE).
If WIDTH is nil or TYPE is t, the window is using the frame's corresponding
value.  */)
     (window)
     Lisp_Object window;
{
  struct window *w = decode_window (window);
  return Fcons (make_number ((WINDOW_CONFIG_SCROLL_BAR_WIDTH (w)
			      ? WINDOW_CONFIG_SCROLL_BAR_WIDTH (w)
			      : WINDOW_SCROLL_BAR_AREA_WIDTH (w))),
		Fcons (make_number (WINDOW_SCROLL_BAR_COLS (w)),
		       Fcons (w->vertical_scroll_bar_type,
			      Fcons (Qnil, Qnil))));
}



/***********************************************************************
			   Smooth scrolling
 ***********************************************************************/

DEFUN ("window-vscroll", Fwindow_vscroll, Swindow_vscroll, 0, 2, 0,
       doc: /* Return the amount by which WINDOW is scrolled vertically.
Use the selected window if WINDOW is nil or omitted.
Normally, value is a multiple of the canonical character height of WINDOW;
optional second arg PIXELS-P means value is measured in pixels.  */)
  (window, pixels_p)
     Lisp_Object window, pixels_p;
{
  Lisp_Object result;
  struct frame *f;
  struct window *w;

  if (NILP (window))
    window = selected_window;
  else
    CHECK_WINDOW (window);
  w = XWINDOW (window);
  f = XFRAME (w->frame);

  if (FRAME_WINDOW_P (f))
    result = (NILP (pixels_p)
	      ? FRAME_CANON_Y_FROM_PIXEL_Y (f, -w->vscroll)
	      : make_number (-w->vscroll));
  else
    result = make_number (0);
  return result;
}


DEFUN ("set-window-vscroll", Fset_window_vscroll, Sset_window_vscroll,
       2, 3, 0,
       doc: /* Set amount by which WINDOW should be scrolled vertically to VSCROLL.
WINDOW nil means use the selected window.  Normally, VSCROLL is a
non-negative multiple of the canonical character height of WINDOW;
optional third arg PIXELS-P non-nil means that VSCROLL is in pixels.
If PIXELS-P is nil, VSCROLL may have to be rounded so that it
corresponds to an integral number of pixels.  The return value is the
result of this rounding.
If PIXELS-P is non-nil, the return value is VSCROLL.  */)
  (window, vscroll, pixels_p)
     Lisp_Object window, vscroll, pixels_p;
{
  struct window *w;
  struct frame *f;

  if (NILP (window))
    window = selected_window;
  else
    CHECK_WINDOW (window);
  CHECK_NUMBER_OR_FLOAT (vscroll);

  w = XWINDOW (window);
  f = XFRAME (w->frame);

  if (FRAME_WINDOW_P (f))
    {
      int old_dy = w->vscroll;

      w->vscroll = - (NILP (pixels_p)
		      ? FRAME_LINE_HEIGHT (f) * XFLOATINT (vscroll)
		      : XFLOATINT (vscroll));
      w->vscroll = min (w->vscroll, 0);

      if (w->vscroll != old_dy)
	{
	  /* Adjust glyph matrix of the frame if the virtual display
	     area becomes larger than before.  */
	  if (w->vscroll < 0 && w->vscroll < old_dy)
	    adjust_glyphs (f);

	  /* Prevent redisplay shortcuts.  */
	  XBUFFER (w->buffer)->prevent_redisplay_optimizations_p = 1;
	}
    }

  return Fwindow_vscroll (window, pixels_p);
}


/* Call FN for all leaf windows on frame F.  FN is called with the
   first argument being a pointer to the leaf window, and with
   additional argument USER_DATA.  Stops when FN returns 0.  */

void
foreach_window (f, fn, user_data)
     struct frame *f;
     int (* fn) P_ ((struct window *, void *));
     void *user_data;
{
  /* Fdelete_frame may set FRAME_ROOT_WINDOW (f) to Qnil.  */
  if (WINDOWP (FRAME_ROOT_WINDOW (f)))
    foreach_window_1 (XWINDOW (FRAME_ROOT_WINDOW (f)), fn, user_data);
}


/* Helper function for foreach_window.  Call FN for all leaf windows
   reachable from W.  FN is called with the first argument being a
   pointer to the leaf window, and with additional argument USER_DATA.
   Stop when FN returns 0.  Value is 0 if stopped by FN.  */

static int
foreach_window_1 (w, fn, user_data)
     struct window *w;
     int (* fn) P_ ((struct window *, void *));
     void *user_data;
{
  int cont;

  for (cont = 1; w && cont;)
    {
      if (!NILP (w->hchild))
 	cont = foreach_window_1 (XWINDOW (w->hchild), fn, user_data);
      else if (!NILP (w->vchild))
 	cont = foreach_window_1 (XWINDOW (w->vchild), fn, user_data);
      else
	cont = fn (w, user_data);

      w = NILP (w->next) ? 0 : XWINDOW (w->next);
    }

  return cont;
}


/* Freeze or unfreeze the window start of W unless it is a
   mini-window or the selected window.  FREEZE_P non-null means freeze
   the window start.  */

static int
freeze_window_start (w, freeze_p)
     struct window *w;
     void *freeze_p;
{
  if (w == XWINDOW (selected_window)
      || MINI_WINDOW_P (w)
      || (MINI_WINDOW_P (XWINDOW (selected_window))
	  && ! NILP (Vminibuf_scroll_window)
	  && w == XWINDOW (Vminibuf_scroll_window)))
    freeze_p = NULL;

  w->frozen_window_start_p = freeze_p != NULL;
  return 1;
}


/* Freeze or unfreeze the window starts of all leaf windows on frame
   F, except the selected window and a mini-window.  FREEZE_P non-zero
   means freeze the window start.  */

void
freeze_window_starts (f, freeze_p)
     struct frame *f;
     int freeze_p;
{
  foreach_window (f, freeze_window_start, (void *) (freeze_p ? f : 0));
}


/***********************************************************************
			    Initialization
 ***********************************************************************/

/* Return 1 if window configurations C1 and C2
   describe the same state of affairs.  This is used by Fequal.   */

int
compare_window_configurations (c1, c2, ignore_positions)
     Lisp_Object c1, c2;
     int ignore_positions;
{
  register struct save_window_data *d1, *d2;
  struct Lisp_Vector *sw1, *sw2;
  int i;

  CHECK_WINDOW_CONFIGURATION (c1);
  CHECK_WINDOW_CONFIGURATION (c2);

  d1 = (struct save_window_data *) XVECTOR (c1);
  d2 = (struct save_window_data *) XVECTOR (c2);
  sw1 = XVECTOR (d1->saved_windows);
  sw2 = XVECTOR (d2->saved_windows);

  if (! EQ (d1->frame_cols, d2->frame_cols))
    return 0;
  if (! EQ (d1->frame_lines, d2->frame_lines))
    return 0;
  if (! EQ (d1->frame_menu_bar_lines, d2->frame_menu_bar_lines))
    return 0;
  if (! EQ (d1->selected_frame, d2->selected_frame))
    return 0;
  /* Don't compare the current_window field directly.
     Instead see w1_is_current and w2_is_current, below.  */
  if (! EQ (d1->current_buffer, d2->current_buffer))
    return 0;
  if (! ignore_positions)
    {
      if (! EQ (d1->minibuf_scroll_window, d2->minibuf_scroll_window))
	return 0;
      if (! EQ (d1->minibuf_selected_window, d2->minibuf_selected_window))
	return 0;
    }
  /* Don't compare the root_window field.
     We don't require the two configurations
     to use the same window object,
     and the two root windows must be equivalent
     if everything else compares equal.  */
  if (! EQ (d1->focus_frame, d2->focus_frame))
    return 0;
  if (! EQ (d1->min_width, d2->min_width))
    return 0;
  if (! EQ (d1->min_height, d2->min_height))
    return 0;

  /* Verify that the two confis have the same number of windows.  */
  if (sw1->size != sw2->size)
    return 0;

  for (i = 0; i < sw1->size; i++)
    {
      struct saved_window *p1, *p2;
      int w1_is_current, w2_is_current;

      p1 = SAVED_WINDOW_N (sw1, i);
      p2 = SAVED_WINDOW_N (sw2, i);

      /* Verify that the current windows in the two
	 configurations correspond to each other.  */
      w1_is_current = EQ (d1->current_window, p1->window);
      w2_is_current = EQ (d2->current_window, p2->window);

      if (w1_is_current != w2_is_current)
	return 0;

      /* Verify that the corresponding windows do match.  */
      if (! EQ (p1->buffer, p2->buffer))
	return 0;
      if (! EQ (p1->left_col, p2->left_col))
	return 0;
      if (! EQ (p1->top_line, p2->top_line))
	return 0;
      if (! EQ (p1->total_cols, p2->total_cols))
	return 0;
      if (! EQ (p1->total_lines, p2->total_lines))
	return 0;
      if (! EQ (p1->display_table, p2->display_table))
	return 0;
      if (! EQ (p1->parent, p2->parent))
	return 0;
      if (! EQ (p1->prev, p2->prev))
	return 0;
      if (! ignore_positions)
	{
	  if (! EQ (p1->hscroll, p2->hscroll))
	    return 0;
	  if (!EQ (p1->min_hscroll, p2->min_hscroll))
	    return 0;
	  if (! EQ (p1->start_at_line_beg, p2->start_at_line_beg))
	    return 0;
	  if (NILP (Fequal (p1->start, p2->start)))
	    return 0;
	  if (NILP (Fequal (p1->pointm, p2->pointm)))
	    return 0;
	  if (NILP (Fequal (p1->mark, p2->mark)))
	    return 0;
	}
      if (! EQ (p1->left_margin_cols, p2->left_margin_cols))
	return 0;
      if (! EQ (p1->right_margin_cols, p2->right_margin_cols))
	return 0;
      if (! EQ (p1->left_fringe_width, p2->left_fringe_width))
	return 0;
      if (! EQ (p1->right_fringe_width, p2->right_fringe_width))
	return 0;
      if (! EQ (p1->fringes_outside_margins, p2->fringes_outside_margins))
	return 0;
      if (! EQ (p1->scroll_bar_width, p2->scroll_bar_width))
	return 0;
      if (! EQ (p1->vertical_scroll_bar_type, p2->vertical_scroll_bar_type))
	return 0;
    }

  return 1;
}

DEFUN ("compare-window-configurations", Fcompare_window_configurations,
       Scompare_window_configurations, 2, 2, 0,
       doc: /* Compare two window configurations as regards the structure of windows.
This function ignores details such as the values of point and mark
and scrolling positions.  */)
     (x, y)
     Lisp_Object x, y;
{
  if (compare_window_configurations (x, y, 1))
    return Qt;
  return Qnil;
}

void
init_window_once ()
{
  struct frame *f = make_terminal_frame ();
  XSETFRAME (selected_frame, f);
  Vterminal_frame = selected_frame;
  minibuf_window = f->minibuffer_window;
  selected_window = f->selected_window;
  last_nonminibuf_frame = f;

  window_initialized = 1;
}

void
init_window ()
{
  Vwindow_list = Qnil;
}

void
syms_of_window ()
{
  Qscroll_up = intern ("scroll-up");
  staticpro (&Qscroll_up);

  Qscroll_down = intern ("scroll-down");
  staticpro (&Qscroll_down);

  Qwindow_size_fixed = intern ("window-size-fixed");
  staticpro (&Qwindow_size_fixed);
  Fset (Qwindow_size_fixed, Qnil);

  staticpro (&Qwindow_configuration_change_hook);
  Qwindow_configuration_change_hook
    = intern ("window-configuration-change-hook");

  Qwindowp = intern ("windowp");
  staticpro (&Qwindowp);

  Qwindow_configuration_p = intern ("window-configuration-p");
  staticpro (&Qwindow_configuration_p);

  Qwindow_live_p = intern ("window-live-p");
  staticpro (&Qwindow_live_p);

  Qtemp_buffer_show_hook = intern ("temp-buffer-show-hook");
  staticpro (&Qtemp_buffer_show_hook);

  staticpro (&Vwindow_list);

  minibuf_selected_window = Qnil;
  staticpro (&minibuf_selected_window);

  window_scroll_pixel_based_preserve_y = -1;

  DEFVAR_LISP ("temp-buffer-show-function", &Vtemp_buffer_show_function,
	       doc: /* Non-nil means call as function to display a help buffer.
The function is called with one argument, the buffer to be displayed.
Used by `with-output-to-temp-buffer'.
If this function is used, then it must do the entire job of showing
the buffer; `temp-buffer-show-hook' is not run unless this function runs it.  */);
  Vtemp_buffer_show_function = Qnil;

  DEFVAR_LISP ("display-buffer-function", &Vdisplay_buffer_function,
	       doc: /* If non-nil, function to call to handle `display-buffer'.
It will receive two args, the buffer and a flag which if non-nil means
that the currently selected window is not acceptable.
It should choose or create a window, display the specified buffer in it,
and return the window.
Commands such as `switch-to-buffer-other-window' and `find-file-other-window'
work using this function.  */);
  Vdisplay_buffer_function = Qnil;

  DEFVAR_LISP ("even-window-heights", &Veven_window_heights,
	       doc: /* *If non-nil, `display-buffer' should even the window heights.
If nil, `display-buffer' will leave the window configuration alone.  */);
  Veven_window_heights = Qt;

  DEFVAR_LISP ("minibuffer-scroll-window", &Vminibuf_scroll_window,
	       doc: /* Non-nil means it is the window that C-M-v in minibuffer should scroll.  */);
  Vminibuf_scroll_window = Qnil;

  DEFVAR_BOOL ("mode-line-in-non-selected-windows", &mode_line_in_non_selected_windows,
	       doc: /* Non-nil means to use `mode-line-inactive' face in non-selected windows.
If the minibuffer is active, the `minibuffer-scroll-window' mode line
is displayed in the `mode-line' face.  */);
  mode_line_in_non_selected_windows = 1;

  DEFVAR_LISP ("other-window-scroll-buffer", &Vother_window_scroll_buffer,
	       doc: /* If non-nil, this is a buffer and \\[scroll-other-window] should scroll its window.  */);
  Vother_window_scroll_buffer = Qnil;

  DEFVAR_BOOL ("pop-up-frames", &pop_up_frames,
	       doc: /* *Non-nil means `display-buffer' should make a separate frame.  */);
  pop_up_frames = 0;

  DEFVAR_BOOL ("auto-window-vscroll", &auto_window_vscroll_p,
	       doc: /* *Non-nil means to automatically adjust `window-vscroll' to view tall lines.  */);
  auto_window_vscroll_p = 1;

  DEFVAR_BOOL ("display-buffer-reuse-frames", &display_buffer_reuse_frames,
	       doc: /* *Non-nil means `display-buffer' should reuse frames.
If the buffer in question is already displayed in a frame, raise that frame.  */);
  display_buffer_reuse_frames = 0;

  DEFVAR_LISP ("pop-up-frame-function", &Vpop_up_frame_function,
	       doc: /* Function to call to handle automatic new frame creation.
It is called with no arguments and should return a newly created frame.

A typical value might be `(lambda () (new-frame pop-up-frame-alist))'
where `pop-up-frame-alist' would hold the default frame parameters.  */);
  Vpop_up_frame_function = Qnil;

  DEFVAR_LISP ("special-display-buffer-names", &Vspecial_display_buffer_names,
	       doc: /* *List of buffer names that should have their own special frames.
Displaying a buffer with `display-buffer' or `pop-to-buffer',
if its name is in this list, makes a special frame for it
using `special-display-function'.  See also `special-display-regexps'.

An element of the list can be a list instead of just a string.
There are two ways to use a list as an element:
  (BUFFER FRAME-PARAMETERS...)   (BUFFER FUNCTION OTHER-ARGS...)
In the first case, the FRAME-PARAMETERS are pairs of the form
\(PARAMETER . VALUE); these parameter values are used to create the frame.
In the second case, FUNCTION is called with BUFFER as the first argument,
followed by the OTHER-ARGS--it can display BUFFER in any way it likes.
All this is done by the function found in `special-display-function'.

If the specified frame parameters include (same-buffer . t), the
buffer is displayed in the currently selected window.  Otherwise, if
they include (same-frame . t), the buffer is displayed in a new window
in the currently selected frame.

If this variable appears \"not to work\", because you add a name to it
but that buffer still appears in the selected window, look at the
values of `same-window-buffer-names' and `same-window-regexps'.
Those variables take precedence over this one.  */);
  Vspecial_display_buffer_names = Qnil;

  DEFVAR_LISP ("special-display-regexps", &Vspecial_display_regexps,
	       doc: /* *List of regexps saying which buffers should have their own special frames.
When displaying a buffer with `display-buffer' or `pop-to-buffer',
if any regexp in this list matches the buffer name, it makes a
special frame for the buffer by calling `special-display-function'.

An element of the list can be a list instead of just a string.
There are two ways to use a list as an element:
  (REGEXP FRAME-PARAMETERS...)   (REGEXP FUNCTION OTHER-ARGS...)
In the first case, the FRAME-PARAMETERS are pairs of the form
\(PARAMETER . VALUE); these parameter values are used to create the frame.
In the second case, FUNCTION is called with BUFFER as the first argument,
followed by the OTHER-ARGS--it can display the buffer in any way it likes.
All this is done by the function found in `special-display-function'.

If the specified frame parameters include (same-buffer . t), the
buffer is displayed in the currently selected window.  Otherwise, if
they include (same-frame . t), the buffer is displayed in a new window
in the currently selected frame.

If this variable appears \"not to work\", because you add a regexp to it
but the matching buffers still appear in the selected window, look at the
values of `same-window-buffer-names' and `same-window-regexps'.
Those variables take precedence over this one.  */);
  Vspecial_display_regexps = Qnil;

  DEFVAR_LISP ("special-display-function", &Vspecial_display_function,
	       doc: /* Function to call to make a new frame for a special buffer.
It is called with two arguments, the buffer and optional buffer specific
data, and should return a window displaying that buffer.
The default value normally makes a separate frame for the buffer,
  using `special-display-frame-alist' to specify the frame parameters.
But if the buffer specific data includes (same-buffer . t) then the
  buffer is displayed in the current selected window.
Otherwise if it includes (same-frame . t) then the buffer is displayed in
  a new window in the currently selected frame.

A buffer is special if it is listed in `special-display-buffer-names'
or matches a regexp in `special-display-regexps'.  */);
  Vspecial_display_function = Qnil;

  DEFVAR_LISP ("same-window-buffer-names", &Vsame_window_buffer_names,
	       doc: /* *List of buffer names that should appear in the selected window.
Displaying one of these buffers using `display-buffer' or `pop-to-buffer'
switches to it in the selected window, rather than making it appear
in some other window.

An element of the list can be a cons cell instead of just a string.
Then the car must be a string, which specifies the buffer name.
This is for compatibility with `special-display-buffer-names';
the cdr of the cons cell is ignored.

See also `same-window-regexps'.  */);
  Vsame_window_buffer_names = Qnil;

  DEFVAR_LISP ("same-window-regexps", &Vsame_window_regexps,
	       doc: /* *List of regexps saying which buffers should appear in the selected window.
If a buffer name matches one of these regexps, then displaying it
using `display-buffer' or `pop-to-buffer' switches to it
in the selected window, rather than making it appear in some other window.

An element of the list can be a cons cell instead of just a string.
Then the car must be a string, which specifies the buffer name.
This is for compatibility with `special-display-buffer-names';
the cdr of the cons cell is ignored.

See also `same-window-buffer-names'.  */);
  Vsame_window_regexps = Qnil;

  DEFVAR_BOOL ("pop-up-windows", &pop_up_windows,
	       doc: /* *Non-nil means display-buffer should make new windows.  */);
  pop_up_windows = 1;

  DEFVAR_INT ("next-screen-context-lines", &next_screen_context_lines,
	      doc: /* *Number of lines of continuity when scrolling by screenfuls.  */);
  next_screen_context_lines = 2;

  DEFVAR_INT ("split-height-threshold", &split_height_threshold,
	      doc: /* *A window must be at least this tall to be eligible for splitting
by `display-buffer'.  The value is in line units.
If there is only one window, it is split regardless of this value.  */);
  split_height_threshold = 500;

  DEFVAR_INT ("window-min-height", &window_min_height,
	      doc: /* *Delete any window less than this tall (including its mode line).
The value is in line units. */);
  window_min_height = 4;

  DEFVAR_INT ("window-min-width", &window_min_width,
	      doc: /* *Delete any window less than this wide (measured in characters).  */);
  window_min_width = 10;

  DEFVAR_LISP ("scroll-preserve-screen-position",
	       &Vscroll_preserve_screen_position,
	       doc: /* *Controls if scroll commands move point to keep its screen line unchanged.
A value of nil means point does not keep its screen position except
at the scroll margin or window boundary respectively.
A value of t means point keeps its screen position if the scroll
command moved it vertically out of the window, e.g. when scrolling
by full screens.
Any other value means point always keeps its screen position.  */);
  Vscroll_preserve_screen_position = Qnil;

  DEFVAR_LISP ("window-configuration-change-hook",
	       &Vwindow_configuration_change_hook,
	       doc: /* Functions to call when window configuration changes.
The selected frame is the one whose configuration has changed.  */);
  Vwindow_configuration_change_hook = Qnil;

  defsubr (&Sselected_window);
  defsubr (&Sminibuffer_window);
  defsubr (&Swindow_minibuffer_p);
  defsubr (&Swindowp);
  defsubr (&Swindow_live_p);
  defsubr (&Spos_visible_in_window_p);
  defsubr (&Swindow_line_height);
  defsubr (&Swindow_buffer);
  defsubr (&Swindow_height);
  defsubr (&Swindow_width);
  defsubr (&Swindow_hscroll);
  defsubr (&Sset_window_hscroll);
  defsubr (&Swindow_redisplay_end_trigger);
  defsubr (&Sset_window_redisplay_end_trigger);
  defsubr (&Swindow_edges);
  defsubr (&Swindow_pixel_edges);
  defsubr (&Swindow_inside_edges);
  defsubr (&Swindow_inside_pixel_edges);
  defsubr (&Scoordinates_in_window_p);
  defsubr (&Swindow_at);
  defsubr (&Swindow_point);
  defsubr (&Swindow_start);
  defsubr (&Swindow_end);
  defsubr (&Sset_window_point);
  defsubr (&Sset_window_start);
  defsubr (&Swindow_dedicated_p);
  defsubr (&Sset_window_dedicated_p);
  defsubr (&Swindow_display_table);
  defsubr (&Sset_window_display_table);
  defsubr (&Snext_window);
  defsubr (&Sprevious_window);
  defsubr (&Sother_window);
  defsubr (&Sget_lru_window);
  defsubr (&Sget_largest_window);
  defsubr (&Sget_buffer_window);
  defsubr (&Sdelete_other_windows);
  defsubr (&Sdelete_windows_on);
  defsubr (&Sreplace_buffer_in_windows);
  defsubr (&Sdelete_window);
  defsubr (&Sset_window_buffer);
  defsubr (&Sselect_window);
  defsubr (&Sspecial_display_p);
  defsubr (&Ssame_window_p);
  defsubr (&Sdisplay_buffer);
  defsubr (&Sforce_window_update);
  defsubr (&Ssplit_window);
  defsubr (&Senlarge_window);
  defsubr (&Sshrink_window);
  defsubr (&Sadjust_window_trailing_edge);
  defsubr (&Sscroll_up);
  defsubr (&Sscroll_down);
  defsubr (&Sscroll_left);
  defsubr (&Sscroll_right);
  defsubr (&Sother_window_for_scrolling);
  defsubr (&Sscroll_other_window);
  defsubr (&Sminibuffer_selected_window);
  defsubr (&Srecenter);
  defsubr (&Swindow_text_height);
  defsubr (&Smove_to_window_line);
  defsubr (&Swindow_configuration_p);
  defsubr (&Swindow_configuration_frame);
  defsubr (&Sset_window_configuration);
  defsubr (&Scurrent_window_configuration);
  defsubr (&Ssave_window_excursion);
  defsubr (&Swindow_tree);
  defsubr (&Sset_window_margins);
  defsubr (&Swindow_margins);
  defsubr (&Sset_window_fringes);
  defsubr (&Swindow_fringes);
  defsubr (&Sset_window_scroll_bars);
  defsubr (&Swindow_scroll_bars);
  defsubr (&Swindow_vscroll);
  defsubr (&Sset_window_vscroll);
  defsubr (&Scompare_window_configurations);
  defsubr (&Swindow_list);
}

void
keys_of_window ()
{
  initial_define_key (control_x_map, '1', "delete-other-windows");
  initial_define_key (control_x_map, '2', "split-window");
  initial_define_key (control_x_map, '0', "delete-window");
  initial_define_key (control_x_map, 'o', "other-window");
  initial_define_key (control_x_map, '^', "enlarge-window");
  initial_define_key (control_x_map, '<', "scroll-left");
  initial_define_key (control_x_map, '>', "scroll-right");

  initial_define_key (global_map, Ctl ('V'), "scroll-up");
  initial_define_key (meta_map, Ctl ('V'), "scroll-other-window");
  initial_define_key (meta_map, 'v', "scroll-down");

  initial_define_key (global_map, Ctl('L'), "recenter");
  initial_define_key (meta_map, 'r', "move-to-window-line");
}

/* arch-tag: 90a9c576-0590-48f1-a5f1-6c96a0452d9f
   (do not change this comment) */
