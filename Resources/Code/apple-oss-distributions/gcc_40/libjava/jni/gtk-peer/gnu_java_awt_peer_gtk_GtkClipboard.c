/* gtkclipboard.c
   Copyright (C) 1998, 1999 Free Software Foundation, Inc.

This file is part of GNU Classpath.

GNU Classpath is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

GNU Classpath is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License
along with GNU Classpath; see the file COPYING.  If not, write to the
Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
02111-1307 USA.

Linking this library statically or dynamically with other modules is
making a combined work based on this library.  Thus, the terms and
conditions of the GNU General Public License cover the whole
combination.

As a special exception, the copyright holders of this library give you
permission to link this library with independent modules to produce an
executable, regardless of the license terms of these independent
modules, and to copy and distribute the resulting executable under
terms of your choice, provided that you also meet, for each linked
independent module, the terms and conditions of the license of that
module.  An independent module is a module which is not derived from
or based on this library.  If you modify this library, you may extend
this exception to your version of the library, but you are not
obligated to do so.  If you do not wish to do so, delete this
exception statement from your version. */


#include "gtkpeer.h"
#include "gnu_java_awt_peer_gtk_GtkClipboard.h"

jmethodID stringSelectionReceivedID;
jmethodID stringSelectionHandlerID;
jmethodID selectionClearID;

void selection_received (GtkWidget *, GtkSelectionData *, guint, gpointer);
void selection_get (GtkWidget *, GtkSelectionData *, guint, guint, gpointer);
gint selection_clear (GtkWidget *, GdkEventSelection *);

GtkWidget *clipboard;
jobject cb_obj;

JNIEXPORT void JNICALL 
Java_gnu_java_awt_peer_gtk_GtkClipboard_initNativeState (JNIEnv *env, 
							 jobject obj)
{
  if (!stringSelectionReceivedID)
    {
      jclass gtkclipboard;

      gtkclipboard = (*env)->FindClass (env, 
					"gnu/java/awt/peer/gtk/GtkClipboard");
      stringSelectionReceivedID = (*env)->GetMethodID (env, gtkclipboard,
						    "stringSelectionReceived",
						    "(Ljava/lang/String;)V");
      stringSelectionHandlerID = (*env)->GetMethodID (env, gtkclipboard,
						      "stringSelectionHandler",
						      "()Ljava/lang/String;");
      selectionClearID = (*env)->GetMethodID (env, gtkclipboard,
					      "selectionClear", "()V");
    }

  cb_obj = (*env)->NewGlobalRef (env, obj);

  gdk_threads_enter ();
  clipboard = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  g_signal_connect (G_OBJECT(clipboard), "selection_received",
		      GTK_SIGNAL_FUNC (selection_received), NULL);

  g_signal_connect (G_OBJECT(clipboard), "selection_clear_event",
		      GTK_SIGNAL_FUNC (selection_clear), NULL);

  gtk_selection_add_target (clipboard, GDK_SELECTION_PRIMARY, 
			    GDK_TARGET_STRING, 0);

  g_signal_connect (G_OBJECT(clipboard), "selection_get",
                      GTK_SIGNAL_FUNC (selection_get), NULL);

  gdk_threads_leave ();
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_gtk_GtkClipboard_requestStringConversion
  (JNIEnv *env __attribute__((unused)), jclass clazz __attribute__((unused)))
{
  gdk_threads_enter ();
  gtk_selection_convert (clipboard, GDK_SELECTION_PRIMARY, 
			 GDK_TARGET_STRING, GDK_CURRENT_TIME);
  gdk_threads_leave ();
}

void
selection_received (GtkWidget *widget __attribute__((unused)),
		    GtkSelectionData *selection_data __attribute__((unused)),
		    guint time __attribute__((unused)),
		    gpointer data __attribute__((unused)))
{
  /* Check to see if retrieval succeeded  */
  if (selection_data->length < 0
      || selection_data->type != GDK_SELECTION_TYPE_STRING)
    {
      (*gdk_env)->CallVoidMethod (gdk_env, cb_obj, stringSelectionReceivedID,
				  NULL);
    }
  else
    {
      char *str = (char *) selection_data->data;
      
      (*gdk_env)->CallVoidMethod (gdk_env, cb_obj, stringSelectionReceivedID,
				  (*gdk_env)->NewStringUTF (gdk_env, str));
    }

  return;
}

void
selection_get (GtkWidget *widget __attribute__((unused)), 
               GtkSelectionData *selection_data,
               guint      info __attribute__((unused)),
               guint      time __attribute__((unused)),
               gpointer   data __attribute__((unused)))
{
  jstring jstr;
  const char *utf;
  jsize utflen;

  jstr = (*gdk_env)->CallObjectMethod (gdk_env, cb_obj, 
				       stringSelectionHandlerID);

  if (!jstr)
    {
      gtk_selection_data_set (selection_data, 
			      GDK_TARGET_STRING, 8, NULL, 0);
      return;
    }

  utflen = (*gdk_env)->GetStringUTFLength (gdk_env, jstr);
  utf = (*gdk_env)->GetStringUTFChars (gdk_env, jstr, NULL);

  gtk_selection_data_set (selection_data, GDK_TARGET_STRING, 8, 
			  (char *)utf, utflen);

  (*gdk_env)->ReleaseStringUTFChars (gdk_env, jstr, utf);
}

JNIEXPORT void JNICALL
Java_gnu_java_awt_peer_gtk_GtkClipboard_selectionGet
  (JNIEnv *env, jclass clazz __attribute__((unused)))
{
  GdkWindow *owner;

  gdk_threads_enter ();

  /* if we already own the clipboard, we need to tell the old data object
     that we're no longer going to be using him */
  owner = gdk_selection_owner_get (GDK_SELECTION_PRIMARY);
  if (owner && owner == clipboard->window)
    (*env)->CallVoidMethod (env, cb_obj, selectionClearID);
    
  gtk_selection_owner_set (clipboard, GDK_SELECTION_PRIMARY, GDK_CURRENT_TIME);

  gdk_threads_leave ();
}

gint
selection_clear (GtkWidget *widget __attribute__((unused)),
		 GdkEventSelection *event __attribute__((unused)))
{
  (*gdk_env)->CallVoidMethod (gdk_env, cb_obj, selectionClearID);

  return TRUE;
}
