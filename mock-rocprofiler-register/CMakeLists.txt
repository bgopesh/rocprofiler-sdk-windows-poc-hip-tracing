#=============================================================================
# Mock rocprofiler-register - Windows Build Configuration
#
# This builds a Windows DLL that implements the rocprofiler-register API.
# The real version would be part of ROCm installation, but this mock
# demonstrates the Windows-specific implementation patterns.
#
# Key Windows-specific aspects:
# - Links to psapi.lib for EnumProcessModules
# - Uses SHARED library type to generate DLL
# - Exports symbols via __declspec(dllexport) in headers
# - Requires C++14 for std::call_once and other features
#=============================================================================

cmake_minimum_required(VERSION 3.16)

#-----------------------------------------------------------------------------
# Build the mock-rocprofiler-register library
#-----------------------------------------------------------------------------

add_library(mock-rocprofiler-register SHARED
    mock_register.cpp
    mock_register.h
)

# Set output name to match ROCm convention
set_target_properties(mock-rocprofiler-register PROPERTIES
    OUTPUT_NAME "rocprofiler-register"
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
)

#-----------------------------------------------------------------------------
# Windows-Specific: Link to psapi.lib for EnumProcessModules
#-----------------------------------------------------------------------------

if(WIN32)
    target_link_libraries(mock-rocprofiler-register
        PRIVATE
            psapi  # For EnumProcessModules, GetModuleFileNameA
    )
endif()

#-----------------------------------------------------------------------------
# Include Directories
#-----------------------------------------------------------------------------

target_include_directories(mock-rocprofiler-register
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

#-----------------------------------------------------------------------------
# Compiler Warnings (Good practice for development)
#-----------------------------------------------------------------------------

if(MSVC)
    target_compile_options(mock-rocprofiler-register PRIVATE
        /W4          # Warning level 4
        /WX-         # Do not treat warnings as errors (relaxed for POC)
        /wd4100      # Disable "unreferenced formal parameter" warning
    )
else()
    target_compile_options(mock-rocprofiler-register PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()

#-----------------------------------------------------------------------------
# Print Configuration Summary
#-----------------------------------------------------------------------------

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Mock ROCProfiler-Register Configuration")
message(STATUS "========================================")
message(STATUS "  Target:           mock-rocprofiler-register")
message(STATUS "  Output Name:      rocprofiler-register.dll")
message(STATUS "  C++ Standard:     14")
message(STATUS "  Build Type:       ${CMAKE_BUILD_TYPE}")
message(STATUS "")
message(STATUS "Windows-Specific Features:")
message(STATUS "  - EnumProcessModules for module enumeration")
message(STATUS "  - LoadLibraryA for dynamic library loading")
message(STATUS "  - GetProcAddress for symbol resolution")
message(STATUS "  - GetEnvironmentVariableA for env var access")
message(STATUS "========================================")
message(STATUS "")
