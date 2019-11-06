project('nnplugins', 'c',
  version : '0.1.0',
  meson_version : '>= 0.42.0',
  default_options : [
    'warning_level=1',
    'c_std=gnu89',
    'buildtype=debugoptimized'
  ]
)

gst_version = meson.project_version()
gst_api_version = '1.0'
gst_dep = dependency('gstreamer-1.0', fallback : ['gstreamer', 'gst_dep'])
gst_app_dep = dependency('gstreamer-app-' + gst_api_version)
gst_video_dep = dependency('gstreamer-video-' + gst_api_version)
plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')

cc = meson.get_compiler('c')
libm_dep = cc.find_library('m', required : true)
cairo_dep = dependency('cairo', required: false)
tflite_dep = dependency('tensorflow-lite', required: false)
nnstreamer_dep = dependency('nnstreamer', required: false)
assert(cc.has_header('nnstreamer/tensor_typedef.h'), 'Missing NNStreamer header files')

plugin_c_args = ['-DHAVE_CONFIG_H']
tests_c_args = []

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', gst_version)
cdata.set_quoted('PACKAGE', 'gst-nn-plugins')
cdata.set_quoted('GST_LICENSE', 'LGPL')
cdata.set_quoted('GST_API_VERSION', gst_api_version)
cdata.set_quoted('GST_PACKAGE_NAME', 'GStreamer Neural Network Plug-ins')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://gstreamer.freedesktop.org')
configure_file(output : 'config.h', configuration : cdata)

# Plugin ssddecode
gstssddecode = library('gstssddecode',
  [
    'src/gstssddecode.c',
    'src/libtensordecode.c',
  ],
  c_args: plugin_c_args,
  dependencies : [gst_dep, gst_video_dep, libm_dep],
  install : true,
  install_dir : plugins_install_dir,
)


# Tests
subdir('tests')
