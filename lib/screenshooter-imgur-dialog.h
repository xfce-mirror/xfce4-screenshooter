#ifndef IMGUR_DIALOG_H
#define IMGUR_DIALOG_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SCREENSHOOTER_TYPE_IMGUR_DIALOG screenshooter_imgur_dialog_get_type ()
G_DECLARE_FINAL_TYPE (ScreenshooterImgurDialog, screenshooter_imgur_dialog, SCREENSHOOTER, IMGUR_DIALOG, GObject)

ScreenshooterImgurDialog *screenshooter_imgur_dialog_new (const gchar *upload_name,
                                                          const gchar *delete_hash);
void screenshooter_imgur_dialog_run ();

G_END_DECLS

#endif
