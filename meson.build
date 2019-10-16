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
gst_video_dep = dependency('gstreamer-video-' + gst_api_version)
plugins_install_dir = join_paths(get_option('libdir'), 'gstreamer-1.0')

nnstreamer_dep = dependency('nnstreamer')

cc = meson.get_compiler('c')
libm = cc.find_library('m', required : true)

plugin_c_args = ['-DHAVE_CONFIG_H']

cdata = configuration_data()
cdata.set_quoted('PACKAGE_VERSION', gst_version)
cdata.set_quoted('PACKAGE', 'gst-nn-plugins')
cdata.set_quoted('GST_LICENSE', 'LGPL')
cdata.set_quoted('GST_API_VERSION', gst_api_version)
cdata.set_quoted('GST_PACKAGE_NAME', 'GStreamer Neural Network Plug-ins')
cdata.set_quoted('GST_PACKAGE_ORIGIN', 'https://gstreamer.freedesktop.org')
configure_file(output : 'config.h', configuration : cdata)

# Plugin tensordecode
plugin_sources = [
  'src/gsttensordecode.c',
  'src/libtensordecode.c',
  ]

gsttensordecode = library('gsttensordecode',
  plugin_sources,
  c_args: plugin_c_args,
  dependencies : [gst_dep, gst_video_dep, nnstreamer_dep, libm],
  install : true,
  install_dir : plugins_install_dir,
)
