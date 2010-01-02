/*  $Id$
 *
 *  Copyright © 2008-2010 Jérôme Guelfucci <jeromeg@xfce.org>
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

#include "screenshooter-job.h"



#define SCREENSHOOTER_JOB_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), SCREENSHOOTER_TYPE_JOB, ScreenshooterJobPrivate))



/* Signal identifiers */
enum
{
  ASK,
  IMAGE_UPLOADED,
  LAST_SIGNAL,
};



static void  screenshooter_job_class_init (ScreenshooterJobClass *klass);
static void  screenshooter_job_init       (ScreenshooterJob      *job);
static void  screenshooter_job_finalize   (GObject               *object);
/*static void  screenshooter_job_real_ask   (ScreenshooterJob      *job,
                                           GtkListStore          *liststore,
                                           const gchar           *message);*/



static ExoJobClass *screenshooter_job_parent_class;
static guint        job_signals[LAST_SIGNAL];



GType
screenshooter_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (EXO_TYPE_JOB,
                                            "ScreenshooterJob",
                                            sizeof (ScreenshooterJobClass),
                                            (GClassInitFunc) screenshooter_job_class_init,
                                            sizeof (ScreenshooterJob),
                                            (GInstanceInitFunc) screenshooter_job_init,
                                            G_TYPE_FLAG_ABSTRACT);
    }

  return type;
}



static void
screenshooter_job_class_init (ScreenshooterJobClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent class */
  screenshooter_job_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = screenshooter_job_finalize;

  klass->ask = NULL;

  /**
   * ScreenshooterJob::ask:
   * @job     : a #ScreenshooterJob.
   * @info    : a #GtkListStore.
   * @message : message to display to the user.
   *
   * The @message is garantied to contain valid UTF-8.
   *
   * @info contains the information already gathered. The first column tells you the
   * name of the information, the second its contents. Some contents might be empty,
   * it means that the user did not provide the correct information as explained per
   * @message.
   *
   * The callback asks for the information which is not valid and updates @info.
   **/
  job_signals[ASK] =
    g_signal_new ("ask",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_NO_HOOKS | G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ScreenshooterJobClass, ask),
                  NULL, NULL,
                  _screenshooter_marshal_VOID__POINTER_STRING,
                  G_TYPE_NONE,
                  2, G_TYPE_POINTER, G_TYPE_STRING);

  /**
   * ScreenshooterJob::image-uploaded:
   * @job       : a #ScreenshooterJob.
   * @file_name : the name of the uploaded image on ZimageZ.com.
   *
   * This signal is emitted when the upload is finished. If it was successful,
   * @file_name contains the name of the file on ZimageZ.com, else it is NULL.
   **/
  job_signals[IMAGE_UPLOADED] =
    g_signal_new ("image-uploaded",
                  G_TYPE_FROM_CLASS (klass), G_SIGNAL_NO_HOOKS,
                  0, NULL, NULL,
                  _screenshooter_marshal_VOID__STRING,
                  G_TYPE_NONE,
                  1, G_TYPE_STRING);
}



static void
screenshooter_job_init (ScreenshooterJob *job)
{

}



static void
screenshooter_job_finalize (GObject *object)
{
  (*G_OBJECT_CLASS (screenshooter_job_parent_class)->finalize) (object);
}



/*static void
screenshooter_job_real_ask (ScreenshooterJob *job,
                            GtkListStore     *liststore,
                            const gchar      *message)
{
  g_return_if_fail (SCREENSHOOTER_IS_JOB (job));

  TRACE ("Emit ask signal.");
  g_signal_emit (job, job_signals[ASK], 0, liststore, message);
}*/



/* Public */



void screenshooter_job_ask_info (ScreenshooterJob *job,
                                 GtkListStore     *info,
                                 const gchar      *format,
                                 ...)
{
  va_list va_args;
  gchar *message;

  g_return_if_fail (SCREENSHOOTER_IS_JOB (job));
  g_return_if_fail (GTK_IS_LIST_STORE (info));
  g_return_if_fail (format != NULL);

  if (G_UNLIKELY (exo_job_is_cancelled (EXO_JOB (job))))
    return;

  va_start (va_args, format);
  message = g_strdup_vprintf (format, va_args);
  va_end (va_args);

  exo_job_emit (EXO_JOB (job), job_signals[ASK], 0, info, message);

  g_free (message);
}



void
screenshooter_job_image_uploaded (ScreenshooterJob *job, const gchar *file_name)
{
  g_return_if_fail (SCREENSHOOTER_IS_JOB (job));

  TRACE ("Emit image-uploaded signal.");
  exo_job_emit (EXO_JOB (job), job_signals[IMAGE_UPLOADED], 0, file_name);
}
