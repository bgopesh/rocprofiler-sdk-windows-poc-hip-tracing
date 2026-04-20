# Mock HIP Runtime for Windows

This directory contains a mock HIP runtime library that demonstrates the proper architecture for profiling tool integration on Windows.

## Architecture

This library simulates the HIP runtime (amdhip64.dll) and implements the critical integration pattern:

```
Application (test-hip-app.exe)
  |
  +-- Links to --> amdhip64.dll (THIS LIBRARY)
                     |
                     +-- Links to --> rocprofiler-register.dll
                                        |
                                        +-- Dynamically loads --> SDK/Tools
```

## Key Design Principles

### 1. Links ONLY to rocprofiler-register

This library has **exactly one dependency**: `rocprofiler-register.dll`

It does **NOT** link to:
- rocprofiler-sdk (loaded dynamically by register layer)
- profiling tools (loaded dynamically by register layer)

This is critical for the correct separation of concerns.

### 2. Registration During hipInit()

When an application calls `hipInit()`, this library:

1. Constructs the HIP dispatch table (function pointers)
2. Calls `rocprofiler_register_library_api_table()` to register with the profiling system
3. The register layer may load profiling tools that wrap the dispatch table

### 3. Dispatch Table Architecture

The library maintains two dispatch tables:

- **original_table**: Unmodified, always points to mock implementations
- **profiler_table**: Modifiable copy that tools can wrap

All public HIP APIs call through `profiler_table`, which may have been wrapped by profiling tools via `rocprofiler_set_api_table()`.

## Files

### mock_hip_runtime.h

Public API header defining:
- HIP error codes (`hipError_t`)
- HIP types (`dim3`, `hipMemcpyKind`)
- HIP dispatch table structure (`HipDispatchTable`)
- Public HIP API declarations

### mock_hip_runtime.cpp

Implementation containing:
- Mock HIP function implementations (use host memory to simulate GPU ops)
- Dispatch table construction and registration
- Public API implementations that delegate to dispatch table

### CMakeLists.txt

Build configuration:
- Creates shared library `amdhip64.dll`
- Links to `mock-rocprofiler-register` only
- Exports symbols using `__declspec(dllexport)`

## API Surface

The library provides these HIP APIs:

```cpp
hipError_t hipInit(unsigned int flags);
hipError_t hipMalloc(void** ptr, size_t size);
hipError_t hipFree(void* ptr);
hipError_t hipMemcpy(void* dst, const void* src, size_t count, hipMemcpyKind kind);
hipError_t hipLaunchKernel(const void* func, dim3 gridDim, dim3 blockDim,
                           void** args, size_t sharedMem, void* stream);
```

## Usage

Applications link to this library and call HIP APIs normally:

```cpp
#include "mock_hip_runtime.h"

int main() {
    hipInit(0);
    
    void* ptr;
    hipMalloc(&ptr, 1024);
    hipFree(ptr);
    
    return 0;
}
```

Profiling is enabled by setting the `ROCPROFILER_REGISTER_LIBRARY` or `ROCP_TOOL_LIBRARIES` environment variable to point to the SDK/tool DLL.

## Windows-Specific Implementation

This implementation uses Windows-specific symbols:
- `__declspec(dllexport)` for exported functions
- `__declspec(dllimport)` for imported functions (from rocprofiler-register)

The registration layer (rocprofiler-register) handles Windows-specific discovery:
- `LoadLibraryA()` for dynamic library loading
- `GetProcAddress()` for symbol resolution
- `EnumProcessModules()` for module enumeration
