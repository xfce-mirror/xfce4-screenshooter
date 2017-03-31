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

#include "screenshooter-job-callbacks.h"

/* Create and return a dialog with a spinner and a translated title
 * will be used during upload jobs
 */

GtkWidget *
create_spinner_dialog             (const gchar        *title,
                                    GtkWidget         **label)
{
  GtkWidget *dialog;
  GtkWidget *status_label;
  GtkWidget *hbox, *spinner;
  GtkWidget *main_box, *main_alignment;

  dialog = gtk_dialog_new_with_buttons (title,
                                 NULL,
                                 GTK_DIALOG_DESTROY_WITH_PARENT,
                                 NULL);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 0);
  gtk_window_set_deletable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "gtk-info");

  /* Create the main alignment for the dialog */
  main_alignment = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_hexpand (main_alignment, TRUE);
  gtk_widget_set_vexpand (main_alignment, TRUE);
  gtk_widget_set_margin_top (main_alignment, 0);
  gtk_widget_set_margin_bottom (main_alignment, 0);
  gtk_widget_set_margin_start (main_alignment, 6);
  gtk_widget_set_margin_end (main_alignment, 6);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), main_alignment, TRUE, TRUE, 0);

  /* Create the main box for the dialog */
  main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width (GTK_CONTAINER (main_box), 12);
  gtk_container_add (GTK_CONTAINER (main_alignment), main_box);

  /* Top horizontal box for the spinner */
  hbox= gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 0);
  gtk_container_add (GTK_CONTAINER (main_box), hbox);

  /* Add the spinner */
  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_box_pack_end (GTK_BOX (hbox), spinner, FALSE, FALSE, 0);

  /* Status label*/
  status_label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (status_label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">"
                          "Status</span>"));
  gtk_widget_set_halign (status_label, GTK_ALIGN_START);
  gtk_widget_set_valign (status_label, GTK_ALIGN_START);
  gtk_box_pack_start (GTK_BOX (hbox), status_label, FALSE, FALSE, 0);

  /* Information label */
  *label = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (main_box), *label);

  gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dialog)));
  return dialog;
}

void cb_error (ExoJob *job, GError *error, gpointer unused)
{
  g_return_if_fail (error != NULL);

  screenshooter_error ("%s", error->message);
}



void cb_finished (ExoJob *job, GtkWidget *dialog)
{
  g_return_if_fail (EXO_IS_JOB (job));
  g_return_if_fail (GTK_IS_DIALOG (dialog));

  g_signal_handlers_disconnect_matched (job,
                                        G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL,
                                        cb_image_uploaded,
                                        NULL);

  g_signal_handlers_disconnect_matched (job,
                                        G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL,
                                        cb_error,
                                        NULL);

  g_signal_handlers_disconnect_matched (job,
                                        G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL,
                                        cb_ask_for_information,
                                        NULL);

  g_signal_handlers_disconnect_matched (job,
                                        G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL,
                                        cb_update_info,
                                        NULL);

  g_signal_handlers_disconnect_matched (job,
                                        G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL,
                                        cb_finished,
                                        NULL);

  g_object_unref (G_OBJECT (job));
  gtk_widget_destroy (dialog);
}



void cb_update_info (ExoJob *job, gchar *message, GtkWidget *label)
{
  g_return_if_fail (EXO_IS_JOB (job));
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_set_text (GTK_LABEL (label), message);
}

void
cb_ask_for_information (ScreenshooterJob *job,
                        GtkListStore     *liststore,
                        const gchar      *message,
                        gpointer          unused)
{
  GtkWidget *dialog;
  GtkWidget *information_label;
  GtkWidget *vbox, *main_alignment;
  GtkWidget *table;
  GtkWidget *user_entry, *password_entry, *title_entry, *comment_entry;
  GtkWidget *user_label, *password_label, *title_label, *comment_label;

  GtkTreeIter iter;
  gint response;

  g_return_if_fail (SCREENSHOOTER_IS_JOB (job));
  g_return_if_fail (GTK_IS_LIST_STORE (liststore));
  g_return_if_fail (message != NULL);

  TRACE ("Create the dialog to ask for user information.");

  /* Create the information dialog */
  dialog =
    xfce_titled_dialog_new_with_buttons (_("Details about the screenshot for ZimageZ"),
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         "gtk-cancel",
                                         GTK_RESPONSE_CANCEL,
                                         "gtk-ok",
                                         GTK_RESPONSE_OK,
                                         NULL);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 12);

  gtk_window_set_icon_name (GTK_WINDOW (dialog), "gtk-info");
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  /* Create the main alignment for the dialog */
  main_alignment = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_hexpand (main_alignment, TRUE);
  gtk_widget_set_vexpand (main_alignment, TRUE);
  gtk_widget_set_margin_top (main_alignment, 6);
  gtk_widget_set_margin_bottom (main_alignment, 0);
  gtk_widget_set_margin_start (main_alignment, 12);
  gtk_widget_set_margin_end (main_alignment, 12);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), main_alignment, TRUE, TRUE, 0);

  /* Create the main box for the dialog */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (main_alignment), vbox);

  /* Create the information label */
  information_label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (information_label), message);
  gtk_widget_set_halign (information_label, GTK_ALIGN_START);
  gtk_widget_set_valign (information_label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (vbox), information_label);

  /* Create the layout table */
  table = gtk_table_new (4, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 12);
  gtk_container_add (GTK_CONTAINER (vbox), table);

  /* Create the user label */
  user_label = gtk_label_new (_("User:"));
  gtk_widget_set_halign (user_label, GTK_ALIGN_START);
  gtk_widget_set_valign (user_label, GTK_ALIGN_CENTER);
  gtk_table_attach (GTK_TABLE (table), user_label,
                    0, 1,
                    0, 1,
                    GTK_FILL, GTK_FILL,
                    0, 0);

  /* Create the user entry */
  user_entry = gtk_entry_new ();
  gtk_widget_set_tooltip_text (user_entry,
                               _("Your Zimagez user name, if you do not have one yet"
                                 " please create one on the Web page linked above"));
  gtk_entry_set_activates_default (GTK_ENTRY (user_entry), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (table), user_entry, 1, 2, 0, 1);

  /* Create the password label */
  password_label = gtk_label_new (_("Password:"));
  gtk_widget_set_halign (password_label, GTK_ALIGN_START);
  gtk_widget_set_valign (password_label, GTK_ALIGN_CENTER);
  gtk_table_attach (GTK_TABLE (table), password_label,
                    0, 1,
                    1, 2,
                    GTK_FILL, GTK_FILL,
                    0, 0);

  /* Create the password entry */
  password_entry = gtk_entry_new ();
  gtk_widget_set_tooltip_text (password_entry, _("The password for the user above"));
  gtk_entry_set_visibility (GTK_ENTRY (password_entry), FALSE);
  gtk_entry_set_activates_default (GTK_ENTRY (password_entry), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (table), password_entry, 1, 2, 1, 2);

  /* Create the title label */
  title_label = gtk_label_new (_("Title:"));
  gtk_widget_set_halign (title_label, GTK_ALIGN_START);
  gtk_widget_set_valign (title_label, GTK_ALIGN_CENTER);
  gtk_table_attach (GTK_TABLE (table), title_label,
                    0, 1,
                    2, 3,
                    GTK_FILL, GTK_FILL,
                    0, 0);
  /* Create the title entry */
  title_entry = gtk_entry_new ();
  gtk_widget_set_tooltip_text (title_entry,
                               _("The title of the screenshot, it will be used when"
                                 " displaying the screenshot on ZimageZ"));
  gtk_entry_set_activates_default (GTK_ENTRY (title_entry), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (table), title_entry, 1, 2, 2, 3);

  /* Create the comment label */
  comment_label = gtk_label_new (_("Comment:"));
  gtk_widget_set_halign (comment_label, GTK_ALIGN_START);
  gtk_widget_set_valign (comment_label, GTK_ALIGN_CENTER);
  gtk_table_attach (GTK_TABLE (table), comment_label,
                    0, 1,
                    3, 4,
                    GTK_FILL, GTK_FILL,
                    0, 0);

  /* Create the comment entry */
  comment_entry = gtk_entry_new ();
  gtk_widget_set_tooltip_text (comment_entry,
                               _("A comment on the screenshot, it will be used when"
                                 " displaying the screenshot on ZimageZ"));
  gtk_entry_set_activates_default (GTK_ENTRY (comment_entry), TRUE);
  gtk_table_attach_defaults (GTK_TABLE (table), comment_entry, 1, 2, 3, 4);

  /* Set the values */
  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (liststore), &iter);

  do
    {
      gint field_index;
      gchar *field_value = NULL;

      gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
                          0, &field_index,
                          1, &field_value,
                          -1);
      switch (field_index)
        {
          case USER:
            gtk_entry_set_text (GTK_ENTRY (user_entry), field_value);
            break;
          case PASSWORD:
            gtk_entry_set_text (GTK_ENTRY (password_entry), field_value);
            break;
          case TITLE:
            gtk_entry_set_text (GTK_ENTRY (title_entry), field_value);
            break;
          case COMMENT:
            gtk_entry_set_text (GTK_ENTRY (comment_entry), field_value);
            break;
          default:
            break;
        }

      g_free (field_value);
    }
  while (gtk_tree_model_iter_next (GTK_TREE_MODEL (liststore), &iter));

  gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dialog)));
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_hide (dialog);

  if (response == GTK_RESPONSE_CANCEL || response == GTK_RESPONSE_DELETE_EVENT)
    {
      exo_job_cancel (EXO_JOB (job));
    }
  else if (response == GTK_RESPONSE_OK)
    {
      gtk_tree_model_get_iter_first (GTK_TREE_MODEL (liststore), &iter);

      do
        {
          gint field_index;

          gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
                              0, &field_index, -1);

          switch (field_index)
            {
              case USER:
                gtk_list_store_set (liststore, &iter,
                                    1, gtk_entry_get_text (GTK_ENTRY (user_entry)),
                                    -1);
                break;
              case PASSWORD:
                gtk_list_store_set (liststore, &iter,
                                    1, gtk_entry_get_text (GTK_ENTRY (password_entry)),
                                    -1);
                break;
              case TITLE:
                gtk_list_store_set (liststore, &iter,
                                    1, gtk_entry_get_text (GTK_ENTRY (title_entry)),
                                    -1);
                break;
              case COMMENT:
                gtk_list_store_set (liststore, &iter,
                                    1, gtk_entry_get_text (GTK_ENTRY (comment_entry)),
                                    -1);
                break;
              default:
                break;
            }
        }
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (liststore), &iter));
    }

  gtk_widget_destroy (dialog);
}

void cb_image_uploaded (ScreenshooterJob  *job,
                        gchar             *upload_name,
                        gchar            **last_user)
{
  GtkWidget *dialog;
  GtkWidget *main_alignment, *vbox;
  GtkWidget *link_label;
  GtkWidget *image_link, *thumbnail_link, *small_thumbnail_link;
  GtkWidget *example_label, *html_label, *bb_label;
  GtkWidget *html_code_view, *bb_code_view;
  GtkWidget *html_frame, *bb_frame;
  GtkWidget *links_alignment, *code_alignment;
  GtkWidget *links_box, *code_box;

  GtkTextBuffer *html_buffer, *bb_buffer;

  const gchar *image_url, *thumbnail_url, *small_thumbnail_url;
  const gchar *image_markup, *thumbnail_markup, *small_thumbnail_markup;
  const gchar *html_code, *bb_code;
  gchar *job_type, *title;
  gchar *last_user_temp;

  g_return_if_fail (upload_name != NULL);
  job_type = g_object_get_data(G_OBJECT (job), "jobtype");
  if (!strcmp(job_type, "imgur")) {
    title = _("My screenshot on Imgur");
    image_url = g_strdup_printf ("http://i.imgur.com/%s.png", upload_name);
    thumbnail_url =
      g_strdup_printf ("http://imgur.com/%sl.png", upload_name);
    small_thumbnail_url =
      g_strdup_printf ("http://imgur.com/%ss.png", upload_name);
  } else {
    g_return_if_fail (last_user == NULL || *last_user == NULL);
    title = _("My screenshot on ZimageZ");
    image_url = g_strdup_printf ("http://www.zimagez.com/zimage/%s.php", upload_name);
    thumbnail_url =
      g_strdup_printf ("http://www.zimagez.com/miniature/%s.php", upload_name);
    small_thumbnail_url =
      g_strdup_printf ("http://www.zimagez.com/avatar/%s.php", upload_name);
    last_user_temp = g_object_get_data (G_OBJECT (job), "user");

    if (last_user_temp == NULL)
      last_user_temp = g_strdup ("");

    *last_user = g_strdup (last_user_temp);
  }
  image_markup =
    g_markup_printf_escaped (_("<a href=\"%s\">Full size image</a>"), image_url);
  thumbnail_markup =
    g_markup_printf_escaped (_("<a href=\"%s\">Large thumbnail</a>"), thumbnail_url);
  small_thumbnail_markup =
    g_markup_printf_escaped (_("<a href=\"%s\">Small thumbnail</a>"), small_thumbnail_url);
  html_code =
    g_markup_printf_escaped ("<a href=\"%s\">\n  <img src=\"%s\" />\n</a>",
                     image_url, thumbnail_url);
  bb_code =
    g_strdup_printf ("[url=%s]\n  [img]%s[/img]\n[/url]", image_url, thumbnail_url);

  /* Dialog */
  dialog =
    xfce_titled_dialog_new_with_buttons (title,
                                         NULL,
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         "gtk-close",
                                         GTK_RESPONSE_CLOSE,
                                         NULL);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 0);
  gtk_box_set_spacing (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), 12);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "applications-internet");
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  /* Create the main alignment for the dialog */
  main_alignment = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_hexpand (main_alignment, TRUE);
  gtk_widget_set_vexpand (main_alignment, TRUE);
  gtk_widget_set_margin_top (main_alignment, 6);
  gtk_widget_set_margin_bottom (main_alignment, 0);
  gtk_widget_set_margin_start (main_alignment, 10);
  gtk_widget_set_margin_end (main_alignment, 10);
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), main_alignment, TRUE, TRUE, 0);

  /* Create the main box for the dialog */
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (main_alignment), vbox);

  /* Links bold label */
  link_label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (link_label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">"
                          "Links</span>"));
  gtk_widget_set_halign (link_label, GTK_ALIGN_START);
  gtk_widget_set_valign (link_label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (vbox), link_label);

  /* Links alignment */
  links_alignment = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_hexpand (links_alignment, TRUE);
  gtk_widget_set_vexpand (links_alignment, TRUE);
  gtk_widget_set_margin_top (links_alignment, 0);
  gtk_widget_set_margin_bottom (links_alignment, 0);
  gtk_widget_set_margin_start (links_alignment, 12);
  gtk_widget_set_margin_end (links_alignment, 0);
  gtk_container_add (GTK_CONTAINER (vbox), links_alignment);

  /* Links box */
  links_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width (GTK_CONTAINER (links_box), 0);
  gtk_container_add (GTK_CONTAINER (links_alignment), links_box);

  /* Create the image link */
  image_link = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (image_link), image_markup);
  gtk_widget_set_halign (image_link, GTK_ALIGN_START);
  gtk_widget_set_valign (image_link, GTK_ALIGN_START);
  gtk_widget_set_tooltip_text (image_link, image_url);
  gtk_container_add (GTK_CONTAINER (links_box), image_link);

  /* Create the thumbnail link */
  thumbnail_link = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (thumbnail_link), thumbnail_markup);
  gtk_widget_set_halign (thumbnail_link, GTK_ALIGN_START);
  gtk_widget_set_valign (thumbnail_link, GTK_ALIGN_START);
  gtk_widget_set_tooltip_text (thumbnail_link, thumbnail_url);
  gtk_container_add (GTK_CONTAINER (links_box), thumbnail_link);

  /* Create the small thumbnail link */
  small_thumbnail_link = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (small_thumbnail_link), small_thumbnail_markup);
  gtk_widget_set_halign (small_thumbnail_link, GTK_ALIGN_START);
  gtk_widget_set_valign (small_thumbnail_link, GTK_ALIGN_START);
  gtk_widget_set_tooltip_text (small_thumbnail_link, small_thumbnail_url);
  gtk_container_add (GTK_CONTAINER (links_box), small_thumbnail_link);

  /* Examples bold label */
  example_label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (example_label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">"
                          "Code for a thumbnail pointing to the full size image</span>"));
  gtk_widget_set_halign (example_label, GTK_ALIGN_START);
  gtk_widget_set_valign (example_label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (vbox), example_label);

  /* Code alignment */
  code_alignment = gtk_box_new (GTK_ORIENTATION_VERTICAL, 1);
  gtk_widget_set_hexpand (code_alignment, TRUE);
  gtk_widget_set_vexpand (code_alignment, TRUE);
  gtk_widget_set_margin_top (code_alignment, 0);
  gtk_widget_set_margin_bottom (code_alignment, 0);
  gtk_widget_set_margin_start (code_alignment, 12);
  gtk_widget_set_margin_end (code_alignment, 0);
  gtk_container_add (GTK_CONTAINER (vbox), code_alignment);

  /* Links box */
  code_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width (GTK_CONTAINER (code_box), 0);
  gtk_container_add (GTK_CONTAINER (code_alignment), code_box);

  /* HTML title */
  html_label = gtk_label_new (_("HTML"));
  gtk_widget_set_halign (html_label, GTK_ALIGN_START);
  gtk_widget_set_valign (html_label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (code_box), html_label);

  /* HTML frame */
  html_frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (html_frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (code_box), html_frame);

  /* HTML code text view */
  html_buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (html_buffer, html_code, -1);

  html_code_view = gtk_text_view_new_with_buffer (html_buffer);
  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (html_code_view),
                                 10);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (html_code_view),
                              FALSE);
  gtk_container_add (GTK_CONTAINER (html_frame), html_code_view);

  /* BB title */
  bb_label = gtk_label_new (_("BBCode for forums"));
  gtk_widget_set_halign (bb_label, GTK_ALIGN_START);
  gtk_widget_set_valign (bb_label, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (code_box), bb_label);

  /* BB frame */
  bb_frame = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (bb_frame), GTK_SHADOW_IN);
  gtk_container_add (GTK_CONTAINER (code_box), bb_frame);

  /* BBcode text view */
  bb_buffer = gtk_text_buffer_new (NULL);
  gtk_text_buffer_set_text (bb_buffer, bb_code, -1);

  bb_code_view = gtk_text_view_new_with_buffer (bb_buffer);
  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (bb_code_view),
                                 10);
  gtk_text_view_set_editable (GTK_TEXT_VIEW (bb_code_view),
                              FALSE);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (bb_code_view),
                               GTK_WRAP_CHAR);
  gtk_container_add (GTK_CONTAINER (bb_frame), bb_code_view);

  /* Show the dialog and run it */
  gtk_widget_show_all (gtk_dialog_get_content_area (GTK_DIALOG (dialog)));
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  g_object_unref (html_buffer);
  g_object_unref (bb_buffer);
}

