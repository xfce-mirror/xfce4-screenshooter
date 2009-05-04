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
                             gpointer              user_data);

static void
open_zimagez_link            (gpointer             unused);



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
      TRACE ("An error occured when opening the URL");

      xfce_err (error->message);
      g_error_free (error);
    }
}



static void open_zimagez_link (gpointer unused)
{
  open_url_hook (NULL, "http://www.zimagez.com", NULL);
}
  

/* Public */



/**
 * screenshooter_upload_to_zimagez:
 * @image_path: the local path of the image that should be uploaded to
 * ZimageZ.com.
 *
 * Uploads the image whose path is @image_path: a dialog asks for the user
 * login, password, a title for the image and a comment; then the image is
 * uploaded. The dialog is shown again with a warning is the password did
 * match the user name. The user can also cancel the upload procedure.
 *
 * Returns: NULL is the upload failed or was cancelled, a #gchar* with the name of
 * the image on Zimagez.com (see the API at the beginning of this file for more
 * details).
 **/
   
gchar *screenshooter_upload_to_zimagez (const gchar *image_path)
{
  xmlrpc_env env;
  xmlrpc_value *resultP = NULL;
  xmlrpc_bool response = 0;

  const gchar * const serverurl = "http://www.zimagez.com/apiXml.php";
  const gchar * const method_login = "apiXml.xmlrpcLogin";
  const gchar * const method_logout = "apiXml.xmlrpcLogout";
  const gchar * const method_upload = "apiXml.xmlrpcUpload";

  gchar *data;
  gchar *password = NULL;
  gchar *user = NULL;
  gchar *title = NULL;
  gchar *comment = NULL;
  gchar *encoded_password = NULL;
  const gchar *encoded_data;
  const gchar *file_name = g_path_get_basename (image_path);
  const gchar *online_file_name;
  const gchar *login_response;
  gsize data_length;

  GtkWidget *dialog;
  GtkWidget *information_label;
  GtkWidget *vbox, *main_alignment;
  GtkWidget *table;
  GtkWidget *user_entry, *password_entry, *title_entry, *comment_entry;
  GtkWidget *user_label, *password_label, *title_label, *comment_label;

  /* Get the user information */
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

  /* Note for translators : make sure to put the <a>....</a> on the first line */
  sexy_url_label_set_markup (SEXY_URL_LABEL (information_label),
                             _("Please file the following fields with your "
                               "<a href=\"http://www.zimagez.com\">ZimageZ©</a> \n"
                               "user name, passsword and details about the screenshot."));

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

  /* Show the widgets of the dialog main box*/

  gtk_widget_show_all (GTK_DIALOG(dialog)->vbox);

  /* Start the user XML RPC session */

  TRACE ("Initiate the RPC environment");
  xmlrpc_env_init(&env);

  TRACE ("Initiate the RPC client");
  xmlrpc_client_init2 (&env, XMLRPC_CLIENT_NO_FLAGS, PACKAGE_NAME, PACKAGE_VERSION,
                       NULL, 0);

  if (warn_if_fault_occurred (&env))
    {
      xmlrpc_env_clean (&env);
      xmlrpc_client_cleanup ();

      return NULL;
    }

  while (!response)
    {
      gint dialog_response = gtk_dialog_run (GTK_DIALOG (dialog));

      if (dialog_response == GTK_RESPONSE_CANCEL)
        {
          xmlrpc_env_clean (&env);
          xmlrpc_client_cleanup ();

          return NULL;
        }
    
      user = g_strdup (gtk_entry_get_text (GTK_ENTRY (user_entry)));
      password = g_strdup (gtk_entry_get_text (GTK_ENTRY (password_entry)));
      title = g_strdup (gtk_entry_get_text (GTK_ENTRY (title_entry)));
      comment = g_strdup (gtk_entry_get_text (GTK_ENTRY (comment_entry)));

      if ((dialog_response == GTK_RESPONSE_OK) && (g_str_equal (user, "") ||
                                                   g_str_equal (password, "") ||
                                                   g_str_equal (title, "") ||
                                                   g_str_equal (comment, "")))
        {
          TRACE ("One of the fields was empty, let the user file it.");

          gtk_label_set_markup (GTK_LABEL (information_label),
                                _("<span weight=\"bold\" foreground=\"darkred\" "
                                  "stretch=\"semiexpanded\">You must fill all the "
                                  " fields.</span>"));

          g_free (user);
          g_free (password);
          g_free (title);
          g_free (comment);

          continue;
        }
      else
        {
          TRACE ("All fields were filed");
          gtk_widget_hide (dialog);
        }
    
      while (gtk_events_pending ())
        gtk_main_iteration_do (FALSE);
    
      encoded_password = g_strdup (g_strreverse (rot13 (password)));
    
      TRACE ("User: %s", user);

      /* Start the user session */
      TRACE ("Call the login method");
     
      resultP = xmlrpc_client_call (&env, serverurl, method_login,
                                    "(ss)", user, encoded_password);
     
      if (warn_if_fault_occurred (&env))
        {
          xmlrpc_env_clean (&env);
          xmlrpc_client_cleanup ();

          g_free (user);
          g_free (password);
          g_free (title);
          g_free (comment);
          g_free (encoded_password);
     
          return NULL;
        }

      TRACE ("Read the login response");
    
      /* If the response is a boolean, there was an error */
      if (xmlrpc_value_type (resultP) == XMLRPC_TYPE_BOOL)
        {
          xmlrpc_read_bool (&env, resultP, &response);
    
          if (warn_if_fault_occurred (&env))
            {
              xmlrpc_env_clean (&env);
              xmlrpc_client_cleanup ();

              g_free (user);
              g_free (password);
              g_free (title);
              g_free (comment);
              g_free (encoded_password);
        
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

              g_free (user);
              g_free (password);
              g_free (title);
              g_free (comment);
              g_free (encoded_password);
    
              return NULL;
            }

          response = 1;
        }

      if (!response)
        gtk_label_set_markup (GTK_LABEL (information_label),
                              _("<span weight=\"bold\" foreground=\"darkred\" "
                                "stretch=\"semiexpanded\">The user and the password you"
                                " entered do not match.</span>"));

    }

  xmlrpc_DECREF (resultP);

  g_free (password);
  g_free (encoded_password);
  
  /* Get the contents of the image file and encode it to base64 */
  g_file_get_contents (image_path, &data, &data_length, NULL);

  encoded_data = g_base64_encode ((guchar*)data, data_length);

  g_free (data);

  TRACE ("Call the upload method");
  resultP = xmlrpc_client_call (&env, serverurl, method_upload,
                                "(sssss)", encoded_data, file_name, title, comment,
                                login_response);

  g_free (user);
  g_free (title);
  g_free (comment);

  if (warn_if_fault_occurred (&env))
    {
      xmlrpc_env_clean (&env);
      xmlrpc_client_cleanup ();

      return NULL;
    }

  /* If the response is a boolean, there was an error */
  if (xmlrpc_value_type (resultP) == XMLRPC_TYPE_BOOL)
    {
      xmlrpc_bool response_upload;

      xmlrpc_read_bool (&env, resultP, &response_upload);

      if (warn_if_fault_occurred (&env))
        {
          xmlrpc_env_clean (&env);
          xmlrpc_client_cleanup ();

          return NULL;
        }

       if (!response_upload)
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



/**
 * screenshooter_display_zimagez_links:
 * @upload_name: the name of the image on ZimageZ.com
 *
 * Shows a dialog linking to the different images hosted on ZimageZ.com:
 * the full size image, the large thumbnail and the small thumbnail.
 * Links can be clicked to open the given page in a browser.
 **/
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

  
