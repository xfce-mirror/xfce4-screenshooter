/*  $Id$
 *
 *  Copyright © 2004 German Poo-Caaman~o <gpoo@ubiobio.cl>
 *  Copyright © 2005,2006 Daniel Bobadilla Leal <dbobadil@dcc.uchile.cl>
 *  Copyright © 2005 Jasper Huijsmans <jasper@xfce.org>
 *  Copyright © 2006 Jani Monoses <jani@ubuntu.com>
 *  Copyright © 2008 Jérôme Guelfucci <jerome.guelfucci@gmail.com>
 *
 *  Portions from the Gimp sources by
 *  Copyright © 1998-2000 Sven Neumann <sven@gimp.org>
 *  Copyright © 2003 Henrik Brix Andersen <brix@gimp.org>
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
#define MODE 0644

typedef struct
{
    XfcePanelPlugin *plugin;

    GtkWidget *button;
    GtkWidget *image;
    GtkWidget *preview;
    GtkTooltips *tooltips;
    GtkWidget *chooser;

    gint whole_screen;
    gint ask_for_file;

    gint screenshot_delay;
    gchar *screenshot_dir;

    gint counter;

    int style_id;
}
ScreenshotData;

/* Panel Plugin Interface */

static void screenshot_properties_dialog (XfcePanelPlugin *plugin,
                                       ScreenshotData *screenshot);
static void screenshot_construct (XfcePanelPlugin * plugin);

XFCE_PANEL_PLUGIN_REGISTER_INTERNAL (screenshot_construct);

/* internal functions */

static gboolean
screenshot_set_size (XfcePanelPlugin *plugin, int size, ScreenshotData *sd)
{
    GdkPixbuf *pb;
    int width = size - 2 - 2 * MAX (sd->button->style->xthickness,
                                    sd->button->style->ythickness);

    pb = xfce_themed_icon_load (SCREENSHOT_ICON_NAME, width);
    gtk_image_set_from_pixbuf (GTK_IMAGE (sd->image), pb);
    g_object_unref (pb);
    gtk_widget_set_size_request (GTK_WIDGET (plugin), size, size);

    return TRUE;
}

static void
screenshot_free_data (XfcePanelPlugin * plugin, ScreenshotData * sd)
{
  if (sd->style_id)
  	g_signal_handler_disconnect (plugin, sd->style_id);

  sd->style_id = 0;
  gtk_object_sink (GTK_OBJECT (sd->tooltips));
  gtk_widget_destroy (sd->chooser);
  g_free (sd);
}

static void
button_clicked(GtkWidget * button,  ScreenshotData * sd)
{
    GdkPixbuf * screenshot;
    GdkPixbuf * thumbnail;
    gint width;
    gint height;
    gint dialog_response;
    gchar * filename = NULL;
    
    gtk_widget_set_sensitive(GTK_WIDGET (sd->button), FALSE);
    
    screenshot = take_screenshot(sd->whole_screen, sd->screenshot_delay);
    
    width = gdk_pixbuf_get_width(screenshot);
    height = gdk_pixbuf_get_height(screenshot);

    thumbnail = gdk_pixbuf_scale_simple (screenshot,
				         width/5,
				         height/5, GDK_INTERP_BILINEAR);

    gtk_image_set_from_pixbuf (GTK_IMAGE (sd->preview), thumbnail);
    g_object_unref (thumbnail);
    
    filename = generate_filename_for_uri (gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER (sd->chooser)));  
    
    if (sd->ask_for_file)
    {
      gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (sd->chooser), filename);
      dialog_response = gtk_dialog_run (GTK_DIALOG (sd->chooser));
      
      if ( dialog_response == GTK_RESPONSE_ACCEPT )
      {
        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(sd->chooser));
        gdk_pixbuf_save (screenshot, filename, "png", NULL, NULL);
       }
      
      gtk_widget_hide ( GTK_WIDGET (sd->chooser) );
    }  
    else
    {    
       filename = generate_filename_for_uri ( sd->screenshot_dir );
       gdk_pixbuf_save (screenshot, filename, "png", NULL, NULL);
    }
    gtk_widget_set_sensitive(GTK_WIDGET ( sd->button), TRUE);
    g_free (filename);
}

static void
screenshot_style_set (XfcePanelPlugin *plugin, gpointer ignored,
                       ScreenshotData *sd)
{
    screenshot_set_size (plugin, xfce_panel_plugin_get_size (plugin), sd);
}

static void
screenshot_read_rc_file (XfcePanelPlugin *plugin, ScreenshotData *screenshot)
{
    char *file;
    XfceRc *rc;
    gint screenshot_delay = 0;
    gint whole_screen = 1;
    gint ask_for_file = 1;
    gchar *screenshot_dir = g_strdup ( xfce_get_homedir ());
        
    if ((file = xfce_panel_plugin_lookup_rc_file (plugin)) != NULL)
    {
        rc = xfce_rc_simple_open (file, TRUE);
        g_free (file);

        if (rc != NULL)
        {
            screenshot_delay = xfce_rc_read_int_entry (rc, "screenshot_delay", 0);
            whole_screen = xfce_rc_read_int_entry (rc, "whole_screen", 1);
            ask_for_file = xfce_rc_read_int_entry (rc, "ask_for_file", 1);
            screenshot_dir = g_strdup ( xfce_rc_read_entry (rc, "screenshot_dir", xfce_get_homedir () ) );

            xfce_rc_close (rc);
        }
    }

    screenshot->screenshot_delay = screenshot_delay;
    screenshot->whole_screen = whole_screen;
    screenshot->ask_for_file = ask_for_file;
    screenshot->screenshot_dir = screenshot_dir;
}

static void
screenshot_write_rc_file (XfcePanelPlugin *plugin, ScreenshotData *screenshot)
{
    char *file;
    XfceRc *rc;

    if (!(file = xfce_panel_plugin_save_location (plugin, TRUE)))
        return;

    rc = xfce_rc_simple_open (file, FALSE);
    g_free (file);

    if (!rc)
        return;

    xfce_rc_write_int_entry (rc, "screenshot_delay", screenshot->screenshot_delay);
    xfce_rc_write_int_entry (rc, "whole_screen", screenshot->whole_screen);
    xfce_rc_write_int_entry (rc, "ask_for_file", screenshot->ask_for_file);
    xfce_rc_write_entry (rc, "screenshot_dir", screenshot->screenshot_dir);

    xfce_rc_close (rc);
}

static void
ask_for_file_toggled (GtkToggleButton *tb, ScreenshotData *screenshot)
{
    screenshot->ask_for_file = gtk_toggle_button_get_active (tb);
}

static void
whole_screen_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
    sd->whole_screen = gtk_toggle_button_get_active (tb);
}

static void
active_window_toggled (GtkToggleButton *tb, ScreenshotData *sd)
{
    sd->whole_screen = !gtk_toggle_button_get_active (tb);
}

static void
screenshot_delay_spinner_changed(GtkWidget * spinner, ScreenshotData *sd)
{
    sd->screenshot_delay = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner));
}

static void
cb_default_folder (GtkWidget * chooser, ScreenshotData *sd)
{
    sd->screenshot_dir = gtk_file_chooser_get_filename ( GTK_FILE_CHOOSER ( chooser ) );
    gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( sd->chooser ), sd->screenshot_dir); 
}

static void
screenshot_dialog_response (GtkWidget *dlg, int reponse,
                         ScreenshotData *screenshot)
{
    g_object_set_data (G_OBJECT (screenshot->plugin), "dialog", NULL);

    gtk_widget_destroy (dlg);
    xfce_panel_plugin_unblock_menu (screenshot->plugin);
    screenshot_write_rc_file (screenshot->plugin, screenshot);
}

static void
screenshot_properties_dialog (XfcePanelPlugin *plugin, ScreenshotData *sd)
{
    GtkWidget *dlg, *vbox, *label2; 
    GtkWidget *options_frame, *modes_frame, *delay_box, *options_box, *modes_box;
    GtkWidget *save_button, *desktop_button, *active_window_button;
    GtkWidget *dir_chooser, *default_save_label, *delay_label;
    GtkWidget *screenshot_delay_spinner;

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
                      sd);

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
    
    desktop_button = gtk_radio_button_new_with_mnemonic (NULL, _("Take a screenshot of desktop"));
    gtk_widget_show (desktop_button);
    gtk_box_pack_start (GTK_BOX (modes_box), desktop_button, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (desktop_button),
                                  sd->whole_screen);
    g_signal_connect (desktop_button, "toggled", G_CALLBACK (whole_screen_toggled),
                      sd);
                      
    active_window_button = gtk_radio_button_new_with_mnemonic ( gtk_radio_button_get_group ( GTK_RADIO_BUTTON (desktop_button) ), 
                                             _("Take a screenshot of the active window"));
    gtk_widget_show (active_window_button);
    gtk_box_pack_start (GTK_BOX (modes_box), active_window_button, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (active_window_button),
                                  !sd->whole_screen);
    g_signal_connect (active_window_button, "toggled", G_CALLBACK (active_window_toggled),
                      sd);
    
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
                                  sd->ask_for_file);
    g_signal_connect (save_button, "toggled", G_CALLBACK (ask_for_file_toggled),
                      sd);
    
    /* Default save location */          
    default_save_label = gtk_label_new ( _("Default save location") );
    gtk_widget_show ( default_save_label );
    gtk_container_add ( GTK_CONTAINER ( options_box ), default_save_label );
    
    dir_chooser = gtk_file_chooser_button_new (_("Default save location"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_widget_show ( dir_chooser );
    gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER (dir_chooser), sd->screenshot_dir);
    gtk_container_add ( GTK_CONTAINER ( options_box ), dir_chooser );
    g_signal_connect (dir_chooser, "selection-changed", G_CALLBACK (cb_default_folder), sd);
        
    /* Screenshot delay */
    delay_label = gtk_label_new ( _("Delay before taking the screenshot") );
    gtk_widget_show (	delay_label );
    gtk_container_add ( GTK_CONTAINER ( options_box ), delay_label );
    
    delay_box = gtk_hbox_new(FALSE, 8);
    gtk_widget_show(delay_box);
    gtk_box_pack_start (GTK_BOX (options_box), delay_box, FALSE, FALSE, 0);

    screenshot_delay_spinner = gtk_spin_button_new_with_range(0.0, 60.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(screenshot_delay_spinner), sd->screenshot_delay);
    gtk_widget_show(screenshot_delay_spinner);
    gtk_box_pack_start (GTK_BOX (delay_box), screenshot_delay_spinner, FALSE, FALSE, 0);

    label2 = gtk_label_new_with_mnemonic(_("seconds"));
    gtk_widget_show(label2);
    gtk_box_pack_start (GTK_BOX (delay_box), label2, FALSE, FALSE, 0);

    g_signal_connect(screenshot_delay_spinner, "value-changed",
                        G_CALLBACK(screenshot_delay_spinner_changed), sd);

    gtk_widget_show (dlg);
}

static void
screenshot_construct (XfcePanelPlugin * plugin)
{
    ScreenshotData *sd = g_new0 (ScreenshotData, 1);

    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    sd->plugin = plugin;

    screenshot_read_rc_file (plugin, sd);

    sd->button = xfce_create_panel_button ();

    sd->counter = 0;

    sd->tooltips = gtk_tooltips_new ();
    gtk_tooltips_set_tip (sd->tooltips, sd->button, _("Take screenshot"), NULL);

    sd->image = gtk_image_new ();
    gtk_container_add (GTK_CONTAINER (sd->button), GTK_WIDGET (sd->image));

    gtk_widget_show_all (sd->button);

    sd->chooser = gtk_file_chooser_dialog_new ( _("Save screenshot as ..."),
                                                NULL,
                                                GTK_FILE_CHOOSER_ACTION_SAVE,
                                                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                                GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                                NULL);

    gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (sd->chooser), TRUE);
    gtk_dialog_set_default_response (GTK_DIALOG (sd->chooser), GTK_RESPONSE_ACCEPT);
    gtk_file_chooser_set_current_folder ( GTK_FILE_CHOOSER ( sd->chooser ), sd->screenshot_dir);

    sd->preview = gtk_image_new ();
    gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (sd->chooser), sd->preview);

    gtk_container_add (GTK_CONTAINER (plugin), sd->button);
    xfce_panel_plugin_add_action_widget (plugin, sd->button);

    g_signal_connect (sd->button, "clicked",
                      G_CALLBACK (button_clicked), sd);

    g_signal_connect (plugin, "free-data",
                      G_CALLBACK (screenshot_free_data), sd);

    g_signal_connect (plugin, "size-changed",
                      G_CALLBACK (screenshot_set_size), sd);

    sd->style_id =
        g_signal_connect (plugin, "style-set",
                          G_CALLBACK (screenshot_style_set), sd);

    xfce_panel_plugin_menu_show_configure (plugin);
    g_signal_connect (plugin, "configure-plugin",
                      G_CALLBACK (screenshot_properties_dialog), sd);
}
XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL (screenshot_construct);
