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
    BOOST_CHECK(usb.listCameras(cam_infos) == 1);
    BOOST_CHECK(usb.listCameras(cam_infos) == 0);
    
    BOOST_CHECK(usb.isOpen() == false);
    BOOST_CHECK(usb.getCameraInfo() == NULL);
    BOOST_CHECK(usb.close() == false);

    BOOST_CHECK(usb.open(cam_infos[0]) == true);
    BOOST_CHECK(usb.isOpen() == true);
    BOOST_CHECK(usb.getCameraInfo() != NULL);

    BOOST_CHECK(usb.close() == true);
}

BOOST_AUTO_TEST_CASE(buffer_test) {
    BOOST_CHECK(usb.grab() == false);
    BOOST_CHECK(usb.isFrameAvailable() == false);

    // reopen
    BOOST_CHECK(usb.open(cam_infos[0]) == true);
    sleep(1);
    BOOST_CHECK(usb.grab() == true);
    BOOST_CHECK(usb.isFrameAvailable() == true);
    base::samples::frame::Frame frame;
    BOOST_CHECK(usb.retrieveFrame(frame,1000) == true);
    BOOST_CHECK(frame.getWidth() == 640);
    BOOST_CHECK(frame.getHeight() == 480);
    BOOST_CHECK(frame.getFrameMode() == base::samples::frame::MODE_PJPG);
    BOOST_CHECK(usb.close() == true);
    
    base::samples::frame::frame_size_t size(1280, 720);
    BOOST_CHECK(usb.setFrameSettings(size, base::samples::frame::MODE_PJPG, 1));
    BOOST_CHECK(usb.retrieveFrame(frame,1000) == false);
    BOOST_CHECK(usb.open(cam_infos[0]) == true);
    BOOST_CHECK(usb.retrieveFrame(frame,1000) == true);
    BOOST_CHECK(frame.getWidth() == 1280);
    BOOST_CHECK(frame.getHeight() == 720);
    sleep(1);
    BOOST_CHECK(usb.skipFrames() == 1);    
    BOOST_CHECK(usb.triggerFrame() == true);

    base::samples::frame::frame_mode_t mode;
    uint8_t color_depth;
    BOOST_CHECK(usb.getFrameSettings(size, mode, color_depth));
    std::cout << "Width: " << size.width << " Height: " << size.height << " Mode: " << 
            mode << " Color Depth: " << (int)color_depth << std::endl;
}

BOOST_AUTO_TEST_CASE(attribute_test) {
    int val = 0;
    bool available = false;
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

    // ENUMs
    BOOST_CHECK((available = usb.isAttribAvail(camera::enum_attrib::WhitebalModeToAuto)) == true);
    if(available) {
        BOOST_REQUIRE_NO_THROW(usb.setAttrib(camera::enum_attrib::WhitebalModeToAuto));
    }
    BOOST_CHECK((available = usb.isAttribAvail(camera::enum_attrib::GainModeToManual)) == false);
    if(!available) {
        BOOST_REQUIRE_THROW(usb.setAttrib(camera::enum_attrib::GainModeToManual), std::runtime_error);
    }
}

BOOST_AUTO_TEST_CASE(other_test) {
    BOOST_REQUIRE_NO_THROW(usb.setToDefault());
    int min=0, max=0;
    BOOST_REQUIRE_NO_THROW(usb.getRange(camera::int_attrib::BrightnessValue, min, max));
    std::cout << "Brightness Min: " << min << ", Max: " << max << std::endl; 
    BOOST_CHECK(usb.getFileDescriptor() != 0);
}

BOOST_AUTO_TEST_SUITE_END()

#endif
