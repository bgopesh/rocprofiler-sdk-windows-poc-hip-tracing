# POC v2: Clean HIP Test Application

This is a simple HIP application that demonstrates the clean separation principle:
**The application ONLY knows about the HIP runtime API.**

## Design Principles

1. **Single Dependency**: Links ONLY to `mock-hip-runtime.dll`
2. **No Profiler Knowledge**: No imports or includes from:
   - rocprofiler-sdk
   - rocprofiler-register
   - mock-tool
3. **Standard HIP API**: Uses standard HIP function declarations with `__declspec(dllimport)`
4. **Runtime Handles Profiling**: All profiler integration happens inside the HIP runtime library

## File Structure

```
poc-v2/test-hip-app/
  test_app.cpp          - Simple HIP application
  CMakeLists.txt        - Build config (links only to mock-hip-runtime)
  README.md             - This file
```

## What the Application Does

The test application exercises basic HIP APIs:

1. `hipInit()` - Initialize HIP runtime
2. `hipMalloc()` - Allocate device memory
3. `hipMemcpy()` - Copy data Host -> Device
4. `hipLaunchKernel()` - Launch a mock kernel
5. `hipDeviceSynchronize()` - Synchronize device
6. `hipMemcpy()` - Copy data Device -> Host
7. `hipFree()` - Free device memory

## Key Features

### Clean API Declarations

The application declares HIP functions with `__declspec(dllimport)`:

```cpp
#ifdef _WIN32
#    define HIP_API __declspec(dllimport)
#else
#    define HIP_API __attribute__((visibility("default")))
#endif

extern "C" {
HIP_API hipError_t hipInit(unsigned int flags);
HIP_API hipError_t hipMalloc(void** ptr, size_t size);
// ... etc
}
```

### No Profiler Dependencies

The CMakeLists.txt shows the clean separation:

```cmake
target_link_libraries(test-hip-app
    PRIVATE mock-hip-runtime)  # ONLY links to HIP runtime
```

### How Profiling Works

Even though the app doesn't know about profilers:

1. App calls `hipInit()` -> goes to `mock-hip-runtime.dll`
2. Runtime internally calls `rocprofiler-register` to register dispatch table
3. Runtime loads profiling tools (if configured via environment variables)
4. Tools can wrap HIP functions by modifying the dispatch table
5. App continues to use HIP APIs normally
6. All calls flow through tool wrappers transparently

## Building

From the poc-v2 directory:

```bash
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

## Running

```bash
cd build/test-hip-app/Release
./test-hip-app.exe
```

The output shows all HIP API calls being executed with mock implementations.

## Architecture Benefits

This design provides:

- **Portability**: App code is identical on Linux and Windows
- **Transparency**: Profiling happens without app code changes
- **Flexibility**: Tools can be added/removed via environment variables
- **Simplicity**: App developer only needs to know HIP API
- **Maintainability**: Profiler changes don't require app recompilation
