rock_library(camera_usb
    SOURCES Dummy.cpp
    HEADERS Dummy.hpp)

rock_executable(camera_usb_bin Main.cpp
    DEPS camera_usb)

