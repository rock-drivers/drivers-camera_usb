#!/bin/bash

if [ $# -ne 1 ]; then
	echo "Will request a single jpeg from the passed device using GStreamer."
	echo "Usage: get_jpeg.sh <video_device>"
fi

filename="test.jpeg"

machine="unknown"
unamestr="`uname -m`"

if [ $unamestr = "i686" ]; then
	#gst-launch-0.10 v4l2src device=$1 num-buffers=1 ! 'video/x-raw-yuv,width=640,height=480,framerate=30/1' ! jpegenc ! filesink location=$filename
	gst-launch-0.10 v4l2src device=$1 ! 'video/x-raw-yuv,width=640,height=480,framerate=30/1' ! xvimagesink
elif [ $unamestr = "armv7l" ]; then
	gst-launch-0.10 v4l2src device=$1 num-buffers=1 ! 'video/x-raw-yuv,width=640,height=480,framerate=30/1' ! dspjpegenc ! filesink location=$filename
fi
      

