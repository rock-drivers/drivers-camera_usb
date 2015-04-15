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

#ifndef _RESTART_TEST_H_
#define _RESTART_TEST_H_

#include "camera_usb/cam_usb.h"



/** Collected errors: 
 * - terminate called after throwing an instance of 'std::length_error' what():  basic_string::_S_create
 * - (<unknown>:4038): GStreamer-CRITICAL **: gst_mini_object_unref: assertion `mini_object->refcount > 0' failed
 * - *** stack smashing detected ***: ./src/tests/unittests/camera-usb-v4l2-test terminated
    ======= Backtrace: =========
    /lib/tls/i686/cmov/libc.so.6(__fortify_fail+0x50)[0x7f72d0]
    /lib/tls/i686/cmov/libc.so.6(+0xe227a)[0x7f727a]
    /opt/software_ruby1.8/install/lib/libbase.so(+0x10bc4)[0x3ddbc4]
    /opt/software_ruby1.8/install/lib/libbase.so(+0x6229)[0x3d3229]
    /lib/ld-linux.so.2(+0x8a26)[0xac6a26]
    /lib/ld-linux.so.2(+0x8fd6)[0xac6fd6]
    /lib/ld-linux.so.2(+0x9252)[0xac7252]
    /lib/tls/i686/cmov/libpthread.so.0(+0x9319)[0x3b6319]
    [0xbfaaf548]
 * - [20120607-17:38:42:893] [DEBUG] - camera_usb::CamGst: readFileDescriptor (/opt/software_ruby1.8/drivers/camera_usb/src/cam_gst.cpp:359 - 
 *   bool camera::CamGst::readFileDescriptor())  Program received signal SIGSEGV, Segmentation fault.
 */

BOOST_AUTO_TEST_CASE(restart_test) {
    base::samples::frame::Frame frame;
    
    std::cout << "RESTART TESTS" << std::endl; 

    std::cout << "CamUsb constructor" << std::endl;
    camera::CamUsb usb("/dev/video0");
    std::vector<camera::CamInfo> cam_infos;

    BOOST_CHECK(usb.listCameras(cam_infos) == 1);

    std::cout << "Open camera" << std::endl;    
    BOOST_CHECK(usb.open(cam_infos[0]) == true); // Allows configuration.

    std::cout << "Change frame settings to 640,480,MODE_JPEG, 3" << std::endl;
    base::samples::frame::frame_size_t size(640,480);
    BOOST_CHECK(usb.setFrameSettings(size, base::samples::frame::MODE_JPEG, 3));
     
    for(int i=0; i < 10; i++) {   
        std::cout << "START GRABBING " << i << std::endl;
        BOOST_CHECK(usb.grab(camera::SingleFrame) == true);
        std::cout << "RETRIEVE FRAME " << i << std::endl;
        BOOST_CHECK(usb.retrieveFrame(frame)); 
        std::cout << "STOP GRABBING " << i << std::endl;
        BOOST_CHECK(usb.grab(camera::Stop) == true);
        std::cout << std::endl;
    }
}



#endif
