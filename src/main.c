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

#include <libxfce4util/libxfce4util.h>

#include "screenshooter-utils.h"

gboolean version = FALSE;
gboolean window = FALSE;
gint delay = 0;

static GOptionEntry entries[] =
{
    {    "version", 'v', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &version,
        N_("Version information"),
        NULL
    },
    {   "window", 'w', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_NONE, &window,
        N_("Take a screenshot of the active window"),
        NULL
    },
    {		"delay", 'd', G_OPTION_FLAG_IN_MAIN, G_OPTION_ARG_INT, &delay,
       N_("Delay in seconds before taking the screenshot"),
       NULL
    
    },
    { NULL }
};

int main(int argc, char **argv)
{
  GError *cli_error = NULL;
  GdkPixbuf * screenshot;
  GdkPixbuf * thumbnail;
  gchar * filename = NULL;
  GtkWidget * preview;
  GtkWidget * chooser;
  gint dialog_response;
  
  xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
    
  if (!gtk_init_with_args(&argc, &argv, _(""), entries, PACKAGE, &cli_error))
  {
    if (cli_error != NULL)
    {
      g_print (_("%s: %s\nTry %s --help to see a full list of available command line options.\n"), PACKAGE, cli_error->message, PACKAGE_NAME);
      g_error_free (cli_error);
      return 1;
    }
  }
  
  if (version)
  {
    g_print("%s\n", PACKAGE_STRING);
    return 0;
  }
  
  if (!window)
  {
    screenshot = take_screenshot (1,delay);
    
  }
  else
  {
    screenshot = take_screenshot (0,delay);
  }
  
  chooser = gtk_file_chooser_dialog_new ( _("Save screenshot as ..."),
                                          NULL,
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                          GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
                                          NULL);
  gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER (chooser), TRUE);
  gtk_dialog_set_default_response (GTK_DIALOG (chooser), GTK_RESPONSE_ACCEPT);
  gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER ( chooser ), xfce_get_homedir () );  
  
  filename = generate_filename_for_uri ( g_strdup ( xfce_get_homedir () ) );
  preview = gtk_image_new ();
  
  gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (chooser), filename);
  gtk_file_chooser_set_preview_widget (GTK_FILE_CHOOSER (chooser), preview);
  
  thumbnail = gdk_pixbuf_scale_simple (screenshot, gdk_pixbuf_get_width(screenshot)/5, gdk_pixbuf_get_height(screenshot)/5, GDK_INTERP_BILINEAR);
  gtk_image_set_from_pixbuf (GTK_IMAGE (preview), thumbnail);
  g_object_unref (thumbnail);
    
  dialog_response = gtk_dialog_run (GTK_DIALOG (chooser));
  
  if ( dialog_response == GTK_RESPONSE_ACCEPT )
  {
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER(chooser));
    gdk_pixbuf_save (screenshot, filename, "png", NULL, NULL);
  }
  
  gtk_widget_destroy(chooser);
  g_free(filename);
  
  return 0;
}
