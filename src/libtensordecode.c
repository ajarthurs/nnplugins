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
#include <gst/gst.h>
#include "gsttensordecode.h"

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

    guint x = xmin * MODEL_WIDTH;
    guint y = ymin * MODEL_HEIGHT;
    guint width = (xmax - xmin) * MODEL_WIDTH;
    guint height = (ymax - ymin) * MODEL_HEIGHT;

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
      detections[*num_detections].prob = score;
      (*num_detections)++;
    }

    predictions += LABEL_SIZE;
    boxes += BOX_SIZE;
  }

  //std::vector<DetectedObject> filtered_vec = nms (detected_vec);
  return TRUE;
}
