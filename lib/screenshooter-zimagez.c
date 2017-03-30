/*  $Id$
 *
 *  Copyright © 2009-2010 Jérôme Guelfucci <jeromeg@xfce.org>
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


/* XML-RPC API for ZimageZ */

/* URL of the API: http://www.zimagez.com/apiXml.php

   xmlrpcLogin: Takes the user name and the password (encrypted using rot13 and
   reversed using g_strrev.
   Returns a string containing the ID for the user session if the couple was correct.
   Returns a boolean set to FALSE if the couple was wrong.

   xmlrpcLogout: destroys the current user session.

   xmlrpcUpload: Takes the file content encoded in base64, the name of the file,
   the title of the picture, a comment and the user session ID.
   Returns the name of the file on the website if the upload was succesful.
   Returns a boolean set to FALSE if the upload failed.

   If the returned name is "wii0". The URLs will be:
   * http://www.zimagez.com/zimage/wii0.php for the image.
   * http://www.zimagez.com/miniature/wii0.jpg for the thumbnail.
   * http://www.zimagez.com/avatar/wii0.jpg for the avatar.

*/

#include "screenshooter-zimagez.h"
#include "screenshooter-job-callbacks.h"

static gboolean          do_xmlrpc                 (SoupSession       *session,
                                                    const gchar       *uri,
                                                    const gchar       *method,
                                                    GError           **error,
                                                    GValue            *retval,
                                                    ...);
static gboolean          has_empty_field           (GtkListStore      *liststore);
static gboolean          zimagez_upload_job        (ScreenshooterJob  *job,
                                                    GArray            *param_values,
                                                    GError           **error);


/* Private */



static gboolean
do_xmlrpc (SoupSession *session, const gchar *uri, const gchar *method,
           GError **error, GValue *retval, ...)
{
  SoupMessage *msg;
  va_list args;
  GArray *params = g_array_sized_new(FALSE, FALSE, sizeof(GValue), 1);
  GError *err = NULL;
  char *body;
  GType type;
  GValue val;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  va_start (args, retval);

  //copy soup_value_array_from_args() in here and change datatypes respectivly
  while ((type = va_arg (args, GType)) != G_TYPE_INVALID)
  {
    SOUP_VALUE_SETV (&val, type, args);
    g_array_append_val (params, val);
  }

  va_end (args);

  body = soup_xmlrpc_build_method_call (method, (GValue*)params->data,
                                   params->len);
  g_array_unref (params);

  if (!body)
    {
      err = g_error_new (SOUP_XMLRPC_FAULT,
                         SOUP_XMLRPC_FAULT_APPLICATION_ERROR,
                         _("An error occurred while creating the XMLRPC"
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
                         _("An error occurred while transferring the data"
                           " to ZimageZ."));
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
                             _("An error occurred while parsing the response"
                               " from ZimageZ."));
          g_propagate_error (error, err);
        }

      g_object_unref (msg);
      return FALSE;
    }

  g_object_unref (msg);

  return TRUE;
}



static gboolean
has_empty_field (GtkListStore *liststore)
{
  GtkTreeIter iter;
  gboolean result = FALSE;

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (liststore), &iter);

  do
    {
      gchar *field = NULL;

      gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter, 1, &field, -1);
      result = result || g_str_equal (field, "");

      g_free (field);

    }
  while (gtk_tree_model_iter_next (GTK_TREE_MODEL (liststore), &iter));

  return result;
}



static gboolean
zimagez_upload_job (ScreenshooterJob *job, GArray *param_values, GError **error)
{
  const gchar *encoded_data;
  const gchar *image_path;
  const gchar *last_user;
  const gchar *proxy_uri;
  /* For translators: the first wildcard is the date, the second one the time,
   * e.g. "Taken on 12/31/99, at 23:13:48". */
  gchar *comment = screenshooter_get_datetime (_("Taken on %x, at %X"));
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

  const gchar *serverurl = "http://www.zimagez.com/apiXml.php";
  const gchar *method_login = "apiXml.xmlrpcLogin";
  const gchar *method_logout = "apiXml.xmlrpcLogout";
  const gchar *method_upload = "apiXml.xmlrpcUpload";
  SoupSession *session;
  SoupURI *soup_proxy_uri;

  GError *tmp_error = NULL;
  GtkTreeIter iter;
  GtkListStore *liststore;
  GValue response_value;

  g_return_val_if_fail (SCREENSHOOTER_IS_JOB (job), FALSE);
  g_return_val_if_fail (param_values != NULL, FALSE);
  g_return_val_if_fail (param_values->len == 3, FALSE);
  g_return_val_if_fail ((G_VALUE_HOLDS_STRING (g_array_index (param_values, GValue*, 0))), FALSE);
  g_return_val_if_fail ((G_VALUE_HOLDS_STRING (g_array_index (param_values, GValue*, 1))), FALSE);
  g_return_val_if_fail ((G_VALUE_HOLDS_STRING (g_array_index (param_values, GValue*, 2))), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  g_object_set_data (G_OBJECT (job), "jobtype", "zimagez");
  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    {
      g_free (comment);
      g_free (password);

      return FALSE;
    }

  /* Get the last user */
  last_user = g_value_get_string (g_array_index (param_values, GValue*, 1));
  user = g_strdup (last_user);

  if (user == NULL)
    user = g_strdup ("");

  if (!g_utf8_validate (user, -1, NULL))
    {
      g_free (user);
      user = g_strdup ("");
    }

  g_object_set_data_full (G_OBJECT (job), "user",
                          g_strdup (user), (GDestroyNotify) g_free);

  /* Get the default title */
  title = g_strdup (g_value_get_string (g_array_index (param_values, GValue*, 2)));
  if (title == NULL)
    title = g_strdup ("");

  if (!g_utf8_validate (title, -1, NULL))
    {
      g_free (title);
      title = g_strdup ("");
    }

  /* Get the path of the image that is to be uploaded */
  image_path = g_value_get_string (g_array_index (param_values, GValue*, 0));

  /* Start the user soup session */
  exo_job_info_message (EXO_JOB (job), _("Initialize the connection..."));
  session = soup_session_sync_new ();

  /* Set the proxy URI if any */
  proxy_uri = g_getenv ("http_proxy");

  if (proxy_uri != NULL)
    {
      soup_proxy_uri = soup_uri_new (proxy_uri);
      g_object_set (session, "proxy-uri", soup_proxy_uri, NULL);
      soup_uri_free (soup_proxy_uri);
    }

  TRACE ("Get the information liststore ready.");
  liststore = gtk_list_store_new (2, G_TYPE_INT, G_TYPE_STRING);

  TRACE ("Append the user");
  gtk_list_store_append (liststore, &iter);
  gtk_list_store_set (liststore, &iter,
                      0, USER,
                      1, user,
                      -1);

  TRACE ("Append the password");
  gtk_list_store_append (liststore, &iter);
  gtk_list_store_set (liststore, &iter,
                      0, PASSWORD,
                      1, password,
                      -1);

  TRACE ("Append the title");
  gtk_list_store_append (liststore, &iter);
  gtk_list_store_set (liststore, &iter,
                      0, TITLE,
                      1, title,
                      -1);

  TRACE ("Append the comment");
  gtk_list_store_append (liststore, &iter);
  gtk_list_store_set (liststore, &iter,
                      0, COMMENT,
                      1, comment,
                      -1);

  TRACE ("Ask the user to fill the information items.");
  screenshooter_job_ask_info (job, liststore,
                              _("Please fill the following fields with your "
                                "<a href=\"http://www.zimagez.com\">ZimageZ</a> \n"
                                "user name, passsword and details about the screenshot."));

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
            user = g_strdup (field_value);
            break;
          case PASSWORD:
            password = g_strdup (field_value);
            break;
          case TITLE:
            title = g_strdup (field_value);
            break;
          case COMMENT:
            comment = g_strdup (field_value);
            break;
          default:
            break;
        }

      g_free (field_value);
    }
  while (gtk_tree_model_iter_next (GTK_TREE_MODEL (liststore), &iter));

  while (!response)
    {
      if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
        {
          soup_session_abort (session);
          g_object_unref (session);

          g_free (user);
          g_free (password);
          g_free (title);
          g_free (comment);
          if (encoded_password != NULL)
            g_free (encoded_password);

          TRACE ("The upload job was cancelled.");

          return FALSE;
        }

      exo_job_info_message (EXO_JOB (job), _("Check the user information..."));

      /* Test if one of the information fields is empty */
      if (has_empty_field (liststore))
        {
          TRACE ("One of the fields was empty, let the user file it.");
          screenshooter_job_ask_info (job, liststore,
                                      _("<span weight=\"bold\" foreground=\"darkred\" "
                                        "stretch=\"semiexpanded\">You must fill all the "
                                        "fields.</span>"));
          continue;
        }

      encoded_password = g_utf8_strreverse (rot13 (password), -1);

      TRACE ("User: %s", user);
      TRACE ("Encoded password: %s", encoded_password);

      /* Start the user session */
      TRACE ("Call the login method");

      exo_job_info_message (EXO_JOB (job), _("Login on ZimageZ..."));

      if (!do_xmlrpc (session, serverurl, method_login,
                      &tmp_error, &response_value,
                      G_TYPE_STRING, user,
                      G_TYPE_STRING, encoded_password,
                      G_TYPE_INVALID))
        {
          g_propagate_error (error, tmp_error);
          soup_session_abort (session);
          g_object_unref (session);

          g_free (password);
          g_free (title);
          g_free (comment);
          g_free (encoded_password);

          return FALSE;
        }

      TRACE ("Read the login response");

      /* If the response is a boolean, there was an error */
      if (G_VALUE_HOLDS_BOOLEAN (&response_value))
        {
          response = g_value_get_boolean (&response_value);
        }
      /* Else we read the string response to get the session ID */
      else if (G_VALUE_HOLDS_STRING (&response_value))
        {
          TRACE ("Read the session ID");
          login_response = g_strdup (g_value_get_string (&response_value));
          response = TRUE;
        }
      /* We received an unexpected reply */
      else
        {
          GError *tmp_err =
            g_error_new (SOUP_XMLRPC_FAULT,
                         SOUP_XMLRPC_FAULT_PARSE_ERROR_NOT_WELL_FORMED,
                         "%s", _("An unexpected reply from ZimageZ was received."
                                 " The upload of the screenshot failed."));
          soup_session_abort (session);
          g_object_unref (session);

          g_free (user);
          g_free (password);
          g_free (title);
          g_free (comment);
          g_free (encoded_password);

          g_propagate_error (error, tmp_err);

          return FALSE;
        }

      g_value_unset (&response_value);

      if (!response)
        {
          /* Login failed, erase the password and ask for the correct on to the
             user */
          gtk_tree_model_get_iter_first (GTK_TREE_MODEL (liststore), &iter);

          do
            {
              gint field_index;

              gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter, 0, &field_index, -1);

              if (field_index == PASSWORD)
                {
                  gtk_list_store_set (liststore, &iter, 1, g_strdup (""), -1);
                  break;
                }
            }
          while (gtk_tree_model_iter_next (GTK_TREE_MODEL (liststore), &iter));

          screenshooter_job_ask_info (job, liststore,
                                      _("<span weight=\"bold\" foreground=\"darkred\" "
                                        "stretch=\"semiexpanded\">The user and the "
                                        "password you entered do not match. "
                                        "Please retry.</span>"));

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
                    user = g_strdup (field_value);
                    break;
                  case PASSWORD:
                    password = g_strdup (field_value);
                    break;
                  case TITLE:
                    title = g_strdup (field_value);
                    break;
                  case COMMENT:
                    comment = g_strdup (field_value);
                    break;
                  default:
                    break;
                }

              g_free (field_value);
            }
          while (gtk_tree_model_iter_next (GTK_TREE_MODEL (liststore), &iter));
        }
    }

  g_object_set_data_full (G_OBJECT (job), "user",
                          g_strdup (user), (GDestroyNotify) g_free);

  g_free (user);
  g_free (password);
  g_free (encoded_password);

  /* Get the contents of the image file and encode it to base64 */
  g_file_get_contents (image_path, &data, &data_length, NULL);

  encoded_data = g_base64_encode ((guchar*)data, data_length);

  g_free (data);

  /* Get the basename of the image path */
  file_name = g_path_get_basename (image_path);

  exo_job_info_message (EXO_JOB (job), _("Upload the screenshot..."));

  TRACE ("Call the upload method");
  do_xmlrpc (session, serverurl, method_upload,
             &tmp_error, &response_value,
             G_TYPE_STRING, encoded_data,
             G_TYPE_STRING, file_name,
             G_TYPE_STRING, title,
             G_TYPE_STRING, comment,
             G_TYPE_STRING, login_response,
             G_TYPE_INVALID);

  g_free (title);
  g_free (comment);
  g_free (file_name);

  if (tmp_error)
    {
      soup_session_abort (session);
      g_object_unref (session);

      g_propagate_error (error, tmp_error);

      return FALSE;
    }

  /* If the response is a boolean, there was an error */
  if (G_VALUE_HOLDS_BOOLEAN (&response_value))
    {
      if (!g_value_get_boolean (&response_value))
        {
          GError *tmp_err =
            g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                         _("An error occurred while uploading the screenshot."));

          soup_session_abort (session);
          g_object_unref (session);
          g_propagate_error (error, tmp_err);

          return FALSE;
        }
    }
  /* Else we get the file name */
  else if (G_VALUE_HOLDS_STRING (&response_value))
    {
      TRACE ("The screenshot has been uploaded, get the file name.");
      online_file_name = g_strdup (g_value_get_string (&response_value));
    }
  /* We received un unexpected reply */
  else
    {
      GError *tmp_err =
        g_error_new (SOUP_XMLRPC_FAULT,
                     SOUP_XMLRPC_FAULT_PARSE_ERROR_NOT_WELL_FORMED,
                     "%s", _("An unexpected reply from ZimageZ was received."
                       " The upload of the screenshot failed."));
      soup_session_abort (session);
      g_object_unref (session);
      g_propagate_error (error, tmp_err);

      return FALSE;
    }

  g_value_unset (&response_value);

  /* End the user session */
  exo_job_info_message (EXO_JOB (job), _("Close the session on ZimageZ..."));

  TRACE ("Closing the user session");

  do_xmlrpc (session, serverurl, method_logout,
             &tmp_error, &response_value,
             G_TYPE_STRING, login_response,
             G_TYPE_INVALID);

  if (G_IS_VALUE (&response_value))
    g_value_unset (&response_value);

  /* Clean the soup session */
  soup_session_abort (session);
  g_object_unref (session);
  g_free (login_response);

  screenshooter_job_image_uploaded (job, online_file_name);

  if (tmp_error)
    {
      g_propagate_error (error, tmp_error);

      return FALSE;
    }

  return TRUE;
}

/* Public */

/**
 * screenshooter_upload_to_zimagez:
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

void screenshooter_upload_to_zimagez (const gchar  *image_path,
                                      const gchar  *last_user,
                                      const gchar  *title,
                                      gchar       **new_last_user)
{
  ScreenshooterJob *job;
  GtkWidget *dialog, *label;

  g_return_if_fail (image_path != NULL);
  g_return_if_fail (new_last_user == NULL || *new_last_user == NULL);

  dialog = create_spinner_dialog(_("ZimageZ"), &label);

  job = screenshooter_simple_job_launch (zimagez_upload_job, 3,
                                          G_TYPE_STRING, image_path,
                                          G_TYPE_STRING, last_user,
                                          G_TYPE_STRING, title);

  g_signal_connect (job, "ask", G_CALLBACK (cb_ask_for_information), NULL);
  g_signal_connect (job, "image-uploaded", G_CALLBACK (cb_image_uploaded), new_last_user);
  g_signal_connect (job, "error", G_CALLBACK (cb_error), NULL);
  g_signal_connect (job, "finished", G_CALLBACK (cb_finished), dialog);
  g_signal_connect (job, "info-message", G_CALLBACK (cb_update_info), label);

  gtk_dialog_run (GTK_DIALOG (dialog));
}
