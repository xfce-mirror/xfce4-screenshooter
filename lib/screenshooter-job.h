/*  $Id$
 *
 *  Copyright © 2008-2009 Jérôme Guelfucci <jeromeg@xfce.org>
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

#ifndef __SCREENSHOOTER_JOB_H__
#define __SCREENSHOOTER_JOB_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>
#include <gtk/gtk.h>
#include <libxfce4util/libxfce4util.h>

#include "exo-simple-job.h"
#include "screenshooter-marshal.h"

G_BEGIN_DECLS

typedef struct _ScreenshooterJobClass   ScreenshooterJobClass;
typedef struct _ScreenshooterJob        ScreenshooterJob;

#define SCREENSHOOTER_TYPE_JOB            (screenshooter_job_get_type ())
#define SCREENSHOOTER_JOB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SCREENSHOOTER_TYPE_JOB, ScreenshooterJob))
#define SCREENSHOOTER_JOB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SCREENSHOOTER_TYPE_JOB, ScreenshooterJobClass))
#define SCREENSHOOTER_IS_JOB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SCREENSHOOTER_TYPE_JOB))
#define SCREENSHOOTER_IS_JOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SCREENSHOOTER_TYPE_JOB))
#define SCREENSHOOTER_JOB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SCREENSHOOTER_TYPE_JOB, ScreenshooterJobClass))

struct _ScreenshooterJobClass
{
  /*< private >*/
  ExoJobClass __parent__;

  /*< public >*/

  /* signals */
  void (*ask) (ScreenshooterJob *job, GtkListStore *info, const gchar *message);
};

struct _ScreenshooterJob
{
  /*< private >*/
  ExoJob __parent__;
};

GType screenshooter_job_get_type       (void) G_GNUC_CONST;

void  screenshooter_job_ask_info       (ScreenshooterJob *job,
                                        GtkListStore     *info,
                                        const gchar      *format,
                                        ...);


void  screenshooter_job_image_uploaded (ScreenshooterJob *job,
                                        const gchar      *file_name);

G_END_DECLS

#endif /* !__SCREENSHOOTER_JOB_H__ */

