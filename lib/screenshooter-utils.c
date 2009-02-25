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

#include "screenshooter-utils.h"

/* Prototypes */



static GdkWindow 
*get_active_window                    (GdkScreen        *screen, 
                                       gboolean         *needs_unref);

static GdkPixbuf 
*get_window_screenshot                (GdkWindow        *window);

static GdkPixbuf
*get_rectangle_screenshot             (void);



static GdkWindow 
*get_active_window (GdkScreen *screen, gboolean *needs_unref)
{
  GdkWindow *window, *window2;

  TRACE ("Get the active window");
  
  window = gdk_screen_get_active_window (screen);
            
  /* If there is no active window, we fallback to the whole screen. */      
  if (window == NULL)
    {
      TRACE ("No active window, fallback to the root window");

      window = gdk_get_default_root_window ();
      *needs_unref = FALSE;
    }
  else if (gdk_window_get_type_hint (window) == GDK_WINDOW_TYPE_HINT_DESKTOP)
    {
      /* If the active window is the desktop, grab the whole screen */
      TRACE ("The active window is the desktop, fallback to the root window");

      g_object_unref (window);
                    
      window = gdk_get_default_root_window ();
      *needs_unref = FALSE;
    }
  else
    {
      /* Else we find the toplevel window to grab the decorations. */
      TRACE ("Active window is a normal window, grab the toplevel window");

      window2 = gdk_window_get_toplevel (window);
      
      g_object_unref (window);
          
      window = window2;
    }

  return window;
}



static GdkPixbuf
*get_window_screenshot (GdkWindow *window)
{
  gint x_orig, y_orig;
  gint width, height;
  
  GdkPixbuf *screenshot;
  GdkWindow *root;
  
  GdkRectangle *rectangle = g_new0 (GdkRectangle, 1);
  
  GdkCursor *cursor;
  GdkPixbuf *cursor_pixbuf;
    
  /* Get the root window */
  TRACE ("Get the root window");
  
  root = gdk_get_default_root_window ();

  TRACE ("Get the frame extents");
  
  gdk_window_get_frame_extents (window, rectangle);
    
  /* Don't grab thing offscreen. */

  TRACE ("Make sure we don't grab things offscreen");
  
  x_orig = rectangle->x;
  y_orig = rectangle->y;
  width  = rectangle->width;
  height = rectangle->height;

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
    
  g_free (rectangle);
  
  /* Take the screenshot from the root GdkWindow, to grab things such as
   * menus. */

  TRACE ("Grab the screenshot");
  
  screenshot = gdk_pixbuf_get_from_drawable (NULL, root, NULL,
                                             x_orig, y_orig, 0, 0,
                                             width, height);

  /* Add the mouse pointer to the grabbed screenshot */

  TRACE ("Get the mouse cursor and its image");

  cursor = gdk_cursor_new_for_display (gdk_display_get_default (), GDK_LEFT_PTR);
  cursor_pixbuf = gdk_cursor_get_image (cursor);

  if (cursor_pixbuf != NULL)
    {
      GdkRectangle rectangle_window, rectangle_cursor;
      gint cursorx, cursory, xhot, yhot;

      TRACE ("Get the coordinates of the cursor");
 	
      gdk_window_get_pointer (window, &cursorx, &cursory, NULL);

      TRACE ("Get the hot-spot x and y values");
      
      sscanf (gdk_pixbuf_get_option (cursor_pixbuf, "x_hot"), "%d", &xhot);
      sscanf (gdk_pixbuf_get_option (cursor_pixbuf, "y_hot"), "%d", &yhot);

      /* rectangle_window stores the window coordinates */
      rectangle_window.x = x_orig;
      rectangle_window.y = y_orig;
      rectangle_window.width = width;
      rectangle_window.height = height;
      
      /* rectangle_cursor stores the cursor coordinates */
      rectangle_cursor.x = cursorx + x_orig;
      rectangle_cursor.y = cursory + y_orig;
      rectangle_cursor.width = gdk_pixbuf_get_width (cursor_pixbuf);
      rectangle_cursor.height = gdk_pixbuf_get_height (cursor_pixbuf);
      
      /* see if the pointer is inside the window */
      if (gdk_rectangle_intersect (&rectangle_window, &rectangle_cursor, &rectangle_cursor))
        {
          TRACE ("Compose the two pixbufs");

          gdk_pixbuf_composite (cursor_pixbuf, screenshot,
                                cursorx - xhot, cursory - yhot,
                                rectangle_cursor.width, rectangle_cursor.height,
                                cursorx - xhot, cursory - yhot,
                                1.0, 1.0,
                                GDK_INTERP_BILINEAR,
                                255);
        }
        
      g_object_unref (cursor_pixbuf);
    }

  gdk_cursor_unref (cursor);

  return screenshot;                                             
}



static GdkPixbuf
*get_rectangle_screenshot (void)
{
  GdkPixbuf *screenshot = NULL;
 
  /* Get root window */
  TRACE ("Get the root window");
  
  GdkWindow *root_window =  gdk_get_default_root_window ();
  
  GdkGCValues gc_values;
  GdkGC *gc;
  GdkGrabStatus grabstatus;
  
  GdkGCValuesMask values_mask =
    GDK_GC_FUNCTION | GDK_GC_FILL	| GDK_GC_CLIP_MASK | 
    GDK_GC_SUBWINDOW | GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN | 
    GDK_GC_EXPOSURES | GDK_GC_LINE_WIDTH | GDK_GC_LINE_STYLE | 
    GDK_GC_CAP_STYLE | GDK_GC_JOIN_STYLE;
  
  GdkColor gc_white = {0, 65535, 65535, 65535};
  GdkColor gc_black = {0, 0, 0, 0};
  
  GdkEventMask mask = 
    GDK_POINTER_MOTION_MASK | GDK_BUTTON_PRESS_MASK | 
    GDK_BUTTON_RELEASE_MASK;
  GdkCursor *xhair_cursor = gdk_cursor_new (GDK_CROSSHAIR);

  gboolean pressed = FALSE;
  gboolean done = FALSE;
  gint x, y, w, h;
  
  /*Set up graphics context for a XOR rectangle that will be drawn as 
   * the user drags the mouse */
  TRACE ("Initialize the graphics context");
  
  gc_values.function           = GDK_XOR;
  gc_values.line_width         = 0;
  gc_values.line_style         = GDK_LINE_SOLID;
  gc_values.fill               = GDK_SOLID;
  gc_values.cap_style          = GDK_CAP_BUTT;
  gc_values.join_style         = GDK_JOIN_MITER;
  gc_values.graphics_exposures = FALSE;
  gc_values.clip_x_origin      = 0;
  gc_values.clip_y_origin      = 0;
  gc_values.clip_mask          = None;
  gc_values.subwindow_mode     = GDK_INCLUDE_INFERIORS;
  
  gc = gdk_gc_new_with_values (root_window, &gc_values, values_mask);
  gdk_gc_set_rgb_fg_color (gc, &gc_white);
  gdk_gc_set_rgb_bg_color (gc, &gc_black);
  
  /* Change cursor to cross-hair */
  TRACE ("Set the cursor");
  
  grabstatus = gdk_pointer_grab (root_window, FALSE, mask,
                                 NULL, xhair_cursor, GDK_CURRENT_TIME);
  
  while (!done && grabstatus == GDK_GRAB_SUCCESS)
    {
      gint x1, y1, x2, y2;
      GdkEvent *event;
      
      event = gdk_event_get ();
      
      if (event == NULL) 
        continue;
        
      switch (event->type)
        {
          /* Start dragging the rectangle out */
     
          case GDK_BUTTON_PRESS:

            TRACE ("Start dragging the rectangle");
            
            x = x2 = x1 = event->button.x;
            y = y2 = y1 = event->button.y;
            w = 0; h = 0;
            pressed = TRUE;
            break;
          
          /* Finish dragging the rectangle out */
          case GDK_BUTTON_RELEASE:
            if (pressed)
              {
                if (w > 0 && h > 0)
                  {
                    /* Remove the rectangle drawn previously */

                    TRACE ("Remove the rectangle drawn previously");
                    
                    gdk_draw_rectangle (root_window, 
                                        gc, 
                                        FALSE, 
                                        x, y, w, h);
                    done = TRUE;
                  } 
                else 
                  {
                    /* The user has not dragged the mouse, start again */

                    TRACE ("Mouse was not dragged, start agan");
                   
                    pressed = FALSE;
                  }
              }
          break;
          
          /* The user is moving the mouse */
          case GDK_MOTION_NOTIFY:
            if (pressed)
              {
                TRACE ("Mouse is moving");

                if (w > 0 && h > 0)
               
                /* Remove the rectangle drawn previously */

                TRACE ("Remove the rectangle drawn previously");
                
                gdk_draw_rectangle (root_window, 
                                    gc, 
                                    FALSE, 
                                    x, y, w, h);

                x2 = event->motion.x;
                y2 = event->motion.y;

                x = MIN (x1, x2);
                y = MIN (y1, y2);
                w = ABS (x2 - x1);
                h = ABS (y2 - y1);

                /* Draw  the rectangle as the user drags  the mouse */

                TRACE ("Draw the new rectangle");
                
                if (w > 0 && h > 0)
                  gdk_draw_rectangle (root_window, 
                                      gc, 
                                      FALSE, 
                                      x, y, w, h);
            
              }
            break;
           
          default: 
            break;
        }
      
      gdk_event_free (event);
    }
 
  if (grabstatus == GDK_GRAB_SUCCESS) 
    {
      TRACE ("Ungrab the pointer");

      gdk_pointer_ungrab(GDK_CURRENT_TIME);
    }
  
  /* Get the screenshot's pixbuf */

  TRACE ("Get the pixbuf for the screenshot");
  
  screenshot = gdk_pixbuf_get_from_drawable (NULL, root_window, NULL,
                                             x, y, 0, 0, w, h);
  
  if (gc!=NULL)
    g_object_unref (gc);
    
  gdk_cursor_unref (xhair_cursor);
  
  return screenshot;
}



/* Public */



/* Takes the screenshot with the options given in sd.
*sd: a ScreenshotData struct.
returns: the screenshot in a *GdkPixbuf.
*/
GdkPixbuf *screenshooter_take_screenshot (gint region, gint delay)
{
  GdkPixbuf *screenshot = NULL;
  GdkWindow *window = NULL;
  GdkScreen *screen;
      
  /* gdk_get_default_root_window () does not need to be unrefed, 
   * needs_unref enables us to unref *window only if a non default 
   * window has been grabbed. */
  gboolean needs_unref = TRUE;
  
  /* Get the screen on which the screenshot should be taken */
  screen = gdk_screen_get_default ();
  
  /* wait for n=delay seconds */ 
  if (region != SELECT)
    sleep (delay);
    
  /* Get the window/desktop we want to screenshot*/  
  if (region == FULLSCREEN)
    {
      TRACE ("We grab the entire screen");

      window = gdk_get_default_root_window ();
      needs_unref = FALSE;
    } 
  else if (region == ACTIVE_WINDOW)
    {
      TRACE ("We grab the active window");

      window = get_active_window (screen, &needs_unref);      
    }
      
  if (region == FULLSCREEN || region == ACTIVE_WINDOW)
    {
      TRACE ("Get the screenshot of the given window");

      screenshot = get_window_screenshot (window);
          
      if (needs_unref)
	      g_object_unref (window);
    }
  else if (region == SELECT)
    {
      TRACE ("Let the user select the region to screenshot");

      screenshot = get_rectangle_screenshot ();
    }

		
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

  TRACE ("Adding the image to the clipboard...");

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
                            ScreenshotData      *sd)
{
  XfceRc *rc;
  gint delay = 0;
  gint region = FULLSCREEN;
  gint action = SAVE;
  gint show_save_dialog = 1;
  gchar *screenshot_dir = g_strdup (DEFAULT_SAVE_DIRECTORY);
  #ifdef HAVE_GIO
  gchar *app = g_strdup ("none");
  #endif
  
  if (file != NULL)
    {
      TRACE ("Open the rc file");

      rc = xfce_rc_simple_open (file, TRUE);

      if (rc != NULL)
        {
          TRACE ("Read the entries");

          delay = xfce_rc_read_int_entry (rc, "delay", 0);
              
          region = xfce_rc_read_int_entry (rc, "region", FULLSCREEN);
              
          action = xfce_rc_read_int_entry (rc, "action", SAVE);
              
          show_save_dialog = 
            xfce_rc_read_int_entry (rc, "show_save_dialog", 1);
              
          #ifdef HAVE_GIO
          g_free (app);
              
          app = 
            g_strdup (xfce_rc_read_entry (rc, "app", "none"));
          #endif
           
          g_free (screenshot_dir);
          
          screenshot_dir = 
            g_strdup (xfce_rc_read_entry (rc, 
                                          "screenshot_dir", 
                                          DEFAULT_SAVE_DIRECTORY));
        }

      TRACE ("Close the rc file");
      
      xfce_rc_close (rc);
    }
   
  /* And set the sd values */
  TRACE ("Set the values of the struct");
  
  sd->delay = delay;
  sd->region = region;
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
  
  g_return_if_fail (file != NULL);

  TRACE ("Open the rc file");

  rc = xfce_rc_simple_open (file, FALSE);
  
  g_return_if_fail (rc != NULL);

  TRACE ("Write the entries.");
  
  xfce_rc_write_int_entry (rc, "delay", sd->delay);
  xfce_rc_write_int_entry (rc, "region", sd->region);
  xfce_rc_write_int_entry (rc, "action", sd->action);
  xfce_rc_write_int_entry (rc, "show_save_dialog", 
                           sd->show_save_dialog);
  xfce_rc_write_entry (rc, "screenshot_dir", sd->screenshot_dir);
  #ifdef HAVE_GIO
  xfce_rc_write_entry (rc, "app", sd->app);
  #endif

  TRACE ("Flush and close the rc file");

  xfce_rc_flush (rc);
  
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
      TRACE ("Path was != NULL");

      if (!g_str_equal (application, "none"))
        {
          gchar *command = 
            g_strconcat (application, " ", screenshot_path, NULL);
    
          GError *error = NULL;

          TRACE ("Launch the command");
          
          /* Execute the command and show an error dialog if there was 
          * an error. */
          if (!xfce_exec_on_screen (gdk_screen_get_default (), command, 
                                    FALSE, TRUE, &error))
            {
              TRACE ("An error occured");

              xfce_err (error->message);
              g_error_free (error);
            }
          
          g_free (command);
       }
    }
}     
#endif
