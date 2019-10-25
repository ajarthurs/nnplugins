/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2019 Aaron Arthurs <aajarthurs@gmail.com>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_TENSORDECODE_H__
#define __GST_TENSORDECODE_H__

#include <gst/gst.h>
#include <gst/base/gstcollectpads.h>
#include <gst/video/gstvideometa.h>

G_BEGIN_DECLS

#define GST_TYPE_TENSORDECODE \
  (gst_tensor_decode_get_type())
#define GST_TENSORDECODE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TENSORDECODE,GstTensorDecode))
#define GST_TENSORDECODE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TENSORDECODE,GstTensorDecodeClass))
#define GST_IS_TENSORDECODE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TENSORDECODE))
#define GST_IS_TENSORDECODE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TENSORDECODE))

typedef struct _GstTensorDecode      GstTensorDecode;
typedef struct _GstTensorDecodeClass GstTensorDecodeClass;

/* FIXME: Consider making these properties with default values */
#define Y_SCALE         10.0f
#define X_SCALE         10.0f
#define H_SCALE         5.0f
#define W_SCALE         5.0f
#define MODEL_WIDTH     300
#define MODEL_HEIGHT    300
#define DETECTION_MAX   1917
#define BOX_SIZE        4
#define LABEL_SIZE      91
#define THRESHOLD_SCORE 0.5f
#define THRESHOLD_IOU   0.0f
#define EXPIT(x) (1.f / (1.f + expf (-x)))
struct _GstTensorDecode
{
  GstElement element;

  GstPad *tensor_sinkpad, *tensor_srcpad;
  GstPad *video_sinkpad, *video_srcpad;
  GstCollectPads *collect;

  const gchar *labels_path;
  const gchar *box_priors_path;
  gfloat box_priors[BOX_SIZE][DETECTION_MAX];
  const gchar *labels[LABEL_SIZE];
  gboolean silent;

  gboolean need_start_events;
};

struct _GstTensorDecodeClass
{
  GstElementClass parent_class;
};

typedef struct _DetectedObject
{
  gint  x;
  gint  y;
  guint width;
  guint height;
  guint class_id;
  const gchar *class_label;
  gfloat score;
} DetectedObject;

GType gst_tensor_decode_get_type (void);
gboolean read_lines (const gchar *file_name, GList **lines);
gboolean tflite_load_labels (const gchar *labels_path, const gchar *labels[LABEL_SIZE]);
gboolean tflite_load_box_priors (const gchar *box_priors_path, gfloat box_priors[BOX_SIZE][DETECTION_MAX]);
gboolean get_detected_objects (gfloat box_priors[BOX_SIZE][DETECTION_MAX], const gchar *labels[LABEL_SIZE], const gfloat *predictions, const gfloat *boxes, DetectedObject *detections, guint *num_detections);

G_END_DECLS

#endif /* __GST_TENSORDECODE_H__ */
