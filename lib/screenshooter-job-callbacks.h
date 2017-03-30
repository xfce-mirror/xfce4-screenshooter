/*  $Id$
 *
 *  Copyright 2008-2009 Jérôme Guelfucci <jeromeg@xfce.org>
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

#ifndef __HAVE_JOB_CB_H__
#define __HAVE_JOB_CB_H__
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "screenshooter-utils.h"
#include "screenshooter-simple-job.h"

typedef enum
{
  USER,
  PASSWORD,
  TITLE,
  COMMENT,
} AskInformation;


GtkWidget *
create_spinner_dialog              (const gchar        *title,
                                    GtkWidget          **label);

void
cb_error                           (ExoJob            *job,
                                    GError            *error,
                                    gpointer           unused);
void
cb_finished                        (ExoJob            *job,
                                    GtkWidget         *dialog);
void
cb_update_info                     (ExoJob            *job,
                                    gchar             *message,
                                    GtkWidget         *label);
void
cb_image_uploaded                  (ScreenshooterJob  *job,
                                    gchar             *upload_name,
                                    gchar            **last_user);
void
cb_ask_for_information             (ScreenshooterJob  *job,
                                    GtkListStore      *liststore,
                                    const gchar       *message,
                                    gpointer           unused);

#endif
