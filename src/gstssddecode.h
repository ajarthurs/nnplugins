/*
 * No license installed
 */

#ifndef __GST_SSDDECODE_H__
#define __GST_SSDDECODE_H__

#include <gst/gst.h>
#include <gst/video/gstvideometa.h>
#include "libtensordecode.h"

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_SSDDECODE \
  (gst_ssddecode_get_type())
#define GST_SSDDECODE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SSDDECODE,GstSSDDecode))
#define GST_SSDDECODE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SSDDECODE,GstSSDDecodeClass))
#define GST_IS_SSDDECODE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SSDDECODE))
#define GST_IS_SSDDECODE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SSDDECODE))

typedef struct _GstSSDDecode      GstSSDDecode;
typedef struct _GstSSDDecodeClass GstSSDDecodeClass;

struct _GstSSDDecode
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  const gchar *labels_path;
  const gchar *box_priors_path;
  gfloat box_priors[BOX_SIZE][DETECTION_MAX];
  const gchar *labels[LABEL_SIZE];
  gboolean silent;
};

struct _GstSSDDecodeClass 
{
  GstElementClass parent_class;
};

GType gst_ssddecode_get_type (void);

G_END_DECLS

#endif /* __GST_SSDDECODE_H__ */
