plugin_LTLIBRARIES = libtensordecode.la libgstssddecode.la

##############################################################################
# Tensor Decoder Utilities/Common Functions
##############################################################################

# sources used to compile this plug-in
libtensordecode_la_SOURCES = libtensordecode.c libtensordecode.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libtensordecode_la_CFLAGS = $(GST_CFLAGS)
libtensordecode_la_LIBADD = $(GST_LIBS)
libtensordecode_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libtensordecode_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = libtensordecode.h

##############################################################################
# SSD Decoder
##############################################################################

# sources used to compile this plug-in
libgstssddecode_la_SOURCES = gstssddecode.c gstssddecode.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstssddecode_la_CFLAGS = $(GST_CFLAGS)
libgstssddecode_la_LIBADD = $(GST_LIBS)
libgstssddecode_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstssddecode_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstssddecode.h
