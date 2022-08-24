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

#include <xfconf/xfconf.h>
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
  gchar *save_location = NULL;
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

      if (response == GTK_RESPONSE_REJECT)
        {
          g_object_unref (sd->screenshot);

          /* If the user clicked 'Back' button */
          screenshooter_region_dialog_show (sd, FALSE);
          return FALSE;
        }
    }

  if (sd->action & CLIPBOARD)
    screenshooter_copy_to_clipboard (sd->screenshot);

  if (sd->action & SAVE)
    {
      if (!sd->path_is_dir)
        save_location = screenshooter_save_screenshot_to (sd->screenshot, sd->screenshot_dir);
      else
        {
          gchar *filename;
          const gchar *temp;

          if (sd->screenshot_dir == NULL)
            sd->screenshot_dir = screenshooter_get_xdg_image_dir_uri ();

          filename = screenshooter_get_filename_for_uri (sd->screenshot_dir,
                                                         sd->title,
                                                         sd->last_extension,
                                                         sd->timestamp);

          save_location = screenshooter_save_screenshot (sd->screenshot,
                                                         sd->screenshot_dir,
                                                         filename,
                                                         sd->last_extension,
                                                         TRUE,
                                                         TRUE);

          g_free (filename);

          if (save_location)
            {
              g_free (sd->screenshot_dir);
              temp = g_path_get_dirname (save_location);
              sd->screenshot_dir = g_build_filename ("file://", temp, NULL);
              TRACE ("New save directory: %s", sd->screenshot_dir);
            }
          else if (!sd->action_specified)
            {
              /* Show actions dialog again if no action was specified from CLI */
              return TRUE;
            }
        }

      if (sd->show_in_folder)
        screenshooter_show_file_in_folder (save_location);
    }
  else
    {
      GFile *temp_dir = g_file_new_for_path (g_get_tmp_dir ());
      gchar *temp_dir_uri = g_file_get_uri (temp_dir);
      gchar *filename = screenshooter_get_filename_for_uri (temp_dir_uri,
                                                            sd->title,
                                                            sd->last_extension,
                                                            sd->timestamp);
      save_location = screenshooter_save_screenshot (sd->screenshot,
                                                     temp_dir_uri,
                                                     filename,
                                                     sd->last_extension,
                                                     FALSE,
                                                     FALSE);

      if (save_location)
        {
          if (sd->action & OPEN)
            {
              screenshooter_open_screenshot (save_location, sd->app, sd->app_info);
            }
          else if (sd->action & UPLOAD_IMGUR)
            {
              gboolean upload_successful = screenshooter_upload_to_imgur (save_location, sd->title);

              /* If upload failed, regardless of whether it was chosen by GUI or CLI, and
               * the action was not selected via CLI, show the actions dialog again.*/
              if (!upload_successful && !sd->action_specified)
                {
                  g_free (save_location);
                  return TRUE;
                }
            }
          else if (sd->action & CUSTOM_ACTION)
            {
              screenshooter_custom_action_execute (filename, sd->custom_action_name, sd->custom_action_command);
            }
        }
      g_object_unref (temp_dir);
      g_free (temp_dir_uri);
      g_free (filename);
    }

  /* Persist last used file extension */
  if (save_location)
    {
      gchar *extension = NULL;

      if (G_LIKELY (g_str_has_suffix (save_location, ".png")))
        extension = g_strdup ("png");
      else if (G_UNLIKELY (g_str_has_suffix (save_location, ".jpg") || g_str_has_suffix (save_location, ".jpeg")))
        extension = g_strdup ("jpg");
      else if (G_UNLIKELY (g_str_has_suffix (save_location, ".bmp")))
        extension = g_strdup ("bmp");
      else if (G_UNLIKELY (g_str_has_suffix (save_location, ".webp")))
        extension = g_strdup ("webp");

      if (extension)
        {
          g_free (sd->last_extension);
          sd->last_extension = extension;
        }

      g_free (save_location);
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
                                                     sd->show_border,
                                                     sd->plugin);

  if (sd->screenshot != NULL)
    g_idle_add (action_idle, sd);
  else if (!sd->plugin)
    gtk_main_quit ();

  return FALSE;
}



static gchar**
screenshooter_parse_envp (gchar **cmd)
{
  gchar **vars;
  gchar **envp;
  gint offset = 0;

  vars = g_strsplit (*cmd, " ", -1);
  envp = g_get_environ ();

  for (gint n = 0; vars[n] != NULL; ++n)
    {
      gchar *var, *val;
      gchar *index = g_strrstr (vars[n], "=");

      if (index == NULL)
        break;

      offset += strlen (vars[n]);

      var = g_strndup (vars[n], index - vars[n]);
      val = g_strdup (index + 1);
      envp = g_environ_setenv (envp, var, val, TRUE);

      g_free (var);
      g_free (val);
    }

  if (offset > 0)
    {
      gchar *temp = g_strdup (*cmd + offset + 1);
      g_free (*cmd);
      *cmd = temp;
    }

  g_strfreev (vars);

  return envp;
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



/* Custom Actions */



void
screenshooter_custom_action_save (GtkTreeModel *list_store)
{
  GtkTreeIter iter;
  gboolean valid;
  gint32 max_id;
  gint32 id = 0;
  XfconfChannel *channel;
  GError *error=NULL;

  if (!xfconf_init (&error))
    {
      g_critical ("Failed to initialized xfconf");
      g_error_free (error);
      return;
    }
  channel = xfconf_channel_get ("screenshooter");

  max_id = xfconf_channel_get_int (channel, "/actions/actions", 0);

  valid = gtk_tree_model_get_iter_first (list_store, &iter);
  while (valid)
    {
      gchar *name;
      gchar *command;
      gchar name_address[50];
      gchar command_address[50];

      gtk_tree_model_get (list_store, &iter,
                          CUSTOM_ACTION_NAME, &name,
                          CUSTOM_ACTION_COMMAND, &command,
                          -1);

      // Storing the values
      g_sprintf (name_address, "/actions/action-%d/name", id);
      g_sprintf (command_address, "/actions/action-%d/command", id);

      xfconf_channel_set_string (channel, name_address, name);
      xfconf_channel_set_string (channel, command_address, command);

      id++;
      valid = gtk_tree_model_iter_next (list_store, &iter);
    }

  for (gint32 i=id; i<max_id; i++)
    {
      gchar base[50];
      g_sprintf (base, "/actions/action-%d", i);
      xfconf_channel_reset_property (channel, base, TRUE);
    }
  xfconf_channel_set_int (channel, "/actions/actions", id);
  xfconf_shutdown ();
}



void
screenshooter_custom_action_load (GtkListStore *list_store)
{
  gint32 max_id;
  gint32 id;
  XfconfChannel *channel;
  GtkTreeIter iter;
  GError *error=NULL;

  if (!xfconf_init (&error))
    {
      g_critical ("Failed to initialized xfconf");
      g_error_free (error);
      return;
    }
  channel = xfconf_channel_get ("screenshooter");

  max_id = xfconf_channel_get_int (channel, "/actions/actions", 0);
  for (id=0; id<max_id; id++)
    {
      gchar *name = "";
      gchar *command = "";
      gchar name_address[50];
      gchar command_address[50];

      g_sprintf (name_address, "/actions/action-%d/name", id);
      g_sprintf (command_address, "/actions/action-%d/command", id);

      name = xfconf_channel_get_string (channel, name_address, "");
      command = xfconf_channel_get_string (channel, command_address, "");

      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (GTK_LIST_STORE (list_store), &iter, CUSTOM_ACTION_NAME, name, CUSTOM_ACTION_COMMAND, command, -1);
    }
  xfconf_shutdown ();
}



void screenshooter_custom_action_execute (gchar *filename, gchar *name, gchar *command) {
  gchar **split;
  gchar **argv;
  gchar **envp;
  GError *error=NULL;

  if (name==NULL || command==NULL)
    {
      xfce_dialog_show_warning (NULL, "Unable to execute the custom action", "Invalid custom action selected");
      return;
    }

  split = g_strsplit (command, "%s", -1);
  command = g_strjoinv (filename, split);

  command = xfce_expand_variables (command, NULL);
  envp = screenshooter_parse_envp (&command);

  if (G_LIKELY (g_shell_parse_argv (command, NULL, &argv, &error)))
    if (!g_spawn_sync (NULL, argv, envp, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, NULL, &error))
      {
        xfce_dialog_show_error (NULL, error, "Failed to run the custom action %s", name);
        g_error_free (error);
      }

  g_free (name);
  g_free (command);
  g_strfreev (split);
  g_strfreev (argv);
  g_strfreev (envp);
}