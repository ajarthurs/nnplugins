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
#include <time.h>
#include <cairo.h>
#include <cairo-gobject.h>

/**
 * @brief Macro for debug mode.
 */
#ifndef DBG
#define DBG TRUE
#endif

/**
 * @brief Macro to check error case.
 */
#define CHECK_COND_ERR(cond) \
  do { \
    if (!(cond)) { \
      GST_ERROR ("app failed! [line : %d]", __LINE__); \
      goto error; \
    } \
  } while (0)

#define TEST_DATA_PATH "./tests/testdata"
#define TEST_VIDEO_FILE "sample_1080p.mp4"
#define TEST_COCO_LABELS_FILE "coco_labels_list.txt"

#define VIDEO_WIDTH     513
#define VIDEO_HEIGHT    513
//#define VIDEO_WIDTH     640
//#define VIDEO_HEIGHT    640
//#define VIDEO_WIDTH     1024
//#define VIDEO_HEIGHT    768

//XXX: Depends on model but must be defined here. Need to replace AppData with a handle-based framework.
#define SEGMAP_WIDTH      513
#define SEGMAP_HEIGHT     513
#define SEGMAP_CLASSES    21
#define CLASS_BACKGROUND  0
#define CLASS_CAR         7
#define CLASS_PERSON      15

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

// SampleCallback signature: processes one sample/frame per call.
typedef void (*SampleCallback) (GstElement * element, GstBuffer * buffer, gpointer user_data);

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
  gboolean frame_stepping; /**< whether or not to time the pipeline by model latency >**/
  TFLiteModelInfo tflite_info; /**< tflite model info */
  SampleCallback sample_handler; /**< callback that processes each sample/frame >**/
  CairoOverlayState overlay_state;
  guint num_detections;
  DetectedObject detected_objects[MAX_OBJECT_DETECTION];
  gfloat segmap[SEGMAP_HEIGHT][SEGMAP_WIDTH][SEGMAP_CLASSES];
  GstElement *appsink;
  GstElement *tensor_res;
  clock_t prev_update_time;
  gdouble fps;
} AppData;

extern AppData g_app;

gboolean init_test(int argc, char ** argv);
gboolean tflite_init_info (TFLiteModelInfo * tflite_info, const gchar * path, const gchar *labels_file, const gchar *tflite_model, const gchar *tflite_box_priors_file);
void free_app_data (void);
void handle_bb_sample (GstElement * element, GstBuffer * buffer, gpointer user_data);
void handle_segmap_sample (GstElement * element, GstBuffer * buffer, gpointer user_data);
GstFlowReturn new_preroll_cb (GstElement * element, gpointer user_data);
GstFlowReturn new_sample_cb (GstElement * element, gpointer user_data);
void set_window_title (const gchar * name, const gchar * title);
void prepare_overlay_cb (GstElement * overlay, GstCaps * caps, gpointer user_data);
void draw_bb_overlay_cb (GstElement * overlay, cairo_t * cr, guint64 timestamp, guint64 duration, gpointer user_data);
void draw_segmap_overlay_cb (GstElement * overlay, cairo_t * cr, guint64 timestamp, guint64 duration, gpointer user_data);
void bus_message_cb (GstBus * bus, GstMessage * message, gpointer user_data);
