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
gchar *screenshot_dir;
#ifdef HAVE_GIO
gchar *application;
#endif
gint delay = 0;



/* Set cli options. */
static GOptionEntry entries[] =
{
    {    "version", 'V', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
        N_("Version information"),
        NULL
    },
    {   "window", 'w', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &window,
        N_("Take a screenshot of the active window"),
        NULL
    },
    {   "fullscreen", 'f', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &fullscreen,
        N_("Take a screenshot of the entire screen"),
        NULL
    },
    {   "region", 'r', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &region,
        N_("Select a region to be captured by clicking a point of the screen "
           "without releasing the mouse button, dragging your mouse to the "
           "other corner of the region, and releasing the mouse button."),
        NULL
    },
    {		"delay", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &delay,
       N_("Delay in seconds before taking the screenshot"),
       NULL
    },
    {   "hide", 'h', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &no_save_dialog,
        N_("Do not display the save dialog"),
        NULL
    },
    {   "save", 's', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_FILENAME, &screenshot_dir,
        N_("Directory where the screenshot will be saved"),
        NULL
    },
    #ifdef HAVE_GIO
    {   "open", 'o', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_STRING, &application,
        N_("Application to open the screenshot"),
        NULL
    },
    #endif
    { NULL, ' ', 0, 0, NULL, NULL, NULL }
};

static void
cb_dialog_response (GtkWidget *dlg, int response, ScreenshotData *sd);



/* Internals */



static void
cb_dialog_response (GtkWidget *dialog, int response,
                    ScreenshotData *sd)
{
  if (response == GTK_RESPONSE_HELP)
    {
      GError *error_help = NULL;

      /* Launch the help page and show an error dialog if there was
       * an error. */
      if (!xfce_exec_on_screen (gdk_screen_get_default (),
                                "xfhelp4 xfce4-screenshooter.html",
                                FALSE, TRUE, &error_help))
        {
          xfce_err (error_help->message);
          g_error_free (error_help);
        }
    }
  else
    {
      gchar *rc_file;

      /* If the response was ok, we take the screenshot */
      if (response == GTK_RESPONSE_OK)
        {
          GdkDisplay *display = gdk_display_get_default ();

          gtk_widget_hide (dialog);

          gdk_display_sync (display);

          /* Make sure the window manager had time to set the new active
          * window.*/
          if (sd->region != SELECT)
            sleep (1);

          screenshooter_take_and_output_screenshot (sd);

        }

      gtk_widget_destroy (dialog);

      rc_file =
        xfce_resource_save_location (XFCE_RESOURCE_CONFIG,
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

  ScreenshotData *sd = g_new0 (ScreenshotData, 1);

  gchar *rc_file =
    xfce_resource_lookup (XFCE_RESOURCE_CONFIG,
                          "xfce4/xfce4-screenshooter");

  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  /* Read the preferences */

  screenshooter_read_rc_file (rc_file, sd);

  if (rc_file != NULL)
    g_free (rc_file);

  /* Check if the directory read from the preferences is valid */
  if (!g_file_test (sd->screenshot_dir, G_FILE_TEST_IS_DIR))
    {
      sd->screenshot_dir = g_strdup (DEFAULT_SAVE_DIRECTORY);
    }

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
      if (no_save_dialog)
        {
          sd->show_save_dialog = 0;
        }
      else
        {
          sd->show_save_dialog = 1;
        }

      sd->delay = delay;

      #ifdef HAVE_GIO
      if (application != NULL)
        {
          sd->app = application;
          sd->action = OPEN;
        }
      else
        {
          sd->app = g_strdup ("none");
          sd->action = SAVE;
        }
      #endif

      /* If the user gave a directory name, verify that it is valid */
      if (screenshot_dir != NULL)
        {
          if (g_file_test (screenshot_dir, G_FILE_TEST_IS_DIR))
            {
              /* Check if the path is absolute, if not make it
               * absolute */
              if (g_path_is_absolute (screenshot_dir))
                {
                  g_free (sd->screenshot_dir);

                  sd->screenshot_dir = screenshot_dir;
                }
              else
                {
                  gchar *current_dir = g_get_current_dir ();

                  g_free (sd->screenshot_dir);

                  sd->screenshot_dir =
                    g_build_filename (current_dir,
                                      screenshot_dir,
                                      NULL);

                  g_free (current_dir);
                  g_free (screenshot_dir);
                }
            }
          else
            {
              g_warning (_("%s is not a valid directory, the default"
                           " directory will be used."),
                         screenshot_dir);

              g_free (screenshot_dir);
            }
        }

      screenshooter_take_and_output_screenshot (sd);
    }
  /* Else we just show up the main application */
  else
    {
      GtkWidget *dialog;

      /* Set the dialog up */

      dialog = screenshooter_dialog_new (sd, FALSE);

      gtk_window_set_type_hint(GTK_WINDOW (dialog),
                               GDK_WINDOW_TYPE_HINT_NORMAL);

      g_signal_connect (dialog,
                        "response",
                        G_CALLBACK (cb_dialog_response),
                        sd);

      gtk_widget_show (dialog);

      gtk_main ();
    }

  g_free (sd->screenshot_dir);
  #ifdef HAVE_GIO
  g_free (sd->app);
  #endif
  g_free (sd);

  return 0;
}
