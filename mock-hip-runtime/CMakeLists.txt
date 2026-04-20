#=============================================================================
# Mock HIP Runtime - Windows Build Configuration
#
# This component simulates HIP runtime behavior and links ONLY to the
# registration layer. It does NOT link to the SDK - the SDK is loaded
# dynamically by rocprofiler-register at runtime.
#
# ARCHITECTURE:
# - Provides public HIP API (hipInit, hipMalloc, hipFree, hipMemcpy, etc.)
# - Links ONLY to mock-rocprofiler-register (not to SDK)
# - Calls rocprofiler_register_library_api_table() during hipInit()
# - Uses dispatch table that tools can wrap via rocprofiler_set_api_table()
#=============================================================================

add_library(mock-hip-runtime SHARED
    mock_hip_runtime.cpp
    mock_hip_runtime.h
)

target_include_directories(mock-hip-runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/mock-rocprofiler-register
)

# CRITICAL: Link ONLY to rocprofiler-register, NOT to the SDK
# The SDK is loaded dynamically at runtime via rocprofiler-register
target_link_libraries(mock-hip-runtime
    PRIVATE
        mock-rocprofiler-register
)

set_target_properties(mock-hip-runtime PROPERTIES
    OUTPUT_NAME "amdhip64"
    VERSION ${PROJECT_VERSION}
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Mock HIP Runtime Configuration")
message(STATUS "========================================")
message(STATUS "  Target:           mock-hip-runtime")
message(STATUS "  Output Name:      amdhip64.dll")
message(STATUS "  Links to:         rocprofiler-register ONLY")
message(STATUS "  C++ Standard:     17")
message(STATUS "")
message(STATUS "NOTE: This library does NOT link to the SDK.")
message(STATUS "      The SDK is loaded dynamically at runtime")
message(STATUS "      by rocprofiler-register.")
message(STATUS "========================================")
message(STATUS "")
