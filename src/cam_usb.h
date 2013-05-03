/*
 * \file    cam_usb.h
 *  
 * \brief   USB-Camera driver using 4vl2 and GStreamer.
 *
 * \details Based on the Rock-CameraInterface. Can be used
 *          for the Gumstix Camera as well.
 *          
 *          German Research Center for Artificial Intelligence\n
 *          Project: Rimres
 *
 * \date    23.11.11
 *
 * \author  Stefan.Haase@dfki.de
 */

#ifndef _CAM_USB_CONFIG_H_
#define _CAM_USB_CONFIG_H_

#include <sys/time.h>

#include "camera_interface/CamInterface.h"

#include "cam_gst.h"
#include "cam_config.h"

namespace camera 
{

    enum CAM_USB_MODE {
        CAM_USB_NONE,
        CAM_USB_V4L2,
        CAM_USB_GST
    };

    static const std::string ModeTxt[] = { "CAM_USB_NONE", "CAM_USB_V4L2", "CAM_USB_GST" };
/**
 * 
 * Allows configuration and image-requesting of cameras supported by Video4Linux.
 * 1. Pass the video-device (e.g. '/dev/video0') to the constructor.
 * 2. Call 'listCameras()' to get the CamInfo structure of the single supported camera.
 * 3. Opens the camera with 'open()' entering the configuration mode via v4l2.
 * 4. Call setFrameSettings() to define the image size. 
 * 5. (optional) Use 'setAttrib()' to change default attributes of the camera interface and 
 *    'setV4L2Attrib()' to change special private attributes of the camera not defined by the interface.
 * 6. Call 'grab()' to create and start the GStreamer pipeline (switching from configuration mode to
 *    image requesting mode -> for further configuration the pipeline has to be stopped again).
 * 7. Use 'retrieveFrame()' to get a Frame.
 *
 * You can use 'fastInit(width, height)' for the steps 2, 3 and 4.
 */
class CamUsb : public CamInterface {

 public: // STATICS
    static const uint32_t CAM_ID = 0;

 public: // CAM USB
    CamUsb(std::string const& device);

    ~CamUsb();

    /**
     * Fast configuration of the camera (step 2 to 4).
     * Opens the camera and sets the frame size.
     * After this configuration, additional attributes can be set, the camera can be started by using
     * grab() and the images can be retrieved with retrieveFrame().
     * If the camera does not support the passed width and height, an appropriate image
     * size will be set.
     */
    void fastInit(int width, int height);
    
    /**
     * Adds the single cam_info structure to the passed vector.
     * The cam_info.display_name will be added later, as soon as 
     * the connection to the camera has been opened.
     */
    virtual int listCameras(std::vector<CamInfo> &cam_infos)const;

    /**
     * Stores the passed CamInfo structure and opens camera for configuration via v4l2.
     * \return Always true.
     */
    virtual bool open(const CamInfo &cam,const AccessMode mode=Master);

    bool isOpen()const;

    /**
     * \return NULL if camera is not open.
     */
    virtual const CamInfo* getCameraInfo()const;

    /**
     * Close camera, sets camera mode to CAM_USB_NONE (configuration or image requesting
     * is not possible anymore).
     * \return Always true.
     */
    virtual bool close();

    /**
     * To start the pipeline, just set 'mode' to sth. other than Stop (SingleFrame,
     * MultiFrame or Continuously). Images are always grapped continuously.
     * Pass Stop to stop grabbing and reenter the configuration mode. 
     * Could throw std::runtime_error if the passed mode is unknown or the mode should 
     * changed during the pipeline is running.
     * \return Return false if the pipeline could not be started.
     */
    virtual bool grab(const GrabMode mode = SingleFrame, const int buffer_len=1);              

    /**
     * Reads a JPEG and initializes the passed frame (blocking read).
     * \return true if a new image could be requested in 'timeout' msecs.
     */
    virtual bool retrieveFrame(base::samples::frame::Frame &frame,const int timeout=1000);

    /**
     * Stores the last retrieved frame to 'file_name'.
     */
    bool storeFrame(base::samples::frame::Frame& frame, std::string const& file_name);

    /**
     * Same as grab(), returns true if a new image is available.
     */
    virtual bool isFrameAvailable();

    /**
     * Skips the current image if available.
     * \return true if an image has been skipped.
     */
    virtual int skipFrames();

    virtual bool setIpSettings(const CamInfo &cam, const IPSettings &ip_settings)const;

    /**
     * Write the control value to the camera or throws a std::runtime_error.
     */
    virtual bool setAttrib(const int_attrib::CamAttrib attrib, const int value);

    /**
     * Sets the FPS or throws a std::runtime_error.
     * This method just sets the member variable mFps. 
     * You have to close and reopen the camera to take the new fps will take into account.
     */
    virtual bool setAttrib(const double_attrib::CamAttrib attrib, const double value);

    virtual bool setAttrib(const str_attrib::CamAttrib attrib,const std::string value) {
        throw std::runtime_error("setAttrib str_attrib is not yet implemented for the camera interface!");
        return 0;
    }

    /**
     * Switches the enumeration attribute or throws a std::runtime_error.
     */
    virtual bool setAttrib(const enum_attrib::CamAttrib attrib);

    /**
     * Returns true if the attribute can be used with the camera.
     * Excludes int_attrib::ExposureValue at the moment, because problems occurs using the
     * Microsoft LiveCam.
     */
    virtual bool isAttribAvail(const int_attrib::CamAttrib attrib);

    /**
     * Returns true if the fps could be set on the camera.
     */
    virtual bool isAttribAvail(const double_attrib::CamAttrib attrib);

    virtual bool isAttribAvail(const str_attrib::CamAttrib attrib) {
        throw std::runtime_error("isAttribAvail str_attrib is not yet implemented for the camera interface!");
        return 0;
    }
	        
    /**
     * Returns true if the attribute can be used with the camera.
     */
    virtual bool isAttribAvail(const enum_attrib::CamAttrib attrib);

    /**
     * Returns the control value or throws a std::runtime_error.
     */
    virtual int getAttrib(const int_attrib::CamAttrib attrib);

    /**
     * Returns the fps of the camera or throws a std::runtime_error.
     */
    virtual double getAttrib(const double_attrib::CamAttrib attrib);

    virtual std::string getAttrib(const str_attrib::CamAttrib attrib) {
        throw std::runtime_error("getAttrib str_attrib is not yet implemented for the camera interface!");
        return 0;
    }

    /**
     * Returns true if the enumeration attribute is set or throws a std::runtime_error.
     */
    virtual bool isAttribSet(const enum_attrib::CamAttrib attrib);

    /**
     * Can be used to check the availability of v4l2 control IDs directly.
     * \param control_id id of the control to check.
     * \param name For different cameras the same private base control id addresses different controls.
     * If the name parameter is used, the name of the control is checked as well.
     * \return false if the camera is not in configuration mode or the attribute is not available.
     */
    bool isV4L2AttribAvail(const int control_id, std::string name = "");

    /**
     * Allows to request the last read control value
     * (the value will not be re-read from the camera).
     * \throws std::runtime_error
     * \param control_id Control id to request.
     * \return false if the camera is not in configuration mode or the passed id is unknown.
     */
    int getV4L2Attrib(const int control_id);

    /**
     * Sets the v4l2 control id directly.
     * \param control_id Control id to set the value for.
     * \param value Value to set.
     * \throws std::runtime_error if the configuration mode is not active, the passed id is unknown
     * or writeControlValue() throws an exception.
     * \return true if the value could be set.
     */
    bool setV4L2Attrib(const int control_id, const int value);

    /**
     * If necessary 'size' will be changed to a valid one. 'mode' should be set to
     * base::samples::frame::MODE_JPEG and 'color_depth' to the bytes per pixel.
     */
    bool setFrameSettings(const base::samples::frame::frame_size_t size,
                                const base::samples::frame::frame_mode_t mode,
                                const uint8_t color_depth,
                                const bool resize_frames = true);
    
    /*
    virtual bool setFrameSettings(const base::samples::frame::Frame &frame,
                                const bool resize_frames = true);
    */

    /**
     * 
     */
    virtual bool getFrameSettings(base::samples::frame::frame_size_t &size,
                                    base::samples::frame::frame_mode_t &mode,
                                    uint8_t &color_depth);

    /**
     * Frames are requested continuously.
     */
    virtual bool triggerFrame();

    /**
     * Sets all control values on the camera back to default values.
     * The image size and the fps are not affected.
     * The connection to the camera has to be closed.
     */
    virtual bool setToDefault();

    //virtual bool setFrameToCameraFrameSettings(base::samples::frame::Frame &frame);

    virtual bool setCallbackFcn(void (*pcallback_function)(const void* p),void *p) {

        if(!pcallback_function)
            throw std::runtime_error ("You can not set the callback function to null!!! "
            "Otherwise CamUsb::callUserCallbackFcn would not be thread safe.");

        mpCallbackFunction = pcallback_function;
        mpPassThroughPointer = p;

        return true;
    }

    virtual void synchronizeWithSystemTime(uint32_t time_interval)
    {
    throw std::runtime_error("This camerea does not support synchronizeWithSystemTime. "
		       "The timestamp of the camera frame will be invalid.");
    };

    virtual void saveConfiguration(uint8_t index) {
        throw std::runtime_error("This camerea does not support saveConfiguration.");
    }

    virtual void loadConfiguration(uint8_t index) {
        throw std::runtime_error("This camerea does not support loadConfiguration.");
    }

    virtual void getRange(const double_attrib::CamAttrib attrib,double &dmin,double &dmax) {
        throw std::runtime_error("This camerea does not support getRange for double_attrib.");
    }

    virtual void getRange(const int_attrib::CamAttrib attrib,int &imin,int &imax);

    /**
     * The pipeline must be running.
     * \return -1 if an error occurred.
     */
    virtual int getFileDescriptor() const;

    inline enum CAM_USB_MODE getCamMode() {
        return mCamMode;
    }

 private:
    CamUsb(){};

    double calculateFPS() {
        timeval stop_time_grabbing;
        gettimeofday(&stop_time_grabbing, 0);
        double sec = stop_time_grabbing.tv_sec - mStartTimeGrabbing.tv_sec;
        if(sec == 0)
            return 0;
        else
            return mReceivedFrameCounter / sec;
    }

    enum CAM_USB_MODE mCamMode;

    /**
     * Because the configuration and the image transfer part
     * have to share one device, only one component can be active at once.
     * The other non-active component will be deleted.
     * \param cam_usb_mode CAM_USB_NONE deletes both components.
     */
    void changeCameraMode(enum CAM_USB_MODE cam_usb_mode);

    CamGst* mCamGst;
    CamConfig* mCamConfig;
    std::string mDevice;

    // Pipeline has been created and is running. No further configuration possible.
    bool mIsOpen; 

    struct CamInfo mCamInfo; 

    std::map<int_attrib::CamAttrib, int> mMapAttrsCtrlsInt;

    // Framerate set on the camera internally. If available it will set to > 0 during startup.
    // Will be set if the pipeline is not running and the fps of the camera is requested.
    double mFps;
    int mBpp;
    timeval mStartTimeGrabbing;
    int mReceivedFrameCounter;
    
    // FD driven image receiving (?).
    void (*mpCallbackFunction)(const void* p);
    void* mpPassThroughPointer;

    void createAttrsCtrlMaps();
};

} // end namespace camera

#endif
