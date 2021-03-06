/*
 * No license installed
 */

/**
 * SECTION:library-tensordecode
 *
 * Utility functions for tensordecode
 *
 */

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <gst/gst.h>
#include "libtensordecode.h"

/**
 * @brief `qsort` callback: Compare score of detected objects in descending order.
 */
static gint
compare_detection_scores (const void *A, const void *B)
{
  const DetectedObject *a = (DetectedObject *)A, *b = (DetectedObject *)B;
  if (a->score > b->score)
    return -1;
  else if(a->score < b->score)
    return 1;
  else
    return 0;
}

/**
 * @brief Intersection of union
 */
static gfloat
iou (const DetectedObject *a, const DetectedObject *b)
{
  guint x1 = (a->x > b->x)? a->x : b->x;
  guint y1 = (a->y > b->y)? a->y : b->y;
  guint ax2 = a->x + a->width;
  guint bx2 = b->x + b->width;
  guint ay2 = a->y + a->height;
  guint by2 = b->y + b->height;
  guint x2 = (ax2 < bx2)? ax2 : bx2;
  guint y2 = (ay2 < by2)? ay2 : by2;
  gfloat w  = (gfloat)x2 - x1;
  gfloat h  = (gfloat)y2 - y1;
  gfloat inter = w * h;
  gfloat areaA = (gfloat)a->width * a->height;
  gfloat areaB = (gfloat)b->width * b->height;
  gfloat o = inter / (areaA + areaB - inter);
  gfloat iou = fmax (0.0f, o);
  GST_DEBUG("IOU = %e; o = %e; %e x %e", iou, o, w, h);
  return iou;
}

/**
 * @brief NMS (non-maximum suppression)
 */
static guint
nms (DetectedObject *detections, guint num_detections)
{
  guint i, j, k, num_overlaps = 0, num_nonoverlaps, num_detections_capped;
  gboolean del[DETECTION_MAX * LABEL_SIZE];
  if (num_detections > 100)
    num_detections_capped = 100;
  else
    num_detections_capped = num_detections;
  qsort(detections, num_detections_capped, sizeof(DetectedObject), compare_detection_scores);
  for (i = 0; i < num_detections_capped; i++) {
    del[i] = FALSE;
  }
  for (i = 0; i < num_detections_capped; i++) {
    if (del[i])
      continue;

    for (j = (i + 1); j < num_detections_capped; j++) {
      if (!del[j] &&
        detections[i].class_id == detections[j].class_id &&
        iou (&detections[i], &detections[j]) > THRESHOLD_IOU
        ) {
        del[j] = TRUE;
        num_overlaps++;
      }
    }
  }
  num_nonoverlaps = num_detections_capped - num_overlaps;
  for (i = 0; i < num_nonoverlaps; i++) {
    if (!del[i])
      continue;

    j = i;
    while(del[i] && j < (num_detections_capped-1)) {
      for(k = i; k < (num_detections_capped-1); k++) {
        del[k] = del[k+1];
        memcpy(&detections[k], &detections[k+1], sizeof(DetectedObject));
      }
      j++;
    }
    num_detections_capped -= (j-i);
  }
  return num_nonoverlaps;
}


/**
 * @brief Get detected objects.
 */
gboolean
get_detected_objects (gfloat box_priors[BOX_SIZE][DETECTION_MAX], const gchar *labels[LABEL_SIZE], const gfloat *predictions, const gfloat *boxes, DetectedObject *detections, guint *num_detections)
{
  *num_detections = 0;
  guint d, l;
  for (d = 0; d < DETECTION_MAX; d++) {
    gfloat ycenter = ((boxes[0] / Y_SCALE) * box_priors[2][d]) + box_priors[0][d];
    gfloat xcenter = ((boxes[1] / X_SCALE) * box_priors[3][d]) + box_priors[1][d];
    gfloat h = (gfloat) expf (boxes[2] / H_SCALE) * box_priors[2][d];
    gfloat w = (gfloat) expf (boxes[3] / W_SCALE) * box_priors[3][d];

    gfloat ymin = ycenter - h / 2.f;
    gfloat xmin = xcenter - w / 2.f;
    gfloat ymax = ycenter + h / 2.f;
    gfloat xmax = xcenter + w / 2.f;

    for (l = 1; l < LABEL_SIZE; l++) {
      gfloat score = EXPIT (predictions[l]);
      /**
       * This score cutoff is taken from Tensorflow's demo app.
       * There are quite a lot of nodes to be run to convert it to the useful possibility
       * scores. As a result of that, this cutoff will cause it to lose good detections in
       * some scenarios and generate too much noise in other scenario.
       */
      if (score < THRESHOLD_SCORE)
        continue;

      detections[*num_detections].class_id = l;
      detections[*num_detections].class_label = labels[l];
      detections[*num_detections].x = UINT_MAX * xmin;
      detections[*num_detections].y = UINT_MAX * ymin;
      detections[*num_detections].width = UINT_MAX * (xmax - xmin);
      detections[*num_detections].height = UINT_MAX * (ymax - ymin);
      detections[*num_detections].score = score;
      (*num_detections)++;
    }
    predictions += LABEL_SIZE;
    boxes += BOX_SIZE;
  }
  *num_detections = nms (detections, *num_detections);
  return TRUE;
}

/**
 * @brief Read strings from file.
 */
gboolean
read_lines (const gchar *file_name, GList **lines)
{
  FILE *stream = fopen(file_name, "r");
  gchar *line = NULL;
  size_t len = 0;
  ssize_t nread;
  if (stream == NULL) {
    GST_ERROR ("Failed to open file %s", file_name);
    return FALSE;
  }
  while ((nread = getline (&line, &len, stream)) != -1) {
    line[nread-1] = '\0'; /* remove extraneous newline character */
    *lines = g_list_append (*lines, g_strdup (line));
  }
  free (line);
  fclose(stream);

  return TRUE;
}

/**
 * @brief Load labels.
 */
gboolean
tflite_load_labels (const gchar *labels_path, const gchar *labels[LABEL_SIZE])
{
  guint i;
  GList *lines = NULL;
  g_return_val_if_fail(read_lines (labels_path, &lines), FALSE);
  for (i = 0; i < LABEL_SIZE; i++) {
    labels[i] = (gchar *) g_list_nth_data (lines, i);
  }
  return TRUE;
}

/**
 * @brief Load box priors.
 */
gboolean
tflite_load_box_priors (const gchar *box_priors_path, gfloat box_priors[BOX_SIZE][DETECTION_MAX])
{
  guint row;
  gchar *box_row;
  GList *box_priors_lines = NULL;

  g_return_val_if_fail (read_lines (box_priors_path, &box_priors_lines), FALSE);

  for (row = 0; row < BOX_SIZE; row++) {
    guint column = 0;
    guint i = 0, j = 0;
    gchar buff[11];

    memset (buff, 0, 11);
    box_row = (gchar *) g_list_nth_data (box_priors_lines, row);

    while ((box_row[i] != '\n') && (box_row[i] != '\0')) {
      if (box_row[i] != ' ') {
        buff[j] = box_row[i];
        j++;
      } else {
        if (j != 0) {
          box_priors[row][column++] = atof (buff);
          memset (buff, 0, 11);
        }
        j = 0;
      }
      i++;
    }

    box_priors[row][column++] = atof (buff);
  }
  g_list_free_full (box_priors_lines, g_free);
  return TRUE;
}
