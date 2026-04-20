# Mock Tool Library
# This component demonstrates a tool that uses the mock SDK
# It implements rocprofiler_configure() which is the tool entry point

add_library(mock-tool SHARED
    mock_tool.cpp
)

target_include_directories(mock-tool
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(mock-tool
    PRIVATE
        mock-sdk
)

set_target_properties(mock-tool PROPERTIES
    OUTPUT_NAME "mock-tool"
    VERSION ${PROJECT_VERSION}
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)
