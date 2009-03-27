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

#include "screenshooter-dialogs.h"

#define ICON_SIZE 16

/* Prototypes */ 

static void 
cb_fullscreen_screen_toggled       (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void 
cb_active_window_toggled           (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void 
cb_rectangle_toggled               (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void
cb_show_mouse_toggled              (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);                                                                       
static void 
cb_save_toggled                    (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void 
cb_toggle_set_sensi                (GtkToggleButton    *tb, 
                                    GtkWidget          *widget);
static void 
cb_toggle_set_insensi              (GtkToggleButton    *tb, 
                                    GtkWidget          *widget);
static void 
cb_open_toggled                    (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void 
cb_clipboard_toggled               (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void 
cb_show_save_dialog_toggled        (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void 
cb_close_toggled                   (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);                                  
static void 
cb_default_folder                  (GtkWidget          *chooser, 
                                    ScreenshotData     *sd);                                        
static void 
cb_delay_spinner_changed           (GtkWidget          *spinner, 
                                    ScreenshotData     *sd);
static gchar 
*generate_filename_for_uri         (char               *uri);
static void 
cb_combo_active_item_changed       (GtkWidget          *box, 
                                    ScreenshotData     *sd);
static void 
add_item                           (GAppInfo           *app_info, 
                                    GtkWidget          *liststore);
static void 
populate_liststore                 (GtkListStore       *liststore);
static void 
set_default_item                   (GtkWidget          *combobox, 
                                    ScreenshotData     *sd);
static void
cb_progress_upload                 (goffset             current_num_bytes,
                                    goffset             total_num_bytes,
                                    gpointer            user_data);
static void
cb_finished_upload                 (GObject            *source_object,
                                    GAsyncResult       *res,
                                    gpointer            user_data);
static void
cb_transfer_dialog_response        (GtkWidget          *dialog,
                                    int                 response,
                                    GCancellable       *cancellable);
static gchar
*save_screenshot_to_local_path     (GdkPixbuf          *screenshot,
                                    GFile              *save_file);
static void
save_screenshot_to_remote_location (GdkPixbuf          *screenshot,
                                    GFile              *save_file);
static gchar
*save_screenshot_to                (GdkPixbuf          *screenshot,
                                    gchar *save_uri);                                                               



/* Internals */



/* Set the captured area when the button is toggled */
static void cb_fullscreen_screen_toggled (GtkToggleButton *tb,
                                          ScreenshotData   *sd)
{
  if (gtk_toggle_button_get_active (tb))
    {
      sd->region = FULLSCREEN;
    }
}



/* Set the captured area when the button is toggled */
static void cb_active_window_toggled (GtkToggleButton *tb,
                                      ScreenshotData   *sd)
{
  if (gtk_toggle_button_get_active (tb))
    {
      sd->region = ACTIVE_WINDOW;
    }
}



/* Set the captured when the button is toggled */
static void cb_rectangle_toggled (GtkToggleButton *tb,
                                  ScreenshotData   *sd)
{
  if (gtk_toggle_button_get_active (tb))
    {
      sd->region = SELECT;
    }
}



/* Set whether the mouse should be captured when the button is toggled */
static void cb_show_mouse_toggled (GtkToggleButton *tb, ScreenshotData   *sd)
{
  if (gtk_toggle_button_get_active (tb))
    {
      sd->show_mouse = 1;
    }
  else
    {
      sd->show_mouse = 0;
    }
}



/* Set the action when the button is toggled */
static void cb_save_toggled (GtkToggleButton *tb, ScreenshotData  *sd)
{
  if (gtk_toggle_button_get_active (tb))
    {
      sd->action = SAVE;
    }
}



/* Set the widget active if the toggle button is active */
static void 
cb_toggle_set_sensi (GtkToggleButton *tb, GtkWidget *widget)
{
  gtk_widget_set_sensitive (widget, gtk_toggle_button_get_active (tb));
}



/* Set the widget active if the toggle button is inactive */
static void 
cb_toggle_set_insensi (GtkToggleButton *tb, GtkWidget *widget)
{
  gtk_widget_set_sensitive (widget, !gtk_toggle_button_get_active (tb));
}



/* Set the action when the button is toggled */
static void cb_open_toggled (GtkToggleButton *tb, ScreenshotData  *sd)
{
  if (gtk_toggle_button_get_active (tb))
    {
      sd->action = OPEN;
    }
}



/* Set the action when the button is toggled */
static void cb_clipboard_toggled (GtkToggleButton *tb, 
                                  ScreenshotData  *sd)
{
  if (gtk_toggle_button_get_active (tb))
    {
      sd->action = CLIPBOARD;
    }
}



/* Set sd->show_save_dialog when the button is toggled */
static void cb_show_save_dialog_toggled (GtkToggleButton *tb, ScreenshotData   *sd)
{
  if (gtk_toggle_button_get_active (tb))
    {
      sd->show_save_dialog = 1;
    }
  else
    {
      sd->show_save_dialog = 0;
    }
}



/* Set sd->close when the button is toggled */
static void cb_close_toggled  (GtkToggleButton *tb, ScreenshotData   *sd)
{
  if (gtk_toggle_button_get_active (tb))
    {
      sd->close = 1;
    }
  else
    {
      sd->close = 0;
    }
}                                 



/* Set sd->screenshot_dir when the user changed the value in the file chooser */
static void cb_default_folder (GtkWidget       *chooser, 
                               ScreenshotData  *sd)
{
  g_free (sd->screenshot_dir);
  
  sd->screenshot_dir = 
    gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (chooser));
}

   

/* Set the delay according to the spinner */
static void cb_delay_spinner_changed (GtkWidget       *spinner, 
                                      ScreenshotData  *sd)
{
  sd->delay = 
    gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spinner));
}



/* Generates filename Screenshot-n.png (where n is the first integer 
 * greater than 0) so that Screenshot-n.jpg does not exist in the folder
 * whose URI is *uri. 
 * @uri: uri of the folder for which the filename should be generated.
 * returns: the filename or NULL if *uri == NULL.
*/
static gchar *generate_filename_for_uri (char *uri)
{
  gboolean exists = TRUE;
  GFile *directory;
  GFile *file;
  gchar *basename;
  gint i;

  if (uri == NULL)
    {
      TRACE ("URI was NULL");

      return NULL;
    }

  TRACE ("Get the folder corresponding to the URI");

  directory = g_file_new_for_uri (uri);

  basename = g_strdup (_("Screenshot.png"));

  file = g_file_get_child (directory, basename);

  if (!g_file_query_exists (file, NULL))
    {
      g_object_unref (file);
      g_object_unref (directory);
      
      return basename;
    }

  g_object_unref (file);
  g_free (basename);

  for (i = 1; exists; ++i)
    {
      basename = g_strdup_printf (_("Screenshot-%d.png"), i);

      file = g_file_get_child (directory, basename);
      
      if (!g_file_query_exists (file, NULL))
        exists = FALSE;

      if (exists)
        g_free (basename);

      g_object_unref (file);
    }

  g_object_unref (directory);
  
  return basename;
}



/* Set sd->app as per the active item in the combobox */
static void cb_combo_active_item_changed (GtkWidget *box, 
                                          ScreenshotData *sd)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (box));
  GtkTreeIter iter;
  gchar *active_command = NULL;
   
  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (box), &iter);
  
  gtk_tree_model_get (model, &iter, 2, &active_command, -1);
  
  g_free (sd->app);
  sd->app = active_command;
}



/* Extract the informations from app_info and add them to the 
 * liststore. 
 * */
static void add_item (GAppInfo *app_info, GtkWidget *liststore)
{
  GtkTreeIter iter;
  gchar *command = g_strdup (g_app_info_get_executable (app_info));
  gchar *name = g_strdup (g_app_info_get_name (app_info));
  GIcon *icon = g_app_info_get_icon (app_info);
  GdkPixbuf *pixbuf = NULL;
  GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
  
  /* Get the icon */
  if (G_IS_LOADABLE_ICON (icon))
    {
      GFile *file = g_file_icon_get_file (G_FILE_ICON (icon));
      gchar *path = g_file_get_path (file);
      
      pixbuf = 
        gdk_pixbuf_new_from_file_at_size (path, ICON_SIZE, 
                                          ICON_SIZE, NULL);
      
      g_free (path);
      g_object_unref (file);
    }
  else
    {
      gchar **names = NULL;
      
      g_object_get (G_OBJECT (icon), "names", &names, NULL);
           
      if (names != NULL)
        {
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
  
  if (pixbuf == NULL)
    {
      pixbuf = 
        gtk_icon_theme_load_icon (icon_theme, "exec", ICON_SIZE,
                                  GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                  NULL);
    }
  
  /* Add to the liststore */
  gtk_list_store_append (GTK_LIST_STORE (liststore), &iter);
          
  gtk_list_store_set (GTK_LIST_STORE (liststore), &iter, 0, pixbuf, 1, 
                      name, 2, command, -1);
  
  /* Free the stuff */      
  g_free (command);
  g_free (name);
  if (pixbuf != NULL)
    g_object_unref (pixbuf);
  g_object_unref (icon);
}



/* Populate the liststore using the applications which can open image/png. */
static void populate_liststore (GtkListStore *liststore)
{
  const gchar *content_type;
  GList	*list_app;
     
  content_type = "image/png";
  
  /* Get all applications for image/png.*/
  list_app = g_app_info_get_all_for_type (content_type);
  
  /* Add them to the liststore */
  if (list_app != NULL)
    {
      g_list_foreach (list_app, (GFunc) add_item, liststore);
            
      g_list_free (list_app);
    }
}



/* Select the sd->app item in the combobox */
static void set_default_item (GtkWidget       *combobox, 
                              ScreenshotData  *sd)
{
  GtkTreeModel *model = 
    gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));
  
  GtkTreeIter iter; 
    
  /* Get the first iter */
  if (gtk_tree_model_get_iter_first (model , &iter))
    {     
      gchar *command = NULL;
      gboolean found = FALSE;

      /* Loop until finding the appropirate item, if any */
      do
        {
          gtk_tree_model_get (model, &iter, 2, &command, -1);
          
          if (g_str_equal (command, sd->app))
            {
              gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), 
                                             &iter);
              
              found = TRUE;
            }
          
          g_free (command);      
        }
      while (gtk_tree_model_iter_next (model, &iter));
      
      /* If no suitable item was found, set the first item as active and
       * set sd->app accordingly. */
      if (!found)
        {
          gtk_tree_model_get_iter_first (model , &iter);
          gtk_tree_model_get (model, &iter, 2, &command, -1);
          
          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);
          
          g_free (sd->app);
          
          sd->app = command;
        }
    }
  else
    {
      g_free (sd->app);

      sd->app = g_strdup ("none");
    }
}



static void cb_progress_upload (goffset current_num_bytes,
                                goffset total_num_bytes,
                                gpointer user_data)
{
  gdouble fraction = (double) current_num_bytes / (double) total_num_bytes;

  gint remaining = (int) total_num_bytes - (int) current_num_bytes;

  gchar *bar_text = g_strdup_printf (_("%i bytes remaining"), remaining);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (user_data), fraction);

  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (user_data), bar_text);

  g_free (bar_text);
}



static void
cb_finished_upload (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  gboolean success;

  g_return_if_fail (G_LIKELY (G_IS_FILE (source_object)));

  success = g_file_copy_finish (G_FILE (source_object), res, &error);

  TRACE ("The transfer is finished");

  if (!success)
    {
      TRACE ("An error occurred");

      xfce_err (error->message);

      g_error_free (error);
    }

  gtk_widget_destroy (GTK_WIDGET (user_data));
}



static void
cb_transfer_dialog_response (GtkWidget *dialog, int response, GCancellable *cancellable)
{
  if (G_LIKELY (response == GTK_RESPONSE_CANCEL))
    {
      TRACE ("Cancel the screenshot");

      g_cancellable_cancel (cancellable);

      gtk_widget_destroy (dialog);
    }
}
 


static gchar
*save_screenshot_to_local_path (GdkPixbuf *screenshot, GFile *save_file)
{
  GError *error = NULL;
  gchar *save_path = g_file_get_path (save_file);

  if (!gdk_pixbuf_save (screenshot, save_path, "png", &error, NULL))
    {
      xfce_err ("%s", error->message);
      
      g_error_free (error);

      g_free (save_path);
      
      return NULL;
    }
  else
    {
      return save_path;
    }
}

static void
save_screenshot_to_remote_location (GdkPixbuf *screenshot, GFile *save_file)
{
  gchar *save_basename = g_file_get_basename (save_file);
  gchar *save_path = g_build_filename (g_get_tmp_dir (), save_basename, NULL);
  GFile *save_file_temp = g_file_new_for_path (save_path);

  GFile *save_parent = g_file_get_parent (save_file);
  const gchar *parent_uri = g_file_get_uri (save_parent);

  GCancellable *cancellable = g_cancellable_new ();
  
  GtkWidget *dialog = gtk_dialog_new_with_buttons (_("Transfer"),
                                                   NULL,
                                                   GTK_DIALOG_NO_SEPARATOR,
                                                   GTK_STOCK_CANCEL,
                                                   GTK_RESPONSE_CANCEL,
                                                   NULL);

  GtkWidget *progress_bar = gtk_progress_bar_new ();
  
  GtkWidget *label1= gtk_label_new ("");

  GtkWidget *label2 = gtk_label_new (parent_uri);

  save_screenshot_to_local_path (screenshot, save_file_temp);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_deletable (GTK_WINDOW (dialog), FALSE);
  
  gtk_container_set_border_width (GTK_CONTAINER (dialog), 20);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "document-save");

  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG(dialog)->vbox), 12);

  gtk_label_set_markup (GTK_LABEL (label1),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">The screenshot "
                          "is being transferred to:</span>"));

  gtk_misc_set_alignment (GTK_MISC (label1), 0, 0.5);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->vbox),
                      label1,
                      FALSE,
                      FALSE,
                      0);

  gtk_widget_show (label1);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->vbox),
                      label2,
                      FALSE,
                      FALSE,
                      0);

  gtk_widget_show (label2);

  gtk_box_pack_start (GTK_BOX (GTK_DIALOG(dialog)->vbox),
                      progress_bar,
                      FALSE,
                      FALSE,
                      0);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar), 0);

  gtk_widget_show (progress_bar);

  g_signal_connect (dialog,
                    "response",
                    G_CALLBACK (cb_transfer_dialog_response),
                    cancellable);

  g_file_copy_async (save_file_temp,
                     save_file,
                     G_FILE_COPY_OVERWRITE,
                     G_PRIORITY_DEFAULT,
                     cancellable,
                     (GFileProgressCallback)cb_progress_upload, progress_bar,
                     (GAsyncReadyCallback)cb_finished_upload, dialog);

  gtk_dialog_run (GTK_DIALOG (dialog));

  g_file_delete (save_file_temp, NULL, NULL);
                     
  g_object_unref (save_file_temp);
  g_object_unref (save_parent);
  g_object_unref (cancellable);
  g_free (save_basename);
  g_free (save_path);
}

static gchar
*save_screenshot_to (GdkPixbuf *screenshot, gchar *save_uri)
{
  GFile *save_file = g_file_new_for_uri (save_uri);

  gchar *result = NULL;
    
  /* If the URI is a local one, we save directly */

  if (!screenshooter_is_remote_uri (save_uri))
    {
      result = save_screenshot_to_local_path (screenshot, save_file);
    }
  else
    {
      save_screenshot_to_remote_location (screenshot, save_file);
    }
  
  g_object_unref (save_file);

  return result;
}



/* Public */



/* Build the preferences dialog.
@sd: a ScreenshotData to set the options.
*/
GtkWidget *screenshooter_dialog_new (ScreenshotData  *sd, 
                                     gboolean plugin)
{
  GtkWidget *dlg, *main_alignment;
  GtkWidget *vbox;

  GtkWidget *capture_table, *actions_table; 
  
  GtkWidget *area_main_box, *area_box, *area_label, *area_alignment;
  GtkWidget *active_window_button, 
            *fullscreen_button,
            *rectangle_button;

  GtkWidget *show_mouse_checkbox;
  
  GtkWidget *delay_main_box, *delay_box, *delay_label, *delay_alignment;
  GtkWidget *delay_spinner_box, *delay_spinner, *seconds_label;

  GtkWidget *options_main_box, *options_label, *options_alignment, *options_box;
  GtkWidget *save_checkbox;
  
  GtkWidget *actions_main_box, *actions_label, *actions_alignment;
  
  GtkWidget *save_radio_button, *dir_chooser;
    
  GtkWidget *clipboard_radio_button;
    
  GtkWidget *open_with_radio_button;
  
  GtkListStore *liststore;
  GtkWidget *combobox;
  GtkCellRenderer *renderer, *renderer_pixbuf;
  
  /* Create the dialog */
  if (!plugin)
    {
      dlg = 
        xfce_titled_dialog_new_with_buttons (_("Screenshot"),
                                             NULL,
                                             GTK_DIALOG_DESTROY_WITH_PARENT|
                                             GTK_DIALOG_NO_SEPARATOR,
                                             GTK_STOCK_HELP,
                                             GTK_RESPONSE_HELP,
                                             GTK_STOCK_CANCEL, 
                                             GTK_RESPONSE_CANCEL,
                                             GTK_STOCK_OK, 
                                             GTK_RESPONSE_OK,
                                             NULL);

      xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dlg), 
                                       _("Take a screenshot"));
    }
  else
    {
      dlg =
        xfce_titled_dialog_new_with_buttons (_("Screenshot"),
                                             NULL,
                                             GTK_DIALOG_DESTROY_WITH_PARENT|GTK_DIALOG_NO_SEPARATOR,
                                             GTK_STOCK_HELP,
                                             GTK_RESPONSE_HELP,
                                             GTK_STOCK_CLOSE, 
                                             GTK_RESPONSE_OK,
                                             NULL);
      
      xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dlg), 
                                       _("Preferences"));
    }                                             

  gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable (GTK_WINDOW (dlg), FALSE);
  
  gtk_container_set_border_width (GTK_CONTAINER (dlg), 0);
  gtk_window_set_icon_name (GTK_WINDOW (dlg), "applets-screenshooter");

  /* Create the main alignment for the dialog */

  main_alignment = gtk_alignment_new (0, 0, 1, 1);

  gtk_alignment_set_padding (GTK_ALIGNMENT (main_alignment), 6, 0, 12, 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), main_alignment,
                      TRUE, TRUE, 0);

  gtk_widget_show (main_alignment);
  
  /* Create the main box for the dialog */
  
	vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);

  gtk_container_add (GTK_CONTAINER (main_alignment), vbox);
  gtk_widget_show (vbox);
  

  /* Create the table to align the differents parts of the top of the UI */

  capture_table = gtk_table_new (2, 2, FALSE);

  gtk_table_set_col_spacings (GTK_TABLE (capture_table), 20);

  gtk_box_pack_start (GTK_BOX (vbox), capture_table, TRUE, TRUE, 0);
  gtk_widget_show (capture_table);

  /* Create the main box for the regions */
  
	area_main_box = gtk_vbox_new (FALSE, 6);
  gtk_widget_show (area_main_box);
  gtk_table_attach_defaults (GTK_TABLE (capture_table), area_main_box, 0, 1, 0, 2);
  
  /* Create area label */
  area_label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (area_label),
  _("<span weight=\"bold\" stretch=\"semiexpanded\">"
    "Region to capture</span>"));
			
  gtk_misc_set_alignment (GTK_MISC (area_label), 0, 0);
  gtk_widget_show (area_label);
  gtk_container_add (GTK_CONTAINER (area_main_box), area_label);
  
  /* Create area alignment */
  
  area_alignment = gtk_alignment_new (0, 0, 1, 1);
  
  gtk_container_add (GTK_CONTAINER (area_main_box), area_alignment);
  
  gtk_alignment_set_padding (GTK_ALIGNMENT (area_alignment), 0, 6, 12, 0);
  
  gtk_widget_show (area_alignment);
  
  /* Create area box */      
  area_box = gtk_vbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (area_alignment), area_box);
  gtk_container_set_border_width (GTK_CONTAINER (area_box), 0);
  gtk_widget_show (area_box);
    
  /* Create radio buttons for areas to screenshot */
  
  /* Fullscreen */
  
  fullscreen_button = 
    gtk_radio_button_new_with_mnemonic (NULL, 
                                        _("Entire screen"));
                                        
  gtk_box_pack_start (GTK_BOX (area_box), 
                      fullscreen_button, FALSE, 
                      FALSE, 0);
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (fullscreen_button),
                                (sd->region == FULLSCREEN));
  
  gtk_widget_set_tooltip_text (fullscreen_button,
                          _("Take a screenshot of the entire screen"));
                                
  g_signal_connect (G_OBJECT (fullscreen_button), "toggled", 
                    G_CALLBACK (cb_fullscreen_screen_toggled),
                    sd);
                    
  gtk_widget_show (fullscreen_button);
  
  /* Active window */
  
  active_window_button = 
    gtk_radio_button_new_with_mnemonic (
      gtk_radio_button_get_group (GTK_RADIO_BUTTON (fullscreen_button)), 
      _("Active window"));
                                        
  gtk_box_pack_start (GTK_BOX (area_box), 
                      active_window_button, FALSE, 
                      FALSE, 0);
  
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active_window_button),
                                (sd->region == ACTIVE_WINDOW));
  
  gtk_widget_set_tooltip_text (active_window_button,
                          _("Take a screenshot of the active window"));
                                
  g_signal_connect (G_OBJECT (active_window_button), "toggled", 
                    G_CALLBACK (cb_active_window_toggled),
                    sd);
                    
  gtk_widget_show (active_window_button);
  
  /* Rectangle */
  
  rectangle_button = 
    gtk_radio_button_new_with_mnemonic (
      gtk_radio_button_get_group (GTK_RADIO_BUTTON (fullscreen_button)), 
                                  _("Select a region"));

   gtk_box_pack_start (GTK_BOX (area_box), 
                       rectangle_button, FALSE, 
                       FALSE, 0);
                       
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rectangle_button),
                                (sd->region == SELECT));
  
  gtk_widget_set_tooltip_text (rectangle_button,
  _("Select a region to be captured by clicking a point of the screen "
    "without releasing the mouse button, dragging your mouse to the "
    "other corner of the region, and releasing the mouse button."));
  
  g_signal_connect (G_OBJECT (rectangle_button), "toggled", 
                    G_CALLBACK (cb_rectangle_toggled),
                    sd);
  
  gtk_widget_show (rectangle_button);

  /* Create show mouse checkbox */

  show_mouse_checkbox = 
    gtk_check_button_new_with_label (_("Capture the mouse pointer"));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (show_mouse_checkbox),
                                (sd->show_mouse == 1));

  gtk_widget_set_sensitive (show_mouse_checkbox, (sd->region != SELECT));

  gtk_box_pack_start (GTK_BOX (area_box), 
                      show_mouse_checkbox, FALSE, 
                      FALSE, 5);

  gtk_widget_show (show_mouse_checkbox);
   
  g_signal_connect (G_OBJECT (show_mouse_checkbox), "toggled", 
                    G_CALLBACK (cb_show_mouse_toggled), sd);

  g_signal_connect (G_OBJECT (rectangle_button), "toggled",
                    G_CALLBACK (cb_toggle_set_insensi), show_mouse_checkbox);

  /* Create the main box for the delay stuff */
  
	delay_main_box = gtk_vbox_new (FALSE, 6);
  gtk_widget_show (delay_main_box);

  gtk_table_attach_defaults (GTK_TABLE (capture_table), delay_main_box, 1, 2, 0, 1);
    
  /* Create delay label */
  
  delay_label = gtk_label_new ("");
  
  gtk_label_set_markup (GTK_LABEL(delay_label),
  _("<span weight=\"bold\" stretch=\"semiexpanded\">"
    "Delay before capturing</span>"));
  
	gtk_misc_set_alignment(GTK_MISC (delay_label), 0, 0); 
  gtk_widget_show (delay_label);
  gtk_box_pack_start (GTK_BOX (delay_main_box), delay_label, FALSE, FALSE, 0);
  
  /* Create delay alignment */
  
  delay_alignment = gtk_alignment_new (0, 0, 0, 0);

  gtk_box_pack_start (GTK_BOX (delay_main_box), delay_alignment, FALSE, FALSE, 0);
  
  gtk_alignment_set_padding (GTK_ALIGNMENT (delay_alignment), 0, 6, 12, 0);
  
  gtk_widget_show (delay_alignment);
  
  /* Create delay box */
  
  delay_box = gtk_vbox_new (FALSE, 0);
  gtk_container_add (GTK_CONTAINER (delay_alignment), delay_box);
  gtk_container_set_border_width (GTK_CONTAINER (delay_box), 0);
  gtk_widget_show (delay_box);
  
  /* Create delay spinner */
      
  delay_spinner_box = gtk_hbox_new (FALSE, 4);
  gtk_widget_show (delay_spinner_box);
  
  gtk_box_pack_start (GTK_BOX (delay_box), delay_spinner_box, FALSE, FALSE, 0);

  delay_spinner = gtk_spin_button_new_with_range(0.0, 60.0, 1.0);
  
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (delay_spinner), 
                             sd->delay);
  
  gtk_widget_set_tooltip_text (delay_spinner,
                _("Delay in seconds before the screenshot is taken"));  
                             
  gtk_widget_show (delay_spinner);
  
  gtk_box_pack_start (GTK_BOX (delay_spinner_box), delay_spinner, FALSE, FALSE, 0);

  seconds_label = gtk_label_new (_("seconds"));
  gtk_widget_show (seconds_label);
  
  gtk_box_pack_start (GTK_BOX (delay_spinner_box), seconds_label, FALSE, FALSE, 0);

  g_signal_connect (G_OBJECT (delay_spinner), "value-changed",
                    G_CALLBACK (cb_delay_spinner_changed), sd);
  
  /* Set the delay box as inactive when we capture rectangles */                  
  g_signal_connect (G_OBJECT (rectangle_button), "toggled",
                    G_CALLBACK (cb_toggle_set_insensi), delay_box);
  
  /* Set the default state */
  cb_toggle_set_insensi (GTK_TOGGLE_BUTTON (rectangle_button),
                         delay_box);

  /* Create the options box */

  options_main_box = gtk_vbox_new (FALSE, 6);

  gtk_widget_show (options_main_box);

  gtk_table_attach_defaults (GTK_TABLE (capture_table), options_main_box, 1, 2, 1, 2);

  /* Create the options label */

  options_label = gtk_label_new ("");

  gtk_label_set_markup (GTK_LABEL(options_label),
  _("<span weight=\"bold\" stretch=\"semiexpanded\">After capturing</span>"));
  
	gtk_misc_set_alignment(GTK_MISC (options_label), 0, 0); 
  gtk_widget_show (options_label);
  gtk_box_pack_start (GTK_BOX (options_main_box), options_label, FALSE, FALSE, 0);

  /* Create options alignment */
  
  options_alignment = gtk_alignment_new (0, 0, 1, 1);

  gtk_container_add (GTK_CONTAINER (options_main_box), options_alignment);

  gtk_alignment_set_padding (GTK_ALIGNMENT (options_alignment), 0, 6, 12, 0);
                             
  gtk_widget_show (options_alignment);

  /* Create the options box to be stored in the options alignment*/

  options_box = gtk_vbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (options_alignment), options_box);
  gtk_container_set_border_width (GTK_CONTAINER (options_box), 0);
  gtk_widget_show (options_box);

  /* Display the save dialog checkbox */          
              
  save_checkbox = 
    gtk_check_button_new_with_label (_("Show the save dialog"));

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (save_checkbox),
                                (sd->show_save_dialog == 1));

  gtk_widget_set_tooltip_text (save_checkbox,
  _("The save dialog allows you to change the file name and the save"
    " location"));

  gtk_box_pack_start (GTK_BOX (options_box), save_checkbox, FALSE, FALSE, 0);
        
  gtk_widget_show (save_checkbox);
   
  g_signal_connect (G_OBJECT (save_checkbox), "toggled", 
                    G_CALLBACK (cb_show_save_dialog_toggled), sd);

  /* Create the 'close the user interface after taking the screenshot' checkbox */
  if (!plugin)
    {
      GtkWidget *close_checkbox = 
        gtk_check_button_new_with_label (_("Close the application"));

      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (close_checkbox),
                                    (sd->close == 1));

      gtk_box_pack_start (GTK_BOX (options_box), close_checkbox, FALSE, FALSE, 0);

      gtk_widget_show (close_checkbox);

      g_signal_connect (G_OBJECT (close_checkbox), "toggled", 
                        G_CALLBACK (cb_close_toggled), sd);
    }

  /* Create the actions main box */

  actions_main_box = gtk_vbox_new (FALSE, 6);

  gtk_box_pack_start (GTK_BOX (vbox), actions_main_box, TRUE, TRUE, 0);
  gtk_widget_show (actions_main_box);

  /* Create actions label */
  
  actions_label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (actions_label),
  _("<span weight=\"bold\" stretch=\"semiexpanded\">Action</span>"));
			
  gtk_misc_set_alignment (GTK_MISC (actions_label), 0, 0);
  gtk_widget_show (actions_label);
  gtk_box_pack_start (GTK_BOX (actions_main_box), actions_label, FALSE, FALSE, 0);

  /* Create actions alignment */

  actions_alignment = gtk_alignment_new (0, 0, 1, 1);

  gtk_alignment_set_padding (GTK_ALIGNMENT (actions_alignment), 0, 6, 12, 0);

  gtk_box_pack_start (GTK_BOX (actions_main_box), actions_alignment, TRUE, TRUE, 0);
  gtk_widget_show (actions_alignment);

  /* Create the actions table */

  actions_table = gtk_table_new (3, 2, FALSE);

  gtk_table_set_row_spacings (GTK_TABLE (actions_table), 3);
  gtk_table_set_col_spacings (GTK_TABLE (actions_table), 15);

  gtk_container_add (GTK_CONTAINER (actions_alignment), actions_table);
  gtk_widget_show (actions_table);
  
  /* Save option radio button */
  
  save_radio_button = 
    gtk_radio_button_new_with_mnemonic (NULL, 
                                        _("Save in:"));
                           
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (save_radio_button),
                                (sd->action == SAVE));
  
  g_signal_connect (G_OBJECT (save_radio_button), "toggled", 
                    G_CALLBACK (cb_save_toggled),
                    sd);
  
  gtk_widget_set_tooltip_text (save_radio_button,
                               _("Save the screenshot to a PNG file"));
  
  gtk_widget_show (save_radio_button);

  gtk_table_attach_defaults (GTK_TABLE (actions_table), save_radio_button, 0, 1, 0, 1);

  /* Set the state of the save checkbox */

  gtk_widget_set_sensitive (GTK_WIDGET (save_checkbox),
                            gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (save_radio_button)));

  g_signal_connect (G_OBJECT (save_radio_button), "toggled",
                    G_CALLBACK (cb_toggle_set_sensi), save_checkbox);

  /* Directory chooser */

  dir_chooser = 
    gtk_file_chooser_button_new (_("Default save location"), 
                                 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
        
  gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (dir_chooser), 
                                           sd->screenshot_dir);

  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (dir_chooser), FALSE);
    
  gtk_widget_show (dir_chooser);
  
  gtk_widget_set_tooltip_text (dir_chooser, _("Set the default save location"));

  g_signal_connect (G_OBJECT (dir_chooser), "selection-changed", 
                    G_CALLBACK (cb_default_folder), sd);
  
  g_signal_connect (G_OBJECT (save_radio_button), "toggled",
                    G_CALLBACK (cb_toggle_set_sensi), dir_chooser);

  gtk_table_attach_defaults (GTK_TABLE (actions_table), dir_chooser, 1, 2, 0, 1);

  /* Copy to clipboard radio button */
  
  clipboard_radio_button = 
    gtk_radio_button_new_with_mnemonic (
      gtk_radio_button_get_group (GTK_RADIO_BUTTON (save_radio_button)), 
      _("Copy to the clipboard"));
  
  gtk_widget_show (clipboard_radio_button);
  
  gtk_widget_set_tooltip_text (clipboard_radio_button,
  _("Copy the screenshot to the clipboard so that it can be "
    "pasted later"));
                      
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (clipboard_radio_button),
                                (sd->action == CLIPBOARD));
  
  g_signal_connect (G_OBJECT (clipboard_radio_button), "toggled", 
                    G_CALLBACK (cb_clipboard_toggled),
                    sd);

  gtk_table_attach_defaults (GTK_TABLE (actions_table), clipboard_radio_button,
                             0, 1, 1, 2);

  /* Open with radio button */
  
  open_with_radio_button = 
    gtk_radio_button_new_with_mnemonic (
      gtk_radio_button_get_group (GTK_RADIO_BUTTON (save_radio_button)), 
      _("Open with:"));
     
  gtk_widget_show (open_with_radio_button);
                      
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (open_with_radio_button),
                                (sd->action == OPEN));
  
  g_signal_connect (G_OBJECT (open_with_radio_button), "toggled", 
                    G_CALLBACK (cb_open_toggled),
                    sd);
   
  gtk_widget_set_tooltip_text (open_with_radio_button,
  _("Open the screenshot with the chosen application"));

  gtk_table_attach_defaults (GTK_TABLE (actions_table), open_with_radio_button,
                             0, 1, 2, 3);                    
  
  /* Open with combobox */
    
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
                                  
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), 
                                  renderer_pixbuf,
                                  "pixbuf", 0, NULL);                                  
  
  populate_liststore (liststore);
    
  set_default_item (combobox, sd);
  
  gtk_table_attach_defaults (GTK_TABLE (actions_table), combobox, 1, 2, 2, 3);
  
  g_signal_connect (G_OBJECT (combobox), "changed", 
                    G_CALLBACK (cb_combo_active_item_changed), sd);
  
  gtk_widget_show_all (combobox); 
  
  gtk_widget_set_tooltip_text (combobox,
  _("Application to open the screenshot"));

  g_signal_connect (G_OBJECT (open_with_radio_button), "toggled",
                    G_CALLBACK (cb_toggle_set_sensi), combobox);
  
  /* Run the callback functions to grey/ungrey the correct widgets */
  
  cb_toggle_set_sensi (GTK_TOGGLE_BUTTON (open_with_radio_button),
                       combobox);
 
  return dlg;                
}



/* Saves the screenshot according to the options in sd. 
 * @screenshot: a GdkPixbuf containing our screenshot
 * show_save_dialog: whether the save dialog should be shown.
 * @default_dir: the default save location.
 */
gchar 
*screenshooter_save_screenshot     (GdkPixbuf      *screenshot, 
                                    gboolean        show_save_dialog,
                                    gchar          *default_dir)
{
  gchar *filename = NULL, *savename = NULL;
  
  /* Generate the filename for the default save location */
 
  filename = generate_filename_for_uri (default_dir);
    
  if (show_save_dialog)
	  {
      GdkPixbuf *thumbnail;

      GtkWidget *preview;
      GtkWidget *chooser;
      gchar *save_uri = NULL;
      gint dialog_response;

      /* If the user wants a save dialog, we run it, and grab the 
       * filename the user has chosen. */
	    
      /* Create the dialog and set its default properties */
      chooser = 
        gtk_file_chooser_dialog_new (_("Save screenshot as..."),
                                     NULL,
                                     GTK_FILE_CHOOSER_ACTION_SAVE,
                                     GTK_STOCK_CANCEL, 
                                     GTK_RESPONSE_CANCEL,
                                     GTK_STOCK_SAVE, 
                                     GTK_RESPONSE_ACCEPT,
                                     NULL);
                                     
      gtk_window_set_icon_name (GTK_WINDOW (chooser), 
                                "applets-screenshooter");
                                
      gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (chooser), TRUE);

      gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), FALSE);
      
      gtk_dialog_set_default_response (GTK_DIALOG (chooser), 
                                       GTK_RESPONSE_ACCEPT);

      gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (chooser), 
                                               default_dir);

      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser), 
                                         filename);

      /* Create the preview and the thumbnail */
      
      preview = gtk_image_new ();
                                          
      gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (chooser), 
                                           preview);
  
      thumbnail = 
        gdk_pixbuf_scale_simple (screenshot, 
                                 gdk_pixbuf_get_width(screenshot)/5, 
                                 gdk_pixbuf_get_height(screenshot)/5, 
                                 GDK_INTERP_BILINEAR);
      
      gtk_image_set_from_pixbuf (GTK_IMAGE (preview), thumbnail);
      
      g_object_unref (thumbnail);
          
      dialog_response = gtk_dialog_run (GTK_DIALOG (chooser));
	    
      /* The user pressed the save button */
	    if (dialog_response == GTK_RESPONSE_ACCEPT)
	      {
          save_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (chooser));
        }
	  
	    gtk_widget_destroy (chooser);

      if (save_uri != NULL)
        {
          savename = save_screenshot_to (screenshot, save_uri);

          g_free (save_uri);
        }
	  }  
	else
	  {    
	    /* Else, we just save the file in the default folder */
      gchar *save_uri = g_build_filename (default_dir, filename, NULL);
      
      savename = save_screenshot_to (screenshot, save_uri);

      g_free (save_uri);
    }

  TRACE ("Free the gchars and unref the GFiles");

  g_free (filename);
  
  return savename;
}
