# Integration Guide: Mock ROCProfiler SDK with rocprofiler-register

## Overview

This document explains how the mock ROCProfiler SDK integrates with the `rocprofiler-register` component to enable runtime profiling tool discovery on Windows.

## Component Architecture

```
test-hip-app.exe
  |
  +-- Links to: mock-hip-runtime.dll
        |
        +-- Links to: rocprofiler-register.dll
              |
              +-- Runtime loads via LoadLibraryA():
                    |
                    +-- rocprofiler-sdk.dll (THIS COMPONENT)
                          |
                          +-- Exports symbols:
                                - rocprofiler_configure()
                                - rocprofiler_set_api_table()
                                - rocprofiler_create_context()
                                - etc.
```

## Symbol Discovery Flow

### Step 1: SDK Loading

**rocprofiler-register.dll** attempts to load the SDK:

```cpp
// In rocprofiler-register initialization
HMODULE sdk_handle = LoadLibraryA("rocprofiler-sdk.dll");

if (!sdk_handle) {
    // Fallback: try environment variable path
    const char* sdk_path = getenv("ROCPROFILER_REGISTER_LIBRARY");
    if (sdk_path) {
        sdk_handle = LoadLibraryA(sdk_path);
    }
}
```

### Step 2: Symbol Search

Register searches for `rocprofiler_configure` in all loaded modules:

```cpp
// Method 1: Direct lookup in loaded DLL
auto configure_func = GetProcAddress(sdk_handle, "rocprofiler_configure");

// Method 2: Search all loaded modules (if SDK brought in dependencies)
HMODULE modules[1024];
DWORD needed;
EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed);

for (DWORD i = 0; i < needed / sizeof(HMODULE); i++) {
    auto func = GetProcAddress(modules[i], "rocprofiler_configure");
    if (func) {
        // Found it!
        break;
    }
}
```

### Step 3: SDK Initialization

Register calls `rocprofiler_configure()`:

```cpp
typedef void* (*rocprofiler_configure_t)(uint32_t, const char*, uint32_t, void*);

auto configure = reinterpret_cast<rocprofiler_configure_t>(
    GetProcAddress(sdk_handle, "rocprofiler_configure"));

if (configure) {
    void* config = configure(
        1,              // version
        "1.0.0",        // runtime version
        0,              // priority
        nullptr         // client ID
    );
}
```

**Mock SDK Output:**
```
[MOCK SDK] ========================================
[MOCK SDK] rocprofiler_configure() called
[MOCK SDK]   Version: 1
[MOCK SDK]   Runtime: 1.0.0
[MOCK SDK]   Priority: 0
[MOCK SDK]   Client ID: 00000000
[MOCK SDK] ========================================
```

### Step 4: API Table Interception

After SDK initialization, register calls `rocprofiler_set_api_table()`:

```cpp
auto set_api_table = reinterpret_cast<rocprofiler_set_api_table_t>(
    GetProcAddress(sdk_handle, "rocprofiler_set_api_table"));

if (set_api_table) {
    // Pass HIP dispatch table to SDK for modification
    void* tables[] = { &hip_dispatch_table };
    set_api_table("hip", 6, 0, tables, 1);
}
```

**Mock SDK Output:**
```
[MOCK SDK] ========================================
[MOCK SDK] rocprofiler_set_api_table() called
[MOCK SDK]   Library: hip
[MOCK SDK]   Version: 6
[MOCK SDK]   Instance: 0
[MOCK SDK]   Num tables: 1
[MOCK SDK] ========================================
```

## Environment Variables

### ROCPROFILER_REGISTER_LIBRARY

Specifies the path to the SDK DLL (if not in default search path):

```batch
REM Absolute path
set ROCPROFILER_REGISTER_LIBRARY=C:\path\to\rocprofiler-sdk.dll

REM Relative path (from executable directory)
set ROCPROFILER_REGISTER_LIBRARY=.\lib\rocprofiler-sdk.dll

REM Just filename (searches in PATH)
set ROCPROFILER_REGISTER_LIBRARY=rocprofiler-sdk.dll
```

### DLL Search Order (Windows)

When `LoadLibraryA("rocprofiler-sdk.dll")` is called, Windows searches:

1. Directory of the executable
2. Current working directory
3. System directory (C:\Windows\System32)
4. Windows directory (C:\Windows)
5. Directories in PATH environment variable

**Best Practice**: Place `rocprofiler-sdk.dll` in same directory as `test-hip-app.exe` or set `ROCPROFILER_REGISTER_LIBRARY` to absolute path.

## Integration Verification

### Step 1: Build Components

```bash
cd poc-v2
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

### Step 2: Verify SDK Exports

```bash
cd poc-v2/mock-sdk
verify_exports.bat
```

Expected output:
```
========================================
ROCProfiler SDK DLL Export Verification
========================================

Found DLL: ..\build\bin\Release\rocprofiler-sdk.dll

Checking exports...

        272 rocprofiler_configure
        273 rocprofiler_configure_buffer_tracing
        274 rocprofiler_create_buffer
        275 rocprofiler_create_context
        276 rocprofiler_destroy_context
        277 rocprofiler_set_api_table
        278 rocprofiler_start_context
        279 rocprofiler_stop_context
```

### Step 3: Run Test Application

```bash
cd poc-v2/build/bin/Release
.\test-hip-app.exe
```

Expected console output flow:

```
[MOCK HIP] DLL_PROCESS_ATTACH - Mock HIP Runtime loaded

[MOCK REGISTER] DLL_PROCESS_ATTACH - ROCProfiler Register loaded
[MOCK REGISTER] Searching for rocprofiler-sdk.dll...
[MOCK REGISTER] Loaded SDK from: C:\...\rocprofiler-sdk.dll

[MOCK SDK] DLL_PROCESS_ATTACH - Mock ROCProfiler SDK loaded
[MOCK SDK] Module handle: 00007FFB12340000
[MOCK SDK] Waiting for rocprofiler_configure() call...

[MOCK REGISTER] Found rocprofiler_configure at 00007FFB12345678
[MOCK REGISTER] Calling rocprofiler_configure()...

[MOCK SDK] ========================================
[MOCK SDK] rocprofiler_configure() called
[MOCK SDK]   Version: 1
[MOCK SDK]   Runtime: 1.0.0
[MOCK SDK]   Priority: 0
[MOCK SDK] ========================================

[MOCK REGISTER] Calling rocprofiler_set_api_table()...

[MOCK SDK] ========================================
[MOCK SDK] rocprofiler_set_api_table() called
[MOCK SDK]   Library: hip
[MOCK SDK]   Version: 6
[MOCK SDK]   Instance: 0
[MOCK SDK]   Num tables: 1
[MOCK SDK] ========================================

[APP] Running HIP application...
[APP] Calling hipMalloc...
[MOCK HIP] hipMalloc(ptr=..., size=1024)
[APP] Calling hipFree...
[MOCK HIP] hipFree(ptr=...)

[MOCK SDK] DLL_PROCESS_DETACH - Cleaning up...
[MOCK REGISTER] DLL_PROCESS_DETACH - Unloading...
```

## Tool Development Example

To create a custom profiling tool using this SDK:

1. **Create tool DLL** that exports `rocprofiler_configure()`
2. **Link against** `rocprofiler-sdk.dll` (or use runtime loading)
3. **Inside `rocprofiler_configure()`**:
   - Call `rocprofiler_create_context()`
   - Call `rocprofiler_create_buffer()` with callback
   - Call `rocprofiler_configure_buffer_tracing()`
   - Call `rocprofiler_start_context()`
4. **Implement buffer callback** to process trace records
5. **Set environment variable**: `ROCPROFILER_REGISTER_LIBRARY=your-tool.dll`

See `example_tool_usage.cpp` for complete implementation.

## Debugging Integration Issues

### Issue: SDK Not Found

**Symptom:**
```
[MOCK REGISTER ERROR] Failed to load rocprofiler-sdk.dll
```

**Solution:**
- Verify DLL exists in build output: `build/bin/Release/rocprofiler-sdk.dll`
- Set `ROCPROFILER_REGISTER_LIBRARY` to absolute path
- Check DLL dependencies with `dumpbin /DEPENDENTS rocprofiler-sdk.dll`

### Issue: Symbol Not Found

**Symptom:**
```
[MOCK REGISTER ERROR] rocprofiler_configure not found
```

**Solution:**
- Verify exports with `dumpbin /EXPORTS rocprofiler-sdk.dll`
- Ensure `extern "C"` wrapper around functions
- Check `__declspec(dllexport)` is present
- Rebuild with `/VERBOSE:LIB` to see export generation

### Issue: Access Violation on Symbol Call

**Symptom:**
```
Exception: 0xC0000005 (Access Violation)
```

**Solution:**
- Verify function signature matches exactly
- Check calling convention (should be default `__cdecl`)
- Ensure DLL is built with matching runtime library (MD vs MT)
- Use debugger to inspect function pointer before call

## Advanced: Tracing Implementation

This basic SDK **does not** implement actual HIP API tracing. For a full tracing implementation with dispatch table wrapping, see `../mock-sdk-tracer/mock_sdk_tracer.cpp`.

The tracer demonstrates:
- Saving original dispatch table
- Replacing function pointers with wrappers
- Recording timestamps and correlation IDs
- Buffering trace records
- Writing CSV output on process detach

## Next Steps

1. **Test SDK loading**: Run `test-hip-app.exe` and verify SDK is loaded
2. **Implement tool**: Create custom tool using `example_tool_usage.cpp` as template
3. **Add tracing**: Enhance `rocprofiler_set_api_table()` to actually wrap dispatch table
4. **Process traces**: Implement buffer callback to write trace output
5. **Extend APIs**: Add HSA runtime support, kernel tracing, etc.

## References

- `../mock-rocprofiler-register/` - Register component implementation
- `../mock-hip-runtime/` - HIP runtime with dispatch table
- `../../ARCHITECTURE.md` - Overall system architecture
- `../../ROCPROFILER_SDK_WINDOWS_PORT.md` - Linux to Windows porting guide
