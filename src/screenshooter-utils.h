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

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gstdio.h>

#include <libxfce4util/libxfce4util.h>

#include <unistd.h>

#define DEFAULT_SAVE_DIRECTORY xfce_get_homedir ()

enum {
  MODE_0,
  FULLSCREEN,
  ACTIVE_WINDOW,
};

/* Struct to store the screenshot options */
typedef struct
{
  gint mode;
  gint show_save_dialog;

  gint delay;
  gchar *screenshot_dir;
}
ScreenshotData;

GdkPixbuf *take_screenshot       (gint         mode, 
                                  gint         delay);
void save_screenshot             (GdkPixbuf   *screenshot, 
                                  gboolean     show_save_dialog,
                                  gchar       *default_dir);
