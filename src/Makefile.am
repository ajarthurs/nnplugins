plugin_LTLIBRARIES = libgstssddecode.la libgstbbdecode.la

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

##############################################################################
# BB Decoder
##############################################################################

# sources used to compile this plug-in
libgstbbdecode_la_SOURCES = gstbbdecode.c gstbbdecode.h

# compiler and linker flags used to compile this plugin, set in configure.ac
libgstbbdecode_la_CFLAGS = $(GST_CFLAGS)
libgstbbdecode_la_LIBADD = $(GST_LIBS)
libgstbbdecode_la_LDFLAGS = $(GST_PLUGIN_LDFLAGS)
libgstbbdecode_la_LIBTOOLFLAGS = --tag=disable-static

# headers we need but don't want installed
noinst_HEADERS = gstbbdecode.h
