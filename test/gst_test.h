/*
 * \file    gst_test.h 
 *  
 * \brief   Boot tests for class CamGst.
 *
 *          German Research Center for Artificial Intelligence\n
 *          Project: Rimres
 *
 * \date    23.11.11
 *
 * \author  Stefan.Haase@dfki.de
 */

#ifndef _GST_TEST_H_
#define _GST_TEST_H_

#include <stdio.h>

extern "C" {
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
}

#include "camera_usb/cam_gst.h"

BOOST_AUTO_TEST_CASE(default_pipeline_test) {
    camera::CamGst gst("/dev/video0");
    BOOST_CHECK(gst.startPipeline() == false);
    std::cout << "Create default pipeline" << std::endl;
    try {
        gst.createDefaultPipeline(true, 0, 0, 0, 
                camera::CamGst::DEFAULT_BPP, 
                base::samples::frame::MODE_UNDEFINED, 
                camera::CamGst::DEFAULT_JPEG_QUALITY);
    } catch (std::runtime_error &e) {
        BOOST_ERROR(e.what());
    }
    std::cout << "Start pipeline twice" << std::endl;
    BOOST_CHECK(gst.startPipeline() == true);
    BOOST_CHECK(gst.startPipeline() == true);
    std::cout << "Delete pipeline twice and try to start pipeline" << std::endl;
    gst.deletePipeline();
    gst.deletePipeline();
    BOOST_CHECK(gst.startPipeline() == false);
    
    std::cout << "Create pipeline with invalid parameters" << std::endl;
    try {
        gst.createDefaultPipeline(false, 6400, 0, 4000);
        // We except a runtime error, so we do not want to be here.
        BOOST_ERROR("No exception has been thrown even invalid parameters has been used");
    } catch (std::runtime_error& e) {
        std::cout << "Runtime error caught: " << e.what() << std::endl;
    }

    std::cout << "Start pipeline, wait 2 sec., delete pipeline" << std::endl; 
    BOOST_CHECK(gst.startPipeline() == false);
    sleep(2);
    gst.deletePipeline();
    
}

BOOST_AUTO_TEST_CASE(request_image_test) {
    camera::CamGst gst("/dev/video0");
    std::vector<uint8_t> buffer;
    int num_requests = 10;
    
    std::cout << "Create default pipeline" << std::endl;
    try {
        gst.createDefaultPipeline(true,
                0, 0, 0, 
                camera::CamGst::DEFAULT_BPP, 
                base::samples::frame::MODE_UNDEFINED, 
                camera::CamGst::DEFAULT_JPEG_QUALITY);
    } catch (std::runtime_error &e) {
        BOOST_ERROR(e.what());
    }
    
    std::cout << "Start pipeline" << std::endl;
    BOOST_CHECK(gst.startPipeline() == true);
    char name_buffer[128];

    int img_received = 0;
    // Non-blocking read.
    for(int i=0; i<num_requests; i++){
        bool new_buf = gst.getBuffer(buffer, false);
        if(new_buf) {
            BOOST_CHECK(buffer.size() > 0);
            snprintf(name_buffer, 128, "test.jpeg");
            gst.storeImageToFile(buffer, name_buffer);
            ++img_received;
        }
        usleep(10);
    }
    std::cout << "Non-blocking Read, images received (" << num_requests << " cycles): " << 
            img_received << std::endl;

    img_received = 0;
    // Blocking read.
    for(int i=0; i<num_requests; i++){
        bool new_buf = gst.getBuffer(buffer, true, 1000);
        if(new_buf) {
            BOOST_CHECK(buffer.size() > 0);
            snprintf(name_buffer, 128, "test.jpeg");
            gst.storeImageToFile(buffer, name_buffer);
            ++img_received;
        }
        usleep(10);
    }
    std::cout << "Non-blocking Read, images received (" << num_requests << " cycles): " << 
            img_received << std::endl;
}

#endif
