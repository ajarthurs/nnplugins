/* Common defintions for tests */
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
#include "../src/libtensordecode.h"

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
//#define tflite_model "detect.tflite"
//#define tflite_model "ssd_mobilenet_v1_coco_postprocessed.tflite"
//#define tflite_model "ssd_mobilenet_v1_coco_uint8.tflite"
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

extern AppData g_app;

gboolean tflite_init_info (TFLiteModelInfo * tflite_info, const gchar * path, const gchar *tflite_model);
void free_app_data (void);
GstFlowReturn new_preroll_cb (GstElement * element, gpointer user_data);
GstFlowReturn new_sample_cb (GstElement * element, gpointer user_data);
void set_window_title (const gchar * name, const gchar * title);
void prepare_overlay_cb (GstElement * overlay, GstCaps * caps, gpointer user_data);
void draw_overlay_cb (GstElement * overlay, cairo_t * cr, guint64 timestamp, guint64 duration, gpointer user_data);
void bus_message_cb (GstBus * bus, GstMessage * message, gpointer user_data);
