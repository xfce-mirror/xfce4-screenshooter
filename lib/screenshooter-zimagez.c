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
static gboolean          do_xmlrpc                 (SoupSession       *session,
                                                    const gchar       *uri,
                                                    const gchar       *method,
                                                    GError           **error,
                                                    GValue            *retval,
                                                    ...);
static gboolean          has_empty_field           (GtkListStore      *liststore);
static ScreenshooterJob *zimagez_upload_to_zimagez (const gchar       *file_name,
                                                    const gchar       *last_user,
                                                    const gchar       *title);
static gboolean          zimagez_upload_job        (ScreenshooterJob  *job,
                                                    GValueArray       *param_values,
                                                    GError           **error);
static void              cb_ask_for_information    (ScreenshooterJob  *job,
                                                    GtkListStore      *liststore,
                                                    const gchar       *message,
                                                    gpointer           unused);
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
                             _("An error occurred when parsing the response"
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
zimagez_upload_job (ScreenshooterJob *job, GValueArray *param_values, GError **error)
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
  g_return_val_if_fail (param_values->n_values == 3, FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_STRING (&param_values->values[0]), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_STRING (&param_values->values[1]), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_STRING (&param_values->values[2]), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return FALSE;

  /* Get the last user */
  last_user = g_value_get_string (g_value_array_get_nth (param_values, 1));
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
  title = g_strdup (g_value_get_string (g_value_array_get_nth (param_values, 2)));
  if (title == NULL)
    title = g_strdup ("");

  if (!g_utf8_validate (title, -1, NULL))
    {
      g_free (title);
      title = g_strdup ("");
    }

  /* Get the path of the image that is to be uploaded */
  image_path = g_value_get_string (g_value_array_get_nth (param_values, 0));

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



static ScreenshooterJob
*zimagez_upload_to_zimagez (const gchar *file_path,
                            const gchar *last_user,
                            const gchar *title)
{
  g_return_val_if_fail (file_path != NULL, NULL);

  return screenshooter_simple_job_launch (zimagez_upload_job, 3,
                                          G_TYPE_STRING, file_path,
                                          G_TYPE_STRING, last_user,
                                          G_TYPE_STRING, title);
}



static void
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
                                         GTK_DIALOG_NO_SEPARATOR,
                                         GTK_STOCK_CANCEL,
                                         GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OK,
                                         GTK_RESPONSE_OK,
                                         NULL);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG(dialog)->vbox), 12);

  gtk_window_set_icon_name (GTK_WINDOW (dialog), "gtk-info");
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  /* Create the main alignment for the dialog */
  main_alignment = gtk_alignment_new (0, 0, 1, 1);
  gtk_alignment_set_padding (GTK_ALIGNMENT (main_alignment), 6, 0, 12, 12);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), main_alignment, TRUE, TRUE, 0);

  /* Create the main box for the dialog */
  vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 12);
  gtk_container_add (GTK_CONTAINER (main_alignment), vbox);

  /* Create the information label */
  information_label = sexy_url_label_new ();
  sexy_url_label_set_markup (SEXY_URL_LABEL (information_label), message);
  g_signal_connect (G_OBJECT (information_label), "url-activated",
                    G_CALLBACK (open_url_hook), NULL);
  gtk_misc_set_alignment (GTK_MISC (information_label), 0, 0);
  gtk_container_add (GTK_CONTAINER (vbox), information_label);

  /* Create the layout table */
  table = gtk_table_new (4, 2, FALSE);
  gtk_table_set_col_spacings (GTK_TABLE (table), 6);
  gtk_table_set_row_spacings (GTK_TABLE (table), 12);
  gtk_container_add (GTK_CONTAINER (vbox), table);

  /* Create the user label */
  user_label = gtk_label_new (_("User:"));
  gtk_misc_set_alignment (GTK_MISC (user_label), 0, 0.5);
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
  gtk_misc_set_alignment (GTK_MISC (password_label), 0, 0.5);
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
  gtk_misc_set_alignment (GTK_MISC (title_label), 0, 0.5);
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
  gtk_misc_set_alignment (GTK_MISC (comment_label), 0, 0.5);
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

  gtk_widget_show_all (GTK_DIALOG(dialog)->vbox);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_hide (dialog);

  if (response == GTK_RESPONSE_CANCEL)
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

  image_url = g_strdup_printf ("http://www.zimagez.com/zimage/%s.php", upload_name);
  thumbnail_url =
    g_strdup_printf ("http://www.zimagez.com/miniature/%s.php", upload_name);
  small_thumbnail_url =
    g_strdup_printf ("http://www.zimagez.com/avatar/%s.php", upload_name);
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
    xfce_titled_dialog_new_with_buttons (_("My screenshot on ZimageZ"),
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



static void cb_update_info (ExoJob *job, gchar *message, GtkWidget *label)
{
  g_return_if_fail (EXO_IS_JOB (job));
  g_return_if_fail (GTK_IS_LABEL (label));

  gtk_label_set_text (GTK_LABEL (label), message);
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
  GtkWidget *dialog;
  GtkWidget *label, *status_label;
  GtkWidget *hbox, *throbber;
  GtkWidget *main_box, *main_alignment;

  g_return_if_fail (image_path != NULL);
  g_return_if_fail (new_last_user == NULL || *new_last_user == NULL);

  dialog =
    gtk_dialog_new_with_buttons (_("ZimageZ"),
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

  job = zimagez_upload_to_zimagez (image_path, last_user, title);

  g_signal_connect (job, "ask", G_CALLBACK (cb_ask_for_information), NULL);
  g_signal_connect (job, "image-uploaded", G_CALLBACK (cb_image_uploaded), new_last_user);
  g_signal_connect (job, "error", G_CALLBACK (cb_error), NULL);
  g_signal_connect (job, "finished", G_CALLBACK (cb_finished), dialog);
  g_signal_connect (job, "info-message", G_CALLBACK (cb_update_info), label);

  gtk_dialog_run (GTK_DIALOG (dialog));
}
