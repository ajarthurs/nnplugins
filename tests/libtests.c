/**
 * @brief Library for unit tests.
 */

#include "../libtests.h"

/**
 * @brief Check tflite model and load labels.
 */
gboolean
tflite_init_info (TFLiteModelInfo * tflite_info, const gchar * path, const gchar *tflite_model)
{
  g_return_val_if_fail (tflite_info != NULL, FALSE);
  tflite_info->model_path = g_strdup_printf ("%s/%s", path, tflite_model);
  tflite_info->label_path = g_strdup_printf ("%s/%s", path, tflite_label);
  tflite_info->box_prior_path =
      g_strdup_printf ("%s/%s", path, tflite_box_priors);

  if (!g_file_test (tflite_info->model_path, G_FILE_TEST_IS_REGULAR)) {
    GST_ERROR ("the file of model_path is not valid: %s\n", tflite_info->model_path);
    return FALSE;
  }
  if (!g_file_test (tflite_info->label_path, G_FILE_TEST_IS_REGULAR)) {
    GST_ERROR ("the file of label_path is not valid%s\n", tflite_info->label_path);
    return FALSE;
  }
  if (!g_file_test (tflite_info->box_prior_path, G_FILE_TEST_IS_REGULAR)) {
    GST_ERROR ("the file of box_prior_path is not valid%s\n", tflite_info->box_prior_path);
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
void
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
  GST_LOG ("%s: format[%d] processed[%" G_GUINT64_FORMAT "] dropped[%"
      G_GUINT64_FORMAT "]", GST_MESSAGE_SRC_NAME(message), format, processed, dropped);
}

/**
 * @brief Callback for tensor sink signal.
 */
static void
new_data_cb2 (GstElement * element, GstBuffer * buffer, gpointer user_data)
{
  guint i = 0;
  clock_t now, tdelta;
  gpointer state = NULL;
  GST_LOG_OBJECT(element, "called new_data_cb2");
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
    GST_LOG_OBJECT(element, "    new_data_cb2: got detection %u: %s (%u): %.2f%%: (%u, %u): %u x %u",
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
  /* Calculate FPS and log time of detections update */
  //g_app.fps = 1.0 / (time(NULL) - g_app.prev_update_time);
  now = clock();
  tdelta = now - g_app.prev_update_time;
  if (tdelta > 0)
    g_app.fps = CLOCKS_PER_SEC / (gdouble)tdelta;
  else
    g_app.fps = 0.0;
  g_app.prev_update_time = now;
  g_mutex_unlock (&g_app.mutex);
}

/**
 * @brief Callback for new-preroll sink signal.
 */
GstFlowReturn
new_preroll_cb (GstElement * element, gpointer user_data)
{
  GstSample *sample;
  sample = gst_app_sink_pull_preroll((GstAppSink *)element);
  GST_LOG_OBJECT(element, "fetched sample from preroll");
  new_data_cb2(element, gst_sample_get_buffer(sample), user_data);
  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

/**
 * @brief Callback for new-sample sink signal.
 */
GstFlowReturn
new_sample_cb (GstElement * element, gpointer user_data)
{
  GstSample *sample;
  sample = gst_app_sink_pull_sample((GstAppSink *)element);
  GST_LOG_OBJECT(element, "fetched sample");
  new_data_cb2(element, gst_sample_get_buffer(sample), user_data);
  gst_sample_unref(sample);
  return GST_FLOW_OK;
}

/**
 * @brief Set window title.
 * @param name GstXImageSink element name
 * @param title window title
 */
void
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
void
prepare_overlay_cb (GstElement * overlay, GstCaps * caps, gpointer user_data)
{
  CairoOverlayState *state = &g_app.overlay_state;
  state->valid = gst_video_info_from_caps (&state->vinfo, caps);
}

/**
 * @brief Callback to draw the overlay.
 */
void
draw_overlay_cb (GstElement * overlay, cairo_t * cr, guint64 timestamp,
    guint64 duration, gpointer user_data)
{
  CairoOverlayState *state = &g_app.overlay_state;
  gfloat x, y, width, height;
  guint drawed = 0;
  guint i;
  char str[32];
  GST_LOG_OBJECT(overlay, "called draw_overlay_cb");
  g_return_if_fail (state->valid);
  g_return_if_fail (g_app.running);
  g_mutex_lock (&g_app.mutex);
  /* set font props */
  cairo_select_font_face (cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
      CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size (cr, 20.0);
  /* draw FPS */
  snprintf(str, 32, "FPS=%.2f", g_app.fps);
  cairo_move_to (cr, 50, 50);
  cairo_text_path (cr, str);
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_fill_preserve (cr);
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_set_line_width (cr, .3);
  cairo_stroke (cr);
  cairo_fill_preserve (cr);
  /* iterate over detections */
  for (i = 0; i < g_app.num_detections; i++) {
    DetectedObject *iter = &(g_app.detected_objects[i]);
    const gchar *label = g_app.tflite_info.labels[iter->class_id];
    x = iter->x;
    y = iter->y;
    width = iter->width;
    height = iter->height;
    /* skip if out-of-bounds */
    if(x < 0 || y < 0 || (x+width+1) > VIDEO_WIDTH || (y+height+1) > VIDEO_HEIGHT)
      continue;
    /* draw rectangle */
    GST_LOG_OBJECT(overlay, "draw_overlay_cb: drawing rectangle");
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
void
bus_message_cb (GstBus * bus, GstMessage * message, gpointer user_data)
{
  switch (GST_MESSAGE_TYPE (message)) {
    case GST_MESSAGE_STREAM_START: {
      GST_INFO_OBJECT (bus, "received stream-start message");
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
          GST_INFO_OBJECT(bus, "sent first step event");
        } else {
          GST_WARNING_OBJECT(bus, "failed to send step event");
        }
      } else { // Normal playback
        GST_INFO_OBJECT(bus, "started stream for normal playback");
      }
    } break;
    case GST_MESSAGE_ASYNC_DONE: {
      GST_LOG_OBJECT (bus, "%s: received async-done message", GST_MESSAGE_SRC_NAME(message));
    } break;
    case GST_MESSAGE_STEP_DONE: {
      GST_LOG_OBJECT (bus, "%s: received step-done message", GST_MESSAGE_SRC_NAME(message));
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
          GST_LOG_OBJECT(bus, "sent step event");
        } else {
          GST_WARNING_OBJECT(bus, "failed to send step event");
        }
      }
    } break;
    case GST_MESSAGE_EOS:
      GST_INFO_OBJECT (bus, "received eos message");
      g_main_loop_quit (g_app.loop);
      break;
    case GST_MESSAGE_ERROR:
      GST_ERROR_OBJECT (bus, "received error message");
      parse_err_message (message);
      g_main_loop_quit (g_app.loop);
      break;
    case GST_MESSAGE_WARNING:
      GST_WARNING_OBJECT (bus, "received warning message");
      parse_err_message (message);
      break;
    case GST_MESSAGE_QOS:
      parse_qos_message (message);
      break;
    default:
      GST_LOG_OBJECT (bus, "%s: received unhandled message: %s",
          GST_MESSAGE_SRC_NAME(message),
          GST_MESSAGE_TYPE_NAME(message)
          );
      break;
  }
}
