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
static gchar *generate_filename_for_uri(char *uri);
static Window find_toplevel_window (Window xid);

/* Borrowed from gnome-screenshot */
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


static gchar *generate_filename_for_uri(char *uri)
{
  gchar *file_name;
  unsigned int i = 0;
    
  if ( uri == NULL )
    {
  	  return NULL;
    }      
  
  file_name = g_strdup ( _("Screenshot.png") );
    
  if( g_access (g_build_filename (uri, file_name, NULL), F_OK ) != 0 ) 
    {
      return file_name;
    }
    
  do
    {
      i++;
      g_free (file_name);
      file_name = g_strdup_printf ( _("Screenshot-%d.png"), i);
    }
  while( g_access (g_build_filename (uri, file_name, NULL), F_OK ) == 0 );
    
  return file_name;
}

/* Public */

GdkPixbuf *take_screenshot (ScreenshotData * sd)
{
  GdkPixbuf *screenshot;
  GdkWindow *window;
  GdkScreen *screen;
  
  gint width;
  gint height;
  gboolean needs_unref = TRUE;
  
  screen = gdk_screen_get_default ();
    
  if (sd->whole_screen)
    {
      window = gdk_get_default_root_window();
      needs_unref = FALSE;
    } 
  else 
    {
      window = gdk_screen_get_active_window (screen);
    
      if (gdk_window_get_type_hint (window) == GDK_WINDOW_TYPE_HINT_DESKTOP)
        {
          window = gdk_get_default_root_window();
          needs_unref = FALSE;
        }
      else
        {
          window = 
            gdk_window_foreign_new (find_toplevel_window (GDK_WINDOW_XID(window)));
        }
    }
  
  sleep(sd->screenshot_delay);
  
  gdk_drawable_get_size(window, &width, &height);

  screenshot = gdk_pixbuf_get_from_drawable (NULL,
					     window,
					     NULL, 0, 0, 0, 0,
					     width, height);
					     
	if (needs_unref)
	  g_object_unref (window);
		
	return screenshot;
}

void save_screenshot (GdkPixbuf * screenshot, ScreenshotData * sd)
{
  GdkPixbuf * thumbnail;
  gchar * filename = NULL;
  GtkWidget * preview;
  GtkWidget * chooser;
  gint dialog_response;

  filename = generate_filename_for_uri ( sd->screenshot_dir );
    
  if (sd->show_save_dialog)
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
      gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (chooser), 
                                                      TRUE);
      gtk_dialog_set_default_response (GTK_DIALOG (chooser), 
                                     GTK_RESPONSE_ACCEPT);
      gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER (chooser), 
                                        sd->screenshot_dir);

      preview = gtk_image_new ();
  
      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser), filename);
      gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (chooser), preview);
  
      thumbnail = 
        gdk_pixbuf_scale_simple (screenshot, gdk_pixbuf_get_width(screenshot)/5, 
                                 gdk_pixbuf_get_height(screenshot)/5, 
                                 GDK_INTERP_BILINEAR);
      gtk_image_set_from_pixbuf (GTK_IMAGE (preview), thumbnail);
      g_object_unref (thumbnail);
    
      dialog_response = gtk_dialog_run (GTK_DIALOG (chooser));
	  
	    if (dialog_response == GTK_RESPONSE_ACCEPT)
	      {
	        filename = 
	          gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser) );
          gdk_pixbuf_save (screenshot, filename, "png", NULL, NULL);
	      }
	  
	    gtk_widget_destroy ( GTK_WIDGET ( chooser ) );
	  }  
	else
	  {    
	    /* Else, we just save the file in the default folder */
      filename = g_build_filename (sd->screenshot_dir, filename, NULL);
	    gdk_pixbuf_save (screenshot, filename, "png", NULL, NULL);
	  }
  g_free (filename);
}
