/*
 * No license installed
 */

#ifndef __LIB_TENSORDECODE_H__
#define __LIB_TENSORDECODE_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/* FIXME: Consider making these properties with default values */
#define Y_SCALE         10.0f
#define X_SCALE         10.0f
#define H_SCALE         5.0f
#define W_SCALE         5.0f
#define DETECTION_MAX   1917
#define BOX_SIZE        4
#define LABEL_SIZE      91
#define THRESHOLD_SCORE 0.5f
#define THRESHOLD_IOU   0.0f
#define EXPIT(x) (1.f / (1.f + expf (-x)))

typedef struct _DetectedObject
{
  guint x;
  guint y;
  guint width;
  guint height;
  guint class_id;
  const gchar *class_label;
  gfloat score;
} DetectedObject;

gboolean read_lines (const gchar *file_name, GList **lines);
gboolean tflite_load_labels (const gchar *labels_path, const gchar *labels[LABEL_SIZE]);
gboolean tflite_load_box_priors (const gchar *box_priors_path, gfloat box_priors[BOX_SIZE][DETECTION_MAX]);
gboolean get_detected_objects (gfloat box_priors[BOX_SIZE][DETECTION_MAX], const gchar *labels[LABEL_SIZE], const gfloat *predictions, const gfloat *boxes, DetectedObject *detections, guint *num_detections);

G_END_DECLS

#endif /* __LIB_TENSORDECODE_H__ */
