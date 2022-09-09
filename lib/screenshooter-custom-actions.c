/*  $Id$
 *
 *  Copyright © 2022 Yogesh Kaushik <masterlukeskywalker02@gmail.com>
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

#include "screenshooter-custom-actions.h"



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



void
screenshooter_custom_action_save (GtkTreeModel *list_store)
{
  GtkTreeIter iter;
  gboolean valid;
  gint32 max_id;
  gint32 id = 0;
  XfconfChannel *channel;
  GError *error = NULL;

  if (!xfconf_init (&error))
    {
      g_critical ("Failed to initialized xfconf");
      g_error_free (error);
      return;
    }
  channel = xfconf_channel_get ("xfce4-screenshooter");

  max_id = xfconf_channel_get_int (channel, "/actions/actions", 0);

  valid = gtk_tree_model_get_iter_first (list_store, &iter);
  while (valid)
    {
      gchar *name;
      gchar *command;
      gchar *name_address;
      gchar *command_address;

      gtk_tree_model_get (list_store, &iter,
                          CUSTOM_ACTION_NAME, &name,
                          CUSTOM_ACTION_COMMAND, &command,
                          -1);

      /* Storing the values */
      name_address = g_strdup_printf ("/actions/action-%d/name", id);
      command_address = g_strdup_printf ("/actions/action-%d/command", id);

      xfconf_channel_set_string (channel, name_address, name);
      xfconf_channel_set_string (channel, command_address, command);

      id++;
      valid = gtk_tree_model_iter_next (list_store, &iter);

      g_free (name_address);
      g_free (command_address);
    }

  for (gint32 i = id; i < max_id; i++)
    {
      gchar *base;
      base = g_strdup_printf ("/actions/action-%d", i);
      xfconf_channel_reset_property (channel, base, TRUE);
      g_free (base);
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
  GError *error = NULL;

  if (!xfconf_init (&error))
    {
      g_critical ("Failed to initialized xfconf");
      g_error_free (error);
      return;
    }
  channel = xfconf_channel_get ("xfce4-screenshooter");

  max_id = xfconf_channel_get_int (channel, "/actions/actions", 0);
  for (id = 0; id < max_id; id++)
    {
      gchar *name = "";
      gchar *command = "";
      gchar *name_address;
      gchar *command_address;

      name_address = g_strdup_printf ("/actions/action-%d/name", id);
      command_address = g_strdup_printf ("/actions/action-%d/command", id);

      name = xfconf_channel_get_string (channel, name_address, "");
      command = xfconf_channel_get_string (channel, command_address, "");

      gtk_list_store_append (list_store, &iter);
      gtk_list_store_set (GTK_LIST_STORE (list_store), &iter, CUSTOM_ACTION_NAME, name, CUSTOM_ACTION_COMMAND, command, -1);

      g_free (name_address);
      g_free (command_address);
    }
  xfconf_shutdown ();
}



void screenshooter_custom_action_execute (gchar *save_location, gchar *name, gchar *command) {
  gchar **split;
  gchar **argv;
  gchar **envp;
  GError *error = NULL;

  if (g_strcmp0 (name, "") == 0 || g_strcmp0 (command, "") == 0)
    {
      name = "none";
      command = "none";
    }

  if (g_strcmp0 (name, "none") == 0 || g_strcmp0 (command, "none") == 0)
    {
      xfce_dialog_show_warning (NULL, "Unable to execute the custom action", "Invalid custom action selected");
      return;
    }

  split = g_strsplit (command, "\%f", -1);
  command = g_strjoinv (save_location, split);

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
