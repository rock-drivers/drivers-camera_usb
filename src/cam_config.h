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

#ifndef _CAM_V4L2_CONFIG_H_
#define _CAM_V4L2_CONFIG_H_

// for v4l2 api (everything required?)
#include <errno.h>
#include <fcntl.h> // for open()
#include <linux/videodev2.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdlib.h>

// e-CAM32 v4l2 controls
#include "omap_v4l2.h"

#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <set>
#include <iostream>

#include <base-logging/Logging.hpp>

#include <base/samples/Frame.hpp>

#include "helpers.h"

namespace camera 
{

/**
 * Exception is thrown if the driver does not support the used command.
 * In most cases this means that a configuration could not be set.
 * Instead, a plain rumtime_error is thrown if communication problems have been occured.
 */
class CamConfigException : public std::runtime_error {
 public:
    CamConfigException(const std::string& what_arg) : std::runtime_error(what_arg) {}
};

/**
 * Using v4l2 to read and set the parameters of the specified camera and to read camera
 * images as well.
 * Important: For newly-connected cameras the device driver may return a wrong image size!
 * Prevent that by calling writeImagePixelFormat() without any parameters during initialization.
 */
class CamConfig
{
 public: // STRUCTURES
    /**
     * Contains all control values of the camera.
     */
    struct CamCtrl {
     public:
        CamCtrl() : mCtrl(), mMenuItems(), mValue(0), 
                mWriteable(true), mReadable(true) {
            memset(&mCtrl, 0, sizeof(struct v4l2_queryctrl));
        }        
        
        ~CamCtrl() {
        }

        struct v4l2_queryctrl mCtrl;
        std::vector<std::string> mMenuItems;
        int32_t mValue;
        bool mWriteable; // Above values are set to zero.
        bool mReadable;
    }; 

 public: // CAMCONFIG
    /**
     * Opens the device and reads all camera informations.
     * Throws std::runtime_error if the device could not be opened
     * or an io-error occurred.
     * TODO Remove read...() from constructor, so its up to the user to read the 
     * required informations?
     */
    CamConfig(std::string const& device);

    ~CamConfig();

     inline int getFd() {
        return mFd;
     }

 public: // CAPABILITY
    void readCapability();

    void listCapabilities();

    std::string getCapabilityDriver();

    std::string getCapabilityCard();

    std::string getCapabilityBusInfo();

    std::string getCapabilityVersion();

    /** 
     * \param field E.g. V4L2_CAP_VIDEO_CAPTURE or V4L2_CAP_VIDEO_OUTPUT.
     * See include/linux/videodev2.h for details.
     */
    bool hasCapability(uint32_t capability_field);

 public: // CONTROL
    /**
     * Generates a list of valid controls requesting all base and private base controls ids.
     * The private base control ids of the e-CAM32 are requested as well (see
     * omap_v4l2.h of the e-CAM32 driver source).
     * readControl(struct v4l2_queryctrl& queryctrl_tmp) doing the main job here.
     */
    void readControl();

    /**
     * Stores the supported control ids and their current values. 
     */
    void readControl(struct v4l2_queryctrl& queryctrl_tmp);

    /**
     * Requests current value from the camera directly.
     * \param id control id like V4L2_CID_BRIGHTNESS or V4L2_CID_CONTRAST, see
     * include/linux/videodev2.h for details.
     * Throws runtime_error if the value could not be requested or the passed
     * id is unknown.
     * \return The current value of the control if available.
     */
    int32_t readControlValue(uint32_t const id);

    /** 
     * Changes the control value on the camera and in the internally stored controls.
     * The passed id and value have to be valid, otherwise a runtime_error is thrown.
     * V4L2_CID_WHITE_BALANCE_TEMPERATURE can not be set (at least on the test camera
     * Microsoft LifeCam Cinema(TM), therefore this parameter is ignored at the moment. 
     * \param just_write If set to true, the passed control value will just be written
     * (without any checks and without storing the new value in 'mCamCtrls').
     * This is used to test whether the control is writeable.
     */
    void writeControlValue(uint32_t const id, int32_t value, bool just_write=false);

    /**
     * Returns list of valid control IDs.
     */
    std::vector<uint32_t> getControlValidIDs();

    /**
     * Returns a copy of all available controls.
     */
    std::vector<struct CamConfig::CamCtrl> getControlList();

    void listControls();

    /**
     * Returns whether this v4l control ID is available on this device
     */
    bool isControlIdValid(uint32_t const id) const;

    /**
     * Returns whether this v4l control ID is available on this device, and
     * whether it can be written to
     */
    bool isControlIdWritable(uint32_t const id) const;

    /**
     * Gets the last set control value, use 'readControl()' if you 
     * want to get the current value set on the camera.
     */
    bool getControlValue(uint32_t const id, int32_t* value);

    bool getControlType(uint32_t const id, uint32_t* type);

    bool getControlName(uint32_t const id, std::string* name);

    bool getControlMinimum(uint32_t const id, int32_t* minimum);

    bool getControlMaximum(uint32_t const id, int32_t* maximum);

    bool getControlStep(uint32_t const id, int32_t* step);

    bool getControlDefaultValue(uint32_t const id, int32_t* default_value);

    /**
     * Checks whether the passed control flag is set within the control.
     * Valid flags are: V4L2_CTRL_FLAG_DISABLED, V4L2_CTRL_FLAG_GRABBED,
     * V4L2_CTRL_FLAG_READ_ONLY, V4L2_CTRL_FLAG_UPDATE, V4L2_CTRL_FLAG_INACTIVE, 
     * V4L2_CTRL_FLAG_SLIDER and V4L2_CTRL_FLAG_WRITE_ONLY.
     * \return false if the passed id or flag is unkown. 
     */
    bool getControlFlag(uint32_t const id, uint32_t const flag, bool* set);

    /**
     * Sets all control (except V4L2_CID_WHITE_BALANCE_TEMPERATURE) values 
     * to their default value.
     */
    void setControlValuesToDefault();

 public: // IMAGE
    void readImageFormat();

    /**
     * Tries to write the requested image size and the pixelformat.
     * Driver will set the most suitable and stores it in mFormat.
     * If you do not want to change a parameter, just pass 0.
     * Requires type V4L2_BUF_TYPE_VIDEO_CAPTURE.
     * \pixelformat E.g. V4L2_PIX_FMT_YUYV, V4L2_PIX_FMT_JPEG, V4L2_PIX_FMT_MJPEG.
     * Pass 0 to use the current pixel format which is set on the camera.
     * See include/linux/videodev2.h for details. 
     */
    void writeImagePixelFormat(uint32_t const width=0, uint32_t const height=0, 
            uint32_t const pixelformat=0);

    void listImageFormat();

    bool getImageWidth(uint32_t* width);

    bool getImageHeight(uint32_t* height);

    bool getImagePixelformat(uint32_t* pixelformat);

    bool getImagePixelformatString(std::string* pixelformat_str);

    /**
     * Whats this?
     */ 
    bool getImageField(uint32_t* field);

    /**
     * For padding, zero if unused.
     */
    bool getImageBytesperline(uint32_t* bytesperline);

    bool getImageSizeimage(uint32_t* sizeimage);

    bool getImageColorspace(uint32_t* colorspace);
    
    /**
     * Tries to map a rock to a v4l2 mode. Problem is that rock 
     * only supports a few image formats and e.g. no YUYV which is a base raw format
     * for most of the cameras. So if RGB is requested but not available
     * YUV will be used and converted to RGB.
     */
    uint32_t toV4L2ImageFormat(base::samples::frame::frame_mode_t mode);

 public: // STREAMPARM, not suoported by e-CAM32!
    void readStreamparm();

    /**
     * Numerator/Denominator, e.g. 1/30.
     * Be aware: Changes to the FPS get lost after closing the device.
     */
    void writeStreamparm(uint32_t const numerator, uint32_t const denominator);

    void listStreamparm();

    /**
     * 
     */
    bool readFPS(uint32_t* fps);

    /**
     * Tries to set the passed fps, just use writeStreamparm(1,fps).
     * Throws std::runtime_error if an io-error occurred.
     */
    void writeFPS(uint32_t const fps);

    /**
     * Returns denominator / numerator.
     * If you want to read the current value from the camera use
     * 'readFPS()', but be careful that the device is not busy.
     * \param fps Sets fps to 0 if not set.
     * \return Always true.
     */
    bool getFPS(uint32_t* fps);

    /** 
     * Only supported flag: V4L2_CAP_TIMEPERFRAME (is it allowd
     * to change the fps).
     */
    bool hasCapabilityStreamparm(uint32_t capability_field);

    /** 
     * Only supported flag: V4L2_MODE_HIGHQUALITY.
     * Used to get single images in maximal quality.
     */
    bool hasCapturemodeStreamparm(uint32_t capturemode);
    
 public: // REQUEST IMAGE
     
    void initRequesting();
    
    /**
     * Uses select() to check if an image is available.
     */
    bool isImageAvailable(int32_t timeout_ms);
    
    /**
     * Used http://www.jayrambhia.com/blog/capture-v4l2
     * \param blocking_read Not used, function always waits timeout_ms milliseconds.
     */
    bool getBuffer(std::vector<uint8_t>& buffer, bool blocking_read, int32_t timeout_ms);
    
    void cleanupRequesting();

 private:
    int mFd; /// File-descriptor to the camera.

    // Capability
    struct v4l2_capability mCapability;
    // Control
    std::map<uint32_t, struct CamCtrl> mCamCtrls;
    // Image
    struct v4l2_format mFormat;
    struct v4l2_cropcap mCropcap;
    std::vector<struct v4l2_fmtdesc> mFormatDescriptions;
    // Stream
    struct v4l2_streamparm mStreamparm;
    // Used to collect all controls depending on another control.
    // E.g. V4L2_CID_FOCUS_ABSOLUTE and V4L2_CID_FOCUS_RELATIVE can only be changed
    // if V4L2_CID_FOCUS_AUTO is set to 0 (manual).
    std::set<uint32_t> mAutoManualDependentControlIds;
    // Points to the image which has been requested via ioctl.
    uint8_t* mmapBuffer;
    bool mStreamingActivated;
    bool mConversionRequiredYUYV2RGB; // YUVU is not yet supported by Rock.

    CamConfig() {}
    
    /**
     * Used in the request image functions.
     */
    void getQueryBuffer(struct v4l2_buffer& query_buffer);
    
    /**
     * ioctl calls could be interrupted (EINTR), in this case another call is required.
     */
    int xioctl(int fd, int request, void *arg);

};

} // end namespace camera

#endif
