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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "libscreenshooter.h"



/* Set default values for cli args */
gboolean version = FALSE;
gboolean window = FALSE;
gboolean region = FALSE;
gboolean fullscreen = FALSE;
gboolean no_save_dialog = FALSE;
gboolean hide_mouse = FALSE;
gboolean upload = FALSE;
gchar *screenshot_dir;
gchar *application;
gint delay = 0;



/* Set cli options. */
static GOptionEntry entries[] =
{
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
    "hide", 'h', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &no_save_dialog,
    N_("Do not display the save dialog"),
    NULL
  },
  {
    "mouse", 'm', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &hide_mouse,
    N_("Do not display the mouse on the screenshot"),
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
    "upload", 'u', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &upload,
    N_("Upload the screenshot to ZimageZ©, a free Web hosting solution"),
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
cb_dialog_response (GtkWidget *dlg, int response, ScreenshotData *sd);



/* Internals */



static void
cb_dialog_response (GtkWidget *dialog, int response, ScreenshotData *sd)
{
  if (response == GTK_RESPONSE_HELP)
    {
      GError *error_help = NULL;

      /* Launch the help page and show an error dialog if there was an error. */
      if (!g_spawn_command_line_async ("xfhelp4 xfce4-screenshooter.html", &error_help))
        {
          xfce_err (error_help->message);
          g_error_free (error_help);
        }
    }
  else if (response == GTK_RESPONSE_OK)
    {
      gchar *rc_file;

      GdkDisplay *display = gdk_display_get_default ();

      gtk_widget_hide (dialog);

      gdk_display_sync (display);

      /* Make sure the window manager had time to set the new active
      * window.*/
      if (sd->region != SELECT)
        sleep (1);

      screenshooter_take_and_output_screenshot (sd);

      if (sd->close == 1)
        {
          gtk_widget_destroy (dialog);

          rc_file = xfce_resource_save_location (XFCE_RESOURCE_CONFIG,
                                                 "xfce4/xfce4-screenshooter",
                                                 TRUE);

          /* Save preferences */

          if (rc_file != NULL)
            {
              screenshooter_write_rc_file (rc_file, sd);

              g_free (rc_file);
            }

          gtk_main_quit ();
        }
      else
        {
          gtk_widget_show (dialog);
        }
    }
  else
    {
      gchar *rc_file;

      gtk_widget_destroy (dialog);

      rc_file = xfce_resource_save_location (XFCE_RESOURCE_CONFIG,
                                             "xfce4/xfce4-screenshooter",
                                             TRUE);

      /* Save preferences */

      if (rc_file != NULL)
        {
          screenshooter_write_rc_file (rc_file, sd);

          g_free (rc_file);
        }

      gtk_main_quit ();
    }
}



/* Main */



int main(int argc, char **argv)
{
  GError *cli_error = NULL;
  GFile *default_save_dir;

  ScreenshotData *sd = g_new0 (ScreenshotData, 1);

  gchar *rc_file;

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
		  
          return 1;
        }
    }

  g_thread_init (NULL);

  /* Read the preferences */

  rc_file = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, "xfce4/xfce4-screenshooter");

  screenshooter_read_rc_file (rc_file, sd);

  if (G_LIKELY (rc_file != NULL))
    g_free (rc_file);

  /* Check if the directory read from the preferences is valid */

  default_save_dir = g_file_new_for_uri (sd->screenshot_dir);
  
  if (G_UNLIKELY (!g_file_query_exists (default_save_dir, NULL)))
    {
      g_free (sd->screenshot_dir);

      sd->screenshot_dir = screenshooter_get_home_uri ();
    }

  g_object_unref (default_save_dir);

  /* Just print the version if we are in version mode */
  if (version)
    {
      g_print ("%s\n", PACKAGE_STRING);
	  
      return 0;
    }

  /* If a region cli option is given, take the screenshot accordingly.*/
  if (fullscreen || window || region)
    {
      /* Set the region to be captured */
      if (window)
        {
          sd->region = ACTIVE_WINDOW;
        }
      else if (fullscreen)
        {
          sd->region = FULLSCREEN;
        }
      else if (region)
        {
          sd->region = SELECT;
        }
	  
      /* Wether to show the save dialog allowing to choose a filename
       * and a save location */
      no_save_dialog ? (sd->show_save_dialog = 0) : (sd->show_save_dialog = 1);

      /* Whether to display the mouse pointer on the screenshot */
      hide_mouse ? (sd->show_mouse = 0) : (sd->show_mouse = 1);

      sd->delay = delay;

      if (application != NULL)
        {
          sd->app = application;
          sd->action = OPEN;
        }
      else if (upload)
        {
          sd->app = g_strdup ("none");
          sd->action = UPLOAD;
        }
      else
        {
          sd->app = g_strdup ("none");
          sd->action = SAVE;
        }

      /* If the user gave a directory name, verify that it is valid */
      if (screenshot_dir != NULL)
        {
          default_save_dir = g_file_new_for_commandline_arg (screenshot_dir);

          if (G_LIKELY (g_file_query_exists (default_save_dir, NULL)))
            {
              g_free (sd->screenshot_dir);
              sd->screenshot_dir = g_file_get_uri (default_save_dir);
            }
          else
            {
              xfce_err (_("%s is not a valid directory, the default"
                          " directory will be used."), screenshot_dir);
            }

          g_object_unref (default_save_dir);
          g_free (screenshot_dir);
        }

      screenshooter_take_and_output_screenshot (sd);
    }
  /* Else we just show up the main application */
  else
    {
      GtkWidget *dialog;

      /* Set the dialog up */

      dialog = screenshooter_dialog_new (sd, FALSE);

      gtk_window_set_type_hint(GTK_WINDOW (dialog), GDK_WINDOW_TYPE_HINT_NORMAL);

      g_signal_connect (dialog, "response", G_CALLBACK (cb_dialog_response), sd);

      gtk_widget_show (dialog);

      gtk_main ();
    }

  g_free (sd->screenshot_dir);
  g_free (sd->app);
  g_free (sd);

  return 0;
}
