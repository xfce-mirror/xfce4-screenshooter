/*  $Id$
 *
 *  Copyright © 2008 Jérôme Guelfucci <jerome.guelfucci@gmail.com>
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
gboolean fullscreen = FALSE;
gboolean no_save_dialog = FALSE;
gchar *screenshot_dir;
gint delay = 0;



/* Set cli options. The -p option creates a conf file named xfce4-screenshooter 
   in ~/.config/xfce4/. This file only contains one entry, the name of the 
   default save folder. 
*/
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
        N_("Take a screenshot of the whole screen"),
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
    { NULL }
};



int main(int argc, char **argv)
{
  GError *cli_error = NULL;
  GdkPixbuf *screenshot;
  ScreenshotData *sd = g_new0 (ScreenshotData, 1);
  gchar *rc_file = 
    xfce_resource_lookup (XFCE_RESOURCE_CONFIG, 
                          "xfce4/xfce4-screenshooter");
  
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
  
  /* Read the preferences */
      
  screenshooter_read_rc_file (rc_file, sd);
  
  if (rc_file != NULL)
    g_free (rc_file);
    
  /* Print a message to advise to use help when a non existing cli option is
  passed to the executable. */  
  if (!gtk_init_with_args(&argc, &argv, _(""), entries, PACKAGE, &cli_error))
    {
      if (cli_error != NULL)
        {
          g_print (_("%s: %s\nTry %s --help to see a full list of available command line options.\n"), 
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
  
  /* If -w is given to the executable, grab the active window, else just 
   * grab the desktop.*/
  if (window)
    {
      sd->mode = ACTIVE_WINDOW;    
    }
  else if (fullscreen)
    {
      sd->mode = FULLSCREEN;
    }
  else
    {
      sd->mode = FULLSCREEN;
    }
  
  /* Wether to show the save dialog allowing to choose a filename and a save 
  location */
  if (no_save_dialog)
    {
      sd->show_save_dialog = 0;
    }
  else
    {
      sd->show_save_dialog = 1;
    }

  sd->delay = delay;
  
  /* If the user gave a directory name, verify that it is valid */
  if (screenshot_dir != NULL)  
    {
      if (g_file_test (screenshot_dir, G_FILE_TEST_IS_DIR))
        {
          /* Check if the path is absolute, if not make it absolute */
          if (g_path_is_absolute (screenshot_dir))
            { 
              g_free (sd->screenshot_dir);
              sd->screenshot_dir = screenshot_dir;              
            }
          else
            {
              g_free (sd->screenshot_dir);
              sd->screenshot_dir = 
              g_build_filename (g_get_current_dir (), screenshot_dir, NULL);
              g_free (screenshot_dir);
            }
        }
      else
        {
          g_warning ("%s is not a valid directory, the default directory will be used.", 
                     screenshot_dir);
          g_free (screenshot_dir);
        }
    }
  
  /* If a mode cli option is given, take the screenshot accordingly. */
  if (fullscreen || window)
    {
      screenshot = screenshooter_take_screenshot (sd->mode, sd->delay);
      screenshooter_save_screenshot (screenshot, sd->show_save_dialog, 
                                     sd->screenshot_dir);
    
      g_object_unref (screenshot);
    }
  /* Else we just show up the main application */
  else
    {
      GtkWidget *dialog;
      gint response = GTK_RESPONSE_OK;
      GdkDisplay *display = gdk_display_get_default ();
      
      rc_file = xfce_resource_lookup (XFCE_RESOURCE_CONFIG, 
                                      "xfce4/xfce4-screenshooter");
      
      /* Read the preferences */
      screenshooter_read_rc_file (rc_file, sd);
      
      if (rc_file != NULL)
        g_free (rc_file);
      
      /* Show the dialog and take the screenshot until the application
       * is exited */
      
      while (response == GTK_RESPONSE_OK)
        {
          /* Set the dialog up */
          dialog = screenshooter_dialog_new (sd, FALSE);
          
          gtk_window_set_type_hint(GTK_WINDOW (dialog), 
                                   GDK_WINDOW_TYPE_HINT_NORMAL);
          
          /* Run the dialog and destroy it, so that it's not grabbed in 
           * active window mode */
          
          response = gtk_dialog_run (GTK_DIALOG (dialog));
          
          gtk_widget_destroy (dialog);
          
          gdk_display_sync (display);
          
          /* Make sure the window manager had time to set the new active
           * window.*/
          sleep (1);
                      
          if (response == GTK_RESPONSE_OK)
            {
              screenshooter_take_and_output_screenshot (sd);
            }
        }
      
      /* Save preferences */
      rc_file = 
        xfce_resource_save_location (XFCE_RESOURCE_CONFIG, 
                                     "xfce4/xfce4-screenshooter",
                                     TRUE);
      if (rc_file != NULL)
        {
          screenshooter_write_rc_file (rc_file, sd);
          
          g_free (rc_file);
        }
    }
  
  g_free (sd->screenshot_dir);
  #ifdef HAVE_GIO
  g_free (sd->app);
  #endif
  g_free (sd);
    
  return 0;
}
