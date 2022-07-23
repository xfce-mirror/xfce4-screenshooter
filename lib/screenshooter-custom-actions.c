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

#include "screenshooter-custom-actions.h"



void ca_dialog_tree_selection_name_cb (GtkTreeSelection *selection, gpointer data) {
  GtkTreeIter iter;
  GtkTreeModel *model;
  gchar *text;
  GtkEntry *entry = GTK_ENTRY (data);

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, CUSTOM_ACTION_NAME, &text, -1);
    gtk_widget_set_sensitive (GTK_WIDGET (entry), TRUE);
    gtk_entry_set_text (entry, text);
    g_free (text);
  }
  else {
    gtk_widget_set_sensitive (GTK_WIDGET (entry), FALSE);
  }
}



void ca_dialog_tree_selection_command_cb (GtkTreeSelection *selection, gpointer data) {
  GtkTreeIter iter;
  GtkTreeModel *model;
  gchar *text;
  GtkEntry *entry = GTK_ENTRY (data);

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, CUSTOM_ACTION_COMMAND, &text, -1);
    gtk_widget_set_sensitive (GTK_WIDGET (entry), TRUE);
    gtk_entry_set_text (entry, text);
    g_free (text);
  }
  else {
    gtk_widget_set_sensitive (GTK_WIDGET (entry), FALSE);
  }
}


void ca_populate_liststore (GtkListStore *liststore) {
    return;
}