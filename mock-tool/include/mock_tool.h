#pragma once

#include <cstdint>

//=============================================================================
// Mock Tool Public API
//=============================================================================
// This header defines the tool's exported interface.
// In this POC, the only exported symbol is rocprofiler_configure(),
// which is called by rocprofiler-register to initialize the tool.
//=============================================================================

#ifdef _WIN32
#    define TOOL_API __declspec(dllexport)
#else
#    define TOOL_API __attribute__((visibility("default")))
#endif

extern "C" {

// Tool initialization function
// Called by rocprofiler-register when the tool is discovered
//
// Parameters:
//   version          - ROCProfiler SDK version
//   runtime_version  - HIP runtime version string
//   priority         - Tool priority (for multiple tools)
//   client_id        - Opaque client identifier
//
// Returns:
//   Tool configuration handle (can be nullptr for simple tools)
//
TOOL_API void* rocprofiler_configure(uint32_t version,
                                     const char* runtime_version,
                                     uint32_t priority,
                                     void* client_id);

}  // extern "C"
