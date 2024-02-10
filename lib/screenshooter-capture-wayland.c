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

#include <syscall.h>
#include <sys/mman.h>

#include <gdk/gdkwayland.h>
#include <protocols/wlr-screencopy-unstable-v1-client.h>
#include <libxfce4util/libxfce4util.h>

#include "screenshooter-global.h"



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

static void handle_global(void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version);
static void handle_global_remove(void *data, struct wl_registry *reg, uint32_t name);
static void frame_handle_ready(void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec);
static void frame_handle_failed(void *data, struct zwlr_screencopy_frame_v1 *frame);
static void frame_handle_flags(void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t flags);
static void frame_handle_buffer(void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t fmt, uint32_t w, uint32_t h, uint32_t str);



typedef struct {
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct zwlr_screencopy_manager_v1 *screencopy_manager;
  struct wl_buffer *buffer;
  struct wl_shm_pool *pool;
  unsigned char *shm_data;
  int width;
  int height;
  int stride;
  int size;
  uint32_t format;
  gboolean capture_done;
  gboolean capture_failed;
}
WaylandClientData;



/* global registry functions */



static void
handle_global (void *data, struct wl_registry *wl_registry, uint32_t name, const char *interface, uint32_t version)
{
  WaylandClientData *client_data = data;

  if (g_strcmp0 (interface, wl_compositor_interface.name) == 0)
    client_data->compositor = wl_registry_bind (wl_registry, name, &wl_compositor_interface, 1);
  else if (g_strcmp0 (interface, wl_shm_interface.name) == 0)
    client_data->shm = wl_registry_bind (wl_registry, name, &wl_shm_interface, 1);
  else if (g_strcmp0 (interface, zwlr_screencopy_manager_v1_interface.name) == 0)
    client_data->screencopy_manager = wl_registry_bind (wl_registry, name, &zwlr_screencopy_manager_v1_interface, 1);
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
frame_handle_ready (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec)
{
  WaylandClientData *client_data = data;

  TRACE ("buffer copy ready");
  client_data->capture_done = TRUE;
}



static void
frame_handle_failed (void *data, struct zwlr_screencopy_frame_v1 *frame)
{
  WaylandClientData *client_data = data;

  fprintf (stderr, "Failed to capture screencopy frame\n");
  client_data->capture_failed = TRUE;
}



static void
frame_handle_flags (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t flags)
{
  TRACE ("buffer flags received");
}



static void
frame_handle_buffer (void *data, struct zwlr_screencopy_frame_v1 *frame, uint32_t format, uint32_t width, uint32_t height, uint32_t stride)
{
  int fd;
  WaylandClientData *client_data = data;

  client_data->format = format;
  client_data->width = width;
  client_data->height = height;
  client_data->stride = stride;
  client_data->size = stride * height;

  fd = syscall (SYS_memfd_create, "buffer", 0);
  if (fd == -1)
    {
      fprintf (stderr, "Failed to create fd\n");
      abort();
    }
  ftruncate (fd, client_data->size);

  client_data->shm_data = mmap (NULL, client_data->size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (client_data->shm_data == MAP_FAILED)
    {
      fprintf (stderr, "Failed to map memory\n");
      close (fd);
      abort ();
    }

  client_data->pool = wl_shm_create_pool (client_data->shm, fd, client_data->size);
  client_data->buffer = wl_shm_pool_create_buffer (client_data->pool, 0, width, height, stride, format);
  close (fd);

  zwlr_screencopy_frame_v1_copy (frame, client_data->buffer);
  /* wait for flags and ready events */
}



const struct zwlr_screencopy_frame_v1_listener frame_listener = {
    .ready = frame_handle_ready,
    .failed = frame_handle_failed,
    .flags = frame_handle_flags,
    .buffer = frame_handle_buffer
};



static GdkPixbuf
*convert_buffer_to_pixbuf (WaylandClientData *client_data)
{
  guint8 *data;
  guchar *pixels;
  GdkPixbuf *pixbuf = NULL;

  data = client_data->shm_data;
  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, client_data->width, client_data->height);
  pixels = gdk_pixbuf_get_pixels (pixbuf);

  if (client_data->format == WL_SHM_FORMAT_ARGB8888 || client_data->format == WL_SHM_FORMAT_XRGB8888)
    {
      for (int y = 0; y < client_data->height; y++)
        {
          for (int x = 0; x < client_data->width; x++)
            {
              gint offset = y * client_data->stride + x * 4;
              guint32 *px = (guint32 *)(data + offset);
              pixels[offset + 0] = (*px >> 16) & 0xFF;  /* red */
              pixels[offset + 1] = (*px >> 8)  & 0xFF;  /* green */
              pixels[offset + 2] =  *px        & 0xFF;  /* blue */
              pixels[offset + 3] = (*px >> 24) & 0xFF;  /* alpha */
            }
        }
    }
  else if (client_data->format == WL_SHM_FORMAT_ABGR8888 || client_data->format == WL_SHM_FORMAT_XBGR8888)
    {
      for (int y = 0; y < client_data->height; y++)
        {
          for (int x = 0; x < client_data->width; x++)
            {
              gint offset = y * client_data->stride + x * 4;
              guint32 *px = (guint32 *)(data + offset);
              pixels[offset + 0] =  *px        & 0xFF; /* red */
              pixels[offset + 1] = (*px >> 8)  & 0xFF; /* green */
              pixels[offset + 2] = (*px >> 16) & 0xFF; /* blue */
              pixels[offset + 3] = (*px >> 24) & 0xFF; /* alpha */
            }
        }
    }
  else
    {
      fprintf (stderr, "unsupported format %d\n", client_data->format);
      return NULL;
    }

  return pixbuf;
}



GdkPixbuf
*screenshooter_capture_screenshot_wayland (gint     region,
                                           gint     delay,
                                           gboolean show_mouse,
                                           gboolean show_border)
{
  GdkDisplay *gdk_display;
  GdkMonitor *monitor;
  struct wl_display *display;
  struct wl_registry *registry;
  struct wl_output *output;
  struct zwlr_screencopy_frame_v1 *frame;
  WaylandClientData client_data = {};
  GdkPixbuf *screenshot = NULL;

  if (region != FULLSCREEN)
    return gdk_pixbuf_new_from_xpm_data (unsupported_image);

  /* Only fullscreen is supported for now */
  TRACE ("Get the screenshot of the entire screen");

  gdk_display = gdk_display_get_default ();
  display = gdk_wayland_display_get_wl_display (gdk_display);
  registry = wl_display_get_registry (display);

  wl_registry_add_listener (registry, &registry_listener, &client_data);
  wl_display_roundtrip (display);

  if (client_data.compositor == NULL)
    {
      fprintf (stderr, "Required Wayland interfaces are missing\n");
      return NULL;
    }
  if (client_data.shm == NULL)
    {
      fprintf (stderr, "Compositor is missing wl_shm\n");
      return NULL;
    }
  if (client_data.screencopy_manager == NULL)
    {
      fprintf (stderr, "Compositor does not support wlr-screencopy-unstable-v1\n");
      return NULL;
    }

  monitor = gdk_display_get_primary_monitor (gdk_display);
  if (monitor == NULL)
    monitor = gdk_display_get_monitor (gdk_display, 0);
  output = gdk_wayland_monitor_get_wl_output (monitor);

  if (output == NULL)
    {
      fprintf (stderr, "No output available\n");
      return NULL;
    }

  frame = zwlr_screencopy_manager_v1_capture_output (client_data.screencopy_manager, 0, output);
  zwlr_screencopy_frame_v1_add_listener (frame, &frame_listener, &client_data);

  while (!client_data.capture_done && !client_data.capture_failed)
    wl_display_dispatch (display);

  if (client_data.capture_done)
    screenshot = convert_buffer_to_pixbuf (&client_data);
  else
    fprintf (stderr, "Failed to capture\n");

  munmap (client_data.shm_data, client_data.size);

  return screenshot;
}
