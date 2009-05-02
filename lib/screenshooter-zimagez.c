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



static gboolean
warn_if_fault_occurred      (xmlrpc_env * const    envP);

static void
open_url_hook               (GtkLinkButton        *button,
                             const gchar          *link,
                             gpointer             user_data);



/* Private */



static gboolean warn_if_fault_occurred (xmlrpc_env * const envP)
{
  gboolean error_occured = FALSE;

  if (envP->fault_occurred)
    {
      TRACE ("An error occured during the XML transaction %s, %d",
             envP->fault_string, envP->fault_code );

      xfce_err (_("An error occurred during the XML exchange: %s (%d).\n The screenshot "
                  "could not be uploaded."),
                envP->fault_string, envP->fault_code );

      error_occured = TRUE;
    }

  return error_occured;
}


static void
open_url_hook (GtkLinkButton *button, const gchar *link, gpointer user_data)
{
  const gchar *command = g_strconcat ("xdg-open ", link, NULL);
  GError *error = NULL;

  if (!g_spawn_command_line_async (command, &error))
    {
      TRACE ("An error occured");

      xfce_err (error->message);
      g_error_free (error);
    }
}
  

/* Public */



/**
 * screenshooter_upload_to_zimagez:
 * @image_path: the local path of the image that should be uploaded to
 * ZimageZ.com.
 *
 * Uploads the image whose path is @image_path: a dialog asks for the user
 * login, password, a title for the image and a comment; then the image is
 * uploaded.
 *
 * Returns: NULL is the upload fail, a #gchar* with the name of the image on
 * Zimagez.com (see the API at the beginning of this file for more details).
 **/
   
gchar *screenshooter_upload_to_zimagez (const gchar *image_path)
{
  xmlrpc_env env;
  xmlrpc_value *resultP;

  const gchar * const serverurl = "http://www.zimagez.com/apiXml.php";
  const gchar * const method_login = "apiXml.xmlrpcLogin";
  const gchar * const method_logout = "apiXml.xmlrpcLogout";
  const gchar * const method_upload = "apiXml.xmlrpcUpload";

  gchar *data;
  gchar *password = NULL;
  const gchar *user;
  const gchar *title;
  const gchar *comment;
  const gchar *encoded_data;
  const gchar *encoded_password;
  const gchar *file_name = g_path_get_basename (image_path);
  const gchar *online_file_name;
  const gchar *login_response;
  gsize data_length;

  GtkWidget *dialog;
  GtkWidget *information_label;
  GtkWidget *user_hbox, *password_hbox, *title_hbox, *comment_hbox;
  GtkWidget *user_entry, *password_entry, *title_entry, *comment_entry;
  GtkWidget *user_label, *password_label, *title_label, *comment_label;

  /* Get the user information */
  /* Create the information dialog */
  dialog =
    xfce_titled_dialog_new_with_buttons (_("Details about the screenshot for ZimageZ©"),
                                         NULL,
                                         GTK_DIALOG_NO_SEPARATOR,
                                         GTK_STOCK_OK,
                                         GTK_RESPONSE_OK,
                                         NULL);

  gtk_window_set_position (GTK_WINDOW (dialog), GTK_WIN_POS_CENTER);
  gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), 20);
  gtk_box_set_spacing (GTK_BOX (GTK_DIALOG(dialog)->vbox), 12);

  gtk_window_set_icon_name (GTK_WINDOW (dialog), "gtk-info");

  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

  /* Create the information label */

  information_label =
    gtk_label_new (_("Please file the following fields with your ZimageZ© user name and "
                     "password."));

  /* Create the user box */

  user_hbox = gtk_hbox_new (FALSE, 6);

  gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), user_hbox);

  /* Create the user label */
  user_label = gtk_label_new (_("User:"));

  gtk_container_add (GTK_CONTAINER (user_hbox), user_label);

  /* Create the user entry */
  user_entry = gtk_entry_new ();

  gtk_container_add (GTK_CONTAINER (user_hbox), user_entry);

  /* Create the password box */

  password_hbox = gtk_hbox_new (FALSE, 6);

  gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), password_hbox);

  /* Create the password label */
  password_label = gtk_label_new (_("Password:"));

  gtk_container_add (GTK_CONTAINER (password_hbox), password_label);

  /* Create the password entry */
  password_entry = gtk_entry_new ();

  gtk_entry_set_visibility (GTK_ENTRY (password_entry), FALSE);

  gtk_container_add (GTK_CONTAINER (password_hbox), password_entry);

  /* Create the title box */

  title_hbox = gtk_hbox_new (FALSE, 6);

  gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), title_hbox);

  /* Create the title label */
  title_label = gtk_label_new (_("Title:"));

  gtk_container_add (GTK_CONTAINER (title_hbox), title_label);

  /* Create the title entry */
  title_entry = gtk_entry_new ();

  gtk_container_add (GTK_CONTAINER (title_hbox), title_entry);

  /* Create the comment box */

  comment_hbox = gtk_hbox_new (FALSE, 6);

  gtk_container_add (GTK_CONTAINER (GTK_DIALOG(dialog)->vbox), comment_hbox);

  /* Create the comment label */
  comment_label = gtk_label_new (_("Comment:"));

  gtk_container_add (GTK_CONTAINER (comment_hbox), comment_label);

  /* Create the comment entry */
  comment_entry = gtk_entry_new ();

  gtk_container_add (GTK_CONTAINER (comment_hbox), comment_entry);

  /* Show the dialog */

  gtk_widget_show_all (GTK_DIALOG(dialog)->vbox);

  gtk_dialog_run (GTK_DIALOG (dialog));

  user = g_strdup (gtk_entry_get_text (GTK_ENTRY (user_entry)));
  password = g_strdup (gtk_entry_get_text (GTK_ENTRY (password_entry)));
  title = g_strdup (gtk_entry_get_text (GTK_ENTRY (title_entry)));
  comment = g_strdup (gtk_entry_get_text (GTK_ENTRY (comment_entry)));

  gtk_widget_destroy (dialog);

  while (gtk_events_pending ())
    gtk_main_iteration_do (FALSE);

  encoded_password = g_strreverse (rot13 (password));

  TRACE ("User: %s Password: %s", user, encoded_password);

  /* Get the contents of the image file and encode it to base64 */
  g_file_get_contents (image_path, &data, &data_length, NULL);

  encoded_data = g_base64_encode ((guchar*)data, data_length);

  g_free (data);

  /* Start the user session */

  TRACE ("Initiate the RPC environment");
  xmlrpc_env_init(&env);

  TRACE ("Initiate the RPC client");
  xmlrpc_client_init2 (&env, XMLRPC_CLIENT_NO_FLAGS, PACKAGE_NAME, PACKAGE_VERSION,
                       NULL, 0);

  if (warn_if_fault_occurred (&env))
    {
      xmlrpc_env_clean (&env);
      xmlrpc_client_cleanup ();

      g_free (password);

      return NULL;
    }

  /* Start the user session */
  TRACE ("Call the login method");

  resultP = xmlrpc_client_call (&env, serverurl, method_login,
                                "(ss)", user, encoded_password);

  g_free (password);

  if (warn_if_fault_occurred (&env))
    {
      xmlrpc_env_clean (&env);
      xmlrpc_client_cleanup ();

      return NULL;
    }

  TRACE ("Read the login response");

  /* If the response is a boolean, there was an error */
  if (xmlrpc_value_type (resultP) == XMLRPC_TYPE_BOOL)
    {
      xmlrpc_bool response;

      xmlrpc_read_bool (&env, resultP, &response);

      if (warn_if_fault_occurred (&env))
        {
          xmlrpc_env_clean (&env);
          xmlrpc_client_cleanup ();

          return NULL;
        }

       if (!response)
         {
           xfce_err (_("The username or the password you gave is incorrect."));

           TRACE ("Incorrect password/login");

           xmlrpc_env_clean (&env);
           xmlrpc_client_cleanup ();

           return NULL;
         }
    }
  /* Else we read the string response to get the session ID */
  else
    {
      TRACE ("Read the session ID");
      xmlrpc_read_string (&env, resultP, (const gchar ** const)&login_response);

      if (warn_if_fault_occurred (&env))
        {
          xmlrpc_env_clean (&env);
          xmlrpc_client_cleanup ();

          return NULL;
        }
    }

  xmlrpc_DECREF (resultP);

  TRACE ("Call the upload method");
  resultP = xmlrpc_client_call (&env, serverurl, method_upload,
                                "(sssss)", encoded_data, file_name, title, comment,
                                login_response);

  if (warn_if_fault_occurred (&env))
    {
      xmlrpc_env_clean (&env);
      xmlrpc_client_cleanup ();

      return NULL;
    }

  /* If the response is a boolean, there was an error */
  if (xmlrpc_value_type (resultP) == XMLRPC_TYPE_BOOL)
    {
      xmlrpc_bool response;

      xmlrpc_read_bool (&env, resultP, &response);

      if (warn_if_fault_occurred (&env))
        {
          xmlrpc_env_clean (&env);
          xmlrpc_client_cleanup ();

          return NULL;
        }

       if (!response)
         {
           xfce_err (_("An error occurred while uploading the screenshot."));

           TRACE ("Error while uploading the screenshot.");

           xmlrpc_env_clean (&env);
           xmlrpc_client_cleanup ();

           return NULL;
         }
    }
  /* Else we get the file name */
  else
    {
      xmlrpc_read_string (&env, resultP, (const char **)&online_file_name);

      TRACE ("The screenshot has been uploaded, get the file name.");

      if (warn_if_fault_occurred (&env))
        {
          xmlrpc_env_clean (&env);
          xmlrpc_client_cleanup ();

          return NULL;
        }
    }

  xmlrpc_DECREF (resultP);

  /* End the user session */

  TRACE ("Closing the user session");

  xmlrpc_client_call (&env, serverurl, method_logout, "(s)", login_response);

  TRACE ("Cleanup the XMLRPC session");
  xmlrpc_env_clean (&env);
  xmlrpc_client_cleanup ();

  return g_strdup (online_file_name);
}



void screenshooter_display_zimagez_links (const gchar *upload_name)
{
  GtkWidget *dialog;
  GtkWidget *image_link, *thumbnail_link, *small_thumbnail_link;

  gchar *image_url =
    g_strdup_printf ("http://www.zimagez.com/zimage/%s.php", upload_name);
  gchar *thumbnail_url =
    g_strdup_printf ("http://www.zimagez.com/miniature/%s.php", upload_name);
  gchar *small_thumbnail_url =
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

  /* Ugly hack to make sure the dialog is not displayed anymore */
  while (gtk_events_pending ())
    gtk_main_iteration_do (FALSE);

  g_free (image_url);
  g_free (thumbnail_url);
  g_free (small_thumbnail_url);
}

  
