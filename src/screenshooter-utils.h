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
 
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <fcntl.h>
#include <X11/Xatom.h>
#include <unistd.h>

GdkPixbuf *take_screenshot (gint fullscreen, gint delay);
gchar *generate_filename_for_uri(char *uri);
