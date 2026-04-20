#pragma once

//=============================================================================
// Mock HIP Runtime - Public API
//
// This is the HIP runtime layer that applications link against.
// It provides the public HIP API and uses rocprofiler-register for tool
// discovery and dispatch table registration.
//
// IMPORTANT: This library ONLY links to rocprofiler-register, NOT to the SDK.
// The SDK is loaded dynamically by rocprofiler-register at runtime.
//=============================================================================

#include <cstdint>

// Forward declarations for dim3
struct dim3
{
    uint32_t x, y, z;
    dim3(uint32_t _x = 1, uint32_t _y = 1, uint32_t _z = 1) : x(_x), y(_y), z(_z) {}
};

// Mock HIP error codes
enum hipError_t
{
    hipSuccess = 0,
    hipErrorInvalidValue = 1,
    hipErrorMemoryAllocation = 2,
    hipErrorInvalidHandle = 3
};

// Mock HIP memory copy kinds
enum hipMemcpyKind
{
    hipMemcpyHostToHost = 0,
    hipMemcpyHostToDevice = 1,
    hipMemcpyDeviceToHost = 2,
    hipMemcpyDeviceToDevice = 3,
    hipMemcpyDefault = 4
};

//=============================================================================
// HIP Dispatch Table Structure
//
// This is the internal function pointer table that rocprofiler-register
// passes to profiling tools. Tools can wrap these function pointers to
// intercept HIP API calls.
//=============================================================================

struct HipDispatchTable
{
    uint64_t size;  // Must be first field - sizeof(HipDispatchTable)

    // Memory management APIs
    hipError_t (*hipMalloc)(void** ptr, size_t size);
    hipError_t (*hipFree)(void* ptr);
    hipError_t (*hipMemcpy)(void* dst, const void* src, size_t count, hipMemcpyKind kind);

    // Kernel launch API
    hipError_t (*hipLaunchKernel)(const void* func, dim3 gridDim, dim3 blockDim,
                                   void** args, size_t sharedMem, void* stream);
};

//=============================================================================
// Public HIP API - Exported Functions
//
// These are the functions that applications call.
// On Windows, we use __declspec(dllexport) to export them from the DLL.
//=============================================================================

#ifdef _WIN32
#    define HIP_API __declspec(dllexport)
#else
#    define HIP_API __attribute__((visibility("default")))
#endif

extern "C" {
// Initialization - this triggers registration with rocprofiler-register
HIP_API hipError_t hipInit(unsigned int flags);

// Memory management
HIP_API hipError_t hipMalloc(void** ptr, size_t size);
HIP_API hipError_t hipFree(void* ptr);
HIP_API hipError_t hipMemcpy(void* dst, const void* src, size_t count, hipMemcpyKind kind);

// Kernel launch
HIP_API hipError_t hipLaunchKernel(const void* func, dim3 gridDim, dim3 blockDim,
                                    void** args, size_t sharedMem, void* stream);

// Device synchronization
HIP_API hipError_t hipDeviceSynchronize(void);
}
