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

#include "screenshooter-dialogs.h"

#define ICON_SIZE 22

/* Prototypes */ 

static void cb_fullscreen_screen_toggled       (GtkToggleButton    *tb,
                                                ScreenshotData     *sd);
static void cb_active_window_toggled           (GtkToggleButton    *tb,
                                                ScreenshotData     *sd);
static void cb_show_save_dialog_toggled        (GtkToggleButton    *tb,
                                                ScreenshotData     *sd);
static void cb_default_folder                  (GtkWidget          *chooser, 
                                                ScreenshotData     *sd);                                        
static void cb_delay_spinner_changed           (GtkWidget          *spinner, 
                                                ScreenshotData     *sd);
#ifdef HAVE_GIO                                                
static void cb_combo_active_item_changed       (GtkComboBox        *box, 
                                                ScreenshotData     *sd);
static void add_item                           (GAppInfo           *app_info, 
                                                GtkWidget          *liststore);
static void populate_liststore                 (GtkListStore       *liststore);
static void set_default_item                   (GtkWidget          *combobox, 
                                                gchar              *default_app);                                               
                                                                                               
#endif                                                                                                                                                                         
                                      
/* Internals */

static void cb_fullscreen_screen_toggled (GtkToggleButton *tb,
                                          ScreenshotData   *sd)
{
  if (gtk_toggle_button_get_active (tb))
    {
      sd->mode = FULLSCREEN;
    }
  else
    {
      sd->mode = ACTIVE_WINDOW;
    }
}



static void cb_active_window_toggled (GtkToggleButton *tb,
                                      ScreenshotData   *sd)
{
  if (gtk_toggle_button_get_active (tb))
    {
      sd->mode = ACTIVE_WINDOW;
    }
  else
    {
      sd->mode = FULLSCREEN;
    }
}



static void cb_show_save_dialog_toggled (GtkToggleButton *tb,
                                         ScreenshotData   *sd)
{
  sd->show_save_dialog = gtk_toggle_button_get_active (tb);
}                                  



static void cb_default_folder (GtkWidget       *chooser, 
                               ScreenshotData  *sd)
{
  sd->screenshot_dir = 
    gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
}

   

static void cb_delay_spinner_changed (GtkWidget       *spinner, 
                                      ScreenshotData  *sd)
{
  sd->delay = 
    gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spinner));
}



#ifdef HAVE_GIO



static void cb_combo_active_item_changed (GtkComboBox *box, ScreenshotData *sd)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (box));
  GtkTreeIter iter;
  gchar *active_command = NULL;
   
  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (box), &iter);
  
  gtk_tree_model_get (model, &iter, 2, &active_command, -1);
  
  if (sd->app != NULL)
    g_free (sd->app);
  
  sd->app = active_command; 
}



static void add_item (GAppInfo *app_info, GtkWidget *liststore)
{
  GtkTreeIter iter;
  gchar *command = g_strdup (g_app_info_get_executable (app_info));
  gchar *name = g_strdup (g_app_info_get_name (app_info));
  GIcon *icon = g_app_info_get_icon (app_info);
  GdkPixbuf *pixbuf = NULL;
  
  if (G_IS_LOADABLE_ICON (icon))
    {
      GFile *file = g_file_icon_get_file (G_FILE_ICON (icon));
      gchar *path = g_file_get_path (file);
      
      pixbuf = 
        gdk_pixbuf_new_from_file_at_size (path, ICON_SIZE, ICON_SIZE, NULL);
      
      g_free (path);
      g_object_unref (file);
    }
  else
    {
      gchar **names = NULL;
      
      g_object_get (G_OBJECT (icon), "names", &names, NULL);
           
      if (names != NULL)
        {
          GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
          
          if (names[0] != NULL)
            {
              pixbuf = 
                gtk_icon_theme_load_icon (icon_theme, 
                                          names[0], 
                                          ICON_SIZE,
                                          GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                          NULL);
            }                                          
          
          g_strfreev (names);                                        
        }
    }
  
  gtk_list_store_append (GTK_LIST_STORE (liststore), &iter);
          
  gtk_list_store_set (GTK_LIST_STORE (liststore), &iter, 0, pixbuf, 1, name,
                      2, command, -1);
          
  g_free (command);
  g_free (name);
  if (pixbuf != NULL)
    g_object_unref (pixbuf);
  g_object_unref (icon);
}



static void populate_liststore (GtkListStore *liststore)
{
  const gchar *content_type;
  GList	*list_app;
  GtkTreeIter iter;
  
  gtk_list_store_append (GTK_LIST_STORE (liststore), &iter);
  
  gtk_list_store_set (GTK_LIST_STORE (liststore), 
                      &iter, 
                      0, NULL, 
                      1, _("Do not open screenshots"),
                      2, "none", 
                      -1);
     
  content_type = "image/png";
    
  list_app = g_app_info_get_all_for_type (content_type);
  
  if (list_app != NULL)
    {
      g_list_foreach (list_app, (GFunc) add_item, liststore);
            
      g_list_free (list_app);
    }
}

static void set_default_item (GtkWidget     *combobox, 
                              gchar         *default_app)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
  GtkTreeIter iter; 
  gchar *command = NULL;
  
  gtk_tree_model_get_iter_first (model , &iter);    

  do
    {
      gtk_tree_model_get (model, &iter, 2, &command, -1);
      
      if (g_str_equal (command, default_app))
        {
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
          break;
        }
      
      g_free (command);      
    }
  while (gtk_tree_model_iter_next (model, &iter)); 
}                              
#endif

                      
/* Public */



GtkWidget *screenshooter_dialog_new (ScreenshotData  *sd, gboolean plugin)
{
  GtkWidget *dlg;
  GtkWidget *vbox;
  GtkWidget *modes_frame, *modes_box;
  GtkWidget *active_window_button, *fullscreen_button;
  GtkWidget *options_frame, *options_box;
  GtkWidget *save_button;
  GtkWidget *default_save_label, *dir_chooser;
  GtkWidget *delay_label, *delay_box, *delay_spinner, *label2;
#ifdef HAVE_GIO
  GtkWidget *open_with_label;
  GtkListStore *liststore;
  GtkWidget *combobox;
  GtkCellRenderer *renderer, *renderer_pixbuf;
#endif
  
  /* Create the dialog */
  if (!plugin)
    {
      dlg = 
        xfce_titled_dialog_new_with_buttons (_("Screenshot"),
                                             NULL,
                                             GTK_DIALOG_DESTROY_WITH_PARENT |
                                             GTK_DIALOG_NO_SEPARATOR,
                                             GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL,
                                             GTK_STOCK_OK, GTK_RESPONSE_OK,
                                             NULL);
    }
  else
    {
      dlg =
        xfce_titled_dialog_new_with_buttons (_("Screenshooter plugin"),
                                             NULL,
                                             GTK_DIALOG_DESTROY_WITH_PARENT |
                                             GTK_DIALOG_NO_SEPARATOR,
                                             GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                                             NULL);
    }                                             

  gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
  
  gtk_container_set_border_width (GTK_CONTAINER (dlg), 2);
  gtk_window_set_icon_name (GTK_WINDOW (dlg), "applets-screenshooter");
  
  /* Create the main box for the dialog */
	vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_widget_show (vbox);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), vbox,
                      TRUE, TRUE, 0);   
  
  /* Create the frame for screenshot modes and fill it with the radio buttons */
  modes_frame = gtk_frame_new (_("Modes"));
  gtk_container_add (GTK_CONTAINER (vbox), modes_frame);
  gtk_widget_show (modes_frame);
      
  modes_box = gtk_vbox_new (FALSE, 8);
  gtk_container_add (GTK_CONTAINER (modes_frame), modes_box);
  gtk_container_set_border_width (GTK_CONTAINER (modes_box), 6);
  gtk_widget_show (modes_box); 
  
  fullscreen_button = 
    gtk_radio_button_new_with_mnemonic (NULL, 
                                        _("Take a screenshot of desktop"));
  gtk_widget_show (fullscreen_button);
  gtk_box_pack_start (GTK_BOX (modes_box), fullscreen_button, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fullscreen_button),
                                (sd->mode == FULLSCREEN));
  g_signal_connect (fullscreen_button, "toggled", 
                    G_CALLBACK (cb_fullscreen_screen_toggled),
                    sd);
  
  active_window_button = 
    gtk_radio_button_new_with_mnemonic (gtk_radio_button_get_group (GTK_RADIO_BUTTON (fullscreen_button)), 
                                        _("Take a screenshot of the active window"));
  gtk_widget_show (active_window_button);
  gtk_box_pack_start (GTK_BOX (modes_box), active_window_button, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active_window_button),
                                (sd->mode == ACTIVE_WINDOW));
  g_signal_connect (active_window_button, "toggled", 
                    G_CALLBACK (cb_active_window_toggled),
                    sd);
  
  /* Create the options frame and add the delay and save options */
  options_frame = gtk_frame_new (_("Options"));
  gtk_container_add(GTK_CONTAINER (vbox), options_frame);
  gtk_widget_show (options_frame);
  
  options_box = gtk_vbox_new (FALSE, 8);
  gtk_container_add (GTK_CONTAINER (options_frame), options_box);
  gtk_container_set_border_width (GTK_CONTAINER (options_box), 6);
  gtk_widget_show (options_box);
  
  
  if (plugin)
    {
		  /* Save option */
		  save_button = gtk_check_button_new_with_mnemonic (_("Show save dialog"));
		  gtk_widget_show (save_button);
		  gtk_box_pack_start (GTK_BOX (options_box), save_button, FALSE, FALSE, 0);
		  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (save_button),
		                                sd->show_save_dialog);
		  g_signal_connect (save_button, "toggled", 
		                    G_CALLBACK (cb_show_save_dialog_toggled), sd);
    }

  if (plugin)
    {
		  /* Default save location */          
		  default_save_label = gtk_label_new ("");
		  gtk_label_set_markup (GTK_LABEL (default_save_label),
			_("<span weight=\"bold\" stretch=\"semiexpanded\">Default save location</span>"));
			
			gtk_misc_set_alignment (GTK_MISC (default_save_label), 0, 0);
		  gtk_widget_show (default_save_label);
		  gtk_container_add (GTK_CONTAINER (options_box), default_save_label);
		  
		  dir_chooser = 
		    gtk_file_chooser_button_new (_("Default save location"), 
		                                 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
		  gtk_widget_show (dir_chooser);
		  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dir_chooser), 
		                                       sd->screenshot_dir);
		  gtk_container_add (GTK_CONTAINER (options_box), dir_chooser);
		  g_signal_connect (dir_chooser, "selection-changed", 
		                    G_CALLBACK (cb_default_folder), sd);
    }
                    
  /* Screenshot delay */
  delay_label = gtk_label_new ("");
  
  gtk_label_set_markup (GTK_LABEL(delay_label),
  _("<span weight=\"bold\" stretch=\"semiexpanded\">Delay before taking the screenshot</span>"));
  
	gtk_misc_set_alignment(GTK_MISC (delay_label), 0, 0); 
  gtk_widget_show (delay_label);
  gtk_container_add (GTK_CONTAINER (options_box), delay_label);
  
  delay_box = gtk_hbox_new(FALSE, 8);
  gtk_widget_show (delay_box);
  gtk_box_pack_start (GTK_BOX (options_box), delay_box, FALSE, FALSE, 0);

  delay_spinner = gtk_spin_button_new_with_range(0.0, 60.0, 1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (delay_spinner), 
                             sd->delay);
  gtk_widget_show (delay_spinner);
  gtk_box_pack_start (GTK_BOX (delay_box), delay_spinner, FALSE, 
                      FALSE, 0);

  label2 = gtk_label_new_with_mnemonic(_("seconds"));
  gtk_widget_show (label2);
  gtk_box_pack_start (GTK_BOX (delay_box), label2, FALSE, FALSE, 0);

  g_signal_connect (delay_spinner, "value-changed",
                    G_CALLBACK (cb_delay_spinner_changed), sd);
                    
  /* Open with */
#ifdef HAVE_GIO 
  open_with_label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL(open_with_label),
  _("<span weight=\"bold\" stretch=\"semiexpanded\">Open the screenshot with</span>"));
  
  gtk_misc_set_alignment(GTK_MISC (open_with_label), 0, 0); 
  gtk_widget_show (open_with_label);
  gtk_container_add (GTK_CONTAINER (options_box), open_with_label);
  
  liststore = gtk_list_store_new (3, GDK_TYPE_PIXBUF, 
                                  G_TYPE_STRING, G_TYPE_STRING);
  
  combobox = gtk_combo_box_new_with_model (GTK_TREE_MODEL (liststore));
  
  renderer = gtk_cell_renderer_text_new ();
  renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
  
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), 
                              renderer_pixbuf, FALSE);
  gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), 
                                  renderer, "text", 1, NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer_pixbuf,
                                  "pixbuf", 0, NULL);                                  
  
  populate_liststore (liststore);
  
  set_default_item (combobox, sd->app);
  
  gtk_container_add (GTK_CONTAINER (options_box), combobox);
  
  g_signal_connect (G_OBJECT (combobox), "changed", 
                    G_CALLBACK (cb_combo_active_item_changed), sd);
  
  gtk_widget_show_all (combobox);                    
#endif

  return dlg;                
}


void screenshooter_preferences_dialog (gchar *rc_file, 
                                       gchar *current_default_dir)
{
  GtkWidget * chooser;
  gint dialog_response;
  gchar * dir;
  XfceRc *rc;
  
  /* The preferences dialog is a plain gtk_file_chooser, we just get the
  folder the user selected and write it in the conf file*/
  
  chooser = 
    gtk_file_chooser_dialog_new (_("Default save folder"),
                                  NULL,
                                  GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                  GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                  GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                  NULL);
  gtk_window_set_icon_name (GTK_WINDOW (chooser), "applets-screenshooter");
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser), 
                                       current_default_dir);
  
  dialog_response = gtk_dialog_run(GTK_DIALOG (chooser));

  if (dialog_response == GTK_RESPONSE_ACCEPT)
    {
      dir = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
      
      rc = xfce_rc_simple_open (rc_file, FALSE);
      xfce_rc_write_entry (rc, "screenshot_dir", dir);
      xfce_rc_close (rc);
  
      g_free (dir);
    }
  gtk_widget_destroy (GTK_WIDGET (chooser));
}
