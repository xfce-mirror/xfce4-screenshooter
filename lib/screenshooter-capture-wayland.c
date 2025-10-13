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

#include "screenshooter-capture-wayland.h"

#include <sys/mman.h>

#include <gdk/gdkwayland.h>
#include <protocols/wlr-screencopy-unstable-v1-client.h>
#include <protocols/ext-image-capture-source-v1-client.h>
#include <protocols/ext-image-copy-source-v1-client.h>
#include <libxfce4util/libxfce4util.h>

#include "screenshooter-global.h"
#include "screenshooter-select.h"
#include "screenshooter-utils.h"



/* Internals */


typedef struct {
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct zwlr_screencopy_manager_v1 *screencopy_manager;
  struct ext_output_image_capture_source_manager_v1 *output_image_capture_source_manager;
  struct ext_image_copy_capture_manager_v1 *image_copy_capture_manager;
}
ClientData;

typedef struct {
  ClientData *client_data;
  GdkMonitor *monitor;
  gchar *name;
  struct zwlr_screencopy_frame_v1 *frame;
  struct wl_buffer *buffer;
  struct wl_shm_pool *pool;
  unsigned char *shm_data;
  int width;
  int height;
  int stride;
  int size;
  enum wl_shm_format format;
  gboolean has_format;
  gboolean capture_done;
  gboolean capture_failed;
  gboolean skip_composition;
  enum wl_output_transform transform;
  struct ext_image_copy_capture_session_v1 *image_copy_capture_session;
  struct ext_image_copy_capture_frame_v1 *image_copy_capture_frame;
}
OutputData;



static void handle_global (void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
static void handle_global_remove (void *data, struct wl_registry *reg, uint32_t name);
static void handle_frame_ready (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec);
static void handle_frame_failed (void *data, struct zwlr_screencopy_frame_v1 *frame);
static void handle_frame_flags (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t flags);
static void handle_frame_buffer (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t fmt, uint32_t w, uint32_t h, uint32_t str);

static void handle_image_copy_capture_frame_transform (void *data, struct ext_image_copy_capture_frame_v1 *frame, uint32_t transform);
static void handle_image_copy_capture_frame_damage (void *data, struct ext_image_copy_capture_frame_v1 *frame, int32_t x, int32_t y, int32_t wdth, int32_t height);
static void handle_image_copy_capture_frame_presentation_time (void *data, struct ext_image_copy_capture_frame_v1 *frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec);
static void handle_image_copy_capture_frame_ready (void *data, struct ext_image_copy_capture_frame_v1 *frame);
static void handle_image_copy_capture_frame_failed (void *data, struct ext_image_copy_capture_frame_v1 *frame, uint32_t reason);

static void screenshooter_free_client_data (ClientData *client_data);
static void screenshooter_free_output_data (gpointer data);
static gboolean screenshooter_initialize_client_data (ClientData *client_data);
static GdkPixbuf *screenshooter_compose_screenshot (GList *outputs);
static gboolean screenshooter_is_shm_format_supported (enum wl_shm_format format);
static gint screenshooter_get_bpp_from_format (enum wl_shm_format format);



void
screenshooter_free_client_data (ClientData *client_data)
{
  if (client_data->compositor != NULL)
    wl_compositor_destroy (client_data->compositor);
  if (client_data->shm != NULL)
    wl_shm_destroy (client_data->shm);
  if (client_data->screencopy_manager != NULL)
    zwlr_screencopy_manager_v1_destroy (client_data->screencopy_manager);
  if (client_data->output_image_capture_source_manager != NULL)
    ext_output_image_capture_source_manager_v1_destroy (client_data->output_image_capture_source_manager);
  if (client_data->image_copy_capture_manager != NULL)
    ext_image_copy_capture_manager_v1_destroy(client_data->image_copy_capture_manager);

  wl_registry_destroy (client_data->registry);
  /* do not destroy client_data->display because it is owned by gdk */
}



void
screenshooter_free_output_data (gpointer data)
{
  OutputData *output = data;

  if (output->shm_data != NULL)
    munmap (output->shm_data, output->size);
  if (output->buffer != NULL)
    wl_buffer_destroy (output->buffer);
  if (output->frame != NULL)
    zwlr_screencopy_frame_v1_destroy (output->frame);
  if (output->image_copy_capture_session != NULL)
    ext_image_copy_capture_session_v1_destroy (output->image_copy_capture_session);
  if (output->image_copy_capture_frame != NULL)
    ext_image_copy_capture_frame_v1_destroy (output->image_copy_capture_frame);

  g_free (output);
}



/* global registry functions */



static void
handle_global (void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version)
{
  ClientData *client_data = data;

  if (g_strcmp0 (interface, wl_compositor_interface.name) == 0)
    client_data->compositor = wl_registry_bind (wl_registry, name, &wl_compositor_interface, 1);
  else if (g_strcmp0 (interface, wl_shm_interface.name) == 0)
    client_data->shm = wl_registry_bind (wl_registry, name, &wl_shm_interface, 1);
  else if (g_strcmp0 (interface, zwlr_screencopy_manager_v1_interface.name) == 0)
    client_data->screencopy_manager = wl_registry_bind (wl_registry, name, &zwlr_screencopy_manager_v1_interface, 1);
  else if (g_strcmp0 (interface, ext_output_image_capture_source_manager_v1_interface.name) == 0)
    client_data->output_image_capture_source_manager = wl_registry_bind (wl_registry, name, &ext_output_image_capture_source_manager_v1_interface, 1);
  else if (g_strcmp0 (interface, ext_image_copy_capture_manager_v1_interface.name) == 0)
    client_data->image_copy_capture_manager = wl_registry_bind(wl_registry, name, &ext_image_copy_capture_manager_v1_interface, 1);

}



static void
handle_global_remove (void *data, struct wl_registry *reg, uint32_t name)
{
  /* do nothing */
}



const struct wl_registry_listener registry_listener = {
  .global = handle_global,
  .global_remove = handle_global_remove
};



/* screencopy frame functions */



static void
handle_frame_ready (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec)
{
  OutputData *output = data;

  TRACE ("buffer copy ready");
  output->capture_done = TRUE;
}



static void
handle_frame_failed (void *data, struct zwlr_screencopy_frame_v1 *frame)
{
  OutputData *output = data;

  screenshooter_error (_("Failed to capture screencopy frame"));
  output->capture_failed = TRUE;
}



static void
handle_frame_flags (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t flags)
{
  TRACE ("buffer flags received");
}



static void
handle_frame_buffer (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t format, uint32_t width, uint32_t height, uint32_t stride)
{
  int fd;
  char template[] = "/tmp/screenshooter-buffer-XXXXXX";
  OutputData *output = data;

  output->format = format;
  output->width = width;
  output->height = height;
  output->stride = stride;
  output->size = stride * height;

  fd = mkstemp (template);
  if (fd == -1)
    {
      screenshooter_error (_("Failed to create file descriptor"));
      g_abort ();
    }
  ftruncate (fd, output->size);
  unlink (template);

  output->shm_data = mmap (NULL, output->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (output->shm_data == MAP_FAILED)
    {
      screenshooter_error (_("Failed to map memory"));
      close (fd);
      g_abort ();
    }

  output->pool = wl_shm_create_pool (output->client_data->shm, fd, output->size);
  output->buffer = wl_shm_pool_create_buffer (output->pool, 0, width, height, stride, format);
  close (fd);
  wl_shm_pool_destroy (output->pool);

  zwlr_screencopy_frame_v1_copy (frame, output->buffer);
  /* wait for flags and ready events */
}



const struct zwlr_screencopy_frame_v1_listener frame_listener = {
    .ready = handle_frame_ready,
    .failed = handle_frame_failed,
    .flags = handle_frame_flags,
    .buffer = handle_frame_buffer
};



/* ext image capture/copy frame functions */



static void
handle_image_copy_capture_frame_transform (void *data, struct ext_image_copy_capture_frame_v1 *frame, uint32_t transform)
{
  OutputData *output = data;
  output->transform = transform;
}



static void
handle_image_copy_capture_frame_damage (void *data, struct ext_image_copy_capture_frame_v1 *frame, int32_t x, int32_t y, int32_t width, int32_t height)
{
  /* do nothing */
}



static void
handle_image_copy_capture_frame_presentation_time (void *data, struct ext_image_copy_capture_frame_v1 *frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec)
{
  /* do nothing */
}



static void
handle_image_copy_capture_frame_ready (void *data, struct ext_image_copy_capture_frame_v1 *frame)
{
  OutputData *output = data;

  TRACE ("buffer copy ready");
  output->capture_done = TRUE;
}



static void
handle_image_copy_capture_frame_failed (void *data, struct ext_image_copy_capture_frame_v1 *frame, uint32_t reason)
{
  OutputData *output = data;
  screenshooter_error (_("Failed to capture frame"));
  output->capture_failed = TRUE;
}



const struct ext_image_copy_capture_frame_v1_listener image_copy_capture_frame_listener = {
  .transform = handle_image_copy_capture_frame_transform,
  .damage = handle_image_copy_capture_frame_damage,
  .presentation_time = handle_image_copy_capture_frame_presentation_time,
  .ready = handle_image_copy_capture_frame_ready,
  .failed = handle_image_copy_capture_frame_failed,
};



/* ext image capture/copy session functions */



static void
handle_image_copy_capture_session_buffer_size (void *data, struct ext_image_copy_capture_session_v1 *session, uint32_t width, uint32_t height)
{
  OutputData *output = data;
  output->width = width;
  output->height = height;
}



static void
handle_image_copy_capture_session_shm_format (void *data, struct ext_image_copy_capture_session_v1 *session, uint32_t format)
{
  OutputData *output = data;

  if (output->has_format || !screenshooter_is_shm_format_supported (format))
    {
      return;
    }

  output->format = format;
  output->has_format = TRUE;
}



static void
handle_image_copy_capture_session_dmabuf_device (void *data, struct ext_image_copy_capture_session_v1 *session, struct wl_array *dev_id_array)
{
  /* do nothing */
}



static void
handle_image_copy_capture_session_dmabuf_format (void *data, struct ext_image_copy_capture_session_v1 *session, uint32_t format, struct wl_array *modifiers)
{
  /* do nothing */
}



static void
handle_image_copy_capture_session_done (void *data, struct ext_image_copy_capture_session_v1 *session)
{
  int fd;
  char template[] = "/tmp/screenshooter-buffer-XXXXXX";
  OutputData *output = data;

  output->stride = output->width * screenshooter_get_bpp_from_format (output->format);
  output->size = output->stride * output->height;

  if (output->image_copy_capture_frame != NULL)
    {
      return;
    }

  if (!output->has_format)
    {
      screenshooter_error (_("Supported format not found"));
      g_abort ();
    }

  fd = mkstemp (template);
  if (fd == -1)
    {
      screenshooter_error (_("Failed to create file descriptor"));
      g_abort ();
    }
  ftruncate (fd, output->size);
  unlink (template);

  output->shm_data = mmap (NULL, output->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (output->shm_data == MAP_FAILED)
    {
      screenshooter_error (_("Failed to map memory"));
      close (fd);
      g_abort ();
    }

  output->pool = wl_shm_create_pool (output->client_data->shm, fd, output->size);
  output->buffer = wl_shm_pool_create_buffer (output->pool, 0, output->width, output->height, output->stride, output->format);
  close (fd);
  wl_shm_pool_destroy (output->pool);

  if (output->buffer == NULL)
    {
      screenshooter_error (_("Failed to create buffer"));
      g_abort ();
    }

  output->image_copy_capture_frame = ext_image_copy_capture_session_v1_create_frame (session);
  ext_image_copy_capture_frame_v1_add_listener (output->image_copy_capture_frame, &image_copy_capture_frame_listener, output);
  ext_image_copy_capture_frame_v1_attach_buffer (output->image_copy_capture_frame, output->buffer);
  ext_image_copy_capture_frame_v1_damage_buffer (output->image_copy_capture_frame, 0, 0, INT32_MAX, INT32_MAX);
  ext_image_copy_capture_frame_v1_capture (output->image_copy_capture_frame);
}



static void
handle_image_copy_capture_session_stopped (void *data, struct ext_image_copy_capture_session_v1 *session)
{
  /* do nothing */
}



static const struct ext_image_copy_capture_session_v1_listener image_copy_capture_session_listener = {
  .buffer_size = handle_image_copy_capture_session_buffer_size,
  .shm_format = handle_image_copy_capture_session_shm_format,
  .dmabuf_device = handle_image_copy_capture_session_dmabuf_device,
  .dmabuf_format = handle_image_copy_capture_session_dmabuf_format,
  .done = handle_image_copy_capture_session_done,
  .stopped = handle_image_copy_capture_session_stopped
};



/* Wayland client initialization */



gboolean
screenshooter_initialize_client_data (ClientData *client_data)
{
  client_data->display = gdk_wayland_display_get_wl_display (gdk_display_get_default ());
  client_data->registry = wl_display_get_registry (client_data->display);

  wl_registry_add_listener (client_data->registry, &registry_listener, client_data);
  wl_display_roundtrip (client_data->display);

  if (client_data->compositor == NULL)
    {
      screenshooter_error (_("Required Wayland interfaces are missing"));
      return FALSE;
    }
  if (client_data->shm == NULL)
    {
      screenshooter_error (_("Compositor is missing wl_shm"));
      return FALSE;
    }
  if (client_data->screencopy_manager == NULL && (client_data->output_image_capture_source_manager == NULL || client_data->image_copy_capture_manager == NULL))
    {
      screenshooter_error (_("Compositor does not support wlr-screencopy-unstable-v1 nor ext-image-capture-source-v1/ext-image-copy-capture-v1"));
      return FALSE;
    }

  return TRUE;
}



/* Pixbuf manipulation functions */



static gboolean
screenshooter_is_shm_format_supported (enum wl_shm_format format)
{
  return format == WL_SHM_FORMAT_ARGB8888 ||
         format == WL_SHM_FORMAT_XRGB8888 ||
         format == WL_SHM_FORMAT_ABGR8888 ||
         format == WL_SHM_FORMAT_XBGR8888 ||
         format == WL_SHM_FORMAT_BGR888;
}



static gint
screenshooter_get_bpp_from_format (enum wl_shm_format format)
{
  switch (format)
    {
      case WL_SHM_FORMAT_ARGB8888:
      case WL_SHM_FORMAT_XRGB8888:
      case WL_SHM_FORMAT_ABGR8888:
      case WL_SHM_FORMAT_XBGR8888:
        return 4;
      case WL_SHM_FORMAT_BGR888:
        return 3;
      default:
        return 0;
    }
}



static GdkPixbuf
*screenshooter_convert_buffer_to_pixbuf (OutputData *output)
{
  guint8 *data = output->shm_data;
  enum wl_shm_format format = output->format;
  gboolean has_alpha = TRUE;

  if (format == WL_SHM_FORMAT_ARGB8888 || format == WL_SHM_FORMAT_XRGB8888)
    {
      for (int y = 0; y < output->height; y++)
        {
          for (int x = 0; x < output->width; x++)
            {
              gint offset = y * output->stride + x * 4;
              guint32 *px = (guint32 *)(gpointer)(data + offset);
              guint8 blue = *px & 0xFF;
              guint8 green = (*px >> 8) & 0xFF;
              guint8 red = (*px >> 16) & 0xFF;
              guint8 alpha = (*px >> 24) & 0xFF;
              data[offset + 0] = red;
              data[offset + 1] = green;
              data[offset + 2] = blue;
              data[offset + 3] = alpha;
            }
        }
    }
  else if (format == WL_SHM_FORMAT_ABGR8888 || format == WL_SHM_FORMAT_XBGR8888)
    {
      for (int y = 0; y < output->height; y++)
        {
          for (int x = 0; x < output->width; x++)
            {
              gint offset = y * output->stride + x * 4;
              guint32 *px = (guint32 *)(gpointer)(data + offset);
              guint8 red = *px & 0xFF;
              guint8 green = (*px >> 8) & 0xFF;
              guint8 blue = (*px >> 16) & 0xFF;
              guint8 alpha = (*px >> 24) & 0xFF;
              data[offset + 0] = red;
              data[offset + 1] = green;
              data[offset + 2] = blue;
              data[offset + 3] = alpha;
            }
        }
    }
  else if (format == WL_SHM_FORMAT_BGR888)
    {
      has_alpha = FALSE;

      for (int y = 0; y < output->height; y++)
        {
          for (int x = 0; x < output->width; x++)
            {
              gint offset = y * output->stride + x * 3;
              guint8 *px = (guint8 *)(gpointer)(data + offset);
              guint8 blue = px[2];
              guint8 green = px[1];
              guint8 red = px[0];
              data[offset + 0] = red;
              data[offset + 1] = green;
              data[offset + 2] = blue;
            }
        }
    }
  else
    {
      screenshooter_error (_("Unsupported pixel format: 0x%x"), format);
      return NULL;
    }

  return gdk_pixbuf_new_from_data (data, GDK_COLORSPACE_RGB, has_alpha, 8, output->width, output->height, output->stride, NULL, NULL);
}



GdkPixbuf
*screenshooter_compose_screenshot (GList *outputs)
{
  GdkRectangle geometry;
  GdkPixbuf *dest = NULL;
  int max_width = 0, max_height = 0;

  /* find the maximum dimensions */
  for (GList *iter = outputs; iter != NULL; iter = iter->next)
    {
      OutputData *output = (OutputData *) iter->data;
      gdk_monitor_get_geometry (output->monitor, &geometry);
      max_width = MAX (max_width, geometry.x + geometry.width);
      max_height = MAX (max_height, geometry.y + geometry.height);
    }

  /* create a new destination pixbuf */
  dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, max_width, max_height);
  if (dest == NULL)
    {
      g_warning ("Failed to create destination pixbuf.");
      return NULL;
    }

  /* fill with transparency */
  gdk_pixbuf_fill (dest, 0x00000000);

  /* composite each screenshot onto the destination pixbuf */
  for (GList *iter = outputs; iter != NULL; iter = iter->next)
    {
      GdkPixbuf *pixbuf;
      OutputData *output = (OutputData *) iter->data;
      if (output->skip_composition)
        continue;
      pixbuf = screenshooter_convert_buffer_to_pixbuf (output);
      gdk_monitor_get_geometry (output->monitor, &geometry);
      gdk_pixbuf_composite (pixbuf, dest,
                            geometry.x, geometry.y,
                            geometry.width, geometry.height,
                            geometry.x, geometry.y,
                            1.0, 1.0,
                            GDK_INTERP_BILINEAR,
                            255);
      g_object_unref (pixbuf);
    }

  return dest;
}



static GdkPixbuf
*screenshooter_capture_fullscreen (gboolean show_mouse,
                                   gboolean show_border,
                                   GdkRectangle* selection_region)
{
  int n_monitors;
  gboolean failure = FALSE;
  GList *outputs = NULL;
  ClientData client_data = {};
  GdkPixbuf *screenshot = NULL;

  TRACE ("Get the screenshot of the entire screen");

  /* initializate the wayland client */
  if (!screenshooter_initialize_client_data (&client_data))
    {
      screenshooter_free_client_data (&client_data);
      return NULL;
    }

  /* collect outputs (monitors) */
  n_monitors = gdk_display_get_n_monitors (gdk_display_get_default ());
  for (int i = 0; i < n_monitors; i++)
    {
      OutputData *output;
      GdkRectangle monitor_geometry;
      GdkMonitor *monitor = gdk_display_get_monitor (gdk_display_get_default (), i);
      struct wl_output *wl_output = gdk_wayland_monitor_get_wl_output (monitor);
      if (wl_output == NULL)
        {
          g_warning ("No output available for monitor %d", i);
          continue;
        }

      output = g_new0 (OutputData, 1);
      outputs = g_list_append (outputs, output);
      output->monitor = monitor;
      output->client_data = &client_data;

      /* Do not capture output if the selection region is present and is not in the monitor */
      if (selection_region != NULL)
        {
          gdk_monitor_get_geometry (monitor, &monitor_geometry);
          if (!gdk_rectangle_intersect (&monitor_geometry, selection_region, NULL))
            {
              output->capture_done = TRUE;
              output->skip_composition = TRUE;
              output->width = monitor_geometry.width;
              output->height = monitor_geometry.height;
              continue;
            }
        }

      if (client_data.output_image_capture_source_manager != NULL)
        {
          guint options = show_mouse ? EXT_IMAGE_COPY_CAPTURE_MANAGER_V1_OPTIONS_PAINT_CURSORS : 0;
          struct ext_image_capture_source_v1 *source = ext_output_image_capture_source_manager_v1_create_source (client_data.output_image_capture_source_manager, wl_output);
          output->image_copy_capture_session = ext_image_copy_capture_manager_v1_create_session (client_data.image_copy_capture_manager, source, options);
          ext_image_copy_capture_session_v1_add_listener (output->image_copy_capture_session, &image_copy_capture_session_listener, output);
          ext_image_capture_source_v1_destroy (source);
        }
      else
        {
          output->frame = zwlr_screencopy_manager_v1_capture_output (client_data.screencopy_manager, show_mouse, wl_output);
          zwlr_screencopy_frame_v1_add_listener (output->frame, &frame_listener, output);
        }
    }

  /* wait for the capture of all outputs */
  while (TRUE)
    {
      gboolean done = TRUE;

      for (GList *elem = outputs; elem; elem = elem->next)
        {
          OutputData *output = elem->data;
          if (!output->capture_done && !output->capture_failed)
            done = FALSE;
          if (output->capture_failed)
            failure = TRUE;
        }

      if (done)
        break;

      wl_display_dispatch (client_data.display);
    }

  /* check the result */
  if (failure)
    screenshooter_error (_("Failed to capture"));
  else
    screenshot = screenshooter_compose_screenshot (outputs);

  screenshooter_free_client_data (&client_data);
  g_list_free_full (outputs, screenshooter_free_output_data);

  return screenshot;
}



static GdkPixbuf
*screenshooter_capture_region (gint     delay,
                               gboolean show_mouse,
                               gboolean show_border)
{

  GdkRectangle region;
  GdkPixbuf *screenshot = NULL, *clipped;
  int root_width, root_height;

  if (G_UNLIKELY (!screenshooter_select_region (&region)))
    return NULL;

  /* Clear the display before capturing */
  wl_display_roundtrip (gdk_wayland_display_get_wl_display (gdk_display_get_default ()));

    /* Await the specified delay, but not less than 200ms */
  if (delay == 0)
    g_usleep (200000);
  else
    sleep (delay);

  screenshot = screenshooter_capture_fullscreen (show_mouse, show_border, &region);
  root_width = gdk_pixbuf_get_width (screenshot);
  root_height = gdk_pixbuf_get_height (screenshot);

  /* Avoid rectangle parts outside the screen */
  if (region.x < 0)
    region.width += region.x;
  if (region.y < 0)
    region.height += region.y;

  region.x = MAX (0, MIN (region.x, root_width));
  region.y = MAX (0, MIN (region.y, root_height));

  if (region.x + region.width > root_width)
    region.width = root_width - region.x;
  if (region.y + region.height > root_height)
    region.height = root_height - region.y;

  /* Avoid regions if 0 width or height */
  if (region.width == 0 && region.x == root_width)
    {
      region.width = 1;
      region.x--;
    }
  if (region.height == 0 && region.y == root_height)
    {
      region.height = 1;
      region.y--;
    }


  clipped = gdk_pixbuf_new_subpixbuf (screenshot, region.x, region.y, region.width, region.height);
  g_object_unref (screenshot);

  return clipped;
}



/* Public */



GdkPixbuf
*screenshooter_capture_screenshot_wayland (gint     region,
                                           gint     delay,
                                           gboolean show_mouse,
                                           gboolean show_border)
{
  if (region == FULLSCREEN)
    {
      return screenshooter_capture_fullscreen (show_mouse, show_border, NULL);
    }
  if (region == SELECT)
    {
      return screenshooter_capture_region (delay, show_mouse, show_border);
    }
  else
    {
      screenshooter_error (_("The selected mode is not supported in Wayland"));
      return NULL;
    }
}
