/*  $Id$
 *
 *  Copyright © 2008-2009 Jérôme Guelfucci <jeromeg@xfce.org>
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

#ifndef __HAVE_ACTIONS_H__
#define __HAVE_ACTIONS_H__

#include "screenshooter-global.h"

typedef struct _ScreenshooterCustomActionDialogData ScreenshooterCustomActionDialogData;
typedef struct _ScreenshooterCustomAction ScreenshooterCustomAction;

struct _ScreenshooterCustomAction {
    GtkListStore *liststore;
    GtkTreeIter selected_action;
    ScreenshooterCustomActionDialogData *data;
};
struct _ScreenshooterCustomActionDialogData {
    GtkWidget *name, *cmd, *tree_view;
    GtkTreeSelection *selection;
};

void screenshooter_take_screenshot      (ScreenshotData *sd,
                                         gboolean immediate);
void
screenshooter_custom_action_save (GtkTreeModel *list_store);
void
screenshooter_custom_action_load (GtkListStore *list_store);
ScreenshooterCustomAction *screenshooter_custom_actions_get (void);
void screenshooter_custom_action_execute (void);

#endif
