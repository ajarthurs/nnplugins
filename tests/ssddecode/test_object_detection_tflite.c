/**
 * @brief	Tensor stream example with TF-Lite model for object detection
 *
 * Get model by
 * $ cd $NNST_ROOT/bin
 * $ bash get-model-objet-detection-tflite.sh
 *
 * Run example :
 * Before running this example, GST_PLUGIN_PATH should be updated for nnstreamer plug-in.
 * $ export GST_PLUGIN_PATH=$GST_PLUGIN_PATH:<nnstreamer plugin path>
 * $ ./test_object_detection_tflite
 *
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include "../../src/libtensordecode.h"

#include <math.h>
#include <cairo.h>
#include <cairo-gobject.h>

/**
 * @brief Macro for debug mode.
 */
#ifndef DBG
#define DBG TRUE
#endif

/**
 * @brief Macro for debug message.
 */
#define _print_log(...) if (DBG) g_message (__VA_ARGS__)

/**
 * @brief Macro to check error case.
 */
#define _check_cond_err(cond) \
  do { \
    if (!(cond)) { \
      _print_log ("app failed! [line : %d]", __LINE__); \
      goto error; \
    } \
  } while (0)

/**
 * Playback video w.r.t. the NN model's latency.
 */
#define FRAME_STEP TRUE

#define VIDEO_WIDTH     640
#define VIDEO_HEIGHT    640
//#define VIDEO_WIDTH     1024
//#define VIDEO_HEIGHT    768

#define tflite_model_path "./tests/testdata"
#define tflite_model "ssd_mobilenet_v1_coco.tflite"
#define MODEL_WIDTH     300
#define MODEL_HEIGHT    300
//#define DETECTION_MAX   1917
#define tflite_box_priors "box_priors-ssd_mobilenet.txt"

////const gchar tflite_model[] = "ssd_resnet50_v1_fpn_coco.tflite";
//const gchar tflite_model[] = "ssd_mobilenet_v1_fpn_coco.tflite";
//#define MODEL_WIDTH     640
//#define MODEL_HEIGHT    640
//#define DETECTION_MAX   51150
//const gchar tflite_box_priors[] = "box_priors-ssd_fpn.txt";

#define tflite_label "coco_labels_list.txt"

#define str_video_file "sample_1080p.mp4"

/**
 * @brief Max objects in display.
 */
#define MAX_OBJECT_DETECTION 1024


typedef struct
{
  gboolean valid;
  GstVideoInfo vinfo;
} CairoOverlayState;

/**
 * @brief Data structure for tflite model info.
 */
typedef struct
{
  gchar *model_path; /**< tflite model file path */
  gchar *label_path; /**< label file path */
  gchar *box_prior_path; /**< box prior file path */
  gfloat box_priors[BOX_SIZE][DETECTION_MAX]; /**< box prior */
  const gchar *labels[LABEL_SIZE]; /**< list of loaded labels */
} TFLiteModelInfo;

/**
 * @brief Data structure for app.
 */
typedef struct
{
  GMainLoop *loop; /**< main event loop */
  GstElement *pipeline; /**< gst pipeline for data stream */
  GstBus *bus; /**< gst bus for data pipeline */
  gboolean running; /**< true when app is running */
  GMutex mutex; /**< mutex for processing */
  TFLiteModelInfo tflite_info; /**< tflite model info */
  CairoOverlayState overlay_state;
  guint num_detections;
  DetectedObject detected_objects[MAX_OBJECT_DETECTION];
  GstElement *appsink;
  GstElement *tensor_res;
} AppData;

/**
 * @brief Data for pipeline and result.
 */
static AppData g_app;


/**
 * @brief Check tflite model and load labels.
 */
static gboolean
tflite_init_info (TFLiteModelInfo * tflite_info, const gchar * path)
{
  g_return_val_if_fail (tflite_info != NULL, FALSE);

  tflite_info->model_path = g_strdup_printf ("%s/%s", path, tflite_model);
  tflite_info->label_path = g_strdup_printf ("%s/%s", path, tflite_label);
  tflite_info->box_prior_path =
      g_strdup_printf ("%s/%s", path, tflite_box_priors);

  if (!g_file_test (tflite_info->model_path, G_FILE_TEST_IS_REGULAR)) {
    g_critical ("the file of model_path is not valid: %s\n", tflite_info->model_path);
    return FALSE;
  }
  if (!g_file_test (tflite_info->label_path, G_FILE_TEST_IS_REGULAR)) {
    g_critical ("the file of label_path is not valid%s\n", tflite_info->label_path);
    return FALSE;
  }
  if (!g_file_test (tflite_info->box_prior_path, G_FILE_TEST_IS_REGULAR)) {
    g_critical ("the file of box_prior_path is not valid%s\n", tflite_info->box_prior_path);
    return FALSE;
  }

  g_return_val_if_fail (tflite_load_box_priors (tflite_info->box_prior_path, tflite_info->box_priors), FALSE);
  g_return_val_if_fail (tflite_load_labels (tflite_info->label_path, tflite_info->labels), FALSE);

  return TRUE;
}

/**
 * @brief Free data in tflite info structure.
 */
static void
tflite_free_info (TFLiteModelInfo * tflite_info)
{
  g_return_if_fail (tflite_info != NULL);

  if (tflite_info->model_path) {
    g_free (tflite_info->model_path);
    tflite_info->model_path = NULL;
  }

  if (tflite_info->label_path) {
    g_free (tflite_info->label_path);
    tflite_info->label_path = NULL;
  }

  if (tflite_info->box_prior_path) {
    g_free (tflite_info->box_prior_path);
    tflite_info->box_prior_path = NULL;
  }
}

/**
 * @brief Free resources in app data.
 */
static void
free_app_data (void)
{
  if (g_app.loop) {
    g_main_loop_unref (g_app.loop);
    g_app.loop = NULL;
  }

  if (g_app.bus) {
    gst_bus_remove_signal_watch (g_app.bus);
    gst_object_unref (g_app.bus);
    g_app.bus = NULL;
  }

  if (g_app.appsink) {
    gst_object_unref (g_app.appsink);
    g_app.appsink = NULL;
  }

  if (g_app.tensor_res) {
    gst_object_unref (g_app.tensor_res);
    g_app.tensor_res = NULL;
  }

  if (g_app.pipeline) {
    gst_object_unref (g_app.pipeline);
    g_app.pipeline = NULL;
  }

  tflite_free_info (&g_app.tflite_info);
  g_mutex_clear (&g_app.mutex);
}

/**
 * @brief Function to print error message.
 */
static void
parse_err_message (GstMessage * message)
{
  gchar *debug;
  GError *error;

  g_return_if_fail (message != NULL);

  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_ERROR:
      gst_message_parse_error (message, &error, &debug);
      break;

    case GST_MESSAGE_WARNING:
      gst_message_parse_warning (message, &error, &debug);
      break;

    default:
      return;
  }

  gst_object_default_error (GST_MESSAGE_SRC (message), error, debug);
  g_error_free (error);
  g_free (debug);
}

/**
 * @brief Function to print qos message.
 */
static void
parse_qos_message (GstMessage * message)
{
  GstFormat format;
  guint64 processed;
  guint64 dropped;

  gst_message_parse_qos_stats (message, &format, &processed, &dropped);
  _print_log ("%s: format[%d] processed[%" G_GUINT64_FORMAT "] dropped[%"
      G_GUINT64_FORMAT "]", GST_MESSAGE_SRC_NAME(message), format, processed, dropped);
}

/**
 * @brief Callback for tensor sink signal.
 */
static void
new_data_cb2 (GstElement * element, GstBuffer * buffer, gpointer user_data)
{
  guint i = 0;
  gpointer state = NULL;
  _print_log("called new_data_cb2");
  GstVideoRegionOfInterestMeta *meta;
  g_mutex_lock (&g_app.mutex);
  g_app.num_detections = 0;
  while((meta = (GstVideoRegionOfInterestMeta *)gst_buffer_iterate_meta(buffer, &state)) && i<MAX_OBJECT_DETECTION) {
    gdouble score;
    GstStructure *s = gst_video_region_of_interest_meta_get_param(meta, "detection");
    const gchar *label = gst_structure_get_string(s, "label_name");
    guint label_id;
    gst_structure_get_uint(s, "label_id", &label_id);
    gst_structure_get_double(s, "confidence", &score);
    DetectedObject *o = &g_app.detected_objects[g_app.num_detections];
    g_app.num_detections++;
    gfloat wscale = (gfloat)VIDEO_WIDTH / UINT_MAX;
    gfloat hscale = (gfloat)VIDEO_HEIGHT / UINT_MAX;
    o->x = (guint)(meta->x * wscale);
    o->y = (guint)(meta->y * hscale);
    o->width  = (guint)(meta->w * wscale);
    o->height = (guint)(meta->h * hscale);
    o->class_id = label_id;
    o->score = score;
    _print_log("    new_data_cb2: got detection %u: %s (%u): %.2f%%: (%u, %u): %u x %u",
      i,
      label,
      label_id,
      100.0 * score,
      o->x,
      o->y,
      o->width,
      o->height
      );
    i++;
  }
  g_mutex_unlock (&g_app.mutex);
}

/**
 * @brief Callback for new-preroll sink signal.
 */
static GstFlowReturn
new_preroll_cb (GstElement * element, gpointer user_data)
{
  GstSample *sample;
  sample = gst_app_sink_pull_preroll((GstAppSink *)element);
  _print_log("fetched sample from preroll");
  new_data_cb2(element, gst_sample_get_buffer(sample), user_data);
  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

/**
 * @brief Callback for new-sample sink signal.
 */
static GstFlowReturn
new_sample_cb (GstElement * element, gpointer user_data)
{
  GstSample *sample;
  sample = gst_app_sink_pull_sample((GstAppSink *)element);
  _print_log("fetched sample");
  new_data_cb2(element, gst_sample_get_buffer(sample), user_data);
  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

/**
 * @brief Set window title.
 * @param name GstXImageSink element name
 * @param title window title
 */
static void
set_window_title (const gchar * name, const gchar * title)
{
  GstTagList *tags;
  GstPad *sink_pad;
  GstElement *element;

  element = gst_bin_get_by_name (GST_BIN (g_app.pipeline), name);

  g_return_if_fail (element != NULL);

  sink_pad = gst_element_get_static_pad (element, "sink");

  if (sink_pad) {
    tags = gst_tag_list_new (GST_TAG_TITLE, title, NULL);
    gst_pad_send_event (sink_pad, gst_event_new_tag (tags));
    gst_object_unref (sink_pad);
  }

  gst_object_unref (element);
}

/**
 * @brief Store the information from the caps that we are interested in.
 */
static void
prepare_overlay_cb (GstElement * overlay, GstCaps * caps, gpointer user_data)
{
  CairoOverlayState *state = &g_app.overlay_state;

  state->valid = gst_video_info_from_caps (&state->vinfo, caps);
}

/**
 * @brief Callback to draw the overlay.
 */
static void
draw_overlay_cb (GstElement * overlay, cairo_t * cr, guint64 timestamp,
    guint64 duration, gpointer user_data)
{
  CairoOverlayState *state = &g_app.overlay_state;
  gfloat x, y, width, height;
  guint drawed = 0;
  guint i;

  _print_log("called draw_overlay_cb");

  g_return_if_fail (state->valid);
  g_return_if_fail (g_app.running);

  g_mutex_lock (&g_app.mutex);

  /* set font props */
  cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
      CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, 20.0);

  for (i = 0; i < g_app.num_detections; i++) {
    DetectedObject *iter = &(g_app.detected_objects[i]);
    const gchar *label = g_app.tflite_info.labels[iter->class_id];

    x = iter->x;
    y = iter->y;
    width = iter->width;
    height = iter->height;

    /* draw rectangle */
    _print_log("draw_overlay_cb: drawing rectangle");
    cairo_rectangle (cr, x, y, width, height);
    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_set_line_width (cr, 1.5);
    cairo_stroke (cr);
    cairo_fill_preserve (cr);

    /* draw title */
    cairo_move_to (cr, x + 5, y + 25);
    cairo_text_path (cr, label);
    cairo_set_source_rgb (cr, 1, 0, 0);
    cairo_fill_preserve (cr);
    cairo_set_source_rgb (cr, 1, 1, 1);
    cairo_set_line_width (cr, .3);
    cairo_stroke (cr);
    cairo_fill_preserve (cr);

    if (++drawed >= MAX_OBJECT_DETECTION) {
      /* max objects drawed */
      break;
    }
  }
  g_mutex_unlock (&g_app.mutex);
}

/**
 * @brief Callback for message.
 */
static void
bus_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_STREAM_START: {
      _print_log ("%s: received stream-start message", GST_MESSAGE_SRC_NAME(message));
      if (FRAME_STEP) {
        if (gst_element_send_event(
              g_app.pipeline,
              gst_event_new_step(
                GST_FORMAT_BUFFERS, // step format (frames)
                1,                  // step value
                1.0,                // data rate
                TRUE,               // flush
                FALSE               // intermediate
                ))) {
          _print_log("sent step event");
        } else {
          g_warning("failed to send step event");
        }
      }
    } break;

    case GST_MESSAGE_ASYNC_DONE: {
      _print_log ("%s: received async-done message", GST_MESSAGE_SRC_NAME(message));
    } break;

    case GST_MESSAGE_STEP_DONE: {
      _print_log ("%s: received step-done message", GST_MESSAGE_SRC_NAME(message));
      if (GST_MESSAGE_SRC(message) == (GstObject *)g_app.appsink) {
        new_preroll_cb(g_app.appsink, user_data);
        //gst_element_set_state (g_app.pipeline, GST_STATE_PLAYING);
        //g_usleep(1e6);
        //gst_element_set_state (g_app.pipeline, GST_STATE_PAUSED);
        if (gst_element_send_event(
              g_app.pipeline,
              gst_event_new_step(
                GST_FORMAT_BUFFERS, // step format (frames)
                1,                  // step value
                1.0,                // data rate
                TRUE,               // flush
                FALSE               // intermediate
                ))) {
          _print_log("sent step event");
        } else {
          g_warning("failed to send step event");
        }
      }
    } break;

    case GST_MESSAGE_EOS:
      _print_log ("%s: received eos message", GST_MESSAGE_SRC_NAME(message));
      g_main_loop_quit (g_app.loop);
      break;

    case GST_MESSAGE_ERROR:
      _print_log ("%s: received error message", GST_MESSAGE_SRC_NAME(message));
      parse_err_message (message);
      g_main_loop_quit (g_app.loop);
      break;

    case GST_MESSAGE_WARNING:
      _print_log ("%s: received warning message", GST_MESSAGE_SRC_NAME(message));
      parse_err_message (message);
      break;

    case GST_MESSAGE_QOS:
      parse_qos_message (message);
      break;

    default:
      _print_log ("%s: received unhandled message: %s",
          GST_MESSAGE_SRC_NAME(message),
          GST_MESSAGE_TYPE_NAME(message)
          );
      break;
  }
}

/**
 * @brief Main function.
 */
int
main (int argc, char ** argv)
{

  gchar *str_pipeline;

  _print_log ("start app..");

  /* init app variable */
  g_app.running = FALSE;
  g_app.loop = NULL;
  g_app.bus = NULL;
  g_app.pipeline = NULL;
  g_app.num_detections = 0;
  g_mutex_init (&g_app.mutex);

  _check_cond_err (tflite_init_info (&g_app.tflite_info, tflite_model_path));

  /* init gstreamer */
  gst_init (&argc, &argv);

  /* main loop */
  g_app.loop = g_main_loop_new (NULL, FALSE);
  _check_cond_err (g_app.loop != NULL);

  /* init pipeline */
  str_pipeline =
      g_strdup_printf
      ("filesrc location=%s/%s ! qtdemux name=demux  demux.video_0 ! decodebin ! videoconvert ! videoscale ! videorate ! "
      "video/x-raw,width=%d,height=%d,format=RGB,framerate=24/1 ! tee name=t_raw "
      "t_raw. ! queue max-size-buffers=1 max-size-bytes=0 max-size-time=0 ! videoconvert ! cairooverlay name=tensor_res ! ximagesink name=img_tensor "
      "t_raw. ! queue leaky=2 max-size-buffers=1 max-size-bytes=0 max-size-time=0 ! videoscale ! video/x-raw,width=%d,height=%d ! tensor_converter ! "
        "tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! "
        "tensor_filter framework=tensorflow-lite model=%s ! "
        //"tensor_filter framework=tensorflow model=%s "
          //"input=1:%d:%d:3 inputname=normalized_input_image_tensor inputtype=float32 "
          //"output=1:%d:%d,1:%d:%d outputname=raw_outputs/box_encodings,scale_logits outputtype=float32,float32 ! "
        "ssddecode name=decoder silent=FALSE labels=%s/%s boxpriors=%s/%s ! "
        "appsink name=appsink emit-signals=TRUE ",
      tflite_model_path, str_video_file,
      VIDEO_WIDTH, VIDEO_HEIGHT,
      MODEL_WIDTH, MODEL_HEIGHT,
      g_app.tflite_info.model_path,
      tflite_model_path, tflite_label,
      tflite_model_path, tflite_box_priors
      );
      //g_app.tflite_info.model_path,
      //MODEL_WIDTH, MODEL_HEIGHT,
      //DETECTION_MAX, BOX_SIZE, DETECTION_MAX, LABEL_SIZE);

  _print_log ("%s\n", str_pipeline);

  g_app.pipeline = gst_parse_launch (str_pipeline, NULL);
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(g_app.pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "pipeline");
  g_free (str_pipeline);
  _check_cond_err (g_app.pipeline != NULL);

  /* bus and message callback */
  g_app.bus = gst_element_get_bus (g_app.pipeline);
  _check_cond_err (g_app.bus != NULL);

  gst_bus_add_signal_watch (g_app.bus);
  g_signal_connect (g_app.bus, "message", G_CALLBACK (bus_message_cb), NULL);

  /* tensor sink signal : new data callback */
  g_app.appsink = gst_bin_get_by_name(GST_BIN (g_app.pipeline), "appsink");
  if (!FRAME_STEP) {
    //g_signal_connect (g_app.appsink, "new-data", G_CALLBACK (new_data_cb), NULL);
    g_signal_connect (g_app.appsink, "new-sample", G_CALLBACK (new_sample_cb), NULL);
    g_signal_connect (g_app.appsink, "new-preroll", G_CALLBACK (new_preroll_cb), NULL);
  }

  /* cairo overlay */
  g_app.tensor_res = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "tensor_res");
  g_signal_connect (g_app.tensor_res, "draw", G_CALLBACK (draw_overlay_cb), NULL);
  g_signal_connect (g_app.tensor_res, "caps-changed", G_CALLBACK (prepare_overlay_cb), NULL);

  /* start pipeline */
  if (FRAME_STEP)
    gst_element_set_state (g_app.pipeline, GST_STATE_PAUSED);
  else // normal playback
    gst_element_set_state (g_app.pipeline, GST_STATE_PLAYING);
  g_app.running = TRUE;

  /* set window title */
  set_window_title ("img_tensor", "NNStreamer Example");

  /* run main loop */
  g_main_loop_run (g_app.loop);

  /* quit when received eos or error message */
  g_app.running = FALSE;
  gst_element_set_state (g_app.pipeline, GST_STATE_NULL);

  g_usleep (200 * 1000);


error:
  _print_log ("close app..");

  free_app_data ();
  return 0;
}
