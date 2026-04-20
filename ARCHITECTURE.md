# Architecture Documentation - Windows ROCProfiler SDK POC

## Overview

This document details the architectural decisions and Windows-specific implementation patterns used in the ROCProfiler SDK HIP Tracing POC for Windows.

## Table of Contents

1. [Component Architecture](#component-architecture)
2. [Windows Porting Strategy](#windows-porting-strategy)
3. [Symbol Discovery Mechanism](#symbol-discovery-mechanism)
4. [Dispatch Table Interception](#dispatch-table-interception)
5. [Trace Recording Pipeline](#trace-recording-pipeline)
6. [Critical Implementation Details](#critical-implementation-details)

---

## Component Architecture

### Layered Design

```
┌──────────────────────────────────────────────────────┐
│                   Application Layer                   │
│              (Links: amdhip64.dll only)               │
└───────────────────────┬──────────────────────────────┘
                        │ HIP API calls
                        │ (hipMalloc, hipFree, etc.)
                        v
┌──────────────────────────────────────────────────────┐
│                  HIP Runtime Layer                    │
│                   (amdhip64.dll)                      │
│  • Maintains dispatch table                          │
│  • Registers with rocprofiler-register               │
│  • Delegates to (potentially wrapped) table          │
│  Links: rocprofiler-register.dll only                │
└───────────────────────┬──────────────────────────────┘
                        │ rocprofiler_register_library_api_table()
                        │ (passes dispatch table pointer)
                        v
┌──────────────────────────────────────────────────────┐
│              Registration/Discovery Layer             │
│           (rocprofiler-register.dll)                  │
│  • LoadLibraryA(ROCPROFILER_REGISTER_LIBRARY)        │
│  • EnumProcessModules() for symbol search            │
│  • GetProcAddress() for symbol resolution            │
│  • Calls rocprofiler_configure()                     │
│  • Calls rocprofiler_set_api_table()                 │
│  Links: psapi.lib (Windows API)                      │
└───────────────────────┬──────────────────────────────┘
                        │ Dynamic loading at runtime
                        v
┌──────────────────────────────────────────────────────┐
│                 Profiling SDK Layer                   │
│              (rocprofiler-sdk.dll)                    │
│  • Exports rocprofiler_configure()                   │
│  • Exports rocprofiler_set_api_table()               │
│  • Wraps dispatch table IN-PLACE                     │
│  • Records high-resolution timestamps                │
│  • Writes CSV traces                                 │
│  Links: None (standalone DLL)                        │
└──────────────────────────────────────────────────────┘
```

### Key Design Principles

1. **Minimal Dependencies**: Each layer links to minimum required libraries
2. **Runtime Discovery**: SDK loaded via environment variable (no compile-time dependency)
3. **In-Place Modification**: Dispatch table modified directly (not copied)
4. **Windows-Native**: All Linux syscalls replaced with Windows APIs

---

## Windows Porting Strategy

### Critical Replacements

| Linux API | Windows API | Purpose | Notes |
|-----------|-------------|---------|-------|
| `dlopen()` | `LoadLibraryA()` | Load shared library | Multi-attempt strategy: direct, with .dll, with path |
| `dlsym()` | `GetProcAddress()` | Resolve symbol | Per-module, not global |
| `dlsym(RTLD_DEFAULT, ...)` | `EnumProcessModules()` + loop | Global symbol search | **No Windows equivalent** - must enumerate manually |
| `/proc/self/maps` parsing | `EnumProcessModules()` | List loaded modules | Returns array of `HMODULE` |
| `getenv()` | `GetEnvironmentVariableA()` | Read env vars | Requires size checking |
| `pthread_mutex_t` | `std::mutex` | Thread safety | C++11 standard (cross-platform) |
| `:` path separator | `;` path separator | `ROCP_TOOL_LIBRARIES` | Windows convention |

### Code Example: Global Symbol Search

**Linux (simple):**
```cpp
void* symbol = dlsym(RTLD_DEFAULT, "rocprofiler_configure");
```

**Windows (complex):**
```cpp
void* find_symbol_in_any_module(const char* symbol_name) {
    HMODULE modules[1024];
    DWORD needed;
    
    if (!EnumProcessModules(GetCurrentProcess(), modules, 
                            sizeof(modules), &needed)) {
        return nullptr;
    }
    
    DWORD module_count = needed / sizeof(HMODULE);
    
    for (DWORD i = 0; i < module_count; i++) {
        void* symbol = GetProcAddress(modules[i], symbol_name);
        if (symbol) {
            return symbol;
        }
    }
    
    return nullptr;
}
```

This is the **most significant Windows-specific implementation** in the POC.

---

## Symbol Discovery Mechanism

### Two-Phase Discovery

#### Phase 1: Load SDK Library

```cpp
// Read environment variable
char sdk_library[4096];
GetEnvironmentVariableA("ROCPROFILER_REGISTER_LIBRARY", 
                        sdk_library, sizeof(sdk_library));

// Load SDK explicitly
HMODULE sdk_handle = LoadLibraryA(sdk_library);
```

#### Phase 2: Search for Symbols

```cpp
// Search all loaded modules for rocprofiler_configure
void* configure_func = find_symbol_in_any_module("rocprofiler_configure");

// Search for rocprofiler_set_api_table
void* set_table_func = find_symbol_in_any_module("rocprofiler_set_api_table");
```

### Module Enumeration Details

**Process:**
1. `EnumProcessModules()` returns array of `HMODULE` handles
2. For each module, call `GetProcAddress(module, "symbol_name")`
3. Return first match (or nullptr if not found)

**Typical Module List:**
```
[ 0] test-hip-app.exe
[ 1] ntdll.dll
[ 2] KERNEL32.DLL
[ 3] KERNELBASE.dll
[ 4] ucrtbase.dll
[ 5] amdhip64.dll           ← HIP runtime
[ 6] VCRUNTIME140.dll
[ 7] rocprofiler-register.dll ← Registration layer
[ 8] MSVCP140.dll
[ 9] VCRUNTIME140_1.dll
[10] rocprofiler-sdk.dll    ← SDK (loaded dynamically)
[11] mock-tool.dll          ← Tool (optional)
```

### Symbol Export Requirements

**SDK must export symbols visibly:**
```cpp
#ifdef _WIN32
  #define ROCPROFILER_SDK_API __declspec(dllexport)
#else
  #define ROCPROFILER_SDK_API __attribute__((visibility("default")))
#endif

extern "C" {
    ROCPROFILER_SDK_API void* rocprofiler_configure(...);
    ROCPROFILER_SDK_API int rocprofiler_set_api_table(...);
}
```

---

## Dispatch Table Interception

### Table Structure

```cpp
struct HipDispatchTable {
    uint64_t size;  // Must be first - sizeof(table)
    
    // Function pointers
    hipError_t (*hipMalloc)(void** ptr, size_t size);
    hipError_t (*hipFree)(void* ptr);
    hipError_t (*hipMemcpy)(void* dst, const void* src, 
                            size_t count, hipMemcpyKind kind);
    hipError_t (*hipLaunchKernel)(const void* func, dim3 gridDim, 
                                   dim3 blockDim, void** args, 
                                   size_t sharedMem, void* stream);
};
```

### HIP Runtime Registration

```cpp
void register_with_rocprofiler() {
    construct_dispatch_table();
    
    void* profiler_table_ptr = static_cast<void*>(&profiler_table);
    
    rocprofiler_register_library_api_table(
        "hip",                      // Library name
        &rocprofiler_register_...,  // Import function
        version,                    // Version
        &profiler_table_ptr,        // Pointer to dispatch table
        1,                          // Number of tables
        &lib_id                     // Output: registration ID
    );
}
```

**Critical Detail**: Passes `&profiler_table_ptr` (pointer to pointer), so SDK receives the actual table address.

### SDK Interception (CORRECT Way)

```cpp
int rocprofiler_set_api_table(const char* name, uint64_t version,
                              uint64_t instance, void** tables,
                              uint64_t num_tables) {
    // Get the ACTUAL dispatch table from runtime
    HipDispatchTable* hip_table = 
        reinterpret_cast<HipDispatchTable*>(tables[0]);
    
    // Save original function pointers
    g_original_hip_table = new HipDispatchTable(*hip_table);
    
    // Wrap function pointers IN-PLACE
    hip_table->hipMalloc = wrapped_hipMalloc;
    hip_table->hipFree = wrapped_hipFree;
    hip_table->hipMemcpy = wrapped_hipMemcpy;
    hip_table->hipLaunchKernel = wrapped_hipLaunchKernel;
    
    return 0;
}
```

### Why In-Place Modification is Critical

**WRONG (creates copy):**
```cpp
// This does NOT work!
HipDispatchTable* wrapped = new HipDispatchTable(*hip_table);
wrapped->hipMalloc = my_wrapper;
tables[0] = wrapped;  // ❌ Runtime still uses original table
```

**CORRECT (modifies actual table):**
```cpp
// This works!
HipDispatchTable* hip_table = reinterpret_cast<HipDispatchTable*>(tables[0]);
hip_table->hipMalloc = my_wrapper;  // ✅ Runtime uses this table
```

The HIP runtime stores `profiler_table` as a global variable. When it calls `hipMalloc()`, it does:
```cpp
return profiler_table.hipMalloc(ptr, size);
```

So we **must** modify `profiler_table` itself, not create a new table.

---

## Trace Recording Pipeline

### Wrapper Function Pattern

```cpp
hipError_t wrapped_hipMalloc(void** ptr, size_t size) {
    // 1. Generate correlation ID
    uint64_t cid = g_correlation_id.fetch_add(1);
    
    // 2. Record start timestamp
    uint64_t start = get_timestamp_ns();
    
    // 3. Call original implementation
    hipError_t result = g_original_hip_table->hipMalloc(ptr, size);
    
    // 4. Record end timestamp
    uint64_t end = get_timestamp_ns();
    
    // 5. Write trace record
    write_trace_record("HIP", "hipMalloc", cid, start, end);
    
    // 6. Return result to application
    return result;
}
```

### Timestamp Implementation

```cpp
inline uint64_t get_timestamp_ns() {
    using namespace std::chrono;
    return duration_cast<nanoseconds>(
        high_resolution_clock::now().time_since_epoch()
    ).count();
}
```

**Resolution**: Windows `high_resolution_clock` provides nanosecond precision (actual resolution ~100ns depending on hardware).

### CSV Writing

```cpp
void write_trace_record(const char* domain, const char* function,
                        uint64_t correlation_id, 
                        uint64_t start_ns, uint64_t end_ns) {
    std::lock_guard<std::mutex> lock(g_trace_mutex);
    
    if (g_trace_file.is_open()) {
        uint64_t duration = end_ns - start_ns;
        
        g_trace_file << domain << ","
                     << function << ","
                     << GetCurrentProcessId() << ","
                     << GetCurrentThreadId() << ","
                     << correlation_id << ","
                     << start_ns << ","
                     << end_ns << ","
                     << duration << "\n";
        
        g_trace_file.flush();  // Ensure immediate write
    }
}
```

**Thread Safety**: Mutex protects concurrent writes from multiple threads.

---

## Critical Implementation Details

### 1. DLL Loading Strategy

Multi-attempt approach handles various path formats:

```cpp
HMODULE load_library_with_fallback(const char* library_path) {
    // Attempt 1: Direct path
    HMODULE handle = LoadLibraryA(library_path);
    if (handle) return handle;
    
    // Attempt 2: Add .dll extension
    std::string with_dll = std::string(library_path) + ".dll";
    handle = LoadLibraryA(with_dll.c_str());
    if (handle) return handle;
    
    // Attempt 3: Current directory + library name
    char current_dir[MAX_PATH];
    GetCurrentDirectoryA(sizeof(current_dir), current_dir);
    std::string full_path = std::string(current_dir) + "\\" + library_path;
    handle = LoadLibraryA(full_path.c_str());
    
    return handle;
}
```

### 2. Environment Variable Handling

Windows requires size checking:

```cpp
char tool_libraries[4096];
DWORD result = GetEnvironmentVariableA("ROCP_TOOL_LIBRARIES",
                                        tool_libraries,
                                        sizeof(tool_libraries));

if (result == 0 || result >= sizeof(tool_libraries)) {
    // Not set or too large
    return;
}
```

**Important**: Return value `0` means not set, return value `>= buffer_size` means truncated.

### 3. Path Separator

Windows uses semicolon (`;`) instead of colon (`:`):

```cpp
// Parse semicolon-separated library paths
std::vector<std::string> library_paths;
char* token = strtok(tool_libraries, ";");  // Note: ";" not ":"
while (token != nullptr) {
    library_paths.push_back(token);
    token = strtok(nullptr, ";");
}
```

### 4. DllMain for Initialization

```cpp
BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            // Initialization
            printf("[SDK] Loaded\n");
            break;
            
        case DLL_PROCESS_DETACH:
            // Cleanup: close trace file, free resources
            if (g_trace_file.is_open()) {
                g_trace_file.close();
            }
            break;
    }
    return TRUE;
}
```

**Critical**: Must close file in `DLL_PROCESS_DETACH` to flush remaining data.

### 5. Thread Safety

All global state protected by mutexes:

```cpp
std::mutex g_state_mutex;      // Protects context/buffer maps
std::mutex g_trace_mutex;      // Protects CSV file writes
std::atomic<uint64_t> g_correlation_id{1};  // Lock-free counter
```

---

## Comparison to Linux Implementation

| Aspect | Linux | Windows | Complexity |
|--------|-------|---------|-----------|
| Symbol Discovery | `dlsym(RTLD_DEFAULT, ...)` | `EnumProcessModules()` + loop | **Windows more complex** |
| Library Loading | `dlopen()` | `LoadLibraryA()` | Similar |
| Module List | Read `/proc/self/maps` | `EnumProcessModules()` | **Windows simpler** |
| Path Separator | `:` | `;` | Trivial |
| DLL Init/Cleanup | `__attribute__((constructor))` | `DllMain()` | Different patterns |
| Thread-Local Storage | `pthread_key_t` | `std::mutex` (C++11) | Cross-platform solution |

---

## Performance Considerations

### Trace Recording Overhead

**Per API Call:**
1. Atomic increment: ~10 ns
2. Two `high_resolution_clock::now()` calls: ~100-200 ns
3. File write (with flush): ~1-10 μs

**Total overhead per HIP call**: ~1-10 microseconds

### Optimization Opportunities

1. **Buffering**: Buffer traces in memory, flush periodically
2. **Binary Format**: Use binary instead of CSV (faster writes)
3. **Lock-Free Queue**: Replace mutex with lock-free queue
4. **Lazy Flushing**: Flush on timer instead of per-write

---

## Security Considerations

### DLL Loading Security

- **DLL Hijacking**: Uses full paths where possible
- **Search Order**: Multi-attempt strategy checks current directory last
- **Validation**: Could add signature verification (not implemented in POC)

### Symbol Export Security

- All exports are explicitly marked with `__declspec(dllexport)`
- No implicit exports (WINDOWS_EXPORT_ALL_SYMBOLS disabled)

---

## Testing Strategy

### Unit Testing Approach

1. **Symbol Discovery**: Verify `find_symbol_in_any_module()` finds known symbols
2. **Table Wrapping**: Verify wrapper functions are called
3. **CSV Output**: Verify file format and data correctness
4. **Environment Parsing**: Verify semicolon-separated paths parsed correctly

### Integration Testing

Full stack test via `rocprofv3-poc.py`:
- Launches application
- Verifies CSV created
- Checks API call count matches expected

---

## Future Enhancements

### Short Term
- [ ] Buffered trace writing
- [ ] Support multiple tools simultaneously
- [ ] Binary trace format option

### Medium Term
- [ ] Real HIP runtime integration
- [ ] HSA API tracing support
- [ ] Performance counter integration

### Long Term
- [ ] Kernel name resolution
- [ ] Memory transfer tracking
- [ ] Timeline visualization tool

---

## References

- **Windows API Documentation**: https://docs.microsoft.com/en-us/windows/win32/
- **ROCProfiler SDK**: AMD ROCm profiling architecture
- **Reference Document**: `rocp-register.txt` (Windows porting requirements)

---

**Document Version**: 1.0  
**Last Updated**: 2026-04-20  
**Status**: Complete and validated
