#include "cam_usb.h"

namespace camera 
{

CamUsb::CamUsb(std::string const& device) : CamInterface(), mCamGst(NULL), 
        mDevice(), mIsOpen(false), mCamInfo(), mMapAttrsCtrlsInt(), mFps(0), 
        mStartTimeGrabbing(), mReceivedFrameCounter(0),
        mpCallbackFunction(NULL), mpPassThroughPointer(NULL) {
    LOG_DEBUG("CamUsb: constructor");
    mCamGst = new CamGst(device);
    mDevice = device;
    createAttrsCtrlMaps();
}

CamUsb::~CamUsb() {
    LOG_DEBUG("CamUsb: destructor");
    delete mCamGst;
}

int CamUsb::listCameras(std::vector<CamInfo> &cam_infos)const {
    LOG_DEBUG("CamUsb: listCameras");
    // Pipeline must not be running, because CamInfo is used.
    if(mCamGst->isPipelineRunning()) {
        LOG_INFO("Cameras can not be listed, because pipeline is running");
        return 0;
    }

    for(uint32_t i=0; i<cam_infos.size(); ++i) {
        if(cam_infos[i].unique_id == CAM_ID) {
            LOG_INFO("Camera already contained in passed vector, nothing added");
            return 0;
        }
    }
    
    CamConfig config(mDevice);

    struct CamInfo cam_info;

    cam_info.unique_id = CAM_ID;
    cam_info.device = mDevice;
    cam_info.interface_type = InterfaceUSB;
    cam_info.display_name = config.getCapabilityCard();
    cam_info.reachable = false;
    
    cam_infos.push_back(cam_info);

    return 1; // number of cameras added.
}

bool CamUsb::open(const CamInfo &cam,const AccessMode mode) {
    LOG_DEBUG("CamUsb: open");

    if(mIsOpen) {
        LOG_INFO("Camera %d already opened", cam.unique_id);
        return true;
    }

    mCamInfo = cam; // Assign camera (not allowed in listCameras()).

    // If one of the parameters is 0, the current setting of the camera is used.
    mCamGst->createDefaultPipeline(image_size_.width, image_size_.height, (uint32_t)mFps);
    mIsOpen = true;

    // Will be started in grab().
    return true;
}

bool CamUsb::isOpen()const {
    return mIsOpen;
}

const CamInfo* CamUsb::getCameraInfo()const {
    LOG_DEBUG("CamUsb: getCameraInfo");
    if(mIsOpen) 
        return &mCamInfo;
    else {
        LOG_INFO("Camera not open, no camera info can be returned");
        return NULL;
    }
}

bool CamUsb::close() {
    LOG_DEBUG("CamUsb: close");

    if(mIsOpen) {
        mCamGst->deletePipeline();
        mIsOpen = false;
    } else {
        LOG_INFO("Camera already closed");
    }
    return true;
}

bool CamUsb::grab(const GrabMode mode, const int buffer_len) {
    LOG_DEBUG("CamUsb: grab");
   
    //check if someone tries to change the grab mode during grabbing
    if(act_grab_mode_!= Stop && mode != Stop) {
        if(act_grab_mode_ != mode)
            throw std::runtime_error("Stop grabbing before switching the grab mode!");
        else {
            LOG_INFO("Mode already set to %d, nothing will be changed");
            return true;
        }
    }

    switch(mode) {
        case Stop:
            mCamGst->stopPipeline();
            break;
        case SingleFrame:
        case MultiFrame:
        case Continuously: {
            bool pipeline_started = false;
            pipeline_started = mCamGst->startPipeline();
            mReceivedFrameCounter = 0;
            if(pipeline_started) {
                gettimeofday(&mStartTimeGrabbing, 0);
            }
            return pipeline_started;
        }
        default: 
            throw std::runtime_error("The grab mode is not supported by the camera!");
    }

    return true;
}                  

bool CamUsb::retrieveFrame(base::samples::frame::Frame &frame,const int timeout) {
    LOG_DEBUG("CamUsb: retrieveFrame");

    if(!mIsOpen || !mCamGst->isPipelineRunning()) {
        LOG_WARN("Frame can not be retrieved, because pipeline is not open or not running.");
        return false;
    }
    uint8_t* buffer = NULL;
    uint32_t buf_size = 0;
    // Write directly to the frame buffer.
    bool success = mCamGst->getBuffer(&buffer, &buf_size, true, timeout);
    if(!success) {
        LOG_ERROR("Buffer could not retrieved.");
        return false;
    }

    frame.init(image_size_.width, image_size_.height, 8, image_mode_, 0, buf_size);
    frame.setImage((char*)buffer, buf_size);
    frame.frame_status = base::samples::frame::STATUS_VALID;
    frame.time = base::Time::now();
    delete buffer; buffer = NULL;

    mReceivedFrameCounter++;
    return true;
}

bool CamUsb::isFrameAvailable() {
    LOG_DEBUG("CamUsb: isFrameAvailable");
    return mCamGst->hasNewBuffer();
}

int CamUsb::skipFrames() {
    LOG_DEBUG("CamUsb: skipFrames");
    return mCamGst->skipBuffer() ? 1 : 0;
}

bool CamUsb::setIpSettings(const CamInfo &cam, const IPSettings &ip_settings)const {
    LOG_DEBUG("CamUsb: setIpSettings");
    throw std::runtime_error("setIpSettings is not yet implemented for the camera interface!");
    return 0;
}

bool CamUsb::setAttrib(const int_attrib::CamAttrib attrib, const int value) {
    LOG_DEBUG("CamUsb: setAttrib int");
    
    if(mCamGst->isPipelineRunning()) {
        LOG_INFO("Stop image requesting before setting an int attribute.");
        return false;
    }

    CamConfig config(mDevice);

    std::map<int_attrib::CamAttrib, int>::iterator it = mMapAttrsCtrlsInt.find(attrib);
    if(it == mMapAttrsCtrlsInt.end())
        throw std::runtime_error("Unknown attribute!");
    else {
        try {
            config.writeControlValue(it->second, value);
        } catch(std::runtime_error& e) {
            LOG_ERROR("Set integer attribute %d to %d: %s", attrib, value, e.what());
        }
    }
    return true;
}


bool CamUsb::setAttrib(const double_attrib::CamAttrib attrib, const double value) {
    LOG_DEBUG("CamUsb: seAttrib double");
   
    if(mCamGst->isPipelineRunning()) {
        LOG_INFO("Stop image requesting before setting a double attribute.");
        return false;
    }

    CamConfig config(mDevice);

    switch(attrib) {

        case double_attrib::FrameRate:
        case double_attrib::StatFrameRate: {
            config.writeFPS((uint32_t)value);
            mFps = value;
            break;
        }
        default:
            throw std::runtime_error("Unknown attribute!");
    }
    return true;
}

bool setAttrib(const str_attrib::CamAttrib attrib,const std::string value) {
    LOG_DEBUG("CamUsb: setAttrib string");
    throw std::runtime_error("setAttrib str_attrib is not yet implemented for the camera interface!");
    return 0;
}

bool CamUsb::setAttrib(const enum_attrib::CamAttrib attrib) {
    LOG_DEBUG("CamUsb: setAttrib enum");

    if(mCamGst->isPipelineRunning()) {
        LOG_INFO("Stop image requesting before setting an enum attribute.");
        return false;
    }

    CamConfig config(mDevice);

    switch(attrib) {
        case enum_attrib::WhitebalModeToManual: {
            config.writeControlValue(V4L2_CID_AUTO_WHITE_BALANCE, 0);
            break;
        }
        case enum_attrib::WhitebalModeToAuto: {
            config.writeControlValue(V4L2_CID_AUTO_WHITE_BALANCE, 1);
            break;
        }
        case enum_attrib::GainModeToManual: {
            config.writeControlValue(V4L2_CID_AUTOGAIN, 0);
            break;
        }
        case enum_attrib::GainModeToAuto: {
            config.writeControlValue(V4L2_CID_AUTOGAIN, 1);
            break;
        }
        case enum_attrib::PowerLineFrequencyDisabled: {
            config.writeControlValue(V4L2_CID_POWER_LINE_FREQUENCY, 0);
            break;
        }
        case enum_attrib::PowerLineFrequencyTo50: {
            config.writeControlValue(V4L2_CID_POWER_LINE_FREQUENCY, 1);
            break;
        }
        case enum_attrib::PowerLineFrequencyTo60: {
            config.writeControlValue(V4L2_CID_POWER_LINE_FREQUENCY, 2);
            break;
        }
        // attribute unknown or not supported (yet)
        default:
            throw std::runtime_error("Unknown attribute!");
    }

    return true;
}

bool CamUsb::isAttribAvail(const int_attrib::CamAttrib attrib) {
    LOG_DEBUG("CamUsb: isAttribAvail int");

    if(attrib == int_attrib::ExposureValue) {
        LOG_WARN("The current driver version ignores the integer attribute ExposureValue.");
        return false;
    }

    if(mCamGst->isPipelineRunning()) {
        LOG_INFO("Stop image requesting before checking whether an int attribute is available.");
        return false;
    }

    CamConfig config(mDevice);

    std::map<int_attrib::CamAttrib, int>::iterator it = mMapAttrsCtrlsInt.find(attrib);
    if(it == mMapAttrsCtrlsInt.end())
        return false;
    else
        return config.isControlIdValid(it->second);
}

bool CamUsb::isAttribAvail(const double_attrib::CamAttrib attrib) {
    LOG_DEBUG("CamUsb: isAttribAvail double");
    
    if(mCamGst->isPipelineRunning()) {
        // mFps will only be set if the double attribute FrameRate or StatFrameRate is available.
        if((attrib == double_attrib::FrameRate || attrib == double_attrib::StatFrameRate) && mFps != 0) { 
            return true;
        } else {
            LOG_INFO("Stop image requesting before checking whether an double attribute is available.");
            return false;
        }
    }

    CamConfig config(mDevice);

    switch(attrib) {

        case double_attrib::FrameRate:
        case double_attrib::StatFrameRate: {
            return config.hasCapabilityStreamparm(V4L2_CAP_TIMEPERFRAME);
        }
        default:
            return false;
    }
    return false;
}
        
bool CamUsb::isAttribAvail(const enum_attrib::CamAttrib attrib) {
    LOG_DEBUG("CamUsb:isAttriAvail enum");
    
    if(mCamGst->isPipelineRunning()) {
        LOG_INFO("Stop image requesting before checking whether an enum attribute is available.");
        return false;
    }

    CamConfig config(mDevice);

    switch(attrib) {
        case enum_attrib::WhitebalModeToManual:
        case enum_attrib::WhitebalModeToAuto: {
            return config.isControlIdValid(V4L2_CID_AUTO_WHITE_BALANCE);
        }
        case enum_attrib::GainModeToManual:
        case enum_attrib::GainModeToAuto: {
            return config.isControlIdValid(V4L2_CID_AUTOGAIN);
        }
        case enum_attrib::PowerLineFrequencyDisabled:
        case enum_attrib::PowerLineFrequencyTo50:
        case enum_attrib::PowerLineFrequencyTo60: {
            return config.isControlIdValid(V4L2_CID_POWER_LINE_FREQUENCY);
        }
        // attribute unknown or not supported (yet)
        default:
            return false;
    }
    return false;
}

int CamUsb::getAttrib(const int_attrib::CamAttrib attrib) {
    LOG_DEBUG("CamUsb: getAttrib int");

    if(mCamGst->isPipelineRunning()) {
        LOG_INFO("Stop image requesting before getting an int attribute.");
        return 0;
    }

    CamConfig config(mDevice);

    std::map<int_attrib::CamAttrib, int>::iterator it = mMapAttrsCtrlsInt.find(attrib);
    if(it == mMapAttrsCtrlsInt.end())
        throw std::runtime_error("Unknown attribute!");

    int32_t value = 0;
    config.getControlValue(it->second, &value);    

    return value; 
}

double CamUsb::getAttrib(const double_attrib::CamAttrib attrib) {
    LOG_DEBUG("CamUsb: getAttrib double");

    if(mCamGst->isPipelineRunning()) {
        // If FrameRate or StatFrameRate is requested, the current fps are returned if the pipeline is running,
        // otherwise the fps which is set on the camera. 
        if(attrib == double_attrib::FrameRate || attrib == double_attrib::StatFrameRate) {
            return calculateFPS();
        }
        LOG_INFO("Stop image requesting before getting a double attribute.");
        return 0.0;
    }

    CamConfig config(mDevice);

    switch(attrib) {
        case double_attrib::FrameRate:
        case double_attrib::StatFrameRate: {
            uint32_t fps;
            config.readFPS(&fps);
            mFps = (double)fps;
            return mFps;
        }
        default:
            throw std::runtime_error("Unknown attribute!");
    }
}

bool CamUsb::isAttribSet(const enum_attrib::CamAttrib attrib) {
    LOG_DEBUG("CamUsb: isAttribSet enum");
   
    if(mCamGst->isPipelineRunning()) {
        LOG_INFO("Stop image requesting before check whether a enum attribute is set.");
        return false;
    }

    CamConfig config(mDevice);

    int32_t value = 0;
    switch(attrib) {
        case enum_attrib::WhitebalModeToManual: 
            config.getControlValue(V4L2_CID_AUTO_WHITE_BALANCE, &value);
            return value == 0;
        case enum_attrib::WhitebalModeToAuto:
            config.getControlValue(V4L2_CID_AUTO_WHITE_BALANCE, &value);
            return value == 1;
        case enum_attrib::GainModeToManual:
            config.getControlValue(V4L2_CID_AUTOGAIN, &value);
            return value == 0;
        case enum_attrib::GainModeToAuto:
            config.getControlValue(V4L2_CID_AUTOGAIN, &value);
            return value == 1;
        case enum_attrib::PowerLineFrequencyDisabled:
            config.getControlValue(V4L2_CID_POWER_LINE_FREQUENCY, &value); 
            return value == 0;
        case enum_attrib::PowerLineFrequencyTo50:
            config.getControlValue(V4L2_CID_POWER_LINE_FREQUENCY, &value); 
            return value == 1;
        case enum_attrib::PowerLineFrequencyTo60:
             config.getControlValue(V4L2_CID_POWER_LINE_FREQUENCY, &value); 
            return value == 2;
        // attribute unknown or not supported (yet)
        default:
            throw std::runtime_error("Unknown attribute!");
    }
}

bool CamUsb::isV4L2AttribAvail(const int control_id) {
    LOG_DEBUG("CamUsb: isV4L2AttribAvail");
   
    if(mCamGst->isPipelineRunning()) {
        LOG_INFO("Stop image requesting before check whether a v4l2 control attribute is available.");
        return false;
    }

    CamConfig config(mDevice);
    return config.isControlIdValid(control_id);
}

int CamUsb::getV4L2Attrib(const int control_id) {
    LOG_DEBUG("CamUsb: isAttribSet enum");
   
    if(mCamGst->isPipelineRunning()) {
        LOG_INFO("Stop image requesting before getting a v4l2 attribute.");
        return 0;
    }

    CamConfig config(mDevice);
    int value = 0;

    if(!config.getControlValue(control_id, &value))
        throw std::runtime_error("Unknown control id!");
    else
        return value;
}

bool CamUsb::setV4L2Attrib(const int control_id, const int value) {
    LOG_DEBUG("CamUsb: isAttribSet enum");
   
    if(mCamGst->isPipelineRunning()) {
        LOG_INFO("Stop image requesting before setting a v4l2 attribute.");
        return false;
    }

    CamConfig config(mDevice);
    if(!config.isControlIdValid(control_id)) {
        return false;
    }
    
    config.writeControlValue(control_id, value);
    return true;
}

bool CamUsb::setFrameSettings(  const base::samples::frame::frame_size_t size,
                                      const base::samples::frame::frame_mode_t mode,
                                      const uint8_t color_depth,
                                      const bool resize_frames) {

    LOG_DEBUG("CamUsb: setFrameSettings");
    
    if(mCamGst->isPipelineRunning()) {
        LOG_INFO("Stop the device before setting frame settings.");
        return false;
    }

    if(mode != base::samples::frame::MODE_JPEG)
        LOG_WARN("Warning: mode should be set to base::samples::frame::MODE_JPEG!");
    
    LOG_DEBUG("color_depth is set to %d", (int)color_depth);

    CamConfig config(mDevice);
    config.writeImagePixelFormat(size.width, size.height); // use V4L2_PIX_FMT_YUV420?
    uint32_t width = 0, height = 0;
    config.getImageWidth(&width);
    config.getImageHeight(&height);

    base::samples::frame::frame_size_t size_tmp;
    size_tmp.width = (uint16_t)width;
    size_tmp.height = (uint16_t)height;

    image_size_ = size_tmp;
    image_mode_ = mode;
    image_color_depth_ = color_depth;
    return true;
}

bool CamUsb::getFrameSettings(base::samples::frame::frame_size_t &size,
                                base::samples::frame::frame_mode_t &mode,
                                uint8_t &color_depth) {

    LOG_DEBUG("CamUsb: getFrameSettings");

    size = image_size_;
    mode = image_mode_;
    color_depth = image_color_depth_;
    
    return true;
}

bool CamUsb::triggerFrame() {
    return true;
}

bool CamUsb::setToDefault() {
    LOG_DEBUG("CamUsb: setToDefault");
    
    if(mCamGst->isPipelineRunning()) {
        LOG_INFO("Stop image requesting before set camera parameters to default.");
        return false;
    }

    CamConfig config(mDevice);

    config.setControlValuesToDefault();
    return true;
}

void CamUsb::getRange(const int_attrib::CamAttrib attrib,int &imin,int &imax) {
    LOG_DEBUG("CamUsb: getRange");
    
    if(mCamGst->isPipelineRunning()) {
        LOG_INFO("Stop image requesting before requesting range.");
        return;
    }

    CamConfig config(mDevice);

    std::map<int_attrib::CamAttrib, int>::iterator it = mMapAttrsCtrlsInt.find(attrib);
    if(it != mMapAttrsCtrlsInt.end()) {
        int32_t min = 0, max = 0;
        config.getControlMinimum(it->second, &min);
        imin = min;
        config.getControlMaximum(it->second, &max);
        imax = max;
    }
}

int CamUsb::getFileDescriptor() const {
    LOG_DEBUG("CamUsb: getFileDescriptor");
   
    int fd = mCamGst->getFileDescriptor();
    if(fd == -1) {
        throw std::runtime_error("File descriptor could not be requested. Images must be requested (use grab()");
    }
    return fd;
}

void CamUsb::createAttrsCtrlMaps() {
    LOG_DEBUG("CamUsb: createAttrsCtrlMaps");
    
    typedef std::pair<int_attrib::CamAttrib, int> ac_int;
    mMapAttrsCtrlsInt.insert(ac_int(int_attrib::BrightnessValue,V4L2_CID_BRIGHTNESS));
    mMapAttrsCtrlsInt.insert(ac_int(int_attrib::ContrastValue,V4L2_CID_CONTRAST));
    mMapAttrsCtrlsInt.insert(ac_int(int_attrib::SaturationValue,V4L2_CID_SATURATION));
    mMapAttrsCtrlsInt.insert(ac_int(int_attrib::WhitebalValue,V4L2_CID_WHITE_BALANCE_TEMPERATURE));
    mMapAttrsCtrlsInt.insert(ac_int(int_attrib::SharpnessValue,V4L2_CID_SHARPNESS));
    mMapAttrsCtrlsInt.insert(ac_int(int_attrib::BacklightCompensation,V4L2_CID_BACKLIGHT_COMPENSATION));
}

} // end namespace camera
