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

#ifndef __SCREENSHOOTER_SIMPLE_JOB_H__
#define __SCREENSHOOTER_SIMPLE_JOB_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "screenshooter-job.h"

#include <gobject/gvaluecollector.h>
#include <string.h>

G_BEGIN_DECLS

/**
 * ScreenshooterSimpleJobFunc:
 * @job            : a #ScreenshooterJob.
 * @param_values   : a #GValueArray of the #GValue<!---->s passed to
 *                   screenshooter_simple_job_launch().
 * @error          : return location for errors.
 *
 * Used by the #ScreenshooterSimpleJob to process the @job. See
 * screenshooter_simple_job_launch() for further details.
 *
 * Return value: %TRUE on success, %FALSE in case of an error.
 **/
typedef gboolean (*ScreenshooterSimpleJobFunc) (ScreenshooterJob *job,
                                                GValueArray      *param_values,
                                                GError           **error);


typedef struct _ScreenshooterSimpleJobClass ScreenshooterSimpleJobClass;
typedef struct _ScreenshooterSimpleJob      ScreenshooterSimpleJob;

#define SCREENSHOOTER_TYPE_SIMPLE_JOB            (screenshooter_simple_job_get_type ())
#define SCREENSHOOTER_SIMPLE_JOB(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SCREENSHOOTER_TYPE_SIMPLE_JOB, ScreenshooterSimpleJob))
#define SCREENSHOOTER_SIMPLE_JOB_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SCREENSHOOTER_TYPE_SIMPLE_JOB, ScreenshooterSimpleJobClass))
#define SCREENSHOOTER_IS_SIMPLE_JOB(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SCREENSHOOTER_TYPE_SIMPLE_JOB))
#define SCREENSHOOTER_IS_SIMPLE_JOB_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), SCREENSHOOTER_TYPE_SIMPLE_JOB))
#define SCREENSHOOTER_SIMPLE_JOB_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SCREENSHOOTER_TYPE_SIMPLE_JOB, ScreenshooterSimpleJobClass))

GType             screenshooter_simple_job_get_type         (void) G_GNUC_CONST;

ScreenshooterJob *screenshooter_simple_job_launch           (ScreenshooterSimpleJobFunc  func,
                                                             guint                       n_param_values,
                                                             ...) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
GValueArray      *screenshooter_simple_job_get_param_values (ScreenshooterSimpleJob     *job);

G_END_DECLS

#endif /* !__SCREENSHOOTER_SIMPLE_JOB_H__ */
