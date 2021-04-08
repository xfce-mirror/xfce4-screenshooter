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
 * */

#include "screenshooter-actions.h"
#include "screenshooter-utils.h"
#include "screenshooter-capture.h"
#include "screenshooter-global.h"
#include "screenshooter-dialogs.h"
#include "screenshooter-imgur.h"

static void
cb_help_response (GtkWidget *dialog, gint response, gpointer unused)
{
  if (response == GTK_RESPONSE_HELP)
    {
      g_signal_stop_emission_by_name (dialog, "response");
      screenshooter_open_help (GTK_WINDOW (dialog));
    }
}



static gboolean
action_idle (gpointer user_data)
{
  ScreenshotData *sd = user_data;

  if (!sd->action_specified)
    {
      GtkWidget *dialog = screenshooter_actions_dialog_new (sd);
      gint response;

      g_signal_connect (dialog, "response",
                        G_CALLBACK (cb_help_response), NULL);
      g_signal_connect (dialog, "key-press-event",
                        G_CALLBACK (screenshooter_f1_key), NULL);

      response = gtk_dialog_run (GTK_DIALOG (dialog));

      gtk_widget_destroy (dialog);

      if (response == GTK_RESPONSE_CANCEL ||
          response == GTK_RESPONSE_DELETE_EVENT ||
          response == GTK_RESPONSE_CLOSE)
        {
          if (!sd->plugin)
            gtk_main_quit ();

          g_object_unref (sd->screenshot);
          return FALSE;
        }
    }

  if (sd->action & CLIPBOARD)
    screenshooter_copy_to_clipboard (sd->screenshot);

  if (sd->action & SAVE)
    {
      if (!sd->path_is_dir)
        screenshooter_save_screenshot_to (sd->screenshot, sd->screenshot_dir);
      else
        {
          gchar *filename;
          const gchar *save_location, *temp;

          if (sd->screenshot_dir == NULL)
            sd->screenshot_dir = screenshooter_get_xdg_image_dir_uri ();

          filename = screenshooter_get_filename_for_uri (sd->screenshot_dir, sd->title, sd->timestamp);

          save_location = screenshooter_save_screenshot (sd->screenshot,
                                                         sd->screenshot_dir,
                                                         filename,
                                                         TRUE,
                                                         TRUE);

          g_free (filename);

          if (save_location != NULL)
            {
              g_free (sd->screenshot_dir);
              temp = g_path_get_dirname (save_location);
              sd->screenshot_dir = g_build_filename ("file://", temp, NULL);
              TRACE ("New save directory: %s", sd->screenshot_dir);
            }
          else if (!sd->action_specified) {
              /* Show actions dialog again if no action was specified from CLI */
              return TRUE;
          }
        }
    }
  else
    {
      GFile *temp_dir = g_file_new_for_path (g_get_tmp_dir ());
      gchar *temp_dir_uri = g_file_get_uri (temp_dir);
      gchar *filename = screenshooter_get_filename_for_uri (temp_dir_uri, sd->title, sd->timestamp);
      gchar *screenshot_path =
        screenshooter_save_screenshot (sd->screenshot,
                                       temp_dir_uri,
                                       filename,
                                       FALSE,
                                       FALSE);
      g_object_unref (temp_dir);
      g_free (temp_dir_uri);
      g_free (filename);

      if (screenshot_path != NULL)
        {
          if (sd->action & OPEN)
            screenshooter_open_screenshot (screenshot_path, sd->app, sd->app_info);
          else if (sd->action & UPLOAD_IMGUR)
            {
              if (!sd->action_specified && !screenshooter_upload_to_imgur (screenshot_path, sd->title))
                {
                  g_free (screenshot_path);
                  /* Show actions dialog again if no action was specified from CLI*/
                  return TRUE;
                }
            }

          g_free (screenshot_path);
        }
    }

  if (!sd->plugin)
    gtk_main_quit ();

  g_object_unref (sd->screenshot);

  return FALSE;
}



static gboolean
take_screenshot_idle (gpointer user_data)
{
  ScreenshotData *sd = user_data;

  sd->screenshot = screenshooter_capture_screenshot (sd->region,
                                                  sd->delay,
                                                  sd->show_mouse,
                                                  sd->plugin);

  if (sd->screenshot != NULL)
    g_idle_add (action_idle, sd);
  else if (!sd->plugin)
    gtk_main_quit ();

  return FALSE;
}



/* Public */



void
screenshooter_take_screenshot (ScreenshotData *sd, gboolean immediate)
{
  gint delay;

  if (sd->region == SELECT)
    {
      /* The delay will be applied after the rectangle selection */
      g_idle_add (take_screenshot_idle, sd);
      return;
    }

  if (sd->delay == 0 && immediate)
    {
      /* If delay is zero and the region was passed as an argument (from cli
       * or panel plugin), thus the first dialog was not shown, we will take
       * the screenshot immediately without a minimal delay */
      g_idle_add (take_screenshot_idle, sd);
      return;
    }

  /* Await the amount of the time specified by the user before capturing the
   * screenshot, but not less than 200ms, otherwise the first dialog might
   * appear on the screenshot. */
  delay = sd->delay == 0 ? 200 : sd->delay * 1000;
  g_timeout_add (delay, take_screenshot_idle, sd);
}
