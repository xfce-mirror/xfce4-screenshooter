/*  $Id$
 *
 *  Copyright © 2023 André Miranda <andreldm@xfce.org>
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

#include "screenshooter-format.h"

#include <libxfce4ui/libxfce4ui.h>



/* Internals */


static GSList *SUPPORTED_FORMATS = NULL;



static void
screenshooter_image_format_free (gpointer data)
{
  ImageFormat *format = data;
  g_free (format->extensions);
  g_free (format->option_keys);
  g_free (format->option_values);
  g_free (format);
}



static gboolean
screenshooter_is_format_supported (const gchar *format)
{
  gboolean result = FALSE;
  GSList *supported_formats;
  gchar *name;

  supported_formats = gdk_pixbuf_get_formats ();

  for (GSList *lp = supported_formats; lp != NULL; lp = lp->next)
    {
      name = gdk_pixbuf_format_get_name (lp->data);
      if (G_UNLIKELY (g_strcmp0 (name, format) == 0 && gdk_pixbuf_format_is_writable (lp->data)))
        {
          result = TRUE;
          g_free (name);
          break;
        }
      g_free (name);
    }

  g_slist_free_1 (supported_formats);

  return result;
}



/* Public */



GSList *
screenshooter_get_supported_formats (void)
{
  ImageFormat *format;

  if (SUPPORTED_FORMATS != NULL)
    return SUPPORTED_FORMATS;

  format = g_new0 (ImageFormat, 1);
  format->type = "png";
  format->name = _("PNG File");
  format->preferred_extension = "png";
  format->extensions = g_new0 (gchar *, 2);
  format->extensions[0] = ".png";
  SUPPORTED_FORMATS = g_slist_append (SUPPORTED_FORMATS, format);

  format = g_new0 (ImageFormat, 1);
  format->type = "jpeg";
  format->name = _("JPEG File");
  format->preferred_extension = "jpg";
  format->extensions = g_new0 (gchar *, 3);
  format->extensions[0] = ".jpg";
  format->extensions[1] = ".jpeg";
  SUPPORTED_FORMATS = g_slist_append (SUPPORTED_FORMATS, format);

  format = g_new0 (ImageFormat, 1);
  format->type = "bmp";
  format->name = _("BMP File");
  format->preferred_extension = "bmp";
  format->extensions = g_new0 (gchar *, 2);
  format->extensions[0] = ".bmp";
  SUPPORTED_FORMATS = g_slist_append (SUPPORTED_FORMATS, format);

  if (screenshooter_is_format_supported ("webp"))
    {
      format = g_new0 (ImageFormat, 1);
      format->type = "webp";
      format->name = _("WebP File");
      format->preferred_extension = "webp";
      format->extensions = g_new0 (gchar *, 2);
      format->extensions[0] = ".webp";
      SUPPORTED_FORMATS = g_slist_append (SUPPORTED_FORMATS, format);
    }

  if (screenshooter_is_format_supported ("jxl"))
    {
      format = g_new0 (ImageFormat, 1);
      format->type = "jxl";
      format->name = _("JPEG XL File");
      format->preferred_extension = "jxl";
      format->extensions = g_new0 (gchar*, 2);
      format->extensions[0] = ".jxl";
      format->option_keys = g_new0 (gchar*, 2);
      format->option_keys[0] = "quality";
      format->option_values = g_new0 (gchar*, 2);
      format->option_values[0] = "100";
      SUPPORTED_FORMATS = g_slist_append (SUPPORTED_FORMATS, format);
    }

  if (screenshooter_is_format_supported ("avif"))
    {
      format = g_new0 (ImageFormat, 1);
      format->type = "avif";
      format->name = _("AVIF File");
      format->preferred_extension = "avif";
      format->extensions = g_new0 (gchar *, 2);
      format->extensions[0] = ".avif";
      SUPPORTED_FORMATS = g_slist_append (SUPPORTED_FORMATS, format);
    }

  return SUPPORTED_FORMATS;
}



void
screenshooter_free_supported_formats (void)
{
  g_slist_free_full (SUPPORTED_FORMATS, screenshooter_image_format_free);
}



gboolean
screenshooter_image_format_match_extension (ImageFormat *format, gchar *filepath)
{
  if (G_UNLIKELY (format->extensions == NULL))
    return FALSE;

  for (gchar **ext = format->extensions; *ext; ext++)
    {
      if (g_str_has_suffix (filepath, *ext))
        return TRUE;
    }
  
  return FALSE;
}
