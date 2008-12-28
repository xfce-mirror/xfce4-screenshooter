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

#ifndef __HAVE_DIALOGS_H__
#define __HAVE_DIALOGS_H__
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_GIO
#include <gio/gio.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>
#include <libxfcegui4/libxfcegui4.h>

#include "screenshooter-utils.h"



GtkWidget   
*screenshooter_dialog_new          (ScreenshotData      *sd, 
                                    gboolean             plugin);
gchar 
*screenshooter_save_screenshot     (GdkPixbuf           *screenshot, 
                                    gboolean             show_save_dialog,
                                    gchar               *default_dir);
#endif                                    
