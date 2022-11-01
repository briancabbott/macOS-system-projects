/* gnu_java_awt_peer_gtk_GdkGraphics2d.c
   Copyright (C) 2003, 2004 Free Software Foundation, Inc.

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

#include "gtkcairopeer.h"
#include "gdkfont.h"
#include "gnu_java_awt_peer_gtk_GdkGraphics2D.h"
#include <gdk/gdktypes.h>
#include <gdk/gdkprivate.h>
#include <gdk/gdkx.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixdata.h>

#include <cairo.h>

#include <stdio.h>
#include <stdlib.h>

struct state_table *native_graphics2d_state_table;

#define NSA_G2D_INIT(env, clazz) \
  native_graphics2d_state_table = init_state_table (env, clazz)

#define NSA_GET_G2D_PTR(env, obj) \
  get_state (env, obj, native_graphics2d_state_table)

#define NSA_SET_G2D_PTR(env, obj, ptr) \
  set_state (env, obj, native_graphics2d_state_table, (void *)ptr)

#define NSA_DEL_G2D_PTR(env, obj) \
  remove_state_slot (env, obj, native_graphics2d_state_table)

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_initStaticState 
  (JNIEnv *env, jclass clazz)
{
   gdk_threads_enter();
   NSA_G2D_INIT (env, clazz);
   gdk_threads_leave();
}

/* these public final constants are part of the java2d public API, so we
   write them explicitly here to save fetching them from the constant pool
   all the time. */

#ifndef min
#define min(x,y) ((x) < (y) ? (x) : (y))
#endif

enum java_awt_alpha_composite_rule
  {
    java_awt_alpha_composite_CLEAR = 1,
    java_awt_alpha_composite_SRC = 2,
    java_awt_alpha_composite_SRC_OVER = 3,
    java_awt_alpha_composite_DST_OVER = 4,
    java_awt_alpha_composite_SRC_IN = 5,
    java_awt_alpha_composite_DST_IN = 6,
    java_awt_alpha_composite_SRC_OUT = 7,
    java_awt_alpha_composite_DST_OUT = 8,
    java_awt_alpha_composite_DST = 9,
    java_awt_alpha_composite_SRC_ATOP = 10,
    java_awt_alpha_composite_DST_ATOP = 11,
    java_awt_alpha_composite_XOR = 12
  };

enum java_awt_basic_stroke_join_rule
  {
    java_awt_basic_stroke_JOIN_MITER = 0,
    java_awt_basic_stroke_JOIN_ROUND = 1,
    java_awt_basic_stroke_JOIN_BEVEL = 2
  };

enum java_awt_basic_stroke_cap_rule
  {
    java_awt_basic_stroke_CAP_BUTT = 0,
    java_awt_basic_stroke_CAP_ROUND = 1,
    java_awt_basic_stroke_CAP_SQUARE = 2
  };

enum java_awt_geom_path_iterator_winding_rule
  {
    java_awt_geom_path_iterator_WIND_EVEN_ODD = 0,
    java_awt_geom_path_iterator_WIND_NON_ZERO = 1
  };

enum java_awt_rendering_hints_filter
  {
    java_awt_rendering_hints_VALUE_INTERPOLATION_NEAREST_NEIGHBOR = 0,    
    java_awt_rendering_hints_VALUE_INTERPOLATION_BILINEAR = 1,
    java_awt_rendering_hints_VALUE_ALPHA_INTERPOLATION_SPEED = 2,
    java_awt_rendering_hints_VALUE_ALPHA_INTERPOLATION_QUALITY = 3,
    java_awt_rendering_hints_VALUE_ALPHA_INTERPOLATION_DEFAULT = 4
 
  };

static int
peer_is_disposed(JNIEnv *env, jobject obj)
{
  static jfieldID fid = NULL;
  jclass cls;
  jobject peer;

  return 0;

  if (fid == NULL)
    {
      cls = (*env)->GetObjectClass(env, obj);
      fid = (*env)->GetFieldID(env, cls, "component",
			       "Lgnu/java/awt/peer/gtk/GtkComponentPeer;");
    }
  g_assert(fid != NULL);
  peer = (*env)->GetObjectField(env, obj, fid);
  if (peer == NULL || NSA_GET_PTR (env, peer) != NULL)
    return 0;
  else
    {
      return 1;
    }
}


static void 
grab_current_drawable (GtkWidget *widget, GdkDrawable **draw, GdkWindow **win)
{  
  g_assert (widget != NULL);
  g_assert (draw != NULL);
  g_assert (win != NULL);

  if (GTK_IS_WINDOW (widget))
    {
      *win = find_gtk_layout (widget)->bin_window;
    }
  else if (GTK_IS_LAYOUT (widget))
    {
      *win = GTK_LAYOUT (widget)->bin_window;
    }
  else
    {
      *win = widget->window;
    }

  *draw = *win;
  gdk_window_get_internal_paint_info (*win, draw, 0, 0); 
  g_object_ref (*draw);
}


static int
x_server_has_render_extension (void)
{
  int ev = 0, err = 0; 
  return (int) XRenderQueryExtension (GDK_DISPLAY (), &ev, &err);
}


static void
init_graphics2d_as_pixbuf (struct graphics2d *gr)
{
  gint width, height;
  gint bits_per_sample = 8;
  gint total_channels = 4;
  gboolean has_alpha = TRUE;
  
  g_assert (gr != NULL);
  g_assert (gr->drawable != NULL);

  if (gr->debug) printf ("initializing graphics2d as pixbuf\n");
  gdk_drawable_get_size (gr->drawable, &width, &height);
  gr->drawbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, 
				has_alpha, bits_per_sample,
				width, height);
  g_assert (gr->drawbuf != NULL);
  g_assert (gdk_pixbuf_get_bits_per_sample (gr->drawbuf) == bits_per_sample);
  g_assert (gdk_pixbuf_get_n_channels (gr->drawbuf) == total_channels);
  
  gr->surface = cairo_surface_create_for_image (gdk_pixbuf_get_pixels (gr->drawbuf), 
						CAIRO_FORMAT_ARGB32, 
						gdk_pixbuf_get_width (gr->drawbuf), 
						gdk_pixbuf_get_height (gr->drawbuf), 
						gdk_pixbuf_get_rowstride (gr->drawbuf));      
  g_assert (gr->surface != NULL);
  g_assert (gr->cr != NULL);
  cairo_set_target_surface (gr->cr, gr->surface);
}

static void
init_graphics2d_as_renderable (struct graphics2d *gr)
{
  Drawable draw;
  Display * dpy;
  Visual * vis;
  
  g_assert (gr != NULL);
  g_assert (gr->drawable != NULL);

  gr->drawbuf = NULL;
  
  if (gr->debug) printf ("initializing graphics2d as renderable\n");
  draw = gdk_x11_drawable_get_xid (gr->drawable);
  
  dpy = gdk_x11_drawable_get_xdisplay (gr->drawable);
  g_assert (dpy != NULL);
  
  vis = gdk_x11_visual_get_xvisual (gdk_drawable_get_visual (gr->drawable));
  g_assert (vis != NULL);
  
  gr->surface = cairo_xlib_surface_create (dpy, draw, vis, 
					   CAIRO_FORMAT_ARGB32,
					   DefaultColormap (dpy, DefaultScreen (dpy)));
  g_assert (gr->surface != NULL);
  g_assert (gr->cr != NULL);
  cairo_set_target_surface (gr->cr, gr->surface);
}

static void
begin_drawing_operation (struct graphics2d * gr)
{  
  g_assert(cairo_status (gr->cr) == CAIRO_STATUS_SUCCESS);
  if (gr->drawbuf)
    {

      gint drawable_width, drawable_height;
      gint pixbuf_width, pixbuf_height;
      gint width, height;
      
      gdk_drawable_get_size (gr->drawable, &drawable_width, &drawable_height);
      pixbuf_width = gdk_pixbuf_get_width (gr->drawbuf);
      pixbuf_height = gdk_pixbuf_get_height (gr->drawbuf);
      width = min (drawable_width, pixbuf_width);
      height = min (drawable_height, pixbuf_height);

      gdk_pixbuf_get_from_drawable (gr->drawbuf, /* destination pixbuf */
				    gr->drawable, 
				    NULL, /* colormap */
				    0, 0, 0, 0,
				    width, height); 
      
      if (gr->debug) printf ("copied (%d, %d) pixels from GDK drawable to pixbuf\n",
			     width, height);      
    }
}

static void
end_drawing_operation (struct graphics2d * gr)
{
  g_assert(cairo_status (gr->cr) == CAIRO_STATUS_SUCCESS);
  if (gr->drawbuf)
    { 
      gint drawable_width, drawable_height;
      gint pixbuf_width, pixbuf_height;
      gint width, height;
      
      gdk_drawable_get_size (gr->drawable, &drawable_width, &drawable_height);
      pixbuf_width = gdk_pixbuf_get_width (gr->drawbuf);
      pixbuf_height = gdk_pixbuf_get_height (gr->drawbuf);
      width = min (drawable_width, pixbuf_width);
      height = min (drawable_height, pixbuf_height);

      gdk_draw_pixbuf (gr->drawable, NULL, gr->drawbuf,
		       0, 0, 0, 0, 
		       width, height, 
		       GDK_RGB_DITHER_NORMAL, 0, 0);

      if (gr->debug) printf ("copied (%d, %d) pixels from pixbuf to GDK drawable\n",
			     width, height);
    }
}


static void 
update_pattern_transform (struct graphics2d *gr)
{
  double a, b, c, d, tx, ty;
  cairo_matrix_t *mat = NULL;

  g_assert (gr != NULL);
  if (gr->pattern == NULL)
    return;

  return;
  /* temporarily disabled: ambiguous behavior */
  /*   cairo_get_matrix (gr->cr, &a, &b, &c, &d, &tx, &ty); */
  mat = cairo_matrix_create ();
  g_assert (mat != NULL);
  cairo_matrix_set_affine (mat, a, b, c, d, tx, ty);
  cairo_pattern_set_matrix (gr->pattern, mat);
  cairo_matrix_destroy (mat);
}

static void
check_for_debug (struct graphics2d *gr)
{
  gr->debug = (gboolean)(getenv("DEBUGJ2D") != NULL);
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_copyState
  (JNIEnv *env, jobject obj, jobject old)
{
  struct graphics2d *g = NULL, *g_old = NULL;

  gdk_threads_enter();
  g = (struct graphics2d *) malloc (sizeof (struct graphics2d));
  g_assert (g != NULL);
  memset (g, 0, sizeof(struct graphics2d));

  g_old = (struct graphics2d *) NSA_GET_G2D_PTR (env, old);
  g_assert (g_old != NULL);

  if (g_old->debug) printf ("copying state from existing graphics2d\n");

  g->drawable = g_old->drawable;
  g->debug = g_old->debug; 

  g_object_ref (g->drawable);
  
  g->cr = cairo_create();
  g_assert (g->cr != NULL);

  if (x_server_has_render_extension ())
    init_graphics2d_as_renderable (g);
  else
    init_graphics2d_as_pixbuf (g);

  cairo_surface_set_filter (g->surface, CAIRO_FILTER_FAST);

  NSA_SET_G2D_PTR (env, obj, g);
  gdk_threads_leave();
}


JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_initState__II
  (JNIEnv *env, jobject obj, jint width, jint height)
{
  struct graphics2d *gr;
  
  gdk_threads_enter();
  gr = (struct graphics2d *) malloc (sizeof (struct graphics2d));
  g_assert (gr != NULL);
  memset (gr, 0, sizeof(struct graphics2d));

  check_for_debug (gr);  

  if (gr->debug) printf ("constructing offscreen drawable of size (%d,%d)\n",
			 width, height);

  gr->drawable = (GdkDrawable *) gdk_pixmap_new (NULL, width, height, 
						 gdk_rgb_get_visual ()->depth);
  g_assert (gr->drawable != NULL);

  gr->cr = cairo_create();
  g_assert (gr->cr != NULL);

  if (x_server_has_render_extension ())
    init_graphics2d_as_renderable (gr);
  else
    init_graphics2d_as_pixbuf (gr);

  if (gr->debug) printf ("constructed offscreen drawable of size (%d,%d)\n",
			 width, height);
  NSA_SET_G2D_PTR (env, obj, gr);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_gdkDrawDrawable
  (JNIEnv *env, jobject self, jobject other, jint x, jint y)
{
  struct graphics2d *src = NULL, *dst = NULL;
  gint s_height, s_width, d_height, d_width, height, width;
  cairo_matrix_t *matrix;
  cairo_operator_t tmp_op;

  gdk_threads_enter();
  if (peer_is_disposed(env, self)) { gdk_threads_leave(); return; }
  src = (struct graphics2d *)NSA_GET_G2D_PTR (env, other);
  dst = (struct graphics2d *)NSA_GET_G2D_PTR (env, self);
  g_assert (src != NULL);
  g_assert (dst != NULL);  

  if (src->debug) printf ("copying from offscreen drawable\n");

  begin_drawing_operation(dst); 

  gdk_flush();

  gdk_drawable_get_size (src->drawable, &s_width, &s_height);
  gdk_drawable_get_size (dst->drawable, &d_width, &d_height);
  width = min (s_width, d_width);
  height = min (s_height, d_height);

  matrix = cairo_matrix_create ();
  cairo_surface_get_matrix (src->surface, matrix);
  cairo_matrix_translate (matrix, (double)-x, (double)-y);
  cairo_surface_set_matrix (src->surface, matrix);

  tmp_op = cairo_current_operator (dst->cr); 
  cairo_set_operator(dst->cr, CAIRO_OPERATOR_SRC); 
  cairo_show_surface (dst->cr, src->surface, width, height);
  cairo_set_operator(dst->cr, tmp_op);

  cairo_matrix_translate (matrix, (double)x, (double)y);
  cairo_surface_set_matrix (src->surface, matrix);
  cairo_matrix_destroy (matrix);

  gdk_flush();

  end_drawing_operation(dst);

  if (src->debug) printf ("copied %d x %d pixels from offscreen drawable\n", width, height);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_initState__Lgnu_java_awt_peer_gtk_GtkComponentPeer_2
  (JNIEnv *env, jobject obj, jobject peer)
{
  struct graphics2d *gr = NULL;
  GtkWidget *widget = NULL;
  void *ptr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }
  ptr = NSA_GET_PTR (env, peer);
  g_assert (ptr != NULL);

  gr = (struct graphics2d *) malloc (sizeof (struct graphics2d));
  g_assert (gr != NULL);
  memset (gr, 0, sizeof(struct graphics2d));

  check_for_debug (gr);

  gr->cr = cairo_create();
  g_assert (gr->cr != NULL);

  widget = GTK_WIDGET (ptr);
  g_assert (widget != NULL);

  grab_current_drawable (widget, &(gr->drawable), &(gr->win));
  g_assert (gr->drawable != NULL);

  if (x_server_has_render_extension ())
    init_graphics2d_as_renderable (gr);
  else
    init_graphics2d_as_pixbuf (gr);

  NSA_SET_G2D_PTR (env, obj, gr);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_dispose
  (JNIEnv *env, jobject obj)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  gr = (struct graphics2d *) NSA_DEL_G2D_PTR (env, obj);
  if (gr == NULL) 
    {
      gdk_threads_leave();
      return; /* dispose has been called more than once */
    }

  if (gr->surface)
    cairo_surface_destroy (gr->surface);

  cairo_destroy (gr->cr);

  if (gr->drawbuf)
    g_object_unref (gr->drawbuf); 

  g_object_unref (gr->drawable);

  if (gr->pattern)
    cairo_pattern_destroy (gr->pattern);

  if (gr->pattern_surface)
    cairo_surface_destroy (gr->pattern_surface);

  if (gr->pattern_pixels)
    free (gr->pattern_pixels);

  if (gr->debug) printf ("disposed of graphics2d\n");

  free (gr);
  gdk_threads_leave();
}


JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_setGradient 
  (JNIEnv *env, jobject obj, 
   jdouble x1, jdouble y1, 
   jdouble x2, jdouble y2,
   jint r1, jint g1, jint b1, jint a1,
   jint r2, jint g2, jint b2, jint a2,
   jboolean cyclic)
{
  struct graphics2d *gr = NULL;
  cairo_surface_t *surf = NULL;
  cairo_matrix_t *mat = NULL;
  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }
  if (gr->debug) printf ("setGradient (%f,%f) -> (%f,%f); (%d,%d,%d,%d) -> (%d,%d,%d,%d)\n",
			 x1, y1, 
			 x2, y2, 
			 r1, g1, b1, a1, 
			 r2, g2, b2, a2);
  
  cairo_save (gr->cr);
  
  if (cyclic)
    surf = cairo_surface_create_similar (gr->surface, CAIRO_FORMAT_ARGB32, 3, 2);
  else
    surf = cairo_surface_create_similar (gr->surface, CAIRO_FORMAT_ARGB32, 2, 2);      
  g_assert (surf != NULL);

  cairo_set_target_surface (gr->cr, surf);
  
  cairo_identity_matrix (gr->cr);

  cairo_set_rgb_color (gr->cr, r1 / 255.0, g1 / 255.0, b1 / 255.0);
  cairo_set_alpha (gr->cr, a1 / 255.0);
  cairo_rectangle (gr->cr, 0, 0, 1, 2);
  cairo_fill (gr->cr);
    
  cairo_set_rgb_color (gr->cr, r2 / 255.0, g2 / 255.0, b2 / 255.0);
  cairo_set_alpha (gr->cr, a2 / 255.0);
  cairo_rectangle (gr->cr, 1, 0, 1, 2);
  cairo_fill (gr->cr);

  if (cyclic)
    {
      cairo_set_rgb_color (gr->cr, r1 / 255.0, g1 / 255.0, b1 / 255.0);
      cairo_set_alpha (gr->cr, a1 / 255.0);
      cairo_rectangle (gr->cr, 2, 0, 1, 2);
      cairo_fill (gr->cr);
    }

  mat = cairo_matrix_create ();
  g_assert (mat != NULL);

  /* 
     consider the vector [x2 - x1, y2 - y1] = [p,q]

     this is a line in space starting at an 'origin' x1, y1.

     it can also be thought of as a "transformed" unit vector in either the
     x or y directions. we have just *drawn* our gradient as a unit vector
     (well, a 2-3x unit vector) in the x dimension. so what we want to know
     is which transformation turns our existing unit vector into [p,q].

     which means solving for M in 
 
     [p,q] = M[1,0]

     [p,q] = |a b| [1,0]
             |c d|      

     [p,q] = [a,c], with b = d = 0.

     what does this mean? it means that our gradient is 1-dimensional; as
     you move through the x axis of our 2 or 3 pixel gradient from logical
     x positions 0 to 1, the transformation of your x coordinate under the
     matrix M causes you to accumulate both x and y values in fill
     space. the y value of a gradient coordinate is ignored, since the
     gradient is one dimensional. which is correct.

     unfortunately we want the opposite transformation, it seems, because of
     the way cairo is going to use this transformation. I'm a bit confused by
     that, but it seems to work right, so we take reciprocals of values and
     negate offsets. oh well.
     
   */
  {
    double a = (x2 - x1 == 0.) ? 0. : ((cyclic ? 3.0 : 2.0) / (x2 - x1));
    double c = (y2 - y1 == 0.) ? 0. : (1. / (y2 - y1));
    double dx = (x1 == 0.) ? 0. : 1. / x1;
    double dy = (y1 == 0.) ? 0. : 1. / y1;
    
    cairo_matrix_set_affine (mat,
			     a, 0.,
			     c, 0.,
			     dx, dy);
    
    cairo_surface_set_matrix (surf, mat);
    cairo_matrix_destroy (mat);
    cairo_surface_set_filter (surf, CAIRO_FILTER_BILINEAR);
  }

  /* FIXME: repeating gradients (not to mention hold gradients) don't seem to work. */
  /*   cairo_surface_set_repeat (surf, cyclic ? 1 : 0); */

  if (gr->pattern)
    cairo_pattern_destroy (gr->pattern);
  
  if (gr->pattern_surface)
    cairo_surface_destroy (gr->pattern_surface);

  if (gr->pattern_pixels)
    free (gr->pattern_pixels);
  
  gr->pattern_pixels = NULL;  
  gr->pattern_surface = surf;  
  gr->pattern = cairo_pattern_create_for_surface(surf);

  cairo_restore (gr->cr);    
  cairo_set_pattern (gr->cr, gr->pattern);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_setTexturePixels 
  (JNIEnv *env, jobject obj, jintArray jarr, jint w, jint h, jint stride)
{
  struct graphics2d *gr = NULL;
  jint *jpixels = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }
  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);

  if (gr->debug) printf ("setTexturePixels (%d pixels, %dx%d, stride: %d)\n",
			 (*env)->GetArrayLength (env, jarr), w, h, stride);

  if (gr->pattern)
    cairo_pattern_destroy (gr->pattern);

  if (gr->pattern_surface)
    cairo_surface_destroy (gr->pattern_surface);

  if (gr->pattern_pixels)
    free (gr->pattern_pixels);

  gr->pattern = NULL;
  gr->pattern_surface = NULL;
  gr->pattern_pixels = NULL;

  gr->pattern_pixels = (char *) malloc (h * stride * 4);
  g_assert (gr->pattern_pixels != NULL);

  jpixels = (*env)->GetIntArrayElements (env, jarr, NULL);
  g_assert (jpixels != NULL);
  memcpy (gr->pattern_pixels, jpixels, h * stride * 4);
  (*env)->ReleaseIntArrayElements (env, jarr, jpixels, 0);

  gr->pattern_surface = cairo_surface_create_for_image (gr->pattern_pixels, 
							CAIRO_FORMAT_ARGB32, 
							w, h, stride * 4);
  g_assert (gr->pattern_surface != NULL);
  cairo_surface_set_repeat (gr->pattern_surface, 1);
  gr->pattern = cairo_pattern_create_for_surface (gr->pattern_surface);
  g_assert (gr->pattern != NULL);
  cairo_set_pattern (gr->cr, gr->pattern);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_drawPixels 
  (JNIEnv *env, jobject obj, jintArray java_pixels, 
   jint w, jint h, jint stride, jdoubleArray java_matrix)
{
  struct graphics2d *gr = NULL;
  jint *native_pixels = NULL;
  jdouble *native_matrix = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);

  if (gr->debug) printf ("drawPixels (%d pixels, %dx%d, stride: %d)\n",
			 (*env)->GetArrayLength (env, java_pixels), w, h, stride);

  native_pixels = (*env)->GetIntArrayElements (env, java_pixels, NULL);
  native_matrix = (*env)->GetDoubleArrayElements (env, java_matrix, NULL);
  g_assert (native_pixels != NULL);
  g_assert (native_matrix != NULL);
  g_assert ((*env)->GetArrayLength (env, java_matrix) == 6);

  begin_drawing_operation (gr);
  
 {
   cairo_matrix_t *mat = NULL;
   cairo_surface_t *surf = cairo_surface_create_for_image ((char *)native_pixels, 
							   CAIRO_FORMAT_ARGB32, 
							   w, h, stride * 4);   
   mat = cairo_matrix_create ();
   cairo_matrix_set_affine (mat, 
			    native_matrix[0], native_matrix[1],
			    native_matrix[2], native_matrix[3],
			    native_matrix[4], native_matrix[5]);
   cairo_surface_set_matrix (surf, mat);
   cairo_surface_set_filter (surf, cairo_surface_get_filter(gr->surface));
   cairo_show_surface (gr->cr, surf, w, h);
   cairo_matrix_destroy (mat);
   cairo_surface_destroy (surf);
 }
  
 end_drawing_operation (gr);
 
 (*env)->ReleaseIntArrayElements (env, java_pixels, native_pixels, 0);
 (*env)->ReleaseDoubleArrayElements (env, java_matrix, native_matrix, 0);
 
  gdk_threads_leave();
}

JNIEXPORT jintArray JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_getImagePixels 
   (JNIEnv *env, jobject obj)
{
  struct graphics2d *gr = NULL;
  jintArray java_pixels;
  jint* native_pixels;
  GdkPixbuf *buf = NULL;
  gint width, height;
  gint bits_per_sample = 8;
  gboolean has_alpha = TRUE;
  gint total_channels = 4;
  jint i;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return NULL; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  
  if (gr->debug) printf ("getImagePixels\n");
  
  gdk_drawable_get_size (gr->drawable, &width, &height);
    
  buf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, has_alpha, 
                        bits_per_sample,
                        width, height);
  g_assert (buf != NULL);
  g_assert (gdk_pixbuf_get_bits_per_sample (buf) == bits_per_sample);
  g_assert (gdk_pixbuf_get_n_channels (buf) == total_channels);
  
      
  /* copy pixels from drawable to pixbuf */
  
  gdk_pixbuf_get_from_drawable (buf, gr->drawable,
                                NULL, 
                                0, 0, 0, 0,
                                width, height);
 								      				      
  native_pixels= gdk_pixbuf_get_pixels (buf);
  
#ifndef WORDS_BIGENDIAN
  /* convert pixels from 0xBBGGRRAA to 0xAARRGGBB */
  for (i=0; i<width * height; i++) 
    {	     
        native_pixels[i] = SWAPU32 ((unsigned)native_pixels[i]);
    }
#endif

   java_pixels = (*env) -> NewIntArray (env, width * height);   
   
   (*env)->SetIntArrayRegion(env, java_pixels, 
                            (jsize)0, (jsize) width*height, 
                            (jint*) native_pixels);
   
   gdk_threads_leave();
   return java_pixels;
}

/* passthrough methods to cairo */

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoSave 
   (JNIEnv *env, jobject obj)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_save\n");
  cairo_save (gr->cr);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoRestore 
   (JNIEnv *env, jobject obj)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_restore\n");
  cairo_restore (gr->cr);
  update_pattern_transform (gr);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoSetMatrix 
   (JNIEnv *env, jobject obj, jdoubleArray java_matrix)
{
  struct graphics2d *gr = NULL;
  jdouble *native_matrix = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);

  native_matrix = (*env)->GetDoubleArrayElements (env, java_matrix, NULL);  
  g_assert (native_matrix != NULL);
  g_assert ((*env)->GetArrayLength (env, java_matrix) == 6);

  if (gr->debug) printf ("cairo_set_matrix [ %f, %f, %f, %f, %f, %f ]\n",
			 native_matrix[0], native_matrix[1],
			 native_matrix[2], native_matrix[3],
			 native_matrix[4], native_matrix[5]);

  {
    cairo_matrix_t * mat = cairo_matrix_create ();
    cairo_matrix_set_affine (mat, 
			     native_matrix[0], native_matrix[1],
			     native_matrix[2], native_matrix[3],
			     native_matrix[4], native_matrix[5]);
    cairo_set_matrix (gr->cr, mat);
    cairo_matrix_destroy (mat);
  }

  (*env)->ReleaseDoubleArrayElements (env, java_matrix, native_matrix, 0);
  update_pattern_transform (gr);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoSetFont 
   (JNIEnv *env, jobject obj, jobject font)
{
  struct graphics2d *gr = NULL;
  struct peerfont *pfont = NULL;
  cairo_font_t *ft = NULL;
  FT_Face face = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);

  pfont = (struct peerfont *)NSA_GET_FONT_PTR (env, font);
  g_assert (pfont != NULL);

  face = pango_ft2_font_get_face (pfont->font);
  g_assert (face != NULL);

  ft = cairo_ft_font_create_for_ft_face (face);
  g_assert (ft != NULL);

  if (gr->debug) printf ("cairo_set_font '%s'\n", face->family_name);
  
  cairo_set_font (gr->cr, ft);

  cairo_scale_font (gr->cr, 
		    pango_font_description_get_size (pfont->desc) / 
		    (double)PANGO_SCALE);

  cairo_font_destroy (ft);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoShowGlyphs
   (JNIEnv *env, jobject obj, jintArray java_codes, jfloatArray java_posns)
{
  struct graphics2d *gr = NULL;
  cairo_glyph_t *glyphs = NULL;
  jfloat *native_posns = NULL;
  jint *native_codes = NULL;
  jint i;
  jint ncodes, nposns;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);

  native_codes = (*env)->GetIntArrayElements (env, java_codes, NULL);  
  native_posns = (*env)->GetFloatArrayElements (env, java_posns, NULL);  
  g_assert (native_codes != NULL);
  g_assert (native_posns != NULL);

  ncodes = (*env)->GetArrayLength (env, java_codes);
  nposns = (*env)->GetArrayLength (env, java_posns);
  g_assert (2 * ncodes == nposns);

  if (gr->debug) printf ("cairo_show_glyphs (%d glyphs)\n", ncodes);

  glyphs = malloc (sizeof(cairo_glyph_t) * ncodes);
  g_assert (glyphs);

  for (i = 0; i < ncodes; ++i)
    {
      glyphs[i].index = native_codes[i];
      glyphs[i].x = (double) native_posns[2*i];
      glyphs[i].y = (double) native_posns[2*i + 1];
      if (gr->debug) printf ("cairo_show_glyphs (glyph %d (code %d) : %f,%f)\n", 
			     i, glyphs[i].index, glyphs[i].x, glyphs[i].y);
    }

  (*env)->ReleaseIntArrayElements (env, java_codes, native_codes, 0);
  (*env)->ReleaseFloatArrayElements (env, java_posns, native_posns, 0);

  begin_drawing_operation (gr);
  cairo_show_glyphs (gr->cr, glyphs, ncodes);
  end_drawing_operation (gr);

  free(glyphs);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoSetOperator 
   (JNIEnv *env, jobject obj, jint op)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_set_operator %d\n", op);
  switch ((enum java_awt_alpha_composite_rule) op)
    {
    case java_awt_alpha_composite_CLEAR: 
      cairo_set_operator (gr->cr, CAIRO_OPERATOR_CLEAR);
      break;
      
    case java_awt_alpha_composite_SRC: 
      cairo_set_operator (gr->cr, CAIRO_OPERATOR_SRC);
      break;
      
    case java_awt_alpha_composite_SRC_OVER: 
      cairo_set_operator (gr->cr, CAIRO_OPERATOR_OVER);
      break;

    case java_awt_alpha_composite_DST_OVER: 
      cairo_set_operator (gr->cr, CAIRO_OPERATOR_OVER_REVERSE);
      break;

    case java_awt_alpha_composite_SRC_IN: 
      cairo_set_operator (gr->cr, CAIRO_OPERATOR_IN);
      break;

    case java_awt_alpha_composite_DST_IN: 
      cairo_set_operator (gr->cr, CAIRO_OPERATOR_IN_REVERSE);
      break;

    case java_awt_alpha_composite_SRC_OUT: 
      cairo_set_operator (gr->cr, CAIRO_OPERATOR_OUT);
      break;

    case java_awt_alpha_composite_DST_OUT: 
      cairo_set_operator (gr->cr, CAIRO_OPERATOR_OUT_REVERSE);
      break;

    case java_awt_alpha_composite_DST: 
      cairo_set_operator (gr->cr, CAIRO_OPERATOR_DST);
      break;

    case java_awt_alpha_composite_SRC_ATOP: 
      cairo_set_operator (gr->cr, CAIRO_OPERATOR_ATOP);
      break;

    case java_awt_alpha_composite_DST_ATOP: 
      cairo_set_operator (gr->cr, CAIRO_OPERATOR_ATOP_REVERSE);
      break;

    case java_awt_alpha_composite_XOR: 
      cairo_set_operator (gr->cr, CAIRO_OPERATOR_XOR);
      break;
    }
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoSetRGBColor 
   (JNIEnv *env, jobject obj, jdouble r, jdouble g, jdouble b)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);

  /* this is a very weird fact: GDK Pixbufs and RENDER drawables consider
     colors in opposite pixel order. I have no idea why.  thus when you
     draw to a PixBuf, you must exchange the R and B components of your
     color. */

  if (gr->debug) printf ("cairo_set_rgb_color (%f, %f, %f)\n", r, g, b);

  if (gr->drawbuf)
    cairo_set_rgb_color (gr->cr, b, g, r);
  else
    cairo_set_rgb_color (gr->cr, r, g, b);

  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoSetAlpha 
   (JNIEnv *env, jobject obj, jdouble a)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_set_alpha %f\n", a);
  cairo_set_alpha (gr->cr, a);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoSetFillRule 
   (JNIEnv *env, jobject obj, jint rule)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  if (gr->debug) printf ("cairo_set_fill_rule %d\n", rule);
  g_assert (gr != NULL);
  switch ((enum java_awt_geom_path_iterator_winding_rule) rule)
    {
    case java_awt_geom_path_iterator_WIND_NON_ZERO:
      cairo_set_fill_rule (gr->cr, CAIRO_FILL_RULE_WINDING);
      break;
    case java_awt_geom_path_iterator_WIND_EVEN_ODD:
      cairo_set_fill_rule (gr->cr, CAIRO_FILL_RULE_EVEN_ODD);
      break;
    }  
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoSetLineWidth 
   (JNIEnv *env, jobject obj, jdouble width)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_set_line_width %f\n", width);
  cairo_set_line_width (gr->cr, width);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoSetLineCap 
   (JNIEnv *env, jobject obj, jint cap)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_set_line_cap %d\n", cap);
  switch ((enum java_awt_basic_stroke_cap_rule) cap)
    {
    case java_awt_basic_stroke_CAP_BUTT: 
      cairo_set_line_cap (gr->cr, CAIRO_LINE_CAP_BUTT);
      break;

    case java_awt_basic_stroke_CAP_ROUND: 
      cairo_set_line_cap (gr->cr, CAIRO_LINE_CAP_ROUND);
      break;

    case java_awt_basic_stroke_CAP_SQUARE: 
      cairo_set_line_cap (gr->cr, CAIRO_LINE_CAP_SQUARE);
      break;
    }
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoSetLineJoin 
   (JNIEnv *env, jobject obj, jint join)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_set_line_join %d\n", join);
  switch ((enum java_awt_basic_stroke_join_rule) join)
    {
    case java_awt_basic_stroke_JOIN_MITER:
      cairo_set_line_join (gr->cr, CAIRO_LINE_JOIN_MITER);
      break;

    case java_awt_basic_stroke_JOIN_ROUND:
      cairo_set_line_join (gr->cr, CAIRO_LINE_JOIN_ROUND);
      break;

    case java_awt_basic_stroke_JOIN_BEVEL:
      cairo_set_line_join (gr->cr, CAIRO_LINE_JOIN_BEVEL);
      break;
    }
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoSetDash 
   (JNIEnv *env, jobject obj, jdoubleArray dashes, jint ndash, jdouble offset)
{
  struct graphics2d *gr = NULL;
  jdouble *dasharr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_set_dash\n");
  dasharr = (*env)->GetDoubleArrayElements (env, dashes, NULL);  
  g_assert (dasharr != NULL);
  cairo_set_dash (gr->cr, dasharr, ndash, offset);
  (*env)->ReleaseDoubleArrayElements (env, dashes, dasharr, 0);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoSetMiterLimit 
   (JNIEnv *env, jobject obj, jdouble miter)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_set_miter_limit %f\n", miter);
  cairo_set_miter_limit (gr->cr, miter);
  gdk_threads_leave();
}


JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoNewPath 
   (JNIEnv *env, jobject obj)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_new_path\n");
  cairo_new_path (gr->cr);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoMoveTo 
   (JNIEnv *env, jobject obj, jdouble x, jdouble y)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_move_to (%f, %f)\n", x, y);
  cairo_move_to (gr->cr, x, y);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoLineTo 
   (JNIEnv *env, jobject obj, jdouble x, jdouble y)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_line_to (%f, %f)\n", x, y);
  cairo_line_to (gr->cr, x, y);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoCurveTo 
   (JNIEnv *env, jobject obj, jdouble x1, jdouble y1, jdouble x2, jdouble y2, jdouble x3, jdouble y3)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_curve_to (%f, %f), (%f, %f), (%f, %f)\n", x1, y1, x2, y2, x3, y3);
  cairo_curve_to (gr->cr, x1, y1, x2, y2, x3, y3);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoRelMoveTo 
   (JNIEnv *env, jobject obj, jdouble dx, jdouble dy)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_rel_move_to (%f, %f)\n", dx, dy);
  cairo_rel_move_to (gr->cr, dx, dy);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoRelLineTo 
   (JNIEnv *env, jobject obj, jdouble dx, jdouble dy)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_rel_line_to (%f, %f)\n", dx, dy);
  cairo_rel_line_to (gr->cr, dx, dy);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoRelCurveTo 
   (JNIEnv *env, jobject obj, jdouble dx1, jdouble dy1, jdouble dx2, jdouble dy2, jdouble dx3, jdouble dy3)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_rel_curve_to (%f, %f), (%f, %f), (%f, %f)\n", dx1, dy1, dx2, dy2, dx3, dy3);
  cairo_rel_curve_to (gr->cr, dx1, dy1, dx2, dy2, dx3, dy3);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoRectangle 
   (JNIEnv *env, jobject obj, jdouble x, jdouble y, jdouble width, jdouble height)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_rectangle (%f, %f) (%f, %f)\n", x, y, width, height);
  cairo_rectangle (gr->cr, x, y, width, height);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoClosePath 
   (JNIEnv *env, jobject obj)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_close_path\n");
  cairo_close_path (gr->cr);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoStroke 
   (JNIEnv *env, jobject obj)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_stroke\n");
  begin_drawing_operation (gr);
  cairo_stroke (gr->cr);
  end_drawing_operation (gr);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoFill 
   (JNIEnv *env, jobject obj)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_fill\n");
  begin_drawing_operation (gr);
  cairo_fill (gr->cr);
  end_drawing_operation (gr);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoClip 
   (JNIEnv *env, jobject obj)
{
  struct graphics2d *gr = NULL;

  gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

  gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
  g_assert (gr != NULL);
  if (gr->debug) printf ("cairo_clip\n");
  begin_drawing_operation (gr);
  cairo_init_clip (gr->cr);
  cairo_clip (gr->cr);
  end_drawing_operation (gr);
  gdk_threads_leave();
}

JNIEXPORT void JNICALL Java_gnu_java_awt_peer_gtk_GdkGraphics2D_cairoSurfaceSetFilter
   (JNIEnv *env, jobject obj, jint filter)
{
   struct graphics2d *gr = NULL;   

   gdk_threads_enter();
  if (peer_is_disposed(env, obj)) { gdk_threads_leave(); return; }

   gr = (struct graphics2d *) NSA_GET_G2D_PTR (env, obj);
   g_assert (gr != NULL);
   if (gr->debug) printf ("cairo_surface_set_filter %d\n", filter);   
   switch ((enum java_awt_rendering_hints_filter) filter)
     {
     case java_awt_rendering_hints_VALUE_INTERPOLATION_NEAREST_NEIGHBOR:
       cairo_surface_set_filter (gr->surface, CAIRO_FILTER_NEAREST);
       break;
     case java_awt_rendering_hints_VALUE_INTERPOLATION_BILINEAR:
       cairo_surface_set_filter (gr->surface, CAIRO_FILTER_BILINEAR);
       break; 
     case java_awt_rendering_hints_VALUE_ALPHA_INTERPOLATION_SPEED:
       cairo_surface_set_filter (gr->surface, CAIRO_FILTER_FAST);
       break;
     case java_awt_rendering_hints_VALUE_ALPHA_INTERPOLATION_DEFAULT:
       cairo_surface_set_filter (gr->surface, CAIRO_FILTER_NEAREST);
       break;
     case java_awt_rendering_hints_VALUE_ALPHA_INTERPOLATION_QUALITY:
       cairo_surface_set_filter (gr->surface, CAIRO_FILTER_BEST);
       break;
     }
   gdk_threads_leave();
}
