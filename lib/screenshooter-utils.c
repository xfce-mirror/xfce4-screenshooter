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

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "screenshooter-utils.h"
#include <libxfce4ui/libxfce4ui.h>



/* Internals */



/* Replacement for gdk_screen_get_active_window */
static Window
get_active_window_from_xlib (void)
{
  GdkDisplay *display;
  Display *dsp;
  Atom active_win, type;
  int status, format;
  unsigned long n_items, bytes_after;
  unsigned char *prop;
  Window window;

  display = gdk_display_get_default ();
  dsp = gdk_x11_display_get_xdisplay (display);

  active_win = XInternAtom (dsp, "_NET_ACTIVE_WINDOW", True);
  if (active_win == None)
    return None;

  gdk_x11_display_error_trap_push (display);

  status = XGetWindowProperty (dsp, DefaultRootWindow (dsp),
                               active_win, 0, G_MAXLONG, False,
                               XA_WINDOW, &type, &format, &n_items,
                               &bytes_after, &prop);

  if (status != Success || type != XA_WINDOW)
    {
      if (prop)
        XFree (prop);

      gdk_x11_display_error_trap_pop_ignored (display);
      return None;
    }

  if (gdk_x11_display_error_trap_pop (display) != Success)
    {
      if (prop)
        XFree (prop);

      return None;
    }

  window = *(Window *) prop;
  XFree (prop);
  return window;
}



/* Public */



/* Copy the screenshot to the Clipboard.
* Code is from gnome-screenshooter.
* @screenshot: the screenshot
*/
void
screenshooter_copy_to_clipboard (GdkPixbuf *screenshot)
{
  GtkClipboard *clipboard;

  TRACE ("Adding the image to the clipboard...");

  clipboard =
    gtk_clipboard_get_for_display (gdk_display_get_default(), GDK_SELECTION_CLIPBOARD);

  gtk_clipboard_set_image (clipboard, screenshot);
}



/* Read the options from file and sets the sd values.
@file: the path to the rc file.
@sd: the ScreenshotData to be set.
@dir_only: if true, only read the screenshot_dir.
*/
void
screenshooter_read_rc_file (const gchar *file, ScreenshotData *sd)
{
  const gchar *default_uri = screenshooter_get_xdg_image_dir_uri ();

  XfceRc *rc;
  gint delay = 0;
  gint region = FULLSCREEN;
  gint action = SAVE;
  gint show_mouse = 1;
  gboolean timestamp = TRUE;
  gchar *screenshot_dir = g_strdup (default_uri);
  gchar *title = g_strdup (_("Screenshot"));
  gchar *app = g_strdup ("none");
  gchar *last_user = g_strdup ("");

  if (G_LIKELY (file != NULL))
    {
      TRACE ("Open the rc file");

      rc = xfce_rc_simple_open (file, TRUE);

      if (G_LIKELY (rc != NULL))
        {
          TRACE ("Read the entries");

          delay = xfce_rc_read_int_entry (rc, "delay", 0);
          region = xfce_rc_read_int_entry (rc, "region", FULLSCREEN);
          action = xfce_rc_read_int_entry (rc, "action", SAVE);
          show_mouse = xfce_rc_read_int_entry (rc, "show_mouse", 1);
          timestamp = xfce_rc_read_bool_entry (rc, "timestamp", TRUE);

          g_free (app);
          app = g_strdup (xfce_rc_read_entry (rc, "app", "none"));

          g_free (last_user);
          last_user = g_strdup (xfce_rc_read_entry (rc, "last_user", ""));

          g_free (screenshot_dir);
          screenshot_dir =
            g_strdup (xfce_rc_read_entry (rc, "screenshot_dir", default_uri));

          g_free (title);
          title =
            g_strdup (xfce_rc_read_entry (rc, "title", _("Screenshot")));

          TRACE ("Close the rc file");

          xfce_rc_close (rc);
        }
    }

  /* And set the sd values */
  TRACE ("Set the values of the struct");

  sd->delay = delay;
  sd->region = region;
  sd->action = action;
  sd->show_mouse = show_mouse;
  sd->timestamp = timestamp;
  sd->screenshot_dir = screenshot_dir;
  sd->title = title;
  sd->app = app;
  sd->app_info = NULL;
  sd->last_user = last_user;
}



/* Writes the options from sd to file.
@file: the path to the rc file.
@sd: a ScreenshotData.
*/
void
screenshooter_write_rc_file (const gchar *file, ScreenshotData *sd)
{
  XfceRc *rc;

  g_return_if_fail (file != NULL);

  TRACE ("Open the rc file");

  rc = xfce_rc_simple_open (file, FALSE);

  g_return_if_fail (rc != NULL);

  TRACE ("Write the entries.");

  xfce_rc_write_entry (rc, "app", sd->app);
  xfce_rc_write_entry (rc, "last_user", sd->last_user);
  xfce_rc_write_entry (rc, "screenshot_dir", sd->screenshot_dir);

  /* do not save if action was specified from cli */
  if (!sd->action_specified)
  {
    xfce_rc_write_int_entry (rc, "action", sd->action);
  }

  /* do not save if region was specified from cli */
  if (!sd->region_specified)
  {
    xfce_rc_write_int_entry (rc, "delay", sd->delay);
    xfce_rc_write_int_entry (rc, "region", sd->region);
    xfce_rc_write_int_entry (rc, "show_mouse", sd->show_mouse);
  }

  TRACE ("Flush and close the rc file");
  xfce_rc_close (rc);
}



/* Opens the screenshot using application.
@screenshot_path: the path to the saved screenshot.
@application: the command to run the application.
@app_info: GAppInfo object associated with application.
*/
void
screenshooter_open_screenshot (const gchar *screenshot_path, const gchar *application, GAppInfo *const app_info)
{
  gpointer screenshot_file = NULL;
  gboolean success = TRUE;
  gchar   *command;
  GError  *error = NULL;
  GList   *files = NULL;

  g_return_if_fail (screenshot_path != NULL);

  TRACE ("Path was != NULL");

  if (g_str_equal (application, "none"))
    return;

  TRACE ("Application was not none");

  if (app_info != NULL)
    {
      TRACE ("Launch the app");

      screenshot_file = g_file_new_for_path (screenshot_path);
      files = g_list_append (NULL, screenshot_file);
      success = g_app_info_launch (app_info, files, NULL, &error);
      g_list_free_full (files, g_object_unref);
    }
  else if (application != NULL)
    {
      TRACE ("Launch the command");

      command = g_strconcat (application, " ", "\"", screenshot_path, "\"", NULL);
      success = g_spawn_command_line_async (command, &error);
      g_free (command);
    }

  /* report any error */
  if (!success && error != NULL)
    {
      TRACE ("An error occured");
      screenshooter_error (_("<b>The application could not be launched.</b>\n%s"), error->message);
      g_error_free (error);
    }
}



gchar *screenshooter_get_home_uri (void)
{
  gchar *result = NULL;
  const gchar *home_path = g_getenv ("HOME");

  if (G_UNLIKELY (!home_path))
    home_path = g_get_home_dir ();

  result = g_strconcat ("file://", home_path, NULL);

  return result;
}



gchar *screenshooter_get_xdg_image_dir_uri (void)
{
  gchar *result, *tmp;

  tmp = g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_PICTURES));

  if (tmp == NULL)
    return screenshooter_get_home_uri ();

  result = g_strconcat ("file://", tmp, NULL);
  g_free (tmp);

  return result;
}


gboolean screenshooter_is_remote_uri (const gchar *uri)
{
  g_return_val_if_fail(uri != NULL, FALSE);

  /* if the URI doesn't start  with "file://", we take it as remote */

  if (G_UNLIKELY (!g_str_has_prefix (uri, "file:")))
    return TRUE;

  return FALSE;
}



/**
 * screenshooter_error:
 * @format: printf()-style format string
 * @...: arguments for @format
 *
 * Shows a modal error dialog with the given error text. Blocks until the user
 * clicks the OK button.
 **/
void screenshooter_error (const gchar *format, ...)
{
  va_list va_args;
  gchar *message = NULL;
  GtkWidget *dialog;

  g_return_if_fail (format != NULL);

  va_start (va_args, format);
  message = g_strdup_vprintf (format, va_args);
  va_end (va_args);

  dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_ERROR, GTK_BUTTONS_OK,
                                   NULL);
  gtk_message_dialog_set_markup (GTK_MESSAGE_DIALOG (dialog), message);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy  (dialog);

  g_free (message);
}

/**
 * screenshooter_get_datetime
 * @format - a date format string
 *
 * Builds a timestamp using local time.
 * Returned string should be released with g_free()
 **/
gchar *screenshooter_get_datetime (const gchar *format)
{
  gchar *timestamp;
  GDateTime *now = g_date_time_new_now_local();
  timestamp = g_date_time_format (now, format);

  g_date_time_unref (now);
  /* TODO: strip potential : and / if the format is configurable */
  DBG("datetime is %s", timestamp);
  return timestamp;
}



void screenshooter_open_help (GtkWindow *parent)
{
  xfce_dialog_show_help (parent, "screenshooter", "start", "");
}



gboolean
screenshooter_f1_key (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
  GtkWidget *window;

  if (event->keyval == GDK_KEY_F1)
    {
      window = gtk_widget_get_toplevel (widget);
      screenshooter_open_help (GTK_WINDOW (window));
      return TRUE;
    }

  return FALSE;
}



/* Replacement for gdk_screen_width/gdk_screen_height */
void
screenshooter_get_screen_geometry (GdkRectangle *geometry)
{
#if LIBXFCE4UI_CHECK_VERSION (4,14,0)
  GdkRectangle *geometry2 = xfce_gdk_screen_get_geometry ();
  geometry->width = geometry2->width;
  geometry->height = geometry2->height;
  g_free (geometry2);
#else
  GdkDisplay *display = gdk_display_get_default ();
  int num_monitors = gdk_display_get_n_monitors (display);

  gint x, y, w, h;

  x = y = G_MAXINT;
  w = h = G_MININT;

  for (int i = 0; i < num_monitors; i++)
    {
      GdkRectangle rect;
      GdkMonitor *monitor = gdk_display_get_monitor (display, i);
      gdk_monitor_get_geometry (monitor, &rect);

      x = MIN (x, rect.x);
      y = MIN (y, rect.y);
      w = MAX (w, rect.x + rect.width);
      h = MAX (h, rect.y + rect.height);
    }

  geometry->width = w - x;
  geometry->height = h - y;
#endif
}



GdkWindow*
screenshooter_get_active_window (GdkScreen *screen,
                                 gboolean  *needs_unref,
                                 gboolean  *border)
{
  GdkWindow *window, *window2;
  Window xwindow;
  GdkDisplay *display;

  TRACE ("Get the active window");

  display = gdk_display_get_default ();
  xwindow = get_active_window_from_xlib ();
  if (xwindow != None)
    window = gdk_x11_window_foreign_new_for_display (display, xwindow);
  else
    window = NULL;

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



gboolean
screenshooter_get_gtk_frame_extents (GdkWindow *window,
                                     GtkBorder *extents)
{
  /* Code adapted from gnome-flashback:
   * Copyright (C) 2015-2017 Alberts Muktupāvels
   * https://gitlab.gnome.org/GNOME/gnome-flashback/-/commit/f884127
   */

  GdkDisplay *display;
  Display *xdisplay;
  Window xwindow;
  Atom gtk_frame_extents;
  Atom type;
  gint format;
  gulong n_items;
  gulong bytes_after;
  guchar *data;
  gint result;
  gulong *borders;

  display = gdk_display_get_default ();
  xdisplay = gdk_x11_display_get_xdisplay (display);
  xwindow = gdk_x11_window_get_xid (window);
  gtk_frame_extents = XInternAtom (xdisplay, "_GTK_FRAME_EXTENTS", False);

  gdk_x11_display_error_trap_push (display);
  result = XGetWindowProperty (xdisplay, xwindow, gtk_frame_extents,
                               0, G_MAXLONG, False, XA_CARDINAL,
                               &type, &format, &n_items, &bytes_after, &data);
  gdk_x11_display_error_trap_pop_ignored (display);

  if (data == NULL)
    return FALSE;

  if (result != Success || type != XA_CARDINAL || format != 32 || n_items != 4)
    {
      XFree (data);
      return FALSE;
    }

  borders = (gulong *) data;

  extents->left = borders[0];
  extents->right = borders[1];
  extents->top = borders[2];
  extents->bottom = borders[3];

  XFree (data);
  return TRUE;
}
