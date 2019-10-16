##############################################################################
# Tensor Decoder
##############################################################################
plugin_LTLIBRARIES = libgsttensordecode.la

# sources used to compile this plug-in
libgsttensordecode_la_SOURCES = gsttensordecode.c libtensordecode.c gsttensordecode.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgsttensordecode_la_CFLAGS = $(GST_CFLAGS)
libgsttensordecode_la_LIBADD = $(GST_LIBS)
libgsttensordecode_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgsttensordecode_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gsttensordecode.h
