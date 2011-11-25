#include "cam_config.h"

#include <fcntl.h> // for open()
#include <stdio.h>
#include <sys/ioctl.h>

#include <iostream>

namespace camera 
{
 
CamConfig::CamConfig(std::string const& device) : mFd(0), mCapability(), mCamCtrls(), 
            mFormat(), mStreamparm() {
        memset(&mCapability, 0, sizeof(struct v4l2_capability));
        memset(&mFormat, 0, sizeof(struct v4l2_format));
        memset(&mStreamparm, 0, sizeof(struct v4l2_streamparm));

    mFd = open(device.c_str(), O_NONBLOCK | O_RDWR);
    if (mFd <= 0) {
        std::string err_str(strerror(errno));
        throw std::runtime_error(err_str.insert(0, "Could not open device: "));
    }

    /*
    // Catches CamConfigException (not supported functionalities).
    // std::runtime error is passed.
    try {
        readCapability();
    } catch (CamConfigException& err) {
        std::cout << err.what() << std::endl;
    }
    try {
        readControl();
    } catch (CamConfigException& err) {
        std::cout << err.what() << std::endl;
    }
    try {
        readImageFormat();
    } catch (CamConfigException& err) {
        std::cout << err.what() << std::endl;
    }
    try {
        readStreamparm();
    } catch (CamConfigException& err) {
        std::cout << err.what() << std::endl;
    }
    */
}

CamConfig::~CamConfig() {
    close(mFd);
}

// CAPABILITY
void CamConfig::readCapability() {
    memset (&mCapability, 0, sizeof (struct v4l2_capability));

    if (ioctl(mFd, VIDIOC_QUERYCAP, &mCapability) != 0) {
        std::string err_str(strerror(errno));
        if(errno == EINVAL) {
            throw CamConfigException(err_str.insert(0, 
                    "VIDIOC_QUERYCAP is not supported by device driver: "));
        }
        throw std::runtime_error(err_str.insert(0, "Could not read capability: "));
    }
}

void CamConfig::listCapabilities() {
    std::cout << "Driver: " << getCapabilityDriver() << std::endl;
    std::cout << "Card: " << getCapabilityCard() << std::endl;
    std::cout << "Bus Info: " << getCapabilityBusInfo() << std::endl;
    std::cout << "Version: " << getCapabilityVersion() << std::endl;

    uint32_t flag = mCapability.capabilities;
    std::cout << "Capabilities: " << std::endl;
    if(flag & V4L2_CAP_VIDEO_CAPTURE) 
        std::cout << "V4L2_CAP_VIDEO_CAPTURE: The device can capture video data." << std::endl;
    if(flag & V4L2_CAP_VIDEO_OUTPUT) 
        std::cout << "V4L2_CAP_VIDEO_OUTPUT: The device can perform video output." << std::endl;
    if(flag & V4L2_CAP_VIDEO_OVERLAY) 
        std::cout << "V4L2_CAP_VIDEO_OVERLAY: It can do video overlay onto the frame buffer." << std::endl;
    if(flag & V4L2_CAP_VBI_CAPTURE) 
        std::cout << "V4L2_CAP_VBI_CAPTURE: It can capture raw video blanking interval data." << std::endl;
    if(flag & V4L2_CAP_VBI_OUTPUT) 
        std::cout << "V4L2_CAP_VBI_OUTPUT: It can do raw VBI output." << std::endl;
    if(flag & V4L2_CAP_SLICED_VBI_CAPTURE) 
        std::cout << "V4L2_CAP_SLICED_VBI_CAPTURE: It can do sliced VBI capture." << std::endl;
    if(flag & V4L2_CAP_SLICED_VBI_OUTPUT) 
        std::cout << "V4L2_CAP_SLICED_VBI_OUTPUT: It can do sliced VBI output." << std::endl;
    if(flag & V4L2_CAP_RDS_CAPTURE) 
        std::cout << "V4L2_CAP_RDS_CAPTURE: It can capture Radio Data System (RDS) data." << std::endl;
    if(flag & V4L2_CAP_TUNER) 
        std::cout << "V4L2_CAP_TUNER: It has a computer-controllable tuner." << std::endl;
    if(flag & V4L2_CAP_AUDIO) 
        std::cout << "V4L2_CAP_AUDIO: It can capture audio data." << std::endl;
    if(flag & V4L2_CAP_RADIO) 
        std::cout << "V4L2_CAP_RADIO: It is a radio device." << std::endl;
    if(flag & V4L2_CAP_READWRITE) 
        std::cout << "V4L2_CAP_READWRITE: It supports the read() and/or write() system calls;" <<
                " very few devices will support both. It makes little sense to write " <<
                "to a camera, normally." << std::endl;
    if(flag & V4L2_CAP_ASYNCIO) 
        std::cout << "V4L2_CAP_ASYNCIO: It supports asynchronous I/O. Unfortunately, " <<
                "the V4L2 layer as a whole does not yet support asynchronous I/O, " <<
                "so this capability is not meaningful." << std::endl;
    if(flag & V4L2_CAP_STREAMING) 
        std::cout << "V4L2_CAP_STREAMING: It supports ioctl()-controlled streaming I/O." << std::endl;
}

std::string CamConfig::getCapabilityDriver() {
    char buffer[16];
    snprintf(buffer, 16, "%s", mCapability.driver);
    return std::string(buffer);
}

std::string CamConfig::getCapabilityCard() {
    char buffer[32];
    snprintf(buffer, 32, "%s", mCapability.card);
    return std::string(buffer);
}

std::string CamConfig::getCapabilityBusInfo() {
    char buffer[32];
    snprintf(buffer, 32, "%s", mCapability.bus_info);
    return std::string(buffer);
}

uint32_t CamConfig::getCapabilityVersion() {
    return mCapability.version;
}

bool CamConfig::hasCapability(uint32_t capability_field) {

    bool valid = false;
    // Flag valid?
    for(uint32_t f=1; f <= V4L2_CAP_STREAMING; f*=2) {
        if(f == capability_field) {
            valid = true;
        }
    }
    if(!valid)
        return false;

    return capability_field & mCapability.capabilities;
}

// CONTROL
void CamConfig::readControl() {
    struct v4l2_queryctrl queryctrl_tmp;
    memset (&queryctrl_tmp, 0, sizeof (struct v4l2_queryctrl));
    mCamCtrls.clear();

    // Checks which controls are offered by the camera / device-driver.
    for (queryctrl_tmp.id = V4L2_CID_BASE; 
            queryctrl_tmp.id < V4L2_CID_LASTP1;
            queryctrl_tmp.id++) {

        // Control-request successful?
        if (0 == ioctl (mFd, VIDIOC_QUERYCTRL, &queryctrl_tmp)) {
            // Control available by the camera (continue if not)?
            if (queryctrl_tmp.flags & V4L2_CTRL_FLAG_DISABLED)
                continue;

            // Create and fill CamCtrl object.
            CamCtrl cam_ctrl;
            cam_ctrl.mCtrl = queryctrl_tmp;

            // Read menue entries if available.
            if (queryctrl_tmp.type == V4L2_CTRL_TYPE_MENU) {
                
                struct v4l2_querymenu querymenu_tmp;
                memset (&querymenu_tmp, 0, sizeof (struct v4l2_querymenu));
                querymenu_tmp.id = queryctrl_tmp.id;

                // Store menu item names if the type of the control is a menu. 
                for (int i = queryctrl_tmp.minimum; i <= queryctrl_tmp.maximum; ++i) {
                    querymenu_tmp.index = (uint32_t)i;
                    if (0 == ioctl (mFd, VIDIOC_QUERYMENU, &querymenu_tmp)) {
                        // Store names of the menu items.
                        char buffer[32];
                        snprintf(buffer, 32, "%s", querymenu_tmp.name);
                        cam_ctrl.mMenuItems.push_back(buffer);
                    } else {
                        std::string err_str(strerror(errno));
                        throw std::runtime_error(err_str.insert(0, 
                            "Could not read menu item: "));
                    }
                }
            }

            // Read and store current value.
            cam_ctrl.mValue = readControlValue(queryctrl_tmp.id);

            // Store CamCtrl using control ID as key.
            mCamCtrls.insert(std::pair<int32_t,struct CamCtrl>(cam_ctrl.mCtrl.id, cam_ctrl));
        } else {
            if (errno == EINVAL) {
                continue;
            }
            std::string err_str(strerror(errno));
            throw std::runtime_error(err_str.insert(0, "Could not read control object: "));
        }
    }
}

int32_t CamConfig::readControlValue(uint32_t const id) {
    struct v4l2_control control;
    memset(&control, 0, sizeof(struct v4l2_control));
    control.id = id;
   
    if (0 != ioctl (mFd, VIDIOC_G_CTRL, &control)) {
        std::string err_str(strerror(errno));
        
        // ID unknown?
        if(errno == EINVAL)
            throw CamConfigException(err_str.insert(0, 
                    "VIDIOC_G_CTRL is not supported by device driver: ")); ;
        
        throw std::runtime_error(err_str.insert(0, "Could not read control object: ")); 
    }
    return control.value;
}

void CamConfig::writeControlValue(uint32_t const id, int32_t value) {
    struct v4l2_control control;
    control.id = id;
    control.value = value;

    // Problem setting control parameter V4L2_CID_WHITE_BALANCE_TEMPERATURE
    // on Microsoft LifeCam Cinema(TM), will be ignored.
    if(id == V4L2_CID_WHITE_BALANCE_TEMPERATURE) {
        std::cerr << "Writing control V4L2_CID_WHITE_BALANCE_TEMPERATURE is ignored." << std::endl;
        return;
    }

    // Request internally stored control.
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    // id unknown?
    if(it == mCamCtrls.end()) {
        throw std::runtime_error("Passed id unknown.");
    }

    // Check borders.
    if(value < it->second.mCtrl.minimum)
        value = it->second.mCtrl.minimum;
    if(value > it->second.mCtrl.maximum)
        value = it->second.mCtrl.maximum;

    if(0 == ioctl (mFd, VIDIOC_S_CTRL, &control)) {
        // Change internally stored value as well.
        it->second.mValue = value;
    } else {
        std::string err_str(strerror(errno));
        
        // ID unknown? Should not happen or data would be out of sync.
        if(errno == EINVAL)
            throw CamConfigException(err_str.insert(0, 
                    "VIDIOC_S_CTRL is not supported by device driver: "));
        
        throw std::runtime_error(err_str.insert(0, "Could not read control object: ")); 
    }
}

std::vector<uint32_t> CamConfig::getControlValidIDs() {
    std::map<uint32_t, struct CamCtrl>::iterator it;

    std::vector<uint32_t> ids;
    for(it = mCamCtrls.begin(); it != mCamCtrls.end(); it++) {
        ids.push_back(it->first);
    }

    return ids;
}

void CamConfig::listControls() {
    std::map<uint32_t, struct CamCtrl>::iterator it;

    for(it = mCamCtrls.begin(); it != mCamCtrls.end(); it++) {
        struct v4l2_queryctrl* pq = &(it->second.mCtrl);
        std::string name;
        getControlName(it->first, &name);
        std::cout << pq->id << ": " << name << ", values: " << 
            pq->minimum << " to " << pq->maximum << " (step " << pq->step << 
            "), default: " << pq->default_value << 
            ", current: " << it->second.mValue << std::endl;
    }       
}

bool CamConfig::isControlIdValid(uint32_t const id) {
    return (mCamCtrls.find(id) != mCamCtrls.end());
}

bool CamConfig::getControlValue(uint32_t const id, int32_t* value) {
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    if(it == mCamCtrls.end())
        return false;

    *value = it->second.mValue;

    return true;   
}

bool CamConfig::getControlType(uint32_t const id, enum v4l2_ctrl_type* type) { 
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    if(it == mCamCtrls.end())
        return false;

    *type = it->second.mCtrl.type;

    return true;
}

bool CamConfig::getControlName(uint32_t const id, std::string* name) { 
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    if(it == mCamCtrls.end())
        return false;

    char buffer[32];
    snprintf(buffer, 32, "%s", it->second.mCtrl.name);

    *name = buffer;

    return true;
}

bool CamConfig::getControlMinimum(uint32_t const id, int32_t* minimum) { 
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    if(it == mCamCtrls.end())
        return false;

    *minimum = it->second.mCtrl.minimum;

    return true;
}

bool CamConfig::getControlMaximum(uint32_t const id, int32_t* maximum) { 
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    if(it == mCamCtrls.end())
        return false;

    *maximum = it->second.mCtrl.maximum;

    return true;
}

bool CamConfig::getControlStep(uint32_t const id, int32_t* step) { 
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    if(it == mCamCtrls.end())
        return false;

    *step = it->second.mCtrl.step;

    return true;
}

bool CamConfig::getControlDefaultValue(uint32_t const id, int32_t* default_value) { 
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    if(it == mCamCtrls.end())
        return false;

    *default_value = it->second.mCtrl.default_value;

    return true;
}

bool CamConfig::getControlFlag(uint32_t const id, uint32_t const flag, bool* set) { 
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    // Valid id?
    if(it == mCamCtrls.end())
        return false;

    // Valid flag?
    bool valid_flag = false;
    for(uint32_t f=1; f<=V4L2_CTRL_FLAG_WRITE_ONLY; f*=2) {
        if(flag == f)
            valid_flag = true;
    }
    if(!valid_flag)
        return false;

    *set = (it->second.mCtrl.flags & flag);

    return true;
}

void CamConfig::setControlValuesToDefault() {
    std::map<uint32_t, struct CamCtrl>::iterator it;
    for(it = mCamCtrls.begin(); it != mCamCtrls.end(); it++) {
        int32_t default_value;
        getControlDefaultValue(it->first, &default_value);
        writeControlValue(it->first, default_value);
    }
}

// IMAGE
void CamConfig::readImageFormat() {

    memset(&mFormat, 0, sizeof(struct v4l2_format));

    // struct v4l2_pix_format should be requested.
    mFormat.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if(0 != ioctl(mFd, VIDIOC_G_FMT, &mFormat)) {
        std::string err_str(strerror(errno));
        if(errno == EINVAL) {
            throw CamConfigException(err_str.insert(0, 
                    "VIDIOC_G_FMT is not supported by device driver: "));
        }
        throw std::runtime_error(err_str.insert(0, "Could not read image format: "));
    }
}

void CamConfig::writeImagePixelFormat(uint32_t const width, uint32_t const height, 
        uint32_t const pixelformat) {

    if(mFormat.type != V4L2_BUF_TYPE_VIDEO_CAPTURE) { // not yet requested?
        readImageFormat();
    }

    if(width != 0)
        mFormat.fmt.pix.width = width;
    if(height != 0)
        mFormat.fmt.pix.height = height;
    if(pixelformat != 0)
        mFormat.fmt.pix.pixelformat = pixelformat;
    
    // Suggest the new format and fill mFormat with the actual one the driver choosed.
    if(0 != ioctl(mFd, VIDIOC_S_FMT, &mFormat)) {
        std::string err_str(strerror(errno));
        if(errno == EINVAL) {
            throw CamConfigException(err_str.insert(0, 
                    "VIDIOC_S_FMT is not supported by device driver: "));
        }
        throw std::runtime_error(err_str.insert(0, "Could not write image format: "));
    }
}

void CamConfig::listImageFormat() {
    std::string pixelformat_str;
    getImagePixelformatString(&pixelformat_str);
    std::cout << "Image width: " << mFormat.fmt.pix.width << std::endl;
    std::cout << "Image height: " << mFormat.fmt.pix.height << std::endl;
    std::cout << "Image pixelformat: " << pixelformat_str << std::endl;
    std::cout << "Image field: " << mFormat.fmt.pix.field << std::endl;
    std::cout << "Image bytesperline: " << mFormat.fmt.pix.bytesperline << std::endl;
    std::cout << "Image sizeimage: " << mFormat.fmt.pix.sizeimage << std::endl;
    std::cout << "Image colorspace: " << mFormat.fmt.pix.colorspace << std::endl;
}

bool CamConfig::getImageWidth(uint32_t* width) {
    if(mFormat.type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return false;
    
    *width = mFormat.fmt.pix.width;

    return true;
}

bool CamConfig::getImageHeight(uint32_t* height) {
    if(mFormat.type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return false;
    
    *height = mFormat.fmt.pix.height;

    return true;
}

bool CamConfig::getImagePixelformat(uint32_t* pixelformat) {
    if(mFormat.type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return false;
    
    *pixelformat = mFormat.fmt.pix.pixelformat;

    return true;
}

bool CamConfig::getImagePixelformatString(std::string* pixelformat_str) {
    if(mFormat.type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return false;
    
    unsigned char c1 =  mFormat.fmt.pix.pixelformat & 0xFF;
    unsigned char c2 =  (mFormat.fmt.pix.pixelformat >> 8) & 0xFF;
    unsigned char c3 =  (mFormat.fmt.pix.pixelformat >> 16) & 0xFF;
    unsigned char c4 =  (mFormat.fmt.pix.pixelformat >> 24) & 0xFF;

    char buffer[5] = {c1,c2,c3,c4,'\0'};
    std::string str(buffer);
    *pixelformat_str = str; 

    return true;
}

bool CamConfig::getImageField(enum v4l2_field* field) {
    if(mFormat.type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return false;
    
    *field = mFormat.fmt.pix.field;

    return true;
}

bool CamConfig::getImageBytesperline(uint32_t* bytesperline) {
    if(mFormat.type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return false;
    
    *bytesperline = mFormat.fmt.pix.bytesperline;

    return true;
}

bool CamConfig::getImageSizeimage(uint32_t* sizeimage) {
    if(mFormat.type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return false;
    
    *sizeimage = mFormat.fmt.pix.sizeimage;

    return true;
}

bool CamConfig::getImageColorspace(enum v4l2_colorspace* colorspace) {
    if(mFormat.type != V4L2_BUF_TYPE_VIDEO_CAPTURE)
        return false;
    
    *colorspace = mFormat.fmt.pix.colorspace;

    return true;
}

// STREAMPARM
void CamConfig::readStreamparm() {
    memset(&mStreamparm, 0, sizeof(struct v4l2_streamparm));

    mStreamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(mFd, VIDIOC_G_PARM, &mStreamparm) != 0) {
        std::string err_str(strerror(errno));
        if(errno == EINVAL) {
            throw CamConfigException(err_str.insert(0, 
                    "VIDIOC_G_PARM is not supported by device driver: "));
        }
        throw std::runtime_error(err_str.insert(0, "Could not read stream parameters: "));
    } 
}

void CamConfig::writeStreamparm(uint32_t const numerator, uint32_t const denominator) {

    if(mStreamparm.type != V4L2_BUF_TYPE_VIDEO_CAPTURE) { // not yet requested?
        readStreamparm();
    }

    if(!hasCapabilityStreamparm(V4L2_CAP_TIMEPERFRAME)) {
        throw CamConfigException("FPS-setting not supported by device driver.");
    }

    if(numerator != 0)
        mStreamparm.parm.capture.timeperframe.numerator = numerator;

    if(denominator != 0)
        mStreamparm.parm.capture.timeperframe.denominator = denominator;

    // Suggest the new stream parameters and fill mStreamparm with the actual one the driver choosed.
    if(0 != ioctl(mFd, VIDIOC_S_PARM, &mStreamparm)) {
        std::string err_str(strerror(errno));
        if(errno == EINVAL) {
            throw CamConfigException(err_str.insert(0, 
                    "VIDIOC_S_PARM is not supported by device driver: "));
        }
        throw std::runtime_error(err_str.insert(0, "Could not write stream parameter: "));
    }
}

void CamConfig::listStreamparm() {
    std::cout << "Capabilities: " << std::endl;
    uint32_t flag = mStreamparm.parm.capture.capability;
    if(flag & V4L2_CAP_TIMEPERFRAME) 
        std::cout << "V4L2_CAP_TIMEPERFRAME: The frame skipping/repeating " <<
                "controlled by the timeperframe field is supported." << std::endl;

    std::cout << "Capturemodes: " << std::endl;
    flag = mStreamparm.parm.capture.capturemode;
        std::cout << "V4L2_MODE_HIGHQUALITY: High quality imaging mode." << std::endl;
    std::cout << "Capturemode: " << mStreamparm.parm.capture.capturemode << std::endl;

    std::cout << "Timeperframe: " << mStreamparm.parm.capture.timeperframe.numerator << 
            "/" << mStreamparm.parm.capture.timeperframe.denominator << std::endl;

    uint32_t extendedmode = mStreamparm.parm.capture.extendedmode;
    std::string ext_str = extendedmode == 0 ? " (unused)" : "";
    std::cout << "Extendedmode: " << extendedmode << ext_str << std::endl;

    uint32_t readbuffers = mStreamparm.parm.capture.readbuffers;       
    std::string read_str = readbuffers == 0 ? " Should not be zero!" : "";
    std::cout << "Readbuffers: " << mStreamparm.parm.capture.readbuffers << read_str << std::endl;
}

bool CamConfig::readFPS(uint32_t* fps) {
    readStreamparm();

    uint32_t n = mStreamparm.parm.capture.timeperframe.numerator;
    uint32_t d = mStreamparm.parm.capture.timeperframe.denominator;
    
    *fps = n == 0 ? 0 : d/n;
    return true;
}

void CamConfig::writeFPS(uint32_t fps) {
    try {
        writeStreamparm(1,fps);
    } catch (CamConfigException& err) {
        std::cerr << err.what() << std::endl;
    }
}

bool CamConfig::getFPS(uint32_t* fps) {
    uint32_t n = mStreamparm.parm.capture.timeperframe.numerator;
    uint32_t d = mStreamparm.parm.capture.timeperframe.denominator;
    
    *fps = n == 0 ? 0 : d/n;
    return true;
}

bool CamConfig::hasCapabilityStreamparm(uint32_t capability_field) {

    bool valid = false;
    // Flag valid?
    for(uint32_t f=V4L2_CAP_TIMEPERFRAME; f <= V4L2_CAP_TIMEPERFRAME; f*=2) {
        if(f == capability_field) {
            valid = true;
        }
    }
    if(!valid)
        return false;

    return (capability_field & mStreamparm.parm.capture.capability);
}

bool CamConfig::hasCapturemodeStreamparm(uint32_t capturemode) {

    bool valid = false;
    // Flag valid?
    for(uint32_t f=	V4L2_MODE_HIGHQUALITY; f <= V4L2_MODE_HIGHQUALITY; f*=2) {
        if(f == capturemode) {
            valid = true;
        }
    }
    if(!valid)
        return false;

    return (capturemode & mStreamparm.parm.capture.capturemode);
}

} // end namespace camera
