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
// TODO Make singleton?
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
     * Starts current pipeline in another thread.
     */
    bool startPipeline();

    /**
     * Deletes pipeline, clears buffer.
     */
    void deletePipeline();

    /**
     * Allows to request a copy of the new image.
     * \param buffer Will be set to the new image or NULL if no new image is available.
     * Take care to free the returned image!
     * \param buf_size Will get the image size or 0 if no image is available.
     * \param blocking_read If true, method will return as soon as a new image is available. 
     * \param timeout Max. time to wait for the frame in msec. < 1 means no timeout.
     * \return true if a new image is available, otherwise false. Always returns true if 
     * blocking_read is set to true. Return false after 
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

    inline uint32_t getWidth() {
        return mWidth;
    }

    inline uint32_t getHeight() {
        return mHeight;
    }

    inline uint32_t getFps() {
        return mFps;
    }

 private:
    CamGst();

    /**
     * Use CamConfig to find and set valid camera parameters
     * (driver will choose most suitable).
     * Sets the member variables as well.
     */
    void setCameraParameters(std::string const& device, 
            uint32_t* width, uint32_t* height, uint32_t* fps);

    GstElement* createDefaultSource(std::string const& device);

    GstElement* createDefaultCap(uint32_t const width, uint32_t const height, uint32_t const fps );

    GstElement* createDefaultEncoder(int32_t const jpeg_quality);

    GstElement* createDefaultSink();

 private: // STATIC METHODS
    static void* mainLoop(void* ptr);

    /**
     * Deletes pipeline if its end has been reached or an error occurred.
     */
    static gboolean callbackMessages (GstBus* bus, GstMessage* msg, gpointer data);

    static void callbackNewBuffer(GstElement* object, CamGst* cam_gst_p); 

 private:
    std::string mDevice;
    uint32_t mWidth, mHeight, mFps;
    uint32_t mJpegQuality;
    GMainLoop* mLoop;
    pthread_t* mMainLoopThread;
    GstElement* mPipeline;
    bool mPipelineRunning;
    static GstBuffer* mBuffer;
    static uint32_t mBufferSize;
    static pthread_mutex_t mMutexBuffer;
    static bool mNewBuffer;
};

} // end namespace camera

#endif
