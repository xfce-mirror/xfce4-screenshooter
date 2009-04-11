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

#ifndef __HAVE_UTILS_H__
#define __HAVE_UTILS_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "screenshooter-global.h"

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include <unistd.h>



void
screenshooter_copy_to_clipboard  (GdkPixbuf            *screenshot) ;

void 
screenshooter_read_rc_file       (gchar                *file, 
                                  ScreenshotData       *sd);
                                  
void 
screenshooter_write_rc_file      (gchar                *file, 
                                  ScreenshotData       *sd);

void
screenshooter_open_screenshot    (gchar                *screenshot_path,
                                  gchar                *application);

gchar
*screenshooter_get_home_uri      ();

gboolean
screenshooter_is_remote_uri      (const gchar          *uri);

#endif                               
