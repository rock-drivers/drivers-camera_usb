#ifndef _V4L2_TEST_H_
#define _V4L2_TEST_H_

#include <linux/videodev2.h>
#include <sys/ioctl.h>

#include <map>
#include <string>

#include "camera_usb/cam_config.h"

BOOST_AUTO_TEST_SUITE(v4l2_test_suite)

#if 0
int fd = 0;

BOOST_AUTO_TEST_CASE(open_camera_video)
{	
    fd = ::open("/dev/video0",O_NONBLOCK | O_RDWR);
	if (fd <= 0) {
		printf("Cannot open device\n");
		perror("Error:");
        BOOST_ERROR("Could not open /dev/video0");
	}

    BOOST_CHECK(fd > 0);
}

/*
struct v4l2_capability
{
    __u8	driver[16];	    // i.e. "bttv"
    __u8	card[32];	    // i.e. "Hauppauge WinTV"
    __u8	bus_info[32];	// "PCI:" + pci_name(pci_dev)
    __u32   version;        // should use KERNEL_VERSION()
    __u32	capabilities;	// Device capabilities
    __u32	reserved[4];
};
*/  
BOOST_AUTO_TEST_CASE(read_capabilities)
{
    printf("\nread_capabilities:\n");

    struct v4l2_capability cap;
    memset (&cap, 0, sizeof (struct v4l2_capability));

    if (0 == ioctl (fd, VIDIOC_QUERYCAP, &cap)) {
        printf("driver: %s\n",cap.driver);
        printf("card: %s\n",cap.card);
        printf("bus_info: %s\n",cap.bus_info);
        printf("version: %d\n",cap.version);
        printf("capabilities: %d\n",cap.capabilities);

        int flag = cap.capabilities;
        if(flag & V4L2_CAP_VIDEO_CAPTURE) printf("V4L2_CAP_VIDEO_CAPTURE: The device can capture video data.\n");
        if(flag & V4L2_CAP_VIDEO_OUTPUT) printf("V4L2_CAP_VIDEO_OUTPUT: The device can perform video output.\n");
        if(flag & V4L2_CAP_VIDEO_OVERLAY) printf("V4L2_CAP_VIDEO_OVERLAY: It can do video overlay onto the frame buffer.\n");
        if(flag & V4L2_CAP_VBI_CAPTURE) printf("V4L2_CAP_VBI_CAPTURE: It can capture raw video blanking interval data.\n");
        if(flag & V4L2_CAP_VBI_OUTPUT) printf("V4L2_CAP_VBI_OUTPUT: It can do raw VBI output.\n");
        if(flag & V4L2_CAP_SLICED_VBI_CAPTURE) printf("V4L2_CAP_SLICED_VBI_CAPTURE: It can do sliced VBI capture.\n");
        if(flag & V4L2_CAP_SLICED_VBI_OUTPUT) printf("V4L2_CAP_SLICED_VBI_OUTPUT: It can do sliced VBI output.\n");
        if(flag & V4L2_CAP_RDS_CAPTURE) printf("V4L2_CAP_RDS_CAPTURE: It can capture Radio Data System (RDS) data.\n");
        if(flag & V4L2_CAP_TUNER) printf("V4L2_CAP_TUNER: It has a computer-controllable tuner.\n");
        if(flag & V4L2_CAP_AUDIO) printf("V4L2_CAP_AUDIO: It can capture audio data.\n");
        if(flag & V4L2_CAP_RADIO) printf("V4L2_CAP_RADIO: It is a radio device.\n");
        if(flag & V4L2_CAP_READWRITE) printf("V4L2_CAP_READWRITE: It supports the read() and/or write() system calls; \
very few devices will support both. It makes little sense to write to a camera, normally.\n");
        if(flag & V4L2_CAP_ASYNCIO) printf("V4L2_CAP_ASYNCIO: It supports asynchronous I/O. Unfortunately, \
the V4L2 layer as a whole does not yet support asynchronous I/O, so this capability is not meaningful.\n");
        if(flag & V4L2_CAP_STREAMING) printf("V4L2_CAP_STREAMING: It supports ioctl()-controlled streaming I/O.\n");
    } else {
        if (errno != EINVAL) {
            perror ("VIDIOC_QUERYCAP");
            BOOST_ERROR("Could not request the capabilities");
        }
    }
}

/*
//  Used in the VIDIOC_QUERYCTRL ioctl for querying controls
struct v4l2_queryctrl {
	__u32		     id;
	enum v4l2_ctrl_type  type;
	__u8		     name[32];	// Whatever
	__s32		     minimum;	// Note signedness
	__s32		     maximum;
	__s32		     step;
	__s32		     default_value;
	__u32                flags;
	__u32		     reserved[2];
};

//  Used in the VIDIOC_QUERYMENU ioctl for querying menu items
struct v4l2_querymenu {
	__u32		id;
	__u32		index;
	__u8		name[32];	// Whatever
	__u32		reserved;
};

enum v4l2_ctrl_type {
	V4L2_CTRL_TYPE_INTEGER	     = 1,
	V4L2_CTRL_TYPE_BOOLEAN	     = 2,
	V4L2_CTRL_TYPE_MENU	         = 3,
	V4L2_CTRL_TYPE_BUTTON	     = 4,
	V4L2_CTRL_TYPE_INTEGER64     = 5,
	V4L2_CTRL_TYPE_CTRL_CLASS    = 6,
	V4L2_CTRL_TYPE_STRING        = 7,
};
*/

std::map<std::string, int> valid_ids;

BOOST_AUTO_TEST_CASE(read_controls) 
{
    printf("\nread_controls:\n");
    struct v4l2_queryctrl queryctrl;
    memset (&queryctrl, 0, sizeof (struct v4l2_queryctrl));
    valid_ids.clear();

    for (queryctrl.id = V4L2_CID_BASE; 
            queryctrl.id < V4L2_CID_LASTP1;
            queryctrl.id++) {
        // Read control informations if control is available.
        if (0 == ioctl (fd, VIDIOC_QUERYCTRL, &queryctrl)) {
            if (queryctrl.flags & V4L2_CTRL_FLAG_DISABLED)
                continue;

            char name_buffer[32];
            sprintf(name_buffer, "%s", queryctrl.name);
            std::string name_str(name_buffer);
            valid_ids.insert(std::pair<std::string, int>(name_str, (int)queryctrl.id));        
                       
            printf ("ID %d, Type %d, Name %s, Min %d, Max %d, Step %d, Default %d, Flags %d\n", 
                queryctrl.id - V4L2_CID_BASE,
                queryctrl.type,
                queryctrl.name,
                queryctrl.minimum,
                queryctrl.maximum,
                queryctrl.step,
                queryctrl.default_value,
                queryctrl.flags);

                // Read menue entries if available.
                if (queryctrl.type == V4L2_CTRL_TYPE_MENU) {
                    printf ("  Menu items:\n");
                    
                    struct v4l2_querymenu querymenu;
                    memset (&querymenu, 0, sizeof (querymenu));
                    querymenu.id = queryctrl.id;

                     for (int i = queryctrl.minimum; i <= queryctrl.maximum; ++i) {
                        querymenu.index = (uint32_t)i;
                        if (0 == ioctl (fd, VIDIOC_QUERYMENU, &querymenu)) {
                            printf ("  %s\n", querymenu.name);
                        } else {
                            perror ("VIDIOC_QUERYMENU");
                            BOOST_ERROR("Could not read menue");
                        }
                    }
                }
        } else {
            if (errno == EINVAL)
                    continue;

            perror ("VIDIOC_QUERYCTRL");
            BOOST_ERROR("Could not read the controls");
        }
    }
}

/*
struct v4l2_control {
	__u32		     id;
	__s32		     value;
};
*/
BOOST_AUTO_TEST_CASE(read_control_values) {
    printf("\nread_control_values:\n");

    struct v4l2_control control;
    memset(&control, 0, sizeof(struct v4l2_control));
    
    std::map<std::string, int>::iterator it;
    for(it=valid_ids.begin(); it != valid_ids.end(); it++) {
        control.id = it->second;

        if (-1 == ioctl (fd, VIDIOC_G_CTRL, &control)) {
            perror ("VIDIOC_G_CTRL");
            BOOST_ERROR("Could not read control value");
        } else {
            printf("%s: %d\n",it->first.c_str(), control.value);
        }
    }
} 

/* contrast is just an example for general setting */
BOOST_AUTO_TEST_CASE(write_control_value_contrast) {
    printf("\nwrite_control_values:\n");

    struct v4l2_control control;
    memset(&control, 0, sizeof(struct v4l2_control));
    control.id = V4L2_CID_CONTRAST;
    control.value = 5;
    
    if (-1 == ioctl (fd, VIDIOC_S_CTRL, &control)) {
        perror ("VIDIOC_S_CTRL");
        BOOST_ERROR("Could not write contrast 5");
    } else {
        printf("Set V4L2_CID_CONTRAST to 5.\n");
    }
} 

/*
enum v4l2_buf_type {
	V4L2_BUF_TYPE_VIDEO_CAPTURE        = 1,
	V4L2_BUF_TYPE_VIDEO_OUTPUT         = 2,
	V4L2_BUF_TYPE_VIDEO_OVERLAY        = 3,
	V4L2_BUF_TYPE_VBI_CAPTURE          = 4,
	V4L2_BUF_TYPE_VBI_OUTPUT           = 5,
	V4L2_BUF_TYPE_SLICED_VBI_CAPTURE   = 6,
	V4L2_BUF_TYPE_SLICED_VBI_OUTPUT    = 7,
#if 1
	// Experimental
	V4L2_BUF_TYPE_VIDEO_OUTPUT_OVERLAY = 8,
#endif
	V4L2_BUF_TYPE_PRIVATE              = 0x80,
};

struct v4l2_format {
	enum v4l2_buf_type type;
	union {
		struct v4l2_pix_format pix;             // V4L2_BUF_TYPE_VIDEO_CAPTURE
		struct v4l2_window win;                 // V4L2_BUF_TYPE_VIDEO_OVERLAY
		struct v4l2_vbi_format vbi;             // V4L2_BUF_TYPE_VBI_CAPTURE
		struct v4l2_sliced_vbi_format sliced;   // V4L2_BUF_TYPE_SLICED_VBI_CAPTURE
		__u8	raw_data[200];                  // user-defined
	} fmt;
};

struct v4l2_pix_format {
	__u32 width;
	__u32 height;
	__u32 pixelformat;
	enum v4l2_field field;
	__u32 bytesperline;	                // for padding, zero if unused
	__u32 sizeimage;
	enum v4l2_colorspace colorspace;
	__u32 priv;		                    // private data, depends on pixelformat
};
*/
BOOST_AUTO_TEST_CASE(request_image_format) {
    printf("\nrequest_image_format:\n");

    struct v4l2_format format;
    memset(&format, 0, sizeof(struct v4l2_format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // set format for request
    if(0 == ioctl(fd, VIDIOC_G_FMT, &format)) {

        struct v4l2_pix_format pix = format.fmt.pix;
        unsigned char c1 =  pix.pixelformat & 0xFF;
        unsigned char c2 =  (pix.pixelformat >> 8) & 0xFF;
        unsigned char c3 =  (pix.pixelformat >> 16) & 0xFF;
        unsigned char c4 =  (pix.pixelformat >> 24) & 0xFF;

        printf("width: %d\n", pix.width);
        printf("height: %d\n",pix.height);
        printf("pixelformat: %c %c %c %c\n", c1, c2, c3, c4);
        printf("v4l2_field: %d\n", pix.field);
        printf("bytes per line: %d\n", pix.bytesperline);
        printf("sizeimage: %d\n", pix.sizeimage);
        printf("v4l2_colorspace: %d\n", pix.colorspace);
        printf("priv: %d\n", pix.priv);

    } else if(errno != EINVAL) {
        perror ("VIDIOC_G_FMT");
        BOOST_ERROR("Could not request image format.");
    } else {
        printf("format type not supported");
    }
}
#endif

BOOST_AUTO_TEST_CASE(capability_test) 
{
    std::cout << std::endl;
    camera::CamConfig* cam_config;

    BOOST_REQUIRE_NO_THROW(cam_config = new camera::CamConfig("/dev/video0"));
    BOOST_REQUIRE_NO_THROW(cam_config->readCapability());

    cam_config->listCapabilities();

    BOOST_CHECK(cam_config->hasCapability(V4L2_CAP_VIDEO_CAPTURE) == true);
    BOOST_CHECK(cam_config->hasCapability(V4L2_CAP_STREAMING) == true);
    BOOST_CHECK(cam_config->hasCapability(V4L2_CAP_AUDIO) == false);
    BOOST_CHECK(cam_config->hasCapability(0xAABBCCDD) == false);
    
    delete cam_config;    
}

BOOST_AUTO_TEST_CASE(control_test) 
{
    std::cout << std::endl;
    camera::CamConfig* cam_config;

    BOOST_REQUIRE_NO_THROW(cam_config = new camera::CamConfig("/dev/video0"));
    BOOST_REQUIRE_NO_THROW(cam_config->readControl());
    
    cam_config->listControls();
    
    std::vector<uint32_t> ids = cam_config->getControlValidIDs();
    uint32_t unknown_id = 1;    
    uint32_t unknown_flag = 0x0003;
    uint32_t known_flag = V4L2_CTRL_FLAG_READ_ONLY;

    enum v4l2_ctrl_type type;
    std::string name;
    int32_t minimum;
    int32_t maximum;
    int32_t step;
    int32_t default_value;
    bool set;

    BOOST_REQUIRE_THROW(cam_config->readControlValue(unknown_id), std::runtime_error);
    BOOST_REQUIRE_THROW(cam_config->writeControlValue(unknown_id, 0), std::runtime_error);
    BOOST_CHECK(cam_config->isControlIdValid(unknown_id) == false);
    BOOST_CHECK(cam_config->getControlType(unknown_id, &type) == false);
    BOOST_CHECK(cam_config->getControlName(unknown_id, &name) == false);
    BOOST_CHECK(cam_config->getControlMinimum(unknown_id, &minimum) == false);
    BOOST_CHECK(cam_config->getControlMaximum(unknown_id, &maximum) == false);
    BOOST_CHECK(cam_config->getControlStep(unknown_id, &step)  == false);
    BOOST_CHECK(cam_config->getControlDefaultValue(unknown_id, &default_value) == false);
    BOOST_CHECK(cam_config->getControlFlag(unknown_id, known_flag, &set)  == false);

    for(uint32_t i=0; i < ids.size(); i++) {
        uint32_t id = ids[i];
        int value = 0;
        BOOST_REQUIRE_NO_THROW(value = cam_config->readControlValue(id));
        // V4L2_CID_WHITE_BALANCE_TEMPERATURE can not be set... why?
        if(id != V4L2_CID_WHITE_BALANCE_TEMPERATURE)
            BOOST_REQUIRE_NO_THROW(cam_config->writeControlValue(id, value));
        BOOST_CHECK(cam_config->isControlIdValid(id) == true);
        BOOST_CHECK(cam_config->getControlType(id, &type) == true);
        BOOST_CHECK(cam_config->getControlName(id, &name) == true);
        BOOST_CHECK(cam_config->getControlMinimum(id, &minimum) == true);
        BOOST_CHECK(cam_config->getControlMaximum(id, &maximum) == true);
        BOOST_CHECK(cam_config->getControlStep(id, &step)  == true);
        BOOST_CHECK(cam_config->getControlDefaultValue(id, &default_value) == true);
        BOOST_CHECK(cam_config->getControlFlag(id, unknown_flag, &set)  == false);
        BOOST_CHECK(cam_config->getControlFlag(id, known_flag, &set)  == true);
    }
    delete cam_config;    
}
/*
 * \file    v4l2_test.h
 *  
 * \brief   Boost tests for class CamConfig.
 *   
 *          German Research Center for Artificial Intelligence\n
 *          Project: Rimres
 *
 * \date    23.11.11
 *
 * \author  Stefan.Haase@dfki.de
 */

BOOST_AUTO_TEST_CASE(image_test) 
{
    std::cout << std::endl;
    camera::CamConfig* cam_config;
    BOOST_REQUIRE_NO_THROW(cam_config = new camera::CamConfig("/dev/video0"));

    // Takes care that the device driver returns the correct image size.
    BOOST_REQUIRE_NO_THROW(cam_config->writeImagePixelFormat());

    BOOST_REQUIRE_NO_THROW(cam_config->readImageFormat());
    uint32_t width = 0;
    BOOST_CHECK(cam_config->getImageWidth(&width) == true);

    cam_config->listImageFormat();

    // Change image size.
    cam_config->writeImagePixelFormat(1024, 768);
    std::cout << std::endl;
    cam_config->listImageFormat();

    // Change pixel format.
    cam_config->writeImagePixelFormat(640, 480, V4L2_PIX_FMT_MJPEG);
    std::cout << std::endl;
    cam_config->listImageFormat();

    delete cam_config;  
}

BOOST_AUTO_TEST_CASE(stream_test) 
{
    std::cout << std::endl;
    camera::CamConfig* cam_config;
    BOOST_REQUIRE_NO_THROW(cam_config = new camera::CamConfig("/dev/video0"));
    
    BOOST_REQUIRE_NO_THROW(cam_config->readStreamparm());
    cam_config->listStreamparm();
    std::cout << std::endl;

    BOOST_CHECK(cam_config->hasCapabilityStreamparm(V4L2_CAP_TIMEPERFRAME) == true);
    BOOST_CHECK(cam_config->hasCapturemodeStreamparm(V4L2_MODE_HIGHQUALITY) == false);

    BOOST_REQUIRE_NO_THROW(cam_config->writeStreamparm(1, 20));
    cam_config->listStreamparm();
  
    delete cam_config;  
}

BOOST_AUTO_TEST_SUITE_END()

#endif
