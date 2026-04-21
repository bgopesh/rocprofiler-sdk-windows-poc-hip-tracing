cmake_minimum_required(VERSION 3.20)

# Create mock-tool-2 shared library
add_library(mock-tool-2 SHARED
    mock_tool_2.cpp
)

# Include directories
target_include_directories(mock-tool-2
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Set C++ standard
set_property(TARGET mock-tool-2 PROPERTY CXX_STANDARD 17)

# Windows-specific: Do not export all symbols automatically
# Only export what's explicitly marked with __declspec(dllexport)
set_target_properties(mock-tool-2 PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)

# Output to standard location
set_target_properties(mock-tool-2 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/mock-tool-2/$<CONFIG>
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/mock-tool-2/$<CONFIG>
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/mock-tool-2/$<CONFIG>
)

message(STATUS "Configured mock-tool-2 - Second tool for multi-tool testing")
