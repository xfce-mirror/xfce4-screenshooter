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



void ca_dialog_tree_selection_cb (GtkTreeSelection *selection, gpointer data) {
  GtkTreeIter iter;
  GtkTreeModel *model;
  gchar *name, *cmd;
  ScreenshooterCustomActionDialog *dialog = data;

  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_tree_model_get (model, &iter, CUSTOM_ACTION_NAME, &name, CUSTOM_ACTION_COMMAND, &cmd, -1);
    gtk_widget_set_sensitive (dialog->name, TRUE);
    gtk_entry_set_text (GTK_ENTRY (dialog->name), name);
    gtk_widget_set_sensitive (dialog->cmd, TRUE);
    gtk_entry_set_text (GTK_ENTRY (dialog->cmd), cmd);
    g_free (cmd);
    g_free (name);
  }
  else {
    gtk_widget_set_sensitive (GTK_WIDGET (dialog->name), FALSE);
    gtk_widget_set_sensitive (GTK_WIDGET (dialog->cmd), FALSE);
  }
}



void ca_dialog_values_changed_cb (GtkEditable* self, gpointer user_data) {
  ScreenshooterCustomActionDialog *dialog = user_data;
  GtkTreeSelection *selection = dialog->selection;
  GtkTreeIter iter;
  GtkTreeModel *model;
  const gchar *name = gtk_entry_get_text (GTK_ENTRY (dialog->name));
  const gchar *cmd = gtk_entry_get_text (GTK_ENTRY (dialog->cmd));
  if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
    gtk_list_store_set (GTK_LIST_STORE (model), &iter, CUSTOM_ACTION_NAME, name, CUSTOM_ACTION_COMMAND, cmd, -1);
  }
}



void ca_dialog_add_button_cb (GtkToolButton* self, gpointer user_data) {
  GtkTreeIter iter;
  GtkTreeSelection *selection;
  GtkTreeView *tree_view = GTK_TREE_VIEW (user_data);
  GtkTreeModel *liststore = gtk_tree_view_get_model (tree_view);
  gtk_list_store_append (GTK_LIST_STORE (liststore), &iter);
  selection = gtk_tree_view_get_selection (tree_view);
  gtk_tree_selection_select_iter (selection, &iter);
}