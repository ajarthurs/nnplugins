/*
 * No license installed
 */

#ifndef __GST_BBDECODE_H__
#define __GST_BBDECODE_H__

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
#include "libtensordecode.h"

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_BBDECODE \
  (gst_bbdecode_get_type())
#define GST_BBDECODE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BBDECODE,GstBBDecode))
#define GST_BBDECODE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BBDECODE,GstBBDecodeClass))
#define GST_IS_BBDECODE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BBDECODE))
#define GST_IS_BBDECODE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BBDECODE))

typedef struct _GstBBDecode      GstBBDecode;
typedef struct _GstBBDecodeClass GstBBDecodeClass;

struct _GstBBDecode
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  const gchar *labels_path;
  const gchar *labels[LABEL_SIZE];
  gboolean silent;
};

struct _GstBBDecodeClass 
{
  GstElementClass parent_class;
};

GType gst_bbdecode_get_type (void);

G_END_DECLS

#endif /* __GST_BBDECODE_H__ */
