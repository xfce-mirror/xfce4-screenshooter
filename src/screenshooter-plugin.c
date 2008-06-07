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
    gchar *screenshots_dir;

    gint counter;

    NetkScreen *screen;
    int netk_id;
    int screen_id;
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
    if (sd->netk_id)
        g_signal_handler_disconnect (sd->screen, sd->netk_id);

    if (sd->screen_id)
        g_signal_handler_disconnect (plugin, sd->screen_id);

    if (sd->style_id)
        g_signal_handler_disconnect (plugin, sd->style_id);

    sd->netk_id = sd->screen_id = sd->style_id = 0;
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
    gint screenshot_delay = 2;
    gint whole_screen = 1;
    gint ask_for_file = 1;

    if ((file = xfce_panel_plugin_lookup_rc_file (plugin)) != NULL)
    {
        rc = xfce_rc_simple_open (file, TRUE);
        g_free (file);

        if (rc != NULL)
        {
            screenshot_delay = xfce_rc_read_int_entry (rc, "screenshot_delay", 2);
            whole_screen = xfce_rc_read_int_entry (rc, "whole_screen", 1);
            ask_for_file = xfce_rc_read_int_entry (rc, "ask_for_file", 1);

            xfce_rc_close (rc);
        }
    }

    screenshot->screenshot_delay = screenshot_delay;
    screenshot->whole_screen = whole_screen;
    screenshot->ask_for_file = ask_for_file;
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
screenshot_delay_spinner_changed(GtkWidget * spinner, ScreenshotData *sd)
{
    sd->screenshot_delay = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spinner));
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
    GtkWidget *dlg, *header, *vbox, *hbox2, *label2, *cb1, *cb2;
    GtkWidget *screenshot_delay_spinner;

    xfce_panel_plugin_block_menu (plugin);

    dlg = gtk_dialog_new_with_buttons (_("Properties"),
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

    header = xfce_create_header (NULL, _("Screenshots"));
    gtk_widget_set_size_request (GTK_BIN (header)->child, 200, 32);
    gtk_container_set_border_width (GTK_CONTAINER (header), 6);
    gtk_widget_show (header);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), header,
                        FALSE, TRUE, 0);

    vbox = gtk_vbox_new (FALSE, 8);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 6);
    gtk_widget_show (vbox);
    gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dlg)->vbox), vbox,
                        TRUE, TRUE, 0);

    cb1 = gtk_check_button_new_with_mnemonic (_("Ask for _filename"));
    gtk_widget_show (cb1);
    gtk_box_pack_start (GTK_BOX (vbox), cb1, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb1),
                                  sd->ask_for_file);
    g_signal_connect (cb1, "toggled", G_CALLBACK (ask_for_file_toggled),
                      sd);

    cb2 = gtk_check_button_new_with_mnemonic (_("Take a screenshot of the whole screen"));
    gtk_widget_show (cb2);
    gtk_box_pack_start (GTK_BOX (vbox), cb2, FALSE, FALSE, 0);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cb2),
                                  sd->whole_screen);
    g_signal_connect (cb2, "toggled", G_CALLBACK (whole_screen_toggled),
                      sd);
    
    /* Screenshot delay */
    hbox2 = gtk_hbox_new(FALSE, 8);
    gtk_widget_show(hbox2);
    gtk_box_pack_start (GTK_BOX (vbox), hbox2, FALSE, FALSE, 0);

    screenshot_delay_spinner = gtk_spin_button_new_with_range(0.0, 60.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(screenshot_delay_spinner), sd->screenshot_delay);
    gtk_widget_show(screenshot_delay_spinner);
    gtk_box_pack_start (GTK_BOX (hbox2), screenshot_delay_spinner, FALSE, FALSE, 0);

    label2 = gtk_label_new_with_mnemonic(_("Delay before taking screenshot"));
    gtk_widget_show(label2);
    gtk_box_pack_start (GTK_BOX (hbox2), label2, FALSE, FALSE, 0);

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
