/*  $Id$
 *
 *  Copyright © 2009-2010 Sebastian Waisbrot <seppo0010@gmail.com>
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


#include "screenshooter-imgur.h"
#include <string.h>
#include <stdlib.h>
#include <libsoup/soup.h>
#include <libxml/parser.h>

typedef enum
{
  USER,
  PASSWORD,
  TITLE,
  COMMENT,
} ZimagezInformation;

static ScreenshooterJob *imgur_upload_to_imgur     (const gchar       *file_name,
                                                    const gchar       *last_user,
                                                    const gchar       *title);
static gboolean          imgur_upload_job          (ScreenshooterJob  *job,
                                                    GValueArray       *param_values,
                                                    GError           **error);
static void              cb_image_uploaded         (ScreenshooterJob  *job,
                                                    gchar             *upload_name,
                                                    gchar            **last_user);
static void              cb_error                  (ExoJob            *job,
                                                    GError            *error,
                                                    gpointer           unused);
static void              cb_finished               (ExoJob            *job,
                                                    GtkWidget         *dialog);
static void              cb_update_info            (ExoJob            *job,
                                                    gchar             *message,
                                                    GtkWidget         *label);

static gboolean
imgur_upload_job (ScreenshooterJob *job, GValueArray *param_values, GError **error)
{
  const gchar *image_path;
  gchar *online_file_name = NULL;
  gchar* proxy_uri;
  SoupURI *soup_proxy_uri;
#if DEBUG > 0
  SoupLogger *log;
#endif
  guint status;
  SoupSession *session;
  SoupMessage *msg;
  SoupBuffer *buf;
  GMappedFile *mapping;
  SoupMultipart *mp;
  xmlDoc *doc;
  xmlNode *root_node, *child_node;

  const gchar *upload_url = "https://api.imgur.com/3/upload.xml";

  GError *tmp_error = NULL;

  g_return_val_if_fail (SCREENSHOOTER_IS_JOB (job), FALSE);
  g_return_val_if_fail (param_values != NULL, FALSE);
  g_return_val_if_fail (param_values->n_values == 3, FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_STRING (&param_values->values[0]), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_STRING (&param_values->values[1]), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_STRING (&param_values->values[2]), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return FALSE;


  image_path = g_value_get_string (g_value_array_get_nth (param_values, 0));

  session = soup_session_sync_new ();
#if DEBUG > 0
  log = soup_logger_new (SOUP_LOGGER_LOG_HEADERS, -1);
  soup_session_add_feature (session, (SoupSessionFeature *)log);
#endif

  /* Set the proxy URI if any */
  proxy_uri = g_getenv ("http_proxy");

  if (proxy_uri != NULL)
    {
      soup_proxy_uri = soup_uri_new (proxy_uri);
      g_object_set (session, "proxy-uri", soup_proxy_uri, NULL);
      soup_uri_free (soup_proxy_uri);
    }

  mapping = g_mapped_file_new(image_path, FALSE, NULL);
  if (!mapping) {
    g_object_unref (session);

    return FALSE;
  }

  mp = soup_multipart_new(SOUP_FORM_MIME_TYPE_MULTIPART);
  buf = soup_buffer_new_with_owner (g_mapped_file_get_contents (mapping),
                                    g_mapped_file_get_length (mapping),
                                    mapping, (GDestroyNotify)g_mapped_file_unref);

  soup_multipart_append_form_file (mp, "image", NULL, NULL, buf);
  soup_multipart_append_form_string (mp, "name", "Screenshot");
  soup_multipart_append_form_string (mp, "title", "Screenshot");
  msg = soup_form_request_new_from_multipart (upload_url, mp);

  // for v3 API - key registered *only* for xfce4-screenshooter!
  soup_message_headers_append (msg->request_headers, "Authorization", "Client-ID 66ab680b597e293");
  status = soup_session_send_message (session, msg);

  if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code))
    {
      TRACE ("Error during the POST exchange: %d %s\n",
             msg->status_code, msg->reason_phrase);

      tmp_error = g_error_new (SOUP_HTTP_ERROR,
                         msg->status_code,
                         _("An error occurred when transferring the data"
                           " to imgur."));
      g_propagate_error (error, tmp_error);
      g_object_unref (session);
      g_object_unref (msg);

      return FALSE;
    }

  TRACE("response was %s\n", msg->response_body->data);
  /* returned XML is like <data type="array" success="1" status="200"><id>xxxxxx</id> */
  doc = xmlParseMemory(msg->response_body->data,
                                  strlen(msg->response_body->data));

  root_node = xmlDocGetRootElement(doc);
  for (child_node = root_node->children; child_node; child_node = child_node->next)
    if (xmlStrEqual(child_node->name, (const xmlChar *) "id"))
       online_file_name = xmlNodeGetContent(child_node);
  TRACE("found picture id %s\n", online_file_name);
  xmlFreeDoc(doc);
  soup_buffer_free (buf);
  g_object_unref (session);
  g_object_unref (msg);

  screenshooter_job_image_uploaded (job, online_file_name);

  return TRUE;
}



static ScreenshooterJob
*imgur_upload_to_imgur (const gchar *file_path,
                            const gchar *last_user,
                            const gchar *title)
{
  g_return_val_if_fail (file_path != NULL, NULL);

  return screenshooter_simple_job_launch (imgur_upload_job, 3,
                                          G_TYPE_STRING, file_path,
                                          G_TYPE_STRING, last_user,
                                          G_TYPE_STRING, title);
}



static void cb_image_uploaded (ScreenshooterJob  *job,
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

  gchar *last_user_temp;

  g_return_if_fail (upload_name != NULL);
  g_return_if_fail (last_user == NULL || *last_user == NULL);

  image_url = g_strdup_printf ("http://i.imgur.com/%s.png", upload_name);
  thumbnail_url =
    g_strdup_printf ("http://imgur.com/%ss.png", upload_name);
  small_thumbnail_url =
    g_strdup_printf ("http://imgur.com/%s1.png", upload_name);
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

  last_user_temp = g_object_get_data (G_OBJECT (job), "user");

  if (last_user_temp == NULL)
    last_user_temp = g_strdup ("");

  *last_user = g_strdup (last_user_temp);

  /* Dialog */
  dialog =
    xfce_titled_dialog_new_with_buttons (_("My screenshot on Imgur"),
                                         NULL,
                                         GTK_DIALOG_NO_SEPARATOR,
                                         GTK_STOCK_CLOSE,
                                         GTK_RESPONSE_CLOSE,
                                         NULL);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), 0);
  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 12);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "applications-internet");
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  /* Create the main alignment for the dialog */
  main_alignment = gtk_alignment_new (0, 0, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (main_alignment), 6, 0, 10, 10);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), main_alignment, TRUE, TRUE, 0);

  /* Create the main box for the dialog */
  vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (main_alignment), vbox);

  /* Links bold label */
  link_label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (link_label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">"
                          "Links</span>"));
  gtk_misc_set_alignment (GTK_MISC (link_label), 0, 0);
  gtk_container_add (GTK_CONTAINER (vbox), link_label);

  /* Links alignment */
  links_alignment = gtk_alignment_new (0, 0, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (links_alignment), 0, 0, 12, 0);
  gtk_container_add (GTK_CONTAINER (vbox), links_alignment);

  /* Links box */
  links_box = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (links_box), 0);
  gtk_container_add (GTK_CONTAINER (links_alignment), links_box);

  /* Create the image link */
  image_link = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (image_link), image_markup);
  gtk_misc_set_alignment (GTK_MISC (image_link), 0, 0);
  gtk_widget_set_tooltip_text (image_link, image_url);
  gtk_container_add (GTK_CONTAINER (links_box), image_link);

  /* Create the thumbnail link */
  thumbnail_link = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (thumbnail_link), thumbnail_markup);
  gtk_misc_set_alignment (GTK_MISC (thumbnail_link), 0, 0);
  gtk_widget_set_tooltip_text (thumbnail_link, thumbnail_url);
  gtk_container_add (GTK_CONTAINER (links_box), thumbnail_link);

  /* Create the small thumbnail link */
  small_thumbnail_link = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (small_thumbnail_link), small_thumbnail_markup);
  gtk_misc_set_alignment (GTK_MISC (small_thumbnail_link), 0, 0);
  gtk_widget_set_tooltip_text (small_thumbnail_link, small_thumbnail_url);
  gtk_container_add (GTK_CONTAINER (links_box), small_thumbnail_link);

  /* Examples bold label */
  example_label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (example_label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">"
                          "Code for a thumbnail pointing to the full size image</span>"));
  gtk_misc_set_alignment (GTK_MISC (example_label), 0, 0);
  gtk_container_add (GTK_CONTAINER (vbox), example_label);

  /* Code alignment */
  code_alignment = gtk_alignment_new (0, 0, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (code_alignment), 0, 0, 12, 0);
  gtk_container_add (GTK_CONTAINER (vbox), code_alignment);

  /* Links box */
  code_box = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (code_box), 0);
  gtk_container_add (GTK_CONTAINER (code_alignment), code_box);

  /* HTML title */
  html_label = gtk_label_new (_("HTML"));
  gtk_misc_set_alignment (GTK_MISC (html_label), 0, 0);
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
  gtk_misc_set_alignment (GTK_MISC (bb_label), 0, 0);
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
  gtk_container_add (GTK_CONTAINER (bb_frame), bb_code_view);

  /* Show the dialog and run it */
  gtk_widget_show_all (GTK_DIALOG(dialog)->vbox);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  g_object_unref (html_buffer);
  g_object_unref (bb_buffer);
}



static void cb_error (ExoJob *job, GError *error, gpointer unused)
{
  g_return_if_fail (error != NULL);

  screenshooter_error ("%s", error->message);
}



static void cb_finished (ExoJob *job, GtkWidget *dialog)
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



static void cb_update_info (ExoJob *job, gchar *message, GtkWidget *label)
{
  g_return_if_fail (EXO_IS_JOB (job));
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_set_text (GTK_LABEL (label), message);
}



/* Public */



/**
 * screenshooter_upload_to_imgur:
 * @image_path: the local path of the image that should be uploaded to
 * ZimageZ.com.
 * @last_user: the last user name used, to pre-fill the user field.
 * @title: a default title, to pre-fill the title field.
 * @new_last_user: address of the string used to store the new user
 * if the upload is succesful.
 *
 * Uploads the image whose path is @image_path: a dialog asks for the user
 * login, password, a title for the image and a comment; then the image is
 * uploaded. The dialog is shown again with a warning is the password did
 * not match the user name. The user can also cancel the upload procedure.
 *
 * If the upload was succesful, @new_last_user points to the user name for
 * which the upload was done.
 *
 **/

void screenshooter_upload_to_imgur   (const gchar  *image_path,
                                      const gchar  *last_user,
                                      const gchar  *title,
                                      gchar       **new_last_user)
{
  ScreenshooterJob *job;
  GtkWidget *dialog;
  GtkWidget *label, *status_label;
  GtkWidget *hbox, *throbber;
  GtkWidget *main_box, *main_alignment;

  g_return_if_fail (image_path != NULL);
  g_return_if_fail (new_last_user == NULL || *new_last_user == NULL);

  dialog =
    gtk_dialog_new_with_buttons (_("Imgur"),
                                 NULL,
                                 GTK_DIALOG_NO_SEPARATOR,
                                 NULL);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG (dialog)->vbox), 0);
  gtk_window_set_deletable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "gtk-info");

  /* Create the main alignment for the dialog */
  main_alignment = gtk_alignment_new (0, 0, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (main_alignment), 0, 0, 6, 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), main_alignment, TRUE, TRUE, 0);

  /* Create the main box for the dialog */
  main_box = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (main_box), 12);
  gtk_container_add (GTK_CONTAINER (main_alignment), main_box);

  /* Top horizontal box for the throbber */
  hbox= gtk_hbox_new (FALSE, 0);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 0);
  gtk_container_add (GTK_CONTAINER (main_box), hbox);

  /* Add the throbber */
  throbber = katze_throbber_new ();
  katze_throbber_set_animated (KATZE_THROBBER (throbber), TRUE);
  gtk_box_pack_end (GTK_BOX (hbox), throbber, FALSE, FALSE, 0);

  /* Status label*/
  status_label = gtk_label_new ("");
  gtk_label_set_markup (GTK_LABEL (status_label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">"
                          "Status</span>"));
  gtk_misc_set_alignment (GTK_MISC (status_label), 0, 0);
  gtk_box_pack_start (GTK_BOX (hbox), status_label, FALSE, FALSE, 0);

  /* Information label */
  label = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (main_box), label);

  gtk_widget_show_all (GTK_DIALOG(dialog)->vbox);

  job = imgur_upload_to_imgur (image_path, last_user, title);

  g_signal_connect (job, "image-uploaded", G_CALLBACK (cb_image_uploaded), new_last_user);
  g_signal_connect (job, "error", G_CALLBACK (cb_error), NULL);
  g_signal_connect (job, "finished", G_CALLBACK (cb_finished), dialog);
  g_signal_connect (job, "info-message", G_CALLBACK (cb_update_info), label);

  gtk_dialog_run (GTK_DIALOG (dialog));
}
