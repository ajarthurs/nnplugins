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
#include <algorithm>
#include <cstring>
#include <fstream>
#include <vector>

#include <gst/gst.h>

#include "gsttensordecode.h"

/**
 * @brief Compare score of detected objects.
 */
static bool
compare_objs (DetectedObject &a, DetectedObject &b)
{
  return a.score > b.score;
}

/**
 * @brief Intersection of union
 */
static gfloat
iou (const DetectedObject &A, const DetectedObject &B)
{
  int x1 = std::max (A.x, B.x);
  int y1 = std::max (A.y, B.y);
  int x2 = std::min (A.x + A.width, B.x + B.width);
  int y2 = std::min (A.y + A.height, B.y + B.height);
  int w = std::max (0, (x2 - x1 + 1));
  int h = std::max (0, (y2 - y1 + 1));
  float inter = w * h;
  float areaA = A.width * A.height;
  float areaB = B.width * B.height;
  float o = inter / (areaA + areaB - inter);
  return (o >= 0) ? o : 0;
}

/**
 * @brief NMS (non-maximum suppression)
 */
static std::vector<DetectedObject>
nms (const std::vector<DetectedObject> &detected)
{
  const float threshold_iou = 0.5f;
  std::vector<DetectedObject> sorted (detected);
  guint i, j, num_overlaps = 0;

  std::sort (sorted.begin (), sorted.end (), compare_objs);

  std::vector<bool> del (detected.size(), false);
  for (i = 0; i < detected.size(); i++) {
    if (!del[i]) {
      for (j = i + 1; j < detected.size(); j++) {
        if (iou (sorted.at (i), sorted.at (j)) > threshold_iou) {
          del[j] = true;
          num_overlaps++;
        }
      }
    }
  }
  std::vector<DetectedObject> filtered(detected.size() - num_overlaps);
  for(i = 0, j = 0; i < detected.size(); i++) {
    if (!del[i] && j < filtered.size()) {
      filtered[j] = sorted[i];
      j++;
    }
  }
  return filtered;
}

/**
 * @brief Read strings from file.
 */
gboolean
DEL_read_lines (const gchar *file_name, GList **lines)
{
  GST_DEBUG("HELLO read_lines: %s", file_name);
  std::ifstream file (file_name);
  GST_DEBUG("read_lines loaded: %s", file_name);
  if (!file) {
    GST_ERROR ("Failed to open file %s", file_name);
    return FALSE;
  }

  std::string str;
  while (std::getline (file, str)) {
    *lines = g_list_append (*lines, g_strdup (str.c_str ()));
  }

  return TRUE;
}

/**
 * @brief Load labels.
 */
gboolean
DEL_tflite_load_labels (const gchar *labels_path, const gchar *labels[LABEL_SIZE])
{
  GList *lines = NULL;
  g_return_val_if_fail(read_lines (labels_path, &lines), FALSE);
  for (int i=0; i<LABEL_SIZE; i++) {
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
  gchar *box_row;
  GList *box_priors_lines = NULL;

  GST_DEBUG("HELLO");
  g_return_val_if_fail (read_lines (box_priors_path, &box_priors_lines), FALSE);
  GST_DEBUG("HELLO AGAIN");

  for (int row = 0; row < BOX_SIZE; row++) {
    int column = 0;
    int i = 0, j = 0;
    char buff[11];

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

/**
 * @brief Get detected objects.
 */
gboolean
DEL_get_detected_objects (gfloat box_priors[BOX_SIZE][DETECTION_MAX], const gchar *labels[LABEL_SIZE], const gfloat * predictions, const gfloat * boxes, DetectedObject *detected, guint *num_detected)
{
  const float threshold_score = 0.5f;
  std::vector<DetectedObject> detected_vec;
  const gfloat *predictions_i = predictions, *boxes_i = boxes;

  for (int d = 0; d < DETECTION_MAX; d++) {
    float ycenter =
        ((boxes_i[0] / Y_SCALE) * box_priors[2][d]) +
        box_priors[0][d];
    float xcenter =
        ((boxes_i[1] / X_SCALE) * box_priors[3][d]) +
        box_priors[1][d];
    float h =
        (float) expf (boxes_i[2] / H_SCALE) * box_priors[2][d];
    float w =
        (float) expf (boxes_i[3] / W_SCALE) * box_priors[3][d];

    float ymin = ycenter - h / 2.f;
    float xmin = xcenter - w / 2.f;
    float ymax = ycenter + h / 2.f;
    float xmax = xcenter + w / 2.f;

    int x = xmin * MODEL_WIDTH;
    int y = ymin * MODEL_HEIGHT;
    int width = (xmax - xmin) * MODEL_WIDTH;
    int height = (ymax - ymin) * MODEL_HEIGHT;

    for (int c = 1; c < LABEL_SIZE; c++) {
      gfloat score = EXPIT (predictions_i[c]);
      /**
       * This score cutoff is taken from Tensorflow's demo app.
       * There are quite a lot of nodes to be run to convert it to the useful possibility
       * scores. As a result of that, this cutoff will cause it to lose good detections in
       * some scenarios and generate too much noise in other scenario.
       */
      if (score < threshold_score)
        continue;

      DetectedObject object;

      object.class_id = c;
      object.class_label = labels[c];
      object.x = x;
      object.y = y;
      object.width = width;
      object.height = height;
      object.score = score;

      detected_vec.push_back (object);
    }

    predictions_i += LABEL_SIZE;
    boxes_i += BOX_SIZE;
  }

  std::vector<DetectedObject> filtered_vec = nms (detected_vec);
  //*detected = (DetectedObject *)malloc (detected_vec.size()*sizeof(DetectedObject));
  //if(!(*detected)) {
  //  GST_ERROR("Failed to malloc");
  //  return FALSE;
  //}
  std::copy(filtered_vec.begin(), filtered_vec.end(), detected);
  *num_detected = filtered_vec.size();
  return TRUE;
}
