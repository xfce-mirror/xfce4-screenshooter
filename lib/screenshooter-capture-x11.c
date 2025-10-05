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

#include "screenshooter-capture-x11.h"

#ifdef HAVE_XFIXES
#include <X11/extensions/Xfixes.h>
#endif

#include <X11/extensions/shape.h>
#include <gdk/gdkx.h>
#include <unistd.h>

#include <libxfce4ui/libxfce4ui.h>

#include "screenshooter-global.h"
#include "screenshooter-select.h"
#include "screenshooter-utils-x11.h"



/* Prototypes */

static void             free_pixmap_data                    (guchar *pixels,
                                                             gpointer data);
static GdkPixbuf       *get_cursor_pixbuf                   (GdkDisplay *display,
                                                             GdkWindow *root,
                                                             gint *cursorx,
                                                             gint *cursory,
                                                             gint *xhot,
                                                             gint *yhot);
static void             capture_cursor                      (GdkPixbuf      *screenshot,
                                                             GtkBorder      *window_extents,
                                                             gint            scale,
                                                             gint            x,
                                                             gint            y,
                                                             gint            w,
                                                             gint            h);
static GdkPixbuf       *get_window_screenshot               (GdkWindow      *window,
                                                             gboolean        show_mouse,
                                                             gboolean        border);
static GdkPixbuf       *get_rectangle_screenshot            (gint            delay,
                                                             gboolean        show_mouse);



/* Internals */


static Window
find_wm_xid (GdkWindow *window)
{
  Window xid, root, parent, *children;
  unsigned int nchildren;

  if (window == gdk_get_default_root_window ())
    return None;

  xid = GDK_WINDOW_XID (window);

  do
    {
      if (XQueryTree (GDK_DISPLAY_XDISPLAY (gdk_display_get_default ()),
                      xid, &root, &parent, &children, &nchildren) == 0)
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

  /* cursors are not scaled */
  if (gdk_window_get_scale_factor (root) != 1)
    return NULL;

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

  g_object_unref (cursor);

  return cursor_pixbuf;
}



static void capture_cursor (GdkPixbuf *screenshot,
                            GtkBorder *window_extents,
                            gint scale,
                            gint x,
                            gint y,
                            gint w,
                            gint h)
{
  gint cursorx, cursory, xhot, yhot;
  GdkPixbuf *cursor_pixbuf;
  GdkRectangle rectangle_window, rectangle_cursor;

  cursor_pixbuf = get_cursor_pixbuf (gdk_display_get_default (),
                                     gdk_get_default_root_window (),
                                     &cursorx, &cursory, &xhot, &yhot);

  if (G_UNLIKELY (cursor_pixbuf == NULL))
    return;

  /* rectangle_window stores the window coordinates */
  rectangle_window.x = x * scale;
  rectangle_window.y = y * scale;
  rectangle_window.width = w * scale;
  rectangle_window.height = h * scale;

  if (window_extents != NULL)
    {
      rectangle_window.x += window_extents->left - 1;
      rectangle_window.y += window_extents->top - 1;
      rectangle_window.width -= window_extents->left + window_extents->right + 2;
      rectangle_window.height -= window_extents->top + window_extents->bottom + 2;
    }

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
      int dest_x, dest_y;

      TRACE ("Compose the two pixbufs");

      dest_x = cursorx - rectangle_window.x - xhot;
      dest_y = cursory - rectangle_window.y - yhot;

      gdk_pixbuf_composite (cursor_pixbuf, screenshot,
                            CLAMP (dest_x, 0, dest_x),
                            CLAMP (dest_y, 0, dest_y),
                            rectangle_cursor.width,
                            rectangle_cursor.height,
                            dest_x,
                            dest_y,
                            1.0, 1.0,
                            GDK_INTERP_BILINEAR,
                            255);
    }

  g_object_unref (cursor_pixbuf);
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

  gint scale;
  GdkRectangle real_coords;
  GdkRectangle screen_geometry;
  GtkBorder extents;
  gboolean has_extents;

  Window wm_xid;

  /* Get the root window */
  TRACE ("Get the root window");

  root = gdk_get_default_root_window ();

  if ((has_extents = xfce_has_gtk_frame_extents (window, &extents)))
    border = FALSE;

  if (border) {
    gdk_window_get_frame_extents (window, &real_coords);
  } else {
    real_coords.width = gdk_window_get_width (window);
    real_coords.height = gdk_window_get_height (window);
    gdk_window_get_origin (window, &real_coords.x, &real_coords.y);
  }

  /* Don't grab things offscreen. */

  TRACE ("Make sure we don't grab things offscreen");

  x_orig = real_coords.x;
  y_orig = real_coords.y;
  width  = real_coords.width;
  height = real_coords.height;

  screenshooter_get_screen_geometry (&screen_geometry);

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

  if (x_orig + width > screen_geometry.width)
    width = screen_geometry.width - x_orig;

  if (y_orig + height > screen_geometry.height)
    height = screen_geometry.height - y_orig;

  scale = gdk_window_get_scale_factor (window);

  TRACE ("Grab the screenshot");

  if (!has_extents)
    screenshot = screenshooter_pixbuf_get_from_window (root, x_orig, y_orig, width, height);
  else
    {
      GdkRectangle rect;
      gdk_window_get_frame_extents (window, &rect);

      /* Add one pixel to sides so the border is visible */
      rect.x = extents.left / scale - 1;
      rect.y = extents.top / scale - 1;
      rect.width -= (extents.left + extents.right) / scale - 2;
      rect.height -= (extents.top + extents.bottom) / scale - 2;
      screenshot = screenshooter_pixbuf_get_from_window (window, rect.x, rect.y, rect.width, rect.height);
    }

  /* Code adapted from gnome-screenshot:
   * Copyright (C) 2001-2006  Jonathan Blandford <jrb@alum.mit.edu>
   * Copyright (C) 2008 Cosimo Cecchi <cosimoc@gnome.org>
   */
  wm_xid = find_wm_xid (window);

  if (border && wm_xid != None)
    {
      GdkRectangle wm_coords;
      XRectangle *rectangles;
      int rectangle_count, rectangle_order, i;
      GtkBorder frame_offset = { 0, 0, 0, 0 };

      GdkWindow *wm_window = gdk_x11_window_foreign_new_for_display
        (gdk_window_get_display (window), wm_xid);

      gdk_window_get_frame_extents (wm_window, &wm_coords);

       /* Calculate frame_offset */
      frame_offset.left = (gdouble) (real_coords.x - wm_coords.x);
      frame_offset.top = (gdouble) (real_coords.y - wm_coords.y);
      frame_offset.right = (gdouble) (wm_coords.width - real_coords.width - frame_offset.left);
      frame_offset.bottom = (gdouble) (wm_coords.height - real_coords.height - frame_offset.top);

      /* we must use XShape to avoid showing what's under the rounder corners
       * of the WM decoration.
       */
      rectangles = XShapeGetRectangles (GDK_DISPLAY_XDISPLAY (gdk_display_get_default()),
                                        wm_xid,
                                        ShapeBounding,
                                        &rectangle_count,
                                        &rectangle_order);
      if (rectangles && rectangle_count > 0)
        {
          gboolean has_alpha = gdk_pixbuf_get_has_alpha (screenshot);
          GdkPixbuf *tmp = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8,
                                           gdk_pixbuf_get_width (screenshot),
                                           gdk_pixbuf_get_height (screenshot));

          gdk_pixbuf_fill (tmp, 0);

          for (i = 0; i < rectangle_count; i++)
            {
              gint rec_x, rec_y;
              gint rec_width, rec_height;

              /* If we're using invisible borders, the ShapeBounding might not
               * have the same size as the frame extents, as it would include the
               * areas for the invisible borders themselves.
               * In that case, trim every rectangle we get by the offset between the
               * WM window size and the frame extents.
               *
               * Note that the XShape values are in actual pixels, whereas the GDK
               * ones are in display pixels (i.e. scaled), so we need to apply the
               * scale factor to the former to use display pixels for all our math.
               */
              rec_x = rectangles[i].x / scale;
              rec_y = rectangles[i].y / scale;
              rec_width = rectangles[i].width / scale - (frame_offset.left + frame_offset.right);
              rec_height = rectangles[i].height / scale - (frame_offset.top + frame_offset.bottom);

              if (real_coords.x < 0)
                {
                  rec_x += real_coords.x;
                  rec_x = MAX(rec_x, 0);
                  rec_width += real_coords.x;
                }

              if (real_coords.y < 0)
                {
                  rec_y += real_coords.y;
                  rec_y = MAX(rec_y, 0);
                  rec_height += real_coords.y;
                }

              if (x_orig + rec_x + rec_width > screen_geometry.width)
                rec_width = screen_geometry.width - x_orig - rec_x;

              if (y_orig + rec_y + rec_height > screen_geometry.height)
                rec_height = screen_geometry.height - y_orig - rec_y;

              /* Undo the scale factor in order to copy the pixbuf data pixel-wise */
              for (gint y = rec_y * scale; y < (rec_y + rec_height) * scale; y++)
                {
                  guchar *src_pixels, *dest_pixels;
                  gint x;

                  src_pixels = gdk_pixbuf_get_pixels (screenshot)
                             + y * gdk_pixbuf_get_rowstride(screenshot)
                             + rec_x * scale * (has_alpha ? 4 : 3);
                  dest_pixels = gdk_pixbuf_get_pixels (tmp)
                              + y * gdk_pixbuf_get_rowstride (tmp)
                              + rec_x * scale * 4;

                  for (x = 0; x < rec_width * scale; x++)
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

          g_set_object (&screenshot, tmp);

          XFree (rectangles);
        }
    }

  if (show_mouse)
    capture_cursor (screenshot, has_extents ? &extents : NULL,
                    scale, x_orig, y_orig, width, height);

  return screenshot;
}



static GdkPixbuf
*get_rectangle_screenshot (gint delay, gboolean show_mouse)
{
  GdkRectangle region;
  GdkPixbuf *screenshot;
  GdkWindow *root;
  int root_width, root_height;

  if (G_UNLIKELY (!screenshooter_select_region (&region)))
    return NULL;

  root = gdk_get_default_root_window ();
  root_width = gdk_window_get_width (root);
  root_height = gdk_window_get_height (root);

  /* Avoid rectangle parts outside the screen */
  if (region.x < 0)
    region.width += region.x;
  if (region.y < 0)
    region.height += region.y;

  region.x = MAX(0, region.x);
  region.y = MAX(0, region.y);

  if (region.x + region.width > root_width)
    region.width = root_width - region.x;
  if (region.y + region.height > root_height)
    region.height = root_height - region.y;

  /* Await the specified delay, but not less than 200ms */
  if (delay == 0)
    g_usleep (200000);
  else
    sleep (delay);

  /* Get the screenshot's pixbuf */
  TRACE ("Get the pixbuf for the screenshot");
  screenshot = screenshooter_pixbuf_get_from_window (root, region.x, region.y, region.width, region.height);

  if (show_mouse)
    capture_cursor (screenshot, NULL, gdk_window_get_scale_factor (root), region.x, region.y, region.width, region.height);

  return screenshot;
}



/* Public */



/**
 * screenshooter_capture_screenshot_x11:
 * @region: the region to be captured. It can be FULLSCREEN,
 *          ACTIVE_WINDOW or SELECT.
 * @delay: the delay before the screenshot is captured, in seconds.
 * @show_mouse: whether the mouse pointer should be displayed on the
 *              screenshot.
 * @show_border: whether the window border should be displayed on the
 *               screenshot.
 *
 * Captures a screenshot with the given options. If @region is FULLSCREEN,
 * the screenshot is captured after @delay seconds. If @region is
 * ACTIVE_WINDOW, a delay of @delay seconds elapses, then the active
 * window is detected and captured. If @region is SELECT, the user will
 * have to select a portion of the screen with the mouse. Then a delay of
 * @delay seconds elapses, and a screenshot is captured.
 *
 * @show_mouse is only taken into account when @region is FULLSCREEN
 * or ACTIVE_WINDOW.
 *
 * @show_border is only taken into account when @region is ACTIVE_WINDOW
 * and if the window uses server side decorations.
 *
 * Return value: a #GdkPixbuf containing the screenshot or %NULL
 * (if @region is SELECT, the user can cancel the operation).
 **/
GdkPixbuf *screenshooter_capture_screenshot_x11 (gint     region,
                                                 gint     delay,
                                                 gboolean show_mouse,
                                                 gboolean show_border)
{
  GdkPixbuf *screenshot = NULL;
  GdkWindow *window = NULL;
  GdkScreen *screen;
  GdkDisplay *display;

  screen = gdk_screen_get_default ();
  display = gdk_display_get_default ();

  /* Sync the display */
  gdk_display_sync (display);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  gdk_window_process_all_updates ();
G_GNUC_END_IGNORE_DEPRECATIONS

  if (region == FULLSCREEN)
    {
      TRACE ("Get the screenshot of the entire screen");
      window = gdk_get_default_root_window ();
      screenshot = get_window_screenshot (window, show_mouse, FALSE);
    }
  else if (region == ACTIVE_WINDOW)
    {
      gboolean border;
      gboolean needs_unref = TRUE;

      TRACE ("Get the screenshot of the active window");
      window = screenshooter_get_active_window (screen, &needs_unref, &border);
      screenshot = get_window_screenshot (window, show_mouse, show_border && border);
      if (needs_unref)
        g_object_unref (window);
    }
  else if (region == SELECT)
    {
      TRACE ("Let the user select the region to screenshot");
      screenshot = get_rectangle_screenshot (delay, show_mouse);
    }

  return screenshot;
}
