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
 
#include "screenshooter-open-with.h"
 
static void populate_liststore (GtkListStore* liststore);
 
/* Internals */

static void populate_liststore (GtkListStore* liststore)
{
  const gchar *content_type;
  GList	*list_app;
  GtkTreeIter iter;
  gpointer data;
  gint n = 0;
  
  content_type = "image/png";
    
  list_app = g_app_info_get_all_for_type (content_type);
  
  if (list_app != NULL)
    {
      while (( data = g_list_nth_data (list_app, n)) != NULL)
        {
          gchar *command = 
            g_strdup (g_app_info_get_executable (G_APP_INFO (data)));
                   
          gtk_list_store_append (liststore, &iter);
          
          gtk_list_store_set (liststore, &iter, 0, command, -1);
          
          g_free (command);
          n++;
        }
      
      g_list_free (list_app);
    }
}



/* Public */



GtkWidget *screenshooter_open_with_combo_box ()
{
  GtkListStore *liststore = gtk_list_store_new (1, G_TYPE_STRING);
  GtkWidget *combobox;
  
  populate_liststore (liststore);
  
  combobox = gtk_combo_box_new_with_model (GTK_TREE_MODEL (liststore));
  
  return combobox;
}
