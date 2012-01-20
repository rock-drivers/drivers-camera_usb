/*
 * \file    cam_gst.h
 *  
 * \brief   Allows to create a default GStreamer pipeline, which requests images using v4l2, 
 *          converts them to jpegs and gives access to the image data. 
 *          
 *          German Research Center for Artificial Intelligence\n
 *          Project: Rimres
 *
 * \date    23.11.11
 *
 * \author  Stefan.Haase@dfki.de
 */

#ifndef _CAM_GST_CONFIG_H_
#define _CAM_GST_CONFIG_H_

extern "C" {
#include <glib.h>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>
}

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <iostream>

#include "cam_config.h"

namespace camera 
{

class CamGstException : public std::runtime_error {
 public:
    CamGstException(const std::string& what_arg) : std::runtime_error(what_arg) {}
};

/**
 * Allows to create a default GStreamer pipeline, which requests images using v4l2,
 * converts them to jpegs and gives access to the image data.
 * Successfully tested with the Microsoft LifeCam Cinema Web camera and 
 * the Gumstix e-CAM32 camera. 
 * Still problem with calling gst_init() several times?
 * E.g. its not possible to create one object of CamGst, remove it,
 * create another, try to create a default source -> not possible. Why?
 */
class CamGst {

 public: // CONSTANTS
    static const uint32_t DEFAULT_WIDTH = 640;
    static const uint32_t DEFAULT_HEIGHT = 480;
    static const uint32_t DEFAULT_FPS = 10;
    static const uint32_t DEFAULT_JPEG_QUALITY = 85; // 0 to 100
    static const uint32_t DEFAULT_PIPELINE_TIMEOUT = 4000000; // 4 sec.

 public:
    /**
     * Initialize GStreamer and starts the GMainLoop in its own thread.
     * \param device Only used to configure the GStreamer source (e.g. /dev/video0).
     * \param cam_config Pointer to a CamConfig object, used to get a valid image size 
     * and fps and for general configurations.
     */
    CamGst(std::string const& device);

    /**
     * Stops the GMainLoop and its thread.
     */
    ~CamGst();

    /**
     * Creates a simple pipeline to read video/x-raw-yuv I420 images,
     * convert the image to jpeg and write the result to a buffer.
     * dspjpegenc is used if available (Gumstix), otherwise jpegenc.
     * Always use the device passed to the constructor.
     * If you want to use the last set parameters, pass 0.
     * E.g. 'createDefaultPipeline(0,0,0,80)' would just change the 
     * JPEG quality.
     */
    void createDefaultPipeline(uint32_t width = DEFAULT_WIDTH, 
            uint32_t height = DEFAULT_HEIGHT, 
            uint32_t fps = DEFAULT_FPS,
            uint32_t jpeg_quality = DEFAULT_JPEG_QUALITY);

    /**
     * Deletes pipeline, clears buffer.
     */
    void deletePipeline();

    /**
     * Starts current pipeline in another a GStreamer intern thread.
     * Pipeline has to be already created.
     * \return True if its already running or could be started.
     */
    bool startPipeline();

    void stopPipeline();

    /**
     * Allows to request a copy of the new image.
     * \param buffer Will be set to the new image or NULL if no new image is available.
     * Take care to free the returned image!
     * \param buf_size Will get the image size or 0 if no image is available.
     * \param blocking_read If true, method will return as soon as a new image is available. 
     * \param timeout Max. time to wait for the frame in msec. < 1 means no timeout.
     * \return blocking-read not active: true if a new image is available, otherwise false. \n
     * blocking_read active: Returns true as soon as a new image is available or false 
     * after 'timeout' msec.
     */
    bool getBuffer(uint8_t** buffer, uint32_t* buf_size, 
            bool blocking_read=FALSE, int32_t timeout=0);

    /**
     * Just sets mNewBuffer to false.
     * \return True if a new buffer was available.
     */
    bool skipBuffer();

    void storeImageToFile(uint8_t* const buffer, uint32_t const buf_size, 
            std::string const& file_name);

    /**
     * True if a new buffer is available.
     */
    inline bool hasNewBuffer() {
        return mNewBuffer;
    }

    inline bool isPipelineRunning() {
        return mPipelineRunning;
    }

    /**
     * Returns the file descriptor used by GStreamer.
     * The pipeline has to be running, otherwise -1 will be returned.
     * \return The fd or -1 if not available.
     */
    inline int getFileDescriptor() {
        return mFileDescriptor;
    }

 private:
    CamGst();

    /**
     * Each parameter which is 0 will get the last stored value of the camera.
     * \warning Use CamConfig to find and set valid camera parameters
     * (driver will choose most suitable). Can this lead to a blocked device?
     */
    void setCameraParameters(uint32_t* width, uint32_t* height, uint32_t* fps,
            uint32_t* fpeg_quality);

    GstElement* createDefaultSource(std::string const& device);

    GstElement* createDefaultCap(uint32_t const width, uint32_t const height, uint32_t const fps );

    GstElement* createDefaultEncoder(int32_t const jpeg_quality);

    GstElement* createDefaultSink();

    /**
     * Set 'mFileDescriptor' to the fd of the current source (e.g. v4l2).
     * To get a valid fd the pipeline has to be running.
     * \return true if the pipeline is running and the 
     * fd could be requested.
     */
    bool readFileDescriptor();

    inline void rmFileDescriptor() {
        mFileDescriptor = -1;
        std::cout << "FD set back to -1" << std::endl;
    }

 private: // STATIC METHODS
    static void* mainLoop(void* ptr);

    /**
     * Deletes pipeline if its end has been reached or an error occurred.
     */
    static gboolean callbackMessages (GstBus* bus, GstMessage* msg, gpointer data);

    static void callbackNewBuffer(GstElement* object, CamGst* cam_gst_p); 

 private:
    std::string mDevice;
    uint32_t mJpegQuality;
    GMainLoop* mLoop;
    pthread_t* mMainLoopThread;
    GstElement* mPipeline;
    GstBus* mGstPipelineBus;
    bool mPipelineRunning;
    static GstBuffer* mBuffer;
    static uint32_t mBufferSize;
    static pthread_mutex_t mMutexBuffer;
    static bool mNewBuffer;

    GstElement* mSource; // Used to request the fd.
    int mFileDescriptor; // File descriptor of the pipeline source. -1 if not available.
};

} // end namespace camera

#endif
