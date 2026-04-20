cmake_minimum_required(VERSION 3.20)
project(rocprofiler-hip-tracing-poc-v2 VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add subdirectories in dependency order
add_subdirectory(mock-rocprofiler-register)
add_subdirectory(mock-hip-runtime)
add_subdirectory(mock-sdk)
add_subdirectory(mock-tool)
add_subdirectory(test-hip-app)
