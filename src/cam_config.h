#ifndef _CAM_V4L2_CONFIG_H_
#define _CAM_V4L2_CONFIG_H_

#include <errno.h>
#include <stdint.h>
#include <string.h> // for strerror()
#include <linux/videodev2.h>

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

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
 * Using v4l2 to read and set the parameters of the specified camera.
 * An object of this class maxy not be used parallel to CamGst, because they would block the device.
 * Thus, use this to configure your camera and set parameters and delete it afterwards.
 * Initially, all values are empty. According to your needs initially call \n
 *  - readCapability()
 *  - readControl()
 *  - readImageFormat()
 *  - readStreamparm() 
 * Important: For newly-connected cameras the device driver can return a wrong image size!
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
        CamCtrl() : mCtrl(), mMenuItems(), mValue(0) {
            memset(&mCtrl, 0, sizeof(struct v4l2_queryctrl));
        }        
        
        ~CamCtrl() {
        }

        struct v4l2_queryctrl mCtrl;
        std::vector<std::string> mMenuItems;
        int32_t mValue;
    }; 

 public: // CAMCONFIG
    /**
     * Opens the device and reads all camera informations.
     * Throws std::runtime_error if the device could not be opened
     * or an io-error occurred.
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

    uint32_t getCapabilityVersion();

    /** 
     * \param field E.g. V4L2_CAP_VIDEO_CAPTURE or V4L2_CAP_VIDEO_OUTPUT.
     * See include/linux/videodev2.h for details.
     */
    bool hasCapability(uint32_t capability_field);

 public: // CONTROL
    void readControl();

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
     */
    void writeControlValue(uint32_t const id, int32_t value);

    /**
     * Returns list of valid control IDs.
     */
    std::vector<uint32_t> getControlValidIDs();

    void listControls();

    /**
     * Returns the last stored value of the control.
     * If you want to take care that the value is the same value stored on the camera,
     * use 'readControlValue()' (but be careful the device is not in used).
     */
    bool isControlIdValid(uint32_t const id);

    /**
     * Gets the last set control value, use 'readControl()' if you 
     * want to get the current value set on the camera.
     */
    bool getControlValue(uint32_t const id, int32_t* value);

    bool getControlType(uint32_t const id, enum v4l2_ctrl_type* type);

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
     * It is recommed to call this method without any parameter to take sure
     * that the image size, which is passed by the device-driver, is correct.
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
    bool getImageField(enum v4l2_field* field);

    /**
     * For padding, zero if unused.
     */
    bool getImageBytesperline(uint32_t* bytesperline);

    bool getImageSizeimage(uint32_t* sizeimage);

    bool getImageColorspace(enum v4l2_colorspace* colorspace);

 public: // STREAMPARM
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

 private:
    int mFd; /// File-descriptor to the camera.

    // Capability
    struct v4l2_capability mCapability;
    // Control
    std::map<uint32_t, struct CamCtrl> mCamCtrls;
    // Image
    struct v4l2_format mFormat;
    // Stream
    struct v4l2_streamparm mStreamparm;

    CamConfig() {}

};

} // end namespace camera

#endif
