#include "cam_config.h"

#include <fcntl.h> // for open()
#include <stdio.h>
#include <sys/ioctl.h>

#include <iostream>

namespace camera 
{
 
CamConfig::CamConfig(std::string const& device) : mFd(0), mCapability(), mCamCtrls(), 
            mFormat(), mStreamparm() {
    LOG_DEBUG("CamConfig: constructor");
    
    memset(&mCapability, 0, sizeof(struct v4l2_capability));
    memset(&mFormat, 0, sizeof(struct v4l2_format));
    memset(&mStreamparm, 0, sizeof(struct v4l2_streamparm));

    mFd = ::open(device.c_str(), O_NONBLOCK | O_RDWR);
    if (mFd <= 0) {
        LOG_FATAL("Could not open device %s",device.c_str());
        std::string err_str(strerror(errno));
        throw std::runtime_error(err_str.insert(0, "Could not open device: "));
    } else {
        LOG_DEBUG("File opened, fd: %d",mFd);
    }

    // Catches CamConfigException (not supported functionalities).
    // std::runtime error is passed.
    try {
        readCapability();
    } catch (CamConfigException& err) {
        LOG_ERROR("%s",err.what());
    }
    try {
        readControl();
    } catch (CamConfigException& err) {
        LOG_ERROR("%s",err.what());
    }
    try {
        readImageFormat();
    } catch (CamConfigException& err) {
        LOG_ERROR("%s",err.what());
    }
    try {
        readStreamparm();
    } catch (CamConfigException& err) {
        LOG_ERROR("%s",err.what());
    }
}

CamConfig::~CamConfig() {
    LOG_DEBUG("CamConfig: destructor, close device");
    close(mFd);
}

// CAPABILITY
void CamConfig::readCapability() {
    LOG_DEBUG("CamConfig: readCapability");
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
    LOG_DEBUG("CamConfig: listCapabilities");

    LOG_INFO("Driver: %s", getCapabilityDriver().c_str());
    LOG_INFO("Card: %s ", getCapabilityCard().c_str());
    LOG_INFO("Bus Info: %s", getCapabilityBusInfo().c_str());
    LOG_INFO("Version: %d", getCapabilityVersion());

    uint32_t flag = mCapability.capabilities;
    LOG_INFO("Capabilities: ");
    if(flag & V4L2_CAP_VIDEO_CAPTURE) 
        LOG_INFO("V4L2_CAP_VIDEO_CAPTURE: The device can capture video data.");
    if(flag & V4L2_CAP_VIDEO_OUTPUT) 
        LOG_INFO("V4L2_CAP_VIDEO_OUTPUT: The device can perform video output.");
    if(flag & V4L2_CAP_VIDEO_OVERLAY) 
        LOG_INFO("V4L2_CAP_VIDEO_OVERLAY: It can do video overlay onto the frame buffer.");
    if(flag & V4L2_CAP_VBI_CAPTURE) 
        LOG_INFO("V4L2_CAP_VBI_CAPTURE: It can capture raw video blanking interval data.");
    if(flag & V4L2_CAP_VBI_OUTPUT) 
        LOG_INFO("V4L2_CAP_VBI_OUTPUT: It can do raw VBI output.");
    if(flag & V4L2_CAP_SLICED_VBI_CAPTURE) 
        LOG_INFO("V4L2_CAP_SLICED_VBI_CAPTURE: It can do sliced VBI capture.");
    if(flag & V4L2_CAP_SLICED_VBI_OUTPUT) 
        LOG_INFO("V4L2_CAP_SLICED_VBI_OUTPUT: It can do sliced VBI output.");
    if(flag & V4L2_CAP_RDS_CAPTURE) 
        LOG_INFO("V4L2_CAP_RDS_CAPTURE: It can capture Radio Data System (RDS) data.");
    if(flag & V4L2_CAP_TUNER) 
        LOG_INFO("V4L2_CAP_TUNER: It has a computer-controllable tuner.");
    if(flag & V4L2_CAP_AUDIO) 
        LOG_INFO("V4L2_CAP_AUDIO: It can capture audio data.");
    if(flag & V4L2_CAP_RADIO) 
        LOG_INFO("V4L2_CAP_RADIO: It is a radio device.");
    if(flag & V4L2_CAP_READWRITE) 
        LOG_INFO("V4L2_CAP_READWRITE: It supports the read() and/or write() system calls;" \
                " very few devices will support both. It makes little sense to write " \
                "to a camera, normally.");
    if(flag & V4L2_CAP_ASYNCIO) 
        LOG_INFO("V4L2_CAP_ASYNCIO: It supports asynchronous I/O. Unfortunately, " \
                "the V4L2 layer as a whole does not yet support asynchronous I/O, " \
                "so this capability is not meaningful.");
    if(flag & V4L2_CAP_STREAMING) 
        LOG_INFO("V4L2_CAP_STREAMING: It supports ioctl()-controlled streaming I/O.");
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
    if(!valid) {
        LOG_DEBUG("Capability flag %d not valid", capability_field);
        return false;
    }

    return capability_field & mCapability.capabilities;
}

// CONTROL
void CamConfig::readControl() {
    LOG_DEBUG("CamConfig: readControl");

    struct v4l2_queryctrl queryctrl_tmp;
    memset (&queryctrl_tmp, 0, sizeof (struct v4l2_queryctrl));
    mCamCtrls.clear();

    // Checks which controls are offered by the camera / device-driver.
    LOG_DEBUG("Check base control IDs");
    for (int i = V4L2_CID_BASE; 
            i < V4L2_CID_LASTP1;
            i++) {
        queryctrl_tmp.id = i;
        try {
            readControl(queryctrl_tmp);
        }
        catch (std::runtime_error& e) {
            LOG_ERROR("Reading V4L2_CID_BASE control parameter %d: %s", 
                    i-V4L2_CID_BASE, e.what());
        }
    }

    // MPEG
    LOG_DEBUG("Check MPEG base control IDs");
    for (int i = V4L2_CID_MPEG_BASE; 
            i < V4L2_CID_MPEG_BASE+8;
            i++) {
        queryctrl_tmp.id = i;
        try {
            readControl(queryctrl_tmp);
        }
        catch (std::runtime_error& e) {
            LOG_ERROR("Reading V4L2_CID_MPEG_BASE control parameter %d: %s", 
                    i-V4L2_CID_MPEG_BASE, e.what());
        }
    }

    LOG_DEBUG("Check MPEG audio base control IDs");
    for (int i = V4L2_CID_MPEG_BASE+100; 
            i < V4L2_CID_MPEG_BASE+112;
            i++) {
        queryctrl_tmp.id = i;
        try {
            readControl(queryctrl_tmp);
        }
        catch (std::runtime_error& e) {
            LOG_ERROR("Reading V4L2_CID_MPEG_BASE control parameter %d: %s", 
                    i-V4L2_CID_MPEG_BASE, e.what());
        }
    }

    LOG_DEBUG("Check MPEG video base control IDs");
    for (int i = V4L2_CID_MPEG_BASE+200; 
            i < V4L2_CID_MPEG_BASE+212;
            i++) {
        queryctrl_tmp.id = i;
        try {
            readControl(queryctrl_tmp);
        }
        catch (std::runtime_error& e) {
            LOG_ERROR("Reading V4L2_CID_MPEG_BASE control parameter %d: %s", 
                    i-V4L2_CID_MPEG_BASE, e.what());
        }
    }

    LOG_DEBUG("Check MPEG base control IDs specific to the CX2341x driver");
    for (int i = V4L2_CID_MPEG_CX2341X_BASE+0; 
            i < V4L2_CID_MPEG_CX2341X_BASE+12;
            i++) {
        queryctrl_tmp.id = i;
        try {
            readControl(queryctrl_tmp);
        }
        catch (std::runtime_error& e) {
            LOG_ERROR("Reading V4L2_CID_MPEG_CX2341X_BASE control parameter %d: %s", 
                    i-V4L2_CID_MPEG_CX2341X_BASE, e.what());
        }
    }

    // Camera class
    LOG_DEBUG("Check camera class control IDs");
    for (int i = V4L2_CID_CAMERA_CLASS_BASE+1; 
            i < V4L2_CID_CAMERA_CLASS_BASE+17;
            i++) {
        queryctrl_tmp.id = i;
        try {
            readControl(queryctrl_tmp);
        }
        catch (std::runtime_error& e) {
            LOG_ERROR("Reading V4L2_CID_CAMERA_CLASS_BASE control parameter %d: %s", 
                    i-V4L2_CID_CAMERA_CLASS_BASE, e.what());
        }
    }
    /*
    // FM Modulator
    LOG_DEBUG("FM Modulator class control IDs");
    for (int i = V4L2_CID_FM_TX_CLASS_BASE + 1; 
            i < V4L2_CID_FM_TX_CLASS_BASE + 7;
            i++) {
        queryctrl_tmp.id = i;
        readControl(queryctrl_tmp);
    }

    LOG_DEBUG("FM Modulator class control IDs");
    for (int i = V4L2_CID_FM_TX_CLASS_BASE + 64; 
            i < V4L2_CID_FM_TX_CLASS_BASE + 67;
            i++) {
        queryctrl_tmp.id = i;
        readControl(queryctrl_tmp);
    }

    LOG_DEBUG("FM Modulator class control IDs");
    for (int i = V4L2_CID_FM_TX_CLASS_BASE + 80; 
            i < V4L2_CID_FM_TX_CLASS_BASE + 85;
            i++) {
        queryctrl_tmp.id = i;
        readControl(queryctrl_tmp);
    }

    LOG_DEBUG("FM Modulator class control IDs");
    for (int i = V4L2_CID_FM_TX_CLASS_BASE + 96; 
            i < V4L2_CID_FM_TX_CLASS_BASE + 99;
            i++) {
        queryctrl_tmp.id = i;
        readControl(queryctrl_tmp);
    }

    LOG_DEBUG("FM Modulator class control IDs");
    for (int i = V4L2_CID_FM_TX_CLASS_BASE + 112; 
            i < V4L2_CID_FM_TX_CLASS_BASE + 115;
            i++) {
        queryctrl_tmp.id = i;
        readControl(queryctrl_tmp);
    }
    */

    // Checks private controls of the e-CAM32_OMAP_GSTIX
    LOG_DEBUG("Check private base control IDs");
    for (int i = V4L2_CID_PRIVATE_BASE + 1; 
            i < V4L2_CID_PRIVATE_BASE + 17;
            i++) {
        queryctrl_tmp.id = i;
        try {
            readControl(queryctrl_tmp);
        }
        catch (std::runtime_error& e) {
            LOG_ERROR("Reading V4L2_CID_PRIVATE_BASE control parameter %d: %s", 
                    i-V4L2_CID_PRIVATE_BASE, e.what());
        }
    }
}

void CamConfig::readControl(struct v4l2_queryctrl& queryctrl_tmp) {
    LOG_DEBUG("CamConfig: readControl %d", queryctrl_tmp.id);

    // Store control id because it could be changed by the driver.
    unsigned int original_control_id = queryctrl_tmp.id;

    // Control-request successful?
    if (0 == ioctl (mFd, VIDIOC_QUERYCTRL, &queryctrl_tmp)) {
        // Control available by the camera (continue if not)?
        if (queryctrl_tmp.flags & V4L2_CTRL_FLAG_DISABLED) {
            LOG_DEBUG("Control id %d marked as disabled", original_control_id);
            return;
        }

        // Driver seems not to like the control.
        if (queryctrl_tmp.id != original_control_id) {
            LOG_DEBUG("Driver has changed the control ID %d, will not be used", original_control_id);
            return;
        }

        // Create and fill CamCtrl object.
        CamCtrl cam_ctrl;
        cam_ctrl.mCtrl = queryctrl_tmp;

        // Read-only control?
        // Flags V4L2_CTRL_FLAG_GRABBED, V4L2_CTRL_FLAG_UPDATE, V4L2_CTRL_FLAG_INACTIVE
        // and V4L2_CTRL_FLAG_SLIDER are ignored at the moment.
        if (queryctrl_tmp.flags & V4L2_CTRL_FLAG_READ_ONLY) {
            LOG_DEBUG("Control id %d marked as read-only", original_control_id);
            cam_ctrl.mReadOnly = true;
            return;
        }

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
                    LOG_DEBUG(" - menu entry %s", buffer);
                } else {
                    std::string err_str(strerror(errno));
                    throw std::runtime_error(err_str.insert(0, 
                        "Could not read menu item: "));
                }
            }
        }

        // Read and store current value.
        try {
            cam_ctrl.mValue = readControlValue(original_control_id);
        } catch(std::runtime_error& e) {
            // Assuming write-only control (id valid, only write operation should work)
            LOG_DEBUG("Control ID %d seem to be write-only", original_control_id);
            cam_ctrl.mWriteOnly = true;
        }

        // Store CamCtrl using control ID as key.
        mCamCtrls.insert(std::pair<int32_t,struct CamCtrl>(original_control_id, cam_ctrl));
    } else {
        // Unknown control, will be ignored.
        if (errno == EINVAL) { 
            LOG_DEBUG("Control ID %d not available and will be ignored", original_control_id);
            return;
        }
        std::string err_str(strerror(errno)); 
        throw std::runtime_error(err_str.insert(0, "Could not query control: ")); 
    }
}

int32_t CamConfig::readControlValue(uint32_t const id) {
    LOG_DEBUG("CamConfig: readControlValue %d", id);

    struct v4l2_control control;
    memset(&control, 0, sizeof(struct v4l2_control));
    control.id = id;
   
    if (0 != ioctl (mFd, VIDIOC_G_CTRL, &control)) {
        std::string err_str(strerror(errno)); 
        throw std::runtime_error(err_str.insert(0, "Could not read control object value: ")); 
    }
    LOG_DEBUG("Control ID 0x%x(%d) value: %d", id, id, control.value);
    return control.value;
}

void CamConfig::writeControlValue(uint32_t const id, int32_t value) {
    LOG_DEBUG("CamConfig: writeControlValue %d to %d", id, value);
    struct v4l2_control control;
    control.id = id;
    control.value = value;

    // Problem setting control parameter V4L2_CID_WHITE_BALANCE_TEMPERATURE
    // on Microsoft LifeCam Cinema(TM), will be ignored.
    if(id == V4L2_CID_WHITE_BALANCE_TEMPERATURE) {
        LOG_WARN("Writing of control V4L2_CID_WHITE_BALANCE_TEMPERATURE is ignored!");
        return;
    }

    // Request internally stored control.
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    // id unknown?
    if(it == mCamCtrls.end()) {
        throw std::runtime_error("Passed id unknown");
    }

    // read-only control?
    if(it->second.mReadOnly) {
        throw std::runtime_error("Control with the passed id is read-only");
    }

    // Check borders.
    if(value < it->second.mCtrl.minimum) {
        LOG_INFO("Control %d value %d set to minimum %d", id, value, it->second.mCtrl.minimum);
        control.value = it->second.mCtrl.minimum;
    }
    if(value > it->second.mCtrl.maximum) {
        LOG_INFO("Control %d value %d set to maximum %d", id, value, it->second.mCtrl.maximum);
        control.value = it->second.mCtrl.maximum;
    }

    if(0 == ioctl (mFd, VIDIOC_S_CTRL, &control)) {
        // Change internally stored value as well.
        it->second.mValue = value;
    } else {
        std::string err_str(strerror(errno));
        
        // ID unknown? Should not happen or data would be out of sync.
        if(errno == EINVAL)
            throw CamConfigException(err_str.insert(0, 
                    "VIDIOC_S_CTRL is not supported by device driver: "));
        
        throw std::runtime_error(err_str.insert(0, "Could not write control object: ")); 
    }
    LOG_DEBUG("Control value 0x%x(%d) set to %d", id, id, value);
}

std::vector<uint32_t> CamConfig::getControlValidIDs() {
    std::map<uint32_t, struct CamCtrl>::iterator it;

    std::vector<uint32_t> ids;
    for(it = mCamCtrls.begin(); it != mCamCtrls.end(); it++) {
        ids.push_back(it->first);
    }

    return ids;
}

std::vector<struct CamConfig::CamCtrl> CamConfig::getControlList() {
    std::map<uint32_t, struct CamConfig::CamCtrl>::iterator it;

    std::vector<struct CamConfig::CamCtrl> controls;
    for(it = mCamCtrls.begin(); it != mCamCtrls.end(); it++) {
        controls.push_back(it->second);
    }
    
    return controls;
}

void CamConfig::listControls() {
    std::map<uint32_t, struct CamCtrl>::iterator it;

    for(it = mCamCtrls.begin(); it != mCamCtrls.end(); it++) {
        struct v4l2_queryctrl* pq = &(it->second.mCtrl);
        std::string name;
        getControlName(it->first, &name);
        LOG_INFO("\n\t0x%x(%d): %s, values: %d to %d (step %d), default: %d, current: %d, write-only: %s, read-only: %s\n", 
            pq->id, pq->id, name.c_str(), pq->minimum, pq->maximum, pq->step, pq->default_value, it->second.mValue,
            it->second.mWriteOnly ? "true" : "false", it->second.mReadOnly ? "true" : "false");
        if(it->second.mMenuItems.size() > 0) {
            LOG_INFO("Menu-Entries");
        }
        for(unsigned int i=0; i < it->second.mMenuItems.size(); i++) {
            LOG_INFO("%d: %s", i, it->second.mMenuItems[i].c_str());
        }
    }       
}

bool CamConfig::isControlIdValid(uint32_t const id) {
    return (mCamCtrls.find(id) != mCamCtrls.end());
}

bool CamConfig::getControlValue(uint32_t const id, int32_t* value) {
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    if(it == mCamCtrls.end()) {
        LOG_DEBUG("Control ID %d not found, no value is returned", id);
        return false;
    }

    *value = it->second.mValue;

    return true;   
}

bool CamConfig::getControlType(uint32_t const id, uint32_t* type) { 
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    if(it == mCamCtrls.end()) {
        LOG_DEBUG("Control ID %d not found, no control type is returned", id);
        return false;
    }

    *type = it->second.mCtrl.type;

    return true;
}

bool CamConfig::getControlName(uint32_t const id, std::string* name) { 
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    if(it == mCamCtrls.end()) {
        LOG_DEBUG("Control ID %d not found, no control name is returned", id);
        return false;
    }

    char buffer[32];
    snprintf(buffer, 32, "%s", it->second.mCtrl.name);

    *name = buffer;

    return true;
}

bool CamConfig::getControlMinimum(uint32_t const id, int32_t* minimum) { 
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    if(it == mCamCtrls.end()) {
        LOG_DEBUG("Control ID %d not found, no control minimum is returned", id);
        return false;
    }

    *minimum = it->second.mCtrl.minimum;

    return true;
}

bool CamConfig::getControlMaximum(uint32_t const id, int32_t* maximum) { 
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    if(it == mCamCtrls.end()) {
        LOG_DEBUG("Control ID %d not found, no control maximum is returned", id);
        return false;
    }

    *maximum = it->second.mCtrl.maximum;

    return true;
}

bool CamConfig::getControlStep(uint32_t const id, int32_t* step) { 
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    if(it == mCamCtrls.end()) {
        LOG_DEBUG("Control ID %d not found, no control step is returned", id);
        return false;
    }

    *step = it->second.mCtrl.step;

    return true;
}

bool CamConfig::getControlDefaultValue(uint32_t const id, int32_t* default_value) { 
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    if(it == mCamCtrls.end()) {
        LOG_DEBUG("Control ID %d not found, no control default value is returned", id);
        return false;
    }

    *default_value = it->second.mCtrl.default_value;

    return true;
}

bool CamConfig::getControlFlag(uint32_t const id, uint32_t const flag, bool* set) { 
    std::map<uint32_t, struct CamCtrl>::iterator it;
    it = mCamCtrls.find(id);

    // Valid id?
    if(it == mCamCtrls.end()) {
        LOG_DEBUG("Control ID %d not found, no control flag is returned", id);
        return false;
    }

    // Valid flag?
    bool valid_flag = false;
    for(uint32_t f=1; f<=V4L2_CTRL_FLAG_WRITE_ONLY; f*=2) {
        if(flag == f)
            valid_flag = true;
    }
    if(!valid_flag) {
        LOG_INFO("Control flag %d is not valid", flag);
        return false;
    }

    *set = (it->second.mCtrl.flags & flag);

    return true;
}

void CamConfig::setControlValuesToDefault() {
    LOG_DEBUG("Set control values to default");
    std::map<uint32_t, struct CamCtrl>::iterator it;
    for(it = mCamCtrls.begin(); it != mCamCtrls.end(); it++) {
        int32_t default_value;
        getControlDefaultValue(it->first, &default_value);
        writeControlValue(it->first, default_value);
    }
}

// IMAGE
void CamConfig::readImageFormat() {
    LOG_DEBUG("CamConfig: readImageFormat");
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

    LOG_DEBUG("CamConfig: writeImagePixelFormat");

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
    LOG_INFO("Image width: %d", mFormat.fmt.pix.width);
    LOG_INFO("Image height: %d", mFormat.fmt.pix.height);
    LOG_INFO("Image pixelformat: %s", pixelformat_str.c_str());
    LOG_INFO("Image field: %d", mFormat.fmt.pix.field);
    LOG_INFO("Image bytesperline: %d", mFormat.fmt.pix.bytesperline);
    LOG_INFO("Image sizeimage: %d", mFormat.fmt.pix.sizeimage);
    LOG_INFO("Image colorspace: %d", mFormat.fmt.pix.colorspace);
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
    
    char c1 =  mFormat.fmt.pix.pixelformat & 0xFF;
    char c2 =  (mFormat.fmt.pix.pixelformat >> 8) & 0xFF;
    char c3 =  (mFormat.fmt.pix.pixelformat >> 16) & 0xFF;
    char c4 =  (mFormat.fmt.pix.pixelformat >> 24) & 0xFF;

    char buffer[5] = {c1,c2,c3,c4,'\0'};
    std::string str(buffer);
    *pixelformat_str = str; 

    return true;
}

bool CamConfig::getImageField(uint32_t* field) {
    if(mFormat.type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        LOG_DEBUG("Format type %d, no image data stored", mFormat.type);
        return false;
    }
    
    *field = mFormat.fmt.pix.field;

    return true;
}

bool CamConfig::getImageBytesperline(uint32_t* bytesperline) {
    if(mFormat.type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        LOG_DEBUG("Format type %d, no image data stored", mFormat.type);
        return false;
    }
    
    *bytesperline = mFormat.fmt.pix.bytesperline;

    return true;
}

bool CamConfig::getImageSizeimage(uint32_t* sizeimage) {
    if(mFormat.type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        LOG_DEBUG("Format type %d, no image data stored", mFormat.type);
        return false;
    }
    
    *sizeimage = mFormat.fmt.pix.sizeimage;

    return true;
}

bool CamConfig::getImageColorspace(uint32_t* colorspace) {
    if(mFormat.type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
        LOG_DEBUG("Format type %d, no image data stored", mFormat.type);
        return false;
    }
    
    *colorspace = mFormat.fmt.pix.colorspace;

    return true;
}

// STREAMPARM
void CamConfig::readStreamparm() {
    LOG_DEBUG("CamConfig: readStreamparm");

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

    LOG_DEBUG("CamConfig: writeStreamparm");

    if(mStreamparm.type != V4L2_BUF_TYPE_VIDEO_CAPTURE) { // not yet requested?
        LOG_DEBUG("Streamparm not yet requested, read streamparm.");
        readStreamparm();
    }

    if(!hasCapabilityStreamparm(V4L2_CAP_TIMEPERFRAME)) {
        throw CamConfigException("FPS-setting not supported by device driver.");
    }

    if(numerator != 0)
        mStreamparm.parm.capture.timeperframe.numerator = numerator;
    else
        LOG_DEBUG("numerator is 0");

    if(denominator != 0)
        mStreamparm.parm.capture.timeperframe.denominator = denominator;
    else
        LOG_DEBUG("denominator is 0");

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
    LOG_INFO("Capabilities: ");
    uint32_t flag = mStreamparm.parm.capture.capability;
    if(flag & V4L2_CAP_TIMEPERFRAME) {
        LOG_INFO("V4L2_CAP_TIMEPERFRAME: The frame skipping/repeating " \
                "controlled by the timeperframe field is supported.");
    }

    LOG_INFO("Capturemodes: ");
    flag = mStreamparm.parm.capture.capturemode;
    if(flag & V4L2_MODE_HIGHQUALITY) {
        LOG_INFO("V4L2_MODE_HIGHQUALITY: High quality imaging mode.");
    }
    LOG_INFO("Capturemode: %d", mStreamparm.parm.capture.capturemode);

    LOG_INFO("Timeperframe: %d/%d", mStreamparm.parm.capture.timeperframe.numerator,
            mStreamparm.parm.capture.timeperframe.denominator);

    uint32_t extendedmode = mStreamparm.parm.capture.extendedmode;
    std::string ext_str = extendedmode == 0 ? " (unused)" : "";
    LOG_INFO("Extendedmode: %d%s", extendedmode, ext_str.c_str());

    uint32_t readbuffers = mStreamparm.parm.capture.readbuffers;       
    std::string read_str = readbuffers == 0 ? " Should not be zero!" : "";
    LOG_INFO("Readbuffers: %d%s", readbuffers, read_str.c_str());
}

bool CamConfig::readFPS(uint32_t* fps) {
    readStreamparm();

    uint32_t n = mStreamparm.parm.capture.timeperframe.numerator;
    uint32_t d = mStreamparm.parm.capture.timeperframe.denominator;
    
    if(n == 0) {
        LOG_INFO("Numerator is 0, fps 0 is returned");
    }

    *fps = n == 0 ? 0 : d/n;
    return true;
}

void CamConfig::writeFPS(uint32_t fps) {
    try {
        writeStreamparm(1,fps);
    } catch (CamConfigException& err) {
        LOG_ERROR("writeFPS: %s", err.what());
    }
}

bool CamConfig::getFPS(uint32_t* fps) {
    uint32_t n = mStreamparm.parm.capture.timeperframe.numerator;
    uint32_t d = mStreamparm.parm.capture.timeperframe.denominator;
    
    if(n == 0) {
        LOG_INFO("Numerator is 0, fps 0 is returned");
    }

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
    if(!valid) {
        LOG_INFO("Passed flag not valid, false is returned");
        return false;
    }

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
    if(!valid) {
        LOG_INFO("Passed flag not valid, false is returned");
        return false;
    }

    return (capturemode & mStreamparm.parm.capture.capturemode);
}

} // end namespace camera
