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

/**
 * SECTION:element-tensordecode
 *
 * Decode boundary boxes from tensors and add results to the stream's GstMeta-space.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v -m fakesrc ! tensordecode ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <nnstreamer/tensor_typedef.h>

#include "gsttensordecode.h"

GST_DEBUG_CATEGORY_STATIC (gst_tensor_decode_debug);
#define GST_CAT_DEFAULT gst_tensor_decode_debug

enum
{
  PROP_0, /* Anchor prop. Do not remove. */
  PROP_LABELS,
  PROP_BOX_PRIORS,
  PROP_SILENT
};

/**
 * @brief Support multi-tensor along with single-tensor as the input
 */
#define TENSOR_CAPS_STRING GST_TENSOR_CAP_DEFAULT "; " GST_TENSORS_CAP_DEFAULT
#define VIDEO_CAPS_STRING GST_VIDEO_CAPS_MAKE(GST_VIDEO_FORMATS_ALL)
#define TENSORDECODE_DESC "Decode boundary boxes from tensors"

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate tensor_sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TENSOR_CAPS_STRING)
    );

static GstStaticPadTemplate tensor_src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TENSOR_CAPS_STRING)
    );

#define gst_tensor_decode_parent_class parent_class
G_DEFINE_TYPE (GstTensorDecode, gst_tensor_decode, GST_TYPE_ELEMENT);

static void gst_tensor_decode_set_property (GObject * object, guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_tensor_decode_get_property (GObject * object, guint prop_id, GValue * value, GParamSpec * pspec);
static GstFlowReturn gst_tensor_decode_chain (GstPad *pad, GstObject *parent, GstBuffer *buf);
static GstFlowReturn gst_tensor_decode_process (GstTensorDecode *filter, GstBuffer *tbuf);

/* GObject vmethod implementations */

/* initialize the tensordecode's class */
static void
gst_tensor_decode_class_init (GstTensorDecodeClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_tensor_decode_set_property;
  gobject_class->get_property = gst_tensor_decode_get_property;

  g_object_class_install_property (gobject_class, PROP_LABELS,
      g_param_spec_string ("labels", "Labels", "Path to labels list file ?",
          "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BOX_PRIORS,
      g_param_spec_string ("boxpriors", "Box-Priors", "Path to box-priors file ?",
          "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  gst_element_class_set_details_simple(gstelement_class,
    "TensorDecode",
    "Tensor Decoder",
    "Tensor Decoder Element",
    "Aaron Arthurs <aajarthurs@gmail.com>");

  gst_element_class_add_pad_template (gstelement_class, gst_static_pad_template_get (&tensor_sink_factory));
  gst_element_class_add_pad_template (gstelement_class, gst_static_pad_template_get (&tensor_src_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_tensor_decode_init (GstTensorDecode * filter)
{
  /* tensor pads */
  filter->tensor_sinkpad = gst_pad_new_from_static_template (&tensor_sink_factory, "tensor_sink");
  gst_element_add_pad (GST_ELEMENT (filter), filter->tensor_sinkpad);

  filter->tensor_srcpad = gst_pad_new_from_static_template (&tensor_src_factory, "src");
  gst_element_add_pad (GST_ELEMENT (filter), filter->tensor_srcpad);

  gst_pad_set_chain_function (filter->tensor_sinkpad, GST_DEBUG_FUNCPTR(gst_tensor_decode_chain));
}

static void
gst_tensor_decode_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTensorDecode *filter = GST_TENSORDECODE (object);

  switch (prop_id) {
    case PROP_LABELS:
      filter->labels_path = g_value_get_string (value);
      if(!tflite_load_labels(filter->labels_path, filter->labels))
        GST_ERROR_OBJECT(filter, "Failed to load labels from %s", filter->labels_path);
      else if (!filter->silent) {
        int irow = 0;
        const gchar *row;
        GST_LOG_OBJECT(filter, "Loaded labels from %s", filter->labels_path);
        do {
          row = filter->labels[irow];
          GST_LOG_OBJECT(filter, "             label %d:'%s'", irow, row);
          irow++;
        } while(row && irow < LABEL_SIZE);
      }
      break;
    case PROP_BOX_PRIORS:
      filter->box_priors_path = g_value_get_string (value);
      if(!tflite_load_box_priors(filter->box_priors_path, filter->box_priors))
        GST_ERROR_OBJECT(filter, "Failed to load box-priors from %s", filter->box_priors_path);
      else if (!filter->silent)
        GST_LOG_OBJECT(filter, "Loaded box-priors from %s", filter->box_priors_path);
      break;
    case PROP_SILENT:
      filter->silent = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_tensor_decode_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTensorDecode *filter = GST_TENSORDECODE (object);

  switch (prop_id) {
    case PROP_LABELS:
      g_value_set_string (value, filter->labels_path);
      break;
    case PROP_BOX_PRIORS:
      g_value_set_string (value, filter->box_priors_path);
      break;
    case PROP_SILENT:
      g_value_set_boolean (value, filter->silent);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */


/*
 * this function is called when a buffer is pushed into the sink-pad
 */
static GstFlowReturn
gst_tensor_decode_chain (GstPad *pad, GstObject *parent, GstBuffer *buf)
{
  GstTensorDecode *filter;
  filter = GST_TENSORDECODE (parent);
  return gst_tensor_decode_process (filter, buf);
}


/*
 * this function decodes and scales objects given the tensor
 */
static GstFlowReturn
gst_tensor_decode_process (GstTensorDecode *filter, GstBuffer *tbuf)
{
  GstBuffer *outbuf;
  gboolean sanity_check = TRUE;
  GstMemory *in_mem[NNS_TENSOR_SIZE_LIMIT];
  GstMapInfo in_info[NNS_TENSOR_SIZE_LIMIT];
  gfloat *predictions;
  gfloat *boxes;
  DetectedObject detections[DETECTION_MAX * LABEL_SIZE];
  guint num_detections = 0, i;
  if (!filter->labels_path) {
    GST_ERROR_OBJECT(filter, "Required property 'labels' is missing");
    sanity_check = FALSE;
  }
  if (!filter->box_priors_path) {
    GST_ERROR_OBJECT(filter, "Required property 'boxpriors' is missing");
    sanity_check = FALSE;
  }
  if (!sanity_check) return GST_FLOW_ERROR;
  /*FIXME: Hard-coding some assumptions about the tensors from the model (tensor_filter) */
  /* Map boxes and predictions tensors from model */
  for (i=0; i<2; i++) {
    in_mem[i] = gst_buffer_peek_memory (tbuf, i);
    g_assert (gst_memory_map (in_mem[i], &in_info[i], GST_MAP_READ));
  }
  boxes = (gfloat *)in_info[0].data;
  predictions = (gfloat *)in_info[1].data;
  /* Process boxes and predictions into an array of DetectedObjects */
  sanity_check = get_detected_objects (filter->box_priors, filter->labels, predictions, boxes, detections, &num_detections);
  /* Teardown tensor mapping */
  for (i=0; i<2; i++) {
    gst_memory_unmap (in_mem[i], &in_info[i]);
  }
  /* Request write-access to tensor buffer to add ROIs, which will be pushed out the tensor srcpad */
  outbuf = gst_buffer_make_writable(tbuf);
  if (!gst_buffer_is_writable(outbuf)) {
    GST_ERROR_OBJECT (filter, "Failed to gain write-access to tensor buffer: %" GST_PTR_FORMAT, outbuf);
    sanity_check = FALSE;
  }
  if(!sanity_check) return GST_FLOW_ERROR;
  /* Attach ROIs to the tensor buffer */
  for(i=0; i<num_detections; i++) {
    DetectedObject *d = &detections[i];
    GstStructure *s = gst_structure_new("detection",
      "confidence", G_TYPE_DOUBLE, d->score,
      "label_id", G_TYPE_UINT, d->class_id,
      "label_name", G_TYPE_STRING, d->class_label,
      NULL /* terminator: do not remove */
      );
    GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
        outbuf,
        d->class_label,
        d->x,
        d->y,
        d->width,
        d->height
        );
    gst_video_region_of_interest_meta_add_param(meta, s);
  }
  /* Push tensor buffer to tensor srcpad */
  return gst_pad_push (filter->tensor_srcpad, outbuf);
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
tensordecode_init (GstPlugin * tensordecode)
{
  /* debug category for fltering log messages */
  GST_DEBUG_CATEGORY_INIT (gst_tensor_decode_debug, "tensordecode",
      0, TENSORDECODE_DESC);

  return gst_element_register (tensordecode, "tensordecode", GST_RANK_NONE,
      GST_TYPE_TENSORDECODE);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "tensor-decode-element"
#endif

/* gstreamer looks for this structure to register tensordecode elements
 *
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    tensordecode,
    TENSORDECODE_DESC,
    tensordecode_init,
    PACKAGE_VERSION,
    GST_LICENSE,
    GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN
)
