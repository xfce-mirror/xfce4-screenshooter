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



static void
cb_help_response (GtkWidget *dialog, gint response, gpointer unused)
{
  if (response == GTK_RESPONSE_HELP)
    {
      g_signal_stop_emission_by_name (dialog, "response");
      screenshooter_open_help ();
    }
}



/* Public */



gboolean screenshooter_take_screenshot_idle (ScreenshotData *sd)
{
  sd->screenshot = screenshooter_take_screenshot (sd->region,
                                                  sd->delay,
                                                  sd->show_mouse,
                                                  sd->plugin);

  if (sd->screenshot != NULL)
    g_idle_add ((GSourceFunc) screenshooter_action_idle, sd);
  else if (!sd->plugin)
    gtk_main_quit ();

  return FALSE;
}

gboolean screenshooter_action_idle (ScreenshotData *sd)
{
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
          return FALSE;
        }
    }

  if (sd->action == SAVE)
    {
      const gchar *save_location;

      if (sd->screenshot_dir == NULL)
        sd->screenshot_dir = screenshooter_get_xdg_image_dir_uri ();

      save_location = screenshooter_save_screenshot (sd->screenshot,
                                                     sd->screenshot_dir,
                                                     sd->title,
                                                     sd->timestamp,
                                                     TRUE,
                                                     sd->action_specified);

      if (save_location)
        {
          const gchar *temp;

          g_free (sd->screenshot_dir);
          temp = g_path_get_dirname (save_location);
          sd->screenshot_dir = g_build_filename ("file://", temp, NULL);
          TRACE ("New save directory: %s", sd->screenshot_dir);
        }
    }
  else if (sd->action == CLIPBOARD)
    {
      screenshooter_copy_to_clipboard (sd->screenshot);
    }
  else
    {
      GFile *temp_dir = g_file_new_for_path (g_get_tmp_dir ());
      const gchar *temp_dir_uri = g_file_get_uri (temp_dir);
      const gchar *screenshot_path =
        screenshooter_save_screenshot (sd->screenshot,
                                       temp_dir_uri,
                                       sd->title,
                                       sd->timestamp,
                                       FALSE,
                                       FALSE);

      if (screenshot_path != NULL)
        {
          if (sd->action == OPEN)
            screenshooter_open_screenshot (screenshot_path, sd->app);
          else
            {
              gchar *new_last_user = NULL;

              screenshooter_upload_to_zimagez (screenshot_path,
                                               sd->last_user,
                                               sd->title,
                                               &new_last_user);

              if (new_last_user)
                {
                  g_free (sd->last_user);
                  sd->last_user = new_last_user;
                }
            }
        }

      g_object_unref (temp_dir);
    }

  if (!sd->plugin)
    gtk_main_quit ();

  g_object_unref (sd->screenshot);

  return FALSE;
}

