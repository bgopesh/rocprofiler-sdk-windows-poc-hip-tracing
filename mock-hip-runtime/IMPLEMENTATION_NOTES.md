# Mock HIP Runtime Implementation Notes

## What Was Created

A complete mock HIP runtime library for Windows that demonstrates the proper architecture for profiling tool integration.

## File Structure

```
poc-v2/mock-hip-runtime/
├── mock_hip_runtime.h       # Public API header (91 lines)
├── mock_hip_runtime.cpp     # Implementation (243 lines)
├── CMakeLists.txt           # Build configuration (51 lines)
├── README.md                # User documentation
└── IMPLEMENTATION_NOTES.md  # This file
```

## Key Implementation Details

### 1. Linking Architecture (CRITICAL)

```
mock-hip-runtime.dll
  └── Links to: rocprofiler-register.dll ONLY
  └── Does NOT link to: SDK, tools, or any other profiling components
```

This is verified in CMakeLists.txt:

```cmake
target_link_libraries(mock-hip-runtime
    PRIVATE
        mock-rocprofiler-register  # ONLY dependency
)
```

### 2. Registration During hipInit()

The registration happens in `mock_hip_runtime.cpp::register_with_rocprofiler()`:

```cpp
// 1. Define import function (allows tools to get unwrapped table)
ROCPROFILER_REGISTER_DEFINE_IMPORT(hip, ROCPROFILER_REGISTER_COMPUTE_VERSION_3(6, 4, 0))

// 2. Construct dispatch table
construct_dispatch_table();

// 3. Register with rocprofiler-register
rocprofiler_register_library_api_table(
    "hip",                                            // Library name
    &ROCPROFILER_REGISTER_IMPORT_FUNC(hip),           // Import function
    ROCPROFILER_REGISTER_COMPUTE_VERSION_3(6, 4, 0),  // Version
    &profiler_table_ptr,                              // Dispatch table pointer
    1,                                                // Number of tables
    &lib_id);                                         // Registration ID
```

### 3. Dispatch Table Architecture

Two tables are maintained:

```cpp
namespace {
    HipDispatchTable original_table = {};  // Unmodified mock implementations
    HipDispatchTable profiler_table = {};  // Potentially wrapped by tools
}
```

**Flow:**
1. `original_table` is filled with mock implementations
2. `profiler_table` is copied from `original_table`
3. `profiler_table` is passed to rocprofiler-register
4. Tools can modify `profiler_table` via `rocprofiler_set_api_table()`
5. Public HIP APIs call through `profiler_table`

**Example:**

```cpp
// Public API calls through profiler_table (which may be wrapped)
HIP_API hipError_t hipMalloc(void** ptr, size_t size)
{
    return profiler_table.hipMalloc(ptr, size);
}
```

### 4. Windows-Specific Symbols

#### Export Declarations

```cpp
#ifdef _WIN32
#    define HIP_API __declspec(dllexport)
#else
#    define HIP_API __attribute__((visibility("default")))
#endif

extern "C" {
HIP_API hipError_t hipInit(unsigned int flags);
HIP_API hipError_t hipMalloc(void** ptr, size_t size);
// ...
}
```

#### Import Declarations

The library imports from rocprofiler-register:

```cpp
#include "mock_register.h"  // Defines ROCPROFILER_REGISTER_PUBLIC_API as __declspec(dllimport)

// Functions used:
rocprofiler_register_error_code_t rocprofiler_register_library_api_table(...);
const char* rocprofiler_register_error_string(rocprofiler_register_error_code_t);
```

### 5. API Coverage

Basic HIP APIs implemented:

| API | Purpose | Status |
|-----|---------|--------|
| `hipInit()` | Initialize HIP runtime, trigger registration | ✓ Complete |
| `hipMalloc()` | Allocate device memory (mocked with malloc) | ✓ Complete |
| `hipFree()` | Free device memory | ✓ Complete |
| `hipMemcpy()` | Copy memory (mocked with memcpy) | ✓ Complete |
| `hipLaunchKernel()` | Launch kernel (simulated, prints message) | ✓ Complete |

### 6. Mock Implementations

Each mock function:
- Prints trace messages showing it was called
- Uses host memory to simulate GPU operations
- Returns appropriate error codes
- Validates parameters

Example:

```cpp
hipError_t mock_hipMalloc_impl(void** ptr, size_t size)
{
    printf("    [HIP Runtime] hipMalloc(%zu bytes)\n", size);
    *ptr = malloc(size);  // Use host memory
    if(*ptr == nullptr)
    {
        printf("    [HIP Runtime] hipMalloc FAILED - malloc returned NULL\n");
        return hipErrorMemoryAllocation;
    }
    allocation_counter++;
    return hipSuccess;
}
```

### 7. Thread Safety

The library uses `std::once_flag` for initialization:

```cpp
std::once_flag init_flag;

HIP_API hipError_t hipInit(unsigned int flags)
{
    std::call_once(init_flag, initialize_hip);
    return hipSuccess;
}
```

This ensures registration happens exactly once, even with multiple threads calling `hipInit()`.

## Integration with rocprofiler-register

When `hipInit()` is called:

1. HIP runtime constructs dispatch tables
2. Calls `rocprofiler_register_library_api_table()`
3. rocprofiler-register checks `ROCP_TOOL_LIBRARIES` environment variable
4. If set, loads SDK/tool DLLs using `LoadLibraryA()`
5. Finds `rocprofiler_set_api_table()` using `EnumProcessModules()` + `GetProcAddress()`
6. Calls `rocprofiler_set_api_table()` passing the HIP dispatch table
7. SDK/tool wraps function pointers in the dispatch table
8. Subsequent HIP API calls go through wrapped functions

## Testing

To test this implementation:

1. Build the library: `cmake --build build`
2. Create a test application that links to `amdhip64.dll`
3. Without tools: HIP APIs work normally (print mock messages)
4. With tools: Set `ROCP_TOOL_LIBRARIES=path/to/tool.dll` and APIs are traced

## Compliance with Requirements

- [x] Links ONLY to rocprofiler-register (not to SDK)
- [x] Calls `rocprofiler_register_library_api_table()` during `hipInit()`
- [x] Provides basic HIP API stubs: hipInit, hipMalloc, hipMemcpy, hipLaunchKernel, hipFree
- [x] Uses `__declspec(dllexport)` for exported functions
- [x] Uses `__declspec(dllimport)` for rocprofiler-register functions (via header)
- [x] Created in `poc-v2/mock-hip-runtime/`
- [x] Includes comments showing this is the HIP runtime layer using register component

## Lines of Code

- **mock_hip_runtime.h**: 91 lines (types, API declarations)
- **mock_hip_runtime.cpp**: 243 lines (implementation, registration)
- **CMakeLists.txt**: 51 lines (build configuration)
- **Total**: 385 lines of implementation code
