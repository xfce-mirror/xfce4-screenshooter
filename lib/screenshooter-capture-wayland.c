/*  $Id$
 *
 *  Copyright © 2024 André Miranda <andreldm@xfce.org>
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

#include <screenshooter-capture-wayland.h>



static const char * unsupported_image[] = {
"100 18 2 1",
" 	c #F5FAFE",
".	c #FF0000",
"                                                                                                    ",
"                                                                                                    ",
" .     .                                                                                         .  ",
" .     .                                                                   .                     .  ",
" .     .                                                                   .                     .  ",
" .     .  . ....    .....   .     .  . ...    . ...      ...      . ...  ......     ...      ... .  ",
" .     .  ..   ..  .     .  .     .  ..   .   ..   .    .   .     ..   .   .       .   .    .   ..  ",
" .     .  .     .  .        .     .  .     .  .     .  .     .    .        .      .     .  .     .  ",
" .     .  .     .  ....     .     .  .     .  .     .  .     .    .        .      .     .  .     .  ",
" .     .  .     .      ...  .     .  .     .  .     .  .     .    .        .      .......  .     .  ",
" .     .  .     .        .  .     .  .     .  .     .  .     .    .        .      .        .     .  ",
" ..   ..  .     .  .     .  ..   ..  ..   .   ..   .    .   .     .        .       .    .   .   ..  ",
"  .....   .     .   .....    .... .  . ...    . ...      ...      .         ...     ....     ... .  ",
"                                     .        .                                                     ",
"                                     .        .                                                     ",
"                                     .        .                                                     ",
"                                                                                                    ",
"                                                                                                    "};



GdkPixbuf
*screenshooter_capture_screenshot_wayland (gint     region,
                                           gint     delay,
                                           gboolean show_mouse,
                                           gboolean show_border)
{
  return gdk_pixbuf_new_from_xpm_data (unsupported_image);
}
