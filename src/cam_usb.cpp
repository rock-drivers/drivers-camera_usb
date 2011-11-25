#include "cam_usb.h"

namespace camera 
{

CamUsb::CamUsb(std::string const& device) : CamInterface(), mCamConfig(NULL), mCamGst(NULL), 
        mDevice(), mIsOpen(false), mCamInfo(), mMapAttrsCtrlsInt(), mFps(0){
    mCamConfig = new CamConfig(device);
    mCamGst = new CamGst(device, mCamConfig);
    mDevice = device;
    createAttrsCtrlMaps();
}

CamUsb::~CamUsb() {
    delete mCamConfig;
    delete mCamGst;
}

int CamUsb::listCameras(std::vector<CamInfo> &cam_infos)const {

    for(uint32_t i=0; i<cam_infos.size(); ++i) {
        if(cam_infos[i].unique_id == CAM_ID)
            return 0;
    }
    
    struct CamInfo cam_info;

    cam_info.unique_id = CAM_ID;
    cam_info.device = mDevice;
    cam_info.interface_type = InterfaceUSB;
    cam_info.display_name = mCamConfig->getCapabilityCard();
    cam_info.reachable = false;
    
    cam_infos.push_back(cam_info);

    return 1; // number of cameras added.
}

bool CamUsb::open(const CamInfo &cam,const AccessMode mode) {
    if(mIsOpen)
        return true;

    mCamInfo = cam; // Assign camera (not allowed in listCameras()).

    // If one of the parameters is 0, the current setting of the camera is used.
    mCamGst->createDefaultPipeline(image_size_.width, image_size_.height, mFps);

    bool started = mCamGst->startPipeline(); // Frames will be requested continuously.
    mIsOpen = started;
    return mIsOpen;
}

bool CamUsb::isOpen()const {
    return mIsOpen;
}

const CamInfo* CamUsb::getCameraInfo()const {
    if(mIsOpen) 
        return &mCamInfo;
    else
        return NULL;
}

bool CamUsb::close() {
    if(mIsOpen) {
        mCamGst->deletePipeline();
        mIsOpen = false;
    }
    return true;
}

bool CamUsb::grab(const GrabMode mode, const int buffer_len) {
    return mCamGst->hasNewBuffer();
}                  

bool CamUsb::retrieveFrame(base::samples::frame::Frame &frame,const int timeout) {
    if(!mIsOpen)
        return false;
    uint8_t* buffer = NULL;
    uint32_t buf_size = 0;
    bool success = mCamGst->getBuffer(&buffer, &buf_size, true, timeout);
    if(!success)
        return false;
    // data_depth is unknown but must not be zero.
    base::samples::frame::frame_size_t size;
    base::samples::frame::frame_mode_t mode;
    uint8_t color_depth;
    getFrameSettings(size, mode, color_depth);
    frame.init(size.width, size.height, color_depth, mode, 0, buf_size);
    frame.setImage((char*)buffer, buf_size);
    delete buffer; buffer = NULL;
    return true;
}

bool CamUsb::isFrameAvailable() {
    return mCamGst->hasNewBuffer();
}

int CamUsb::skipFrames() {
    return mCamGst->skipBuffer() ? 1 : 0;
}

bool CamUsb::setIpSettings(const CamInfo &cam, const IPSettings &ip_settings)const {
    throw std::runtime_error("setIpSettings is not yet implemented for the camera interface!");
    return 0;
}

bool CamUsb::setAttrib(const int_attrib::CamAttrib attrib, const int value) {
    std::map<int_attrib::CamAttrib, int>::iterator it = mMapAttrsCtrlsInt.find(attrib);
    if(it == mMapAttrsCtrlsInt.end())
        throw std::runtime_error("Unknown attribute!");
    else
        mCamConfig->writeControlValue(it->second, value);

    return true;
}

bool CamUsb::setAttrib(const double_attrib::CamAttrib attrib, const double value) {
    if(mIsOpen) {
        std::cerr << "Close the device before reading and writing attributes." << std::endl;        
        return false;
    }

    switch(attrib) {

        case double_attrib::FrameRate:
        case double_attrib::StatFrameRate: {
            // Following line throws ERROR Could not write stream parameter: Device or resource busy.
            // Just set the member variable mFps. If the camera is closed and reopen, the
            // new fps will take into account.
            mCamConfig->writeFPS((uint32_t)value);
            break;
        }
        default:
            throw std::runtime_error("Unknown attribute!");
    }
    return true;
}

bool setAttrib(const str_attrib::CamAttrib attrib,const std::string value) {
    throw std::runtime_error("setAttrib str_attrib is not yet implemented for the camera interface!");
    return 0;
}

bool CamUsb::setAttrib(const enum_attrib::CamAttrib attrib) {
    if(mIsOpen) {
        std::cerr << "Close the device before reading and writing attributes." << std::endl;        
        return false;
    }

    switch(attrib) {
        case enum_attrib::WhitebalModeToManual: {
            mCamConfig->writeControlValue(V4L2_CID_AUTO_WHITE_BALANCE, 0);
            break;
        }
        case enum_attrib::WhitebalModeToAuto: {
            mCamConfig->writeControlValue(V4L2_CID_AUTO_WHITE_BALANCE, 1);
            break;
        }
        case enum_attrib::GainModeToManual: {
            mCamConfig->writeControlValue(V4L2_CID_AUTOGAIN, 0);
            break;
        }
        case enum_attrib::GainModeToAuto: {
            mCamConfig->writeControlValue(V4L2_CID_AUTOGAIN, 1);
            break;
        }
        case enum_attrib::PowerLineFrequencyDisabled: {
            mCamConfig->writeControlValue(V4L2_CID_POWER_LINE_FREQUENCY, 0);
            break;
        }
        case enum_attrib::PowerLineFrequencyTo50: {
            mCamConfig->writeControlValue(V4L2_CID_POWER_LINE_FREQUENCY, 1);
            break;
        }
        case enum_attrib::PowerLineFrequencyTo60: {
            mCamConfig->writeControlValue(V4L2_CID_POWER_LINE_FREQUENCY, 2);
            break;
        }
        // attribute unknown or not supported (yet)
        default:
            throw std::runtime_error("Unknown attribute!");
    }

    return true;
}

bool CamUsb::isAttribAvail(const int_attrib::CamAttrib attrib) {
    std::map<int_attrib::CamAttrib, int>::iterator it = mMapAttrsCtrlsInt.find(attrib);
    if(it == mMapAttrsCtrlsInt.end())
        return false;
    else
        return mCamConfig->isControlIdValid(it->second);
}

bool CamUsb::isAttribAvail(const double_attrib::CamAttrib attrib) {
    switch(attrib) {

        case double_attrib::FrameRate:
        case double_attrib::StatFrameRate: {
            return mCamConfig->hasCapabilityStreamparm(V4L2_CAP_TIMEPERFRAME);
        }
        default:
            return false;
    }
    return false;
}
        
bool CamUsb::isAttribAvail(const enum_attrib::CamAttrib attrib) {
    switch(attrib) {
        case enum_attrib::WhitebalModeToManual:
        case enum_attrib::WhitebalModeToAuto: {
            return mCamConfig->isControlIdValid(V4L2_CID_AUTO_WHITE_BALANCE);
        }
        case enum_attrib::GainModeToManual:
        case enum_attrib::GainModeToAuto: {
            return mCamConfig->isControlIdValid(V4L2_CID_AUTOGAIN);
        }
        case enum_attrib::PowerLineFrequencyDisabled:
        case enum_attrib::PowerLineFrequencyTo50:
        case enum_attrib::PowerLineFrequencyTo60: {
            return mCamConfig->isControlIdValid(V4L2_CID_POWER_LINE_FREQUENCY);
        }
        // attribute unknown or not supported (yet)
        default:
            return false;
    }
    return false;
}

int CamUsb::getAttrib(const int_attrib::CamAttrib attrib) {
    std::map<int_attrib::CamAttrib, int>::iterator it = mMapAttrsCtrlsInt.find(attrib);
    if(it == mMapAttrsCtrlsInt.end())
        throw std::runtime_error("Unknown attribute!");
    
    if(mIsOpen) {
        std::cout << "Returns the last set attribute. Close the device if "<<
            "you want to request the attribute from the camera directly. " << std::endl;        
        int32_t value = 0;
        mCamConfig->getControlValue(it->second, &value);
        return value;
    } else {
        return mCamConfig->readControlValue(it->second);   
    }
}

double CamUsb::getAttrib(const double_attrib::CamAttrib attrib) {

    switch(attrib) {
        case double_attrib::FrameRate:
        case double_attrib::StatFrameRate: {
            uint32_t fps;
            if(mIsOpen) {
                mCamConfig->getFPS(&fps);   
                std::cout << "Returns the last set fps. Close the device if "<<
                    "you want to request the fps from the camera directly. " << std::endl;        
            }
            else {
                mCamConfig->readFPS(&fps);
            }
            return (double)mFps;
        }
        default:
            throw std::runtime_error("Unknown attribute!");
    }
}

bool CamUsb::isAttribSet(const enum_attrib::CamAttrib attrib) {
    int32_t value = 0;
    switch(attrib) {
        case enum_attrib::WhitebalModeToManual: {
            if(mIsOpen)
                mCamConfig->getControlValue(V4L2_CID_AUTO_WHITE_BALANCE,&value);
            else
                value = mCamConfig->readControlValue(V4L2_CID_AUTO_WHITE_BALANCE); 
            return value == 0;
        }
        case enum_attrib::WhitebalModeToAuto: {
            if(mIsOpen)
                mCamConfig->getControlValue(V4L2_CID_AUTO_WHITE_BALANCE,&value);
            else
                value = mCamConfig->readControlValue(V4L2_CID_AUTO_WHITE_BALANCE); 
            return value == 1;
        }
        case enum_attrib::GainModeToManual: {
            if(mIsOpen)
                mCamConfig->getControlValue(V4L2_CID_AUTOGAIN,&value);
            else
                value = mCamConfig->readControlValue(V4L2_CID_AUTOGAIN); 
            return value == 0;
        }
        case enum_attrib::GainModeToAuto: {
            if(mIsOpen)
                mCamConfig->getControlValue(V4L2_CID_AUTOGAIN,&value);
            else
                value = mCamConfig->readControlValue(V4L2_CID_AUTOGAIN); 
            return value == 1;
        }
        case enum_attrib::PowerLineFrequencyDisabled: {
            if(mIsOpen) {
                mCamConfig->getControlValue(V4L2_CID_POWER_LINE_FREQUENCY, &value);
                return value == 0;
            }
            else
                return 0 == mCamConfig->readControlValue(V4L2_CID_POWER_LINE_FREQUENCY); 
        }
        case enum_attrib::PowerLineFrequencyTo50: {
           if(mIsOpen) {
                mCamConfig->getControlValue(V4L2_CID_POWER_LINE_FREQUENCY, &value);
                return value == 1;
            }
            else
                return 1 == mCamConfig->readControlValue(V4L2_CID_POWER_LINE_FREQUENCY); 
            break;
        }
        case enum_attrib::PowerLineFrequencyTo60: {
            if(mIsOpen) {
                mCamConfig->getControlValue(V4L2_CID_POWER_LINE_FREQUENCY, &value);
                return value == 2;
            }
            else
                return 2 == mCamConfig->readControlValue(V4L2_CID_POWER_LINE_FREQUENCY); 
        }
        // attribute unknown or not supported (yet)
        default:
            throw std::runtime_error("Unknown attribute!");
    }
}

bool CamUsb::getFrameSettings(base::samples::frame::frame_size_t &size,
                                base::samples::frame::frame_mode_t &mode,
                                uint8_t &color_depth) {
    // Read current values from the camera if device is not open.
    if(!mIsOpen) {      
        mCamConfig->readImageFormat();
    } else {
        std::cout << "Returns the last set image size. Close the device if "<<
                "you want to request the image size from the camera directly. " << std::endl;  
    }
    uint32_t width = 0, height = 0;
    mCamConfig->getImageWidth(&width);
    mCamConfig->getImageHeight(&height);
    size.width = (uint16_t)width;
    size.height = (uint16_t)height;
    mode = base::samples::frame::MODE_PJPG;
    color_depth = 3;
    
    return true;
}

bool CamUsb::triggerFrame() {
    return true;
}

bool CamUsb::setToDefault() {
    if(mIsOpen)
        return false;
    mCamConfig->setControlValuesToDefault();
    return true;
}

void CamUsb::getRange(const int_attrib::CamAttrib attrib,int &imin,int &imax) {
    std::map<int_attrib::CamAttrib, int>::iterator it = mMapAttrsCtrlsInt.find(attrib);
    if(it != mMapAttrsCtrlsInt.end()) {
        int32_t min = 0, max = 0;
        mCamConfig->getControlMinimum(it->second, &min);
        imin = min;
        mCamConfig->getControlMaximum(it->second, &max);
        imax = max;
    }
}

int CamUsb::getFileDescriptor() const {
    return mCamConfig->getFd();
}

void CamUsb::createAttrsCtrlMaps() {
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
