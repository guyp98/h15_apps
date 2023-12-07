gst-launch-1.0 -v udpsrc port=5002 address=0.0.0.0 ! application/x-rtp,encoding-name=H264 \
! queue ! rtpjitterbuffer mode=0 ! queue ! rtph264depay ! queue ! h264parse \
! avdec_h264 ! queue ! videoconvert ! fpsdisplaysink video-sink=autovideosink text-overlay=false sync=false