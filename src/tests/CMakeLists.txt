set(CMAKE_BUILD_TYPE debug)

rock_executable(main-test 
    SOURCES main_test.cpp
    DEPS_PKGCONFIG base-lib camera_interface opencv
    DEPS camera_usb)

add_subdirectory(unittests)
