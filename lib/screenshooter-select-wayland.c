/*  $Id$
 *
 *  Copyright © 2025 André Miranda <andreldm@xfce.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */



#include "screenshooter-select-wayland.h"

#include <gtk-layer-shell.h>
#include <libxfce4util/libxfce4util.h>

#include "gdk/gdk.h"
#include "glib-object.h"
#include "glib.h"
#include "gtk/gtk.h"
#include "screenshooter-utils.h"



#define BACKGROUND_TRANSPARENCY 0.4

enum {
  ANCHOR_UNSET = 0,
  ANCHOR_NONE = 1,
  ANCHOR_TOP = 2,
  ANCHOR_LEFT = 4
};

typedef struct
{
  gboolean left_pressed;
  gboolean rubber_banding;
  gboolean cancelled;
  gboolean move_rectangle;
  gint anchor;
  gint x;
  gint y;
  gint x_root;
  gint y_root;
  cairo_rectangle_int_t rectangle;
  GSList *overlays;
  GtkWidget *size_window;
  GtkWidget *size_label;
} RubberBandData;

/* Overlay window data */
typedef struct
{
  GtkWidget *window;
  GdkMonitor *monitor;
  GdkRectangle monitor_geometry;
} OverlayData;



static OverlayData* lookup_overlay (GtkWidget      *widget,
                                    RubberBandData *rbdata)
{
  GSList *l;
  OverlayData* result = NULL;

  for (l = rbdata->overlays; l != NULL; l = l->next)
    {
      OverlayData *data = l->data;
      if (GTK_WIDGET (data->window) == widget)
        {
          result = l->data;
          break;
        }
    }

  if (result == NULL)
    {
      screenshooter_error (_("Failed to find overlay info"));
      g_abort ();
    }

  return result;
}



static void destroy_overlay (OverlayData *overlay)
{
  gtk_widget_destroy (overlay->window);
  g_free (overlay);
}



/* Callbacks for the rubber banding function */
static gboolean cb_key_pressed (GtkWidget      *widget,
                                GdkEventKey    *event,
                                RubberBandData *rbdata)
{
  guint key = event->keyval;

  if (key == GDK_KEY_Escape)
    {
      rbdata->cancelled = TRUE;
      gtk_main_quit ();
      return TRUE;
    }

  if (rbdata->left_pressed && (key == GDK_KEY_Control_L || key == GDK_KEY_Control_R))
    {
      rbdata->move_rectangle = TRUE;
      return TRUE;
    }

  return FALSE;
}



static gboolean cb_key_released (GtkWidget      *widget,
                                 GdkEventKey    *event,
                                 RubberBandData *rbdata)
{
  guint key = event->keyval;

  if (rbdata->left_pressed && (key == GDK_KEY_Control_L || key == GDK_KEY_Control_R))
    {
      rbdata->move_rectangle = FALSE;
      rbdata->anchor = ANCHOR_UNSET;
      return TRUE;
    }

  return FALSE;
}



static gboolean cb_draw (GtkWidget      *widget,
                         cairo_t        *cr,
                         RubberBandData *rbdata)
{
  /* Draw the transparent background */
  cairo_set_source_rgba (cr, 0, 0, 0, BACKGROUND_TRANSPARENCY);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);

  if (rbdata->rubber_banding)
    {
      OverlayData *overlay;
      cairo_rectangle_int_t intersection;
      cairo_rectangle_int_t selection_rect_local = rbdata->rectangle;

      overlay = lookup_overlay (widget, rbdata);

      /* Translate global selection rectangle to local window coordinates */
      selection_rect_local.x -= overlay->monitor_geometry.x;
      selection_rect_local.y -= overlay->monitor_geometry.y;

      /* Draw the selection if intersects with current window/monitor */
      if (gdk_rectangle_intersect ((GdkRectangle*) &selection_rect_local,
                                   &(GdkRectangle) {0, 0, overlay->monitor_geometry.width, overlay->monitor_geometry.height},
                                   &intersection))
        {
          /* First, clear the area of the selection from the dark overlay */
          cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
          gdk_cairo_rectangle (cr, &intersection);
          cairo_fill (cr);

          /* Then, draw the semi-transparent white rectangle inside */
          cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
          cairo_set_source_rgba (cr, 1.0f, 1.0f, 1.0f, 0.1f);
          gdk_cairo_rectangle (cr, &intersection);
          cairo_fill(cr);
        }
    }

  return FALSE;
}



static gboolean cb_button_pressed (GtkWidget *widget,
                                   GdkEventButton *event,
                                   RubberBandData *rbdata)
{
  if (event->button == 1)
    {
      OverlayData *overlay;

      TRACE ("Left button pressed");

      overlay = lookup_overlay (widget, rbdata);

      rbdata->left_pressed = TRUE;
      rbdata->x_root = overlay->monitor_geometry.x + event->x;
      rbdata->y_root = overlay->monitor_geometry.y + event->y;

      return TRUE;
    }

  return FALSE;
}



static gboolean cb_button_released (GtkWidget *widget,
                                    GdkEventButton *event,
                                    RubberBandData *rbdata)
{
  if (event->button == 1)
    {
      if (rbdata->rubber_banding && rbdata->rectangle.width > 0 && rbdata->rectangle.height > 0)
        {
          gtk_widget_destroy (rbdata->size_window);
          rbdata->size_window = NULL;
          gtk_main_quit ();
          return TRUE;
        }
      rbdata->left_pressed = rbdata->rubber_banding = FALSE;
    }

  return FALSE;
}



static gboolean cb_size_window_draw (GtkWidget *widget,
                                     cairo_t *cr,
                                     gpointer user_data)
{
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba (cr, 1.0, 1.0, 1.0, 0.0);
  cairo_paint (cr);

  return FALSE;
}



static void size_window_get_offset (GtkWidget *widget,
                                    gint digits,
                                    gint digit_width,
                                    gint line_height,
                                    gint x_event,
                                    gint y_event,
                                    gint *x_offset,
                                    gint *y_offset)
{
  GdkRectangle geometry;
  gint relative_x, relative_y, text_width;

  GdkDisplay *display = gtk_widget_get_display (widget);
  GdkMonitor *monitor = gdk_display_get_monitor_at_point (display, x_event, y_event);
  gdk_monitor_get_geometry (monitor, &geometry);

  relative_x = x_event - geometry.x;
  relative_y = y_event - geometry.y;

  *x_offset = -2;
  *y_offset = -4;

  /* Add 3/4 of a digit width as right padding */
  text_width = (digits + 0.75) * digit_width;
  /* Add 10% of line height as bottom padding */
  line_height = line_height * 1.1;

  if (relative_x > geometry.width - text_width)
    *x_offset -= text_width;

  if (relative_y > geometry.height - line_height)
    *y_offset -= line_height;
}



static void create_size_window (RubberBandData *rbdata)
{
  GtkWidget *window, *label;
  GtkCssProvider *css_provider;
  GtkStyleContext *context;
  GdkVisual *visual;

  rbdata->size_window = window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_layer_init_for_window (GTK_WINDOW (window));
  gtk_layer_set_layer (GTK_WINDOW (window), GTK_LAYER_SHELL_LAYER_OVERLAY);
  gtk_layer_set_anchor (GTK_WINDOW (window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
  gtk_layer_set_anchor (GTK_WINDOW (window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_window_set_default_size (GTK_WINDOW (window), 100, 50);
  gtk_widget_set_size_request (GTK_WIDGET (window), 100, 50);
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  gtk_widget_set_app_paintable (GTK_WIDGET (window), TRUE);
  gtk_window_set_skip_taskbar_hint (GTK_WINDOW (window), FALSE);
  g_signal_connect (G_OBJECT (window), "draw",
                    G_CALLBACK (cb_size_window_draw), NULL);

  visual = gdk_screen_get_rgba_visual (gtk_widget_get_screen (window));
  gtk_widget_set_visual (window, visual);

  rbdata->size_label = label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_widget_set_valign (label, GTK_ALIGN_START);
  gtk_widget_set_margin_start (label, 6);
  gtk_widget_set_margin_top (label, 6);
  gtk_container_add (GTK_CONTAINER (window), label);

  css_provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (css_provider,
    "label { font-family: monospace; color: #fff; text-shadow: 1px 1px 0px black; }", -1, NULL);

  context = gtk_widget_get_style_context (GTK_WIDGET (label));
  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (css_provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (css_provider);

  gtk_widget_show_all (GTK_WIDGET (window));
}



static gboolean cb_motion_notify (GtkWidget *widget,
                                  GdkEventMotion *event,
                                  RubberBandData *rbdata)
{
  gchar *coords;
  gint rect_width, rect_height, x_offset, y_offset;
  static gint digit_width = -1, line_height = -1;

  if (rbdata->left_pressed)
    {
      cairo_rectangle_int_t *new_rect;
      cairo_rectangle_int_t old_rect, intersect;
      cairo_region_t *region;
      OverlayData *overlay;
      gint event_x_root, event_y_root;
      GSList *l;

      TRACE ("Mouse is moving with left button pressed");

      overlay = lookup_overlay (widget, rbdata);
      /* The event x and y under wayland are relative to the monitor */
      event_x_root = overlay->monitor_geometry.x + event->x;
      event_y_root = overlay->monitor_geometry.y + event->y;
      new_rect = &rbdata->rectangle;

      if (!rbdata->rubber_banding)
        {
          /* This is the start of a rubber banding */
          rbdata->rubber_banding = TRUE;
          rbdata->x = event_x_root;
          rbdata->y = event_y_root;
          old_rect.x = rbdata->x;
          old_rect.y = rbdata->y;
          old_rect.height = old_rect.width = 1;

          gtk_layer_set_monitor (GTK_WINDOW (rbdata->size_window), overlay->monitor);
        }
      else
        {
          /* Rubber banding has already started, update it */
          old_rect.x = new_rect->x;
          old_rect.y = new_rect->y;
          old_rect.width = new_rect->width;
          old_rect.height = new_rect->height;
        }

      if (rbdata->move_rectangle)
        {
          /* Define the anchor right after the control key is pressed */
          if (rbdata->anchor == ANCHOR_UNSET)
            {
              rbdata->anchor = ANCHOR_NONE;
              rbdata->anchor |= (event_x_root < rbdata->x) ? ANCHOR_LEFT : 0;
              rbdata->anchor |= (event_y_root < rbdata->y) ? ANCHOR_TOP : 0;
            }

          /* Do not resize, instead move the rubber banding rectangle around */
          if (rbdata->anchor & ANCHOR_LEFT)
            rbdata->x = (new_rect->x = event_x_root) + new_rect->width;
          else
            rbdata->x = new_rect->x = event_x_root - new_rect->width;

          if (rbdata->anchor & ANCHOR_TOP)
            rbdata->y = (new_rect->y = event_y_root) + new_rect->height;
          else
            rbdata->y = new_rect->y = event_y_root - new_rect->height;
        }
      else
        {
          /* Get the new rubber banding rectangle */
          new_rect->x = MIN (rbdata->x, event_x_root);
          new_rect->y = MIN (rbdata->y, event_y_root);
          new_rect->width = ABS (rbdata->x - event_x_root) + 1;
          new_rect->height = ABS (rbdata->y - event_y_root) + 1;
        }

      rect_width = new_rect->width;
      rect_height = new_rect->height;

      /* Take into account when the rectangle is moved out of screen */
      if (new_rect->x < 0) rect_width += new_rect->x;
      if (new_rect->y < 0) rect_height += new_rect->y;

      coords = g_strdup_printf ("%d x %d", rect_width, rect_height);

      if (digit_width == -1)
        {
          PangoLayout *layout;
          PangoContext *context;
          PangoFontMetrics *metrics;

          layout = gtk_label_get_layout (GTK_LABEL (rbdata->size_label));
          context = pango_layout_get_context (layout);
          metrics = pango_context_get_metrics (context,
                                               pango_context_get_font_description (context),
                                               pango_context_get_language (context));

          digit_width = PANGO_PIXELS_CEIL (pango_font_metrics_get_approximate_digit_width (metrics));
          line_height = PANGO_PIXELS_CEIL (pango_font_metrics_get_height (metrics));

          pango_font_metrics_unref (metrics);
        }

      size_window_get_offset (rbdata->size_window, strlen (coords),
                              digit_width, line_height,
                              event->x, event->y,
                              &x_offset, &y_offset);

      gtk_layer_set_margin (GTK_WINDOW (rbdata->size_window), GTK_LAYER_SHELL_EDGE_LEFT, event->x + x_offset);
      gtk_layer_set_margin (GTK_WINDOW (rbdata->size_window), GTK_LAYER_SHELL_EDGE_TOP, event->y + y_offset);

      gtk_label_set_text (GTK_LABEL (rbdata->size_label), coords);
      g_free (coords);

      region = cairo_region_create_rectangle (&old_rect);
      cairo_region_union_rectangle (region, new_rect);

      /* Try to be smart: don't send the expose event for regions which
       * have already been painted */
      if (gdk_rectangle_intersect (&old_rect, new_rect, &intersect)
          && intersect.width > 2 && intersect.height > 2)
        {
          cairo_region_t *region_intersect;

          intersect.x += 1;
          intersect.width -= 2;
          intersect.y += 1;
          intersect.height -= 2;

          region_intersect = cairo_region_create_rectangle (&intersect);
          cairo_region_subtract (region, region_intersect);
          cairo_region_destroy (region_intersect);
        }

      for (l = rbdata->overlays; l != NULL; l = l->next)
        {
          OverlayData *data = l->data;

          /* Compensate monitor offset */
          cairo_region_translate (region, -(data->monitor_geometry.x), -(data->monitor_geometry.y));
          gdk_window_invalidate_region (gtk_widget_get_window (data->window), region, TRUE);
          /* Revert changes to the region */
          cairo_region_translate (region, data->monitor_geometry.x, data->monitor_geometry.y);
        }

      cairo_region_destroy (region);

      return TRUE;
    }

  return FALSE;
}



/* Public */



gboolean
screenshooter_select_region_wayland (GdkRectangle *region)
{
  RubberBandData rbdata = {0};
  GdkCursor *xhair_cursor;
  GdkDisplay *display;
  int n_monitors;

  if (!gtk_layer_is_supported())
    {
      screenshooter_error (_("Compositor does not support zwlr_layer_shell_v1"));
      return FALSE;
    }

  display = gdk_display_get_default ();
  xhair_cursor = gdk_cursor_new_for_display (display, GDK_CROSSHAIR);
  n_monitors = gdk_display_get_n_monitors (display);

  /* Create the fullscreen windows on which the rubber banding will be drawn */
  for (int n_monitor = 0; n_monitor < n_monitors; n_monitor++)
    {
      GdkMonitor *monitor;
      GtkWidget *window;
      OverlayData *overlay;

      monitor = gdk_display_get_monitor (display, n_monitor);
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      overlay = g_new0 (OverlayData ,1);
      overlay->window = window;
      gdk_monitor_get_geometry (monitor, &(overlay->monitor_geometry));
      rbdata.overlays = g_slist_append (rbdata.overlays, overlay);

      gtk_window_set_title (GTK_WINDOW (window), _("Screenshooter overlay"));
      gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
      gtk_window_set_deletable (GTK_WINDOW (window), FALSE);
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      gtk_widget_set_can_focus (window, FALSE);
      gtk_widget_set_app_paintable (window, TRUE);
      gtk_widget_add_events (window,
                            GDK_BUTTON_RELEASE_MASK |
                            GDK_BUTTON_PRESS_MASK |
                            GDK_POINTER_MOTION_MASK |
                            GDK_KEY_PRESS_MASK |
                            GDK_ENTER_NOTIFY_MASK);
      gtk_widget_set_visual (window, gdk_screen_get_rgba_visual (gdk_screen_get_default ()));

      gtk_layer_init_for_window (GTK_WINDOW (window));
      gtk_layer_set_monitor (GTK_WINDOW (window), monitor);
      gtk_layer_set_layer (GTK_WINDOW (window), GTK_LAYER_SHELL_LAYER_OVERLAY);
      gtk_layer_set_keyboard_mode (GTK_WINDOW (window), GTK_LAYER_SHELL_KEYBOARD_MODE_EXCLUSIVE);
      gtk_layer_set_anchor (GTK_WINDOW (window), GTK_LAYER_SHELL_EDGE_TOP, TRUE);
      gtk_layer_set_anchor (GTK_WINDOW (window), GTK_LAYER_SHELL_EDGE_LEFT, TRUE);
      gtk_layer_set_anchor (GTK_WINDOW (window), GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE);
      gtk_layer_set_anchor (GTK_WINDOW (window), GTK_LAYER_SHELL_EDGE_RIGHT, TRUE);

      /* Make sure it's possible to drag from screen edges */
      gtk_layer_set_margin (GTK_WINDOW (window), GTK_LAYER_SHELL_EDGE_TOP, -1);
      gtk_layer_set_margin (GTK_WINDOW (window), GTK_LAYER_SHELL_EDGE_LEFT, -1);
      gtk_layer_set_margin (GTK_WINDOW (window), GTK_LAYER_SHELL_EDGE_BOTTOM, -1);
      gtk_layer_set_margin (GTK_WINDOW (window), GTK_LAYER_SHELL_EDGE_RIGHT, -1);
      gtk_layer_set_exclusive_zone (GTK_WINDOW (window), -1);

      /* Connect to the interesting signals */
      g_signal_connect (window, "key-press-event", G_CALLBACK (cb_key_pressed), &rbdata);
      g_signal_connect (window, "key-release-event", G_CALLBACK (cb_key_released), &rbdata);
      g_signal_connect (window, "draw", G_CALLBACK (cb_draw), &rbdata);
      g_signal_connect (window, "button-press-event", G_CALLBACK (cb_button_pressed), &rbdata);
      g_signal_connect (window, "button-release-event", G_CALLBACK (cb_button_released), &rbdata);
      g_signal_connect (window, "motion-notify-event", G_CALLBACK (cb_motion_notify), &rbdata);

      gtk_widget_show_now (window);
      gdk_window_set_cursor (gtk_widget_get_window (window), xhair_cursor);
    }

  /* Set up the window showing the screenshot size */
  create_size_window (&rbdata);

  gtk_main ();
  g_slist_foreach (rbdata.overlays, (GFunc) (void (*)(void)) destroy_overlay, NULL);
  g_slist_free (rbdata.overlays);
  g_object_unref (xhair_cursor);

  if (rbdata.cancelled)
    return FALSE;

  region->x = rbdata.rectangle.x;
  region->y = rbdata.rectangle.y;
  region->width = rbdata.rectangle.width;
  region->height = rbdata.rectangle.height;

  return TRUE;
}
