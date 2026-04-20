//=============================================================================
// Mock HIP Runtime - Implementation
//
// This simulates the HIP runtime library on Windows.
//
// ARCHITECTURE:
// - This library links ONLY to rocprofiler-register (not to the SDK)
// - During hipInit(), it calls rocprofiler_register_library_api_table()
// - The registration layer may load profiling tools that wrap the dispatch table
// - All public HIP APIs call through the dispatch table (which may be wrapped)
//
// This matches the real HIP runtime behavior on Linux/Windows.
//=============================================================================

#include "mock_hip_runtime.h"
#include "mock_register.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

// Define the import function for the HIP runtime
// This allows profiling tools to import the original (unwrapped) dispatch table
ROCPROFILER_REGISTER_DEFINE_IMPORT(hip, ROCPROFILER_REGISTER_COMPUTE_VERSION_3(6, 4, 0))

namespace
{
//=============================================================================
// Internal State
//=============================================================================

// Original dispatch table (unmodified, always points to mock implementations)
HipDispatchTable original_table = {};

// Profiler-modifiable dispatch table (what the application uses)
// This may be modified by rocprofiler-sdk to insert wrappers
HipDispatchTable profiler_table = {};

// Thread-safe initialization
std::once_flag init_flag;

// Mock allocations for tracking (simple counter)
uint64_t allocation_counter = 0;

//=============================================================================
// Mock HIP Function Implementations
//
// These are the actual HIP runtime implementations.
// They simulate GPU operations using host memory and print trace messages.
//=============================================================================

hipError_t
mock_hipMalloc_impl(void** ptr, size_t size)
{
    printf("    [HIP Runtime] hipMalloc(%zu bytes)\n", size);
    *ptr = malloc(size);
    if(*ptr == nullptr)
    {
        printf("    [HIP Runtime] hipMalloc FAILED - malloc returned NULL\n");
        return hipErrorMemoryAllocation;
    }
    allocation_counter++;
    return hipSuccess;
}

hipError_t
mock_hipFree_impl(void* ptr)
{
    printf("    [HIP Runtime] hipFree(%p)\n", ptr);
    if(ptr == nullptr)
    {
        printf("    [HIP Runtime] hipFree WARNING - freeing NULL pointer\n");
        return hipErrorInvalidHandle;
    }
    free(ptr);
    return hipSuccess;
}

hipError_t
mock_hipMemcpy_impl(void* dst, const void* src, size_t count, hipMemcpyKind kind)
{
    const char* kind_str = "Unknown";
    switch(kind)
    {
        case hipMemcpyHostToHost: kind_str = "HostToHost"; break;
        case hipMemcpyHostToDevice: kind_str = "HostToDevice"; break;
        case hipMemcpyDeviceToHost: kind_str = "DeviceToHost"; break;
        case hipMemcpyDeviceToDevice: kind_str = "DeviceToDevice"; break;
        case hipMemcpyDefault: kind_str = "Default"; break;
    }
    printf("    [HIP Runtime] hipMemcpy(%zu bytes, kind=%s)\n", count, kind_str);

    if(dst == nullptr || src == nullptr)
    {
        printf("    [HIP Runtime] hipMemcpy FAILED - NULL pointer\n");
        return hipErrorInvalidValue;
    }

    memcpy(dst, src, count);
    return hipSuccess;
}

hipError_t
mock_hipLaunchKernel_impl(const void*  func,
                          dim3         gridDim,
                          dim3         blockDim,
                          void**       args,
                          size_t       sharedMem,
                          void*        stream)
{
    printf("    [HIP Runtime] hipLaunchKernel(func=%p, grid=(%u,%u,%u), block=(%u,%u,%u), "
           "sharedMem=%zu, stream=%p)\n",
           func,
           gridDim.x,
           gridDim.y,
           gridDim.z,
           blockDim.x,
           blockDim.y,
           blockDim.z,
           sharedMem,
           stream);

    // Mock kernel launch - just print and return success
    printf("    [HIP Runtime] Kernel launch simulated\n");
    return hipSuccess;
}

//=============================================================================
// Dispatch Table Construction and Registration
//=============================================================================

void
construct_dispatch_table()
{
    printf("  [HIP Runtime] Constructing dispatch table...\n");

    // Fill original table with mock implementations
    original_table.size            = sizeof(HipDispatchTable);
    original_table.hipMalloc       = &mock_hipMalloc_impl;
    original_table.hipFree         = &mock_hipFree_impl;
    original_table.hipMemcpy       = &mock_hipMemcpy_impl;
    original_table.hipLaunchKernel = &mock_hipLaunchKernel_impl;

    // Copy to profiler table (profilers can modify this)
    profiler_table = original_table;

    printf("  [HIP Runtime] Dispatch table constructed (size=%llu bytes)\n",
           (unsigned long long) original_table.size);
}

void
register_with_rocprofiler()
{
    printf("  [HIP Runtime] Registering with rocprofiler-register...\n");

    // Construct dispatch table
    construct_dispatch_table();

    // Get the table pointer as void* (profiler can modify this)
    void* profiler_table_ptr = static_cast<void*>(&profiler_table);

    // Register with rocprofiler-register
    // The library name "hip" and version match the DEFINE_IMPORT
    rocprofiler_register_library_indentifier_t lib_id = {};
    rocprofiler_register_error_code_t result = rocprofiler_register_library_api_table(
        "hip",                                            // Library name
        &ROCPROFILER_REGISTER_IMPORT_FUNC(hip),           // Import function
        ROCPROFILER_REGISTER_COMPUTE_VERSION_3(6, 4, 0),  // Version
        &profiler_table_ptr,                              // Dispatch table pointer
        1,                                                // Number of tables
        &lib_id);                                         // Registration ID

    if(result == ROCP_REG_SUCCESS)
    {
        printf("  [HIP Runtime] Registration successful! ID: %llu\n",
               (unsigned long long) lib_id.handle);
    }
    else
    {
        printf("  [HIP Runtime] Registration failed with code: %d\n", result);
        printf("  [HIP Runtime] Error: %s\n", rocprofiler_register_error_string(result));
    }
}

void
initialize_hip()
{
    printf("[HIP Runtime] Initializing HIP runtime...\n");
    register_with_rocprofiler();
    printf("[HIP Runtime] Initialization complete\n");
}

}  // namespace

//=============================================================================
// Public HIP API Implementations
//
// These are the exported functions that applications call.
// They delegate to the profiler_table, which may have been wrapped by tools.
//=============================================================================

extern "C" {

HIP_API hipError_t
hipInit(unsigned int flags)
{
    (void) flags;
    printf("\n[HIP API] hipInit(flags=%u)\n", flags);
    std::call_once(init_flag, initialize_hip);
    return hipSuccess;
}

HIP_API hipError_t
hipMalloc(void** ptr, size_t size)
{
    // Call through the profiler table (which may have been wrapped by tracer)
    return profiler_table.hipMalloc(ptr, size);
}

HIP_API hipError_t
hipFree(void* ptr)
{
    return profiler_table.hipFree(ptr);
}

HIP_API hipError_t
hipMemcpy(void* dst, const void* src, size_t count, hipMemcpyKind kind)
{
    return profiler_table.hipMemcpy(dst, src, count, kind);
}

HIP_API hipError_t
hipLaunchKernel(const void* func,
                dim3        gridDim,
                dim3        blockDim,
                void**      args,
                size_t      sharedMem,
                void*       stream)
{
    return profiler_table.hipLaunchKernel(func, gridDim, blockDim, args, sharedMem, stream);
}

HIP_API hipError_t
hipDeviceSynchronize(void)
{
    printf("[HIP API] hipDeviceSynchronize()\n");
    // Mock implementation - just return success
    return hipSuccess;
}

}  // extern "C"
