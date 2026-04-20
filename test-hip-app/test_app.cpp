#include <cstdio>
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

// Import HIP API functions from mock-hip-runtime.dll
// The app ONLY knows about HIP runtime - no SDK, no register, no tool
#ifdef _WIN32
#    define HIP_API __declspec(dllimport)
#else
#    define HIP_API __attribute__((visibility("default")))
#endif

extern "C" {
// Initialization
HIP_API hipError_t hipInit(unsigned int flags);

// Memory management
HIP_API hipError_t hipMalloc(void** ptr, size_t size);
HIP_API hipError_t hipFree(void* ptr);
HIP_API hipError_t hipMemcpy(void* dst, const void* src, size_t count, hipMemcpyKind kind);

// Kernel launch
HIP_API hipError_t hipLaunchKernel(const void* func, dim3 gridDim, dim3 blockDim,
                                    void** args, size_t sharedMem, void* stream);

// Synchronization
HIP_API hipError_t hipDeviceSynchronize();
}

// Mock kernel function (just a dummy pointer for testing)
void mock_kernel() {}

int main()
{
    printf("=== POC v2: Clean HIP Application ===\n");
    printf("This app ONLY links to mock-hip-runtime\n");
    printf("No SDK, no register, no tool dependencies\n\n");

    hipError_t err;

    // 1. Initialize HIP
    printf("1. Calling hipInit...\n");
    err = hipInit(0);
    if(err != hipSuccess)
    {
        printf("   ERROR: hipInit failed with code %d\n", err);
        return 1;
    }
    printf("   SUCCESS: HIP initialized\n\n");

    // 2. Allocate device memory
    printf("2. Calling hipMalloc...\n");
    void* d_ptr = nullptr;
    size_t alloc_size = 1024 * 1024;  // 1 MB
    err = hipMalloc(&d_ptr, alloc_size);
    if(err != hipSuccess)
    {
        printf("   ERROR: hipMalloc failed with code %d\n", err);
        return 1;
    }
    printf("   SUCCESS: Allocated %zu bytes at %p\n\n", alloc_size, d_ptr);

    // 3. Prepare host data and copy to device
    printf("3. Calling hipMemcpy (Host -> Device)...\n");
    constexpr size_t copy_size = 256;
    uint8_t h_data[copy_size];
    for(size_t i = 0; i < copy_size; ++i)
    {
        h_data[i] = static_cast<uint8_t>(i);
    }
    err = hipMemcpy(d_ptr, h_data, copy_size, hipMemcpyHostToDevice);
    if(err != hipSuccess)
    {
        printf("   ERROR: hipMemcpy failed with code %d\n", err);
        hipFree(d_ptr);
        return 1;
    }
    printf("   SUCCESS: Copied %zu bytes from host to device\n\n", copy_size);

    // 4. Launch a kernel
    printf("4. Calling hipLaunchKernel...\n");
    dim3 grid(1, 1, 1);
    dim3 block(256, 1, 1);
    void* kernel_args[] = {&d_ptr};
    err = hipLaunchKernel(reinterpret_cast<const void*>(&mock_kernel),
                         grid,
                         block,
                         kernel_args,
                         0,      // sharedMem
                         nullptr // stream
    );
    if(err != hipSuccess)
    {
        printf("   ERROR: hipLaunchKernel failed with code %d\n", err);
        hipFree(d_ptr);
        return 1;
    }
    printf("   SUCCESS: Kernel launched\n\n");

    // 5. Synchronize device
    printf("5. Calling hipDeviceSynchronize...\n");
    err = hipDeviceSynchronize();
    if(err != hipSuccess)
    {
        printf("   ERROR: hipDeviceSynchronize failed with code %d\n", err);
        hipFree(d_ptr);
        return 1;
    }
    printf("   SUCCESS: Device synchronized\n\n");

    // 6. Copy data back from device
    printf("6. Calling hipMemcpy (Device -> Host)...\n");
    uint8_t h_result[copy_size];
    err = hipMemcpy(h_result, d_ptr, copy_size, hipMemcpyDeviceToHost);
    if(err != hipSuccess)
    {
        printf("   ERROR: hipMemcpy failed with code %d\n", err);
        hipFree(d_ptr);
        return 1;
    }
    printf("   SUCCESS: Copied %zu bytes from device to host\n\n", copy_size);

    // 7. Free device memory
    printf("7. Calling hipFree...\n");
    err = hipFree(d_ptr);
    if(err != hipSuccess)
    {
        printf("   ERROR: hipFree failed with code %d\n", err);
        return 1;
    }
    printf("   SUCCESS: Device memory freed\n\n");

    printf("=== All HIP API calls completed successfully ===\n");
    printf("This demonstrates clean separation:\n");
    printf("  - App only knows about HIP runtime API\n");
    printf("  - No direct dependency on SDK/register/tool\n");
    printf("  - Runtime handles all profiler integration internally\n");

    return 0;
}
