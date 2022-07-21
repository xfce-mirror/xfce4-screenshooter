/*  $Id$
 *
 *  Copyright © 2008-2010 Jérôme Guelfucci <jeromeg@xfce.org>
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
 * */

#ifndef __HAVE_CUSTOM_ACTIONS_H__
#define __HAVE_CUSTOM_ACTIONS_H__

#include <gio/gio.h>
#include <gtk/gtk.h>

typedef struct _ScreenshooterCustomAction ScreenshooterCustomAction;


struct _ScreenshooterCustomAction {
    gchar *name;
    gchar *command;
};

enum {
    CUSTOM_ACTION_NAME=0,
    CUSTOM_ACTION_COMMAND,
};

#endif