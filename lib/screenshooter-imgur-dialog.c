/*  $Id$
 *
 *  Copyright © 2018 Arthur Jansen <arthurj155@gmail.com>
 *  Copyright © 2018 Andre Miranda <andreldm@xfce.org>
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



#include "screenshooter-imgur-dialog.h"
#include "screenshooter-imgur-dialog_ui.h"

#include <exo/exo.h>
#include <libxfce4ui/libxfce4ui.h>



struct _ScreenshooterImgurDialog
{
  GObject parent;
  GtkWidget *window;
  GtkEntry *link_entry;

  gchar *image_url, *thumbnail_url, *small_thumbnail_url;
  gchar *delete_link;
  GtkToggleButton *embed_html_toggle, *embed_markdown_toggle, *embed_bb_code_toggle;
  GtkToggleButton *embed_tiny_toggle, *embed_medium_toggle, *embed_full_toggle;
  GtkToggleButton *embed_link_full_size_toggle;
  GtkTextView *embed_text_view;
};



G_DEFINE_TYPE (ScreenshooterImgurDialog, screenshooter_imgur_dialog, G_TYPE_OBJECT)



static void cb_link_toggle_full (GtkToggleButton *button, gpointer user_data);
static void cb_link_toggle_medium (GtkToggleButton *button, gpointer user_data);
static void cb_link_toggle_tiny (GtkToggleButton *button, gpointer user_data);

static void cb_link_copy (GtkWidget *widget, gpointer user_data);
static void cb_link_view_in_browser (GtkWidget *widget, gpointer user_data);

static void cb_generate_embed_text (GtkWidget *widget, gpointer user_data);
static void cb_embed_text_copy (GtkWidget *widget, gpointer user_data);

static void cb_delete_link_copy (GtkWidget *widget, gpointer user_data);
static void cb_delete_link_view (GtkWidget *widget, gpointer user_data);



void screenshooter_imgur_dialog_init (ScreenshooterImgurDialog *self)
{
}



static void
screenshooter_imgur_dialog_finalize (GObject *object)
{
  ScreenshooterImgurDialog *self = SCREENSHOOTER_IMGUR_DIALOG (object);

  g_free (self->image_url);
  g_free (self->thumbnail_url);
  g_free (self->small_thumbnail_url);
  g_free (self->delete_link);

  gtk_widget_destroy (self->window);

  (*G_OBJECT_CLASS (screenshooter_imgur_dialog_parent_class)->finalize) (object);
}



static void
screenshooter_imgur_dialog_class_init (ScreenshooterImgurDialogClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  screenshooter_imgur_dialog_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = screenshooter_imgur_dialog_finalize;
}



ScreenshooterImgurDialog *
screenshooter_imgur_dialog_new (const gchar *upload_name,
                                const gchar *delete_hash)
{
  GtkBuilder *builder;
  GtkWidget *notebook;
  GtkEntry *delete_link_entry;
  GtkButton *delete_link_copy_button, *delete_link_view_button;
  GtkToggleButton *link_full_toggle, *link_medium_toggle, *link_tiny_toggle;
  GtkButton *link_copy_button, *link_view_button, *embed_copy_button;
  ScreenshooterImgurDialog *self;

  g_return_val_if_fail (upload_name != NULL, NULL);

  self = g_object_new (SCREENSHOOTER_TYPE_IMGUR_DIALOG, NULL);

  self->image_url = g_strdup_printf ("https://imgur.com/%s.png", upload_name);
  self->thumbnail_url = g_strdup_printf ("https://imgur.com/%sl.png", upload_name);
  self->small_thumbnail_url = g_strdup_printf ("https://imgur.com/%ss.png", upload_name);
  self->delete_link = g_strdup_printf ("https://imgur.com/delete/%s", delete_hash);

  builder = gtk_builder_new_from_string (screenshooter_imgur_dialog_ui,
                                         screenshooter_imgur_dialog_ui_length);

  // Setup window
  self->window = xfce_titled_dialog_new_with_mixed_buttons (_("Screenshot"),
    NULL, GTK_DIALOG_DESTROY_WITH_PARENT,
    "", _("_Close"), GTK_RESPONSE_CLOSE,
    NULL);
  gtk_window_set_icon_name (GTK_WINDOW (self->window), "org.xfce.screenshooter");
  gtk_window_set_default_size (GTK_WINDOW (self->window), 500, 330);

  // Add notebook widget to window
  notebook = GTK_WIDGET (gtk_builder_get_object (builder, "dialog-notebook"));
  gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (self->window))), notebook);

  self->link_entry = GTK_ENTRY (gtk_builder_get_object (builder, "link_entry"));
  self->embed_text_view = GTK_TEXT_VIEW (gtk_builder_get_object (builder, "embed_text_view"));
  gtk_entry_set_text (self->link_entry, self->image_url);

  // Image tab

  link_full_toggle = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "link_full_toggle"));
  link_medium_toggle = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "link_medium_toggle"));
  link_tiny_toggle = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "link_tiny_toggle"));

  g_signal_connect (link_full_toggle, "toggled", (GCallback) cb_link_toggle_full, (gpointer) self);
  g_signal_connect (link_medium_toggle, "toggled", (GCallback) cb_link_toggle_medium, (gpointer) self);
  g_signal_connect (link_tiny_toggle, "toggled", (GCallback) cb_link_toggle_tiny, (gpointer) self);

  link_copy_button = GTK_BUTTON (gtk_builder_get_object (builder, "link_copy_button"));
  link_view_button = GTK_BUTTON (gtk_builder_get_object (builder, "link_view_button"));
  embed_copy_button = GTK_BUTTON (gtk_builder_get_object (builder, "embed_copy_button"));

  g_signal_connect (link_copy_button, "clicked", (GCallback) cb_link_copy, (gpointer) self);
  g_signal_connect (link_view_button, "clicked", (GCallback) cb_link_view_in_browser, (gpointer) self);
  g_signal_connect (embed_copy_button, "clicked", (GCallback) cb_embed_text_copy, (gpointer) self);

  // Embed tab

  self->embed_html_toggle = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "embed_html_toggle"));
  self->embed_markdown_toggle = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "embed_markdown_toggle"));
  self->embed_bb_code_toggle = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "embed_bb_code_toggle"));
  self->embed_tiny_toggle = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "embed_tiny_toggle"));
  self->embed_medium_toggle = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "embed_medium_toggle"));
  self->embed_full_toggle = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "embed_full_toggle"));
  self->embed_link_full_size_toggle = GTK_TOGGLE_BUTTON (gtk_builder_get_object (builder, "embed_link_full_size_toggle"));

  // Regenerate the embed text when any togglebutton on the embed tab is toggled
  g_signal_connect (self->embed_html_toggle, "toggled", (GCallback) cb_generate_embed_text, (gpointer) self);
  g_signal_connect (self->embed_markdown_toggle, "toggled", (GCallback) cb_generate_embed_text, (gpointer) self);
  g_signal_connect (self->embed_bb_code_toggle, "toggled", (GCallback) cb_generate_embed_text, (gpointer) self);
  g_signal_connect (self->embed_tiny_toggle, "toggled", (GCallback) cb_generate_embed_text, (gpointer) self);
  g_signal_connect (self->embed_medium_toggle, "toggled", (GCallback) cb_generate_embed_text, (gpointer) self);
  g_signal_connect (self->embed_full_toggle, "toggled", (GCallback) cb_generate_embed_text, (gpointer) self);
  g_signal_connect (self->embed_link_full_size_toggle, "toggled", (GCallback) cb_generate_embed_text, (gpointer) self);

  // Generate default embed text
  cb_generate_embed_text (NULL, (gpointer) self);

  // Deletion link tab

  delete_link_entry = GTK_ENTRY (gtk_builder_get_object (builder, "delete_link_entry"));
  gtk_entry_set_text (delete_link_entry, self->delete_link);

  delete_link_copy_button = GTK_BUTTON (gtk_builder_get_object (builder, "delete_link_copy_button"));
  delete_link_view_button = GTK_BUTTON (gtk_builder_get_object (builder, "delete_link_view_button"));

  g_signal_connect (delete_link_copy_button, "clicked", G_CALLBACK (cb_delete_link_copy), self);
  g_signal_connect (delete_link_view_button, "clicked", G_CALLBACK (cb_delete_link_view), self);

  g_object_unref (builder);

  return self;
}



void
screenshooter_imgur_dialog_run (ScreenshooterImgurDialog *self)
{
  GtkDialog *dialog;

  g_return_if_fail (SCREENSHOOTER_IS_IMGUR_DIALOG (self));

  dialog = GTK_DIALOG (self->window);

  gtk_widget_show_all (gtk_dialog_get_content_area (dialog));
  gtk_dialog_run (dialog);
}



// Callbacks



static void
cb_link_toggle_full (GtkToggleButton *button, gpointer user_data)
{
  ScreenshooterImgurDialog *dialog;

  g_return_if_fail (SCREENSHOOTER_IS_IMGUR_DIALOG (user_data));

  dialog = SCREENSHOOTER_IMGUR_DIALOG (user_data);
  if (gtk_toggle_button_get_active (button))
    gtk_entry_set_text (dialog->link_entry, dialog->image_url);
}



static void
cb_link_toggle_medium (GtkToggleButton *button, gpointer user_data)
{
  ScreenshooterImgurDialog *dialog;

  g_return_if_fail (SCREENSHOOTER_IS_IMGUR_DIALOG (user_data));

  dialog = SCREENSHOOTER_IMGUR_DIALOG (user_data);
  if (gtk_toggle_button_get_active (button))
    gtk_entry_set_text (dialog->link_entry, dialog->thumbnail_url);
}



static void
cb_link_toggle_tiny (GtkToggleButton *button, gpointer user_data)
{
  ScreenshooterImgurDialog *dialog;

  g_return_if_fail (SCREENSHOOTER_IS_IMGUR_DIALOG (user_data));

  dialog = SCREENSHOOTER_IMGUR_DIALOG (user_data);
  if (gtk_toggle_button_get_active (button))
    gtk_entry_set_text (dialog->link_entry, dialog->small_thumbnail_url);
}



static void
cb_link_copy (GtkWidget *widget, gpointer user_data)
{
  GtkClipboard *clipboard;
  ScreenshooterImgurDialog *dialog;
  guint16 len;
  const gchar *text;

  g_return_if_fail (SCREENSHOOTER_IS_IMGUR_DIALOG (user_data));

  dialog = SCREENSHOOTER_IMGUR_DIALOG (user_data);
  text = gtk_entry_get_text (dialog->link_entry);
  len = gtk_entry_get_text_length (dialog->link_entry);
  clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, text, len);
}



static void
cb_link_view_in_browser (GtkWidget *widget, gpointer user_data)
{
  ScreenshooterImgurDialog *dialog;
  const gchar *link;

  g_return_if_fail (SCREENSHOOTER_IS_IMGUR_DIALOG (user_data));

  dialog = SCREENSHOOTER_IMGUR_DIALOG (user_data);
  link = gtk_entry_get_text (dialog->link_entry);
  exo_execute_preferred_application ("WebBrowser", link, NULL, NULL, NULL);
}



static void
cb_generate_embed_text (GtkWidget *widget, gpointer user_data)
{
  ScreenshooterImgurDialog *dialog;
  const gchar *link = NULL;
  gchar *text = NULL;
  gboolean link_to_full_size;

  g_return_if_fail (SCREENSHOOTER_IS_IMGUR_DIALOG (user_data));

  dialog = SCREENSHOOTER_IMGUR_DIALOG (user_data);

  if (gtk_toggle_button_get_active (dialog->embed_full_toggle))
    link = dialog->image_url;
  else if (gtk_toggle_button_get_active (dialog->embed_medium_toggle))
    link = dialog->thumbnail_url;
  else if (gtk_toggle_button_get_active (dialog->embed_tiny_toggle))
    link = dialog->small_thumbnail_url;
  else
    g_return_if_reached ();

  g_return_if_fail (link != NULL);

  link_to_full_size = gtk_toggle_button_get_active (dialog->embed_link_full_size_toggle);

  if (gtk_toggle_button_get_active (dialog->embed_html_toggle))
    if (link_to_full_size)
      text = g_markup_printf_escaped ("<a href=\"%s\">\n  <img src=\"%s\" />\n</a>", dialog->image_url, link);
    else
      text = g_markup_printf_escaped ("<img src=\"%s\" />", link);
  else if (gtk_toggle_button_get_active (dialog->embed_markdown_toggle))
    if (link_to_full_size)
      text = g_strdup_printf ("[![%s](%s)](%s)", dialog->image_url, link, link);
    else
      text = g_strdup_printf ("![%s](%s)", dialog->image_url, link);
  else if (gtk_toggle_button_get_active (dialog->embed_bb_code_toggle))
    if (link_to_full_size)
      text = g_strdup_printf ("[url=%s]\n  [img]%s[/img]\n[/url]", dialog->image_url, link);
    else
      text = g_strdup_printf ("[img]%s[/img]", link);
  else
    g_return_if_reached ();

  g_return_if_fail (text != NULL);

  gtk_text_buffer_set_text (gtk_text_view_get_buffer (dialog->embed_text_view), text, strlen(text));

  g_free (text);
}



static void
cb_embed_text_copy (GtkWidget *widget, gpointer user_data)
{
  GtkClipboard *clipboard;
  GtkTextBuffer *buffer;
  ScreenshooterImgurDialog *dialog;
  guint16 len;
  const gchar *text;
  GtkTextIter start, end;

  g_return_if_fail (SCREENSHOOTER_IS_IMGUR_DIALOG (user_data));

  dialog = SCREENSHOOTER_IMGUR_DIALOG (user_data);

  buffer = gtk_text_view_get_buffer (dialog->embed_text_view);
  gtk_text_buffer_get_bounds (buffer, &start, &end);

  text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
  len = strlen (text);

  clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, text, len);
}



static void
cb_delete_link_copy (GtkWidget *widget, gpointer user_data)
{
  GtkClipboard *clipboard;
  ScreenshooterImgurDialog *dialog;

  g_return_if_fail (SCREENSHOOTER_IS_IMGUR_DIALOG (user_data));

  dialog = SCREENSHOOTER_IMGUR_DIALOG (user_data);
  clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
  gtk_clipboard_set_text (clipboard, dialog->delete_link, strlen (dialog->delete_link));
}



static void
cb_delete_link_view (GtkWidget *widget, gpointer user_data)
{
  ScreenshooterImgurDialog *dialog;

  g_return_if_fail (SCREENSHOOTER_IS_IMGUR_DIALOG (user_data));

  dialog = SCREENSHOOTER_IMGUR_DIALOG (user_data);
  exo_execute_preferred_application ("WebBrowser", dialog->delete_link, NULL, NULL, NULL);
}
