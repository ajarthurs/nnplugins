/**
 * @brief	Tensor stream example with TF-Lite model for object detection
 */

#include "../libtests.h"

GST_DEBUG_CATEGORY_STATIC(myapp);
#define GST_CAT_DEFAULT myapp
#define TFLITE_MODEL_FILE "ssd_mobilenet_v1_coco_uint8_batch2.tflite"
#define TFLITE_BOX_PRIORS_FILE "box_priors-ssd_mobilenet.txt"
#define MODEL_WIDTH     300
#define MODEL_HEIGHT    300

/**
* @brief Data for pipeline and result.
*/
AppData g_app;

/**
 * @brief Main function.
 */
int
main (int argc, char ** argv)
{
  gchar *str_pipeline;
  g_app.frame_stepping = TRUE;
  g_app.sample_handler = &handle_bb_sample;
  CHECK_COND_ERR(init_test(argc, argv));
  GST_DEBUG_CATEGORY_INIT (myapp, "via-nnplugins-test", 0, "Test object detection with multiple video files");
  CHECK_COND_ERR (tflite_init_info (&g_app.tflite_info, TEST_DATA_PATH, TEST_COCO_LABELS_FILE, TFLITE_MODEL_FILE, TFLITE_BOX_PRIORS_FILE));
  /* init pipeline */
  str_pipeline =
      g_strdup_printf(
      // video source 0
      "filesrc location=%s/%s ! qtdemux name=demux1  demux1.video_0 ! decodebin ! videoconvert ! videoscale ! videorate ! "
        "video/x-raw,width=%d,height=%d,format=RGB,framerate=24/1 ! tee name=t_raw0 "
      // video source 1
      "filesrc location=%s/%s ! qtdemux name=demux2  demux2.video_0 ! decodebin ! videoconvert ! videoscale ! videorate ! "
        "video/x-raw,width=%d,height=%d,format=RGB,framerate=24/1 ! tee name=t_raw1 "
      // X window 0
      "t_raw0. ! queue max-size-buffers=1 max-size-bytes=0 max-size-time=0 ! videoconvert ! cairooverlay name=tensor_res0 ! ximagesink name=img_tensor0 "
      // X window 1
      "t_raw1. ! queue max-size-buffers=1 max-size-bytes=0 max-size-time=0 ! videoconvert ! cairooverlay name=tensor_res1 ! ximagesink name=img_tensor1 "
      // Tensor conversion 0
      "t_raw0. ! queue leaky=2 max-size-buffers=1 max-size-bytes=0 max-size-time=0 ! videoscale ! video/x-raw,width=%d,height=%d ! tensor_converter ! tm.sink_0 "
      // Tensor conversion 1
      "t_raw1. ! queue leaky=2 max-size-buffers=1 max-size-bytes=0 max-size-time=0 ! videoscale ! video/x-raw,width=%d,height=%d ! tensor_converter ! tm.sink_1 "
      // Merge along batch-dimension (`option=3`) and output a multi-stream tensor.
      "tensor_merge name=tm mode=linear option=3 ! "
      "tensor_filter framework=tensorflow-lite model=%s ! "
      "ssddecode name=decoder batch-size=2 dequant=TRUE labels=%s/%s boxpriors=%s/%s ! "
      "appsink name=appsink emit-signals=TRUE "
      ,
      TEST_DATA_PATH, TEST_VIDEO_FILE_B,
      VIDEO_WIDTH, VIDEO_HEIGHT,
      TEST_DATA_PATH, TEST_VIDEO_FILE,
      VIDEO_WIDTH, VIDEO_HEIGHT,
      MODEL_WIDTH, MODEL_HEIGHT,
      MODEL_WIDTH, MODEL_HEIGHT,
      g_app.tflite_info.model_path,
      TEST_DATA_PATH, TEST_COCO_LABELS_FILE,
      TEST_DATA_PATH, TFLITE_BOX_PRIORS_FILE
      );
      //g_app.tflite_info.model_path,
      //MODEL_WIDTH, MODEL_HEIGHT,
      //DETECTION_MAX, BOX_SIZE, DETECTION_MAX, LABEL_SIZE);
  GST_INFO ("%s", str_pipeline);
  g_app.pipeline = gst_parse_launch (str_pipeline, NULL);
  GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(g_app.pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "pipeline");
  g_free (str_pipeline);
  CHECK_COND_ERR (g_app.pipeline != NULL);
  /* bus and message callback */
  g_app.bus = gst_element_get_bus (g_app.pipeline);
  CHECK_COND_ERR (g_app.bus != NULL);
  gst_bus_add_signal_watch (g_app.bus);
  g_signal_connect (g_app.bus, "message", G_CALLBACK (bus_message_cb), NULL);
  /* tensor sink signal : new data callback */
  g_app.appsink  = gst_bin_get_by_name(GST_BIN (g_app.pipeline), "appsink");
  if (!g_app.frame_stepping) {
    g_signal_connect (g_app.appsink, "new-sample", G_CALLBACK (new_sample_cb), NULL);
    g_signal_connect (g_app.appsink, "new-preroll", G_CALLBACK (new_preroll_cb), NULL);
  }
  /* cairo overlay */
  g_app.tensor_res0 = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "tensor_res0");
  g_signal_connect (g_app.tensor_res0, "draw", G_CALLBACK (draw_bb_overlay_cb), 0);
  g_signal_connect (g_app.tensor_res0, "caps-changed", G_CALLBACK (prepare_overlay_cb), 0);
  g_app.tensor_res1 = gst_bin_get_by_name (GST_BIN (g_app.pipeline), "tensor_res1");
  g_signal_connect (g_app.tensor_res1, "draw", G_CALLBACK (draw_bb_overlay_cb), 1);
  g_signal_connect (g_app.tensor_res1, "caps-changed", G_CALLBACK (prepare_overlay_cb), 1);
  /* start pipeline */
  if (g_app.frame_stepping)
    gst_element_set_state (g_app.pipeline, GST_STATE_PAUSED);
  else // normal playback
    gst_element_set_state (g_app.pipeline, GST_STATE_PLAYING);
  g_app.running = TRUE;
  /* set window title */
  set_window_title ("img_tensor0", "NNStreamer Stream 0");
  set_window_title ("img_tensor1", "NNStreamer Stream 1");
  /* run main loop */
  g_main_loop_run (g_app.loop);
  /* quit when received eos or error message */
  g_app.running = FALSE;
  gst_element_set_state (g_app.pipeline, GST_STATE_NULL);
  g_usleep (200 * 1000);
  free_app_data ();
  GST_INFO ("close app..");
  return 0;

error:
  free_app_data ();
  GST_ERROR ("See above for error.");
  return 1;
}
