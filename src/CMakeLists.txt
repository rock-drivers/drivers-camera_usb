rock_library(camera_usb
    SOURCES cam_config.cpp cam_gst.cpp cam_usb.cpp
    HEADERS cam_config.h cam_gst.h cam_usb.h omap_v4l2.h
    DEPS_PKGCONFIG base-lib camera_interface 
    DEPS_PKGCONFIG gstreamer-0.10 gstreamer-plugins-base-0.10 gstreamer-app-0.10
)
target_link_libraries(camera_usb pthread)

rock_executable(camera_usb_control 
    SOURCES main_test.cpp
    DEPS_PKGCONFIG base-lib camera_interface opencv
    DEPS camera_usb)
