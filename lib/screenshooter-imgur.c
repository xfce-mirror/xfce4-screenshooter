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
#include "screenshooter-job-callbacks.h"
#include <string.h>
#include <stdlib.h>
#include <libsoup/soup.h>
#include <libxml/parser.h>

/* for v3 API - key registered *only* for xfce4-screenshooter! */
#define CLIENT_ID "66ab680b597e293"
#define HEADER_CLIENT_ID "Client-ID " CLIENT_ID

static gboolean          imgur_upload_job          (ScreenshooterJob  *job,
                                                    GArray            *param_values,
                                                    GError           **error);

static gboolean
imgur_upload_job (ScreenshooterJob *job, GArray *param_values, GError **error)
{
  const gchar *image_path, *title;
  guchar *online_file_name = NULL;
  guchar *delete_hash = NULL;
  const gchar *proxy_uri;
#ifdef DEBUG
  SoupLogger *log;
#endif
  SoupSession *session;
  SoupMessage *msg;
  GMappedFile *mapping;
  SoupMultipart *mp;
  xmlDoc *doc;
  xmlNode *root_node, *child_node;

#ifdef HAVE_SOUP3
  GUri *soup_proxy_uri;
  GBytes *buf, *response;
#else
  SoupURI *soup_proxy_uri;
  guint status;
  SoupBuffer *buf;
#endif

  const gchar *upload_url = "https://api.imgur.com/3/upload.xml";

  GError *tmp_error = NULL;

  g_return_val_if_fail (SCREENSHOOTER_IS_JOB (job), FALSE);
  g_return_val_if_fail (param_values != NULL, FALSE);
  g_return_val_if_fail (param_values->len == 2, FALSE);
  g_return_val_if_fail ((G_VALUE_HOLDS_STRING (&g_array_index(param_values, GValue, 0))), FALSE);
  g_return_val_if_fail ((G_VALUE_HOLDS_STRING (&g_array_index(param_values, GValue, 1))), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_object_set_data (G_OBJECT (job), "jobtype", "imgur");
  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return FALSE;


  image_path = g_value_get_string (&g_array_index (param_values, GValue, 0));
  title = g_value_get_string (&g_array_index (param_values, GValue, 1));

  session = soup_session_new ();
#ifdef DEBUG
#ifdef HAVE_SOUP3
  log = soup_logger_new (SOUP_LOGGER_LOG_HEADERS);
#else
  log = soup_logger_new (SOUP_LOGGER_LOG_HEADERS, -1);
#endif
  soup_session_add_feature (session, (SoupSessionFeature *)log);
#endif

  /* Set the proxy URI if any */
  proxy_uri = g_getenv ("http_proxy");

  if (proxy_uri != NULL)
    {
#ifdef HAVE_SOUP3
      soup_proxy_uri = g_uri_parse (proxy_uri, G_URI_FLAGS_NONE, NULL);
      g_object_set (session, "proxy-uri", soup_proxy_uri, NULL);
      g_uri_unref (soup_proxy_uri);
#else
      soup_proxy_uri = soup_uri_new (proxy_uri);
      g_object_set (session, "proxy-uri", soup_proxy_uri, NULL);
      soup_uri_free (soup_proxy_uri);
#endif
    }

  mapping = g_mapped_file_new (image_path, FALSE, NULL);
  if (!mapping) {
    g_object_unref (session);
    return FALSE;
  }

#ifdef HAVE_SOUP3
  buf = g_mapped_file_get_bytes (mapping);
#else
  buf = soup_buffer_new_with_owner (g_mapped_file_get_contents (mapping),
                                    g_mapped_file_get_length (mapping),
                                    mapping, (GDestroyNotify)g_mapped_file_unref);
#endif

  mp = soup_multipart_new (SOUP_FORM_MIME_TYPE_MULTIPART);
  soup_multipart_append_form_file (mp, "image", NULL, NULL, buf);
  soup_multipart_append_form_string (mp, "name", title);
  soup_multipart_append_form_string (mp, "title", title);

#ifdef HAVE_SOUP3
  msg = soup_message_new_from_multipart (upload_url, mp);
  soup_message_headers_append (soup_message_get_request_headers (msg), "Authorization", HEADER_CLIENT_ID);
#else
  msg = soup_form_request_new_from_multipart (upload_url, mp);
  soup_message_headers_append (msg->request_headers, "Authorization", HEADER_CLIENT_ID);
#endif

  exo_job_info_message (EXO_JOB (job), _("Upload the screenshot..."));

#ifdef HAVE_SOUP3
  response = soup_session_send_and_read (session, msg, NULL, &tmp_error);

  g_mapped_file_unref (mapping);
  g_bytes_unref (buf);
  g_object_unref (session);
  g_object_unref (msg);
#ifdef DEBUG
  g_object_unref (log);
#endif

  if (!response)
    {
      TRACE ("Error during the POST exchange: %s\n", tmp_error->message);
      g_propagate_error (error, tmp_error);
      return FALSE;
    }

  TRACE("response was %s\n", (gchar*) g_bytes_get_data (response, NULL));
  /* returned XML is like <data type="array" success="1" status="200"><id>xxxxxx</id> */
  doc = xmlParseMemory (g_bytes_get_data (response, NULL), g_bytes_get_size (response));
#else
  status = soup_session_send_message (session, msg);

  if (!SOUP_STATUS_IS_SUCCESSFUL (status))
    {
      TRACE ("Error during the POST exchange: %d %s\n",
             status, msg->reason_phrase);

      tmp_error = g_error_new (SOUP_HTTP_ERROR,
                         status,
                         _("An error occurred while transferring the data"
                           " to imgur."));
      g_propagate_error (error, tmp_error);
      g_object_unref (session);
      g_object_unref (msg);

      return FALSE;
    }

  TRACE("response was %s\n", msg->response_body->data);
  /* returned XML is like <data type="array" success="1" status="200"><id>xxxxxx</id> */
  doc = xmlParseMemory(msg->response_body->data, strlen(msg->response_body->data));
#endif

  root_node = xmlDocGetRootElement(doc);
  for (child_node = root_node->children; child_node; child_node = child_node->next)
  {
    if (xmlStrEqual(child_node->name, (const xmlChar *) "id"))
      online_file_name = xmlNodeGetContent(child_node);
    else if (xmlStrEqual (child_node->name, (const xmlChar *) "deletehash"))
      delete_hash = xmlNodeGetContent (child_node);
  }

  TRACE("found picture id %s\n", online_file_name);
  xmlFreeDoc(doc);

  screenshooter_job_image_uploaded (job,
                                    (const gchar*) online_file_name,
                                    (const gchar*) delete_hash);

#ifdef HAVE_SOUP3
  g_bytes_unref (response);
  g_free (online_file_name);
  g_free (delete_hash);
#else
  soup_buffer_free (buf);
  g_object_unref (session);
  g_object_unref (msg);
#endif

  return TRUE;
}


/* Public */



/**
 * screenshooter_upload_to_imgur:
 * @image_path: the local path of the image that should be uploaded to
 * imgur.com.
 *
 * Uploads the image whose path is @image_path
 *
 **/

gboolean screenshooter_upload_to_imgur   (const gchar  *image_path,
                                          const gchar  *title)
{
  ScreenshooterJob *job;
  GtkWidget *dialog, *label;

  g_return_val_if_fail (image_path != NULL, TRUE);

  dialog = create_spinner_dialog(_("Imgur"), &label);

  job = screenshooter_simple_job_launch (imgur_upload_job, 2,
                                          G_TYPE_STRING, image_path,
                                          G_TYPE_STRING, title);

  /* dismiss the spinner dialog after success or error */
  g_signal_connect_swapped (job, "error", G_CALLBACK (gtk_widget_hide), dialog);
  g_signal_connect_swapped (job, "image-uploaded", G_CALLBACK (gtk_widget_hide), dialog);

  g_signal_connect (job, "ask", G_CALLBACK (cb_ask_for_information), NULL);
  g_signal_connect (job, "image-uploaded", G_CALLBACK (cb_image_uploaded), NULL);
  g_signal_connect (job, "error", G_CALLBACK (cb_error), dialog);
  g_signal_connect (job, "finished", G_CALLBACK (cb_finished), dialog);
  g_signal_connect (job, "info-message", G_CALLBACK (cb_update_info), label);

  return gtk_dialog_run (GTK_DIALOG (dialog)) != DIALOG_RESPONSE_ERROR;
}



/**
 * screenshooter_imgur_client_id:
 *
 * Returns: the API client id, *only* for xfce4-screenshooter. Do not free it.
 **/
const char*
screenshooter_imgur_client_id (void)
{
  return CLIENT_ID;
}
