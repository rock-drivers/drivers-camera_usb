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

BOOST_AUTO_TEST_SUITE(gst_test_suite)

BOOST_AUTO_TEST_CASE(default_pipeline_test) {
    camera::CamGst gst("/dev/video0");
    camera::CamConfig config("/dev/video0");
    config.writeControlValue(V4L2_CID_BRIGHTNESS, 100);
    BOOST_CHECK(gst.startPipeline() == false);
    std::cout << "Create pipeline 640,480,20" << std::endl;
    BOOST_REQUIRE_NO_THROW(gst.createDefaultPipeline(640, 480, 20));
    std::cout << "Start pipeline twice" << std::endl;
    BOOST_CHECK(gst.startPipeline() == true);
    BOOST_CHECK(gst.startPipeline() == false);
    std::cout << "Delete pipeline twice and try to start pipeline" << std::endl;
    gst.deletePipeline();
    gst.deletePipeline();
    BOOST_CHECK(gst.startPipeline() == false);
    std::cout << "Create pipeline with invalid parameters 640,640,40" << std::endl;
    BOOST_REQUIRE_NO_THROW(gst.createDefaultPipeline(640, 640, 40));
    std::cout << "Start pipeline, wait four sec., delete pipeline" << std::endl; 
    BOOST_CHECK(gst.startPipeline() == true);
    sleep(4);
    gst.deletePipeline();
}

BOOST_AUTO_TEST_CASE(request_image_test) {
    camera::CamGst gst("/dev/video0");
    uint32_t buf_size = 0;
    uint8_t* buffer = NULL;
    BOOST_REQUIRE_NO_THROW(gst.createDefaultPipeline(1024, 768, 30));
    BOOST_CHECK(gst.startPipeline() == true);
    char name_buffer[128];

    int img_received = 0;
    // Non-blocking read.
    for(int i=0; i<10000; i++){
        bool new_buf = gst.getBuffer(&buffer, &buf_size, false);
        if(new_buf) {
            BOOST_CHECK(buffer != NULL);
            BOOST_CHECK(buf_size > 0);
            snprintf(name_buffer, 128, "test.jpeg");
            gst.storeImageToFile(buffer, buf_size, name_buffer);
            free(buffer);
            ++img_received;
        }
        usleep(10);
    }
    std::cout << "Non-blocking Read, images received (10000 cycles): " << img_received << std::endl;

    img_received = 0;
    // Blocking read.
    for(int i=0; i<10; i++){
        bool new_buf = gst.getBuffer(&buffer, &buf_size, true, 1000);
        if(new_buf) {
            BOOST_CHECK(buffer != NULL);
            BOOST_CHECK(buf_size > 0);
            snprintf(name_buffer, 128, "test.jpeg");
            gst.storeImageToFile(buffer, buf_size, name_buffer);
            free(buffer);
            ++img_received;
        }
        usleep(10);
    }
    std::cout << "Blocking Read, images received (10 cycles): " << img_received << std::endl;
}

BOOST_AUTO_TEST_SUITE_END()

#endif
