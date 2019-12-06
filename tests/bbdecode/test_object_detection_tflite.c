/**
 * @brief	Tensor stream example with TF-Lite model for object detection
 */

#include "../libtests.h"

#define tflite_model "ssd_mobilenet_v1_coco_postprocessed_uint8.tflite"
GST_DEBUG_CATEGORY_STATIC(myapp);
#define GST_CAT_DEFAULT myapp

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
  /* init app variable */
  g_app.running = FALSE;
  g_app.loop = NULL;
  g_app.bus = NULL;
  g_app.pipeline = NULL;
  g_app.num_detections = 0;
  g_mutex_init (&g_app.mutex);

  _check_cond_err (tflite_init_info (&g_app.tflite_info, tflite_model_path, tflite_model));

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
        //"tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! "
        //"tensor_transform mode=arithmetic option=typecast:uint8,add:0 ! "
        "tensor_filter framework=tensorflow-lite model=%s ! "
        //"tensor_filter framework=tensorflow model=%s "
          //"input=1:%d:%d:3 inputname=normalized_input_image_tensor inputtype=float32 "
          //"output=1:%d:%d,1:%d:%d outputname=raw_outputs/box_encodings,scale_logits outputtype=float32,float32 ! "
        //"other/tensors, num_tensors=(int)2, framerate=(fraction)[ 0/1, 2147483647/1 ], dimensions=(string)"4:1917:1:1\,91:1917:1:1", types=(string)"uint8\,uint8"
        //"tensor_split name=ssds tensorseg=1:1917:4,1:1917:91 silent=FALSE "
        //  "ssds.src_0 ! tensor_transform mode=arithmetic option=typecast:float32,add:-180.0,mul:0.0448576174609375 ! ssdm.sink_0 "
        //  "ssds.src_1 ! tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! ssdm.sink_1 "
        //"tensor_merge name=ssdm ! "
        "bbdecode name=decoder labels=%s/%s ! "
        "appsink name=appsink emit-signals=TRUE ",
      tflite_model_path, str_video_file,
      VIDEO_WIDTH, VIDEO_HEIGHT,
      MODEL_WIDTH, MODEL_HEIGHT,
      g_app.tflite_info.model_path,
      tflite_model_path, tflite_label
      );
      //g_app.tflite_info.model_path,
      //MODEL_WIDTH, MODEL_HEIGHT,
      //DETECTION_MAX, BOX_SIZE, DETECTION_MAX, LABEL_SIZE);

  GST_INFO ("%s", str_pipeline);
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
  set_window_title ("img_tensor", "NNStreamer Example");
  g_main_loop_run (g_app.loop);

  /* quit when received eos or error message */
  g_app.running = FALSE;
  gst_element_set_state (g_app.pipeline, GST_STATE_NULL);

  g_usleep (200 * 1000);


error:
  free_app_data ();
  return 0;
}
