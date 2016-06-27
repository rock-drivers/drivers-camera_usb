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

#include <glib.h>
#include <gst/app/gstappsink.h>
#include <gst/gst.h>

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>

#include <iostream>

#include "cam_config.h"
#include <base/samples/Frame.hpp>

namespace camera 
{

class CamGstException : public std::runtime_error {
 public:
    CamGstException(const std::string& what_arg) : std::runtime_error(what_arg) {}
};

/**
 * Allows to create a default GStreamer pipeline, which requests images using v4l2src,
 * converts them to jpegs and gives access to the image data.
 * Successfully tested with the Microsoft LifeCam Cinema Web camera and 
 * the Gumstix e-CAM32 camera. 
 */
class CamGst {

 public: // CONSTANTS
    static const uint32_t DEFAULT_WIDTH = 640;
    static const uint32_t DEFAULT_HEIGHT = 480;
    static const uint32_t DEFAULT_FPS = 10;
    static const uint32_t DEFAULT_BPP = 24;
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
     * Creates a simple pipeline to write images with the requested format to a buffer.
     * \param check_for_valid_params If set to true the parameters (width, height, fps) are validated.
     * If they are not valid on the camera valid parameters are used. In addition if you 
     * pass 0 for a parameter, the last valid parameter on the camera will be used.  
     * E.g. 'createDefaultPipeline(true,0,0,0,80)' would just change the JPEG quality.
     * But for this functionality a CamConfig object has to be created, which uses 
     * the same device like GStreamer what should be avoided.
     * In addition using this method within the CamUsb driver is not necessary because
     * all parameters have been validated and set already.\n
     * If set to false the pipeline may not be created if the parameters are not supported by
     * the camera (a CamGstException may be thrown).
     * \param mode Valid modes: MODE_GRAYSCALE, MODE_RGB, MODE_UYVY, MODE_JPEG. 
     * If MODE_UNDEFINED is passed the raw image video/x-raw-yuv will be requested.
     * \return If an error occurs a CamGstException is thrown.
     */
    void createDefaultPipeline(bool check_for_valid_params=false,
            uint32_t width = DEFAULT_WIDTH, 
            uint32_t height = DEFAULT_HEIGHT, 
            uint32_t fps = DEFAULT_FPS,
            uint32_t bpp = DEFAULT_BPP,
            base::samples::frame::frame_mode_t mode = base::samples::frame::MODE_UNDEFINED,
            uint32_t jpeg_quality = DEFAULT_JPEG_QUALITY);

    /**
     * Deletes pipeline, clears buffer.
     */
    void deletePipeline();

    /**
     * Starts current pipeline in another a intern GStreamer thread.
     * Pipeline has to be already created. Restarting may not work, in this case
     * an error message will be printed and false will be returned.
     * \warning Just stopping and starting a pipeline may not work.
     * You should delete and recreate the pipeline instead!
     * \return True if its already running or could be started.
     */
    bool startPipeline();

    /**
     * \warning Just stopping and starting a pipeline may not work.
     * You should delete and recreate the pipeline instead!
     */
    void stopPipeline();

    /**
     * Allows to request a copy of the new image.
     * \param buffer Will receive the image if available.
     * \param blocking_read If true, method will return as soon as a new image is available. 
     * \param timeout Max. time to wait for the frame in msec. < 1 means no timeout.
     * \return blocking-read not active: true if a new image is available, otherwise false. \n
     * blocking_read active: Returns true as soon as a new image is available or false 
     * after 'timeout' msec.
     */
    bool getBuffer(std::vector<uint8_t>& buffer, 
            bool blocking_read=false, int32_t timeout=0);

    /**
     * Just sets mNewBuffer to false.
     * \return True if a new buffer was available.
     */
    bool skipBuffer();

    /**
     * Stores the image to a file.
     * \return false if the file could not be opened or not all of the bytes could be written.
     */
    bool storeImageToFile(std::vector<uint8_t> const& buffer, 
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
    void setCameraParameters(uint32_t* width, uint32_t* height, uint32_t* fps);

    GstElement* createDefaultSource(std::string const& device);

    GstElement* createDefaultCap(uint32_t const width, uint32_t const height, uint32_t const fps, uint32_t bpp, base::samples::frame::frame_mode_t mode );

    /**
     * Currently not used anymore, instead the encoding of the camera base class is used.
     */
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
     * Calls the method 'callbackMessages()' of the passed (using 'gpointer data') CamGst object.
     */
    static gboolean callbackMessagesStatic(GstBus* bus, GstMessage* msg, gpointer data);

    /**
     * Reports GST_MESSAGE_EOS and GST_MESSAGE_ERROR messages.
     */
    gboolean callbackMessages(GstBus* bus, GstMessage* msg, gpointer data);

    /**
     * Calls the method 'callbackNewBuffer()' of the passed CamGst object.
     */
    static void callbackNewBufferStatic(GstElement* object, CamGst* cam_gst_p); 
    
    /**
     * Copies the received image to 'mBuffer'.
     */
    void callbackNewBuffer(GstElement* object, CamGst* cam_gst_p); 

    /**
     * Print element factories for debugging purposes
     */
    void printElementFactories();

 private:
    std::string mDevice;
    uint32_t mJpegQuality;
    GMainLoop* mLoop;
    pthread_t* mMainLoopThread;
    GstElement* mPipeline;
    GstBus* mGstPipelineBus;
    bool mPipelineRunning;

    pthread_mutex_t mMutexBuffer;
    GstBuffer* mBuffer;
    uint32_t mBufferSize;
    bool mNewBuffer;

    GstElement* mSource; // Used to request the fd.
    int mFileDescriptor; // File descriptor of the pipeline source. -1 if not available.
    
    base::samples::frame::frame_mode_t mRequestedFrameMode;

    /**
     * Using GstGuard to making sure that GStreamer is initialized/deinitialized only once, i.e. use
     * static InitGuard gInit;
     */
    class InitGuard {
    public:
        InitGuard();
        ~InitGuard();
    };

};

} // end namespace camera

#endif
