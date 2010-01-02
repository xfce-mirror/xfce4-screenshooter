/*  $Id$
 *
 *  Copyright © 2009-2010 Jérôme Guelfucci <jeromeg@xfce.org>
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

#include "screenshooter-simple-job.h"

static void     screenshooter_simple_job_class_init (ScreenshooterSimpleJobClass  *klass);
static void     screenshooter_simple_job_finalize   (GObject                      *object);
static gboolean screenshooter_simple_job_execute    (ExoJob                       *job,
                                                     GError                      **error);



struct _ScreenshooterSimpleJobClass
{
  ScreenshooterJobClass __parent__;
};

struct _ScreenshooterSimpleJob
{
  ScreenshooterJob            __parent__;
  ScreenshooterSimpleJobFunc  func;
  GValueArray                *param_values;
};



static GObjectClass *screenshooter_simple_job_parent_class;



GType
screenshooter_simple_job_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_type_register_static_simple (SCREENSHOOTER_TYPE_JOB,
                                            "ScreenshooterSimpleJob",
                                            sizeof (ScreenshooterSimpleJobClass),
                                            (GClassInitFunc) screenshooter_simple_job_class_init,
                                            sizeof (ScreenshooterSimpleJob),
                                            NULL,
                                            0);
    }

  return type;
}



static void
screenshooter_simple_job_class_init (ScreenshooterSimpleJobClass *klass)
{
  GObjectClass *gobject_class;
  ExoJobClass  *exojob_class;

  /* determine the parent type class */
  screenshooter_simple_job_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = screenshooter_simple_job_finalize;

  exojob_class = EXO_JOB_CLASS (klass);
  exojob_class->execute = screenshooter_simple_job_execute;
}



static void
screenshooter_simple_job_finalize (GObject *object)
{
  ScreenshooterSimpleJob *simple_job = SCREENSHOOTER_SIMPLE_JOB (object);

  /* release the param values */
  g_value_array_free (simple_job->param_values);

  (*G_OBJECT_CLASS (screenshooter_simple_job_parent_class)->finalize) (object);
}



static gboolean
screenshooter_simple_job_execute (ExoJob  *job,
                           GError **error)
{
  ScreenshooterSimpleJob *simple_job = SCREENSHOOTER_SIMPLE_JOB (job);
  gboolean success = TRUE;
  GError *err = NULL;

  g_return_val_if_fail (SCREENSHOOTER_IS_SIMPLE_JOB (job), FALSE);
  g_return_val_if_fail (simple_job->func != NULL, FALSE);

  /* try to execute the job using the supplied function */
  success = (*simple_job->func) (SCREENSHOOTER_JOB (job), simple_job->param_values, &err);

  if (!success)
    {
      g_assert (err != NULL || exo_job_is_cancelled (job));

      /* set error if the job was cancelled. otherwise just propagate
       * the results of the processing function */
      if (exo_job_set_error_if_cancelled (job, error))
        {
          g_clear_error (&err);
        }
      else
        {
          if (err != NULL)
            g_propagate_error (error, err);
        }

      return FALSE;
    }
  else
    return TRUE;
}



/**
 * screenshooter_simple_job_launch:
 * @func           : the #ScreenshooterSimpleJobFunc to execute the job.
 * @n_param_values : the number of parameters to pass to the @func.
 * @...            : a list of #GType and parameter pairs (exactly
 *                   @n_param_values pairs) that are passed to @func.
 *
 * Allocates a new #ScreenshooterSimpleJob, which executes the specified
 * @func with the specified parameters.
 *
 * The caller is responsible to release the returned object using
 * screenshooter_job_unref() when no longer needed.
 *
 * Return value: the launched #ScreenshooterJob.
 **/
ScreenshooterJob *
screenshooter_simple_job_launch (ScreenshooterSimpleJobFunc func,
                                 guint                      n_param_values,
                                 ...)
{
  ScreenshooterSimpleJob *simple_job;
  va_list var_args;
  GValue value = { 0, };
  gchar *error_message;
  guint n;

  /* allocate and initialize the simple job */
  simple_job = g_object_new (SCREENSHOOTER_TYPE_SIMPLE_JOB, NULL);
  simple_job->func = func;
  simple_job->param_values = g_value_array_new (n_param_values);

  /* collect the parameters */
  va_start (var_args, n_param_values);
  for (n = 0; n < n_param_values; ++n)
    {
      /* initialize the value to hold the next parameter */
      g_value_init (&value, va_arg (var_args, GType));

      /* collect the value from the stack */
      G_VALUE_COLLECT (&value, var_args, 0, &error_message);

      /* check if an error occurred */
      if (G_UNLIKELY (error_message != NULL))
        {
          g_error ("%s: %s", G_STRLOC, error_message);
          g_free (error_message);
        }

      g_value_array_insert (simple_job->param_values, n, &value);
      g_value_unset (&value);
    }
  va_end (var_args);

  /* launch the job */
  return SCREENSHOOTER_JOB (exo_job_launch (EXO_JOB (simple_job)));
}



GValueArray *
screenshooter_simple_job_get_param_values (ScreenshooterSimpleJob *job)
{
  g_return_val_if_fail (SCREENSHOOTER_IS_SIMPLE_JOB (job), NULL);

  return job->param_values;
}
