#include <iostream>
#include <camera_usb/cam_config.h>
#include <camera_usb/cam_gst.h>

int main(int argc, char** argv)
{
	camera::CamConfig cam_config("/dev/video0");
    camera::CamGst cam_gst("/dev/video0");
	return 0;
}
