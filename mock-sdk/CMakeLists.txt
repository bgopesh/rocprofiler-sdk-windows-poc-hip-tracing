# Mock ROCProfiler SDK - Windows Runtime-Loadable Implementation
# Provides rocprofiler_configure() and basic SDK APIs for runtime discovery

add_library(mock-sdk SHARED
    mock_rocprofiler_sdk.cpp
    mock_rocprofiler_sdk.h
)

# Include directory for header
target_include_directories(mock-sdk
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# No dependencies - completely standalone DLL
# This ensures the SDK can be loaded via LoadLibraryA without dependency issues
target_link_libraries(mock-sdk
    PRIVATE
        # Windows system libraries only
)

# Output name matches expected SDK library name
set_target_properties(mock-sdk PROPERTIES
    OUTPUT_NAME "rocprofiler-sdk"
    VERSION ${PROJECT_VERSION}
    # Ensure all symbols are exported
    WINDOWS_EXPORT_ALL_SYMBOLS OFF  # We use __declspec(dllexport) explicitly
)

# Install to build/bin for easy testing
install(TARGETS mock-sdk
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
)
