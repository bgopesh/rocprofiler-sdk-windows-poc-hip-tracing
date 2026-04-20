# Mock rocprofiler-register - Windows Implementation

This directory contains a Windows-native implementation of the `rocprofiler-register` library, which is the critical component that enables dynamic profiling tool injection for HIP and HSA runtime libraries.

## Purpose

The `rocprofiler-register` library serves as a bridge between:
1. **Runtime libraries** (HIP, HSA) that provide dispatch tables for API interception
2. **Profiling tools** (rocprofiler-sdk, custom tools) that wrap API functions for tracing

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│ HIP Application (test-hip-app.exe)                             │
│ - Links ONLY to mock-hip-runtime.dll                           │
│ - No awareness of profiling infrastructure                     │
└───────────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ HIP Runtime (mock-hip-runtime.dll)                             │
│ - Calls rocprofiler_register_library_api_table()               │
│ - Passes dispatch table pointer                                │
└───────────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ rocprofiler-register.dll (THIS COMPONENT)                      │
│                                                                 │
│ Key Responsibilities:                                           │
│ 1. Store registration info and dispatch table pointers         │
│ 2. Read ROCP_TOOL_LIBRARIES environment variable               │
│ 3. Load tool DLLs using LoadLibraryA()                         │
│ 4. Search for symbols using EnumProcessModules +               │
│    GetProcAddress()                                             │
│ 5. Call rocprofiler_configure() to initialize tools            │
│ 6. Call rocprofiler_set_api_table() to modify dispatch tables  │
└───────────────────────────────┬─────────────────────────────────┘
                                │
                                ▼
┌─────────────────────────────────────────────────────────────────┐
│ Profiling Tool (mock-sdk-tracer.dll)                           │
│ - Exports rocprofiler_configure                                │
│ - Exports rocprofiler_set_api_table                            │
│ - Wraps HIP functions to capture timing/arguments              │
└─────────────────────────────────────────────────────────────────┘
```

## Windows-Specific Implementation Details

### 1. Dynamic Library Loading

**Linux Approach:**
```cpp
void* handle = dlopen("rocprofiler-sdk.so", RTLD_NOW | RTLD_GLOBAL);
```

**Windows Approach:**
```cpp
HMODULE handle = LoadLibraryA("rocprofiler-sdk.dll");
// Fallback strategies:
// - Try with .dll extension
// - Try with current directory prefix
// - Search in PATH directories
```

**Implementation:** `load_library_with_fallback()` in `mock_register.cpp`

### 2. Symbol Resolution - Single Module

**Linux Approach:**
```cpp
void* symbol = dlsym(handle, "rocprofiler_configure");
```

**Windows Approach:**
```cpp
void* symbol = GetProcAddress(handle, "rocprofiler_configure");
```

### 3. Symbol Resolution - Global Search (Critical Difference!)

**Linux Approach:**
```cpp
// RTLD_DEFAULT searches ALL loaded libraries globally
void* symbol = dlsym(RTLD_DEFAULT, "rocprofiler_configure");
```

**Windows Approach:**
```cpp
// Windows has NO RTLD_DEFAULT equivalent!
// Must enumerate ALL loaded modules manually:
HMODULE modules[1024];
DWORD needed;
EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed);

DWORD count = needed / sizeof(HMODULE);
for (DWORD i = 0; i < count; i++) {
    void* symbol = GetProcAddress(modules[i], "rocprofiler_configure");
    if (symbol) {
        return symbol; // Found it!
    }
}
```

**Implementation:** `find_symbol_in_any_module()` in `mock_register.cpp`

This is the most significant Windows-specific change. Linux's `dlsym(RTLD_DEFAULT)` provides a single-call global symbol search, while Windows requires explicit iteration through all loaded modules.

### 4. Module Enumeration (Library Discovery)

**Linux Approach:**
```cpp
// Parse /proc/self/maps to get loaded libraries
FILE* maps = fopen("/proc/self/maps", "r");
while (fgets(line, sizeof(line), maps)) {
    // Parse: 7f1234567000-7f1234789000 r-xp ... /lib/x86_64-linux-gnu/libc.so.6
}
```

**Windows Approach:**
```cpp
HMODULE modules[1024];
DWORD needed;
EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed);

for (DWORD i = 0; i < needed / sizeof(HMODULE); i++) {
    char path[MAX_PATH];
    GetModuleFileNameA(modules[i], path, sizeof(path));
    // Process: C:\Windows\System32\kernel32.dll
}
```

**Implementation:** `enumerate_loaded_modules()` in `mock_register.cpp`

### 5. Environment Variable Access

**Linux Approach:**
```cpp
const char* value = getenv("ROCP_TOOL_LIBRARIES");
```

**Windows Approach:**
```cpp
char buffer[4096];
DWORD result = GetEnvironmentVariableA("ROCP_TOOL_LIBRARIES", buffer, sizeof(buffer));
if (result > 0 && result < sizeof(buffer)) {
    // Use buffer
}
```

**Implementation:** `discover_and_initialize_tools()` in `mock_register.cpp`

### 6. Path Separator

**Linux:** Colon (`:`) for path lists
```bash
export ROCP_TOOL_LIBRARIES=/opt/rocm/lib/rocprofiler-sdk.so:/path/to/tool2.so
```

**Windows:** Semicolon (`;`) for path lists
```cmd
set ROCP_TOOL_LIBRARIES=C:\rocm\bin\rocprofiler-sdk.dll;C:\tools\tool2.dll
```

**Implementation:** Uses `strtok(str, ";")` in `discover_and_initialize_tools()`

## API Functions Implemented

### `rocprofiler_register_library_api_table()`

**Purpose:** Main entry point called by runtime libraries (HIP, HSA) to register their API dispatch tables.

**Parameters:**
- `lib_name`: Library identifier (e.g., "hip", "hsa")
- `import_func`: Function returning library version
- `lib_version`: Encoded version number (10000*major + 100*minor + patch)
- `api_tables`: Array of pointers to modifiable dispatch tables
- `api_table_length`: Number of tables
- `register_id`: Output parameter receiving unique registration ID

**Behavior:**
1. Validates arguments
2. Checks for re-entrant calls (deadlock detection)
3. Stores registration information
4. On first call, triggers tool discovery via `discover_and_initialize_tools()`
5. Returns success or error code

**Thread Safety:** Uses `g_in_registration` thread-local flag to detect re-entrance

### `rocprofiler_register_error_string()`

**Purpose:** Convert error code to human-readable string

**Example:**
```cpp
const char* msg = rocprofiler_register_error_string(ROCP_REG_DEADLOCK);
// Returns: "Deadlock detected (re-entrant call)"
```

### `rocprofiler_register_iterate_registration_info()`

**Purpose:** Iterate over all registered libraries (for debugging/introspection)

**Usage:**
```cpp
int callback(rocprofiler_register_registration_info_t* info, void* data) {
    printf("Library: %s, version: %u\n", info->common_name, info->lib_version);
    return 0; // Continue iteration
}

rocprofiler_register_iterate_registration_info(callback, user_data);
```

## Tool Discovery Flow

1. **Library Registration**
   - HIP runtime calls `rocprofiler_register_library_api_table()`
   - Dispatch table pointers are stored

2. **Environment Variable Check**
   - Read `ROCP_TOOL_LIBRARIES` (semicolon-separated DLL paths)
   - Example: `mock-sdk-tracer.dll` or `C:\tools\profiler.dll`

3. **Tool Loading**
   - For each path, call `LoadLibraryA()` with fallback strategies
   - Track successfully loaded modules

4. **Symbol Discovery - rocprofiler_configure**
   - Search ALL loaded modules using `EnumProcessModules()`
   - Call `GetProcAddress()` on each module
   - First match wins

5. **Tool Initialization**
   - Call `rocprofiler_configure(version, runtime_version, priority, &client_id)`
   - Tool initializes internal state

6. **Symbol Discovery - rocprofiler_set_api_table**
   - Search ALL loaded modules again
   - Find the function that modifies API tables

7. **API Table Modification**
   - For each registered library (HIP, HSA, etc.):
   - Call `rocprofiler_set_api_table(lib_name, version, instance, tables, count)`
   - Tool wraps API functions by modifying dispatch table pointers

## Multi-Attempt Loading Strategy

The Linux implementation tries multiple loading modes with RTLD flags:
```cpp
// Attempt 1: RTLD_NOW | RTLD_LOCAL
// Attempt 2: RTLD_LAZY | RTLD_LOCAL  
// Attempt 3: RTLD_NOW | RTLD_GLOBAL
// Attempt 4: RTLD_LAZY | RTLD_GLOBAL
```

Windows has no direct equivalent to RTLD flags, so we use path-based fallbacks:
```cpp
// Attempt 1: Load as specified (searches current dir, system dirs, PATH)
// Attempt 2: Try adding .dll extension if not present
// Attempt 3: Try prefixing with current directory path
```

**Implementation:** `load_library_with_fallback()` function

## Environment Variables

| Variable | Purpose | Example (Windows) |
|----------|---------|-------------------|
| `ROCP_TOOL_LIBRARIES` | Semicolon-separated list of tool DLL paths | `mock-sdk-tracer.dll;C:\tools\custom.dll` |
| `ROCPROFILER_REGISTER_VERBOSE` | Enable detailed logging | `1` or `true` |

## Debug Logging

Set `ROCPROFILER_REGISTER_VERBOSE=1` to see detailed trace:

```
[ROCPROF-REG] ========================================
[ROCPROF-REG] rocprofiler_register_library_api_table called
[ROCPROF-REG]   Library: hip
[ROCPROF-REG]   Version: 60400
[ROCPROF-REG]   Tables: 1
[ROCPROF-REG] ========================================
[ROCPROF-REG] Registration successful: ID=1
[ROCPROF-REG] ========================================
[ROCPROF-REG] Tool Discovery Phase
[ROCPROF-REG] ========================================
[ROCPROF-REG] ROCP_TOOL_LIBRARIES: mock-sdk-tracer.dll
[ROCPROF-REG] Attempting to load library: mock-sdk-tracer.dll
[ROCPROF-REG]   -> Loaded successfully (attempt 1)
[ROCPROF-REG] Enumerated 8 loaded modules
[ROCPROF-REG]   [0] test-hip-app.exe
[ROCPROF-REG]   [1] ntdll.dll
[ROCPROF-REG]   [2] KERNEL32.DLL
[ROCPROF-REG]   [3] mock-hip-runtime.dll
[ROCPROF-REG]   [4] rocprofiler-register.dll
[ROCPROF-REG]   [5] mock-sdk-tracer.dll
[ROCPROF-REG] Searching for symbol: rocprofiler_configure
[ROCPROF-REG]   -> Found 'rocprofiler_configure' in mock-sdk-tracer.dll (module index 5)
[ROCPROF-REG] Calling rocprofiler_configure()
[ROCPROF-REG] rocprofiler_configure returned: 0 (client_id=1)
[ROCPROF-REG] Searching for symbol: rocprofiler_set_api_table
[ROCPROF-REG]   -> Found 'rocprofiler_set_api_table' in mock-sdk-tracer.dll (module index 5)
[ROCPROF-REG] Calling rocprofiler_set_api_table for each registered library
[ROCPROF-REG]   Library: hip (version 60400, 1 tables)
[ROCPROF-REG]   -> rocprofiler_set_api_table returned: 0
[ROCPROF-REG] ========================================
[ROCPROF-REG] Tool Discovery Phase Complete
[ROCPROF-REG] ========================================
```

## Error Handling

The implementation includes comprehensive error handling:

- **Invalid arguments:** Returns `ROCP_REG_INVALID_ARGUMENT`
- **Null API table pointer:** Returns `ROCP_REG_INVALID_API_ADDRESS`
- **Re-entrant call:** Returns `ROCP_REG_DEADLOCK`
- **Library load failure:** Logs warning, continues (allows fallback)
- **Symbol not found:** Logs warning, returns early (profiling disabled)

## Thread Safety

- **Registration storage:** Protected by `g_registration_mutex`
- **Re-entrance detection:** Thread-local `g_in_registration` flag
- **One-time initialization:** Uses `std::call_once()` for tool discovery

## Comparison with Linux Implementation

| Feature | Linux | Windows |
|---------|-------|---------|
| **Dynamic loading** | `dlopen()` | `LoadLibraryA()` |
| **Symbol lookup** | `dlsym()` | `GetProcAddress()` |
| **Global symbol search** | `dlsym(RTLD_DEFAULT)` | `EnumProcessModules()` + loop |
| **Module enumeration** | Parse `/proc/self/maps` | `EnumProcessModules()` |
| **Module info** | Parse map file lines | `GetModuleFileNameA()` |
| **Env variable access** | `getenv()` | `GetEnvironmentVariableA()` |
| **Path separator** | `:` (colon) | `;` (semicolon) |
| **Library extension** | `.so` | `.dll` |

## Building

```bash
cd poc-v2
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

Output: `build/bin/Release/rocprofiler-register.dll`

## Testing

1. **Build the library:**
   ```bash
   cmake --build build --config Release
   ```

2. **Set environment variables:**
   ```cmd
   set ROCP_TOOL_LIBRARIES=mock-sdk-tracer.dll
   set ROCPROFILER_REGISTER_VERBOSE=1
   set PATH=%PATH%;build\bin\Release
   ```

3. **Run HIP application:**
   ```cmd
   test-hip-app.exe
   ```

4. **Verify in output:**
   - See `[ROCPROF-REG]` log messages
   - Confirm tool discovery succeeded
   - Confirm API table modification completed

## Integration with Full System

This component integrates with:

1. **HIP Runtime** (`mock-hip-runtime.dll`)
   - Calls `rocprofiler_register_library_api_table()`
   - Provides modifiable dispatch table

2. **Profiling SDK** (`mock-sdk-tracer.dll`)
   - Exports `rocprofiler_configure()`
   - Exports `rocprofiler_set_api_table()`
   - Modifies dispatch table to wrap API functions

3. **Application** (`test-hip-app.exe`)
   - Unaware of profiling infrastructure
   - Calls HIP APIs normally
   - Profiling is transparent

## Key Design Decisions

1. **No RTLD_DEFAULT Equivalent**
   - Windows requires manual module enumeration
   - Implemented via `EnumProcessModules()` + iteration
   - More verbose than Linux but functionally equivalent

2. **Multi-Attempt Loading**
   - Handles relative and absolute paths
   - Automatic .dll extension addition
   - Current directory fallback

3. **Verbose Logging**
   - Essential for debugging module/symbol discovery
   - Controlled by environment variable
   - Minimal overhead when disabled

4. **Thread Safety**
   - Thread-local re-entrance detection
   - Mutex-protected registration storage
   - One-time tool initialization

## References

- **ARCHITECTURE.md**: Overall Windows architecture documentation
- **ROCPROFILER_SDK_WINDOWS_PORT.md**: SDK porting guide
- **Linux implementation**: ROCm source code (rocprofiler-register)
- **Windows API docs**: MSDN documentation for EnumProcessModules, LoadLibraryA, GetProcAddress

## Future Enhancements

1. **Security mode**: Validate tool library signatures before loading
2. **Priority ordering**: Support multiple tools with priority-based ordering
3. **Attachment mode**: Support runtime attach/detach of tools
4. **Error recovery**: Graceful fallback when tool loading fails
5. **Performance**: Cache symbol lookups to avoid repeated enumeration
