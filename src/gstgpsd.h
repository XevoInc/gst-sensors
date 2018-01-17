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

#ifndef _GST_GPSDSRC_H__
#define _GST_GPSDSRC_H__

#include <gps.h>
#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>
#include <poll.h>

G_BEGIN_DECLS
#define GST_TYPE_GPSD_SRC   (gst_gpsd_src_get_type())
#define GST_GPSD_SRC(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_GPSD_SRC,GstGpsdSrc))
#define GST_GPSD_SRC_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_GPSD_SRC,GstGpsdSrcClass))
#define GST_IS_GPSD_SRC(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_GPSD_SRC))
#define GST_IS_GPSD_SRC_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_GPSD_SRC))
typedef struct _GstGpsdSrc GstGpsdSrc;
typedef struct _GstGpsdSrcClass GstGpsdSrcClass;

struct _GstGpsdSrc
{
  /* < private > */
  GstPushSrc parent;

  /* properties */
  gchar *host;
  gchar *port;
  gint32 timestamp_source;

  /* gpsd context */
  struct gps_data_t gps;
  struct pollfd pfd;
};

struct _GstGpsdSrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_gpsd_src_get_type (void);

G_END_DECLS
#endif
