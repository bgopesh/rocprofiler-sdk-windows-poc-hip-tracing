#pragma once

#include <cstdint>

// Windows DLL export macro
#ifdef _WIN32
#    define ROCPROFILER_SDK_API __declspec(dllexport)
#else
#    define ROCPROFILER_SDK_API __attribute__((visibility("default")))
#endif

// Status codes
enum rocprofiler_status_t
{
    ROCPROFILER_STATUS_SUCCESS = 0,
    ROCPROFILER_STATUS_ERROR   = -1
};

// Context handle (opaque pointer)
struct rocprofiler_context_s;
typedef struct rocprofiler_context_s* rocprofiler_context_id_t;

// Buffer handle (opaque pointer)
struct rocprofiler_buffer_s;
typedef struct rocprofiler_buffer_s* rocprofiler_buffer_id_t;

// Callback types
typedef void (*rocprofiler_buffer_callback_t)(rocprofiler_context_id_t context,
                                               rocprofiler_buffer_id_t   buffer,
                                               void*                     user_data);

// API table modification callback
typedef int (*rocprofiler_set_api_table_t)(const char* name,
                                           uint64_t    lib_version,
                                           uint64_t    lib_instance,
                                           void**      tables,
                                           uint64_t    num_tables);

extern "C" {

// Configuration function (called by rocprofiler-register to discover SDK)
// This is the primary entry point that rocprofiler-register searches for
ROCPROFILER_SDK_API void*
rocprofiler_configure(uint32_t    version,
                      const char* runtime_version,
                      uint32_t    priority,
                      void*       client_id);

// Core SDK APIs
ROCPROFILER_SDK_API rocprofiler_status_t
rocprofiler_create_context(rocprofiler_context_id_t* context);

ROCPROFILER_SDK_API rocprofiler_status_t
rocprofiler_start_context(rocprofiler_context_id_t context);

ROCPROFILER_SDK_API rocprofiler_status_t
rocprofiler_stop_context(rocprofiler_context_id_t context);

ROCPROFILER_SDK_API rocprofiler_status_t
rocprofiler_destroy_context(rocprofiler_context_id_t context);

// Buffer tracing APIs
ROCPROFILER_SDK_API rocprofiler_status_t
rocprofiler_create_buffer(rocprofiler_context_id_t       context,
                          size_t                         buffer_size,
                          rocprofiler_buffer_callback_t  callback,
                          void*                          user_data,
                          rocprofiler_buffer_id_t*       buffer);

ROCPROFILER_SDK_API rocprofiler_status_t
rocprofiler_configure_buffer_tracing(rocprofiler_context_id_t context,
                                      rocprofiler_buffer_id_t  buffer);

// API table interception hook
// This function is exported and searched for by rocprofiler-register
// It allows the SDK to intercept and modify HIP/HSA dispatch tables
ROCPROFILER_SDK_API int
rocprofiler_set_api_table(const char* name,
                          uint64_t    lib_version,
                          uint64_t    lib_instance,
                          void**      tables,
                          uint64_t    num_tables);

}  // extern "C"
