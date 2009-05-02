/*  $Id$
 *
 *  Copyright © 2008-2009 Jérôme Guelfucci <jerome.guelfucci@gmail.com>
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

#include "screenshooter-actions.h"

void screenshooter_take_and_output_screenshot (ScreenshotData *sd)
{
  GdkPixbuf *screenshot =
    screenshooter_take_screenshot (sd->region, sd->delay, sd->show_mouse);

  g_return_if_fail (screenshot != NULL);
  
  if (sd->action == SAVE)
    {
      if (sd->screenshot_dir == NULL)
        {
          sd->screenshot_dir = screenshooter_get_home_uri ();
        }

      screenshooter_save_screenshot (screenshot,
                                     sd->show_save_dialog,
                                     sd->screenshot_dir);
    }
  else if (sd->action == CLIPBOARD)
    {
      screenshooter_copy_to_clipboard (screenshot);
    }
  else
    {
      GFile *temp_dir = g_file_new_for_path (g_get_tmp_dir ());
      const gchar *temp_dir_uri = g_file_get_uri (temp_dir);
      const gchar *screenshot_path =
        screenshooter_save_screenshot (screenshot, FALSE, temp_dir_uri);

      if (screenshot_path != NULL)
        {
          if (sd->action == OPEN)
            {
              screenshooter_open_screenshot (screenshot_path, sd->app);
            }
          else
            {
              const gchar *upload_name =
                screenshooter_upload_to_zimagez (screenshot_path);

              if (upload_name != NULL)
                screenshooter_display_zimagez_links (upload_name);
            }
        }

      g_object_unref (temp_dir);
    }

  g_object_unref (screenshot);
}

