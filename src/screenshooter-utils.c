/*  $Id$
 *
 *  Copyright © 2004 German Poo-Caaman~o <gpoo@ubiobio.cl>
 *  Copyright © 2005,2006 Daniel Bobadilla Leal <dbobadil@dcc.uchile.cl>
 *  Copyright © 2005 Jasper Huijsmans <jasper@xfce.org>
 *  Copyright © 2006 Jani Monoses <jani@ubuntu.com>
 *  Copyright © 2008 Jérôme Guelfucci <jerome.guelfucci@gmail.com>
 *
 *  Portions from the Gimp sources by
 *  Copyright © 1998-2000 Sven Neumann <sven@gimp.org>
 *  Copyright © 2003 Henrik Brix Andersen <brix@gimp.org>
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

static Window get_window_property (Window xwindow, Atom atom);
static Window find_toplevel_window (Window xid);
static Window screenshot_find_active_window (void);

/* Internals */

/* Borrowed from libwnck */
static Window
get_window_property (Window  xwindow,
		     Atom    atom)
{
  Atom type;
  int format;
  gulong nitems;
  gulong bytes_after;
  Window *w;
  int err, result;
  Window retval;

  gdk_error_trap_push ();

  type = None;
  result = XGetWindowProperty (gdk_display,
			       xwindow,
			       atom,
			       0, G_MAXLONG,
			       False, XA_WINDOW, &type, &format, &nitems,
			       &bytes_after, (unsigned char **) &w);  
  err = gdk_error_trap_pop ();

  if (err != Success ||
      result != Success)
    return None;
  
  if (type != XA_WINDOW)
    {
      XFree (w);
      return None;
    }

  retval = *w;
  XFree (w);
  
  return retval;
}

/* Borrowed from gnome-screenshot */
static Window
screenshot_find_active_window (void)
{
  Window retval = None;
  Window root_window;

  root_window = GDK_ROOT_WINDOW ();

  if (gdk_net_wm_supports (gdk_atom_intern ("_NET_ACTIVE_WINDOW", FALSE)))
    {
      retval = get_window_property (root_window,
				    gdk_x11_get_xatom_by_name ("_NET_ACTIVE_WINDOW"));
    }

  return retval;  
}

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
	  g_warning ("Couldn't find window manager window");
	  return None;
	}

      if (root == parent)
	return xid;

      xid = parent;
    }
  while (TRUE);
}

/* Public */

GdkPixbuf *take_screenshot (gint fullscreen, gint delay)
{
  GdkPixbuf * screenshot;
  GdkWindow * window;
  gint width;
  gint height;
  
  if (fullscreen)
  {
    window = gdk_get_default_root_window();
  } 
  else 
  {
    window = gdk_window_foreign_new (find_toplevel_window (screenshot_find_active_window ()));
  }
  
  sleep(delay);
  
  gdk_drawable_get_size(window, &width, &height);

  screenshot = gdk_pixbuf_get_from_drawable (NULL,
					     window,
					     NULL, 0, 0, 0, 0,
					     width, height);
	
	return screenshot;
}

gchar *generate_filename_for_uri(char *uri)
{
  int test;
  gchar *file_name;
  unsigned int i = 0;
    
  if ( uri == NULL )
  {
  	return NULL;
  }      
  
  file_name = g_strdup ("Screenshot.png");
    
  if( ( test = g_access ( g_build_filename (uri, file_name, NULL), F_OK ) ) != 0 ) 
  {
    return file_name;
  }
    
  do
  {
    i++;
    g_free (file_name);
    file_name = g_strdup_printf ("Screenshot-%d.png", i);
  }
  while( ( test = g_access ( g_build_filename (uri, file_name, NULL), F_OK ) ) == -1 );
    
  return file_name;
}
