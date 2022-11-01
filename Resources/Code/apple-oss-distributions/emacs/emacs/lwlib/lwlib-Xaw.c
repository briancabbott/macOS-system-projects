/* The lwlib interface to Athena widgets.
Copyright (C) 1993 Chuck Thompson <cthomp@cs.uiuc.edu>
Copyright (C) 1994, 2001, 2002, 2003, 2004, 2005, 2006,
  2007 Free Software Foundation, Inc.

This file is part of the Lucid Widget Library.

The Lucid Widget Library is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

The Lucid Widget Library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Emacs; see the file COPYING.  If not, write to
the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA 02110-1301, USA.  */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include "../src/lisp.h"

#include "lwlib-Xaw.h"

#include <X11/StringDefs.h>
#include <X11/IntrinsicP.h>
#include <X11/CoreP.h>
#include <X11/Shell.h>

#include <X11/Xaw/Scrollbar.h>
#include <X11/Xaw/Paned.h>
#include <X11/Xaw/Dialog.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Label.h>

#include <X11/Xatom.h>

static void xaw_generic_callback (/*Widget, XtPointer, XtPointer*/);


Boolean
lw_xaw_widget_p (widget)
     Widget widget;
{
  return (XtIsSubclass (widget, scrollbarWidgetClass) ||
	  XtIsSubclass (widget, dialogWidgetClass));
}

#if 0
static void
xaw_update_scrollbar (instance, widget, val)
     widget_instance *instance;
     Widget widget;
     widget_value *val;
{
  if (val->scrollbar_data)
    {
      scrollbar_values *data = val->scrollbar_data;
      Dimension height, width;
      Dimension pos_x, pos_y;
      int widget_shown, widget_topOfThumb;
      float new_shown, new_topOfThumb;

      XtVaGetValues (widget,
		     XtNheight, &height,
		     XtNwidth, &width,
		     XtNx, &pos_x,
		     XtNy, &pos_y,
		     XtNtopOfThumb, &widget_topOfThumb,
		     XtNshown, &widget_shown,
		     NULL);

      /*
       * First size and position the scrollbar widget.
       * We need to position it to second-guess the Paned widget's notion
       * of what should happen when the WMShell gets resized.
       */
      if (height != data->scrollbar_height || pos_y != data->scrollbar_pos)
	{
	  XtConfigureWidget (widget, pos_x, data->scrollbar_pos,
			     width, data->scrollbar_height, 0);

	  XtVaSetValues (widget,
			 XtNlength, data->scrollbar_height,
			 XtNthickness, width,
			 NULL);
	}

      /*
       * Now the size the scrollbar's slider.
       */
      new_shown = (float) data->slider_size /
	(float) (data->maximum - data->minimum);

      new_topOfThumb = (float) (data->slider_position - data->minimum) /
	(float) (data->maximum - data->minimum);

      if (new_shown > 1.0)
	new_shown = 1.0;
      if (new_shown < 0)
	new_shown = 0;

      if (new_topOfThumb > 1.0)
	new_topOfThumb = 1.0;
      if (new_topOfThumb < 0)
	new_topOfThumb = 0;

      if (new_shown != widget_shown || new_topOfThumb != widget_topOfThumb)
	XawScrollbarSetThumb (widget, new_topOfThumb, new_shown);
    }
}
#endif

void
#ifdef PROTOTYPES
xaw_update_one_widget (widget_instance *instance, Widget widget,
		       widget_value *val, Boolean deep_p)
#else
xaw_update_one_widget (instance, widget, val, deep_p)
     widget_instance *instance;
     Widget widget;
     widget_value *val;
     Boolean deep_p;
#endif
{
#if 0
  if (XtIsSubclass (widget, scrollbarWidgetClass))
    {
      xaw_update_scrollbar (instance, widget, val);
    }
#endif
  if (XtIsSubclass (widget, dialogWidgetClass))
    {
      Arg al[1];
      int ac = 0;
      XtSetArg (al[ac], XtNlabel, val->contents->value); ac++;
      XtSetValues (widget,  al, ac);
    }
  else if (XtIsSubclass (widget, commandWidgetClass))
    {
      Dimension bw = 0;
      Arg al[3];

      XtVaGetValues (widget, XtNborderWidth, &bw, NULL);
      if (bw == 0)
	/* Don't let buttons end up with 0 borderwidth, that's ugly...
	   Yeah, all this should really be done through app-defaults files
	   or fallback resources, but that's a whole different can of worms
	   that I don't feel like opening right now.  Making Athena widgets
	   not look like shit is just entirely too much work.
	 */
	{
	  XtSetArg (al[0], XtNborderWidth, 1);
	  XtSetValues (widget, al, 1);
	}

      XtSetSensitive (widget, val->enabled);
      XtSetArg (al[0], XtNlabel, val->value);
      /* Force centered button text.  Se above. */
      XtSetArg (al[1], XtNjustify, XtJustifyCenter);
      XtSetValues (widget, al, 2);
      XtRemoveAllCallbacks (widget, XtNcallback);
      XtAddCallback (widget, XtNcallback, xaw_generic_callback, instance);
    }
}

void
xaw_update_one_value (instance, widget, val)
     widget_instance *instance;
     Widget widget;
     widget_value *val;
{
  /* This function is not used by the scrollbars and those are the only
     Athena widget implemented at the moment so do nothing. */
  return;
}

void
xaw_destroy_instance (instance)
     widget_instance *instance;
{
  if (XtIsSubclass (instance->widget, dialogWidgetClass))
    /* Need to destroy the Shell too. */
    XtDestroyWidget (XtParent (instance->widget));
  else
    XtDestroyWidget (instance->widget);
}

void
xaw_popup_menu (widget, event)
     Widget widget;
     XEvent *event;
{
  /* An Athena menubar has not been implemented. */
  return;
}

void
#ifdef PROTOTYPES
xaw_pop_instance (widget_instance *instance, Boolean up)
#else
xaw_pop_instance (instance, up)
     widget_instance *instance;
     Boolean up;
#endif
{
  Widget widget = instance->widget;

  if (up)
    {
      if (XtIsSubclass (widget, dialogWidgetClass))
	{
	  /* For dialogs, we need to call XtPopup on the parent instead
	     of calling XtManageChild on the widget.
	     Also we need to hack the shell's WM_PROTOCOLS to get it to
	     understand what the close box is supposed to do!!
	   */
	  Display *dpy = XtDisplay (widget);
	  Widget shell = XtParent (widget);
	  Atom props [2];
	  int i = 0;
	  props [i++] = XInternAtom (dpy, "WM_DELETE_WINDOW", False);
	  XChangeProperty (dpy, XtWindow (shell),
			   XInternAtom (dpy, "WM_PROTOCOLS", False),
			   XA_ATOM, 32, PropModeAppend,
			   (unsigned char *) props, i);

	  /* Center the widget in its parent.  Why isn't this kind of crap
	     done automatically?  I thought toolkits were supposed to make
	     life easier?
	   */
	  {
	    unsigned int x, y, w, h;
	    Widget topmost = instance->parent;
	    Arg args[2];

	    w = shell->core.width;
	    h = shell->core.height;
	    while (topmost->core.parent && XtIsRealized (topmost->core.parent))
	      topmost = topmost->core.parent;
	    if (topmost->core.width < w) x = topmost->core.x;
	    else x = topmost->core.x + ((topmost->core.width - w) / 2);
	    if (topmost->core.height < h) y = topmost->core.y;
	    else y = topmost->core.y + ((topmost->core.height - h) / 2);
	    /* Using XtMoveWidget caused the widget to come
	       out in the wrong place with vtwm.
	       Question of virtual vs real coords, perhaps.  */
	    XtSetArg (args[0], XtNx, x);
	    XtSetArg (args[1], XtNy, y);
	    XtSetValues (shell, args, 2);
	  }

	  /* Finally, pop it up. */
	  XtPopup (shell, XtGrabNonexclusive);
	}
      else
	XtManageChild (widget);
    }
  else
    {
      if (XtIsSubclass (widget, dialogWidgetClass))
	XtUnmanageChild (XtParent (widget));
      else
	XtUnmanageChild (widget);
    }
}


/* Dialog boxes */

static char overrideTrans[] =
	"<Message>WM_PROTOCOLS: lwlib_delete_dialog()";
/* Dialogs pop down on any key press */
static char dialogOverride[] =
       "<KeyPress>Escape:	lwlib_delete_dialog()";
static void wm_delete_window();
static XtActionsRec xaw_actions [] = {
  {"lwlib_delete_dialog", wm_delete_window}
};
static Boolean actions_initted = False;

static Widget
make_dialog (name, parent, pop_up_p, shell_title, icon_name, text_input_slot, radio_box, list, left_buttons, right_buttons)
     char* name;
     Widget parent;
     Boolean pop_up_p;
     char* shell_title;
     char* icon_name;
     Boolean text_input_slot;
     Boolean radio_box;
     Boolean list;
     int left_buttons;
     int right_buttons;
{
  Arg av [20];
  int ac = 0;
  int i, bc;
  char button_name [255];
  Widget shell;
  Widget dialog;
  Widget button;
  XtTranslations override;

  if (! pop_up_p) abort (); /* not implemented */
  if (text_input_slot) abort (); /* not implemented */
  if (radio_box) abort (); /* not implemented */
  if (list) abort (); /* not implemented */

  if (! actions_initted)
    {
      XtAppContext app = XtWidgetToApplicationContext (parent);
      XtAppAddActions (app, xaw_actions,
		       sizeof (xaw_actions) / sizeof (xaw_actions[0]));
      actions_initted = True;
    }

  override = XtParseTranslationTable (overrideTrans);

  ac = 0;
  XtSetArg (av[ac], XtNtitle, shell_title); ac++;
  XtSetArg (av[ac], XtNallowShellResize, True); ac++;

  /* Don't allow any geometry request from the user.  */
  XtSetArg (av[ac], XtNgeometry, 0); ac++;

  shell = XtCreatePopupShell ("dialog", transientShellWidgetClass,
			      parent, av, ac);
  XtOverrideTranslations (shell, override);

  ac = 0;
  dialog = XtCreateManagedWidget (name, dialogWidgetClass, shell, av, ac);
  override = XtParseTranslationTable (dialogOverride);
  XtOverrideTranslations (dialog, override);

  bc = 0;
  button = 0;
  for (i = 0; i < left_buttons; i++)
    {
      ac = 0;
      XtSetArg (av [ac], XtNfromHoriz, button); ac++;
      XtSetArg (av [ac], XtNleft, XtChainLeft); ac++;
      XtSetArg (av [ac], XtNright, XtChainLeft); ac++;
      XtSetArg (av [ac], XtNtop, XtChainBottom); ac++;
      XtSetArg (av [ac], XtNbottom, XtChainBottom); ac++;
      XtSetArg (av [ac], XtNresizable, True); ac++;
      sprintf (button_name, "button%d", ++bc);
      button = XtCreateManagedWidget (button_name, commandWidgetClass,
				      dialog, av, ac);
    }
  if (right_buttons)
    {
      /* Create a separator

	 I want the separator to take up the slack between the buttons on
	 the right and the buttons on the left (that is I want the buttons
	 after the separator to be packed against the right edge of the
	 window) but I can't seem to make it do it.
       */
      ac = 0;
      XtSetArg (av [ac], XtNfromHoriz, button); ac++;
/*  XtSetArg (av [ac], XtNfromVert, XtNameToWidget (dialog, "label")); ac++; */
      XtSetArg (av [ac], XtNleft, XtChainLeft); ac++;
      XtSetArg (av [ac], XtNright, XtChainRight); ac++;
      XtSetArg (av [ac], XtNtop, XtChainBottom); ac++;
      XtSetArg (av [ac], XtNbottom, XtChainBottom); ac++;
      XtSetArg (av [ac], XtNlabel, ""); ac++;
      XtSetArg (av [ac], XtNwidth, 30); ac++;	/* #### aaack!! */
      XtSetArg (av [ac], XtNborderWidth, 0); ac++;
      XtSetArg (av [ac], XtNshapeStyle, XmuShapeRectangle); ac++;
      XtSetArg (av [ac], XtNresizable, False); ac++;
      XtSetArg (av [ac], XtNsensitive, False); ac++;
      button = XtCreateManagedWidget ("separator",
				      /* labelWidgetClass, */
				      /* This has to be Command to fake out
					 the Dialog widget... */
				      commandWidgetClass,
				      dialog, av, ac);
    }
  for (i = 0; i < right_buttons; i++)
    {
      ac = 0;
      XtSetArg (av [ac], XtNfromHoriz, button); ac++;
      XtSetArg (av [ac], XtNleft, XtChainRight); ac++;
      XtSetArg (av [ac], XtNright, XtChainRight); ac++;
      XtSetArg (av [ac], XtNtop, XtChainBottom); ac++;
      XtSetArg (av [ac], XtNbottom, XtChainBottom); ac++;
      XtSetArg (av [ac], XtNresizable, True); ac++;
      sprintf (button_name, "button%d", ++bc);
      button = XtCreateManagedWidget (button_name, commandWidgetClass,
				      dialog, av, ac);
    }

  return dialog;
}

Widget
xaw_create_dialog (instance)
     widget_instance* instance;
{
  char *name = instance->info->type;
  Widget parent = instance->parent;
  Widget widget;
  Boolean pop_up_p = instance->pop_up_p;
  char *shell_name = 0;
  char *icon_name = 0;
  Boolean text_input_slot = False;
  Boolean radio_box = False;
  Boolean list = False;
  int total_buttons;
  int left_buttons = 0;
  int right_buttons = 1;

  switch (name [0]) {
  case 'E': case 'e':
    icon_name = "dbox-error";
    shell_name = "Error";
    break;

  case 'I': case 'i':
    icon_name = "dbox-info";
    shell_name = "Information";
    break;

  case 'L': case 'l':
    list = True;
    icon_name = "dbox-question";
    shell_name = "Prompt";
    break;

  case 'P': case 'p':
    text_input_slot = True;
    icon_name = "dbox-question";
    shell_name = "Prompt";
    break;

  case 'Q': case 'q':
    icon_name = "dbox-question";
    shell_name = "Question";
    break;
  }

  total_buttons = name [1] - '0';

  if (name [3] == 'T' || name [3] == 't')
    {
      text_input_slot = False;
      radio_box = True;
    }
  else if (name [3])
    right_buttons = name [4] - '0';

  left_buttons = total_buttons - right_buttons;

  widget = make_dialog (name, parent, pop_up_p,
			shell_name, icon_name, text_input_slot, radio_box,
			list, left_buttons, right_buttons);

  return widget;
}


static void
xaw_generic_callback (widget, closure, call_data)
     Widget widget;
     XtPointer closure;
     XtPointer call_data;
{
  widget_instance *instance = (widget_instance *) closure;
  Widget instance_widget;
  LWLIB_ID id;
  XtPointer user_data;

  lw_internal_update_other_instances (widget, closure, call_data);

  if (! instance)
    return;
  if (widget->core.being_destroyed)
    return;

  instance_widget = instance->widget;
  if (!instance_widget)
    return;

  id = instance->info->id;

#if 0
  user_data = NULL;
  XtVaGetValues (widget, XtNuserData, &user_data, NULL);
#else
  /* Damn!  Athena doesn't give us a way to hang our own data on the
     buttons, so we have to go find it...  I guess this assumes that
     all instances of a button have the same call data. */
  {
    widget_value *val = instance->info->val->contents;
    char *name = XtName (widget);
    while (val)
      {
	if (val->name && !strcmp (val->name, name))
	  break;
	val = val->next;
      }
    if (! val) abort ();
    user_data = val->call_data;
  }
#endif

  if (instance->info->selection_cb)
    instance->info->selection_cb (widget, id, user_data);
}

static void
wm_delete_window (w, closure, call_data)
     Widget w;
     XtPointer closure;
     XtPointer call_data;
{
  LWLIB_ID id;
  Cardinal nkids;
  int i;
  Widget *kids = 0;
  Widget widget, shell;

  if (XtIsSubclass (w, dialogWidgetClass))
    shell = XtParent (w);
  else
    shell = w;

  if (! XtIsSubclass (shell, shellWidgetClass))
    abort ();
  XtVaGetValues (shell, XtNnumChildren, &nkids, NULL);
  XtVaGetValues (shell, XtNchildren, &kids, NULL);
  if (!kids || !*kids)
    abort ();
  for (i = 0; i < nkids; i++)
    {
      widget = kids[i];
      if (XtIsSubclass (widget, dialogWidgetClass))
	break;
    }
  id = lw_get_widget_id (widget);
  if (! id) abort ();

  {
    widget_info *info = lw_get_widget_info (id);
    if (! info) abort ();
    if (info->selection_cb)
      info->selection_cb (widget, id, (XtPointer) -1);
  }

  lw_destroy_all_widgets (id);
}


/* Scrollbars */

#if 0
static void
xaw_scrollbar_scroll (widget, closure, call_data)
     Widget widget;
     XtPointer closure;
     XtPointer call_data;
{
  widget_instance *instance = (widget_instance *) closure;
  LWLIB_ID id;
  scroll_event event_data;

  if (!instance || widget->core.being_destroyed)
    return;

  id = instance->info->id;
  event_data.slider_value = 0;
  event_data.time = 0;

  if ((int) call_data > 0)
    event_data.action = SCROLLBAR_PAGE_DOWN;
  else
    event_data.action = SCROLLBAR_PAGE_UP;

  if (instance->info->pre_activate_cb)
    instance->info->pre_activate_cb (widget, id, (XtPointer) &event_data);
}
#endif

#if 0
static void
xaw_scrollbar_jump (widget, closure, call_data)
     Widget widget;
     XtPointer closure;
     XtPointer call_data;
{
  widget_instance *instance = (widget_instance *) closure;
  LWLIB_ID id;
  scroll_event event_data;
  scrollbar_values *val =
    (scrollbar_values *) instance->info->val->scrollbar_data;
  float percent;

  if (!instance || widget->core.being_destroyed)
    return;

  id = instance->info->id;

  percent = * (float *) call_data;
  event_data.slider_value =
    (int) (percent * (float) (val->maximum - val->minimum)) + val->minimum;

  event_data.time = 0;
  event_data.action = SCROLLBAR_DRAG;

  if (instance->info->pre_activate_cb)
    instance->info->pre_activate_cb (widget, id, (XtPointer) &event_data);
}
#endif

static Widget
xaw_create_scrollbar (instance)
     widget_instance *instance;
{
#if 0
  Arg av[20];
  int ac = 0;
  Dimension width;
  Widget scrollbar;

  XtVaGetValues (instance->parent, XtNwidth, &width, NULL);

  XtSetArg (av[ac], XtNshowGrip, 0); ac++;
  XtSetArg (av[ac], XtNresizeToPreferred, 1); ac++;
  XtSetArg (av[ac], XtNallowResize, True); ac++;
  XtSetArg (av[ac], XtNskipAdjust, True); ac++;
  XtSetArg (av[ac], XtNwidth, width); ac++;
  XtSetArg (av[ac], XtNmappedWhenManaged, True); ac++;

  scrollbar =
    XtCreateWidget (instance->info->name, scrollbarWidgetClass,
		    instance->parent, av, ac);

  /* We have to force the border width to be 0 otherwise the
     geometry manager likes to start looping for awhile... */
  XtVaSetValues (scrollbar, XtNborderWidth, 0, NULL);

  XtRemoveAllCallbacks (scrollbar, "jumpProc");
  XtRemoveAllCallbacks (scrollbar, "scrollProc");

  XtAddCallback (scrollbar, "jumpProc", xaw_scrollbar_jump,
		 (XtPointer) instance);
  XtAddCallback (scrollbar, "scrollProc", xaw_scrollbar_scroll,
		 (XtPointer) instance);

  return scrollbar;
#else
  return NULL;
#endif
}

static Widget
xaw_create_main (instance)
     widget_instance *instance;
{
  Arg al[1];
  int ac;

  /* Create a vertical Paned to hold menubar */
  ac = 0;
  XtSetArg (al[ac], XtNborderWidth, 0); ac++;
  return XtCreateWidget (instance->info->name, panedWidgetClass,
			 instance->parent, al, ac);
}

widget_creation_entry
xaw_creation_table [] =
{
  {"scrollbar",			xaw_create_scrollbar},
  {"main",			xaw_create_main},
  {NULL, NULL}
};

/* arch-tag: fbbd3589-ae1c-41a0-9142-f628cfee6564
   (do not change this comment) */
