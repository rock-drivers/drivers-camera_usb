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

#include "camera_interface/CamInterface.h"

#include "cam_gst.h"
#include "cam_config.h"

namespace camera 
{

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
     * Kind of initialization, creates and starts pipeline and stores the passed
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
     * Images are grapped continuously, method just returns true if a new image is available.
     */
    virtual bool grab(const GrabMode mode = SingleFrame, const int buffer_len=1);              

    /**
     * Reads a JPEG and initializes the passed frame (blocking read).
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
     * At the moment only the parameter 'size' is used.
     * Where to set fps?
     */
    /*
    virtual bool setFrameSettings(const base::samples::frame::frame_size_t size,
                                  const base::samples::frame::frame_mode_t mode,
                                  const uint8_t color_depth,
                                  const bool resize_frames = true);

    virtual bool setFrameSettings(const base::samples::frame::Frame &frame,
                                const bool resize_frames = true);
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
        throw std::runtime_error("This camerea does not support callback functions. "
		        "Use is isFrameAvailable() instead.");
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

    //virtual std::string doDiagnose();

 private:
    CamUsb(){};

    CamConfig* mCamConfig;
    CamGst* mCamGst;
    std::string mDevice;

    bool mIsOpen;

    struct CamInfo mCamInfo; 

    std::map<int_attrib::CamAttrib, int> mMapAttrsCtrlsInt;

    uint32_t mFps;

    void createAttrsCtrlMaps();
};

} // end namespace camera

#endif
