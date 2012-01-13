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

/**
 * Uses CamConfig to set and get all releveant camera parameters.
 * The GStreamer component starts a thread internally requesting images from the
 * same device camera-config is working with. This could lead to 
 * an exception telling the device is already in use.
 * Therefore, this class takes care that configuration is only possible if the
 * device is closed / the pipeline is deleted.\n
 * To get an image you have to do:\n
 * 1. CamUsb constructor passing the device.
 * 2. setFrameSettings to define the size, mode and data_depth of the image.
 *    optional: set attributes like brightness etc.
 * 3. listCameras to get a list of available cameras.
 * 4. open one of the available cameras (here always the first and only one).
 * 5. call grab to start requesting images / the GStreamer pipeline. 
 * 6. retrieveFrame to get a Frame.
 */
class CamUsb : public CamInterface {

 public: // STATICS
    static const uint32_t CAM_ID = 0;

 public: // CAM USB
    CamUsb(std::string const& device);

    ~CamUsb();
    
    /**
     * Adds the single cam_info structure to the passed vector.
     */
    virtual int listCameras(std::vector<CamInfo> &cam_infos)const;

    /**
     * Kind of initialization, creates pipeline and stores the passed
     * CamInfo structure.
     * \return Sets mIsOpen to true and returns true if the pipeline could be started.
     */
    virtual bool open(const CamInfo &cam,const AccessMode mode=Master);

    bool isOpen()const;

    /**
     * \return NULL if camera is not open.
     */
    virtual const CamInfo *getCameraInfo()const;

    /**
     * Deletes and stops pipeline / image requesting.
     * \return false if the pipeline is not open.
     */
    virtual bool close();

    /**
     * To start the pipeline, just set 'mode' to sth. other than Stop (SingleFrame,
     * MultiFrame or Continuously). Images are always grapped continuously.
     * Could throw std::runtime_error if the passed mode is unknown or the mode should 
     * changed during the pipeline is running.
     * \return Return false if the pipeline could not be started.
     */
    virtual bool grab(const GrabMode mode = SingleFrame, const int buffer_len=1);              

    /**
     * Reads a JPEG and initializes the passed frame (blocking read).
     * TODO Put timestamp information in frame.
     * \return true if a new image could be requested in 'timeout' msecs.
     */
    virtual bool retrieveFrame(base::samples::frame::Frame &frame,const int timeout=1000);

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
     * Returns the fps of the camera.
     */
    virtual double getAttrib(const double_attrib::CamAttrib attrib);

    virtual std::string getAttrib(const str_attrib::CamAttrib attrib) {
        throw std::runtime_error("getAttrib str_attrib is not yet implemented for the camera interface!");
        return 0;
    }

    /**
     * Returns true if the enumeration attribute is set.
     */
    virtual bool isAttribSet(const enum_attrib::CamAttrib attrib);

    /**
     * Can be used to check the availability of v4l2 control IDs directly.
     * Throws a CamConfigException if a used command is not supported or
     * std::runtime_exception if an read/write error occurred.
     */
    bool isV4L2AttribAvail(const int control_id);

    /**
     * Returns the value of the passed control id.
     * If the pipeline is still running, 0 will be returned.
     * Throws a CamConfigException if a used command is not supported or
     * std::runtime_exception if an read/write error occurred.
     */
    int getV4L2Attrib(const int control_id);

    /**
     * Sets the v4l2 control id directly.
     * Throws a CamConfigException if a used command is not supported or
     * std::runtime_exception if an read/write error occurred.
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

    virtual int getFileDescriptor() const;

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

    CamGst* mCamGst;
    std::string mDevice;

    // Pipeline has been created and is running. No further configuration possible.
    bool mIsOpen; 

    struct CamInfo mCamInfo; 

    std::map<int_attrib::CamAttrib, int> mMapAttrsCtrlsInt;

    // Framerate set on the camera internally. If available it will set to > 0 during startup.
    // Will be set if the pipeline is not running and the fps of the camera is requested.
    double mFps;
    timeval mStartTimeGrabbing;
    int mReceivedFrameCounter;
    
    // FD driven image receiving (?).
    void (*mpCallbackFunction)(const void* p);
    void* mpPassThroughPointer;

    void createAttrsCtrlMaps();
};

} // end namespace camera

#endif
