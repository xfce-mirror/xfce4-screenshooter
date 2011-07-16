/*  $Id$
 *
 *  Copyright © 2009 Sebastian Waisbrot <seppo0010@gmail.com>
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

#ifndef __HAVE_IMGUR_H__
#define __HAVE_IMGUR_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gstdio.h>
#include <libsoup/soup.h>

#include "screenshooter-utils.h"
#include "screenshooter-simple-job.h"
#include "sexy-url-label.h"
#include "katze-throbber.h"

void screenshooter_upload_to_imgur (const gchar  *image_path,
                                    const gchar  *last_user,
                                    const gchar  *title,
                                    gchar       **new_last_user);


#endif
