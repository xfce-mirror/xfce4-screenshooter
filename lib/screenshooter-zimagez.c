/*  $Id$
 *
 *  Copyright © 2009 Jérôme Guelfucci <jerome.guelfucci@gmail.com>
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


/* XML-RPC API for ZimageZ.com */

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



static void              open_url_hook             (GtkLinkButton     *button,
                                                    const gchar       *link,
                                                    gpointer           user_data);
static void              open_zimagez_link         (gpointer           unused);
static ScreenshooterJob *zimagez_upload_to_zimagez (const gchar       *file_name,
                                                    gchar             *last_user);
static gboolean          zimagez_upload_job        (ScreenshooterJob  *job,
                                                    GValueArray       *param_values,
                                                    GError           **error);
static void              cb_ask_for_information    (ScreenshooterJob  *job,
                                                    GtkListStore      *liststore,
                                                    const gchar       *message,
                                                    gpointer           unused);
static void              cb_image_uploaded         (ScreenshooterJob  *job,
                                                    gchar             *upload_name,
                                                    gpointer           unused);
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
open_url_hook (GtkLinkButton *button, const gchar *link, gpointer user_data)
{
  const gchar *command = g_strconcat ("xdg-open ", link, NULL);
  GError *error = NULL;

  if (!g_spawn_command_line_async (command, &error))
    {
      TRACE ("An error occured when opening the URL");

      screenshooter_error ("%s", error->message);
      g_error_free (error);
    }
}



static void open_zimagez_link (gpointer unused)
{
  open_url_hook (NULL, "http://www.zimagez.com", NULL);
}



static gboolean
zimagez_upload_job (ScreenshooterJob *job, GValueArray *param_values, GError **error)
{
  const gchar *encoded_data;
  const gchar *image_path;
  gchar *comment = g_strdup ("");
  gchar *data = NULL;
  gchar *encoded_password = NULL;
  gchar *file_name = NULL;
  gchar *login_response = NULL;
  gchar *online_file_name = NULL;
  gchar *password = g_strdup ("");
  gchar *title = g_strdup ("");
  gchar *user;
  gchar **last_user;

  gsize data_length;

  xmlrpc_env env;
  xmlrpc_value *resultP = NULL;
  xmlrpc_bool response = 0;

  const gchar * const serverurl = "http://www.zimagez.com/apiXml.php";
  const gchar * const method_login = "apiXml.xmlrpcLogin";
  const gchar * const method_logout = "apiXml.xmlrpcLogout";
  const gchar * const method_upload = "apiXml.xmlrpcUpload";

  GtkTreeIter iter;
  GtkListStore *liststore;

  g_return_val_if_fail (SCREENSHOOTER_IS_JOB (job), FALSE);
  g_return_val_if_fail (param_values != NULL, FALSE);
  g_return_val_if_fail (param_values->n_values == 2, FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_STRING (&param_values->values[0]), FALSE);
  g_return_val_if_fail (G_VALUE_HOLDS_POINTER (&param_values->values[1]), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
    return FALSE;

  /* Get the last user */
  last_user = g_value_get_pointer (g_value_array_get_nth (param_values, 1));
  user = g_strdup (*last_user);

  /* Get the path of the image that is to be uploaded */
  image_path = g_value_get_string (g_value_array_get_nth (param_values, 0));

  /* Start the user XML RPC session */

  exo_job_info_message (EXO_JOB (job), _("Initialize the connexion..."));

  TRACE ("Initialize the RPC environment");
  xmlrpc_env_init(&env);

  TRACE ("Initialize the RPC client");
  xmlrpc_client_init2 (&env, XMLRPC_CLIENT_NO_FLAGS, PACKAGE_NAME, PACKAGE_VERSION,
                       NULL, 0);

  if (env.fault_occurred)
    {
      GError *tmp_error =
        g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                     _("An error occurred during the XML exchange: %s (%d).\n "
                       "The screenshot could not be uploaded."),
                     env.fault_string, env.fault_code);

      xmlrpc_env_clean (&env);
      xmlrpc_client_cleanup ();

      g_propagate_error (error, tmp_error);

      return FALSE;
    }

  TRACE ("Get the information liststore ready.");

  liststore = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);

  TRACE ("Append the user");

  gtk_list_store_append (liststore, &iter);
  gtk_list_store_set (liststore, &iter,
                      0, g_strdup ("user"),
                      1, user,
                      -1);

  TRACE ("Append the password");

  gtk_list_store_append (liststore, &iter);
  gtk_list_store_set (liststore, &iter,
                      0, g_strdup ("password"),
                      1, password,
                      -1);

  TRACE ("Append the title");

  gtk_list_store_append (liststore, &iter);
  gtk_list_store_set (liststore, &iter,
                      0, g_strdup ("title"),
                      1, title,
                      -1);

  TRACE ("Append the comment");

  gtk_list_store_append (liststore, &iter);
  gtk_list_store_set (liststore, &iter,
                      0, g_strdup ("comment"),
                      1, comment,
                      -1);

  TRACE ("Ask for the user information");

  screenshooter_job_ask_info (job, liststore,
                              _("Please file the following fields with your "
                                "<a href=\"http://www.zimagez.com\">ZimageZ©</a> \n"
                                "user name, passsword and details about the screenshot."));

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (liststore), &iter);

  do
    {
      gchar *field_name = NULL;
      gchar *field_value = NULL;

      gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
                          0, &field_name,
                          1, &field_value,
                          -1);

      if (g_str_equal (field_name, "user"))
        {
          user = g_strdup (field_value);
        }
      else if (g_str_equal (field_name, "password"))
        {
          password = g_strdup (field_value);
        }
      else if (g_str_equal (field_name, "title"))
        {
          title = g_strdup (field_value);
        }
      else if (g_str_equal (field_name, "comment"))
        {
          comment = g_strdup (field_value);
        }

      g_free (field_name);
      g_free (field_value);
    }
  while (gtk_tree_model_iter_next (GTK_TREE_MODEL (liststore), &iter));

  while (!response)
    {
      gboolean empty_field = FALSE;

      if (exo_job_set_error_if_cancelled (EXO_JOB (job), error))
        {
          xmlrpc_env_clean (&env);
          xmlrpc_client_cleanup ();

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
      TRACE ("Check for empty fields");
      gtk_tree_model_get_iter_first (GTK_TREE_MODEL (liststore), &iter);

      do
        {
          gchar *field = NULL;

          gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter, 1, &field, -1);
          empty_field = g_str_equal (field, "");

          g_free (field);
        }
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (liststore), &iter));

      if (empty_field)
        {
          TRACE ("One of the fields was empty, let the user file it.");

          screenshooter_job_ask_info (job, liststore,
                                      _("<span weight=\"bold\" foreground=\"darkred\" "
                                        "stretch=\"semiexpanded\">You must fill all the "
                                        " fields.</span>"));
          continue;
        }

      encoded_password = g_strdup (g_strreverse (rot13 (password)));

      TRACE ("User: %s", user);

      /* Start the user session */
      TRACE ("Call the login method");

      exo_job_info_message (EXO_JOB (job), _("Login on ZimageZ.com..."));

      resultP = xmlrpc_client_call (&env, serverurl, method_login,
                                    "(ss)", user, encoded_password);

      if (env.fault_occurred)
        {
          GError *tmp_error =
            g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                         _("An error occurred during the XML exchange: %s (%d).\n "
                           "The screenshot could not be uploaded."),
                         env.fault_string, env.fault_code);

          xmlrpc_env_clean (&env);
          xmlrpc_client_cleanup ();

          g_free (user);
          g_free (password);
          g_free (title);
          g_free (comment);
          g_free (encoded_password);

          g_propagate_error (error, tmp_error);

          return FALSE;
        }

      TRACE ("Read the login response");

      /* If the response is a boolean, there was an error */
      if (xmlrpc_value_type (resultP) == XMLRPC_TYPE_BOOL)
        {
          xmlrpc_read_bool (&env, resultP, &response);

          if (env.fault_occurred)
            {
              GError *tmp_error =
                g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("An error occurred during the XML exchange: %s (%d).\n "
                               "The screenshot could not be uploaded."),
                             env.fault_string, env.fault_code);

              xmlrpc_env_clean (&env);
              xmlrpc_client_cleanup ();

              g_free (user);
              g_free (password);
              g_free (title);
              g_free (comment);
              g_free (encoded_password);

              g_propagate_error (error, tmp_error);

              return FALSE;
            }
        }
      /* Else we read the string response to get the session ID */
      else
        {
          TRACE ("Read the session ID");
          xmlrpc_read_string (&env, resultP, (const gchar ** const)&login_response);

          if (env.fault_occurred)
           {
             GError *tmp_error =
               g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                            _("An error occurred during the XML exchange: %s (%d).\n "
                              "The screenshot could not be uploaded."),
                            env.fault_string, env.fault_code);

             xmlrpc_env_clean (&env);
             xmlrpc_client_cleanup ();

             g_free (user);
             g_free (password);
             g_free (title);
             g_free (comment);
             g_free (encoded_password);

             g_propagate_error (error, tmp_error);

             return FALSE;
           }

          response = 1;
        }

      if (!response)
        {
          /* Login failed, erase the password and ask for the correct on to the
             user */
          gtk_tree_model_get_iter_first (GTK_TREE_MODEL (liststore), &iter);

          do
            {
              gchar *field_name = NULL;

              gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter, 0, &field_name, -1);

              if (g_str_equal (field_name, "password"))
                {
                  gtk_list_store_set (liststore, &iter, 1, g_strdup (""), -1);

                  g_free (field_name);

                  break;
                }

              g_free (field_name);
            }
          while (gtk_tree_model_iter_next (GTK_TREE_MODEL (liststore), &iter));

          screenshooter_job_ask_info (job, liststore,
                                      _("<span weight=\"bold\" foreground=\"darkred\" "
                                        "stretch=\"semiexpanded\">The user and the "
                                        "password you entered do not match. "
                                        "Please correct this.</span>"));

          gtk_tree_model_get_iter_first (GTK_TREE_MODEL (liststore), &iter);

          do
            {
              gchar *field_name = NULL;
              gchar *field_value = NULL;

              gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
                                  0, &field_name,
                                  1, &field_value,
                                  -1);

              if (g_str_equal (field_name, "user"))
                {
                  user = g_strdup (field_value);
                }
              else if (g_str_equal (field_name, "password"))
                {
                  password = g_strdup (field_value);
                }
              else if (g_str_equal (field_name, "title"))
                {
                  title = g_strdup (field_value);
                }
              else if (g_str_equal (field_name, "comment"))
                {
                  comment = g_strdup (field_value);
                }

              g_free (field_name);
              g_free (field_value);
            }
          while (gtk_tree_model_iter_next (GTK_TREE_MODEL (liststore), &iter));
        }
    }

  xmlrpc_DECREF (resultP);

  g_free (*last_user);
  *last_user = g_strdup (user);

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
  resultP = xmlrpc_client_call (&env, serverurl, method_upload,
                                "(sssss)", encoded_data, file_name, title, comment,
                                login_response);

  g_free (title);
  g_free (comment);
  g_free (file_name);

  if (env.fault_occurred)
    {
      GError *tmp_error =
        g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                     _("An error occurred during the XML exchange: %s (%d).\n "
                       "The screenshot could not be uploaded."),
                     env.fault_string, env.fault_code);

      xmlrpc_env_clean (&env);
      xmlrpc_client_cleanup ();

      g_propagate_error (error, tmp_error);

      return FALSE;
    }

  /* If the response is a boolean, there was an error */
  if (xmlrpc_value_type (resultP) == XMLRPC_TYPE_BOOL)
    {
      xmlrpc_bool response_upload;

      xmlrpc_read_bool (&env, resultP, &response_upload);

      if (env.fault_occurred)
        {
          GError *tmp_error =
            g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                         _("An error occurred during the XML exchange: %s (%d).\n "
                           "The screenshot could not be uploaded."),
                         env.fault_string, env.fault_code);

          xmlrpc_env_clean (&env);
          xmlrpc_client_cleanup ();

          g_propagate_error (error, tmp_error);

          return FALSE;
        }

      if (!response_upload)
        {
          GError *tmp_error =
            g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                         _("An error occurred while uploading the screenshot."));

          xmlrpc_env_clean (&env);
          xmlrpc_client_cleanup ();

          g_propagate_error (error, tmp_error);

          return FALSE;
        }
    }
  /* Else we get the file name */
  else
    {
      xmlrpc_read_string (&env, resultP, (const char **)&online_file_name);

      TRACE ("The screenshot has been uploaded, get the file name.");

      if (env.fault_occurred)
        {
          GError *tmp_error =
            g_error_new (G_IO_ERROR, G_IO_ERROR_FAILED,
                         _("An error occurred during the XML exchange: %s (%d).\n "
                           "The screenshot could not be uploaded."),
                         env.fault_string, env.fault_code);

          xmlrpc_env_clean (&env);
          xmlrpc_client_cleanup ();

          g_propagate_error (error, tmp_error);

          return FALSE;
        }
    }

  xmlrpc_DECREF (resultP);

  /* End the user session */

  exo_job_info_message (EXO_JOB (job), _("Close the session on ZimageZ.com..."));

  TRACE ("Closing the user session");

  xmlrpc_client_call (&env, serverurl, method_logout, "(s)", login_response);

  TRACE ("Cleanup the XMLRPC session");
  xmlrpc_env_clean (&env);
  xmlrpc_client_cleanup ();

  screenshooter_job_image_uploaded (job, online_file_name);

  return TRUE;
}



static ScreenshooterJob
*zimagez_upload_to_zimagez (const gchar *file_path, gchar *last_user)
{
  g_return_val_if_fail (file_path != NULL, NULL);

  return screenshooter_simple_job_launch (zimagez_upload_job, 2,
                                          G_TYPE_STRING, file_path,
                                          G_TYPE_POINTER, &last_user);
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
    xfce_titled_dialog_new_with_buttons (_("Details about the screenshot for ZimageZ©"),
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

  g_signal_connect_swapped (G_OBJECT (information_label), "url-activated",
                            G_CALLBACK (open_zimagez_link), NULL);

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
                                 "please create one on the Web page linked above"));


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

  gtk_widget_set_tooltip_text (title_entry,
                               _("A comment on the screenshot, it will be used when"
                                 " displaying the screenshot on ZimageZ"));

  gtk_table_attach_defaults (GTK_TABLE (table), comment_entry, 1, 2, 3, 4);

  /* Set the values */
  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (liststore), &iter);

  do
    {
      gchar *field_name = NULL;
      gchar *field_value = NULL;

      gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
                          0, &field_name,
                          1, &field_value,
                          -1);

      if (g_str_equal (field_name, "user"))
        {
          gtk_entry_set_text (GTK_ENTRY (user_entry), field_value);
        }
      else if (g_str_equal (field_name, "password"))
        {
          gtk_entry_set_text (GTK_ENTRY (password_entry), field_value);
        }
      else if (g_str_equal (field_name, "title"))
        {
          gtk_entry_set_text (GTK_ENTRY (title_entry), field_value);
        }
      else if (g_str_equal (field_name, "comment"))
        {
          gtk_entry_set_text (GTK_ENTRY (comment_entry), field_value);
        }

      g_free (field_name);
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
          gchar *field_name = NULL;

          gtk_tree_model_get (GTK_TREE_MODEL (liststore), &iter,
                              0, &field_name, -1);

          if (g_str_equal (field_name, "user"))
            {
              gtk_list_store_set (liststore, &iter,
                                  1, gtk_entry_get_text (GTK_ENTRY (user_entry)),
                                  -1);
            }
          else if (g_str_equal (field_name, "password"))
            {
              gtk_list_store_set (liststore, &iter,
                                  1, gtk_entry_get_text (GTK_ENTRY (password_entry)),
                                  -1);
            }
          else if (g_str_equal (field_name, "title"))
            {
              gtk_list_store_set (liststore, &iter,
                                  1, gtk_entry_get_text (GTK_ENTRY (title_entry)),
                                  -1);
            }
          else if (g_str_equal (field_name, "comment"))
            {
              gtk_list_store_set (liststore, &iter,
                                  1, gtk_entry_get_text (GTK_ENTRY (comment_entry)),
                                  -1);
            }

          g_free (field_name);
        }
      while (gtk_tree_model_iter_next (GTK_TREE_MODEL (liststore), &iter));
    }

  gtk_widget_destroy (dialog);
}



static void cb_image_uploaded (ScreenshooterJob *job, gchar *upload_name, gpointer unused)
{
  GtkWidget *dialog;
  GtkWidget *image_link, *thumbnail_link, *small_thumbnail_link;

  gchar *image_url;
  gchar *thumbnail_url;
  gchar *small_thumbnail_url;

  g_return_if_fail (upload_name != NULL);

  image_url = g_strdup_printf ("http://www.zimagez.com/zimage/%s.php", upload_name);
  thumbnail_url =
    g_strdup_printf ("http://www.zimagez.com/miniature/%s.php", upload_name);
  small_thumbnail_url =
    g_strdup_printf ("http://www.zimagez.com/avatar/%s.php", upload_name);

  dialog =
    xfce_titled_dialog_new_with_buttons (_("My screenshot on ZimageZ©"),
                                         NULL,
                                         GTK_DIALOG_NO_SEPARATOR,
                                         GTK_STOCK_CLOSE,
                                         GTK_RESPONSE_CLOSE,
                                         NULL);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), 20);
  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG(dialog)->vbox), 12);

  gtk_window_set_icon_name (GTK_WINDOW (dialog), "applications-internet");

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  /* Create the image link */
  image_link =
    gtk_link_button_new_with_label (image_url, _("Link to the full-size screenshot"));

  gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), image_link);

  /* Create the thumbnail link */
  thumbnail_link =
    gtk_link_button_new_with_label (thumbnail_url,
                                    _("Link to a thumbnail of the screenshot"));

  gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), thumbnail_link);

  /* Create the small thumbnail link */
  small_thumbnail_link =
    gtk_link_button_new_with_label (small_thumbnail_url,
                                    _("Link to a small thumbnail of the screenshot"));

  gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), small_thumbnail_link);

  /* Set the url hook for the buttons */
  gtk_link_button_set_uri_hook ((GtkLinkButtonUriFunc) open_url_hook, NULL, NULL);

  /* Show the dialog and run it */
  gtk_widget_show_all (GTK_DIALOG(dialog)->vbox);

  gtk_dialog_run (GTK_DIALOG (dialog));

  gtk_widget_destroy (dialog);

  g_free (image_url);
  g_free (thumbnail_url);
  g_free (small_thumbnail_url);
}



static void cb_error (ExoJob *job, GError *error, gpointer unused)
{
  GtkWidget *dialog;

  g_return_if_fail (error != NULL);

  dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_NO_SEPARATOR | GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "%s", error->message);

  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
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
 *
 * Uploads the image whose path is @image_path: a dialog asks for the user
 * login, password, a title for the image and a comment; then the image is
 * uploaded. The dialog is shown again with a warning is the password did
 * match the user name. The user can also cancel the upload procedure.
 *
 **/

void screenshooter_upload_to_zimagez (const gchar *image_path, gchar *last_user)
{
  ScreenshooterJob *job;
  GtkWidget *dialog;
  GtkWidget *label, *status_label;
  GtkWidget *main_box, *main_alignment;

  g_return_if_fail (image_path != NULL);

  dialog =
    gtk_dialog_new_with_buttons (_("Uploading to ZimageZ..."),
                                 NULL,
                                 GTK_DIALOG_NO_SEPARATOR,
                                 NULL);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG(dialog)->vbox), 12);
  gtk_window_set_deletable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_icon_name (GTK_WINDOW (dialog), "gtk-info");

  /* Create the main alignment for the dialog */
  main_alignment = gtk_alignment_new (0, 0, 1, 1);

  gtk_alignment_set_padding (GTK_ALIGNMENT (main_alignment), 6, 0, 6, 6);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), main_alignment, TRUE, TRUE, 0);

  /* Create the main box for the dialog */
  main_box = gtk_vbox_new (FALSE, 10);

  gtk_container_set_border_width (GTK_CONTAINER (main_box), 12);
  gtk_container_add (GTK_CONTAINER (main_alignment), main_box);

  /* Status label*/
  status_label = gtk_label_new ("");

  gtk_label_set_markup (GTK_LABEL (status_label),
                        _("<span weight=\"bold\" stretch=\"semiexpanded\">"
                          "Status</span>"));

  gtk_misc_set_alignment (GTK_MISC (status_label), 0, 0);
  gtk_container_add (GTK_CONTAINER (main_box), status_label);

  /* Information label */
  label = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (main_box), label);

  gtk_widget_show_all (GTK_DIALOG(dialog)->vbox);

  job = zimagez_upload_to_zimagez (image_path, last_user);

  g_signal_connect (job, "ask", (GCallback) cb_ask_for_information, NULL);
  g_signal_connect (job, "image-uploaded", (GCallback) cb_image_uploaded, NULL);
  g_signal_connect (job, "error", (GCallback) cb_error, NULL);
  g_signal_connect (job, "finished", (GCallback) cb_finished, dialog);
  g_signal_connect (job, "info-message", (GCallback) cb_update_info, label);

  gtk_dialog_run (GTK_DIALOG (dialog));
}
