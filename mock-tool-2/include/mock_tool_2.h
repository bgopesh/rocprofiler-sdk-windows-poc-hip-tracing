#pragma once

#include <cstdint>

#ifdef _WIN32
#    define TOOL_API __declspec(dllexport)
#else
#    define TOOL_API __attribute__((visibility("default")))
#endif

extern "C" {

/**
 * @brief Tool configuration entry point - TOOL 2 VERSION
 *
 * This function is searched for and called by rocprofiler-register
 * during tool discovery. Multiple tools can export this symbol.
 *
 * @param version ROCProfiler API version
 * @param runtime_version Runtime version string
 * @param priority Tool priority (lower = higher priority)
 * @param client_id Output parameter for client ID
 * @return Opaque pointer (implementation defined)
 */
TOOL_API void* rocprofiler_configure(uint32_t    version,
                                      const char* runtime_version,
                                      uint32_t    priority,
                                      void*       client_id);

}  // extern "C"
