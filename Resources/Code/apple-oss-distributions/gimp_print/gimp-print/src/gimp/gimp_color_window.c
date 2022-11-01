/*
 * "$Id: gimp_color_window.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
 *
 *   Main window code for Print plug-in for the GIMP.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com),
 *	Robert Krawitz (rlk@alum.mit.edu), Steve Miller (smiller@rni.net)
 *      and Michael Natterer (mitch@gimp.org)
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "../../lib/libprintut.h"

#include "print_gimp.h"

#include "print-intl.h"
#include <string.h>

gint    thumbnail_w, thumbnail_h, thumbnail_bpp;
guchar *thumbnail_data;
gint    adjusted_thumbnail_bpp;
guchar *adjusted_thumbnail_data;

GtkWidget *gimp_color_adjust_dialog;

static GtkObject *brightness_adjustment;
static GtkObject *saturation_adjustment;
static GtkObject *density_adjustment;
static GtkObject *contrast_adjustment;
static GtkObject *cyan_adjustment;
static GtkObject *magenta_adjustment;
static GtkObject *yellow_adjustment;
static GtkObject *gamma_adjustment;

GtkWidget   *dither_algo_combo       = NULL;
static gint  dither_algo_callback_id = -1;

static void gimp_brightness_update  (GtkAdjustment *adjustment);
static void gimp_saturation_update  (GtkAdjustment *adjustment);
static void gimp_density_update     (GtkAdjustment *adjustment);
static void gimp_contrast_update    (GtkAdjustment *adjustment);
static void gimp_cyan_update        (GtkAdjustment *adjustment);
static void gimp_magenta_update     (GtkAdjustment *adjustment);
static void gimp_yellow_update      (GtkAdjustment *adjustment);
static void gimp_gamma_update       (GtkAdjustment *adjustment);
static void gimp_set_color_defaults (void);

static void gimp_dither_algo_callback (GtkWidget *widget,
				       gpointer   data);


void gimp_build_dither_combo               (void);

static GtkDrawingArea *swatch = NULL;

#define SWATCH_W (128)
#define SWATCH_H (128)

static char *
c_strdup(const char *s)
{
  char *ret = malloc(strlen(s) + 1);
  if (!s)
    exit(1);
  strcpy(ret, s);
  return ret;
}

static void
gimp_dither_algo_callback (GtkWidget *widget,
			   gpointer   data)
{
  const gchar *new_algo =
    gtk_entry_get_text (GTK_ENTRY (GTK_COMBO (dither_algo_combo)->entry));
  int i;

  for (i = 0; i < stp_dither_algorithm_count (); i ++)
    if (strcasecmp (new_algo, stp_dither_algorithm_text (i)) == 0)
      {
        stp_set_dither_algorithm (*pv, stp_dither_algorithm_name (i));
        break;
      }
}

void
gimp_build_dither_combo (void)
{
  int i;
  stp_param_t *vec = xmalloc(sizeof(stp_param_t) * stp_dither_algorithm_count());

  for (i = 0; i < stp_dither_algorithm_count(); i++)
    {
      vec[i].name = c_strdup (stp_dither_algorithm_name (i));
      vec[i].text = c_strdup (stp_dither_algorithm_text (i));
    }

  gimp_plist_build_combo (dither_algo_combo,
			  stp_dither_algorithm_count (),
			  vec,
			  stp_get_dither_algorithm (*pv),
			  stp_default_dither_algorithm (),
			  &gimp_dither_algo_callback,
			  &dither_algo_callback_id);

  for (i = 0; i < stp_dither_algorithm_count (); i++)
    {
      free ((void *) vec[i].name);
      free ((void *) vec[i].text);
    }
  free (vec);
}

void
gimp_redraw_color_swatch (void)
{
  static GdkGC *gc = NULL;
  static GdkColormap *cmap;

  if (swatch == NULL || swatch->widget.window == NULL)
    return;

#if 0
  gdk_window_clear (swatch->widget.window);
#endif

  if (gc == NULL)
    {
      gc = gdk_gc_new (swatch->widget.window);
      cmap = gtk_widget_get_colormap (GTK_WIDGET(swatch));
    }

  (adjusted_thumbnail_bpp == 1
   ? gdk_draw_gray_image
   : gdk_draw_rgb_image) (swatch->widget.window, gc,
			  (SWATCH_W - thumbnail_w) / 2,
			  (SWATCH_H - thumbnail_h) / 2,
			  thumbnail_w, thumbnail_h, GDK_RGB_DITHER_NORMAL,
			  adjusted_thumbnail_data,
			  adjusted_thumbnail_bpp * thumbnail_w);
}

/*
 * gimp_create_color_adjust_window (void)
 *
 * NOTES:
 *   creates the color adjuster popup, allowing the user to adjust brightness,
 *   contrast, saturation, etc.
 */
void
gimp_create_color_adjust_window (void)
{
  GtkWidget *table;
  GtkWidget *event_box;
  const stp_vars_t lower   = stp_minimum_settings ();
  const stp_vars_t upper   = stp_maximum_settings ();
  const stp_vars_t defvars = stp_default_settings ();

  /*
   * Fetch a thumbnail of the image we're to print from the Gimp.  This must
   *
   */

  thumbnail_w = THUMBNAIL_MAXW;
  thumbnail_h = THUMBNAIL_MAXH;
  thumbnail_data = gimp_image_get_thumbnail_data (image_ID, &thumbnail_w,
                                                  &thumbnail_h, &thumbnail_bpp);

  /*
   * thumbnail_w and thumbnail_h have now been adjusted to the actual
   * thumbnail dimensions.  Now initialize a color-adjusted version of
   * the thumbnail.
   */

  adjusted_thumbnail_data = g_malloc (3 * thumbnail_w * thumbnail_h);

  gimp_color_adjust_dialog =
    gimp_dialog_new (_("Print Color Adjust"), "print",
		     gimp_standard_help_func, "filters/print.html",
		     GTK_WIN_POS_MOUSE, FALSE, TRUE, FALSE,

		     _("Set Defaults"), gimp_set_color_defaults,
		     NULL, NULL, NULL, FALSE, FALSE,
		     _("Close"), gtk_widget_hide,
		     NULL, 1, NULL, TRUE, TRUE,

		     NULL);

  table = gtk_table_new (10, 3, FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (table), 6);
  gtk_table_set_col_spacings (GTK_TABLE (table), 4);
  gtk_table_set_row_spacings (GTK_TABLE (table), 2);
  gtk_table_set_row_spacing (GTK_TABLE (table), 2, 6);
  gtk_table_set_row_spacing (GTK_TABLE (table), 5, 6);
  gtk_table_set_row_spacing (GTK_TABLE (table), 8, 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (gimp_color_adjust_dialog)->vbox),
		      table, FALSE, FALSE, 0);
  gtk_widget_show (table);

  /*
   * Drawing area for color swatch feedback display...
   */

  event_box = gtk_event_box_new ();
  gtk_widget_show (GTK_WIDGET (event_box));
  gtk_table_attach (GTK_TABLE (table), GTK_WIDGET (event_box),
                    0, 3, 0, 1, 0, 0, 0, 0);

  swatch = (GtkDrawingArea *) gtk_drawing_area_new ();
  gtk_widget_set_events (GTK_WIDGET (swatch), GDK_EXPOSURE_MASK);
  gtk_drawing_area_size (swatch, SWATCH_W, SWATCH_H);
  gtk_container_add (GTK_CONTAINER (event_box), GTK_WIDGET (swatch));
  gtk_widget_show (GTK_WIDGET (swatch));

  gimp_help_set_help_data (GTK_WIDGET (event_box), _("Image preview"), NULL);
  gtk_signal_connect (GTK_OBJECT (swatch), "expose_event",
                      GTK_SIGNAL_FUNC (gimp_redraw_color_swatch),
                      NULL);

  /*
   * Brightness slider...
   */

  brightness_adjustment =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 1, _("Brightness:"), 200, 0,
                          stp_get_brightness (defvars),
			  stp_get_brightness (lower),
			  stp_get_brightness (upper),
			  stp_get_brightness (defvars) / 100,
			  stp_get_brightness (defvars) / 10,
			  3, TRUE, 0, 0, NULL, NULL);
  set_adjustment_tooltip (brightness_adjustment,
                          _("Set the brightness of the print.\n"
                            "0 is solid black, 2 is solid white"),
                          NULL);
  gtk_signal_connect (GTK_OBJECT (brightness_adjustment), "value_changed",
                      GTK_SIGNAL_FUNC (gimp_brightness_update),
                      NULL);

  /*
   * Contrast slider...
   */

  contrast_adjustment =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 2, _("Contrast:"), 200, 0,
                          stp_get_contrast (defvars),
			  stp_get_contrast (lower),
			  stp_get_contrast (upper),
			  stp_get_contrast (defvars) / 100,
			  stp_get_contrast (defvars) / 10,
			  3, TRUE, 0, 0, NULL, NULL);
  set_adjustment_tooltip (contrast_adjustment,
                          _("Set the contrast of the print"),
                          NULL);
  gtk_signal_connect (GTK_OBJECT (contrast_adjustment), "value_changed",
                      GTK_SIGNAL_FUNC (gimp_contrast_update),
                      NULL);

  /*
   * Cyan slider...
   */

  cyan_adjustment =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 3, _("Cyan:"), 200, 0,
                          stp_get_cyan (defvars),
			  stp_get_cyan (lower),
			  stp_get_cyan (upper),
			  stp_get_cyan (defvars) / 100,
			  stp_get_cyan (defvars) / 10,
			  3, TRUE, 0, 0, NULL, NULL);
  set_adjustment_tooltip (cyan_adjustment,
                          _("Adjust the cyan balance of the print"),
                          NULL);
  gtk_signal_connect (GTK_OBJECT (cyan_adjustment), "value_changed",
                      GTK_SIGNAL_FUNC (gimp_cyan_update),
                      NULL);

  /*
   * Magenta slider...
   */

  magenta_adjustment =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 4, _("Magenta:"), 200, 0,
                          stp_get_magenta (defvars),
			  stp_get_magenta (lower),
			  stp_get_magenta (upper),
			  stp_get_magenta (defvars) / 100,
			  stp_get_magenta (defvars) / 10,
			  3, TRUE, 0, 0, NULL, NULL);
  set_adjustment_tooltip (magenta_adjustment,
                          _("Adjust the magenta balance of the print"),
                          NULL);
  gtk_signal_connect (GTK_OBJECT (magenta_adjustment), "value_changed",
                      GTK_SIGNAL_FUNC (gimp_magenta_update),
                      NULL);

  /*
   * Yellow slider...
   */

  yellow_adjustment =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 5, _("Yellow:"), 200, 0,
                          stp_get_yellow (defvars),
			  stp_get_yellow (lower),
			  stp_get_yellow (upper),
			  stp_get_yellow (defvars) / 100,
			  stp_get_yellow (defvars) / 10,
			  3, TRUE, 0, 0, NULL, NULL);
  set_adjustment_tooltip (yellow_adjustment,
                          _("Adjust the yellow balance of the print"),
                          NULL);
  gtk_signal_connect (GTK_OBJECT (yellow_adjustment), "value_changed",
                      GTK_SIGNAL_FUNC (gimp_yellow_update),
                      NULL);

  /*
   * Saturation slider...
   */

  saturation_adjustment =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 6, _("Saturation:"), 200, 0,
                          stp_get_saturation (defvars),
			  stp_get_saturation (lower),
			  stp_get_saturation (upper),
			  stp_get_saturation (defvars) / 1000,
			  stp_get_saturation (defvars) / 100,
			  3, TRUE, 0, 0, NULL, NULL);
  set_adjustment_tooltip (saturation_adjustment,
                          _("Adjust the saturation (color balance) of the print\n"
                            "Use zero saturation to produce grayscale output "
                            "using color and black inks"),
                          NULL);
  gtk_signal_connect (GTK_OBJECT (saturation_adjustment), "value_changed",
                      GTK_SIGNAL_FUNC (gimp_saturation_update),
                      NULL);

  /*
   * Density slider...
   */

  density_adjustment =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 7, _("Density:"), 200, 0,
                          stp_get_density (defvars),
			  stp_get_density (lower),
			  stp_get_density (upper),
			  stp_get_density (defvars) / 1000,
			  stp_get_density (defvars) / 100,
			  3, TRUE, 0, 0, NULL, NULL);
  set_adjustment_tooltip (density_adjustment,
                          _("Adjust the density (amount of ink) of the print. "
                            "Reduce the density if the ink bleeds through the "
                            "paper or smears; increase the density if black "
                            "regions are not solid."),
                          NULL);
  gtk_signal_connect (GTK_OBJECT (density_adjustment), "value_changed",
                      GTK_SIGNAL_FUNC (gimp_density_update),
                      NULL);

  /*
   * Gamma slider...
   */

  gamma_adjustment =
    gimp_scale_entry_new (GTK_TABLE (table), 0, 8, _("Gamma:"), 200, 0,
                          stp_get_gamma (defvars),
			  stp_get_gamma (lower),
			  stp_get_gamma (upper),
			  stp_get_gamma (defvars) / 1000,
			  stp_get_gamma (defvars) / 100,
			  3, TRUE, 0, 0, NULL, NULL);
  set_adjustment_tooltip (gamma_adjustment,
                          _("Adjust the gamma of the print. Larger values will "
                            "produce a generally brighter print, while smaller "
                            "values will produce a generally darker print. "
                            "Black and white will remain the same, unlike with "
                            "the brightness adjustment."),
                          NULL);
  gtk_signal_connect (GTK_OBJECT (gamma_adjustment), "value_changed",
                      GTK_SIGNAL_FUNC (gimp_gamma_update),
                      NULL);

  /*
   * Dither algorithm option combo...
   */

  event_box = gtk_event_box_new ();
  gimp_table_attach_aligned (GTK_TABLE (table), 0, 9,
                             _("Dither Algorithm:"), 1.0, 0.5,
                             event_box, 1, TRUE);

  dither_algo_combo = gtk_combo_new ();
  gtk_container_add (GTK_CONTAINER(event_box), dither_algo_combo);
  gtk_widget_show (dither_algo_combo);

  gimp_help_set_help_data (GTK_WIDGET (event_box),
                           _("Choose the dither algorithm to be used.\n"
                             "Adaptive Hybrid usually produces the best "
                             "all-around quality.\n"
                             "Ordered is faster and produces almost as good "
                             "quality on photographs.\n"
                             "Fast and Very Fast are considerably faster, and "
                             "work well for text and line art.\n"
                             "Hybrid Floyd-Steinberg generally produces "
                             "inferior output."),
                           NULL);

  gimp_build_dither_combo ();
}

static void
gimp_brightness_update (GtkAdjustment *adjustment)
{
  gimp_invalidate_preview_thumbnail ();
  if (stp_get_brightness (*pv) != adjustment->value)
    {
      stp_set_brightness (*pv, adjustment->value);
      gimp_update_adjusted_thumbnail ();
    }
}

static void
gimp_contrast_update (GtkAdjustment *adjustment)
{
  gimp_invalidate_preview_thumbnail ();

  if (stp_get_contrast (*pv) != adjustment->value)
    {
      stp_set_contrast (*pv, adjustment->value);
      gimp_update_adjusted_thumbnail ();
    }
}

static void
gimp_cyan_update (GtkAdjustment *adjustment)
{
  gimp_invalidate_preview_thumbnail ();

  if (stp_get_cyan (*pv) != adjustment->value)
    {
      stp_set_cyan (*pv, adjustment->value);
      gimp_update_adjusted_thumbnail ();
    }
}

static void
gimp_magenta_update (GtkAdjustment *adjustment)
{
  gimp_invalidate_preview_thumbnail ();

  if (stp_get_magenta (*pv) != adjustment->value)
    {
      stp_set_magenta (*pv, adjustment->value);
      gimp_update_adjusted_thumbnail ();
    }
}

static void
gimp_yellow_update (GtkAdjustment *adjustment)
{
  gimp_invalidate_preview_thumbnail ();

  if (stp_get_yellow (*pv) != adjustment->value)
    {
      stp_set_yellow (*pv, adjustment->value);
      gimp_update_adjusted_thumbnail ();
    }
}

static void
gimp_saturation_update (GtkAdjustment *adjustment)
{
  gimp_invalidate_preview_thumbnail ();

  if (stp_get_saturation (*pv) != adjustment->value)
    {
      stp_set_saturation (*pv, adjustment->value);
      gimp_update_adjusted_thumbnail ();
    }
}

static void
gimp_density_update (GtkAdjustment *adjustment)
{
  if (stp_get_density (*pv) != adjustment->value)
    {
      stp_set_density (*pv, adjustment->value);
    }
}

static void
gimp_gamma_update (GtkAdjustment *adjustment)
{
  gimp_invalidate_preview_thumbnail ();

  if (stp_get_gamma (*pv) != adjustment->value)
    {
      stp_set_gamma (*pv, adjustment->value);
      gimp_update_adjusted_thumbnail ();
    }
}

static void
gimp_set_adjustment_active (GtkObject *adj,
                            gboolean   active)
{
  gtk_widget_set_sensitive (GTK_WIDGET (GIMP_SCALE_ENTRY_LABEL (adj)), active);
  gtk_widget_set_sensitive (GTK_WIDGET (GIMP_SCALE_ENTRY_SCALE (adj)), active);
  gtk_widget_set_sensitive (GTK_WIDGET (GIMP_SCALE_ENTRY_SPINBUTTON (adj)),
                            active);
}

void
gimp_set_color_sliders_active (gboolean active)
{
  gimp_set_adjustment_active (cyan_adjustment, active);
  gimp_set_adjustment_active (magenta_adjustment, active);
  gimp_set_adjustment_active (yellow_adjustment, active);
  gimp_set_adjustment_active (saturation_adjustment, active);
}

void
gimp_do_color_updates (void)
{
  gtk_adjustment_set_value (GTK_ADJUSTMENT (brightness_adjustment),
			    stp_get_brightness (*pv));

  gtk_adjustment_set_value (GTK_ADJUSTMENT (gamma_adjustment),
			    stp_get_gamma (*pv));

  gtk_adjustment_set_value (GTK_ADJUSTMENT (contrast_adjustment),
			    stp_get_contrast (*pv));

  gtk_adjustment_set_value (GTK_ADJUSTMENT (cyan_adjustment),
			    stp_get_cyan (*pv));

  gtk_adjustment_set_value (GTK_ADJUSTMENT (magenta_adjustment),
			    stp_get_magenta (*pv));

  gtk_adjustment_set_value (GTK_ADJUSTMENT (yellow_adjustment),
			    stp_get_yellow (*pv));

  gtk_adjustment_set_value (GTK_ADJUSTMENT (saturation_adjustment),
			    stp_get_saturation (*pv));

  gtk_adjustment_set_value (GTK_ADJUSTMENT (density_adjustment),
			    stp_get_density (*pv));

  gimp_update_adjusted_thumbnail ();
}

void
gimp_set_color_defaults (void)
{
  const stp_vars_t defvars = stp_default_settings ();

  stp_set_brightness (*pv, stp_get_brightness (defvars));
  stp_set_gamma (*pv, stp_get_gamma (defvars));
  stp_set_contrast (*pv, stp_get_contrast (defvars));
  stp_set_cyan (*pv, stp_get_cyan (defvars));
  stp_set_magenta (*pv, stp_get_magenta (defvars));
  stp_set_yellow (*pv, stp_get_yellow (defvars));
  stp_set_saturation (*pv, stp_get_saturation (defvars));
  stp_set_density (*pv, stp_get_density (defvars));

  gimp_do_color_updates ();
}
