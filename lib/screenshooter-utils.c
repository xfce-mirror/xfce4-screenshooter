/*  $Id$
 *
 *  Copyright © 2008-2009 Jérôme Guelfucci <jeromeg@xfce.org>
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

#include "screenshooter-utils.h"



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

  gtk_clipboard_store (clipboard);
}



/* Read the options from file and sets the sd values.
@file: the path to the rc file.
@sd: the ScreenshotData to be set.
@dir_only: if true, only read the screenshot_dir.
*/
void
screenshooter_read_rc_file (const gchar *file, ScreenshotData *sd)
{
  const gchar *home_uri = screenshooter_get_home_uri ();

  XfceRc *rc;
  gint delay = 0;
  gint region = FULLSCREEN;
  gint action = SAVE;
  gint show_save_dialog = 1;
  gint show_mouse = 1;
  gint close_app = 1;
  gchar *screenshot_dir = g_strdup (home_uri);
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
          show_save_dialog = xfce_rc_read_int_entry (rc, "show_save_dialog", 1);
          show_mouse = xfce_rc_read_int_entry (rc, "show_mouse", 1);
          close_app = xfce_rc_read_int_entry (rc, "close", 1);

          g_free (app);
          app = g_strdup (xfce_rc_read_entry (rc, "app", "none"));

          g_free (last_user);
          last_user = g_strdup (xfce_rc_read_entry (rc, "last_user", ""));

          g_free (screenshot_dir);
          screenshot_dir =
            g_strdup (xfce_rc_read_entry (rc, "screenshot_dir", home_uri));
        }

      TRACE ("Close the rc file");

      xfce_rc_close (rc);
    }

  /* And set the sd values */
  TRACE ("Set the values of the struct");

  sd->delay = delay;
  sd->region = region;
  sd->action = action;
  sd->show_save_dialog = show_save_dialog;
  sd->show_mouse = show_mouse;
  sd->close = close_app;
  sd->screenshot_dir = screenshot_dir;
  sd->app = app;
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

  xfce_rc_write_int_entry (rc, "delay", sd->delay);
  xfce_rc_write_int_entry (rc, "region", sd->region);
  xfce_rc_write_int_entry (rc, "action", sd->action);
  xfce_rc_write_int_entry (rc, "show_save_dialog", sd->show_save_dialog);
  xfce_rc_write_int_entry (rc, "show_mouse", sd->show_mouse);
  xfce_rc_write_int_entry (rc, "close", sd->close);
  xfce_rc_write_entry (rc, "screenshot_dir", sd->screenshot_dir);
  xfce_rc_write_entry (rc, "app", sd->app);
  xfce_rc_write_entry (rc, "last_user", sd->last_user);

  TRACE ("Flush and close the rc file");
  xfce_rc_flush (rc);
  xfce_rc_close (rc);
}



/* Opens the screenshot using application.
@screenshot_path: the path to the saved screenshot.
@application: the command to run the application.
*/
void
screenshooter_open_screenshot (const gchar *screenshot_path, const gchar *application)
{
  gchar *command;
  GError *error = NULL;

  g_return_if_fail (screenshot_path != NULL);

  TRACE ("Path was != NULL");

  g_return_if_fail (!g_str_equal (application, "none"));

  TRACE ("Application was not none");

  command = g_strconcat (application, " ", screenshot_path, NULL);

  TRACE ("Launch the command");

  /* Execute the command and show an error dialog if there was
  * an error. */
  if (!g_spawn_command_line_async (command, &error))
    {
      TRACE ("An error occured");

      screenshooter_error ("%s", error->message);
      g_error_free (error);
    }

  g_free (command);
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



gboolean screenshooter_is_remote_uri (const gchar *uri)
{
  g_return_val_if_fail(uri != NULL, FALSE);

  /* if the URI doesn't start  with "file://", we take it as remote */

  if (G_UNLIKELY (!g_str_has_prefix (uri, "file:")))
    return TRUE;

  return FALSE;
}



gchar *rot13 (gchar *string)
{
  gchar *result = string;

  for (; *string; string++)
    if (*string >= 'a' && *string <= 'z')
      *string = (*string - 'a' + 13) % 26 + 'a';
    else if (*string >= 'A' && *string <= 'Z')
      *string = (*string - 'A' + 13) % 26 + 'A';

  return result;
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

  dialog =
    gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                            GTK_BUTTONS_OK, "%s", message);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy  (dialog);

  g_free (message);
}

