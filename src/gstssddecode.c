/*
 * No license installed
 */

/**
 * SECTION:element-ssddecode
 *
 * Decode boundary boxes from tensors and add results to the stream's GstMeta-space.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 -v -m fakesrc ! ssddecode boxpriors=PATH labels=PATH ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include <nnstreamer/tensor_typedef.h>

#include "gstssddecode.h"

GST_DEBUG_CATEGORY_STATIC (gst_ssddecode_debug);
#define GST_CAT_DEFAULT gst_ssddecode_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

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
#define SSDDECODE_DESC "Decode boundary boxes from tensors"

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TENSOR_CAPS_STRING)
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (TENSOR_CAPS_STRING)
    );

#define gst_ssddecode_parent_class parent_class
G_DEFINE_TYPE (GstSSDDecode, gst_ssddecode, GST_TYPE_ELEMENT);

static void gst_ssddecode_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_ssddecode_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_ssddecode_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_ssddecode_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);
static GstBuffer *gst_ssddecode_process (GstSSDDecode *filter, GstBuffer *inbuf);

/* GObject vmethod implementations */

/* initialize the ssddecode's class */
static void
gst_ssddecode_class_init (GstSSDDecodeClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_ssddecode_set_property;
  gobject_class->get_property = gst_ssddecode_get_property;

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
    "SSDDecode",
    "SSD Decoder",
    "SSD Decoder Element",
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
gst_ssddecode_init (GstSSDDecode * filter)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_ssddecode_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_ssddecode_chain));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->silent = FALSE;
}

static void
gst_ssddecode_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstSSDDecode *filter = GST_SSDDECODE (object);

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
gst_ssddecode_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstSSDDecode *filter = GST_SSDDECODE (object);

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
gst_ssddecode_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstSSDDecode *filter;
  gboolean ret;

  filter = GST_SSDDECODE (parent);

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
 * this function is called when a buffer is pushed into the sink-pad
 */
static GstFlowReturn
gst_ssddecode_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstSSDDecode *filter;
  GstBuffer *outbuf;
  gboolean sanity_check = TRUE;
  filter = GST_SSDDECODE (parent);
  if (!filter->labels_path) {
    GST_ERROR_OBJECT(filter, "Required property 'labels' is missing");
    sanity_check = FALSE;
  }
  if (!filter->box_priors_path) {
    GST_ERROR_OBJECT(filter, "Required property 'boxpriors' is missing");
    sanity_check = FALSE;
  }
  if (!sanity_check) return GST_FLOW_ERROR;
  outbuf = gst_ssddecode_process (filter, buf);
  if (!outbuf) return GST_FLOW_ERROR;
  /* Push tensor buffer to srcpad */
  return gst_pad_push (filter->srcpad, outbuf);
}

/*
 * this function decodes and scales objects given the tensor
 * returns annotated buffer on success, NULL on error
 */
static GstBuffer *
gst_ssddecode_process (GstSSDDecode *filter, GstBuffer *inbuf)
{
  GstBuffer *outbuf;
  gboolean sanity_check = TRUE;
  GstMemory *in_mem[NNS_TENSOR_SIZE_LIMIT];
  GstMapInfo in_info[NNS_TENSOR_SIZE_LIMIT];
  gfloat *predictions;
  gfloat *boxes;
  DetectedObject detections[DETECTION_MAX * LABEL_SIZE];
  guint num_detections = 0, i;
  /* Map boxes and predictions tensors from model */
  for (i=0; i<2; i++) {
    in_mem[i] = gst_buffer_peek_memory (inbuf, i);
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
  outbuf = gst_buffer_make_writable(inbuf);
  if (!gst_buffer_is_writable(outbuf)) {
    GST_ERROR_OBJECT (filter, "Failed to gain write-access to tensor buffer: %" GST_PTR_FORMAT, outbuf);
    sanity_check = FALSE;
  }
  if(!sanity_check) return NULL;
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
  return outbuf;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
ssddecode_init (GstPlugin * ssddecode)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template ssddecode' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_ssddecode_debug, "ssddecode",
      0, SSDDECODE_DESC);

  return gst_element_register (ssddecode, "ssddecode", GST_RANK_NONE,
      GST_TYPE_SSDDECODE);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "ssddecode"
#endif

/* gstreamer looks for this structure to register ssddecodes
 *
 * exchange the string 'Template ssddecode' with your ssddecode description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    ssddecode,
    SSDDECODE_DESC,
    ssddecode_init,
    PACKAGE_VERSION,
    GST_LICENSE,
    GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN
)
