# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Set version information
set(PROJECT_VER "1.0.0")
string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d")
set(PROJECT_VER_FULL "${PROJECT_VER}_${BUILD_TIMESTAMP}")

project(six-iot-sdk)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/main/version.h.in
    ${CMAKE_CURRENT_SOURCE_DIR}/main/version.h
)

# Option 1: Best solution - Use POST_BUILD on the app target
add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
        "${CMAKE_BINARY_DIR}/${PROJECT_NAME}.bin"
        "${CMAKE_BINARY_DIR}/${PROJECT_NAME}_v${PROJECT_VER_FULL}.bin"
    COMMENT "Creating versioned binary: ${PROJECT_NAME}_v${PROJECT_VER_FULL}.bin"
)
