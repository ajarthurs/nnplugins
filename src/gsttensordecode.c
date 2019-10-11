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
static GstStaticPadTemplate tensor_sink_factory = GST_STATIC_PAD_TEMPLATE ("tensor_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TENSOR_CAPS_STRING)
    );

static GstStaticPadTemplate tensor_src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TENSOR_CAPS_STRING)
    );

static GstStaticPadTemplate video_sink_factory = GST_STATIC_PAD_TEMPLATE ("video_sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (VIDEO_CAPS_STRING)
    );

//static GstStaticPadTemplate video_src_factory = GST_STATIC_PAD_TEMPLATE ("video_src",
//    GST_PAD_SRC,
//    GST_PAD_ALWAYS,
//    GST_STATIC_CAPS (VIDEO_CAPS_STRING)
//    );

#define gst_tensor_decode_parent_class parent_class
G_DEFINE_TYPE (GstTensorDecode, gst_tensor_decode, GST_TYPE_ELEMENT);

static void gst_tensor_decode_pad_destroy_notify (GstCollectData * data);
static void gst_tensor_decode_clear (GstTensorDecode *filter);
static GstStateChangeReturn gst_tensor_decode_change_state (GstElement * element, GstStateChange transition);
static void gst_tensor_decode_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tensor_decode_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);
static GstFlowReturn gst_tensor_decode_process (GstTensorDecode *filter, GstBuffer *tbuf, GstBuffer *vbuf);
static GstFlowReturn gst_tensor_decode_collected (GstCollectPads * pads, GstTensorDecode * filter);
static void gst_tensor_decode_send_start_events (GstTensorDecode * filter, GstCollectPads * pads);

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
  gst_element_class_add_pad_template (gstelement_class, gst_static_pad_template_get (&video_sink_factory));
  //gst_element_class_add_pad_template (gstelement_class, gst_static_pad_template_get (&video_src_factory));

  gstelement_class->change_state = gst_tensor_decode_change_state;
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

  /* video pads */
  filter->video_sinkpad = gst_pad_new_from_static_template (&video_sink_factory, "video_sink");
  gst_element_add_pad (GST_ELEMENT (filter), filter->video_sinkpad);

  //filter->video_srcpad = gst_pad_new_from_static_template (&video_src_factory, "video_src");
  //GST_PAD_SET_PROXY_CAPS (filter->video_srcpad);
  //gst_element_add_pad (GST_ELEMENT (filter), filter->video_srcpad);

  /* sink pads collection */
  filter->collect = gst_collect_pads_new ();
  gst_collect_pads_set_function (filter->collect,
    (GstCollectPadsFunction) GST_DEBUG_FUNCPTR (gst_tensor_decode_collected),
    filter);
  gst_collect_pads_add_pad (filter->collect, filter->tensor_sinkpad,
    sizeof (GstCollectData), gst_tensor_decode_pad_destroy_notify, TRUE);
  gst_collect_pads_add_pad (filter->collect, filter->video_sinkpad,
    sizeof (GstCollectData), gst_tensor_decode_pad_destroy_notify, TRUE);

  gst_tensor_decode_clear (filter);
}

static void
gst_tensor_decode_pad_destroy_notify (GstCollectData * data)
{
}

static void
gst_tensor_decode_clear (GstTensorDecode *filter)
{
  filter->need_start_events = TRUE;
}

static GstStateChangeReturn
gst_tensor_decode_change_state (GstElement * element, GstStateChange transition)
{
  GstTensorDecode *filter;
  GstStateChangeReturn ret;

  filter = GST_TENSORDECODE (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_tensor_decode_clear (filter);
      //gst_ogg_mux_init_collectpads (ogg_mux->collect);
      gst_collect_pads_start (filter->collect);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_collect_pads_stop (filter->collect);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  //switch (transition) {
  //  case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
  //    break;
  //  case GST_STATE_CHANGE_PAUSED_TO_READY:
  //    gst_ogg_mux_clear_collectpads (ogg_mux->collect);
  //    break;
  //  case GST_STATE_CHANGE_READY_TO_NULL:
  //    break;
  //  default:
  //    break;
  //}

  return ret;
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

/* collect function
 * this function manages data across all sink pads (tensor and video).
 */
static GstFlowReturn
gst_tensor_decode_collected (GstCollectPads * pads, GstTensorDecode * filter)
{
  GSList *walk;
  GstBuffer *tbuf = NULL, *vbuf = NULL;
  GstFlowReturn ret;
  GST_DEBUG_OBJECT (filter, "collected");
  if (filter->need_start_events) {
    /* FIXME (maybe): Investigate stream initialization and headers */
    GstSegment segment;
    gst_tensor_decode_send_start_events (filter, pads);
    gst_segment_init (&segment, GST_FORMAT_TIME);
    gst_pad_push_event (filter->tensor_srcpad, gst_event_new_segment (&segment));
    filter->need_start_events = FALSE;
  }
  /* step through the sinkpads to collect their buffers (should just be video and tensor) */
  walk = filter->collect->data;
  while (walk) {
    GstCollectData *data = walk->data;
    GstBuffer *buf;
    buf = gst_collect_pads_pop (pads, walk->data);
    if (buf) {
      GST_DEBUG_OBJECT (data->pad, "looking at this pad");
      if (data->pad == filter->video_sinkpad) {
        GST_DEBUG_OBJECT (filter, "it's the video sinkpad");
        vbuf = buf;
      } else if (data->pad == filter->tensor_sinkpad) {
        GST_DEBUG_OBJECT (filter, "it's the tensor sinkpad");
        tbuf = buf;
      } else {
        GST_ERROR_OBJECT (filter, "unknown what the sinkpad is");
        return GST_FLOW_ERROR;
      }
      GST_DEBUG_OBJECT (filter, "got buffer %" GST_PTR_FORMAT, buf);
    } else { // EOS
      GST_DEBUG_OBJECT (filter, "no data available, must be EOS");
      gst_pad_push_event (filter->tensor_srcpad, gst_event_new_eos ());
      return GST_FLOW_EOS;
    }
    walk = g_slist_next (walk);
  }
  if (!tbuf || !vbuf) {
    GST_ERROR_OBJECT (filter, "missing tensor and video buffers: tensor buf = %" GST_PTR_FORMAT " video buf = %" GST_PTR_FORMAT,
      tbuf, vbuf);
    return GST_FLOW_ERROR;
  }
  /* process the tensor and video buffers */
  ret = gst_tensor_decode_process(filter, tbuf, vbuf);
  /* free the tensor and video buffers */
  //gst_buffer_unref (tbuf); // FIXME (maybe): Cannot free tensor-buffer because it's been pushed into the tensor sourcepad.
  gst_buffer_unref (vbuf);
  return ret;
}

static void
gst_tensor_decode_send_start_events (GstTensorDecode * filter, GstCollectPads * pads)
{
  gchar s_id[32];

  /* stream-start (FIXME: create id based on input ids) and
   * also do something with the group id */
  g_snprintf (s_id, sizeof (s_id), "tensordecode-%08x", g_random_int ());
  gst_pad_push_event (filter->tensor_srcpad, gst_event_new_stream_start (s_id));

  /* we'll send caps later, need to collect all headers first */
}

/*
 * this function decodes and scales objects given the tensor and video buffers
 */
static GstFlowReturn
gst_tensor_decode_process (GstTensorDecode *filter, GstBuffer *tbuf, GstBuffer *vbuf)
{
  GstVideoMeta *vmeta;
  gboolean sanity_check = TRUE;
  GstMemory *in_mem[NNS_TENSOR_SIZE_LIMIT];
  GstMapInfo in_info[NNS_TENSOR_SIZE_LIMIT];
  gfloat *predictions;
  gfloat *boxes;
  DetectedObject detections[DETECTION_MAX * LABEL_SIZE];
  guint num_detections = 0, i;
  vmeta = gst_buffer_get_video_meta(vbuf);
  if(!vmeta) {
    GST_ERROR_OBJECT(filter, "Failed to fetch video meta");
    sanity_check = FALSE;
  }
  GST_DEBUG_OBJECT(filter, "Video width x height = %d x %d", vmeta->width, vmeta->height);
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
    in_mem[i] = gst_buffer_peek_memory (tbuf, i);
    g_assert (gst_memory_map (in_mem[i], &in_info[i], GST_MAP_READ));
  }
  boxes = (gfloat *)in_info[0].data;
  predictions = (gfloat *)in_info[1].data;
  sanity_check = get_detected_objects (filter->box_priors, filter->labels, predictions, boxes, vmeta, detections, &num_detections);
  for (i=0; i<2; i++) {
    gst_memory_unmap (in_mem[i], &in_info[i]);
  }
  if(!sanity_check) return GST_FLOW_ERROR;
  /* attach ROI */
  for(i=0; i<num_detections; i++) {
    DetectedObject *d = &detections[i];
    GstStructure *s = gst_structure_new("detection",
      "confidence", G_TYPE_DOUBLE, d->score,
      "label_id", G_TYPE_UINT, d->class_id,
      "label_name", G_TYPE_STRING, d->class_label,
      NULL /* terminator: do not remove */
      );
    GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
        tbuf,
        d->class_label,
        d->x,
        d->y,
        d->width,
        d->height
        );
    gst_video_region_of_interest_meta_add_param(meta, s);
  }
  /* push tensor buffer to tensor srcpad */
  return gst_pad_push (filter->tensor_srcpad, tbuf);
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
