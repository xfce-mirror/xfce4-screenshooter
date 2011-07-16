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
#include <curl/curl.h>
#include <json-glib/json-glib.h>
#include <json-glib/json-gobject.h>

typedef enum
{
  USER,
  PASSWORD,
  TITLE,
  COMMENT,
} ZimagezInformation;

static void              open_url_hook             (SexyUrlLabel      *url_label,
                                                    gchar             *url,
                                                    gpointer           user_data);
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



/* Private */


struct MemoryStruct {
  char *memory;
  size_t size;
};
 
 
static size_t WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)data;

	mem->memory = realloc(mem->memory, mem->size + realsize + 1);
	if (mem->memory == NULL) {
		printf("not enough memory (realloc returned NULL)\n");
		exit(0);
	}

	memcpy(&(mem->memory[mem->size]), ptr, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

static char *get_image_hash(char *json)
{
	JsonParser *parser;
	JsonNode *root;
	GError *error;

	g_type_init ();

	parser = json_parser_new ();

	error = NULL;
	json_parser_load_from_data(parser, json, strlen(json), &error);
	gchar *ret = NULL;

	if (error)
	{
		g_error_free (error);
		g_object_unref (parser);
		return NULL;
	}

	root = json_parser_get_root (parser);
	if (JSON_NODE_TYPE(root) == JSON_NODE_OBJECT) {
		JsonNode *upload = json_object_get_member(json_node_get_object(root), "upload");
		if (JSON_NODE_TYPE(upload) == JSON_NODE_OBJECT) {
			JsonNode *image = json_object_get_member(json_node_get_object(upload), "image");
			if (JSON_NODE_TYPE(image) == JSON_NODE_OBJECT) {
				JsonNode *hash = json_object_get_member(json_node_get_object(image), "hash");
				if (JSON_NODE_TYPE(hash) == JSON_NODE_VALUE) {
					ret = json_node_dup_string(hash);
				}
			}
		}
	}

	g_object_unref (parser);

	return ret;
}

static void
open_url_hook (SexyUrlLabel *url_label, gchar *url, gpointer user_data)
{
  const gchar *command = g_strconcat ("xdg-open ", url, NULL);
  GError *error = NULL;

  if (!g_spawn_command_line_async (command, &error))
    {
      TRACE ("An error occurred when opening the URL");

      screenshooter_error ("%s", error->message);
      g_error_free (error);
    }
}



static gboolean
do_xmlrpc (SoupSession *session, const gchar *uri, const gchar *method,
           GError **error, GValue *retval, ...)
{
  SoupMessage *msg;
  va_list args;
  GValueArray *params;
  GError *err = NULL;
  char *body;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  va_start (args, retval);
  params = soup_value_array_from_args (args);
  va_end (args);

  body =
    soup_xmlrpc_build_method_call (method, params->values,
                                   params->n_values);
  g_value_array_free (params);

  if (!body)
    {
      err = g_error_new (SOUP_XMLRPC_FAULT,
                         SOUP_XMLRPC_FAULT_APPLICATION_ERROR,
                         _("An error occurred when creating the XMLRPC"
                           " request."));
      g_propagate_error (error, err);

      return FALSE;
    }

  msg = soup_message_new ("POST", uri);
  soup_message_set_request (msg, "text/xml", SOUP_MEMORY_TAKE,
                            body, strlen (body));
  soup_session_send_message (session, msg);

  if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code))
    {
      TRACE ("Error during the XMLRPC exchange: %d %s\n",
             msg->status_code, msg->reason_phrase);

      err = g_error_new (SOUP_XMLRPC_FAULT,
                         SOUP_XMLRPC_FAULT_TRANSPORT_ERROR,
                         _("An error occurred when transferring the data"
                           " to Imgur."));
      g_propagate_error (error, err);
      g_object_unref (msg);

      return FALSE;
    }

  if (!soup_xmlrpc_parse_method_response (msg->response_body->data,
                                          msg->response_body->length,
                                          retval, &err))
    {
      if (err)
        {
          TRACE ("Fault when parsing the response: %d %s\n",
                 err->code, err->message);

          g_propagate_error (error, err);
        }
      else
        {
          TRACE ("Unable to parse the response, and no error...");

          err = g_error_new (SOUP_XMLRPC_FAULT,
                             SOUP_XMLRPC_FAULT_APPLICATION_ERROR,
                             _("An error occurred when parsing the response"
                               " from Imgur."));
          g_propagate_error (error, err);
        }

      g_object_unref (msg);
      return FALSE;
    }

  g_object_unref (msg);

  return TRUE;
}

static gboolean
imgur_upload_job (ScreenshooterJob *job, GValueArray *param_values, GError **error)
{
  const gchar *encoded_data;
  const gchar *image_path;
  const gchar *last_user;
  const gchar *date = screenshooter_get_date (FALSE);
  const gchar *current_time = screenshooter_get_time ();
  const gchar *proxy_uri;
  /* For translators: the first wildcard is the date, the second one the time,
   * e.g. "Taken on 12/31/99, at 23:13:48". */
  gchar *comment = g_strdup_printf (_("Taken on %s, at %s"), date, current_time);
  gchar *data = NULL;
  gchar *encoded_password = NULL;
  gchar *file_name = NULL;
  gchar *login_response = NULL;
  gchar *online_file_name = NULL;
  gchar *password = g_strdup ("");
  gchar *title;
  gchar *user;

  gsize data_length;
  gboolean response = FALSE;

  const gchar *upload_url = "http://api.imgur.com/2/upload.json";

  GError *tmp_error = NULL;
  GtkTreeIter iter;
  GtkListStore *liststore;
  GValue response_value;

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

	CURL *curl;
	CURLcode res;

	curl = curl_easy_init();
	if(curl) {
		struct MemoryStruct chunk;
		chunk.memory = malloc(1);  /* will be grown as needed by the realloc above */ 
		chunk.size = 0;    /* no data at this point */ 

		struct curl_httppost *formpost=NULL;
		struct curl_httppost *lastptr=NULL;

		curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME, "image",
			CURLFORM_FILE, image_path,
			CURLFORM_END);

		curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME, "title",
			CURLFORM_COPYCONTENTS, "Screenshot",
			CURLFORM_END);

		curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME, "name",
			CURLFORM_COPYCONTENTS, "Screenshot",
			CURLFORM_END);

		curl_formadd(&formpost,
			&lastptr,
			CURLFORM_COPYNAME, "key",
			CURLFORM_COPYCONTENTS, "a094536e9503bf5e289b65a8116a8d1c",
			CURLFORM_END);


		curl_easy_setopt(curl, CURLOPT_URL, upload_url);

		curl_easy_setopt(curl, CURLOPT_HTTPPOST, formpost);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);


		res = curl_easy_perform(curl);

		curl_formfree(formpost);
		curl_easy_cleanup(curl);

		online_file_name = get_image_hash(chunk.memory);

		if(chunk.memory)
			free(chunk.memory);

		curl_global_cleanup();
	} else {
		fprintf(stderr, "Unable to init curl\n");
	}

  screenshooter_job_image_uploaded (job, online_file_name);

  if (tmp_error)
    {
      g_propagate_error (error, tmp_error);

      return FALSE;
    }

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
    g_strdup_printf (_("<a href=\"%s\">Full size image</a>"), image_url);
  thumbnail_markup =
    g_strdup_printf (_("<a href=\"%s\">Large thumbnail</a>"), thumbnail_url);
  small_thumbnail_markup =
    g_strdup_printf (_("<a href=\"%s\">Small thumbnail</a>"), small_thumbnail_url);
  html_code =
    g_strdup_printf ("<a href=\"%s\">\n  <img src=\"%s\" />\n</a>",
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
  image_link = sexy_url_label_new ();
  sexy_url_label_set_markup (SEXY_URL_LABEL (image_link), image_markup);
  gtk_misc_set_alignment (GTK_MISC (image_link), 0, 0);
  g_signal_connect (G_OBJECT (image_link), "url-activated",
                    G_CALLBACK (open_url_hook), NULL);
  gtk_widget_set_tooltip_text (image_link, image_url);
  gtk_container_add (GTK_CONTAINER (links_box), image_link);

  /* Create the thumbnail link */
  thumbnail_link = sexy_url_label_new ();
  sexy_url_label_set_markup (SEXY_URL_LABEL (thumbnail_link), thumbnail_markup);
  gtk_misc_set_alignment (GTK_MISC (thumbnail_link), 0, 0);
  g_signal_connect (G_OBJECT (thumbnail_link), "url-activated",
                    G_CALLBACK (open_url_hook), NULL);
  gtk_widget_set_tooltip_text (thumbnail_link, thumbnail_url);
  gtk_container_add (GTK_CONTAINER (links_box), thumbnail_link);

  /* Create the small thumbnail link */
  small_thumbnail_link = sexy_url_label_new ();
  sexy_url_label_set_markup (SEXY_URL_LABEL (small_thumbnail_link), small_thumbnail_markup);
  gtk_misc_set_alignment (GTK_MISC (small_thumbnail_link), 0, 0);
  g_signal_connect (G_OBJECT (small_thumbnail_link), "url-activated",
                    G_CALLBACK (open_url_hook), NULL);
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
