/*  $Id$
 *
 *  Copyright © 2008-2009 Jérôme Guelfucci <jerome.guelfucci@gmail.com>
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



static void     cb_help_response (GtkWidget *dlg,
                                  gint       response,
                                  gpointer   unused);



/* Internals */



static void
cb_help_response (GtkWidget *dialog, gint response, gpointer unused)
{
  if (response == GTK_RESPONSE_HELP)
    {
      GError *error_help = NULL;

      g_signal_stop_emission_by_name (dialog, "response");

      /* Launch the help page and show an error dialog if there was an error. */
      if (!g_spawn_command_line_async ("xfhelp4 xfce4-screenshooter.html", &error_help))
        {
          screenshooter_error ("%s", error_help->message);
          g_error_free (error_help);
        }
    }
}



/* Public */



gboolean screenshooter_take_and_output_screenshot (ScreenshotData *sd)
{
  GdkPixbuf *screenshot;

  if (sd->cli == FALSE)
    {
      GtkWidget *dialog;
      gint response;

      /* Set the dialog up */
      dialog = screenshooter_dialog_new (sd, FALSE);
      g_signal_connect (dialog, "response", (GCallback) cb_help_response, NULL);

      response = gtk_dialog_run (GTK_DIALOG (dialog));

      if (response == GTK_RESPONSE_OK)
        {
          gtk_widget_hide (dialog);
        }
      else
        {
          gtk_widget_destroy (dialog);
          gtk_main_quit ();

          return FALSE;
        }
    }

  screenshot = screenshooter_take_screenshot (sd->region, sd->delay, sd->show_mouse);

  g_return_val_if_fail (screenshot != NULL, FALSE);

  if (sd->action == SAVE)
    {
      if (sd->screenshot_dir == NULL)
        {
          sd->screenshot_dir = screenshooter_get_home_uri ();
        }

      screenshooter_save_screenshot (screenshot,
                                     sd->show_save_dialog,
                                     sd->screenshot_dir);
    }
  else if (sd->action == CLIPBOARD)
    {
      screenshooter_copy_to_clipboard (screenshot);
    }
  else
    {
      GFile *temp_dir = g_file_new_for_path (g_get_tmp_dir ());
      const gchar *temp_dir_uri = g_file_get_uri (temp_dir);
      const gchar *screenshot_path =
        screenshooter_save_screenshot (screenshot, FALSE, temp_dir_uri);

      if (screenshot_path != NULL)
        {
          if (sd->action == OPEN)
            {
              screenshooter_open_screenshot (screenshot_path, sd->app);
            }
#ifdef HAVE_XMLRPC
#ifdef HAVE_CURL
          else
            {
              screenshooter_upload_to_zimagez (screenshot_path, sd->last_user);
            }
#endif
#endif
        }

      g_object_unref (temp_dir);
    }

  g_object_unref (screenshot);

  if (sd->close == 1)
    {
      TRACE ("Exit the main loop");
      gtk_main_quit ();
      return FALSE;
    }
  else
    return TRUE;
}

