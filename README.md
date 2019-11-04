gst-tensor-decode is a plug-in/element for decoding a tensor stream.

This element attaches regions-of-interest (ROIs) using GStreamer's GstMeta API.

## Prerequisites

* GStreamer 1.x
* NNStreamer 1.x

## Installation

```sh
meson build
sudo ninja -vC build install
```

Insert `ssddecode name=decoder labels=<labels_file> boxpriors=<box_priors_file>`, preferably after NNStreamer's `tensor_filter` element. Pipe the desired output video stream into `decoder.video_sink`.


## Example

Basic pipeline that detects objects with SSD-MobileNetV1:
```sh
<video source> ! videoconvert ! tee name=t
  t. ! queue ! decoder.video_sink
  t. ! queue ! videoscale ! video/x-raw,width=300,height=300,format=RGB !  tensor_converter !
    tensor_transform mode=arithmetic option=typecast:float32,add:-127.5,div:127.5 !
    tensor_filter framework=tensorflow-lite model=./tflite_model/ssd_mobilenet_v1_coco.tflite !
    ssddecode name=decoder labels=./tflite_model/coco_labels_list.txt boxpriors=./tflite_model/box_priors-ssd_mobilenet.txt !
    appsink name=test sync=false
```
