set(CMAKE_BUILD_TYPE debug)

rock_executable(main-test 
    SOURCES main_test.cpp
    DEPS camera_usb)

add_subdirectory(unittests)
