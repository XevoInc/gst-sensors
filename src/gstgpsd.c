/* GStreamer gpsd Source
 * Copyright (C) 2017 Xevo Inc
 *   Author: Martin Kelly <mkelly@xevo.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */
/**
 * SECTION:element-gpsdsrc
 * @title: gstgpsdsrc
 *
 * The gpsdsrc element reads GPS data from gpsd, the GPS Daemon.
 *
 * ## Example launch line
 * |[
 * gst-launch -v gpsdsrc ! fakesink
 * ]|
 * Push GPS data into a fakesink.
 *
 */

#define _POSIX_C_SOURCE 200809L

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <gst/gst.h>
#include <gst/gstclock.h>
#include <gst/base/gstbasesrc.h>
#include <gst/base/gstpushsrc.h>
#include <string.h>
#include "gstgpsd.h"

#define POLL_HAS_DATA (POLLIN|POLLPRI)

GST_DEBUG_CATEGORY_STATIC (gst_gpsd_src_debug);
#define GST_CAT_DEFAULT gst_gpsd_src_debug

#define parent_class gst_gpsd_src_parent_class

#define GPSD_ERROR_STR(err) \
  ("gpsd err: %d (%s)", err, gps_errstr (err))

/* GObject */
static void gst_gpsd_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_gpsd_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static void gst_gpsd_src_dispose (GObject * object);

/* GstBaseSrc */
static gboolean gst_gpsd_src_start (GstBaseSrc * src);
static gboolean gst_gpsd_src_stop (GstBaseSrc * src);
static gboolean gst_gpsd_src_get_size (GstBaseSrc * src, guint64 * size);
static gboolean gst_gpsd_src_is_seekable (GstBaseSrc * src);
static gboolean gst_gpsd_src_unlock (GstBaseSrc * src);
static gboolean gst_gpsd_src_unlock_stop (GstBaseSrc * src);

/* GstPushSrc */
static GstFlowReturn gst_gpsd_src_create (GstPushSrc * src, GstBuffer ** buf);

enum
{
  PROP_0,
  PROP_HOST,
  PROP_PORT,
  PROP_TIMESTAMP,
  PROP_LAST
};

static GstStaticPadTemplate gst_gpsd_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/gpsd"));

G_DEFINE_TYPE_WITH_CODE (GstGpsdSrc, gst_gpsd_src, GST_TYPE_PUSH_SRC,
    GST_DEBUG_CATEGORY_INIT (gst_gpsd_src_debug, "gpsdsrc", 0, "gpsdsrc"))

enum
{
  /* Get timestamps from the GPS. */
  TIMESTAMP_GPS,
  /* Get timestamps from the pipeline. */
  TIMESTAMP_PIPELINE
};

#define GST_TYPE_GPSD_TIMESTAMP_SOURCE (gst_gpsd_src_get_sensor_delay ())
static GType
gst_gpsd_src_get_sensor_delay (void)
{
  static GType gst_gpsd_src_timestamp_source = 0;

  if (!gst_gpsd_src_timestamp_source) {
    static GEnumValue timestamp_source[] = {
      {.value = TIMESTAMP_GPS,.value_name = "gps",.value_nick = "gps"},
      {.value = TIMESTAMP_PIPELINE,.value_name = "pipeline",.value_nick =
            "pipeline"}
    };
    gst_gpsd_src_timestamp_source =
        g_enum_register_static ("GstGpsdSrcTimestampSource", timestamp_source);
  }

  return gst_gpsd_src_timestamp_source;
}

static void
gst_gpsd_src_class_init (GstGpsdSrcClass * klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS (klass);
  GstElementClass *element_class = GST_ELEMENT_CLASS (klass);
  GstPushSrcClass *push_src_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->set_property = gst_gpsd_src_set_property;
  gobject_class->get_property = gst_gpsd_src_get_property;
  gobject_class->dispose = gst_gpsd_src_dispose;

  base_src_class->start = GST_DEBUG_FUNCPTR (gst_gpsd_src_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR (gst_gpsd_src_stop);
  base_src_class->get_size = GST_DEBUG_FUNCPTR (gst_gpsd_src_get_size);
  base_src_class->is_seekable = GST_DEBUG_FUNCPTR (gst_gpsd_src_is_seekable);
  base_src_class->unlock = GST_DEBUG_FUNCPTR (gst_gpsd_src_unlock);
  base_src_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_gpsd_src_unlock_stop);

  push_src_class->create = GST_DEBUG_FUNCPTR (gst_gpsd_src_create);

  gst_element_class_add_pad_template (element_class,
      gst_static_pad_template_get (&gst_gpsd_src_template));

  g_object_class_install_property (gobject_class, PROP_HOST,
      g_param_spec_string ("host", "gpsd host",
          "The host location for gpsd (URL/IP address)",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PORT,
      g_param_spec_string ("port", "gpsd port", "The gpsd port number",
          NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_TIMESTAMP,
      g_param_spec_enum ("timestamp", "timestamp source",
          "Timestamp source", GST_TYPE_GPSD_TIMESTAMP_SOURCE, TIMESTAMP_GPS,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT | G_PARAM_STATIC_STRINGS));

  gst_element_class_set_static_metadata (element_class,
      "Source for gpsd data", "Source/Sensor/Device", "Stream data from gpsd",
      "Martin Kelly <mkelly@xevo.com>");
}

static void
gst_gpsd_src_init (GstGpsdSrc * self)
{
  gst_base_src_set_live (GST_BASE_SRC (self), TRUE);
  gst_base_src_set_format (GST_BASE_SRC (self), GST_FORMAT_TIME);
  gst_base_src_set_do_timestamp (GST_BASE_SRC (self), FALSE);

  self->host = NULL;
  self->port = NULL;
}

static void
gst_gpsd_src_dispose (GObject * object)
{
  GstGpsdSrc *self = GST_GPSD_SRC (object);

  if (self->host != NULL) {
    g_free (self->host);
  }
  if (self->port != NULL) {
    g_free (self->port);
  }

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_gpsd_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstGpsdSrc *self = GST_GPSD_SRC (object);

  switch (prop_id) {
    case PROP_HOST:
      if (self->host != NULL) {
        g_free (self->host);
      }
      self->host = g_value_dup_string (value);
      break;
    case PROP_PORT:
      if (self->port != NULL) {
        g_free (self->port);
      }
      self->port = g_value_dup_string (value);
      break;
    case PROP_TIMESTAMP:
      self->timestamp_source = g_value_get_enum (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gpsd_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstGpsdSrc *self = GST_GPSD_SRC (object);

  switch (prop_id) {
    case PROP_HOST:
      g_value_set_string (value, self->host);
      break;
    case PROP_PORT:
      g_value_set_string (value, self->port);
      break;
    case PROP_TIMESTAMP:
      g_value_set_enum (value, self->timestamp_source);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
  }
}

static gboolean
gst_gpsd_src_start (GstBaseSrc * src)
{
  GstGpsdSrc *self = GST_GPSD_SRC (src);
  gint status;

  status = gps_open (self->host, self->port, &self->gps);
  if (status == -1) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, GPSD_ERROR_STR (status),
        NULL);
    return FALSE;
  }

  status = gps_stream (&self->gps, WATCH_ENABLE, NULL);
  if (status == -1) {
    GST_ELEMENT_ERROR (self, RESOURCE, READ, GPSD_ERROR_STR (status), NULL);
    status = gps_close (&self->gps);
    if (status == -1) {
      GST_ELEMENT_ERROR (self, RESOURCE, CLOSE, GPSD_ERROR_STR (status), NULL);
    }
    return FALSE;
  }

  self->pfd.fd = self->gps.gps_fd;
  self->pfd.events = POLL_HAS_DATA;

  return TRUE;
}

static gboolean
gst_gpsd_src_stop (GstBaseSrc * src)
{
  GstGpsdSrc *self = GST_GPSD_SRC (src);
  gint status;

  status = gps_close (&self->gps);
  if (status == -1) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, GPSD_ERROR_STR (status),
        NULL);
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_gpsd_src_get_size (GstBaseSrc * src, guint64 * size)
{
  *size = sizeof (struct gps_fix_t);

  return TRUE;
}

static gboolean
gst_gpsd_src_is_seekable (GstBaseSrc * src)
{
  return FALSE;
}

static gboolean
gst_gpsd_src_unlock (GstBaseSrc * src)
{
  return TRUE;
}

static gboolean
gst_gpsd_src_unlock_stop (GstBaseSrc * src)
{
  return TRUE;
}

static GstClockTime
unix_to_gst_time (double timestamp)
{
  double fraction;
  GstClockTime sec;
  GstClockTime nsec;

  /* gpsd timestamps as a double value representing UNIX epoch time. */
  sec = (GstClockTime) timestamp;
  fraction = timestamp - sec;
  nsec = GST_SECOND * fraction;

  return sec * GST_SECOND + nsec * GST_NSECOND;
}

static GstFlowReturn
gst_gpsd_src_create (GstPushSrc * src, GstBuffer ** buffer)
{
  struct gps_fix_t *data;
  GstClockTime base_time;
  GstClockTime delay;
  GstClockTime now;
  GstClock *pipeline_clock;
  GstClockTime pipeline_ts;
  GstClockTime sample_ts;
  GstGpsdSrc *self = GST_GPSD_SRC (src);
  gint status;
  GTimeVal tv;

  pipeline_clock = GST_ELEMENT_CLOCK (self);
  if (pipeline_clock == NULL)
    return GST_FLOW_ERROR;

  /*
   * We use poll rather than read because we want to let gpsd do the actual
   * reading, but we also want an infinite poll (no timeout), which gps_waiting
   * does not allow for.
   */
  while (true) {
    status = poll (&self->pfd, 1, -1);
    if (status == -1 && errno != EINTR) {
      if (errno == EINTR) {
        continue;
      }
      if (errno == ENOMEM) {
        GST_ELEMENT_ERROR (self, RESOURCE, READ,
            ("poll on GPS socket failed with ENOMEM"), NULL);
      } else if (errno == EIO) {
        GST_ELEMENT_ERROR (self, RESOURCE, READ,
            ("poll on GPS socket failed with EIO"), NULL);
      } else {
        GST_ELEMENT_ERROR (self, RESOURCE, READ,
            ("poll on GPS socket failed with unknown error %d", status), NULL);
      }
      return GST_FLOW_ERROR;
    }
    g_assert_true (self->pfd.revents & POLL_HAS_DATA);

    status = gps_read (&self->gps);
    if (status == -1) {
      GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, GPSD_ERROR_STR (status),
          NULL);
      return GST_FLOW_ERROR;
    }

    if (self->gps.status == STATUS_NO_FIX || self->gps.fix.mode == MODE_NO_FIX
        || self->gps.fix.mode == MODE_NOT_SEEN) {
      GST_INFO ("no gps fix");
      continue;
    }

    break;
  }

  /*
   * Calculate the buffer timestamp via:
   * - Calculate the delay between gpsd timestamping the sample and now.
   * - Subtract this delay from the pipeline timestamp.
   * - Subtract the element base time from the pipeline timestamp.
   *
   * In this way, we can use highly accurate GPS time while staying in sync with
   * the pipeline clock.
   */
  g_get_current_time (&tv);
  pipeline_ts = gst_clock_get_time (pipeline_clock);
  now = GST_TIMEVAL_TO_TIME (tv);
  base_time = GST_ELEMENT_CAST (self)->base_time;

  if (self->timestamp_source == TIMESTAMP_GPS) {
    sample_ts = unix_to_gst_time (self->gps.fix.time);
    if (G_LIKELY (now > sample_ts)) {
      delay = now - sample_ts;
      if (delay > pipeline_ts) {
        GST_WARNING ("GPS is sending data older than the pipeline; dropping");
        return GST_FLOW_ERROR;
      }
    } else {
      delay = 0;
      GST_WARNING
          ("gpsd sample time is greater than current time; one of these clocks is wrong");
    }
  } else if (self->timestamp_source == TIMESTAMP_PIPELINE) {
    delay = 0;
  } else {
    g_assert_not_reached ();
  }

  /* Everything is good; let's make a buffer. */
  data = g_malloc (sizeof (*data));
  memcpy (data, &self->gps.fix, sizeof (self->gps.fix));

  *buffer = gst_buffer_new_wrapped (data, sizeof (self->gps.fix));
  GST_BUFFER_DURATION (buffer) = GST_CLOCK_TIME_NONE;
  GST_BUFFER_PTS (buffer) = pipeline_ts - delay - base_time;

  GST_DEBUG_OBJECT (self, "creating buffer %p", (void *) buffer);

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "gpsdsrc", GST_RANK_NONE,
      GST_TYPE_GPSD_SRC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    gpsd,
    "gpsd source",
    plugin_init,
    PACKAGE_VERSION, GST_LICENSE, GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
