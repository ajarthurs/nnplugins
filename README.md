gst-tensor-decode is a plug-in/element for decoding a tensor stream.

This element attaches regions-of-interest (ROIs) using GStreamer's GstMeta API.

HOW TO USE IT
-------------

Insert `tensordecode labels=<labels_file> boxpriors=<box_priors_file>`, preferably after NNStreamer's `tensor_filter` element.
