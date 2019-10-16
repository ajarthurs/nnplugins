/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2019 Aaron Arthurs <aajarthurs@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:library-tensordecode
 *
 * Utility functions for tensordecode
 *
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <gst/gst.h>
#include "gsttensordecode.h"

/**
 * @brief Compare score of detected objects in descending order.
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
  gfloat x1 = fmax ((gfloat)a->x, (gfloat)b->x);
  gfloat y1 = fmax ((gfloat)a->y, (gfloat)b->y);
  gfloat x2 = fmin ((gfloat)(a->x + a->width - 1.0f), (gfloat)(b->x + b->width - 1.0f));
  gfloat y2 = fmin ((gfloat)(a->y + a->height - 1.0f), (gfloat)(b->y + b->height - 1.0f));
  gfloat w  = fmax (0.0f, (x2 - x1 + 1.0f));
  gfloat h  = fmax (0.0f, (y2 - y1 + 1.0f));
  gfloat inter = w * h;
  gfloat areaA = (gfloat)(a->width * a->height);
  gfloat areaB = (gfloat)(b->width * b->height);
  gfloat o = inter / (areaA + areaB - inter);
  return fmax (0.0f, o);
}

/**
 * @brief NMS (non-maximum suppression)
 */
static guint
nms (DetectedObject *detections, guint num_detections)
{
  guint i, j, k, num_overlaps = 0, num_nonoverlaps;
  gboolean del[DETECTION_MAX * LABEL_SIZE];
  qsort(detections, num_detections, sizeof(DetectedObject), compare_detection_scores);
  for (i = 0; i < num_detections; i++) {
    del[i] = FALSE;
  }
  for (i = 0; i < num_detections; i++) {
    if (del[i])
      continue;

    for (j = (i + 1); j < num_detections; j++) {
      if (!del[j] &&
        detections[i].class_id == detections[j].class_id &&
        iou (&detections[i], &detections[j]) > THRESHOLD_IOU
        ) {
        del[j] = TRUE;
        num_overlaps++;
      }
    }
  }
  num_nonoverlaps = num_detections - num_overlaps;
  for (i = 0; i < num_nonoverlaps; i++) {
    if (!del[i])
      continue;

    j = i;
    while(del[i] && j < (num_detections-1)) {
      for(k = i; k < (num_detections-1); k++) {
        del[k] = del[k+1];
        memcpy(&detections[k], &detections[k+1], sizeof(DetectedObject));
      }
      j++;
    }
    num_detections -= (j-i);
  }
  return num_nonoverlaps;
}


/**
 * @brief Get detected objects.
 */
gboolean
get_detected_objects (gfloat box_priors[BOX_SIZE][DETECTION_MAX], const gchar *labels[LABEL_SIZE], const gfloat *predictions, const gfloat *boxes, const GstVideoMeta *vmeta, DetectedObject *detections, guint *num_detections)
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

    gint vwidth = (vmeta)? vmeta->width : MODEL_WIDTH;
    gint vheight = (vmeta)? vmeta->height : MODEL_HEIGHT;
    gint x = xmin * vwidth;
    gint y = ymin * vheight;
    guint width = (xmax - xmin) * vwidth;
    guint height = (ymax - ymin) * vheight;

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
      detections[*num_detections].x = x;
      detections[*num_detections].y = y;
      detections[*num_detections].width = width;
      detections[*num_detections].height = height;
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
