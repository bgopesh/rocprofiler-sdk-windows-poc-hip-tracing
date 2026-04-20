# Example Usage: Windows rocprofiler-register

This document provides practical examples of using the Windows rocprofiler-register implementation.

## Example 1: Basic HIP Runtime Registration

This example shows how a HIP runtime library registers with rocprofiler-register.

### Code (in HIP runtime DLL)

```cpp
#include <rocprofiler-register/mock_register.h>
#include <windows.h>
#include <cstdio>

// Define import function (returns HIP version)
ROCPROFILER_REGISTER_DEFINE_IMPORT(hip, ROCPROFILER_REGISTER_COMPUTE_VERSION_3(6, 4, 0))

// HIP API dispatch table structure
struct HipDispatchTable {
    uint64_t size;
    void* (*hipMalloc)(void** ptr, size_t size);
    void* (*hipFree)(void* ptr);
    // ... other HIP APIs
};

// Original and profiler-modifiable tables
HipDispatchTable original_table = { /* ... initialized with real functions ... */ };
HipDispatchTable profiler_table = original_table;  // Copy

void register_with_rocprofiler() {
    printf("[HIP] Registering with rocprofiler-register...\n");
    
    // Prepare table pointer
    void* table_ptr = &profiler_table;
    
    // Call registration function
    rocprofiler_register_library_indentifier_t lib_id;
    rocprofiler_register_error_code_t result;
    
    result = rocprofiler_register_library_api_table(
        "hip",                                           // Library name
        &ROCPROFILER_REGISTER_IMPORT_FUNC(hip),          // Import function
        ROCPROFILER_REGISTER_COMPUTE_VERSION_3(6, 4, 0), // Version
        &table_ptr,                                      // API table pointer
        1,                                               // Number of tables
        &lib_id);                                        // Output: registration ID
    
    if (result == ROCP_REG_SUCCESS) {
        printf("[HIP] Registration successful! ID: %llu\n", lib_id.handle);
    } else {
        printf("[HIP] Registration failed: %s\n", 
               rocprofiler_register_error_string(result));
    }
}

// Called during HIP initialization
void hipInit() {
    static bool initialized = false;
    if (!initialized) {
        register_with_rocprofiler();
        initialized = true;
    }
}
```

### Output (with ROCPROFILER_REGISTER_VERBOSE=1)

```
[HIP] Registering with rocprofiler-register...
[ROCPROF-REG] ========================================
[ROCPROF-REG] rocprofiler_register_library_api_table called
[ROCPROF-REG]   Library: hip
[ROCPROF-REG]   Version: 60400
[ROCPROF-REG]   Tables: 1
[ROCPROF-REG] ========================================
[ROCPROF-REG] Registration successful: ID=1
[HIP] Registration successful! ID: 1
```

## Example 2: Tool Implementation

This example shows how to implement a profiling tool that uses rocprofiler-register.

### Code (in tool DLL)

```cpp
#include <windows.h>
#include <cstdio>

// Export these symbols so rocprofiler-register can find them
#define TOOL_API __declspec(dllexport)

extern "C" {

//-----------------------------------------------------------------------------
// Tool Configuration Function
// Called once when tool is discovered
//-----------------------------------------------------------------------------

TOOL_API int rocprofiler_configure(
    uint32_t    version,         // ROCProfiler SDK version
    const char* runtime_version, // Runtime version string (e.g., "6.4.0")
    uint32_t    priority,        // Tool priority (0 = highest)
    uint64_t*   client_id)       // Output: unique client ID
{
    printf("[TOOL] rocprofiler_configure called\n");
    printf("[TOOL]   SDK Version: %u\n", version);
    printf("[TOOL]   Runtime Version: %s\n", runtime_version);
    printf("[TOOL]   Priority: %u\n", priority);
    
    // Assign client ID
    *client_id = 12345;
    
    printf("[TOOL] Tool initialized with client_id=%llu\n", *client_id);
    return 0;  // Success
}

//-----------------------------------------------------------------------------
// API Table Modification Function
// Called once for each registered library (HIP, HSA, etc.)
//-----------------------------------------------------------------------------

TOOL_API int rocprofiler_set_api_table(
    const char* lib_name,       // Library name (e.g., "hip")
    uint64_t    lib_version,    // Library version
    uint64_t    lib_instance,   // Instance ID from registration
    void**      tables,         // Array of dispatch table pointers
    uint64_t    num_tables)     // Number of tables
{
    printf("[TOOL] rocprofiler_set_api_table called\n");
    printf("[TOOL]   Library: %s\n", lib_name);
    printf("[TOOL]   Version: %llu\n", lib_version);
    printf("[TOOL]   Instance: %llu\n", lib_instance);
    printf("[TOOL]   Num Tables: %llu\n", num_tables);
    
    if (strcmp(lib_name, "hip") == 0) {
        // Modify HIP dispatch table
        struct HipDispatchTable {
            uint64_t size;
            void* (*hipMalloc)(void** ptr, size_t size);
            void* (*hipFree)(void* ptr);
        };
        
        auto* table = static_cast<HipDispatchTable*>(tables[0]);
        
        // Save original functions
        auto original_hipMalloc = table->hipMalloc;
        auto original_hipFree = table->hipFree;
        
        // Replace with wrappers
        table->hipMalloc = [](void** ptr, size_t size) -> void* {
            printf("[TOOL] INTERCEPTED: hipMalloc(%zu bytes)\n", size);
            // Call original implementation
            return original_hipMalloc(ptr, size);
        };
        
        table->hipFree = [](void* ptr) -> void* {
            printf("[TOOL] INTERCEPTED: hipFree(%p)\n", ptr);
            return original_hipFree(ptr);
        };
        
        printf("[TOOL] HIP dispatch table modified!\n");
    }
    
    return 0;  // Success
}

}  // extern "C"
```

### Building the Tool

```cmake
# CMakeLists.txt
add_library(my-profiler-tool SHARED
    tool.cpp
)

set_target_properties(my-profiler-tool PROPERTIES
    OUTPUT_NAME "my-profiler-tool"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF  # Use explicit __declspec(dllexport)
)
```

### Verifying Exports

```bash
# Check that symbols are exported
dumpbin /EXPORTS my-profiler-tool.dll

# Expected output:
# ordinal hint RVA      name
#       1    0 00001234 rocprofiler_configure
#       2    1 00001567 rocprofiler_set_api_table
```

## Example 3: Using the Tool

### Step 1: Set Environment Variables

```cmd
REM Specify tool DLL to load
set ROCP_TOOL_LIBRARIES=my-profiler-tool.dll

REM Enable verbose logging (optional)
set ROCPROFILER_REGISTER_VERBOSE=1

REM Add build directory to PATH so DLLs can be found
set PATH=%PATH%;C:\build\bin\Release
```

### Step 2: Run Application

```cmd
test-hip-app.exe
```

### Expected Output

```
[ROCPROF-REG] ========================================
[ROCPROF-REG] Tool Discovery Phase
[ROCPROF-REG] ========================================
[ROCPROF-REG] ROCP_TOOL_LIBRARIES: my-profiler-tool.dll
[ROCPROF-REG] Attempting to load library: my-profiler-tool.dll
[ROCPROF-REG]   -> Loaded successfully (attempt 1)
[ROCPROF-REG] ----------------------------------------
[ROCPROF-REG] Searching for rocprofiler_configure symbol
[ROCPROF-REG] ----------------------------------------
[ROCPROF-REG] Enumerated 7 loaded modules
[ROCPROF-REG]   [0] test-hip-app.exe
[ROCPROF-REG]   [1] ntdll.dll
[ROCPROF-REG]   [2] KERNEL32.DLL
[ROCPROF-REG]   [3] mock-hip-runtime.dll
[ROCPROF-REG]   [4] rocprofiler-register.dll
[ROCPROF-REG]   [5] my-profiler-tool.dll
[ROCPROF-REG]   -> Found 'rocprofiler_configure' in my-profiler-tool.dll (module index 5)
[ROCPROF-REG] ----------------------------------------
[ROCPROF-REG] Calling rocprofiler_configure()
[ROCPROF-REG] ----------------------------------------
[TOOL] rocprofiler_configure called
[TOOL]   SDK Version: 1
[TOOL]   Runtime Version: 6.4.0
[TOOL]   Priority: 0
[TOOL] Tool initialized with client_id=12345
[ROCPROF-REG] rocprofiler_configure returned: 0 (client_id=12345)
[ROCPROF-REG] ----------------------------------------
[ROCPROF-REG] Searching for rocprofiler_set_api_table symbol
[ROCPROF-REG] ----------------------------------------
[ROCPROF-REG]   -> Found 'rocprofiler_set_api_table' in my-profiler-tool.dll (module index 5)
[ROCPROF-REG] ----------------------------------------
[ROCPROF-REG] Calling rocprofiler_set_api_table for each registered library
[ROCPROF-REG] ----------------------------------------
[ROCPROF-REG]   Library: hip (version 60400, 1 tables)
[TOOL] rocprofiler_set_api_table called
[TOOL]   Library: hip
[TOOL]   Version: 60400
[TOOL]   Instance: 1
[TOOL]   Num Tables: 1
[TOOL] HIP dispatch table modified!
[ROCPROF-REG]   -> rocprofiler_set_api_table returned: 0
[ROCPROF-REG] ========================================
[ROCPROF-REG] Tool Discovery Phase Complete
[ROCPROF-REG] ========================================

[APPLICATION] Starting HIP operations...
[TOOL] INTERCEPTED: hipMalloc(1024 bytes)
[HIP] hipMalloc(1024 bytes)
[TOOL] INTERCEPTED: hipFree(0x12345678)
[HIP] hipFree(0x12345678)
[APPLICATION] Done!
```

## Example 4: Multiple Tools

### Setup

```cmd
set ROCP_TOOL_LIBRARIES=tool1.dll;tool2.dll
```

### Behavior

1. Both `tool1.dll` and `tool2.dll` are loaded
2. `rocprofiler_configure` is searched across ALL modules
3. **First match wins** (typically the first tool in the list)
4. `rocprofiler_set_api_table` is searched across ALL modules
5. First match's implementation is called

### Output

```
[ROCPROF-REG] Loaded tool library: tool1.dll
[ROCPROF-REG] Loaded tool library: tool2.dll
[ROCPROF-REG] Found 'rocprofiler_configure' in tool1.dll (module index 5)
[TOOL1] Configuration called
[ROCPROF-REG] Found 'rocprofiler_set_api_table' in tool1.dll (module index 5)
[TOOL1] API table modification called
```

**Note:** Only `tool1` is initialized because it's the first to export `rocprofiler_configure`.

## Example 5: Debugging Missing Symbols

### Problem: Tool DLL loaded but functions not called

```cmd
set ROCP_TOOL_LIBRARIES=my-tool.dll
set ROCPROFILER_REGISTER_VERBOSE=1
test-hip-app.exe
```

### Output (problem case)

```
[ROCPROF-REG] Loaded tool library: my-tool.dll
[ROCPROF-REG] Searching for symbol: rocprofiler_configure
[ROCPROF-REG]   -> Symbol 'rocprofiler_configure' not found in any loaded module
[ROCPROF-REG] WARNING: rocprofiler_configure symbol not found
```

### Diagnosis

The symbol is not exported. Check with `dumpbin`:

```bash
dumpbin /EXPORTS my-tool.dll
```

If `rocprofiler_configure` is missing from the exports table, the problem is:

1. **Missing `__declspec(dllexport)`**
   ```cpp
   // WRONG (no export)
   extern "C" {
       int rocprofiler_configure(...) { ... }
   }
   
   // CORRECT (with export)
   extern "C" {
       __declspec(dllexport) int rocprofiler_configure(...) { ... }
   }
   ```

2. **Name mangling (missing `extern "C"`)**
   ```cpp
   // WRONG (C++ name mangling)
   __declspec(dllexport) int rocprofiler_configure(...) { ... }
   
   // CORRECT (C linkage, no mangling)
   extern "C" {
       __declspec(dllexport) int rocprofiler_configure(...) { ... }
   }
   ```

3. **Symbol stripped by linker**
   - Ensure function is actually used or mark with `#pragma comment(linker, "/EXPORT:rocprofiler_configure")`

## Example 6: Error Handling

### Invalid Arguments

```cpp
rocprofiler_register_error_code_t result;

// Bad: null library name
result = rocprofiler_register_library_api_table(
    nullptr,  // ERROR!
    import_func, version, &tables, 1, &id);

printf("Result: %s\n", rocprofiler_register_error_string(result));
// Output: "Invalid argument"
```

### Re-entrant Call Detection

```cpp
// Tool's rocprofiler_configure calls HIP API, which tries to re-register
int rocprofiler_configure(...) {
    void* ptr;
    hipMalloc(&ptr, 1024);  // This triggers hipInit()
                            // hipInit() tries to register again
                            // Result: ROCP_REG_DEADLOCK
    return 0;
}
```

### Output

```
[ROCPROF-REG] ERROR: Re-entrant call detected (deadlock)
[ROCPROF-REG] Registration failed: Deadlock detected (re-entrant call)
```

## Example 7: Iterating Registered Libraries

### Code

```cpp
int registration_callback(rocprofiler_register_registration_info_t* info, void* data) {
    printf("Registered library: %s\n", info->common_name);
    printf("  Version: %u\n", info->lib_version);
    printf("  Tables: %llu\n", info->api_table_length);
    return 0;  // Continue iteration
}

void list_registrations() {
    rocprofiler_register_iterate_registration_info(registration_callback, nullptr);
}
```

### Output

```
Registered library: hip
  Version: 60400
  Tables: 1
Registered library: hsa
  Version: 10200
  Tables: 2
```

## Troubleshooting Guide

### Issue: Tool DLL not found

**Error:**
```
[ROCPROF-REG] Attempting to load library: my-tool.dll
[ROCPROF-REG]   -> Attempt 1 failed (error 126)
[ROCPROF-REG]   -> Attempt 2 failed (error 126)
[ROCPROF-REG]   -> Attempt 3 failed (error 126)
[ROCPROF-REG]   -> Failed to load library: my-tool.dll
```

**Solutions:**
1. Add DLL directory to PATH: `set PATH=%PATH%;C:\path\to\dll`
2. Use absolute path: `set ROCP_TOOL_LIBRARIES=C:\full\path\to\my-tool.dll`
3. Copy DLL to application directory

### Issue: Tool loaded but symbols not found

**Error:**
```
[ROCPROF-REG] Loaded tool library: my-tool.dll
[ROCPROF-REG]   -> Symbol 'rocprofiler_configure' not found
```

**Solutions:**
1. Verify exports: `dumpbin /EXPORTS my-tool.dll`
2. Add `extern "C"` and `__declspec(dllexport)`
3. Check for typos in function name

### Issue: Multiple DLLs, wrong one selected

**Behavior:** `tool2.dll` exports `rocprofiler_configure` but `tool1.dll` is called

**Cause:** `tool1.dll` also exports the symbol and was loaded first

**Solution:** Change load order:
```cmd
REM Wrong order
set ROCP_TOOL_LIBRARIES=tool1.dll;tool2.dll

REM Correct order (tool2 loaded first, found first)
set ROCP_TOOL_LIBRARIES=tool2.dll;tool1.dll
```

## Best Practices

1. **Always use `extern "C"`** for exported functions
2. **Explicitly export symbols** with `__declspec(dllexport)`
3. **Verify exports** with `dumpbin /EXPORTS` before deployment
4. **Enable verbose logging** during development: `set ROCPROFILER_REGISTER_VERBOSE=1`
5. **Use absolute paths** in production to avoid search path issues
6. **Test without profiling** to ensure application doesn't depend on tool
7. **Handle errors gracefully** in tool functions (return 0 for success, non-zero for error)
8. **Avoid HIP/HSA calls** in `rocprofiler_configure` (deadlock risk)

## Summary

The Windows rocprofiler-register implementation provides a flexible, robust mechanism for dynamic tool injection that matches the Linux behavior while accommodating Windows-specific DLL loading and symbol resolution patterns.
