/*
 * \file    usb_test.h
 *  
 * \brief   Boost tests for class CamUsb.   
 * 
 *          German Research Center for Artificial Intelligence\n
 *          Project: Rimres
 *
 * \date    23.11.11
 *
 * \author  Stefan.Haase@dfki.de
 */

#ifndef _USB_TEST_H_
#define _USB_TEST_H_

#include "camera_usb/cam_usb.h"

BOOST_AUTO_TEST_SUITE(usb_test_suite)

camera::CamUsb usb("/dev/video0");
std::vector<camera::CamInfo> cam_infos;

BOOST_AUTO_TEST_CASE(init_test) {
    std::cout << "INIT TESTS" << std::endl; 
    BOOST_CHECK(usb.listCameras(cam_infos) == 1);
    BOOST_CHECK(usb.listCameras(cam_infos) == 0);
    
    BOOST_CHECK(usb.isOpen() == false);
    BOOST_CHECK(usb.getCameraInfo() == NULL);
    usb.close();

    std::cout << "Open camera" << std::endl;
    BOOST_REQUIRE_NO_THROW(usb.open(cam_infos[0]));
    BOOST_CHECK(usb.isOpen() == true);
    BOOST_CHECK(usb.getCameraInfo() != NULL);

    usb.close();
}

BOOST_AUTO_TEST_CASE(buffer_test) {
    std::cout << "BUFFER TESTS" << std::endl;
    std::cout << "Close camera and change frame settings to 640,640,MODE_JPEG, 3" << std::endl;
    usb.close();
    base::samples::frame::frame_size_t size(640,640);
    BOOST_CHECK(usb.setFrameSettings(size, base::samples::frame::MODE_JPEG, 3));

    BOOST_CHECK(usb.grab() == false);
    BOOST_CHECK(usb.isFrameAvailable() == false);

    std::cout << "Open and start grabbing" << std::endl;
    BOOST_CHECK(usb.open(cam_infos[0]) == true);
    BOOST_CHECK(usb.grab(camera::Continuously) == true);
    sleep(1);
    std::cout << "Get image and check size and mode" << std::endl;
    BOOST_CHECK(usb.isFrameAvailable() == true);
    base::samples::frame::Frame frame;
    BOOST_CHECK(usb.retrieveFrame(frame,1000) == true);
    BOOST_CHECK(frame.getWidth() == 640);
    BOOST_CHECK(frame.getHeight() == 480);
    BOOST_CHECK(frame.getFrameMode() == base::samples::frame::MODE_JPEG);
    usb.close();
    
    std::cout << "Change size to 1280, 720 and request 100 images" << std::endl;
    size.width = 1280;
    size.height = 720;
    BOOST_CHECK(usb.setFrameSettings(size, base::samples::frame::MODE_JPEG, 3));
    BOOST_CHECK(usb.retrieveFrame(frame,1000) == false);
    BOOST_CHECK(usb.open(cam_infos[0]) == true);
    BOOST_CHECK(usb.grab(camera::Continuously) == true);
    sleep(1);
    for(int i=0; i<100; i++)
        BOOST_CHECK(usb.retrieveFrame(frame,1000) == true);
    sleep(1);
    BOOST_CHECK(usb.skipFrames() == 1);    
    BOOST_CHECK(frame.getWidth() == 1280);
    BOOST_CHECK(frame.getHeight() == 720);
    
    base::samples::frame::frame_mode_t mode;
    uint8_t color_depth;
    BOOST_CHECK(usb.getFrameSettings(size, mode, color_depth));
    std::cout << "Width: " << size.width << " Height: " << size.height << " Mode: " << 
            mode << " Color Depth: " << (int)color_depth << std::endl;
}

BOOST_AUTO_TEST_CASE(attribute_test) {
    std::cout << "ATTRIBUTE TESTS" << std::endl;
    std::cout << "Set INT attributes" << std::endl;
    int val = 0;
    bool available = false;
    usb.close();
    // INTs
    BOOST_CHECK((available = usb.isAttribAvail(camera::int_attrib::BrightnessValue)) == true);
    if(available) {
        BOOST_REQUIRE_NO_THROW(val = usb.getAttrib(camera::int_attrib::BrightnessValue));
        std::cout << "BrightnessValue: " << val << std::endl;
        BOOST_REQUIRE_NO_THROW(usb.setAttrib(camera::int_attrib::BrightnessValue, val));
    }
    BOOST_CHECK((available = usb.isAttribAvail(camera::int_attrib::IrisAutoTarget)) == false);
    if(!available) {
        BOOST_REQUIRE_THROW(val = usb.getAttrib(camera::int_attrib::IrisAutoTarget), std::runtime_error);
        BOOST_REQUIRE_THROW(usb.setAttrib(camera::int_attrib::IrisAutoTarget, val), std::runtime_error);
    }

    std::cout << "Set DOUBLE attributes" << std::endl;
    // DOUBLEs
    BOOST_CHECK((available = usb.isAttribAvail(camera::double_attrib::FrameRate)) == true);
    if(available) {
        BOOST_REQUIRE_NO_THROW(val = usb.getAttrib(camera::double_attrib::FrameRate));
        try{
            usb.setAttrib(camera::double_attrib::FrameRate, val);
        } catch(std::runtime_error& e) {
            std::cout << "ERROR " << e.what() << std::endl;
        }
        BOOST_REQUIRE_NO_THROW(usb.setAttrib(camera::double_attrib::FrameRate, val));
        std::cout << "FrameRate: " << val << std::endl;
    }

    std::cout << "Set ENUM attributes" << std::endl;
    // ENUMs
    BOOST_CHECK((available = usb.isAttribAvail(camera::enum_attrib::WhitebalModeToAuto)) == true);
    if(available) {
        BOOST_REQUIRE_NO_THROW(usb.setAttrib(camera::enum_attrib::WhitebalModeToAuto));
    }
    BOOST_CHECK((available = usb.isAttribAvail(camera::enum_attrib::GainModeToManual)) == false);
    if(!available) {
        BOOST_REQUIRE_THROW(usb.setAttrib(camera::enum_attrib::GainModeToManual), std::runtime_error);
    }
    std::cout << "Set attributes to default " << std::endl;
    BOOST_REQUIRE_NO_THROW(usb.setToDefault());
}

BOOST_AUTO_TEST_CASE(other_test) {
    std::cout << "OTHER TESTS" << std::endl;
    std::cout << "Get range" << std::endl; 
    usb.close();
    int min=0, max=0;
    BOOST_REQUIRE_NO_THROW(usb.getRange(camera::int_attrib::BrightnessValue, min, max));
    std::cout << "Brightness Min: " << min << ", Max: " << max << std::endl; 
    BOOST_REQUIRE_THROW(usb.getFileDescriptor(),std::runtime_error);
}

BOOST_AUTO_TEST_SUITE_END()

#endif
