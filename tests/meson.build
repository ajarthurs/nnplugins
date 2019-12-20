if not get_option('tests').disabled()
  subdir('bbdecode')
  subdir('ssddecode')
endif

if cairo_dep.found() and tflite_dep.found()
  executable('test_segmap_tflite',
    [
      'test_segmap_tflite.c',
      'libtests.c',
      '../src/libtensordecode.c',
    ],
    install: false,
    dependencies: [gst_dep, gst_app_dep, gst_video_dep, libm_dep, nnstreamer_dep, tflite_dep, cairo_dep],
    c_args: tests_c_args,
  )
  executable('test_segmap_quantized_tflite',
    [
      'test_segmap_quantized_tflite.c',
      'libtests.c',
      '../src/libtensordecode.c',
    ],
    install: false,
    dependencies: [gst_dep, gst_app_dep, gst_video_dep, libm_dep, nnstreamer_dep, tflite_dep, cairo_dep],
    c_args: tests_c_args,
  )
endif
