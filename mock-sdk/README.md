# Mock ROCProfiler SDK - Windows Runtime-Loadable Implementation

## Overview

This is a Windows mock implementation of the ROCProfiler SDK that can be loaded at runtime by `rocprofiler-register.dll` via `LoadLibraryA()`. It provides the essential symbols and APIs needed for profiler tool registration and basic profiling operations.

## Key Features

1. **Runtime Loadable**: Designed to be loaded dynamically via `LoadLibraryA()`, no link-time dependencies required
2. **Symbol Discovery**: Exports `rocprofiler_configure()` which rocprofiler-register searches for
3. **Basic SDK APIs**: Implements context management and buffer tracing APIs
4. **API Table Interception**: Provides `rocprofiler_set_api_table()` hook for dispatch table modification
5. **Windows DLL Semantics**: Uses `__declspec(dllexport)` for all public APIs
6. **Console Logging**: Prints diagnostic messages when SDK functions are called

## Exported Symbols

### Primary Entry Point

```cpp
void* rocprofiler_configure(uint32_t version, const char* runtime_version, 
                            uint32_t priority, void* client_id);
```

This is the **critical symbol** that rocprofiler-register searches for when discovering tool libraries. When found, it calls this function to initialize the SDK.

### Context Management APIs

```cpp
rocprofiler_status_t rocprofiler_create_context(rocprofiler_context_id_t* context);
rocprofiler_status_t rocprofiler_start_context(rocprofiler_context_id_t context);
rocprofiler_status_t rocprofiler_stop_context(rocprofiler_context_id_t context);
rocprofiler_status_t rocprofiler_destroy_context(rocprofiler_context_id_t context);
```

Context APIs allow tools to create isolated profiling sessions.

### Buffer Tracing APIs

```cpp
rocprofiler_status_t rocprofiler_create_buffer(
    rocprofiler_context_id_t context,
    size_t buffer_size,
    rocprofiler_buffer_callback_t callback,
    void* user_data,
    rocprofiler_buffer_id_t* buffer);

rocprofiler_status_t rocprofiler_configure_buffer_tracing(
    rocprofiler_context_id_t context,
    rocprofiler_buffer_id_t buffer);
```

Buffer APIs enable recording of HIP/HSA API traces into circular buffers.

### API Table Interception Hook

```cpp
int rocprofiler_set_api_table(const char* name, uint64_t lib_version,
                              uint64_t lib_instance, void** tables,
                              uint64_t num_tables);
```

This hook allows the SDK to intercept and modify HIP/HSA dispatch tables. Called by rocprofiler-register after loading the SDK.

## Build Requirements

- CMake 3.20+
- MSVC compiler (Visual Studio 2019+)
- Windows SDK

## Building

```bash
cd poc-v2
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Output: `build/bin/Release/rocprofiler-sdk.dll`

## Usage with rocprofiler-register

### Environment Setup

```bash
# Set SDK library path (optional - register can find it in PATH)
set ROCPROFILER_REGISTER_LIBRARY=path\to\rocprofiler-sdk.dll

# Run HIP application
test-hip-app.exe
```

### Runtime Flow

1. `test-hip-app.exe` loads `mock-hip-runtime.dll`
2. `mock-hip-runtime.dll` loads `rocprofiler-register.dll`
3. `rocprofiler-register.dll` calls `LoadLibraryA("rocprofiler-sdk.dll")`
4. Register searches all loaded modules for `rocprofiler_configure` (finds it in this DLL)
5. Register calls `rocprofiler_configure()` to initialize SDK
6. Register calls `rocprofiler_set_api_table()` to let SDK intercept HIP APIs
7. SDK wraps HIP dispatch table with tracing wrappers
8. Application HIP calls go through SDK wrappers

## Implementation Notes

### Standalone DLL Design

This SDK has **zero dependencies** (except Windows system DLLs). This ensures:
- Clean runtime loading via `LoadLibraryA()`
- No DLL dependency hell
- Fast initialization
- Minimal failure points

### Symbol Export Strategy

All public APIs use `__declspec(dllexport)`:

```cpp
#define ROCPROFILER_SDK_API __declspec(dllexport)

extern "C" {
    ROCPROFILER_SDK_API void* rocprofiler_configure(...);
}
```

This ensures symbols are exported in the DLL export table and discoverable via `GetProcAddress()`.

### Thread Safety

All context and buffer management operations are protected by `g_state_mutex` to ensure thread-safe operation.

### Logging

The SDK logs to stdout for diagnostic purposes. All log messages are prefixed with `[MOCK SDK]` for easy filtering.

## Differences from Full ROCProfiler SDK

This is a **minimal mock** for POC purposes. A full SDK implementation would include:

1. **HSA Runtime Integration**: Actual GPU agent enumeration via `hsa_iterate_agents()`
2. **Hardware Performance Counters**: PMU/SQ counter collection
3. **Kernel Dispatch Tracing**: Recording kernel launches with correlation IDs
4. **Memory Copy Tracing**: Tracking async memory operations
5. **PC Sampling**: Instruction pointer sampling during kernel execution
6. **ATT (Address Translation Tracing)**: Page fault tracking
7. **Double-Buffered Tracing**: Efficient lock-free buffer swapping
8. **CSV/JSON Output**: Formatted trace output
9. **Correlation ID Management**: Matching async operations across API/kernel boundaries

For a **full tracing implementation** with dispatch table wrapping, see `../mock-sdk-tracer/`.

## Verification

After building, verify the DLL exports the correct symbols:

```bash
dumpbin /EXPORTS rocprofiler-sdk.dll
```

Expected output should include:
- `rocprofiler_configure`
- `rocprofiler_create_context`
- `rocprofiler_start_context`
- `rocprofiler_stop_context`
- `rocprofiler_destroy_context`
- `rocprofiler_create_buffer`
- `rocprofiler_configure_buffer_tracing`
- `rocprofiler_set_api_table`

## Testing

See `../test-hip-app/` for a sample HIP application that exercises the SDK through the rocprofiler-register framework.

## Architecture Documentation

For detailed Windows vs Linux architecture differences, see:
- `../../ARCHITECTURE.md` - Component interaction patterns
- `../../ROCPROFILER_SDK_WINDOWS_PORT.md` - Linux to Windows porting guide
