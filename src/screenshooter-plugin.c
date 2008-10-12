/*  $Id$
 *
 *  Copyright © 2004 German Poo-Caaman~o <gpoo@ubiobio.cl>
 *  Copyright © 2005,2006 Daniel Bobadilla Leal <dbobadil@dcc.uchile.cl>
 *  Copyright © 2005 Jasper Huijsmans <jasper@xfce.org>
 *  Copyright © 2006 Jani Monoses <jani@ubuntu.com>
 *  Copyright © 2008 Jérôme Guelfucci <jerome.guelfucci@gmail.com>
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
t */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4panel/xfce-panel-convenience.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <X11/Xatom.h>

#include "screenshooter-utils.h"

#define SCREENSHOT_ICON_NAME  "applets-screenshooter"

/* Struct containing all panel plugin data */
typedef struct
{
  XfcePanelPlugin *plugin;

  GtkWidget *button;
  GtkWidget *image;
  
  int style_id;
  ScreenshotData *sd;
}
PluginData;



/* Protoypes */

static void screenshot_properties_dialog (XfcePanelPlugin *plugin,
                                          PluginData *pd);
static void screenshot_construct (XfcePanelPlugin *plugin);
static gboolean screenshot_set_size (XfcePanelPlugin *plugin, 
                                     int size, PluginData *pd);
static void screenshot_free_data (XfcePanelPlugin *plugin, 
                                  PluginData *pd);
static void button_clicked (GtkWidget *button, PluginData *pd);
static void screenshot_style_set (XfcePanelPlugin *plugin, gpointer ignored,
                                  PluginData *pd);
static void screenshot_read_rc_file (XfcePanelPlugin *plugin, PluginData *pd);                
static void screenshot_write_rc_file (XfcePanelPlugin *plugin, PluginData *pd);
static void show_save_dialog_toggled (GtkToggleButton *tb, PluginData *pd);
static void whole_screen_toggled (GtkToggleButton *tb, PluginData *pd);
static void active_window_toggled (GtkToggleButton *tb, PluginData *pd);
static void cb_delay_spinner_changed (GtkWidget *spinner, 
                                              PluginData *pd);
static void cb_default_folder (GtkWidget *chooser, PluginData *pd);
static void screenshot_dialog_response (GtkWidget *dlg, int reponse,
                                        PluginData *pd);
                                        
                                              

/* Register the panel plugin */
XFCE_PANEL_PLUGIN_REGISTER_INTERNAL (screenshot_construct);



/* Internal functions */



/* Modify the size of the panel button 
Returns TRUE if succesful. 
*/
static gboolean
screenshot_set_size (XfcePanelPlugin *plugin, int size, PluginData *pd)
{
  GdkPixbuf *pb;
  int width = size - 2 - 2 * MAX (pd->button->style->xthickness,
                                    pd->button->style->ythickness);

  pb = xfce_themed_icon_load (SCREENSHOT_ICON_NAME, width);
  gtk_image_set_from_pixbuf (GTK_IMAGE (pd->image), pb);
  g_object_unref (pb);
  gtk_widget_set_size_request (GTK_WIDGET (plugin), size, size);

  return TRUE;
}



/* Free the panel plugin data stored in pd
plugin: a XfcePanelPlugin (a screenshooter one).
pd: the associated PluginData. 
*/
static void
screenshot_free_data (XfcePanelPlugin *plugin, PluginData *pd)
{
  if (pd->style_id)
  	g_signal_handler_disconnect (plugin, pd->style_id);

  pd->style_id = 0;
  g_free (pd->sd->screenshot_dir);
  g_free (pd->sd);
  g_free (pd);
}



/* Take the screenshot when the button is clicked.
button: the panel button.
pd: the PluginData storing the options for taking the screenshot.
*/
static void
button_clicked (GtkWidget *button, PluginData *pd)
{
  GdkPixbuf *screenshot;
    
	/* Make the button unclickable so that the user does not press it while 
	another screenshot is in progress */
	gtk_widget_set_sensitive (GTK_WIDGET (pd->button), FALSE);

  /* Get the screenshot */
	screenshot = take_screenshot (pd->sd->mode, pd->sd->delay);

  save_screenshot (screenshot, pd->sd->show_save_dialog, 
                   pd->sd->screenshot_dir);
  
	gtk_widget_set_sensitive (GTK_WIDGET (pd->button), TRUE);
	
	g_object_unref (screenshot);
}



/* Set the style of the panel plugin.
plugin: a XfcePanelPlugin (a screenshooter one).
pd: the associated PluginData.
*/
static void
screenshot_style_set (XfcePanelPlugin *plugin, gpointer ignored,
                       PluginData *pd)
{
  screenshot_set_size (plugin, xfce_panel_plugin_get_size (plugin), pd);
}



/* Read the rc file associated to the panel plugin and store the options in pd.
plugin: a XfcePanelPlugin (a screenshooter one).
pd: the associated PluginData.
*/
static void
screenshot_read_rc_file (XfcePanelPlugin *plugin, PluginData *pd)
{
  char *file;
  XfceRc *rc;
  gint delay = 0;
  gint mode = FULLSCREEN;
  gint show_save_dialog = 1;
  gchar *screenshot_dir = g_strdup (DEFAULT_SAVE_DIRECTORY);

  /* If there is an rc file, we read it */
  if ( (file = xfce_panel_plugin_lookup_rc_file (plugin) ) != NULL)
  {
      rc = xfce_rc_simple_open (file, TRUE);

      if ( rc != NULL)
      {
          delay = xfce_rc_read_int_entry (rc, "delay", 0);
          mode = xfce_rc_read_int_entry (rc, "mode", FULLSCREEN);
          show_save_dialog = xfce_rc_read_int_entry (rc, "show_save_dialog", 1);
          screenshot_dir = 
            g_strdup (xfce_rc_read_entry (rc, 
                                          "screenshot_dir", 
                                          DEFAULT_SAVE_DIRECTORY));
          xfce_rc_close (rc);
      }
      
      g_free (file);
  }
  
  /* And set the pd values */
  pd->sd->delay = delay;
  pd->sd->mode = mode;
  pd->sd->show_save_dialog = show_save_dialog;
  pd->sd->screenshot_dir = screenshot_dir;
}



/* Write the pd options in the rc file associated to plugin
plugin: a XfcePanelPlugin (a screenshooter one).
pd: the associated PluginData.
*/
static void
screenshot_write_rc_file (XfcePanelPlugin *plugin, PluginData *pd)
{
  char *file;
  XfceRc *rc;

  if (!(file = xfce_panel_plugin_save_location (plugin, TRUE)))
    return;

  rc = xfce_rc_simple_open (file, FALSE);
  g_free (file);

  if (!rc)
    return;

  xfce_rc_write_int_entry (rc, "delay", pd->sd->delay);
  xfce_rc_write_int_entry (rc, "mode", pd->sd->mode);
  xfce_rc_write_int_entry (rc, "show_save_dialog", pd->sd->show_save_dialog);
  xfce_rc_write_entry (rc, "screenshot_dir", pd->sd->screenshot_dir);

  xfce_rc_close (rc);
}



/* Callback for save dialog:
   Get the value of the toggle button and set the save dialog option.
*/ 
static void
show_save_dialog_toggled (GtkToggleButton *tb, PluginData *pd)
{
  pd->sd->show_save_dialog = gtk_toggle_button_get_active (tb);
}



/* Callback for whole screen:
   Get the value of the toggle button and set the whole screen option.
*/ 
static void
whole_screen_toggled (GtkToggleButton *tb, PluginData *pd)
{
  if (gtk_toggle_button_get_active (tb))
    {
      pd->sd->mode = FULLSCREEN;
    }
  else
    {
      pd->sd->mode = ACTIVE_WINDOW;
    }
}



/* Callback for active window:
   Get the value of the toggle button and set the whole screen option.
*/ 
static void
active_window_toggled (GtkToggleButton *tb, PluginData *pd)
{
  if (gtk_toggle_button_get_active (tb))
    {
      pd->sd->mode = ACTIVE_WINDOW;
    }
  else
    {
      pd->sd->mode = FULLSCREEN;
    }
}



/* Callback for delay:
   Get the value of the toggle button and set the delay option.
*/ 
static void
cb_delay_spinner_changed (GtkWidget *spinner, PluginData *pd)
{
  pd->sd->delay = 
    gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spinner));
}



/* Callback for default folder:
   Get the value of the toggle button and set the default folder option.
*/ 
static void
cb_default_folder (GtkWidget *chooser, PluginData *pd)
{
  pd->sd->screenshot_dir = 
    gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
}



/* Callback for dialog response:
   Update the tooltips if using gtk >= 2.12.
   Unblock the plugin contextual menu.
   Save the options in the rc file.*/
static void
screenshot_dialog_response (GtkWidget *dlg, int reponse,
                            PluginData *pd)
{
  g_object_set_data (G_OBJECT (pd->plugin), "dialog", NULL);

  gtk_widget_destroy (dlg);
  
  /* Update tooltips according to the chosen option */
  #if GTK_CHECK_VERSION(2,12,0)
  if (pd->sd->mode == FULLSCREEN)
  {
    gtk_widget_set_tooltip_text (GTK_WIDGET (pd->button),
                                 _("Take a screenshot of the desktop"));
  }
  else
  {
    gtk_widget_set_tooltip_text (GTK_WIDGET (pd->button),
                                 _("Take a screenshot of the active window"));
  }
  #endif
  
  /* Unblock the menu and save options */
  xfce_panel_plugin_unblock_menu (pd->plugin);
  screenshot_write_rc_file (pd->plugin, pd);
}



/* Properties dialog to set the plugin options */
static void
screenshot_properties_dialog (XfcePanelPlugin *plugin, PluginData *pd)
{
  GtkWidget *dlg, *vbox, *label2; 
  GtkWidget *options_frame, *modes_frame, *delay_box, *options_box, *modes_box;
  GtkWidget *save_button, *desktop_button, *active_window_button;
  GtkWidget *dir_chooser, *default_save_label, *delay_label;
  GtkWidget *delay_spinner;
  
  /* Block the menu to prevent the user from launching several dialogs at
  the same time */
  xfce_panel_plugin_block_menu (plugin);

	/* Create the dialog */
  dlg = xfce_titled_dialog_new_with_buttons (_("Screenshooter plugin"),
              GTK_WINDOW (gtk_widget_get_toplevel (GTK_WIDGET (plugin))),
              GTK_DIALOG_DESTROY_WITH_PARENT |
              GTK_DIALOG_NO_SEPARATOR,
              GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
              NULL);

  g_object_set_data (G_OBJECT (plugin), "dialog", dlg);

  gtk_window_set_position (GTK_WINDOW (dlg), GTK_WIN_POS_CENTER);

  g_signal_connect (dlg, "response", G_CALLBACK (screenshot_dialog_response),
                    pd);

  gtk_container_set_border_width (GTK_CONTAINER (dlg), 2);
  gtk_window_set_icon_name (GTK_WINDOW (dlg), "applets-screenshooter");

	/* Create the main box for the dialog */
	vbox = gtk_vbox_new (FALSE, 8);
  gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
  gtk_widget_show (vbox);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), vbox,
                      TRUE, TRUE, 0);
  
  /* Create the frame for screenshot modes and fill it with the radio buttons */
  modes_frame = gtk_frame_new (_("Modes"));
  gtk_container_add (GTK_CONTAINER (vbox), modes_frame);
  gtk_widget_show (modes_frame);
      
  modes_box = gtk_vbox_new (FALSE, 8);
  gtk_container_add (GTK_CONTAINER (modes_frame), modes_box);
  gtk_container_set_border_width (GTK_CONTAINER (modes_box), 6);
  gtk_widget_show (modes_box);
  
  desktop_button = 
    gtk_radio_button_new_with_mnemonic (NULL, 
                                        _("Take a screenshot of desktop"));
  gtk_widget_show (desktop_button);
  gtk_box_pack_start (GTK_BOX (modes_box), desktop_button, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (desktop_button),
                                (pd->sd->mode == FULLSCREEN));
  g_signal_connect (desktop_button, "toggled", 
                    G_CALLBACK (whole_screen_toggled),
                    pd);
                    
  active_window_button = 
    gtk_radio_button_new_with_mnemonic (gtk_radio_button_get_group (GTK_RADIO_BUTTON (desktop_button)), 
                                        _("Take a screenshot of the active window"));
  gtk_widget_show (active_window_button);
  gtk_box_pack_start (GTK_BOX (modes_box), active_window_button, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active_window_button),
                                (pd->sd->mode == ACTIVE_WINDOW));
  g_signal_connect (active_window_button, "toggled", 
                    G_CALLBACK (active_window_toggled),
                    pd);
  
  /* Create the options frame and add the delay and save options */
  options_frame = gtk_frame_new (_("Options"));
  gtk_container_add(GTK_CONTAINER (vbox), options_frame);
  gtk_widget_show (options_frame);
  
  options_box = gtk_vbox_new (FALSE, 8);
  gtk_container_add (GTK_CONTAINER (options_frame), options_box);
  gtk_container_set_border_width (GTK_CONTAINER (options_box), 6);
  gtk_widget_show (options_box);
  
  /* Save option */
  save_button = gtk_check_button_new_with_mnemonic (_("Show save dialog"));
  gtk_widget_show (save_button);
  gtk_box_pack_start (GTK_BOX (options_box), save_button, FALSE, FALSE, 0);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (save_button),
                                pd->sd->show_save_dialog);
  g_signal_connect (save_button, "toggled", 
                    G_CALLBACK (show_save_dialog_toggled), pd);
  
  /* Default save location */          
  default_save_label = gtk_label_new ( "" );
  gtk_label_set_markup (GTK_LABEL (default_save_label),
	                      _("<span weight=\"bold\" stretch=\"semiexpanded\">Default save location</span>"));
	gtk_misc_set_alignment (GTK_MISC (default_save_label), 0, 0);
  gtk_widget_show (default_save_label);
  gtk_container_add (GTK_CONTAINER (options_box), default_save_label);
  
  dir_chooser = 
    gtk_file_chooser_button_new (_("Default save location"), 
                                 GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
  gtk_widget_show (dir_chooser);
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dir_chooser), 
                                       pd->sd->screenshot_dir);
  gtk_container_add (GTK_CONTAINER (options_box), dir_chooser);
  g_signal_connect (dir_chooser, "selection-changed", 
                    G_CALLBACK (cb_default_folder), pd);
      
  /* Screenshot delay */
  delay_label = gtk_label_new ( "" );
  gtk_label_set_markup (GTK_LABEL(delay_label),
	                      _("<span weight=\"bold\" stretch=\"semiexpanded\">Delay before taking the screenshot</span>"));
	gtk_misc_set_alignment(GTK_MISC (delay_label), 0, 0); 
  gtk_widget_show (delay_label);
  gtk_container_add (GTK_CONTAINER (options_box), delay_label);
  
  delay_box = gtk_hbox_new(FALSE, 8);
  gtk_widget_show (delay_box);
  gtk_box_pack_start (GTK_BOX (options_box), delay_box, FALSE, FALSE, 0);

  delay_spinner = gtk_spin_button_new_with_range(0.0, 60.0, 1.0);
  gtk_spin_button_set_value (GTK_SPIN_BUTTON (delay_spinner), 
                             pd->sd->delay);
  gtk_widget_show (delay_spinner);
  gtk_box_pack_start (GTK_BOX (delay_box), delay_spinner, FALSE, 
                      FALSE, 0);

  label2 = gtk_label_new_with_mnemonic(_("seconds"));
  gtk_widget_show (label2);
  gtk_box_pack_start (GTK_BOX (delay_box), label2, FALSE, FALSE, 0);

  g_signal_connect (delay_spinner, "value-changed",
                    G_CALLBACK (cb_delay_spinner_changed), pd);

  gtk_widget_show (dlg);
}



/* Create the plugin button */
static void
screenshot_construct (XfcePanelPlugin *plugin)
{
  /* Initialise the data structs */
  PluginData *pd = g_new0 (PluginData, 1);
  ScreenshotData *sd = g_new0 (ScreenshotData, 1);

  pd->sd = sd;
  
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

  pd->plugin = plugin;

  /* Read the options */
  screenshot_read_rc_file (plugin, pd);
  
  /* Create the panel button */
  pd->button = xfce_create_panel_button ();

  pd->image = gtk_image_new ();

  gtk_container_add (GTK_CONTAINER (pd->button), GTK_WIDGET (pd->image));
  
  /* Set the tooltips if available */
  #if GTK_CHECK_VERSION(2,12,0)
  if (pd->sd->mode == FULLSCREEN)
  {
    gtk_widget_set_tooltip_text (GTK_WIDGET (pd->button),
                                 _("Take a screenshot of desktop"));
  }
  else
  {
    gtk_widget_set_tooltip_text (GTK_WIDGET (pd->button),
                                 _("Take a screenshot of the active window"));
  }
  #endif
    
  gtk_widget_show_all (pd->button);
  
  gtk_container_add (GTK_CONTAINER (plugin), pd->button);
  xfce_panel_plugin_add_action_widget (plugin, pd->button);
  
  /* Set the callbacks */
  g_signal_connect (pd->button, "clicked",
                    G_CALLBACK (button_clicked), pd);

  g_signal_connect (plugin, "free-data",
                    G_CALLBACK (screenshot_free_data), pd);

  g_signal_connect (plugin, "size-changed",
                    G_CALLBACK (screenshot_set_size), pd);

  pd->style_id =
      g_signal_connect (plugin, "style-set",
                        G_CALLBACK (screenshot_style_set), pd);

  xfce_panel_plugin_menu_show_configure (plugin);
  g_signal_connect (plugin, "configure-plugin",
                    G_CALLBACK (screenshot_properties_dialog), pd);
}
XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (screenshot_construct);
