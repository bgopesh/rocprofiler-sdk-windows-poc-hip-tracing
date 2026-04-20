cmake_minimum_required(VERSION 3.16)

# POC v2: Clean HIP Test Application
# This application ONLY links to mock-hip-runtime
# No SDK, no register, no tool dependencies

add_executable(test-hip-app
    test_app.cpp)

# Only link to the HIP runtime library
# The app doesn't know about rocprofiler-sdk, rocprofiler-register, or mock-tool
target_link_libraries(test-hip-app
    PRIVATE mock-hip-runtime)

# Set C++ standard
set_target_properties(test-hip-app PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)

# Copy mock-hip-runtime.dll to the output directory on Windows
if(WIN32)
    add_custom_command(TARGET test-hip-app POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:mock-hip-runtime>
            $<TARGET_FILE_DIR:test-hip-app>
        COMMENT "Copying mock-hip-runtime.dll to test-hip-app directory")
endif()

message(STATUS "Configured test-hip-app (POC v2) - Links ONLY to mock-hip-runtime")
