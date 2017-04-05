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

#define BACKGROUND_TRANSPARENCY 0.4

/* Rubberband data for composited environment */
typedef struct
{
  gboolean left_pressed;
  gboolean rubber_banding;
  gint x;
  gint y;
  gint x_root;
  gint y_root;
  cairo_rectangle_int_t rectangle;
  cairo_rectangle_int_t rectangle_root;
} RubberBandData;

/* For non-composited environments */
typedef struct
{
  gboolean pressed;
  gboolean cancelled;
  cairo_rectangle_int_t rectangle;
  gint x1, y1; /* holds the position where the mouse was pressed */
  GC *context;
} RbData;


/* Prototypes */



static GdkWindow       *get_active_window                   (GdkScreen      *screen,
                                                             gboolean       *needs_unref,
                                                             gboolean       *border);
static void             free_pixmap_data                    (guchar *pixels,
                                                             gpointer data);
static GdkPixbuf       *get_cursor_pixbuf                   (GdkDisplay *display,
                                                             GdkWindow *root,
                                                             gint *cursorx,
                                                             gint *cursory,
                                                             gint *xhot,
                                                             gint *yhot);
static GdkPixbuf       *get_window_screenshot               (GdkWindow      *window,
                                                             gboolean        show_mouse,
                                                             gboolean        border);
static GdkFilterReturn  region_filter_func                  (GdkXEvent      *xevent,
                                                             GdkEvent       *event,
                                                             RbData         *rbdata);
static GdkPixbuf       *get_rectangle_screenshot            (gint delay);
static gboolean         cb_key_pressed                      (GtkWidget      *widget,
                                                             GdkEventKey    *event,
                                                             gboolean       *cancelled);
static gboolean         cb_draw                             (GtkWidget      *widget,
                                                             cairo_t        *cr,
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
static GdkPixbuf       *get_rectangle_screenshot_composited (gint delay);



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
  else if (G_UNLIKELY (gdk_window_is_destroyed (window)))
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
      if (XQueryTree (gdk_x11_get_default_xdisplay (), xid, &root,
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


static void free_pixmap_data (guchar *pixels,  gpointer data)
{
  g_free (pixels);
}


static GdkPixbuf *get_cursor_pixbuf (GdkDisplay *display,
                                     GdkWindow *root,
                                     gint *cursorx,
                                     gint *cursory,
                                     gint *xhot,
                                     gint *yhot)
{
  GdkCursor *cursor = NULL;
  GdkPixbuf *cursor_pixbuf = NULL;
  GdkDevice *pointer = NULL;
  GdkSeat   *seat = NULL;

#ifdef HAVE_XFIXES
  XFixesCursorImage *cursor_image = NULL;
  guint32            tmp;
  guchar            *cursor_pixmap_data = NULL;
  gint               i, j;
  int                event_basep;
  int                error_basep;

  if (!XFixesQueryExtension (GDK_DISPLAY_XDISPLAY (display),
                             &event_basep,
                             &error_basep))
    goto fallback;

  TRACE ("Get the mouse cursor, its image, position and hotspot");

  cursor_image = XFixesGetCursorImage (GDK_DISPLAY_XDISPLAY (display));
  if (cursor_image == NULL)
    goto fallback;

  *cursorx = cursor_image->x;
  *cursory = cursor_image->y;
  *xhot = cursor_image->xhot;
  *yhot = cursor_image->yhot;

  /* cursor_image->pixels contains premultiplied 32-bit ARGB data stored
   * in long (!) */
  cursor_pixmap_data =
    g_new (guchar, cursor_image->width * cursor_image->height * 4);

  for (i = 0, j = 0;
       i < cursor_image->width * cursor_image->height;
       i++, j += 4)
    {
      tmp = ((guint32)cursor_image->pixels[i] << 8) | \
            ((guint32)cursor_image->pixels[i] >> 24);
      cursor_pixmap_data[j] = tmp >> 24;
      cursor_pixmap_data[j + 1] = (tmp >> 16) & 0xff;
      cursor_pixmap_data[j + 2] = (tmp >> 8) & 0xff;
      cursor_pixmap_data[j + 3] = tmp & 0xff;
    }

  cursor_pixbuf = gdk_pixbuf_new_from_data (cursor_pixmap_data,
                                            GDK_COLORSPACE_RGB,
                                            TRUE,
                                            8,
                                            cursor_image->width,
                                            cursor_image->height,
                                            cursor_image->width * 4,
                                            free_pixmap_data,
                                            NULL);

  XFree(cursor_image);

  if (cursor_pixbuf != NULL)
    return cursor_pixbuf;

fallback:
#endif
  TRACE ("Get the mouse cursor and its image through fallback mode");

  cursor = gdk_cursor_new_for_display (display, GDK_LEFT_PTR);
  cursor_pixbuf = gdk_cursor_get_image (cursor);

  if (cursor_pixbuf == NULL)
    return NULL;

  TRACE ("Get the coordinates of the cursor");
  
  seat = gdk_display_get_default_seat (gdk_display_get_default ());
  pointer = gdk_seat_get_pointer (seat);
  gdk_window_get_device_position (root, pointer, cursorx, cursory, NULL);

  TRACE ("Get the cursor hotspot");
  sscanf (gdk_pixbuf_get_option (cursor_pixbuf, "x_hot"), "%d", xhot);
  sscanf (gdk_pixbuf_get_option (cursor_pixbuf, "y_hot"), "%d", yhot);

  gdk_cursor_unref (cursor);

  return cursor_pixbuf;
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
	  Window xwindow = GDK_WINDOW_XID (window);
      window = gdk_x11_window_foreign_new_for_display (gdk_window_get_display (window),
    		                                           find_wm_window (xwindow));
    }

  rectangle.width = gdk_window_get_width (window);
  rectangle.height = gdk_window_get_height (window);
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

  screenshot = gdk_pixbuf_get_from_window (root, x_orig, y_orig, width, height);

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

      rectangles = XShapeGetRectangles (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                                        GDK_WINDOW_XID (window),
                                        ShapeBounding,
                                        &rectangle_count,
                                        &rectangle_order);

      if (rectangles && rectangle_count > 0 && window != root)
        {
          gboolean has_alpha = gdk_pixbuf_get_has_alpha (screenshot);

          tmp =
            gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, width, height);
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

              if (rec_x < 0)
                {
                  rec_width = rec_width + rec_x;
                  rec_x = 0;
                }

              if (rec_y < 0)
                {
                  rec_height = rec_height + rec_y;
                  rec_y = 0;
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
        gint cursorx, cursory, xhot, yhot;
        GdkPixbuf *cursor_pixbuf;
        GdkDisplay *display = gdk_display_get_default ();

        cursor_pixbuf = get_cursor_pixbuf (display, root, &cursorx, &cursory,
                                           &xhot, &yhot);

        if (G_LIKELY (cursor_pixbuf != NULL))
          {
            GdkRectangle rectangle_window, rectangle_cursor;

            /* rectangle_window stores the window coordinates */
            rectangle_window.x = x_orig;
            rectangle_window.y = y_orig;
            rectangle_window.width = width;
            rectangle_window.height = height;

            /* rectangle_cursor stores the cursor coordinates */
            rectangle_cursor.x = cursorx;
            rectangle_cursor.y = cursory;
            rectangle_cursor.width =
              gdk_pixbuf_get_width (cursor_pixbuf);
            rectangle_cursor.height =
              gdk_pixbuf_get_height (cursor_pixbuf);

            /* see if the pointer is inside the window */
            if (gdk_rectangle_intersect (&rectangle_window,
                                         &rectangle_cursor,
                                         &rectangle_cursor))
              {
                TRACE ("Compose the two pixbufs");

                gdk_pixbuf_composite (cursor_pixbuf, screenshot,
                                      cursorx - x_orig -xhot,
                                      cursory - y_orig -yhot,
                                      rectangle_cursor.width,
                                      rectangle_cursor.height,
                                      cursorx - x_orig - xhot,
                                      cursory - y_orig -yhot,
                                      1.0, 1.0,
                                      GDK_INTERP_BILINEAR,
                                      255);
              }

            g_object_unref (cursor_pixbuf);
          }
    }

  return screenshot;
}


/* Callbacks for the rubber banding function */
static gboolean cb_key_pressed (GtkWidget   *widget,
                                GdkEventKey *event,
                                gboolean    *cancelled)
{
  if (event->keyval == GDK_KEY_Escape)
    {
      gtk_widget_hide (widget);
      *cancelled = TRUE;
      return TRUE;
    }

  return FALSE;
}



static gboolean cb_draw (GtkWidget *widget,
                         cairo_t *cr,
                         RubberBandData *rbdata)
{
  cairo_rectangle_t *rects = NULL;
  cairo_rectangle_list_t *list = NULL;
  gint n_rects = 0, i;

  TRACE ("Draw event received.");

  list = cairo_copy_clip_rectangle_list (cr);
  n_rects = list->num_rectangles;
  rects = list->rectangles;

  if (rbdata->rubber_banding)
    {
      cairo_rectangle_int_t intersect;
      GdkRectangle rect;

      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

      for (i = 0; i < n_rects; ++i)
        {
          /* Restore the transparent background */
          cairo_set_source_rgba (cr, 0, 0, 0, BACKGROUND_TRANSPARENCY);
          cairo_rectangle(cr, rects[i].x, rects[i].y, rects[i].width, rects[i].height);
          cairo_fill (cr);

          rect.x = (rects[i].x);
          rect.y =  (rects[i].y);
          rect.width = (rects[i].width);
          rect.height = (rects[i].height);

          if (!gdk_rectangle_intersect (&rect, &rbdata->rectangle, &intersect))
            {
              continue;
            }

          /* Paint the rubber banding rectangles */
          cairo_set_source_rgba (cr, 1.0f, 1.0f, 1.0f, 0.0f);
          gdk_cairo_rectangle (cr, &intersect);
          cairo_fill (cr);
        }
    }
  else
    {
      /* Draw the transparent background */
      cairo_set_source_rgba (cr, 0, 0, 0, BACKGROUND_TRANSPARENCY);
      cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);

      for (i = 0; i < n_rects; ++i)
        {
          cairo_rectangle(cr, rects[i].x, rects[i].y, rects[i].width, rects[i].height);
          cairo_fill (cr);
        }

    }

  cairo_rectangle_list_destroy (list);

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
      cairo_rectangle_int_t *new_rect, *new_rect_root;
      cairo_rectangle_int_t old_rect, intersect;
      cairo_region_t *region;

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

          region_intersect = cairo_region_create_rectangle(&intersect);
          cairo_region_subtract(region, region_intersect);
          cairo_region_destroy(region_intersect);
        }

      gdk_window_invalidate_region (gtk_widget_get_window (widget), region, TRUE);
      cairo_region_destroy (region);

      return TRUE;
    }

  return FALSE;
}


static GdkPixbuf
*get_rectangle_screenshot_composited (gint delay)
{
  GtkWidget *window;
  RubberBandData rbdata;
  gboolean cancelled = FALSE;
  GdkPixbuf *screenshot;
  GdkWindow *root;
  GdkDevice *pointer, *keyboard;
  GdkSeat   *seat;
  GdkCursor *xhair_cursor = gdk_cursor_new_for_display (gdk_display_get_default (),
                                                        GDK_CROSSHAIR);

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
  gtk_widget_set_app_paintable (window, TRUE);
  gtk_widget_add_events (window,
                         GDK_BUTTON_RELEASE_MASK |
                         GDK_BUTTON_PRESS_MASK |
                         GDK_EXPOSURE_MASK |
                         GDK_POINTER_MOTION_MASK |
                         GDK_KEY_PRESS_MASK);
  gtk_widget_set_visual (window, gdk_screen_get_rgba_visual (gdk_screen_get_default ()));

  /* Connect to the interesting signals */
  g_signal_connect (window, "key-press-event",
                    G_CALLBACK (cb_key_pressed), &cancelled);
  g_signal_connect (window, "draw",
                    G_CALLBACK (cb_draw), &rbdata);
  g_signal_connect (window, "button-press-event",
                    G_CALLBACK (cb_button_pressed), &rbdata);
  g_signal_connect (window, "button-release-event",
                    G_CALLBACK (cb_button_released), &rbdata);
  g_signal_connect (window, "motion-notify-event",
                    G_CALLBACK (cb_motion_notify), &rbdata);

  /* This window is not managed by the window manager, we have to set everything
   * ourselves */
  gtk_widget_realize (window);
  gdk_window_set_cursor (gtk_widget_get_window (window), xhair_cursor);
  gdk_window_set_override_redirect (gtk_widget_get_window (window), TRUE);
  gtk_widget_set_size_request (window,
                               gdk_screen_get_width (gdk_screen_get_default ()),
                               gdk_screen_get_height (gdk_screen_get_default ()));
  gdk_window_raise (gtk_widget_get_window (window));
  gtk_widget_show_now (window);
  gtk_widget_grab_focus (window);
  gdk_flush ();

  /* Grab the mouse and the keyboard to prevent any interaction with other
   * applications */
  seat = gdk_display_get_default_seat (gdk_display_get_default ());
  pointer = gdk_seat_get_pointer (seat);
  keyboard = gdk_seat_get_keyboard (seat);

  gdk_device_grab (keyboard, gtk_widget_get_window (window),
                  GDK_OWNERSHIP_NONE, FALSE,
                  GDK_KEY_PRESS_MASK |
                  GDK_KEY_RELEASE_MASK,
                  NULL, GDK_CURRENT_TIME);

  gdk_device_grab (pointer, gtk_widget_get_window (window),
                   GDK_OWNERSHIP_NONE, FALSE,
                   GDK_POINTER_MOTION_MASK |
                   GDK_BUTTON_PRESS_MASK | 
                   GDK_BUTTON_RELEASE_MASK,
                   NULL, GDK_CURRENT_TIME);

  gtk_dialog_run (GTK_DIALOG (window));
  gtk_widget_destroy (window);
  gdk_cursor_unref (xhair_cursor);
  gdk_flush();

  if (cancelled)
    return NULL;

  screenshot = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
                               TRUE,
                               8,
                               rbdata.rectangle.width,
                               rbdata.rectangle.height);

  /* Grab the screenshot on the main window */
  root = gdk_get_default_root_window ();

  sleep(delay);

  screenshot = gdk_pixbuf_get_from_window (root,
                                           rbdata.rectangle_root.x,
                                           rbdata.rectangle_root.y,
                                           rbdata.rectangle.width,
                                           rbdata.rectangle.height);

  /* Ungrab the mouse and the keyboard */
  gdk_device_ungrab (pointer, GDK_CURRENT_TIME);
  gdk_device_ungrab (keyboard, GDK_CURRENT_TIME);
  gdk_flush ();

  return screenshot;
}



static GdkFilterReturn
region_filter_func (GdkXEvent *xevent, GdkEvent *event, RbData *rbdata)
{
  XEvent *x_event = (XEvent *) xevent;
  gint x2 = 0, y2 = 0;
  XIDeviceEvent *device_event;
  Display *display;
  Window root_window;

  display = gdk_x11_get_default_xdisplay ();
  root_window = gdk_x11_get_default_root_xwindow ();

  if (x_event->type != GenericEvent)
    return GDK_FILTER_CONTINUE;

  switch (x_event->xgeneric.evtype)
    {
      /* Start dragging the rectangle out */
      case XI_ButtonPress:
        TRACE ("Start dragging the rectangle");

        device_event = (XIDeviceEvent*) x_event->xcookie.data;

        rbdata->rectangle.x = rbdata->x1 = device_event->root_x;
        rbdata->rectangle.y = rbdata->y1 = device_event->root_y;
        rbdata->rectangle.width = 0;
        rbdata->rectangle.height = 0;
        rbdata->pressed = TRUE;

        return GDK_FILTER_REMOVE;
      break;

      /* Finish dragging the rectangle out */
      case XI_ButtonRelease:
        if (rbdata->pressed)
          {
            if (rbdata->rectangle.width > 0 && rbdata->rectangle.height > 0)
              {
                /* Remove the rectangle drawn previously */
                TRACE ("Remove the rectangle drawn previously");

                XDrawRectangle (display,
                                root_window,
                                *rbdata->context,
                                rbdata->rectangle.x,
                                rbdata->rectangle.y,
                                (unsigned int) rbdata->rectangle.width-1,
                                (unsigned int) rbdata->rectangle.height-1);

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
      case XI_Motion:
        if (rbdata->pressed)
          {
            TRACE ("Mouse is moving");

            if (rbdata->rectangle.width > 0 && rbdata->rectangle.height > 0)
              {
                /* Remove the rectangle drawn previously */
                TRACE ("Remove the rectangle drawn previously");

                XDrawRectangle (display,
                                root_window,
                                *rbdata->context,
                                rbdata->rectangle.x,
                                rbdata->rectangle.y,
                                (unsigned int) rbdata->rectangle.width-1,
                                (unsigned int) rbdata->rectangle.height-1);
              }

            device_event = (XIDeviceEvent*) x_event->xcookie.data;
            x2 = device_event->root_x;
            y2 = device_event->root_y;

            rbdata->rectangle.x = MIN (rbdata->x1, x2);
            rbdata->rectangle.y = MIN (rbdata->y1, y2);
            rbdata->rectangle.width = ABS (x2 - rbdata->x1);
            rbdata->rectangle.height = ABS (y2 - rbdata->y1);

            /* Draw  the rectangle as the user drags the mouse */
            TRACE ("Draw the new rectangle");
            if (rbdata->rectangle.width > 0 && rbdata->rectangle.height > 0)
              {
                XDrawRectangle (display,
                                root_window,
                                *rbdata->context,
                                rbdata->rectangle.x,
                                rbdata->rectangle.y,
                                (unsigned int) rbdata->rectangle.width-1,
                                (unsigned int) rbdata->rectangle.height-1);
              }
          }
        return GDK_FILTER_REMOVE;
        break;

      case XI_KeyPress:
        device_event = (XIDeviceEvent*) x_event->xcookie.data;

        if (device_event->detail == XKeysymToKeycode (gdk_x11_get_default_xdisplay(), XK_Escape))
          {
            TRACE ("Escape key was pressed, cancel the screenshot.");

            if (rbdata->pressed)
              {
                if (rbdata->rectangle.width > 0 && rbdata->rectangle.height > 0)
                  {
                    /* Remove the rectangle drawn previously */
                    TRACE ("Remove the rectangle drawn previously");

                    XDrawRectangle (display,
                                   root_window,
                                   *rbdata->context,
                                   rbdata->rectangle.x,
                                   rbdata->rectangle.y,
                                   (unsigned int) rbdata->rectangle.width-1,
                                   (unsigned int) rbdata->rectangle.height-1);
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
*get_rectangle_screenshot (gint delay)
{
  GdkPixbuf *screenshot = NULL;
  GdkWindow *root_window;

  XGCValues gc_values;
  GC gc;

  Display *display;
  gint screen;
  RbData rbdata;
  GdkEventMask mask;
  GdkCursor *xhair_cursor;
  long value_mask;

  /* Get root window */
  TRACE ("Get the root window");
  root_window = gdk_get_default_root_window ();
  display = gdk_x11_get_default_xdisplay ();
  screen = gdk_x11_get_default_screen ();

  /*Set up graphics context for a XOR rectangle that will be drawn as
   * the user drags the mouse */
  TRACE ("Initialize the graphics context");

  gc_values.function = GXxor;
  gc_values.line_width = 2;
  gc_values.line_style = LineOnOffDash;
  gc_values.fill_style = FillSolid;
  gc_values.graphics_exposures = FALSE;
  gc_values.subwindow_mode = IncludeInferiors;
  gc_values.background = XBlackPixel (display, screen);
  gc_values.foreground = XWhitePixel (display, screen);

  value_mask = GCFunction | GCLineWidth | GCLineStyle |
               GCFillStyle | GCGraphicsExposures | GCSubwindowMode |
               GCBackground | GCForeground;

  gc = XCreateGC (display,
                  gdk_x11_get_default_root_xwindow (),
                  value_mask,
                  &gc_values);

  /* Change cursor to cross-hair */
  TRACE ("Set the cursor");

  xhair_cursor = gdk_cursor_new (GDK_CROSSHAIR);

  mask = GDK_POINTER_MOTION_MASK |
         GDK_BUTTON_PRESS_MASK |
         GDK_BUTTON_RELEASE_MASK;

  gdk_pointer_grab (root_window, FALSE, mask, NULL,
                    xhair_cursor, GDK_CURRENT_TIME);
  gdk_keyboard_grab (root_window, FALSE, GDK_CURRENT_TIME);

  /* Initialize the rubber band data */
  TRACE ("Initialize the rubber band data");
  rbdata.context = &gc;
  rbdata.pressed = FALSE;
  rbdata.cancelled = FALSE;

  /* Set the filter function to handle the GDK events */
  TRACE ("Add the events filter");
  gdk_window_add_filter (root_window,
                         (GdkFilterFunc) region_filter_func, &rbdata);

  gdk_flush ();

  gtk_main ();

  gdk_window_remove_filter (root_window,
                            (GdkFilterFunc) region_filter_func,
                            &rbdata);

  gdk_pointer_ungrab (GDK_CURRENT_TIME);
  gdk_keyboard_ungrab (GDK_CURRENT_TIME);

  /* Get the screenshot's pixbuf */
  if (G_LIKELY (!rbdata.cancelled))
    {
      TRACE ("Get the pixbuf for the screenshot");

      sleep(delay);

      screenshot =
        gdk_pixbuf_get_from_window (root_window,
                                    rbdata.rectangle.x,
                                    rbdata.rectangle.y,
                                    rbdata.rectangle.width,
                                    rbdata.rectangle.height);
    }

  if (G_LIKELY (gc != NULL))
    XFreeGC (display, gc);

  g_object_unref (xhair_cursor);

  return screenshot;
}



/* Public */



/**
 * screenshooter_take_screenshot:
 * @region: the region to be screenshoted. It can be FULLSCREEN,
 *          ACTIVE_WINDOW or SELECT.
 * @delay: the delay before the screenshot is taken, in seconds.
 * @mouse: whether the mouse pointer should be displayed on the screenshot.
 *
 * Takes a screenshot with the given options. If @region is FULLSCREEN,
 * the screenshot is taken after @delay seconds. If @region is
 * ACTIVE_WINDOW, a delay of @delay seconds elapses, then the active
 * window is detected and captured. If @region is SELECT, the user will
 * have to select a portion of the screen with the mouse. Then a delay of
 * @delay seconds elapses, and a screenshot is taken.
 *
 * @show_mouse is only taken into account when @region is FULLSCREEN
 * or ACTIVE_WINDOW.
 *
 * Return value: a #GdkPixbuf containing the screenshot or %NULL
 * (if @region is SELECT, the user can cancel the operation).
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
        screenshot = get_rectangle_screenshot (delay);
      else
        screenshot = get_rectangle_screenshot_composited (delay);
    }

  return screenshot;
}
