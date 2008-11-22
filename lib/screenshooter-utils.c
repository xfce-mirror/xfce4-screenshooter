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

#include "screenshooter-utils.h"

/* Prototypes */



static 
Window find_toplevel_window           (Window            xid);

static GdkWindow 
*get_active_window                    (GdkScreen        *screen, 
                                       gboolean         *needs_unref);

static GdkPixbuf 
*get_window_screenshot                (GdkWindow        *window);



/* Borrowed from gnome-screenshot */

/* This function returns the toplevel window containing Window, for most 
 * window managers this will enable you to get the decorations around 
 * the window. Does not work with Compiz.
 * Window: the X identifier of the window
 * Returns: the X identifier of the toplevel window containing Window.*/
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



static GdkWindow 
*get_active_window (GdkScreen *screen, gboolean *needs_unref)
{
  GdkWindow *window, *window2;
  
  window = gdk_screen_get_active_window (screen);
            
  /* If there is no active window, we fallback to the whole screen. */      
  if (window == NULL)
    {
      window = gdk_get_default_root_window ();
      *needs_unref = FALSE;
    }
  
  /* If the active window is the desktop, we grab the whole screen, else
   * we find the toplevel window to grab the decorations. */
  if (gdk_window_get_type_hint (window) == GDK_WINDOW_TYPE_HINT_DESKTOP)
    {
      g_object_unref (window);
                    
      window = gdk_get_default_root_window ();
      *needs_unref = FALSE;
    }
  else
    {
      window2 = gdk_window_foreign_new (find_toplevel_window 
                                        (GDK_WINDOW_XID (window)));
      g_object_unref (window);
          
      window = window2;
    }

  return window;
}



static GdkPixbuf
*get_window_screenshot (GdkWindow *window)
{
  gint x_real_orig, y_real_orig, x_orig, y_orig;
  gint width, real_width, height, real_height;
  GdkPixbuf *screenshot;
  GdkWindow *root;
    
  /* Get the root window */
  root = gdk_get_default_root_window ();
  
  /* Based on gnome-screenshot code */
    
  /* Get the size and the origin of the part of the screen we want to 
   * screenshot. */
  gdk_drawable_get_size (window, &real_width, &real_height);      
  gdk_window_get_origin (window, &x_real_orig, &y_real_orig);
  
  /* Don't grab thing offscreen. */
    
  x_orig = x_real_orig;
  y_orig = y_real_orig;
  width  = real_width;
  height = real_height;

  if (x_orig < 0)
    {
      width = width + x_orig;
      x_orig = 0;
    }

  if (y_orig < 0)
    {
      height = height + y_orig;
      y_orig = 0;
    }

  if (x_orig + width > gdk_screen_width ())
    width = gdk_screen_width () - x_orig;

  if (y_orig + height > gdk_screen_height ())
    height = gdk_screen_height () - y_orig;
  
  /* Take the screenshot from the root GdkWindow, to grab things such as
   * menus. */
  screenshot = gdk_pixbuf_get_from_drawable (NULL, root, NULL,
                                             x_orig, y_orig, 0, 0,
                                             width, height);
  
  return screenshot;                                             
}



/* Public */



/* Takes the screenshot with the options given in sd.
*sd: a ScreenshotData struct.
returns: the screenshot in a *GdkPixbuf.
*/
GdkPixbuf *screenshooter_take_screenshot (gint mode, gint delay)
{
  GdkPixbuf *screenshot;
  GdkWindow *window = NULL;
  GdkScreen *screen;
      
  /* gdk_get_default_root_window () does not need to be unrefed, 
   * needs_unref enables us to unref *window only if a non default 
   * window has been grabbed. */
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
      window = get_active_window (screen, &needs_unref);      
    }
  
  /* wait for n=delay seconds */ 
  sleep (delay);
  
  if (mode == FULLSCREEN || mode == ACTIVE_WINDOW)
    {
      screenshot = get_window_screenshot (window);
    }
					     
	if (needs_unref)
	  g_object_unref (window);
		
	return screenshot;
}



/* Copy the screenshot to the Clipboard.
* Code is from gnome-screenshooter.
* @screenshot: the screenshot
*/
void
screenshooter_copy_to_clipboard (GdkPixbuf *screenshot) 
{
  GtkClipboard *clipboard;

  clipboard = gtk_clipboard_get_for_display (gdk_display_get_default(), 
                                             GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_image (clipboard, screenshot);

  gtk_clipboard_store (clipboard);
}



/* Read the options from file and sets the sd values.
@file: the path to the rc file.
@sd: the ScreenshotData to be set.
@dir_only: if true, only read the screenshot_dir.
*/
void
screenshooter_read_rc_file (gchar               *file, 
                            ScreenshotData      *sd, 
                            gboolean             dir_only)
{
  XfceRc *rc;
  gint delay = 0;
  gint mode = FULLSCREEN;
  gint action = SAVE;
  gint show_save_dialog = 1;
  gchar *screenshot_dir = g_strdup (DEFAULT_SAVE_DIRECTORY);
  #ifdef HAVE_GIO
  gchar *app = NULL;
  #endif
  
  if (g_file_test (file, G_FILE_TEST_EXISTS))
    {
      rc = xfce_rc_simple_open (file, TRUE);

      if (rc != NULL)
        {
          if (!dir_only)
            {
              delay = xfce_rc_read_int_entry (rc, "delay", 0);
              
              mode = xfce_rc_read_int_entry (rc, "mode", FULLSCREEN);
              
              action = xfce_rc_read_int_entry (rc, "action", SAVE);
              
              show_save_dialog = 
                xfce_rc_read_int_entry (rc, "show_save_dialog", 1);
              
              #ifdef HAVE_GIO
              g_free (app);
              
              app = 
                g_strdup (xfce_rc_read_entry (rc, "app", NULL));
              #endif
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
  sd->action = action;
  sd->show_save_dialog = show_save_dialog;
  sd->screenshot_dir = screenshot_dir;
  #ifdef HAVE_GIO
  sd->app = app;
  #endif
}



/* Writes the options from sd to file.
@file: the path to the rc file.
@sd: a ScreenshotData.
*/
void
screenshooter_write_rc_file (gchar               *file, 
                             ScreenshotData      *sd)
{
  XfceRc *rc;

  rc = xfce_rc_simple_open (file, FALSE);
  
  g_return_if_fail (rc != NULL);
  
  xfce_rc_write_int_entry (rc, "delay", sd->delay);
  xfce_rc_write_int_entry (rc, "mode", sd->mode);
  xfce_rc_write_int_entry (rc, "action", sd->action);
  xfce_rc_write_int_entry (rc, "show_save_dialog", 
                           sd->show_save_dialog);
  xfce_rc_write_entry (rc, "screenshot_dir", sd->screenshot_dir);
  #ifdef HAVE_GIO
  xfce_rc_write_entry (rc, "app", sd->app);
  #endif
  
  xfce_rc_close (rc);
}



/* Opens the screenshot using application.
@screenshot_path: the path to the saved screenshot.
@application: the command to run the application.
*/
#ifdef HAVE_GIO
void
screenshooter_open_screenshot (gchar *screenshot_path,
                               gchar *application)
{
  if (screenshot_path != NULL)
    {
      gchar *command = 
        g_strconcat (application, " ", screenshot_path, NULL);
    
      GError      *error = NULL;
          
      /* Execute the command and show an error dialog if there was 
      * an error. */
      if (!xfce_exec_on_screen (gdk_screen_get_default (), command, 
                                FALSE, TRUE, &error))
        {
          xfce_err (error->message);
          g_error_free (error);
        }
          
      g_free (command);
    }
}     
#endif
