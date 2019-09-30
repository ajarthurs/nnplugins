gst-tensor-decode is a plug-in/element for decoding a tensor stream.

This element attaches regions-of-interest (ROIs) using GStreamer's GstMeta API.

## Prerequisites

## Installation

Insert `tensordecode labels=<labels_file> boxpriors=<box_priors_file>`, preferably after NNStreamer's `tensor_filter` element.


## Example

Basic pipeline that detects objects with SSD-MobileNetV1:
```sh
<video source> ! videoconvert ! videoscale ! video/x-raw,width=300,height=300,format=RGB ! tensor_converter ! tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 ! tensor_filter framework=tensorflow-lite model=./tflite_model/ssd_mobilenet_v1_coco.tflite ! tensordecode labels=./tflite_model/coco_labels_list.txt boxpriors=./tflite_model/box_priors-ssd_mobilenet.txt ! appsink name=test sync=false
```
