rock_library(camera_usb
    SOURCES cam_config.cpp cam_gst.cpp cam_usb.cpp
    HEADERS cam_config.h cam_gst.h cam_usb.h
    DEPS_PKGCONFIG base-lib
    DEPS_PKGCONFIG camera_interface gst_dsp_arm
)

add_subdirectory(tests)
