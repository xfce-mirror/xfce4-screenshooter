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
 */

#include "screenshooter-dialogs.h"

#define ICON_SIZE 16
#define THUMB_X_SIZE 200
#define THUMB_Y_SIZE 125

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
cb_radiobutton_activate            (GtkToggleButton    *tb,
                                    GtkWidget          *widget);
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
cb_open_toggled                    (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void
cb_clipboard_toggled               (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void
cb_imgur_toggled                   (GtkToggleButton    *tb,
                                    ScreenshotData     *sd);
static void
cb_delay_spinner_changed           (GtkWidget          *spinner,
                                    ScreenshotData     *sd);
static gchar
*generate_filename_for_uri         (const gchar        *uri,
                                    const gchar        *title,
                                    gboolean            timestamp);
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
static GdkPixbuf
*screenshot_get_thumbnail          (GdkPixbuf          *screenshot);
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



/* Internals */



/* Set the captured area when the button is toggled */
static void cb_fullscreen_screen_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->region = FULLSCREEN;
}



/* Set the captured area when the button is toggled */
static void cb_active_window_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->region = ACTIVE_WINDOW;
}



/* Set the captured when the button is toggled */
static void cb_rectangle_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->region = SELECT;
}



/* Confirm the selected screenshot options and proceed to capture */
static void cb_radiobutton_activate (GtkToggleButton *tb, GtkWidget *widget)
{
  gtk_dialog_response (GTK_DIALOG (widget), GTK_RESPONSE_OK);
}



/* Set whether the mouse should be captured when the button is toggled */
static void cb_show_mouse_toggled (GtkToggleButton *tb, ScreenshotData   *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->show_mouse = 1;
  else
    sd->show_mouse = 0;
}



/* Set the action when the button is toggled */
static void cb_save_toggled (GtkToggleButton *tb, ScreenshotData  *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->action = SAVE;
}



/* Set the widget active if the toggle button is active */
static void
cb_toggle_set_sensi (GtkToggleButton *tb, GtkWidget *widget)
{
  gtk_widget_set_sensitive (widget, gtk_toggle_button_get_active (tb));
}



static void cb_open_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->action = OPEN;
}



static void cb_clipboard_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->action = CLIPBOARD;
}



static void cb_imgur_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
  if (gtk_toggle_button_get_active (tb))
    sd->action = UPLOAD_IMGUR;
}



static void cb_imgur_warning_change_cursor (GtkWidget *widget, GdkCursor *cursor)
{
  gdk_window_set_cursor (gtk_widget_get_window (widget), cursor);
}



static void cb_imgur_warning_clicked (GtkWidget *popover)
{
  gtk_widget_set_visible (popover, TRUE);
}



/* Set the delay according to the spinner */
static void cb_delay_spinner_changed (GtkWidget *spinner, ScreenshotData *sd)
{
  sd->delay = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spinner));
}



/* If @timestamp is true, generates a file name @title - date - hour - n.png,
 * where n is the lowest integer such as this file does not exist in the @uri
 * folder.
 * Else, generates a file name @title-n.png, where n is the lowest integer
 * such as this file does not exist in the @uri folder.
 *
 * @uri: uri of the folder for which the filename should be generated.
 * @title: the main title of the file name.
 * @timestamp: whether the date and the hour should be appended to the file name.
 *
 * returns: the filename or NULL if *uri == NULL.
*/
static gchar *generate_filename_for_uri (const gchar *uri,
                                         const gchar *title,
                                         gboolean timestamp)
{
  gboolean exists = TRUE;
  GFile *directory;
  GFile *file;
  gchar *base_name;
  gchar *datetime;
  const gchar *strftime_format = "%Y-%m-%d_%H-%M-%S";

  gint i;

  if (G_UNLIKELY (uri == NULL))
    {
      TRACE ("URI was NULL");

      return NULL;
    }

  TRACE ("Get the folder corresponding to the URI");
  datetime = screenshooter_get_datetime (strftime_format);
  directory = g_file_new_for_uri (uri);
  if (!timestamp)
    base_name = g_strconcat (title, ".png", NULL);
  else
    base_name = g_strconcat (title, "_", datetime, ".png", NULL);

  file = g_file_get_child (directory, base_name);

  if (!g_file_query_exists (file, NULL))
    {
      g_object_unref (file);
      g_object_unref (directory);

      return base_name;
    }

  g_object_unref (file);
  g_free (base_name);

  for (i = 1; exists; ++i)
    {
      const gchar *extension =
        g_strdup_printf ("-%d.png", i);

      if (!timestamp)
         base_name = g_strconcat (title, extension, NULL);
       else
         base_name = g_strconcat (title, "_", datetime, extension, NULL);

      file = g_file_get_child (directory, base_name);

      if (!g_file_query_exists (file, NULL))
        exists = FALSE;

      if (exists)
        g_free (base_name);

      g_object_unref (file);
    }

  g_free(datetime);
  g_object_unref (directory);

  return base_name;
}



/* Set sd->app as per the active item in the combobox */
static void cb_combo_active_item_changed (GtkWidget *box, ScreenshotData *sd)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (box));
  GtkTreeIter iter;
  gchar *active_command = NULL;
  GAppInfo *app_info = NULL;

  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (box), &iter);
  gtk_tree_model_get (model, &iter, 2, &active_command, -1);
  gtk_tree_model_get (model, &iter, 3, &app_info, -1);

  g_free (sd->app);
  sd->app = active_command;
  sd->app_info = app_info;
}



/* Extract the informations from app_info and add them to the
 * liststore.
 * */
static void add_item (GAppInfo *app_info, GtkWidget *liststore)
{
  GtkTreeIter iter;
  const gchar *command = g_app_info_get_executable (app_info);
  const gchar *name = g_app_info_get_name (app_info);
  GIcon *icon = g_app_info_get_icon (app_info);
  GdkPixbuf *pixbuf = NULL;
  GtkIconTheme *icon_theme = gtk_icon_theme_get_default ();
  GtkIconInfo *icon_info;

  icon_info =
    gtk_icon_theme_lookup_by_gicon (icon_theme, icon,
                                    ICON_SIZE,
                                    GTK_ICON_LOOKUP_FORCE_SIZE);

  pixbuf = gtk_icon_info_load_icon (icon_info, NULL);

  if (G_UNLIKELY (pixbuf == NULL))
    {
      pixbuf = gtk_icon_theme_load_icon (icon_theme, "exec", ICON_SIZE,
                                         GTK_ICON_LOOKUP_FORCE_SIZE,
                                         NULL);
    }

  /* Add to the liststore */
  gtk_list_store_append (GTK_LIST_STORE (liststore), &iter);

  gtk_list_store_set (GTK_LIST_STORE (liststore), &iter,
                      0, pixbuf,
                      1, name,
                      2, command,
                      3, g_app_info_dup (app_info),
                      -1);

  /* Free the stuff */
  g_object_unref (pixbuf);
  g_object_unref (icon);
  g_object_unref (icon_info);
}



/* Populate the liststore using the applications which can open image/png. */
static void populate_liststore (GtkListStore *liststore)
{
  const gchar *content_type;
  GList *list_app;

  content_type = "image/png";

  /* Get all applications for image/png.*/
  list_app = g_app_info_get_all_for_type (content_type);

  /* Add them to the liststore */
  if (G_LIKELY (list_app != NULL))
    {
      g_list_foreach (list_app, (GFunc) add_item, liststore);
      g_list_free (list_app);
    }
}



/* Select the sd->app item in the combobox */
static void set_default_item (GtkWidget *combobox, ScreenshotData *sd)
{
  GtkTreeModel *model = gtk_combo_box_get_model (GTK_COMBO_BOX (combobox));

  GtkTreeIter iter;

  /* Get the first iter */
  if (G_LIKELY (gtk_tree_model_get_iter_first (model , &iter)))
    {
      gchar *command = NULL;
      gboolean found = FALSE;
      GAppInfo *app_info;

      /* Loop until finding the appropirate item, if any */
      do
        {
          gtk_tree_model_get (model, &iter, 2, &command, -1);
          gtk_tree_model_get (model, &iter, 3, &app_info, -1);

          if (g_str_equal (command, sd->app))
            {
              gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox),
                                             &iter);
              sd->app_info = app_info;

              found = TRUE;
            }

          g_free (command);
        }
      while (gtk_tree_model_iter_next (model, &iter));

      /* If no suitable item was found, set the first item as active and
       * set sd->app accordingly. */
      if (G_UNLIKELY (!found))
        {
          gtk_tree_model_get_iter_first (model , &iter);
          gtk_tree_model_get (model, &iter, 2, &command, -1);
          gtk_tree_model_get (model, &iter, 3, &app_info, -1);

          gtk_combo_box_set_active_iter (GTK_COMBO_BOX (combobox), &iter);

          g_free (sd->app);

          sd->app = command;
          sd->app_info = app_info;
        }
    }
  else
    {
      g_free (sd->app);

      sd->app = g_strdup ("none");
      sd->app_info = NULL;
    }
}



static GdkPixbuf
*screenshot_get_thumbnail (GdkPixbuf *screenshot)
{
  gint w = gdk_pixbuf_get_width (screenshot);
  gint h = gdk_pixbuf_get_height (screenshot);
  gint width = THUMB_X_SIZE;
  gint height = THUMB_Y_SIZE;

  if (G_LIKELY (w >= h))
    height = width * h / w;
  else
    width = height * w / h;

  return gdk_pixbuf_scale_simple (screenshot, width, height, GDK_INTERP_BILINEAR);
}



static void cb_progress_upload (goffset current_num_bytes,
                                goffset total_num_bytes,
                                gpointer user_data)
{
  gdouble fraction = (double) current_num_bytes / (double) total_num_bytes;

  gfloat current = (float) current_num_bytes / 1000;
  gfloat total = (float) total_num_bytes / 1000;

  gchar *bar_text = g_strdup_printf (_("%.2fKb of %.2fKb"), current, total);

  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (user_data), fraction);

  gtk_progress_bar_set_text (GTK_PROGRESS_BAR (user_data), bar_text);

  g_free (bar_text);
}



static void
cb_finished_upload (GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  gboolean success;

  g_return_if_fail (G_IS_FILE (source_object));

  success = g_file_copy_finish (G_FILE (source_object), res, &error);

  TRACE ("The transfer is finished");

  if (G_UNLIKELY (!success))
    {
      TRACE ("An error occurred");

      screenshooter_error ("%s", error->message);

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

  if (G_UNLIKELY (!gdk_pixbuf_save (screenshot, save_path, "png", &error, NULL)))
    {
      if (error)
        {
          /* See bug #8443, looks like error is not set when path is NULL */
          screenshooter_error ("%s", error->message);
          g_error_free (error);
        }

      g_free (save_path);

      return NULL;
    }
  else
    return save_path;
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
                                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                                   "gtk-cancel",
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
  gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 12);

  gtk_label_set_markup (GTK_LABEL (label1),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">The screenshot "
                          "is being transferred to:</span>"));
  gtk_widget_set_halign (label1, GTK_ALIGN_START);
  gtk_widget_set_valign (label1, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                      label1,
                      FALSE,
                      FALSE,
                      0);
  gtk_widget_show (label1);

  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                      label2,
                      FALSE,
                      FALSE,
                      0);
  gtk_widget_show (label2);

  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))),
                      progress_bar,
                      FALSE,
                      FALSE,
                      0);
  gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (progress_bar), 0);
  gtk_widget_show (progress_bar);

  g_signal_connect (dialog, "response", G_CALLBACK (cb_transfer_dialog_response),
                    cancellable);

  g_file_copy_async (save_file_temp,
                     save_file,
                     G_FILE_COPY_OVERWRITE,
                     G_PRIORITY_DEFAULT,
                     cancellable,
                     (GFileProgressCallback) cb_progress_upload, progress_bar,
                     (GAsyncReadyCallback) cb_finished_upload, dialog);

  gtk_dialog_run (GTK_DIALOG (dialog));

  g_file_delete (save_file_temp, NULL, NULL);

  g_object_unref (save_file_temp);
  g_object_unref (save_parent);
  g_object_unref (cancellable);
  g_free (save_basename);
  g_free (save_path);
}

static void
preview_drag_begin (GtkWidget *widget, GdkDragContext *context, gpointer data)
{
  GdkPixbuf *pixbuf = data;
  gtk_drag_source_set_icon_pixbuf (widget, pixbuf);
}

static void
preview_drag_data_get (GtkWidget *widget, GdkDragContext *context, GtkSelectionData *selection_data,
                       guint info, guint utime, gpointer data)
{
  GdkPixbuf *pixbuf = data;
  gtk_selection_data_set_pixbuf (selection_data, pixbuf);
}

static void
preview_drag_end (GtkWidget *widget, GdkDragContext *context, gpointer data)
{
  /* Try to put ourself on top again */
  GtkWidget *dlg = data;
  gtk_window_present (GTK_WINDOW (dlg));
}




/* Public */



GtkWidget *screenshooter_region_dialog_new (ScreenshotData *sd, gboolean plugin)
{
  GtkWidget *dlg, *main_alignment;
  GtkWidget *vbox;

  GtkWidget *layout_grid;

  GtkWidget *area_main_box, *area_box, *area_label, *area_alignment;
  GtkWidget *active_window_button,
            *fullscreen_button,
            *rectangle_button;

  GtkWidget *show_mouse_checkbox;

  GtkWidget *delay_main_box, *delay_box, *delay_label, *delay_alignment;
  GtkWidget *delay_spinner_box, *delay_spinner, *seconds_label;

  /* Create the dialog */
  if (plugin)
    {
#if LIBXFCE4UI_CHECK_VERSION (4,14,0)
      dlg = xfce_titled_dialog_new_with_mixed_buttons (_("Screenshot"),
        NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
        "help-browser", _("_Help"), GTK_RESPONSE_HELP,
        "window-close", _("_Close"), GTK_RESPONSE_OK,
        NULL);
#else
      dlg = xfce_titled_dialog_new_with_buttons (_("Screenshot"),
        NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
        "gtk-help", GTK_RESPONSE_HELP,
        "gtk-close", GTK_RESPONSE_OK,
        NULL);
#endif

      xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dlg), _("Preferences"));
    }
  else
    {
#if LIBXFCE4UI_CHECK_VERSION (4,14,0)
      dlg = xfce_titled_dialog_new_with_mixed_buttons (_("Screenshot"),
        NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
        "help-browser", _("_Help"), GTK_RESPONSE_HELP,
        "", _("_Cancel"), GTK_RESPONSE_CANCEL,
        "", _("_OK"), GTK_RESPONSE_OK,
        NULL);
#else
      dlg = xfce_titled_dialog_new_with_buttons (_("Screenshot"),
        NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
        "gtk-help", GTK_RESPONSE_HELP,
        "gtk-cancel", GTK_RESPONSE_CANCEL,
        "gtk-ok", GTK_RESPONSE_OK,
        NULL);
#endif

      xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dlg), _("Take a screenshot"));
    }

  gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable (GTK_WINDOW (dlg), FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dlg), 0);
  gtk_window_set_icon_name (GTK_WINDOW (dlg), "org.xfce.screenshooter");
  gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);

  /* Create the main alignment for the dialog */
  main_alignment = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_hexpand (main_alignment, TRUE);
  gtk_widget_set_vexpand (main_alignment, TRUE);
  gtk_widget_set_margin_top (main_alignment, 6);
  gtk_widget_set_margin_bottom (main_alignment, 0);
  gtk_widget_set_margin_start (main_alignment, 12);
  gtk_widget_set_margin_end (main_alignment, 12);

  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))), main_alignment, TRUE, TRUE, 0);

  /* Create the main box for the dialog */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (main_alignment), vbox);

  /* Create the grid to align the differents parts of the top of the UI */
  layout_grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (layout_grid), 20);
  gtk_box_pack_start (GTK_BOX (vbox), layout_grid, TRUE, TRUE, 0);

  /* Create the main box for the regions */
  area_main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_grid_attach (GTK_GRID (layout_grid), area_main_box, 0, 0, 1, 2);

  /* Create area label */
  area_label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (area_label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">"
                          "Region to capture</span>"));
  gtk_widget_set_halign (area_label, GTK_ALIGN_START);
  gtk_widget_set_valign (area_label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (area_main_box), area_label);

  /* Create area alignment */
  area_alignment = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_hexpand (area_alignment, TRUE);
  gtk_widget_set_vexpand (area_alignment, TRUE);
  gtk_widget_set_margin_top (area_alignment, 0);
  gtk_widget_set_margin_bottom (area_alignment, 6);
  gtk_widget_set_margin_start (area_alignment, 12);
  gtk_widget_set_margin_end (area_alignment, 0);
  gtk_container_add (GTK_CONTAINER (area_main_box), area_alignment);

  /* Create area box */
  area_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_add (GTK_CONTAINER (area_alignment), area_box);
  gtk_box_set_spacing (GTK_BOX (area_box), 12);
  gtk_container_set_border_width (GTK_CONTAINER (area_box), 0);

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
  g_signal_connect (G_OBJECT (fullscreen_button), "activate",
                    G_CALLBACK (cb_radiobutton_activate),
                    dlg);

  /* Active window */
  active_window_button =
    gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (fullscreen_button),
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
  g_signal_connect (G_OBJECT (active_window_button), "activate",
                    G_CALLBACK (cb_radiobutton_activate),
                    dlg);

  /* Rectangle */
  rectangle_button =
    gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (fullscreen_button),
                                                 _("Select a region"));
  gtk_box_pack_start (GTK_BOX (area_box), rectangle_button, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rectangle_button),
                                (sd->region == SELECT));
  gtk_widget_set_tooltip_text (rectangle_button,
                               _("Select a region to be captured by clicking a point of "
                                 "the screen without releasing the mouse button, "
                                 "dragging your mouse to the other corner of the region, "
                                 "and releasing the mouse button.\n\n"
                                 "Press Ctrl while dragging to move the region."));

  g_signal_connect (G_OBJECT (rectangle_button), "toggled",
                    G_CALLBACK (cb_rectangle_toggled), sd);
  g_signal_connect (G_OBJECT (rectangle_button), "activate",
                    G_CALLBACK (cb_radiobutton_activate),
                    dlg);

  /* Create show mouse checkbox */
  show_mouse_checkbox =
    gtk_check_button_new_with_label (_("Capture the mouse pointer"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (show_mouse_checkbox),
                                (sd->show_mouse == 1));
  gtk_widget_set_tooltip_text (show_mouse_checkbox,
                               _("Display the mouse pointer on the screenshot"));
  gtk_box_pack_start (GTK_BOX (area_box),
                      show_mouse_checkbox, FALSE,
                      FALSE, 5);
  g_signal_connect (G_OBJECT (show_mouse_checkbox), "toggled",
                    G_CALLBACK (cb_show_mouse_toggled), sd);

  /* Create the main box for the delay stuff */
  delay_main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_grid_attach (GTK_GRID (layout_grid), delay_main_box, 1, 0, 1, 1);

  /* Create delay label */
  delay_label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL(delay_label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">"
                          "Delay before capturing</span>"));
  gtk_widget_set_halign (delay_label, GTK_ALIGN_START);
  gtk_widget_set_valign (delay_label, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (delay_main_box), delay_label, FALSE, FALSE, 0);

  /* Create delay alignment */
  delay_alignment = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_hexpand (delay_alignment, FALSE);
  gtk_widget_set_vexpand (delay_alignment, FALSE);
  gtk_widget_set_margin_top (delay_alignment, 0);
  gtk_widget_set_margin_bottom (delay_alignment, 6);
  gtk_widget_set_margin_start (delay_alignment, 12);
  gtk_widget_set_margin_end (delay_alignment, 0);
  gtk_box_pack_start (GTK_BOX (delay_main_box), delay_alignment, FALSE, FALSE, 0);

  /* Create delay box */
  delay_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (delay_alignment), delay_box);
  gtk_container_set_border_width (GTK_CONTAINER (delay_box), 0);

  /* Create delay spinner */
  delay_spinner_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  gtk_box_pack_start (GTK_BOX (delay_box), delay_spinner_box, FALSE, FALSE, 0);

  delay_spinner = gtk_spin_button_new_with_range(0.0, 60.0, 1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (delay_spinner), sd->delay);
  gtk_widget_set_tooltip_text (delay_spinner,
                               _("Delay in seconds before the screenshot is taken"));
  gtk_box_pack_start (GTK_BOX (delay_spinner_box), delay_spinner, FALSE, FALSE, 0);

  seconds_label = gtk_label_new (_("seconds"));
  gtk_box_pack_start (GTK_BOX (delay_spinner_box), seconds_label, FALSE, FALSE, 0);
  g_signal_connect (G_OBJECT (delay_spinner), "value-changed",
                    G_CALLBACK (cb_delay_spinner_changed), sd);

  gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dlg)));

  /* Set focus to the selected radio button */
  switch (sd->region)
    {
      case FULLSCREEN:
        gtk_widget_grab_focus (fullscreen_button);
        break;
      case ACTIVE_WINDOW:
        gtk_widget_grab_focus (active_window_button);
        break;
      case SELECT:
        gtk_widget_grab_focus (rectangle_button);
        break;
      default:
        break;
    }

  return dlg;
}




GtkWidget *screenshooter_actions_dialog_new (ScreenshotData *sd)
{
  GtkWidget *dlg, *main_alignment;
  GtkWidget *vbox, *hbox, *evbox;

  GtkWidget *layout_grid;

  GtkWidget *left_box;
  GtkWidget *actions_label, *actions_alignment, *actions_grid;
  GtkWidget *save_radio_button;
  GtkWidget *clipboard_radio_button, *open_with_radio_button;
  GtkWidget *imgur_radio_button, *imgur_warning_image, *imgur_warning_label;
  GtkWidget *popover;

  GtkListStore *liststore;
  GtkWidget *combobox;
  GtkCellRenderer *renderer, *renderer_pixbuf;

  GtkWidget *preview, *preview_ebox, *preview_box, *preview_label;
  GdkPixbuf *thumbnail;
  GdkCursor *cursor;

#if LIBXFCE4UI_CHECK_VERSION (4,14,0)
  dlg = xfce_titled_dialog_new_with_mixed_buttons (_("Screenshot"),
    NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
    "help-browser", _("_Help"), GTK_RESPONSE_HELP,
    "", _("_Cancel"), GTK_RESPONSE_CANCEL,
    "", _("_OK"), GTK_RESPONSE_OK,
    NULL);
#else
  dlg = xfce_titled_dialog_new_with_buttons (_("Screenshot"),
    NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
    "gtk-help", GTK_RESPONSE_HELP,
    "gtk-cancel", GTK_RESPONSE_CANCEL,
    "gtk-ok", GTK_RESPONSE_OK,
    NULL);
#endif

  xfce_titled_dialog_set_subtitle (XFCE_TITLED_DIALOG (dlg),
                                   _("Choose what to do with the screenshot"));
  gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);
  gtk_window_set_resizable (GTK_WINDOW (dlg), FALSE);
  gtk_container_set_border_width (GTK_CONTAINER (dlg), 0);
  gtk_window_set_icon_name (GTK_WINDOW (dlg), "org.xfce.screenshooter");
  gtk_dialog_set_default_response (GTK_DIALOG (dlg), GTK_RESPONSE_OK);

  /* Create the main alignment for the dialog */
  main_alignment = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_hexpand (main_alignment, TRUE);
  gtk_widget_set_vexpand (main_alignment, TRUE);
  gtk_widget_set_margin_top (main_alignment, 6);
  gtk_widget_set_margin_bottom (main_alignment, 0);
  gtk_widget_set_margin_start (main_alignment, 12);
  gtk_widget_set_margin_end (main_alignment, 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dlg))), main_alignment, TRUE, TRUE, 0);

  /* Create the main box for the dialog */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (main_alignment), vbox);

  /* Create the grid to align the two columns of the UI */
  layout_grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (layout_grid), 20);
  gtk_box_pack_start (GTK_BOX (vbox), layout_grid, TRUE, TRUE, 0);

  /* Create the box which containes the left part of the UI */
  left_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_grid_attach (GTK_GRID (layout_grid), left_box, 0, 0, 1, 1);

  /* Create actions label */
  actions_label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (actions_label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">Action"
                          "</span>"));
  gtk_widget_set_halign (actions_label, GTK_ALIGN_START);
  gtk_widget_set_valign (actions_label, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (left_box), actions_label, FALSE, FALSE, 0);

  /* Create actions alignment */
  actions_alignment = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_hexpand (actions_alignment, TRUE);
  gtk_widget_set_vexpand (actions_alignment, TRUE);
  gtk_widget_set_margin_top (actions_alignment, 0);
  gtk_widget_set_margin_bottom (actions_alignment, 6);
  gtk_widget_set_margin_start (actions_alignment, 12);
  gtk_widget_set_margin_end (actions_alignment, 0);
  gtk_box_pack_start (GTK_BOX (left_box), actions_alignment, TRUE, TRUE, 0);

  /* Create the actions box */
  actions_grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (actions_alignment), actions_grid);

  gtk_grid_set_row_spacing (GTK_GRID (actions_grid), 6);
  gtk_grid_set_column_spacing (GTK_GRID (actions_grid), 6);
  gtk_container_set_border_width (GTK_CONTAINER (actions_grid), 0);

  /* Save option radio button */
  save_radio_button = gtk_radio_button_new_with_mnemonic (NULL, _("Save"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (save_radio_button),
                                (sd->action & SAVE));
  g_signal_connect (G_OBJECT (save_radio_button), "toggled",
                    G_CALLBACK (cb_save_toggled), sd);
  g_signal_connect (G_OBJECT (save_radio_button), "activate",
                    G_CALLBACK (cb_radiobutton_activate), dlg);
  gtk_widget_set_tooltip_text (save_radio_button, _("Save the screenshot to a PNG file"));
  gtk_grid_attach (GTK_GRID (actions_grid), save_radio_button, 0, 0, 1, 1);

  if (sd->plugin ||
      gdk_display_supports_clipboard_persistence (gdk_display_get_default ()))
    {
      /* Copy to clipboard radio button */
      clipboard_radio_button =
        gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (save_radio_button),
                                                     _("Copy to the clipboard"));
      gtk_widget_set_tooltip_text (clipboard_radio_button,
                                   _("Copy the screenshot to the clipboard so that it can be "
                                     "pasted later"));
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (clipboard_radio_button),
                                    (sd->action & CLIPBOARD));
      g_signal_connect (G_OBJECT (clipboard_radio_button), "toggled",
                        G_CALLBACK (cb_clipboard_toggled), sd);
      g_signal_connect (G_OBJECT (clipboard_radio_button), "activate",
                        G_CALLBACK (cb_radiobutton_activate), dlg);
      gtk_grid_attach (GTK_GRID (actions_grid), clipboard_radio_button, 0, 1, 1, 1);
    }

  /* Open with radio button */
  open_with_radio_button =
    gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (save_radio_button),
                                                 _("Open with:"));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (open_with_radio_button),
                                (sd->action & OPEN));
  g_signal_connect (G_OBJECT (open_with_radio_button), "toggled",
                    G_CALLBACK (cb_open_toggled), sd);
  g_signal_connect (G_OBJECT (open_with_radio_button), "activate",
                    G_CALLBACK (cb_radiobutton_activate), dlg);
  gtk_widget_set_tooltip_text (open_with_radio_button,
                               _("Open the screenshot with the chosen application"));
  gtk_grid_attach (GTK_GRID (actions_grid), open_with_radio_button, 0, 2, 1, 1);

  /* Open with combobox */
  liststore = gtk_list_store_new (4, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_APP_INFO);
  combobox = gtk_combo_box_new_with_model (GTK_TREE_MODEL (liststore));
  renderer = gtk_cell_renderer_text_new ();
  renderer_pixbuf = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combobox), renderer_pixbuf, FALSE);
  gtk_cell_layout_pack_end (GTK_CELL_LAYOUT (combobox), renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer, "text", 1, NULL);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combobox), renderer_pixbuf,
                                  "pixbuf", 0, NULL);
  populate_liststore (liststore);
  set_default_item (combobox, sd);
  gtk_grid_attach (GTK_GRID (actions_grid), combobox, 1, 2, 1, 1);

  g_signal_connect (G_OBJECT (combobox), "changed",
                    G_CALLBACK (cb_combo_active_item_changed), sd);
  gtk_widget_set_tooltip_text (combobox, _("Application to open the screenshot"));
  g_signal_connect (G_OBJECT (open_with_radio_button), "toggled",
                    G_CALLBACK (cb_toggle_set_sensi), combobox);

  /* Run the callback functions to grey/ungrey the correct widgets */
  cb_toggle_set_sensi (GTK_TOGGLE_BUTTON (open_with_radio_button), combobox);

  /* Upload to imgur radio button */
  if (sd->enable_imgur_upload)
    {
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_grid_attach (GTK_GRID (actions_grid), hbox, 0, 4, 1, 1);

    imgur_radio_button =
      gtk_radio_button_new_with_label_from_widget (GTK_RADIO_BUTTON (save_radio_button),
                                                   _("Host on Imgur"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (imgur_radio_button),
                                (sd->action & UPLOAD_IMGUR));
    gtk_widget_set_tooltip_text (imgur_radio_button,
                                 _("Host the screenshot on Imgur, a free online "
                                   "image hosting service"));
    g_signal_connect (G_OBJECT (imgur_radio_button), "toggled",
                      G_CALLBACK (cb_imgur_toggled), sd);
    g_signal_connect (G_OBJECT (imgur_radio_button), "activate",
                      G_CALLBACK (cb_radiobutton_activate), dlg);
    gtk_container_add (GTK_CONTAINER (hbox), imgur_radio_button);

    /* Upload to imgur warning info */
    imgur_warning_image = gtk_image_new_from_icon_name ("dialog-warning",
                                                        GTK_ICON_SIZE_BUTTON);
    imgur_warning_label = gtk_label_new (
      _("Watch for sensitive content, the uploaded image will be publicly\n"
        "available and there is no guarantee it can be certainly deleted."));

    popover = gtk_popover_new (imgur_warning_image);
    gtk_container_add (GTK_CONTAINER (popover), imgur_warning_label);
    gtk_container_set_border_width (GTK_CONTAINER (popover), 6);
    gtk_widget_show (imgur_warning_label);

    evbox = gtk_event_box_new ();
    g_signal_connect_swapped (G_OBJECT (evbox), "button-press-event",
                              G_CALLBACK (cb_imgur_warning_clicked), popover);
    gtk_container_add (GTK_CONTAINER (hbox), evbox);
    gtk_container_add (GTK_CONTAINER (evbox), imgur_warning_image);

    cursor = gdk_cursor_new_for_display (gdk_display_get_default (), GDK_HAND2);
    g_signal_connect (evbox, "realize",
                      G_CALLBACK (cb_imgur_warning_change_cursor), cursor);
    g_object_unref (cursor);
    }

  /* Preview box */
  preview_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
  gtk_container_set_border_width (GTK_CONTAINER (preview_box), 0);
  gtk_grid_attach (GTK_GRID (layout_grid), preview_box, 1, 0, 1, 1);

  /* Preview label*/
  preview_label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (preview_label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">"
                          "Preview</span>"));
  gtk_widget_set_halign (preview_label, GTK_ALIGN_START);
  gtk_widget_set_valign (preview_label, GTK_ALIGN_CENTER);
  gtk_box_pack_start (GTK_BOX (preview_box), preview_label, FALSE, FALSE, 0);

  /* The preview image */
  thumbnail = screenshot_get_thumbnail (sd->screenshot);
  preview_ebox = gtk_event_box_new ();
  preview = gtk_image_new_from_pixbuf (thumbnail);
  g_object_unref (thumbnail);
  gtk_container_add (GTK_CONTAINER (preview_ebox), preview);
  gtk_box_pack_start (GTK_BOX (preview_box), preview_ebox, FALSE, FALSE, 0);

  /* DND for the preview image */
  gtk_drag_source_set (preview_ebox, GDK_BUTTON1_MASK, NULL, 0, GDK_ACTION_COPY);
  gtk_drag_source_add_image_targets (preview_ebox);
  g_signal_connect (preview_ebox, "drag-begin", G_CALLBACK (preview_drag_begin), thumbnail);
  g_signal_connect (preview_ebox, "drag-data-get", G_CALLBACK (preview_drag_data_get), sd->screenshot);
  g_signal_connect (preview_ebox, "drag-end", G_CALLBACK (preview_drag_end), dlg);

  gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dlg)));

  return dlg;
}



/* Saves the @screenshot in the given @directory using
 * @title and @timestamp to generate the file name.
 *
 * @screenshot: a GdkPixbuf containing the screenshot.
 * @directory: the save location.
 * @title: the title of the screenshot.
 * @timestamp: whether the date and the hour should be added to
 * the file name.
 * @save_dialog: whether a save dialog should be displayed to
 * let the user set a custom save location
 * @show_preview: if @save_dialog is true, @show_preview will
 * decide whether the save dialog should display a preview of
 * @screenshot.
 *
 * Returns: a string containing the path to the saved file.
 */
gchar
*screenshooter_save_screenshot (GdkPixbuf *screenshot,
                                const gchar *directory,
                                const gchar *title,
                                gboolean timestamp,
                                gboolean save_dialog,
                                gboolean show_preview)
{
  const gchar *filename = generate_filename_for_uri (directory, title, timestamp);
  gchar *save_uri = g_build_filename (directory, filename, NULL);
  gchar *result;

  if (save_dialog)
  {
    GtkWidget *chooser;
    gint dialog_response;

    chooser =
      gtk_file_chooser_dialog_new (_("Save screenshot as..."),
                                   NULL,
                                   GTK_FILE_CHOOSER_ACTION_SAVE,
                                   "gtk-cancel",
                                   GTK_RESPONSE_CANCEL,
                                   "gtk-save",
                                   GTK_RESPONSE_ACCEPT,
                                   NULL);

    gtk_window_set_icon_name (GTK_WINDOW (chooser), "org.xfce.screenshooter");
    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (chooser),
                                                    TRUE);
    gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), FALSE);
    gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);
    gtk_file_chooser_set_current_folder_uri (GTK_FILE_CHOOSER (chooser), directory);
    gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser), filename);

    if(show_preview)
      {
        GtkWidget *preview, *preview_ebox;
        GdkPixbuf *thumbnail;

        /* Create the preview and the thumbnail */
        preview_ebox = gtk_event_box_new ();
        preview = gtk_image_new ();
        gtk_widget_set_margin_end (preview, 12);
        gtk_container_add (GTK_CONTAINER (preview_ebox), preview);
        gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (chooser), preview_ebox);

        thumbnail = screenshot_get_thumbnail (screenshot);

        gtk_image_set_from_pixbuf (GTK_IMAGE (preview), thumbnail);
        g_object_unref (thumbnail);

        /* DND for the preview image */
        gtk_drag_source_set (preview_ebox, GDK_BUTTON1_MASK, NULL, 0, GDK_ACTION_COPY);
        gtk_drag_source_add_image_targets (preview_ebox);
        g_signal_connect (preview_ebox, "drag-begin", G_CALLBACK (preview_drag_begin), thumbnail);
        g_signal_connect (preview_ebox, "drag-data-get", G_CALLBACK (preview_drag_data_get), screenshot);
        g_signal_connect (preview_ebox, "drag-end", G_CALLBACK (preview_drag_end), chooser);

        gtk_widget_show (preview);
      }

    dialog_response = gtk_dialog_run (GTK_DIALOG (chooser));

    if (G_LIKELY (dialog_response == GTK_RESPONSE_ACCEPT))
      {
        g_free (save_uri);
        save_uri = gtk_file_chooser_get_uri (GTK_FILE_CHOOSER (chooser));
        result = screenshooter_save_screenshot_to (screenshot, save_uri);
      }
    else
      result = NULL;

    gtk_widget_destroy (chooser);
  }
  else
    result = screenshooter_save_screenshot_to (screenshot, save_uri);

  g_free (save_uri);

  return result;
}

gchar
*screenshooter_save_screenshot_to (GdkPixbuf   *screenshot,
                                   const gchar *save_uri)
{
  GFile *save_file = NULL;
  gchar *result = NULL;

  g_return_val_if_fail (save_uri != NULL, NULL);

  save_file = g_file_new_for_uri (save_uri);

  /* If the URI is a local one, we save directly */
  if (!screenshooter_is_remote_uri (save_uri))
    result = save_screenshot_to_local_path (screenshot, save_file);
  else
    save_screenshot_to_remote_location (screenshot, save_file);

  g_object_unref (save_file);

  return result;
}
