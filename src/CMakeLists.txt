rock_library(camera_usb
    SOURCES cam_config.cpp cam_gst.cpp cam_usb.cpp
    HEADERS cam_config.h cam_gst.h cam_usb.h
    DEPS_PKGCONFIG camera_interface gst_dsp_arm)

rock_executable(camera_usb_bin 
    SOURCES Main.cpp
    DEPS camera_usb)

add_subdirectory(tests)
