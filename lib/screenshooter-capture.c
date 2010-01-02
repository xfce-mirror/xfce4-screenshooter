/*  $Id$
 *
 *  Copyright © 2008-2010 Jérôme Guelfucci <jeromeg@xfce.org>
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

#include "screenshooter-capture.h"



/* Rubberband data for composited environment */
typedef struct
{
  gboolean left_pressed;
  gboolean rubber_banding;
  gint x;
  gint y;
  gint x_root;
  gint y_root;
  GdkRectangle rectangle;
  GdkRectangle rectangle_root;
} RubberBandData;

/* For non-composited environments */
typedef struct
{
  gboolean pressed;
  gboolean cancelled;
  GdkRectangle rectangle;
  gint x1, y1; /* holds the position where the mouse was pressed */
  GdkGC *gc;
  GdkWindow *root_window;
} RbData;


/* Prototypes */



static GdkWindow       *get_active_window                   (GdkScreen      *screen,
                                                             gboolean       *needs_unref,
                                                             gboolean       *border);
static GdkPixbuf       *get_window_screenshot               (GdkWindow      *window,
                                                             gboolean        show_mouse,
                                                             gboolean        border);
static GdkFilterReturn  region_filter_func                  (GdkXEvent      *xevent,
                                                             GdkEvent       *event,
                                                             RbData         *rbdata);
static GdkPixbuf       *get_rectangle_screenshot            (void);
static gboolean         cb_key_pressed                      (GtkWidget      *widget,
                                                             GdkEventKey    *event,
                                                             gboolean       *cancelled);
static gboolean         cb_expose                           (GtkWidget      *widget,
                                                             GdkEventExpose *event,
                                                             RubberBandData *rbdata);
static gboolean         cb_button_pressed                   (GtkWidget      *widget,
                                                             GdkEventButton *event,
                                                             RubberBandData *rbdata);
static gboolean         cb_button_released                  (GtkWidget      *widget,
                                                             GdkEventButton *event,
                                                             RubberBandData *rbdata);
static gboolean         cb_motion_notify                    (GtkWidget      *widget,
                                                             GdkEventMotion *event,
                                                             RubberBandData *rbdata);
static GdkPixbuf       *get_rectangle_screenshot_composited (void);



/* Internals */



static GdkWindow
*get_active_window (GdkScreen *screen,
                    gboolean  *needs_unref,
                    gboolean  *border)
{
  GdkWindow *window, *window2;

  TRACE ("Get the active window");

  window = gdk_screen_get_active_window (screen);

  /* If there is no active window, we fallback to the whole screen. */
  if (G_UNLIKELY (window == NULL))
    {
      TRACE ("No active window, fallback to the root window");

      window = gdk_get_default_root_window ();
      *needs_unref = FALSE;
      *border = FALSE;
    }
  else if (G_UNLIKELY (GDK_WINDOW_DESTROYED (window)))
    {
      TRACE ("The active window is destroyed, fallback to the root window.");

      g_object_unref (window);
      window = gdk_get_default_root_window ();
      *needs_unref = FALSE;
      *border = FALSE;
    }
  else if (gdk_window_get_type_hint (window) == GDK_WINDOW_TYPE_HINT_DESKTOP)
    {
      /* If the active window is the desktop, grab the whole screen */
      TRACE ("The active window is the desktop, fallback to the root window");

      g_object_unref (window);

      window = gdk_get_default_root_window ();
      *needs_unref = FALSE;
      *border = FALSE;
    }
  else
    {
      /* Else we find the toplevel window to grab the decorations. */
      TRACE ("Active window is a normal window, grab the toplevel window");

      window2 = gdk_window_get_toplevel (window);

      g_object_unref (window);

      window = window2;
      *border = TRUE;
    }

  return window;
}



static Window
find_wm_window (Window xid)
{
  Window root, parent, *children;
  unsigned int nchildren;

  do
    {
      if (XQueryTree (GDK_DISPLAY (), xid, &root,
                      &parent, &children, &nchildren) == 0)
        {
          g_warning ("Couldn't find window manager window");
          return None;
        }

      if (root == parent)
        return xid;

      xid = parent;
    }
  while (TRUE);
}



static GdkPixbuf
*get_window_screenshot (GdkWindow *window,
                        gboolean show_mouse,
                        gboolean border)
{
  gint x_orig, y_orig;
  gint width, height;

  GdkPixbuf *screenshot;
  GdkWindow *root;

  GdkRectangle rectangle;

  /* Get the root window */
  TRACE ("Get the root window");

  root = gdk_get_default_root_window ();

  if (border)
    {
      Window xwindow = GDK_WINDOW_XWINDOW (window);
      window = gdk_window_foreign_new (find_wm_window (xwindow));
    }

  gdk_drawable_get_size (window, &rectangle.width, &rectangle.height);
  gdk_window_get_origin (window, &rectangle.x, &rectangle.y);

  /* Don't grab thing offscreen. */

  TRACE ("Make sure we don't grab things offscreen");

  x_orig = rectangle.x;
  y_orig = rectangle.y;
  width  = rectangle.width;
  height = rectangle.height;

  if (x_orig < 0)
    {
      width = width + x_orig;
      x_orig = 0;
    }

  if (y_orig < 0)
    {
      height = height + y_orig;
      y_orig = 0;
    }

  if (x_orig + width > gdk_screen_width ())
    width = gdk_screen_width () - x_orig;

  if (y_orig + height > gdk_screen_height ())
    height = gdk_screen_height () - y_orig;

  /* Take the screenshot from the root GdkWindow, to grab things such as
   * menus. */

  TRACE ("Grab the screenshot");

  screenshot = gdk_pixbuf_get_from_drawable (NULL, root, NULL,
                                             x_orig, y_orig, 0, 0,
                                             width, height);

  /* Code adapted from gnome-screenshot:
   * Copyright (C) 2001-2006  Jonathan Blandford <jrb@alum.mit.edu>
   * Copyright (C) 2008 Cosimo Cecchi <cosimoc@gnome.org>
   */
  if (border)
    {
      /* Use XShape to make the background transparent */
      XRectangle *rectangles;
      GdkPixbuf *tmp;
      int rectangle_count, rectangle_order, i;

      rectangles = XShapeGetRectangles (GDK_DISPLAY (),
                                        GDK_WINDOW_XWINDOW (window),
                                        ShapeBounding,
                                        &rectangle_count,
                                        &rectangle_order);

      if (rectangles && rectangle_count > 0 && window != root)
        {
          gboolean has_alpha = gdk_pixbuf_get_has_alpha (screenshot);

          tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
          gdk_pixbuf_fill (tmp, 0);

          for (i = 0; i < rectangle_count; i++)
            {
              gint rec_x, rec_y;
              gint rec_width, rec_height;
              gint y;

              rec_x = rectangles[i].x;
              rec_y = rectangles[i].y;
              rec_width = rectangles[i].width;
              rec_height = rectangles[i].height;

              if (rectangle.x < 0)
                {
                  rec_x += rectangle.x;
                  rec_x = MAX(rec_x, 0);
                  rec_width += rectangle.x;
                }

              if (rectangle.y < 0)
                {
                  rec_y += rectangle.y;
                  rec_y = MAX(rec_y, 0);
                  rec_height += rectangle.y;
                }

              if (x_orig + rec_x + rec_width > gdk_screen_width ())
                rec_width = gdk_screen_width () - x_orig - rec_x;

              if (y_orig + rec_y + rec_height > gdk_screen_height ())
                rec_height = gdk_screen_height () - y_orig - rec_y;

              for (y = rec_y; y < rec_y + rec_height; y++)
                {
                  guchar *src_pixels, *dest_pixels;
                  gint x;

                  src_pixels = gdk_pixbuf_get_pixels (screenshot)
                             + y * gdk_pixbuf_get_rowstride(screenshot)
                             + rec_x * (has_alpha ? 4 : 3);
                  dest_pixels = gdk_pixbuf_get_pixels (tmp)
                              + y * gdk_pixbuf_get_rowstride (tmp)
                              + rec_x * 4;

                  for (x = 0; x < rec_width; x++)
                    {
                      *dest_pixels++ = *src_pixels++;
                      *dest_pixels++ = *src_pixels++;
                      *dest_pixels++ = *src_pixels++;

                      if (has_alpha)
                        *dest_pixels++ = *src_pixels++;
                      else
                        *dest_pixels++ = 255;
                    }
                }
            }

          g_object_unref (screenshot);
          screenshot = tmp;
        }
    }

  if (show_mouse)
    {
        GdkCursor *cursor;
        GdkPixbuf *cursor_pixbuf;

        /* Add the mouse pointer to the grabbed screenshot */
        TRACE ("Get the mouse cursor and its image");

        cursor = gdk_cursor_new_for_display (gdk_display_get_default (), GDK_LEFT_PTR);
        cursor_pixbuf = gdk_cursor_get_image (cursor);

        if (G_LIKELY (cursor_pixbuf != NULL))
          {
            GdkRectangle rectangle_window, rectangle_cursor;
            gint cursorx, cursory, xhot, yhot;

            TRACE ("Get the coordinates of the cursor");

            gdk_window_get_pointer (root, &cursorx, &cursory, NULL);

            TRACE ("Get the cursor hotspot");

            sscanf (gdk_pixbuf_get_option (cursor_pixbuf, "x_hot"), "%d", &xhot);
            sscanf (gdk_pixbuf_get_option (cursor_pixbuf, "y_hot"), "%d", &yhot);

            /* rectangle_window stores the window coordinates */
            rectangle_window.x = x_orig;
            rectangle_window.y = y_orig;
            rectangle_window.width = width;
            rectangle_window.height = height;

            /* rectangle_cursor stores the cursor coordinates */
            rectangle_cursor.x = cursorx;
            rectangle_cursor.y = cursory;
            rectangle_cursor.width = gdk_pixbuf_get_width (cursor_pixbuf);
            rectangle_cursor.height = gdk_pixbuf_get_height (cursor_pixbuf);

            /* see if the pointer is inside the window */
            if (gdk_rectangle_intersect (&rectangle_window,
                                         &rectangle_cursor,
                                         &rectangle_cursor))
              {
                TRACE ("Compose the two pixbufs");

                gdk_pixbuf_composite (cursor_pixbuf, screenshot,
                                      cursorx - x_orig -xhot, cursory - y_orig -yhot,
                                      rectangle_cursor.width, rectangle_cursor.height,
                                      cursorx - x_orig - xhot, cursory - y_orig -yhot,
                                      1.0, 1.0,
                                      GDK_INTERP_BILINEAR,
                                      255);
              }

            g_object_unref (cursor_pixbuf);
          }

        gdk_cursor_unref (cursor);
    }

  return screenshot;
}



/* Callbacks for the rubber banding function */
static gboolean cb_key_pressed (GtkWidget   *widget,
                                GdkEventKey *event,
                                gboolean    *cancelled)
{
  if (event->keyval == GDK_Escape)
    {
      gtk_widget_hide (widget);
      *cancelled = TRUE;
      return TRUE;
    }

  return FALSE;
}



static gboolean cb_expose (GtkWidget *widget,
                           GdkEventExpose *event,
                           RubberBandData *rbdata)
{
  GdkRectangle *rects = NULL;
  gint n_rects = 0, i;

  TRACE ("Expose event received.");

  gdk_region_get_rectangles (event->region, &rects, &n_rects);

  if (rbdata->rubber_banding)
    {
      GdkRectangle intersect;
      cairo_t *cr;

      cr = gdk_cairo_create (GDK_DRAWABLE (widget->window));
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

      for (i = 0; i < n_rects; ++i)
        {
          /* Restore the transparent background */
          cairo_set_source_rgba (cr, 0, 0, 0, 0.8);
          gdk_cairo_rectangle (cr, &rects[i]);
          cairo_fill (cr);

          if (!gdk_rectangle_intersect (&rects[i],
                                        &rbdata->rectangle,
                                        &intersect))
            {
              continue;
            }

          /* Paint the rubber banding rectangles */
          cairo_set_source_rgba (cr, 1.0f, 1.0f, 1.0f, 0.0f);
          gdk_cairo_rectangle (cr, &intersect);
          cairo_fill (cr);
        }

      cairo_destroy (cr);
    }
  else
    {
      cairo_t *cr;

      /* Draw the transparent background */
      cr = gdk_cairo_create (GDK_DRAWABLE (widget->window));
      cairo_set_source_rgba (cr, 0, 0, 0, 0.8);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

      for (i = 0; i < n_rects; ++i)
        {
            gdk_cairo_rectangle (cr, &rects[i]);
            cairo_fill (cr);
        }

      cairo_destroy (cr);
    }

  g_free (rects);

  return FALSE;
}



static gboolean cb_button_pressed (GtkWidget *widget,
                                   GdkEventButton *event,
                                   RubberBandData *rbdata)
{
  if (event->button == 1)
    {
      TRACE ("Left button pressed");

      rbdata->left_pressed = TRUE;
      rbdata->x = event->x;
      rbdata->y = event->y;
      rbdata->x_root = event->x_root;
      rbdata->y_root = event->y_root;

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
      if (rbdata->rubber_banding)
        {
          gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_NONE);
          return TRUE;
        }
      else
        rbdata->left_pressed = rbdata->rubber_banding = FALSE;
    }

  return FALSE;
}



static gboolean cb_motion_notify (GtkWidget *widget,
                                  GdkEventMotion *event,
                                  RubberBandData *rbdata)
{
  if (rbdata->left_pressed)
    {
      GdkRectangle *new_rect, *new_rect_root;
      GdkRectangle old_rect, intersect;
      GdkRegion *region;

      TRACE ("Mouse is moving with left button pressed");

      new_rect = &rbdata->rectangle;
      new_rect_root = &rbdata->rectangle_root;

      if (!rbdata->rubber_banding)
        {
          /* This is the start of a rubber banding */
          rbdata->rubber_banding = TRUE;
          old_rect.x = rbdata->x;
          old_rect.y = rbdata->y;
          old_rect.height = old_rect.width = 1;
        }
      else
        {
          /* Rubber banding has already started, update it */
          old_rect.x = new_rect->x;
          old_rect.y = new_rect->y;
          old_rect.width = new_rect->width;
          old_rect.height = new_rect->height;
        }

      /* Get the new rubber banding rectangle */
      new_rect->x = MIN (rbdata->x , event->x);
      new_rect->y = MIN (rbdata->y, event->y);
      new_rect->width = ABS (rbdata->x - event->x) + 1;
      new_rect->height = ABS (rbdata->y - event->y) +1;

      new_rect_root->x = MIN (rbdata->x_root , event->x_root);
      new_rect_root->y = MIN (rbdata->y_root, event->y_root);
      new_rect_root->width = ABS (rbdata->x_root - event->x_root) + 1;
      new_rect_root->height = ABS (rbdata->y_root - event->y_root) +1;

      region = gdk_region_rectangle (&old_rect);
      gdk_region_union_with_rect (region, new_rect);

      /* Try to be smart: don't send the expose event for regions which
       * have already been painted */
      if (gdk_rectangle_intersect (&old_rect, new_rect, &intersect)
          && intersect.width > 2 && intersect.height > 2)
        {
          GdkRegion *region_intersect;

          intersect.x += 1;
          intersect.width -= 2;
          intersect.y += 1;
          intersect.height -= 2;

          region_intersect = gdk_region_rectangle(&intersect);
          gdk_region_subtract(region, region_intersect);
          gdk_region_destroy(region_intersect);
        }

      gdk_window_invalidate_region (widget->window, region, TRUE);
      gdk_region_destroy (region);

      return TRUE;
    }

  return FALSE;
}


static GdkPixbuf
*get_rectangle_screenshot_composited (void)
{
  GtkWidget *window;
  RubberBandData rbdata;
  gboolean cancelled = FALSE;
  GdkPixbuf *screenshot;
  GdkWindow *root;
  GdkCursor *xhair_cursor = gdk_cursor_new (GDK_CROSSHAIR);

  /* Initialize the rubber band data */
  rbdata.left_pressed = FALSE;
  rbdata.rubber_banding = FALSE;
  rbdata.x = rbdata.y = 0;

  /* Create the fullscreen window on which the rubber banding
   * will be drawn. */
  window = gtk_dialog_new ();
  gtk_window_set_decorated (GTK_WINDOW (window), FALSE);
  gtk_window_set_deletable (GTK_WINDOW (window), FALSE);
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  gtk_dialog_set_has_separator (GTK_DIALOG (window), FALSE);
  gtk_widget_set_app_paintable (window, TRUE);
  gtk_widget_add_events (window,
                         GDK_BUTTON_RELEASE_MASK | GDK_BUTTON_PRESS_MASK |
                         GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_MASK |
                         GDK_KEY_PRESS_MASK);
  gtk_widget_set_colormap (window,
                           gdk_screen_get_rgba_colormap (gdk_screen_get_default ()));

  /* Connect to the interesting signals */
  g_signal_connect (window, "key-press-event",
                    G_CALLBACK (cb_key_pressed), &cancelled);
  g_signal_connect (window, "expose-event",
                    G_CALLBACK (cb_expose), &rbdata);
  g_signal_connect (window, "button-press-event",
                    G_CALLBACK (cb_button_pressed), &rbdata);
  g_signal_connect (window, "button-release-event",
                    G_CALLBACK (cb_button_released), &rbdata);
  g_signal_connect (window, "motion-notify-event",
                    G_CALLBACK (cb_motion_notify), &rbdata);

  /* This window is not managed by the window manager, we have to set everything
   * ourselves */
  gtk_widget_realize (window);
  gdk_window_set_cursor (window->window, xhair_cursor);
  gdk_window_set_override_redirect (window->window, TRUE);
  gtk_widget_set_size_request (window,
                               gdk_screen_get_width (gdk_screen_get_default ()),
                               gdk_screen_get_height (gdk_screen_get_default ()));
  gdk_window_raise (window->window);
  gtk_widget_show_now (window);
  gtk_widget_grab_focus (window);
  gdk_flush ();

  /* Grab the mouse and the keyboard to prevent any interaction with other 
   * applications */
  gdk_keyboard_grab (window->window, FALSE, GDK_CURRENT_TIME);
  gdk_pointer_grab (window->window, TRUE, 0, NULL, NULL, GDK_CURRENT_TIME);

  gtk_dialog_run (GTK_DIALOG (window));
  gtk_widget_destroy (window);
  gdk_cursor_unref (xhair_cursor);

  if (cancelled)
    return NULL;

  screenshot = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                               TRUE,
                               8,
                               rbdata.rectangle.width,
                               rbdata.rectangle.height);

  /* Grab the screenshot on the main window */
  root = gdk_get_default_root_window ();
  gdk_pixbuf_get_from_drawable (screenshot, root, NULL,
                                rbdata.rectangle_root.x,
                                rbdata.rectangle_root.y,
                                0, 0,
                                rbdata.rectangle.width,
                                rbdata.rectangle.height);

  /* Ungrab the mouse and the keyboard */
  gdk_pointer_ungrab (GDK_CURRENT_TIME);
  gdk_keyboard_ungrab (GDK_CURRENT_TIME);
  gdk_flush ();

  return screenshot;
}



static GdkFilterReturn
region_filter_func (GdkXEvent *xevent, GdkEvent *event, RbData *rbdata)
{
  XEvent *x_event = (XEvent *) xevent;
  gint x2 = 0, y2 = 0;

  switch (x_event->type)
    {
      /* Start dragging the rectangle out */
      case ButtonPress:
        TRACE ("Start dragging the rectangle");

        rbdata->rectangle.x = rbdata->x1 = x_event->xkey.x_root;
        rbdata->rectangle.y = rbdata->y1 = x_event->xkey.y_root;
        rbdata->rectangle.width = 0;
        rbdata->rectangle.height = 0;
        rbdata->pressed = TRUE;

        return GDK_FILTER_REMOVE;
      break;

      /* Finish dragging the rectangle out */
      case ButtonRelease:
        if (rbdata->pressed)
          {
            if (rbdata->rectangle.width > 0 && rbdata->rectangle.height > 0)
              {
                /* Remove the rectangle drawn previously */

                TRACE ("Remove the rectangle drawn previously");

                gdk_draw_rectangle (rbdata->root_window,
                                    rbdata->gc,
                                    FALSE,
                                    rbdata->rectangle.x,
                                    rbdata->rectangle.y,
                                    rbdata->rectangle.width,
                                    rbdata->rectangle.height);

                gtk_main_quit ();
              }
            else
              {
                /* The user has not dragged the mouse, start again */

                TRACE ("Mouse was not dragged, start again");

                rbdata->pressed = FALSE;
              }
          }
        return GDK_FILTER_REMOVE;
      break;

      /* The user is moving the mouse */
      case MotionNotify:
        if (rbdata->pressed)
          {
            TRACE ("Mouse is moving");

            if (rbdata->rectangle.width > 0 && rbdata->rectangle.height > 0)
              {
                /* Remove the rectangle drawn previously */

                TRACE ("Remove the rectangle drawn previously");
                gdk_draw_rectangle (rbdata->root_window,
                                    rbdata->gc,
                                    FALSE,
                                    rbdata->rectangle.x,
                                    rbdata->rectangle.y,
                                    rbdata->rectangle.width,
                                    rbdata->rectangle.height);
              }

            x2 = x_event->xkey.x_root;
            y2 = x_event->xkey.y_root;

            rbdata->rectangle.x = MIN (rbdata->x1, x2);
            rbdata->rectangle.y = MIN (rbdata->y1, y2);
            rbdata->rectangle.width = ABS (x2 - rbdata->x1);
            rbdata->rectangle.height = ABS (y2 - rbdata->y1);

            /* Draw  the rectangle as the user drags the mouse */
            TRACE ("Draw the new rectangle");
            if (rbdata->rectangle.width > 0 && rbdata->rectangle.height > 0)
              gdk_draw_rectangle (rbdata->root_window,
                                  rbdata->gc,
                                  FALSE,
                                  rbdata->rectangle.x,
                                  rbdata->rectangle.y,
                                  rbdata->rectangle.width,
                                  rbdata->rectangle.height);
          }
        return GDK_FILTER_REMOVE;
        break;

      case KeyPress:
        if (x_event->xkey.keycode == XKeysymToKeycode (gdk_display, XK_Escape))
          {
            TRACE ("Escape key was pressed, cancel the screenshot.");

            if (rbdata->pressed)
              {
                if (rbdata->rectangle.width > 0 && rbdata->rectangle.height > 0)
                  {
                    /* Remove the rectangle drawn previously */

                    TRACE ("Remove the rectangle drawn previously");

                    gdk_draw_rectangle (rbdata->root_window,
                                        rbdata->gc,
                                        FALSE,
                                        rbdata->rectangle.x,
                                        rbdata->rectangle.y,
                                        rbdata->rectangle.width,
                                        rbdata->rectangle.height);
                  }
              }

            rbdata->cancelled = TRUE;
            gtk_main_quit ();
            return GDK_FILTER_REMOVE;
          }
        break;

      default:
        break;
    }

  return GDK_FILTER_CONTINUE;
}



static GdkPixbuf
*get_rectangle_screenshot (void)
{
  GdkPixbuf *screenshot = NULL;
  GdkWindow *root_window;

  GdkGCValues gc_values;
  GdkGC *gc;

  GdkGCValuesMask values_mask =
    GDK_GC_FUNCTION | GDK_GC_FILL | GDK_GC_CLIP_MASK |
    GDK_GC_SUBWINDOW | GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN |
    GDK_GC_EXPOSURES | GDK_GC_LINE_WIDTH | GDK_GC_LINE_STYLE |
    GDK_GC_CAP_STYLE | GDK_GC_JOIN_STYLE;

  GdkColor gc_white = {0, 65535, 65535, 65535};
  GdkColor gc_black = {0, 0, 0, 0};

  GdkEventMask mask =
    GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK |
    GDK_BUTTON_RELEASE_MASK;
  GdkCursor *xhair_cursor = gdk_cursor_new (GDK_CROSSHAIR);

  RbData rbdata;

  /* Get root window */
  TRACE ("Get the root window");
  root_window = gdk_get_default_root_window ();

  /*Set up graphics context for a XOR rectangle that will be drawn as
   * the user drags the mouse */
  TRACE ("Initialize the graphics context");

  gc_values.function           = GDK_XOR;
  gc_values.line_width         = 2;
  gc_values.line_style         = GDK_LINE_ON_OFF_DASH;
  gc_values.fill               = GDK_SOLID;
  gc_values.cap_style          = GDK_CAP_BUTT;
  gc_values.join_style         = GDK_JOIN_MITER;
  gc_values.graphics_exposures = FALSE;
  gc_values.clip_x_origin      = 0;
  gc_values.clip_y_origin      = 0;
  gc_values.clip_mask          = None;
  gc_values.subwindow_mode     = GDK_INCLUDE_INFERIORS;

  gc = gdk_gc_new_with_values (root_window, &gc_values, values_mask);
  gdk_gc_set_rgb_fg_color (gc, &gc_white);
  gdk_gc_set_rgb_bg_color (gc, &gc_black);

  /* Change cursor to cross-hair */
  TRACE ("Set the cursor");

  gdk_pointer_grab (root_window, FALSE, mask, NULL, xhair_cursor, GDK_CURRENT_TIME);
  gdk_keyboard_grab (root_window, FALSE, GDK_CURRENT_TIME);

  /* Initialize the rubber band data */
  TRACE ("Initialize the rubber band data");
  rbdata.root_window = root_window;
  rbdata.gc = gc;
  rbdata.pressed = FALSE;
  rbdata.cancelled = FALSE;

  /* Set the filter function to handle the GDK events */
  TRACE ("Add the events filter");
  gdk_window_add_filter (root_window, (GdkFilterFunc) region_filter_func, &rbdata);

  gdk_flush ();

  gtk_main ();

  gdk_window_remove_filter (root_window, (GdkFilterFunc) region_filter_func, &rbdata);

  gdk_pointer_ungrab(GDK_CURRENT_TIME);
  gdk_keyboard_ungrab (GDK_CURRENT_TIME);

  /* Get the screenshot's pixbuf */
  if (G_LIKELY (!rbdata.cancelled))
    {
      TRACE ("Get the pixbuf for the screenshot");

      screenshot =
        gdk_pixbuf_get_from_drawable (NULL, root_window, NULL,
                                      rbdata.rectangle.x,
                                      rbdata.rectangle.y,
                                      0, 0,
                                      rbdata.rectangle.width,
                                      rbdata.rectangle.height);
    }

  if (G_LIKELY (gc != NULL))
    g_object_unref (gc);

  gdk_cursor_unref (xhair_cursor);

  return screenshot;
}



/* Public */



/**
 * screenshooter_take_screenshot:
 * @region: the region to be screenshoted. It can be FULLSCREEN, ACTIVE_WINDOW or SELECT.
 * @delay: the delay before the screenshot is taken, in seconds.
 * @mouse: whether the mouse pointer should be displayed on the screenshot.
 *
 * Takes a screenshot with the given options. If @region is FULLSCREEN, the screenshot
 * is taken after @delay seconds. If @region is ACTIVE_WINDOW, a delay of @delay seconds
 * ellapses, then the active window is detected and captured. If @region is SELECT, @delay
 * will be ignored and the user will have to select a portion of the screen with the
 * mouse.
 *
 * @show_mouse is only taken into account when @region is FULLSCREEN or ACTIVE_WINDOW.
 *
 * Return value: a #GdkPixbuf containing the screenshot or %NULL (if @region is SELECT,
 * the user can cancel the operation).
 **/
GdkPixbuf *screenshooter_take_screenshot (gint     region,
                                          gint     delay,
                                          gboolean show_mouse,
                                          gboolean plugin)
{
  GdkPixbuf *screenshot = NULL;
  GdkWindow *window = NULL;
  GdkScreen *screen;
  GdkDisplay *display;
  gboolean border;

  /* gdk_get_default_root_window () does not need to be unrefed,
   * needs_unref enables us to unref *window only if a non default
   * window has been grabbed. */
  gboolean needs_unref = TRUE;

  /* Get the screen on which the screenshot should be taken */
  screen = gdk_screen_get_default ();

  /* Sync the display */
  display = gdk_display_get_default ();
  gdk_display_sync (display);

  gdk_window_process_all_updates ();

  /* wait for n=delay seconds */
  if (region != SELECT)
    sleep (delay);

  /* Get the window/desktop we want to screenshot*/
  if (region == FULLSCREEN)
    {
      TRACE ("We grab the entire screen");

      window = gdk_get_default_root_window ();
      needs_unref = FALSE;
      border = FALSE;
    }
  else if (region == ACTIVE_WINDOW)
    {
      TRACE ("We grab the active window");

      window = get_active_window (screen, &needs_unref, &border);
    }

  if (region == FULLSCREEN || region == ACTIVE_WINDOW)
    {
      TRACE ("Get the screenshot of the given window");

      screenshot = get_window_screenshot (window, show_mouse, border);

      if (needs_unref)
        g_object_unref (window);
    }
  else if (region == SELECT)
    {
      TRACE ("Let the user select the region to screenshot");
      if (!gdk_screen_is_composited (screen))
        screenshot = get_rectangle_screenshot ();
      else
        screenshot = get_rectangle_screenshot_composited ();
    }

  return screenshot;
}
