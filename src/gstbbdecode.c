/*
 * No license installed
 */

/**
 * SECTION:element-bbdecode
 *
 * Decode boundary boxes from a TFLite detections postprocessor and add results to the stream's GstMeta-space.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! bbdecode labels=PATH ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>

#include "gstbbdecode.h"

GST_DEBUG_CATEGORY_STATIC (gst_bbdecode_debug);
#define GST_CAT_DEFAULT gst_bbdecode_debug

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
  PROP_SILENT
};

#define BBDECODE_DESC "Decode boundary boxes from a TFLite detections postprocessor"

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

#define gst_bbdecode_parent_class parent_class
G_DEFINE_TYPE (GstBBDecode, gst_bbdecode, GST_TYPE_ELEMENT);

static void gst_bbdecode_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_bbdecode_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_bbdecode_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_bbdecode_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);
static GstBuffer *gst_bbdecode_process (GstBBDecode *filter, GstBuffer *inbuf);

/* GObject vmethod implementations */

/* initialize the bbdecode's class */
static void
gst_bbdecode_class_init (GstBBDecodeClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_bbdecode_set_property;
  gobject_class->get_property = gst_bbdecode_get_property;

  g_object_class_install_property (gobject_class, PROP_LABELS,
      g_param_spec_string ("labels", "Labels", "Path to labels list file ?",
          "", G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  gst_element_class_set_details_simple(gstelement_class,
    "BBDecode",
    "BB Decoder",
    "BB Decoder Element",
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
gst_bbdecode_init (GstBBDecode * filter)
{
  filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_bbdecode_sink_event));
  gst_pad_set_chain_function (filter->sinkpad,
                              GST_DEBUG_FUNCPTR(gst_bbdecode_chain));
  GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

  filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (filter->srcpad);
  gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

  filter->silent = FALSE;
}

static void
gst_bbdecode_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstBBDecode *filter = GST_BBDECODE (object);

  switch (prop_id) {
    case PROP_LABELS:
      filter->labels_path = g_value_get_string (value);
      if(!tflite_load_labels(filter->labels_path, filter->labels))
        GST_ERROR_OBJECT(filter, "Failed to load labels from %s", filter->labels_path);
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
gst_bbdecode_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstBBDecode *filter = GST_BBDECODE (object);

  switch (prop_id) {
    case PROP_LABELS:
      g_value_set_string (value, filter->labels_path);
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
gst_bbdecode_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
  GstBBDecode *filter;
  gboolean ret;

  filter = GST_BBDECODE (parent);

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
gst_bbdecode_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
  GstBBDecode *filter;
  GstBuffer *outbuf;
  gboolean sanity_check = TRUE;
  filter = GST_BBDECODE (parent);
  if (!filter->labels_path) {
    GST_ERROR_OBJECT(filter, "Required property 'labels' is missing");
    sanity_check = FALSE;
  }
  if (!sanity_check) return GST_FLOW_ERROR;
  outbuf = gst_bbdecode_process (filter, buf);
  if (!outbuf) return GST_FLOW_ERROR;
  /* just push out the incoming buffer without touching it */
  return gst_pad_push (filter->srcpad, buf);
}

/*
 * this function decodes and scales objects given the tensor
 * returns annotated buffer on success, NULL on error
 */
static GstBuffer *
gst_bbdecode_process (GstBBDecode *filter, GstBuffer *inbuf)
{
  GstBuffer *outbuf;
  gboolean sanity_check = TRUE;
  GstMemory *in_mem[NNS_TENSOR_SIZE_LIMIT];
  GstMapInfo in_info[NNS_TENSOR_SIZE_LIMIT];
  gfloat *boxes, *classes, *scores;
  guint num_detections, i;
  /* Map the following outputs from the TFLite detections postprocessor in this order:
   *    Boxes:             [1, num_detections, 4]
   *    Classes:           [1, num_detections]
   *    Scores:            [1, num_detections]
   *    Number detections: [1]
   * NOTE: All outputs are assumed float32 regardless of model's inference type.
   */
  for (i=0; i<4; i++) {
    in_mem[i] = gst_buffer_peek_memory (inbuf, i);
    g_assert (gst_memory_map (in_mem[i], &in_info[i], GST_MAP_READ));
  }
  boxes = (gfloat *)in_info[0].data;
  classes = (gfloat *)in_info[1].data;
  scores = (gfloat *)in_info[2].data;
  num_detections = (guint)*((gfloat *)in_info[3].data);
  /* Request write-access to tensor buffer to add ROIs, which will be pushed out the tensor srcpad */
  outbuf = gst_buffer_make_writable(inbuf);
  if (!gst_buffer_is_writable(outbuf)) {
    GST_ERROR_OBJECT (filter, "Failed to gain write-access to tensor buffer: %" GST_PTR_FORMAT, outbuf);
    sanity_check = FALSE;
  }
  if(!sanity_check) return NULL;
  /* Attach ROIs to the tensor buffer */
  for(i=0; i<num_detections; i++) {
    guint label_id = (guint)classes[i] + 1;
    const gchar *label = filter->labels[label_id];
    gfloat *box = &boxes[4*i];
    GstStructure *s = gst_structure_new("detection",
      "confidence", G_TYPE_DOUBLE, scores[i],
      "label_id", G_TYPE_UINT, label_id,
      "label_name", G_TYPE_STRING, label,
      NULL /* terminator: do not remove */
      );
    GstVideoRegionOfInterestMeta *meta = gst_buffer_add_video_region_of_interest_meta(
        outbuf,
        label,
        (guint)(UINT_MAX * box[1]), // x
        (guint)(UINT_MAX * box[0]), // y
        (guint)(UINT_MAX * (box[3] - box[1])), // width
        (guint)(UINT_MAX * (box[2] - box[0]))  // height
        );
    gst_video_region_of_interest_meta_add_param(meta, s);
  }
  /* Teardown tensor mapping */
  for (i=0; i<4; i++) {
    gst_memory_unmap (in_mem[i], &in_info[i]);
  }
  return outbuf;
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
bbdecode_init (GstPlugin * bbdecode)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template bbdecode' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_bbdecode_debug, "bbdecode",
      0, BBDECODE_DESC);

  return gst_element_register (bbdecode, "bbdecode", GST_RANK_NONE,
      GST_TYPE_BBDECODE);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "bbdecode"
#endif

/* gstreamer looks for this structure to register bbdecodes
 *
 * exchange the string 'Template bbdecode' with your bbdecode description
 */
GST_PLUGIN_DEFINE (
    GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    bbdecode,
    BBDECODE_DESC,
    bbdecode_init,
    PACKAGE_VERSION,
    GST_LICENSE,
    GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN
)
