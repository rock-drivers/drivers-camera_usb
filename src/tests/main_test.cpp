#include "camera_usb/cam_usb.h"

int main(int argc, char* argv[]) 
{
    using namespace camera;

    CamUsb usb("/dev/video0");
    std::vector<camera::CamInfo> cam_infos;

    usb.listCameras(cam_infos);
    
    usb.grab();
    usb.isFrameAvailable();

    usb.open(cam_infos[0]);
    usb.grab(camera::Continuously);

    base::samples::frame::frame_size_t size(640,480);
    usb.setFrameSettings(size, base::samples::frame::MODE_PJPG, 4);

    sleep(1);
    usb.isFrameAvailable();
    base::samples::frame::Frame frame;
    usb.retrieveFrame(frame,1000);
    std::cout << "width " << frame.getWidth() << "height " << frame.getHeight() << " mode " << frame.getFrameMode() << std::endl;
    usb.close();
    
}

