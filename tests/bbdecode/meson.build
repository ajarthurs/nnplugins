if cairo_dep.found() and tflite_dep.found()
  executable('test_object_detection_tflite',
    [
      'test_object_detection_tflite.c',
      '../libtests.c',
      '../../src/libtensordecode.c',
    ],
    install: false,
    dependencies: [gst_dep, gst_app_dep, gst_video_dep, libm_dep, nnstreamer_dep, tflite_dep, cairo_dep],
    c_args: tests_c_args,
  )

  executable('test_object_detection_tflite_with_webcam',
    [
      'test_object_detection_tflite_with_webcam.c',
      '../libtests.c',
      '../../src/libtensordecode.c',
    ],
    install: false,
    dependencies: [gst_dep, gst_app_dep, gst_video_dep, libm_dep, nnstreamer_dep, tflite_dep, cairo_dep],
    c_args: tests_c_args,
  )
endif
