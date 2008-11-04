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

#ifndef __HAVE_UTILS_H__
#define __HAVE_UTILS_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <glib/gstdio.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <unistd.h>



#define DEFAULT_SAVE_DIRECTORY xfce_get_homedir ()
#define DEFAULT_APPLICATION "none"



/* Screenshot Modes */
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
  
  #ifdef HAVE_GIO
  gchar *app;
  #endif
  
}
ScreenshotData;



GdkPixbuf 
*screenshooter_take_screenshot   (gint                  mode, 
                                  gint                  delay);
                                  
gchar 
*screenshooter_save_screenshot   (GdkPixbuf            *screenshot, 
                                  gboolean              show_save_dialog,
                                  gchar                *default_dir);
                                  
void 
screenshooter_read_rc_file       (gchar                *file, 
                                  ScreenshotData       *sd, 
                                  gboolean              dir_only);
                                  
void 
screenshooter_write_rc_file      (gchar                *file, 
                                  ScreenshotData       *sd);

#ifdef HAVE_GIO                                 
void
screenshooter_open_screenshot    (gchar *screenshot_path,
                                  gchar *application);                                  
#endif
#endif                               
