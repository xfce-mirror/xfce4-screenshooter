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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "libscreenshooter.h"
#include <glib.h>
#include <stdlib.h>



/* Set default values for cli args */
gboolean version = FALSE;
gboolean window = FALSE;
gboolean region = FALSE;
gboolean fullscreen = FALSE;
gboolean mouse = FALSE;
gboolean clipboard = FALSE;
gboolean upload_imgur = FALSE;
gchar *screenshot_dir = NULL;
gchar *application = NULL;
gint delay = 0;



/* Set cli options. */
static GOptionEntry entries[] =
{
  {
    "clipboard", 'c', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &clipboard,
    N_("Copy the screenshot to the clipboard"),
    NULL
  },
  {
    "delay", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &delay,
    N_("Delay in seconds before taking the screenshot"),
    NULL
  },
  {
    "fullscreen", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &fullscreen,
    N_("Take a screenshot of the entire screen"),
    NULL
  },
  {
    "mouse", 'm', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &mouse,
    N_("Display the mouse on the screenshot"),
    NULL
  },
  {
    "open", 'o', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &application,
    N_("Application to open the screenshot"),
    NULL
  },
  {
    "region", 'r', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &region,
    N_("Select a region to be captured by clicking a point of the screen "
       "without releasing the mouse button, dragging your mouse to the "
       "other corner of the region, and releasing the mouse button."),
    NULL
  },
  {
    "save", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_FILENAME, &screenshot_dir,
    N_("Directory where the screenshot will be saved"),
    NULL
  },
  {
    "imgur", 'i', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &upload_imgur,
    N_("Host the screenshot on Imgur, a free online image hosting service"),
    NULL
  },
  {
    "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
    N_("Version information"),
    NULL
  },
  {
    "window", 'w', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &window,
    N_("Take a screenshot of the active window"),
    NULL
  },
  {
    NULL, ' ', 0, 0, NULL,
    NULL,
    NULL
  }
};



static void
take_screenshot (ScreenshotData *sd, gboolean from_cli)
{
  if (sd->region == SELECT)
    {
      /* The delay will be applied after the rectangle selection */
      g_idle_add ((GSourceFunc) screenshooter_take_screenshot_idle, sd);
      return;
    }

  if (sd->delay == 0 && from_cli)
    {
      /* If delay is zero and the region was passed as an argument, thus the
       * first dialog was not shown, we will take the screenshot immediately
       * without a minimal delay */
      g_idle_add ((GSourceFunc) screenshooter_take_screenshot_idle, sd);
      return;
    }

  /* Await the amount of the time specified by the user before capturing the
   * screenshot, but not less than 200ms, otherwise the first dialog might
   * appear on the screenshot. */
  gint delay = sd->delay == 0 ? 200 : sd->delay * 1000;
  g_timeout_add (delay, (GSourceFunc) screenshooter_take_screenshot_idle, sd);
}



static void
cb_dialog_response (GtkWidget *dialog, gint response, ScreenshotData *sd)
{
  if (response == GTK_RESPONSE_HELP)
    {
      g_signal_stop_emission_by_name (dialog, "response");
      screenshooter_open_help (GTK_WINDOW (dialog));
    }
  else if (response == GTK_RESPONSE_OK)
    {
      gtk_widget_destroy (dialog);
      take_screenshot (sd, FALSE);
    }
  else
    {
      gtk_widget_destroy (dialog);
      gtk_main_quit ();
    }
}



/* Main */



int main (int argc, char **argv)
{
  GError *cli_error = NULL;
  GFile *default_save_dir;
  const gchar *rc_file;
  const gchar *conflict_error =
    _("Conflicting options: --%s and --%s cannot be used at the same time.\n");
  const gchar *ignore_error =
    _("The --%s option is only used when --fullscreen, --window or"
      " --region is given. It will be ignored.\n");

  ScreenshotData *sd = g_new0 (ScreenshotData, 1);
  sd->plugin = FALSE;
  sd->app_info = NULL;
  sd->action = 0;

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* Print a message to advise to use help when a non existing cli option is
  passed to the executable. */
  if (!gtk_init_with_args(&argc, &argv, "", entries, PACKAGE, &cli_error))
    {
      if (cli_error != NULL)
        {
          g_print (_("%s: %s\nTry %s --help to see a full list of"
                     " available command line options.\n"),
                   PACKAGE, cli_error->message, PACKAGE_NAME);

          g_error_free (cli_error);
          g_free (sd);

          return EXIT_FAILURE;
        }
    }

  /* Exit if two region options were given */
  if (window && fullscreen)
    {
      g_printerr (conflict_error, "window", "fullscreen");

      g_free (sd);
      return EXIT_FAILURE;
    }
  else if (window && region)
    {
      g_printerr (conflict_error, "window", "region");

      g_free (sd);
      return EXIT_FAILURE;
    }
  else if (fullscreen && region)
    {
      g_printerr (conflict_error, "fullscreen", "region");

      g_free (sd);
      return EXIT_FAILURE;
    }

  /* Exit if two actions options were given */
  if ((application != NULL) && (screenshot_dir != NULL))
    {
      g_printerr (conflict_error, "open", "save");

      g_free (sd);
      return EXIT_FAILURE;
    }
  else if (upload_imgur && (screenshot_dir != NULL))
    {
      g_printerr (conflict_error, "imgur", "save");
      return EXIT_FAILURE;
    }
  else if (upload_imgur && (application != NULL))
    {
      g_printerr (conflict_error, "imgur", "open");
      return EXIT_FAILURE;
    }

  /* Warn that action options, mouse and delay will be ignored in
   * non-cli mode */
  if ((application != NULL) && !(fullscreen || window || region))
    g_printerr (ignore_error, "open");
  if ((screenshot_dir != NULL)  && !(fullscreen || window || region ))
    g_printerr (ignore_error, "save");
  if (upload_imgur && !(fullscreen || window || region))
    g_printerr (ignore_error, "imgur");
  if (clipboard && !(fullscreen || window || region))
    g_printerr (ignore_error, "clipboard");
  if (delay && !(fullscreen || window || region))
    g_printerr (ignore_error, "delay");
  if (mouse && !(fullscreen || window || region))
    g_printerr (ignore_error, "mouse");

  /* Just print the version if we are in version mode */
  if (version)
    {
      g_print ("%s\n", PACKAGE_STRING);

      g_free (sd);

      return EXIT_SUCCESS;
    }

  /* Read the preferences */
  rc_file = xfce_resource_save_location (XFCE_RESOURCE_CONFIG, "xfce4/xfce4-screenshooter", TRUE);
  screenshooter_read_rc_file (rc_file, sd);

  /* Default to no action specified */
  sd->action_specified = FALSE;

  /* Check if the directory read from the preferences is valid */
  default_save_dir = g_file_new_for_uri (sd->screenshot_dir);

  if (G_UNLIKELY (!g_file_query_exists (default_save_dir, NULL)))
    {
      g_free (sd->screenshot_dir);
      sd->screenshot_dir = screenshooter_get_xdg_image_dir_uri ();
    }

  g_object_unref (default_save_dir);

  /* If a region cli option is given, take the screenshot accordingly.*/
  if (fullscreen || window || region)
    {
      /* Set the region to be captured */
      if (window)
        sd->region = ACTIVE_WINDOW;
      else if (fullscreen)
        sd->region = FULLSCREEN;
      else if (region)
        sd->region = SELECT;

      /* Whether to display the mouse pointer on the screenshot */
      mouse ? (sd->show_mouse = 1) : (sd->show_mouse = 0);

      sd->delay = delay;

      if (application != NULL)
        {
          sd->app = application;
          sd->action = OPEN;
          sd->action_specified = TRUE;
        }
      else if (upload_imgur)
        {
          sd->action = UPLOAD_IMGUR;
          sd->action_specified = TRUE;
        }
      else if (screenshot_dir != NULL)
        {
          sd->action = SAVE;
          sd->action_specified = TRUE;
        }

      if (clipboard)
        {
          /* if no other action was specified, reset the value loaded from prefs */
          if (!sd->action_specified)
            sd->action = NONE;

          sd->action |= CLIPBOARD;
          sd->action_specified = TRUE;
        }

      if (!sd->app)
        sd->app = g_strdup ("none");

      /* If the user gave a directory name, check that it is valid */
      if (screenshot_dir != NULL)
        {
          default_save_dir = g_file_new_for_commandline_arg (screenshot_dir);

          if (G_LIKELY (g_file_query_exists (default_save_dir, NULL)))
            {
              g_free (sd->screenshot_dir);
              sd->screenshot_dir = g_file_get_uri (default_save_dir);
              sd->action_specified = TRUE;
            }
          else
              screenshooter_error (_("%s is not a valid directory, the default"
                                   " directory will be used."),
                                   screenshot_dir);

          g_object_unref (default_save_dir);
          g_free (screenshot_dir);
        }

      take_screenshot (sd, TRUE);
    }
  /* Else we show a dialog which allows to set the screenshot options */
  else
    {
      GtkWidget *dialog;

      /* Set the dialog up */
      dialog = screenshooter_region_dialog_new (sd, FALSE);
      g_signal_connect (dialog, "response",
                        G_CALLBACK (cb_dialog_response), sd);
      g_signal_connect (dialog, "key-press-event",
                        G_CALLBACK (screenshooter_f1_key), NULL);
      gtk_widget_show (dialog);
    }

  gtk_main ();

  /* Save preferences */
  screenshooter_write_rc_file (rc_file, sd);

  g_free (sd->screenshot_dir);
  g_free (sd->title);
  g_free (sd->app);
  g_free (sd->last_user);
  g_free (sd);

  TRACE ("Ciao");

  return EXIT_SUCCESS;
}
