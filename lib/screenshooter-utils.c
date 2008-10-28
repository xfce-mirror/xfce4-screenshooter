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

#include <screenshooter-utils.h>

/* Prototypes */
static gchar *generate_filename_for_uri      (char             *uri);
static Window find_toplevel_window           (Window            xid);
static void cb_current_folder_changed        (GtkFileChooser   *chooser, 
                                              gpointer          user_data);


/* Borrowed from gnome-screenshot */
/* This function returns the toplevel window containing Window, for most window
managers this will enable you to get the decorations around Window. Does not
work with Compiz.
Window: the X identifier of the window
Returns: the X identifier of the toplevel window containing Window*/
static Window
find_toplevel_window (Window xid)
{
  Window root, parent, *children;
  unsigned int nchildren;

  do
    {
      if (XQueryTree (GDK_DISPLAY (), xid, &root,
		      &parent, &children, &nchildren) == 0)
	      {
	        g_warning ( _("Couldn't find window manager window") );
	        return None;
	      }

      if (root == parent)
	      return xid;

      xid = parent;
    }
  while (TRUE);
}



/* Generates filename Screenshot-n.png (where n is the first integer greater than 
0) so that Screenshot-n.jpg does not exist in the folder whose URI is *uri.
*uri: the uri of the folder for which the filename should be generated.
returns: a filename verifying the above conditions or NULL if *uri == NULL.
*/
static gchar *generate_filename_for_uri(char *uri)
{
  gchar *file_name;
  unsigned int i = 0;
    
  if ( uri == NULL )
    {
  	  return NULL;
    }      
  
  file_name = g_strdup (_("Screenshot.png"));
  
  /* If the plain filename matches the condition, go for it. */
  if (g_access (g_build_filename (uri, file_name, NULL), F_OK) != 0) 
    {
      return file_name;
    }
  
  /* Else, we find the first n that matches the condition */  
  do
    {
      i++;
      g_free (file_name);
      file_name = g_strdup_printf (_("Screenshot-%d.png"), i);
    }
  while (g_access (g_build_filename (uri, file_name, NULL), F_OK) == 0);
    
  return file_name;
}



/* Generate a correct file name when setting a folder in chooser
GtkFileChooser *chooser: the file chooser we are using.
gpointer user_data: not used here.
*/
static void
cb_current_folder_changed (GtkFileChooser *chooser, gpointer user_data)
{
  gchar *current_folder = 
    gtk_file_chooser_get_current_folder (GTK_FILE_CHOOSER (chooser));
  
  if (current_folder)
    {
      gchar *new_filename = generate_filename_for_uri (current_folder);
      
      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser), 
                                         new_filename);
      g_free (new_filename);
    }
  
  g_free (current_folder);
}



/* Public */



/* Takes the screenshot with the options given in sd.
*sd: a ScreenshotData struct.
returns: the screenshot in a *GdkPixbuf.
*/
GdkPixbuf *screenshooter_take_screenshot (gint       mode, 
                                          gint       delay)
{
  GdkPixbuf *screenshot;
  GdkWindow *window = NULL;
  GdkScreen *screen;
  
  gint width;
  gint height;
  /* gdk_get_default_root_window (), needs_unref enables us to unref *window 
  only if a non default window has been grabbed */
  gboolean needs_unref = TRUE;
  
  /* Get the screen on which the screenshot should be taken */
  screen = gdk_screen_get_default ();
  
  /* Get the window/desktop we want to screenshot*/  
  if (mode == FULLSCREEN)
    {
      window = gdk_get_default_root_window ();
      needs_unref = FALSE;
    } 
  else if (mode == ACTIVE_WINDOW)
    {
      window = gdk_screen_get_active_window (screen);
      
      /* If we are supposed to take a screenshot of the active window, and if 
      the active window is the desktop background, grab the whole screen.*/      
      if (window == NULL || 
          gdk_window_get_type_hint (window) == GDK_WINDOW_TYPE_HINT_DESKTOP)
        {
          if (!(window == NULL))
            {
              g_object_unref (window);
            }
          
          window = gdk_get_default_root_window ();
          needs_unref = FALSE;
        }
      else
        {
          GdkWindow *window2;
          
          window2 = 
            gdk_window_foreign_new (find_toplevel_window 
                                    (GDK_WINDOW_XID (window)));
          
          g_object_unref (window);
          
          window = window2;
        }
    }
  
  /* wait for n=delay seconds */ 
  sleep (delay);
  
  /* get the size of the part of the screen we want to screenshot */
  gdk_drawable_get_size(window, &width, &height);
  
  /* get the screenshot */
  screenshot = gdk_pixbuf_get_from_drawable (NULL,
					     window,
					     NULL, 0, 0, 0, 0,
					     width, height);
					     
	if (needs_unref)
	  g_object_unref (window);
		
	return screenshot;
}



/* Saves the screenshot according to the options in sd. 
*screenshot: a GdkPixbuf containing our screenshot
*sd: a ScreenshotData struct containing the save options.*/
void screenshooter_save_screenshot (GdkPixbuf      *screenshot, 
                                    gboolean        show_save_dialog,
                                    gchar          *default_dir)
{
  GdkPixbuf *thumbnail;
  gchar *filename = NULL;
  GtkWidget *preview;
  GtkWidget *chooser;
  gint dialog_response;

  filename = generate_filename_for_uri (default_dir);
    
  if (show_save_dialog)
	  {
	    /* If the user wants a save dialog, we run it, and grab the filename 
	    the user has chosen. */
	  
      chooser = 
        gtk_file_chooser_dialog_new (_("Save screenshot as ..."),
                                     NULL,
                                     GTK_FILE_CHOOSER_ACTION_SAVE,
                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                     GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                     NULL);
      gtk_window_set_icon_name (GTK_WINDOW (chooser), "applets-screenshooter");
      gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (chooser), 
                                                      TRUE);
      gtk_dialog_set_default_response (GTK_DIALOG (chooser), 
                                     GTK_RESPONSE_ACCEPT);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (chooser), 
                                          default_dir);

      preview = gtk_image_new ();
  
      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser), filename);
      gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (chooser), preview);
  
      thumbnail = 
        gdk_pixbuf_scale_simple (screenshot, gdk_pixbuf_get_width(screenshot)/5, 
                                 gdk_pixbuf_get_height(screenshot)/5, 
                                 GDK_INTERP_BILINEAR);
      gtk_image_set_from_pixbuf (GTK_IMAGE (preview), thumbnail);
      g_object_unref (thumbnail);
      
      /* We the user opens a folder in the fine_chooser, we set a valid
      filename */
      g_signal_connect (G_OBJECT (chooser), "current-folder-changed", 
                        G_CALLBACK(cb_current_folder_changed), NULL);
    
      dialog_response = gtk_dialog_run (GTK_DIALOG (chooser));
	  
	    if (dialog_response == GTK_RESPONSE_ACCEPT)
	      {
	        g_free (filename);
	        filename = 
	          gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser) );
          gdk_pixbuf_save (screenshot, filename, "png", NULL, NULL);
	      }
	  
	    gtk_widget_destroy ( GTK_WIDGET ( chooser ) );
	  }  
	else
	  {    
	    gchar *savename = NULL;
	    /* Else, we just save the file in the default folder */
      
      savename = g_build_filename (default_dir, filename, NULL);
	    gdk_pixbuf_save (screenshot, savename, "png", NULL, NULL);
	    
	    g_free (savename);
	  }

  g_free (filename);
}



void
screenshooter_read_rc_file (gchar               *file, 
                            ScreenshotData      *sd, 
                            gboolean             dir_only)
{
  XfceRc *rc;
  gint delay = 0;
  gint mode = FULLSCREEN;
  gint show_save_dialog = 1;
  gchar *screenshot_dir = g_strdup (DEFAULT_SAVE_DIRECTORY);

  if (g_file_test (file, G_FILE_TEST_EXISTS))
    {
      rc = xfce_rc_simple_open (file, TRUE);

      if (rc != NULL)
        {
          if (!dir_only)
            {
              delay = xfce_rc_read_int_entry (rc, "delay", 0);
              mode = xfce_rc_read_int_entry (rc, "mode", FULLSCREEN);
              show_save_dialog = 
                xfce_rc_read_int_entry (rc, "show_save_dialog", 1);
            }
  
          g_free (screenshot_dir);
          screenshot_dir = 
            g_strdup (xfce_rc_read_entry (rc, 
                                          "screenshot_dir", 
                                          DEFAULT_SAVE_DIRECTORY));
        }
      
      xfce_rc_close (rc);
    }
   
  /* And set the sd values */
  sd->delay = delay;
  sd->mode = mode;
  sd->show_save_dialog = show_save_dialog;
  sd->screenshot_dir = screenshot_dir;
}



void
screenshooter_write_rc_file (gchar               *file, 
                             ScreenshotData      *sd)
{
  XfceRc *rc;

  rc = xfce_rc_simple_open (file, FALSE);
  
  g_return_if_fail (rc != NULL);
  
  xfce_rc_write_int_entry (rc, "delay", sd->delay);
  xfce_rc_write_int_entry (rc, "mode", sd->mode);
  xfce_rc_write_int_entry (rc, "show_save_dialog", sd->show_save_dialog);
  xfce_rc_write_entry (rc, "screenshot_dir", sd->screenshot_dir);
  
  xfce_rc_close (rc);
}
