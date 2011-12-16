#include "cam_usb.h"

namespace camera 
{

CamUsb::CamUsb(std::string const& device) : CamInterface(), mCamGst(NULL), 
        mDevice(), mIsOpen(false), mCamInfo(), mMapAttrsCtrlsInt(), mFps(0), 
        mStartTimeGrabbing(), mReceivedFrameCounter(0),
        mpCallbackFunction(NULL), mpPassThroughPointer(NULL) {
    #if PRINT_DEBUG
    std::cout << "CamUsb: constructor" << std::endl;
    #endif
    mCamGst = new CamGst(device);
    mDevice = device;
    createAttrsCtrlMaps();
}

CamUsb::~CamUsb() {
    #if PRINT_DEBUG
    std::cout << "CamUsb: destructor" << std::endl;
    #endif
    delete mCamGst;
}

int CamUsb::listCameras(std::vector<CamInfo> &cam_infos)const {
    #if PRINT_DEBUG
    std::cout << "CamUsb: listCameras" << std::endl;
    #endif
    // Pipeline must not be running, because CamInfo is used.
    if(mCamGst->isPipelineRunning())
        return 0;

    for(uint32_t i=0; i<cam_infos.size(); ++i) {
        if(cam_infos[i].unique_id == CAM_ID)
            return 0;
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
    #if PRINT_DEBUG
    std::cout << "CamUsb: open" << std::endl;
    #endif
    if(mIsOpen)
        return true;

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
    #if PRINT_DEBUG
    std::cout << "CamUsb: getCameraInfo" << std::endl;
    #endif
    if(mIsOpen) 
        return &mCamInfo;
    else
        return NULL;
}

bool CamUsb::close() {
    #if PRINT_DEBUG
    std::cout << "CamUsb: close" << std::endl;
    #endif
    if(mIsOpen) {
        mCamGst->deletePipeline();
        mIsOpen = false;
    }
    return true;
}

bool CamUsb::grab(const GrabMode mode, const int buffer_len) {
    #if PRINT_DEBUG
    std::cout << "CamUsb: grab" << std::endl;
    #endif 
    //check if someone tries to change the grab mode during grabbing
    if(act_grab_mode_!= Stop && mode != Stop) {
        if(act_grab_mode_ != mode)
            throw std::runtime_error("Stop grabbing before switching the grab mode!");
        else
            return true;
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
    #if PRINT_DEBUG
    std::cout << "CamUsb: retrieveFrame" << std::endl;
    #endif
    if(!mIsOpen || !mCamGst->isPipelineRunning()) {
        std::cerr << "Frame can not be retrieved, because pipeline is not open or not running."
                << std::endl;
        return false;
    }
    uint8_t* buffer = NULL;
    uint32_t buf_size = 0;
    // direkt in den frame buffer schreiben.
    bool success = mCamGst->getBuffer(&buffer, &buf_size, true, timeout);
    if(!success) {
        std::cerr << "Buffer could not retrieved." << std::endl;
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
    #if PRINT_DEBUG
    std::cout << "CamUsb: isFrameAvailable" << std::endl;
    #endif
    return mCamGst->hasNewBuffer();
}

int CamUsb::skipFrames() {
    #if PRINT_DEBUG
    std::cout << "CamUsb: skipFrames" << std::endl;
    #endif
    return mCamGst->skipBuffer() ? 1 : 0;
}

bool CamUsb::setIpSettings(const CamInfo &cam, const IPSettings &ip_settings)const {
    #if PRINT_DEBUG
    std::cout << "CamUsb: setIpSettings" << std::endl;
    #endif
    throw std::runtime_error("setIpSettings is not yet implemented for the camera interface!");
    return 0;
}

bool CamUsb::setAttrib(const int_attrib::CamAttrib attrib, const int value) {
    #if PRINT_DEBUG
    std::cout << "CamUsb: setAttrib int" << std::endl;
    #endif
    if(mCamGst->isPipelineRunning()) {
        std::cerr << "Stop image requesting before setting an int attribute." << std::endl;        
        return false;
    }

    CamConfig config(mDevice);

    std::map<int_attrib::CamAttrib, int>::iterator it = mMapAttrsCtrlsInt.find(attrib);
    if(it == mMapAttrsCtrlsInt.end())
        throw std::runtime_error("Unknown attribute!");
    else
        config.writeControlValue(it->second, value);

    return true;
}

bool CamUsb::setAttrib(const double_attrib::CamAttrib attrib, const double value) {
    #if PRINT_DEBUG
    std::cout << "CamUsb: seAttrib double" << std::endl;
    #endif
    if(mCamGst->isPipelineRunning()) {
        std::cerr << "Stop image requesting before setting a double attribute." << std::endl;        
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
    #if PRINT_DEBUG
    std::cout << "CamUsb: setAttrib string" << std::endl;
    #endif
    throw std::runtime_error("setAttrib str_attrib is not yet implemented for the camera interface!");
    return 0;
}

bool CamUsb::setAttrib(const enum_attrib::CamAttrib attrib) {
    #if PRINT_DEBUG
    std::cout << "CamUsb: setAttrib enum" << std::endl;
    #endif
    if(mCamGst->isPipelineRunning()) {
        std::cerr << "Stop image requesting before setting an enum attribute." << std::endl;        
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
    #if PRINT_DEBUG
    std::cout << "CamUsb: isAttribAvail int" << std::endl;
    #endif
    if(mCamGst->isPipelineRunning()) {
        std::cerr << "Stop image requesting before checking whether an int attribute is available." << std::endl;        
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
    #if PRINT_DEBUG
    std::cout << "CamUsb: isAttribAvail double" << std::endl;
    #endif
    
    if(mCamGst->isPipelineRunning()) {
        // mFps will only be set if the double attribute FrameRate or StatFrameRate is available.
        if((attrib == double_attrib::FrameRate || attrib == double_attrib::StatFrameRate) && mFps != 0) { 
            return true;
        } else {
            std::cerr << "Stop image requesting before checking whether an double attribute is available: " << attrib << std::endl;        
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
    #if PRINT_DEBUG
    std::cout << "CamUsb:isAttriAvail enum " << std::endl;
    #endif
    if(mCamGst->isPipelineRunning()) {
        std::cerr << "Stop image requesting before checking whether an enum attribute is available." << std::endl;        
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
    #if PRINT_DEBUG
    std::cout << "CamUsb: getAttrib int" << std::endl;
    #endif
    if(mCamGst->isPipelineRunning()) {
        std::cerr << "Stop image requesting before getting an int attribute." << std::endl;        
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
    #if PRINT_DEBUG
    std::cout << "CamUsb: getAttrib double" << std::endl;
    #endif
    if(mCamGst->isPipelineRunning()) {
        // If FrameRate or StatFrameRate is requested, the current fps are returned if the pipeline is running,
        // otherwise the fps which is set on the camera. 
        if(attrib == double_attrib::FrameRate || attrib == double_attrib::StatFrameRate) {
            return calculateFPS();
        }
        std::cerr << "Stop image requesting before getting a double attribute." << std::endl;        
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
    #if PRINT_DEBUG
    std::cout << "CamUsb: isAttribSet enum" << std::endl;
    #endif
    if(mCamGst->isPipelineRunning()) {
        std::cerr << "Stop image requesting before check whether a enum attribute is set." << std::endl;        
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

bool CamUsb::setFrameSettings(  const base::samples::frame::frame_size_t size,
                                      const base::samples::frame::frame_mode_t mode,
                                      const uint8_t color_depth,
                                      const bool resize_frames) {
    #if PRINT_DEBUG
    std::cout << "CamUsb: setFrameSettings" << std::endl;
    #endif
    if(mCamGst->isPipelineRunning()) {
        std::cerr << "Stop the device before setting frame settings." << std::endl;        
        return false;
    }

    if(mode != base::samples::frame::MODE_JPEG)
        std::cout << "Warning: mode should be set to base::samples::frame::MODE_JPEG!" << std::endl;
    
    std::cout << "color_depth is set to " << (int)color_depth << std::endl;

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
    #if PRINT_DEBUG
    std::cout << "CamUsb: getFrameSettings" << std::endl;
    #endif
    size = image_size_;
    mode = image_mode_;
    color_depth = image_color_depth_;
    
    return true;
}

bool CamUsb::triggerFrame() {
    return true;
}

bool CamUsb::setToDefault() {
    #if PRINT_DEBUG
    std::cout << "CamUsb: setToDefault" << std::endl;
    #endif
    if(mCamGst->isPipelineRunning()) {
        std::cerr << "Stop image requesting before set camera parameters to default." << std::endl;        
        return false;
    }

    CamConfig config(mDevice);

    config.setControlValuesToDefault();
    return true;
}

void CamUsb::getRange(const int_attrib::CamAttrib attrib,int &imin,int &imax) {
    #if PRINT_DEBUG
    std::cout << "CamUsb: getRange" << std::endl;
    #endif
    if(mCamGst->isPipelineRunning()) {
        std::cerr << "Stop image requesting before requesting range." << std::endl;        
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
    #if PRINT_DEBUG
    std::cout << "CamUsb: getFileDescriptor" << std::endl;
    #endif
    int fd = mCamGst->getFileDescriptor();
    if(fd == -1) {
        throw std::runtime_error("File descriptor could not be requested. Images must be requested (use grab()");
    }
    return fd;
}

void CamUsb::createAttrsCtrlMaps() {
    #if PRINT_DEBUG
    std::cout << "CamUsb: createAttrsCtrlMaps" << std::endl;
    #endif
    typedef std::pair<int_attrib::CamAttrib, int> ac_int;
    mMapAttrsCtrlsInt.insert(ac_int(int_attrib::BrightnessValue,V4L2_CID_BRIGHTNESS));
    mMapAttrsCtrlsInt.insert(ac_int(int_attrib::ContrastValue,V4L2_CID_CONTRAST));
    mMapAttrsCtrlsInt.insert(ac_int(int_attrib::SaturationValue,V4L2_CID_SATURATION));
    mMapAttrsCtrlsInt.insert(ac_int(int_attrib::WhitebalValue,V4L2_CID_WHITE_BALANCE_TEMPERATURE));
    mMapAttrsCtrlsInt.insert(ac_int(int_attrib::SharpnessValue,V4L2_CID_SHARPNESS));
    mMapAttrsCtrlsInt.insert(ac_int(int_attrib::BacklightCompensation,V4L2_CID_BACKLIGHT_COMPENSATION));
    mMapAttrsCtrlsInt.insert(ac_int(int_attrib::ExposureValue,V4L2_CID_EXPOSURE));
}

} // end namespace camera
