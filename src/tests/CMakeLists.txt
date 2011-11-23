set(CMAKE_BUILD_TYPE debug)

set(SOURCES main_test.cpp)
set(MAIN_TEST main-test)
add_executable(${MAIN_TEST} ${SOURCES})

target_link_libraries(${MAIN_TEST} ${PROJECT_NAME} ${EXTERNAL_LIBS})

add_subdirectory(unittests)
