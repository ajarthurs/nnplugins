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
#include <gst/video/gstvideometa.h>
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
#define CAPS_STRING GST_TENSOR_CAP_DEFAULT "; " GST_TENSORS_CAP_DEFAULT
#define TENSORDECODE_DESC "Decode boundary boxes from tensors"

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STRING)
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (CAPS_STRING)
    );

#define gst_tensor_decode_parent_class parent_class
G_DEFINE_TYPE (GstTensorDecode, gst_tensor_decode, GST_TYPE_ELEMENT);

static void gst_tensor_decode_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tensor_decode_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_tensor_decode_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_tensor_decode_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);

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

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_tensor_decode_init (GstTensorDecode * filter)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_tensor_decode_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_tensor_decode_chain));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);
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

/* this function handles sink events */
static gboolean
gst_tensor_decode_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstTensorDecode *filter;
  gboolean ret;

  filter = GST_TENSORDECODE (parent);

  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps * caps;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_tensor_decode_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstTensorDecode *filter;
  GstMemory *in_mem[NNS_TENSOR_SIZE_LIMIT];
  GstMapInfo in_info[NNS_TENSOR_SIZE_LIMIT];
  gfloat *predictions;
  gfloat *boxes;
  DetectedObject detections[DETECTION_MAX * LABEL_SIZE];
  guint num_detections = 0, i;
  gboolean sanity_check = TRUE;

  filter = GST_TENSORDECODE (parent);

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
  for (i=0; i<2; i++) {
    in_mem[i] = gst_buffer_peek_memory (buf, i);
    g_assert (gst_memory_map (in_mem[i], &in_info[i], GST_MAP_READ));
  }
  boxes = (gfloat *)in_info[0].data;
  predictions = (gfloat *)in_info[1].data;

  if (filter->silent == FALSE)
    GST_LOG_OBJECT (filter, "Received buffer");

  sanity_check = get_detected_objects (filter->box_priors, filter->labels, predictions, boxes, detections, &num_detections);
  for (i=0; i<2; i++) {
    gst_memory_unmap (in_mem[i], &in_info[i]);
  }
  if(!sanity_check) return GST_FLOW_ERROR;

  /* attach ROI */
  for(i=0; i<num_detections; i++) {
    DetectedObject *d = &detections[i];
    GstStructure *s = gst_structure_new("detection",
      "confidence", G_TYPE_DOUBLE, d->prob,
      "label_id", G_TYPE_INT, d->class_id,
      "label_name", G_TYPE_STRING, d->class_label,
      NULL /* terminator: do not remove */
      );
    GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
        buf,
        d->class_label,
        d->x,
        d->y,
        d->width,
        d->height
        );
    gst_video_region_of_interest_meta_add_param(meta, s);
  }

  /* just push out the incoming buffer without touching it */
  return gst_pad_push (filter->srcpad, buf);
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
