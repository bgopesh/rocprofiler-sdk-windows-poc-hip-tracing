# Windows rocprofiler-register Design Document

## Executive Summary

This document describes the design and implementation of the Windows version of `rocprofiler-register`, focusing on the platform-specific challenges and solutions. The primary challenge is that Windows lacks a direct equivalent to Linux's `dlsym(RTLD_DEFAULT)` for global symbol resolution, requiring a custom module enumeration and symbol search implementation.

## Problem Statement

### Linux Architecture

On Linux, `rocprofiler-register` uses a simple and elegant approach:

```cpp
// Load tool library
void* handle = dlopen("rocprofiler-sdk.so", RTLD_NOW | RTLD_GLOBAL);

// Find configuration symbol globally (searches ALL loaded libraries)
void* configure = dlsym(RTLD_DEFAULT, "rocprofiler_configure");

// Find API table modification symbol
void* set_table = dlsym(RTLD_DEFAULT, "rocprofiler_set_api_table");
```

Key Linux features:
- `RTLD_DEFAULT`: Special handle for global symbol search across ALL loaded libraries
- `/proc/self/maps`: File containing all loaded libraries and their addresses
- `dlsym()`: Single function call for symbol resolution

### Windows Constraints

Windows does NOT provide:
1. A global symbol search mechanism (no `RTLD_DEFAULT`)
2. A file listing loaded modules (no `/proc/self/maps`)
3. Direct equivalents to RTLD flags (RTLD_NOW, RTLD_LAZY, RTLD_GLOBAL, RTLD_LOCAL)

Windows DOES provide:
1. `EnumProcessModules()`: Enumerate all loaded DLLs
2. `GetProcAddress()`: Resolve symbol in a specific module
3. `GetModuleFileNameA()`: Get full path of a module
4. `LoadLibraryA()`: Load a DLL dynamically

## Solution Architecture

### Core Design Pattern

The Windows implementation replaces `dlsym(RTLD_DEFAULT)` with a manual search loop:

```cpp
void* find_symbol_in_any_module(const char* symbol_name)
{
    // Step 1: Enumerate all loaded modules
    HMODULE modules[1024];
    DWORD needed;
    EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed);
    
    DWORD count = needed / sizeof(HMODULE);
    
    // Step 2: Search each module for the symbol
    for (DWORD i = 0; i < count; i++) {
        void* symbol = GetProcAddress(modules[i], symbol_name);
        if (symbol) {
            return symbol;  // Found it!
        }
    }
    
    return nullptr;  // Not found in any module
}
```

This is the **fundamental Windows-specific pattern** that replaces Linux's single `dlsym(RTLD_DEFAULT)` call.

## Detailed Implementation

### 1. Module Enumeration

**Purpose:** Get list of all DLLs loaded in the current process

**Linux Equivalent:**
```cpp
FILE* maps = fopen("/proc/self/maps", "r");
while (fgets(line, sizeof(line), maps)) {
    // Parse: 7f1234567000-7f1234789000 r-xp ... /lib/libc.so.6
}
```

**Windows Implementation:**
```cpp
std::vector<HMODULE> enumerate_loaded_modules()
{
    HMODULE modules[1024];
    DWORD needed = 0;
    
    if (!EnumProcessModules(GetCurrentProcess(), 
                            modules, 
                            sizeof(modules), 
                            &needed)) {
        // Error handling
        return {};
    }
    
    DWORD count = needed / sizeof(HMODULE);
    return std::vector<HMODULE>(modules, modules + count);
}
```

**Key Points:**
- `EnumProcessModules()` returns modules in **load order**
- First module is always the main executable
- System DLLs (ntdll.dll, kernel32.dll) are typically near the beginning
- Must link to `psapi.lib` to use this function

**Example Output (with verbose logging):**
```
[ROCPROF-REG] Enumerated 8 loaded modules
[ROCPROF-REG]   [0] test-hip-app.exe
[ROCPROF-REG]   [1] ntdll.dll
[ROCPROF-REG]   [2] KERNEL32.DLL
[ROCPROF-REG]   [3] mock-hip-runtime.dll
[ROCPROF-REG]   [4] rocprofiler-register.dll
[ROCPROF-REG]   [5] mock-sdk-tracer.dll
[ROCPROF-REG]   [6] mock-tool.dll
[ROCPROF-REG]   [7] msvcrt.dll
```

### 2. Symbol Search

**Purpose:** Find a symbol across all loaded modules

**Linux Equivalent:**
```cpp
void* symbol = dlsym(RTLD_DEFAULT, "rocprofiler_configure");
```

**Windows Implementation:**
```cpp
void* find_symbol_in_any_module(const char* symbol_name)
{
    auto modules = enumerate_loaded_modules();
    
    for (size_t i = 0; i < modules.size(); i++) {
        void* symbol = GetProcAddress(modules[i], symbol_name);
        if (symbol) {
            // Log which module contained the symbol
            char path[MAX_PATH];
            GetModuleFileNameA(modules[i], path, sizeof(path));
            
            VERBOSE_LOG("Found '%s' in %s (index %zu)", 
                        symbol_name, basename(path), i);
            return symbol;
        }
    }
    
    VERBOSE_LOG("Symbol '%s' not found", symbol_name);
    return nullptr;
}
```

**Performance Considerations:**
- Worst case: O(N) where N = number of loaded modules
- Typical N: 20-50 modules for a HIP application
- Each `GetProcAddress()` call is fast (hash table lookup in PE exports)
- Total overhead: ~1-2 microseconds (negligible for one-time initialization)

**Export Requirements:**
Symbols MUST be exported with `__declspec(dllexport)` to be found by `GetProcAddress()`:

```cpp
#ifdef _WIN32
#define TOOL_API __declspec(dllexport)
#else
#define TOOL_API __attribute__((visibility("default")))
#endif

extern "C" {
    TOOL_API int rocprofiler_configure(...);
    TOOL_API int rocprofiler_set_api_table(...);
}
```

### 3. Dynamic Library Loading

**Purpose:** Load tool libraries specified in environment variable

**Linux Equivalent:**
```cpp
// Linux tries multiple RTLD flag combinations
void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
if (!handle) handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
if (!handle) handle = dlopen(path, RTLD_NOW | RTLD_GLOBAL);
if (!handle) handle = dlopen(path, RTLD_LAZY | RTLD_GLOBAL);
```

**Windows Implementation:**
```cpp
HMODULE load_library_with_fallback(const char* library_path)
{
    // Attempt 1: Load as-is
    HMODULE handle = LoadLibraryA(library_path);
    if (handle) return handle;
    
    // Attempt 2: Add .dll extension if missing
    std::string with_dll = library_path;
    if (with_dll.find(".dll") == std::string::npos) {
        with_dll += ".dll";
        handle = LoadLibraryA(with_dll.c_str());
        if (handle) return handle;
    }
    
    // Attempt 3: Try current directory
    char current_dir[MAX_PATH];
    GetCurrentDirectoryA(sizeof(current_dir), current_dir);
    std::string full_path = std::string(current_dir) + "\\" + library_path;
    handle = LoadLibraryA(full_path.c_str());
    if (handle) return handle;
    
    return nullptr;  // All attempts failed
}
```

**LoadLibraryA() Search Order:**
1. Directory of the executable
2. System directory (`C:\Windows\System32`)
3. 16-bit system directory (`C:\Windows\System`)
4. Windows directory (`C:\Windows`)
5. Current directory
6. Directories in `PATH` environment variable

**Why Multiple Attempts?**
- **Attempt 1:** Handles absolute paths and DLLs in PATH
- **Attempt 2:** Convenience for users (can specify "rocprofiler-sdk" instead of "rocprofiler-sdk.dll")
- **Attempt 3:** Handles relative paths from current directory

### 4. Environment Variable Parsing

**Purpose:** Read tool library paths from environment

**Linux Equivalent:**
```cpp
const char* tools = getenv("ROCP_TOOL_LIBRARIES");
// Colon-separated: /opt/rocm/lib/tool1.so:/usr/local/lib/tool2.so
char* token = strtok(tools, ":");
```

**Windows Implementation:**
```cpp
char tool_libraries[4096] = {0};
DWORD result = GetEnvironmentVariableA("ROCP_TOOL_LIBRARIES",
                                       tool_libraries,
                                       sizeof(tool_libraries));

if (result > 0 && result < sizeof(tool_libraries)) {
    // Semicolon-separated: C:\rocm\bin\tool1.dll;C:\tools\tool2.dll
    char* token = strtok(tool_libraries, ";");
    while (token) {
        // Load library
        token = strtok(nullptr, ";");
    }
}
```

**Key Differences:**
- **Separator:** `;` (semicolon) instead of `:` (colon)
  - Windows uses `:` for drive letters (C:, D:)
  - Standard Windows convention for PATH-like variables
- **Function:** `GetEnvironmentVariableA()` instead of `getenv()`
  - Returns error codes for better diagnostics
  - Allows size checking to prevent buffer overflows

**Example:**
```cmd
set ROCP_TOOL_LIBRARIES=mock-sdk-tracer.dll;C:\custom\profiler.dll
```

### 5. Tool Discovery and Initialization

**Purpose:** Coordinate tool loading and configuration

**Full Flow:**

```cpp
void discover_and_initialize_tools()
{
    // 1. Read environment variable
    char libraries[4096];
    GetEnvironmentVariableA("ROCP_TOOL_LIBRARIES", libraries, sizeof(libraries));
    
    // 2. Parse semicolon-separated paths
    std::vector<std::string> paths;
    char* token = strtok(libraries, ";");
    while (token) {
        paths.push_back(token);
        token = strtok(nullptr, ";");
    }
    
    // 3. Load each tool library
    std::vector<HMODULE> loaded_tools;
    for (const auto& path : paths) {
        HMODULE handle = load_library_with_fallback(path.c_str());
        if (handle) {
            loaded_tools.push_back(handle);
        }
    }
    
    // 4. Search for rocprofiler_configure symbol
    void* configure = find_symbol_in_any_module("rocprofiler_configure");
    if (!configure) {
        VERBOSE_LOG("WARNING: rocprofiler_configure not found");
        return;
    }
    
    // 5. Call rocprofiler_configure
    auto configure_func = (rocprofiler_configure_func_t)configure;
    uint64_t client_id = 0;
    configure_func(1, "6.4.0", 0, &client_id);
    
    // 6. Search for rocprofiler_set_api_table symbol
    void* set_table = find_symbol_in_any_module("rocprofiler_set_api_table");
    if (!set_table) {
        VERBOSE_LOG("WARNING: rocprofiler_set_api_table not found");
        return;
    }
    
    // 7. Call rocprofiler_set_api_table for each registered library
    auto set_table_func = (rocprofiler_set_api_table_func_t)set_table;
    for (const auto& registration : g_registrations) {
        set_table_func(registration.name.c_str(),
                      registration.version,
                      registration.id.handle,
                      registration.api_tables.data(),
                      registration.api_tables.size());
    }
}
```

**Timing:** This function is called once using `std::call_once()` on the first library registration.

## Threading and Synchronization

### 1. One-Time Initialization

**Challenge:** Multiple threads may call `rocprofiler_register_library_api_table()` simultaneously

**Solution:** Use `std::call_once()` to ensure tool discovery happens exactly once

```cpp
static std::once_flag init_flag;

rocprofiler_register_error_code_t
rocprofiler_register_library_api_table(...)
{
    // ... store registration ...
    
    // Trigger tool discovery (once only)
    std::call_once(init_flag, discover_and_initialize_tools);
    
    return ROCP_REG_SUCCESS;
}
```

**Benefits:**
- Thread-safe without explicit locking
- First caller does initialization, others wait
- Subsequent callers skip immediately

### 2. Registration Storage Protection

**Challenge:** Multiple threads may register libraries concurrently

**Solution:** Protect registration vector with mutex

```cpp
std::vector<registration_record_t> g_registrations;
std::mutex g_registration_mutex;

// When adding registration:
{
    std::lock_guard<std::mutex> lock(g_registration_mutex);
    g_registrations.push_back(record);
}
```

### 3. Re-Entrance Detection

**Challenge:** Tool initialization might call HIP APIs, causing re-entrance

**Solution:** Use thread-local flag to detect and prevent re-entrance

```cpp
thread_local bool g_in_registration = false;

rocprofiler_register_error_code_t
rocprofiler_register_library_api_table(...)
{
    if (g_in_registration) {
        return ROCP_REG_DEADLOCK;  // Reject re-entrant call
    }
    
    g_in_registration = true;
    // ... do work ...
    g_in_registration = false;
}
```

**Why thread-local?**
- Different threads can register simultaneously (allowed)
- Same thread cannot re-enter (deadlock protection)

## Error Handling

### 1. Graceful Degradation

**Philosophy:** If tool loading fails, the application should still run (without profiling)

**Implementation:**
```cpp
// Load tool libraries
for (const auto& path : library_paths) {
    HMODULE handle = load_library_with_fallback(path.c_str());
    if (!handle) {
        VERBOSE_LOG("WARNING: Failed to load %s", path.c_str());
        continue;  // Try next library, don't abort
    }
    loaded_tools.push_back(handle);
}

// Search for symbols
void* configure = find_symbol_in_any_module("rocprofiler_configure");
if (!configure) {
    VERBOSE_LOG("WARNING: rocprofiler_configure not found - profiling disabled");
    return;  // Early return, but no error propagated
}
```

**Result:** Application continues execution without profiling if tool loading fails.

### 2. Diagnostic Logging

**Verbose Mode:** Controlled by `ROCPROFILER_REGISTER_VERBOSE` environment variable

```cpp
void init_verbose_mode() {
    char buffer[32];
    DWORD result = GetEnvironmentVariableA("ROCPROFILER_REGISTER_VERBOSE", 
                                           buffer, sizeof(buffer));
    g_verbose = (result > 0 && (strcmp(buffer, "1") == 0 || 
                                 strcmp(buffer, "true") == 0));
}

#define VERBOSE_LOG(...) \
    do { \
        if (g_verbose) { \
            printf("[ROCPROF-REG] " __VA_ARGS__); \
            printf("\n"); \
        } \
    } while(0)
```

**Usage:**
```cmd
set ROCPROFILER_REGISTER_VERBOSE=1
test-hip-app.exe
```

**Output:** Detailed trace of module loading, symbol search, and tool initialization

## Design Trade-offs

### 1. Module Enumeration Performance

**Trade-off:** Enumerate all modules vs. cache results

**Choice:** Enumerate on-demand, no caching

**Rationale:**
- Tool discovery happens once per process lifetime
- Module enumeration is fast (~microseconds for 50 modules)
- Caching adds complexity with minimal benefit
- Tool DLLs are loaded AFTER enumeration (first search would miss them anyway)

### 2. Symbol Search Ordering

**Trade-off:** First match vs. best match (by priority)

**Choice:** First match wins

**Rationale:**
- Matches Linux `dlsym(RTLD_DEFAULT)` behavior (first match in link order)
- Simpler implementation
- Tools can control priority by controlling load order
- Future enhancement: add explicit priority support

### 3. Error Recovery

**Trade-off:** Abort on error vs. continue without profiling

**Choice:** Continue without profiling (graceful degradation)

**Rationale:**
- Tool loading should not break application
- Users may want to run application without profiling
- Errors logged for debugging but not fatal
- Matches ROCProfiler philosophy: "profiling is optional"

## Testing Strategy

### 1. Unit Tests (Conceptual - not implemented in POC)

```cpp
TEST(ModuleEnumeration, FindsAllLoadedModules) {
    auto modules = enumerate_loaded_modules();
    ASSERT_GT(modules.size(), 0);
    // Should find at least kernel32.dll and ntdll.dll
}

TEST(SymbolSearch, FindsExportedSymbol) {
    // Load a test DLL with known export
    HMODULE test_dll = LoadLibraryA("test_exports.dll");
    void* symbol = find_symbol_in_any_module("test_function");
    ASSERT_NE(symbol, nullptr);
}

TEST(LibraryLoading, HandlesMissingExtension) {
    HMODULE handle = load_library_with_fallback("kernel32");
    ASSERT_NE(handle, nullptr);  // Should auto-add .dll
}
```

### 2. Integration Tests

**Test 1: Basic Registration**
```cpp
// HIP runtime calls registration
rocprofiler_register_library_api_table("hip", ..., &tables, 1, &id);

// Verify ID was assigned
ASSERT_NE(id.handle, 0);
```

**Test 2: Tool Discovery**
```cmd
set ROCP_TOOL_LIBRARIES=mock-sdk-tracer.dll
set ROCPROFILER_REGISTER_VERBOSE=1
test-hip-app.exe

# Verify output contains:
# - "Loaded tool library: mock-sdk-tracer.dll"
# - "Found 'rocprofiler_configure' in mock-sdk-tracer.dll"
# - "rocprofiler_configure returned: 0"
```

**Test 3: Multiple Libraries**
```cmd
set ROCP_TOOL_LIBRARIES=tool1.dll;tool2.dll
# Both should load, first one with rocprofiler_configure wins
```

### 3. Stress Tests

**Test 1: Many Modules**
- Load 100+ DLLs before tool discovery
- Verify symbol search still works

**Test 2: Concurrent Registration**
- Multiple threads register different libraries
- Verify thread safety with ThreadSanitizer (if available on Windows)

## Comparison: Linux vs Windows Implementation

| Aspect | Linux | Windows | Complexity |
|--------|-------|---------|------------|
| **Symbol search** | `dlsym(RTLD_DEFAULT)` (1 call) | `EnumProcessModules()` + loop (N calls) | Windows +30 LOC |
| **Module enumeration** | Parse `/proc/self/maps` (file I/O) | `EnumProcessModules()` (API call) | Similar |
| **Library loading** | `dlopen()` with RTLD flags (4 attempts) | `LoadLibraryA()` with path variants (3 attempts) | Similar |
| **Env variables** | `getenv()` | `GetEnvironmentVariableA()` | Similar |
| **Path separator** | `:` (colon) | `;` (semicolon) | Trivial |
| **Error handling** | `dlerror()` string | `GetLastError()` code | Similar |

**Total Additional Complexity:** ~50 lines of code for Windows-specific symbol search

## Future Enhancements

### 1. Attachment Mode

**Goal:** Allow runtime attach/detach of profiling tools

**Linux:** Uses `LD_AUDIT` or `LD_PRELOAD` for runtime injection

**Windows:** Would require:
- DLL injection via `CreateRemoteThread()`
- Hook installation via Detours or similar
- Message passing for control commands

### 2. Security Validation

**Goal:** Validate tool libraries before loading (prevent malicious DLL injection)

**Implementation:**
```cpp
bool validate_tool_library(const char* path) {
    // Check digital signature
    // Verify library is in trusted location (e.g., C:\Program Files\ROCm)
    // Validate export table contains expected symbols
}
```

### 3. Priority-Based Tool Ordering

**Goal:** Control order of tool invocation when multiple tools exist

**Current:** First match wins (based on module load order)

**Enhanced:**
```cpp
struct tool_info {
    HMODULE handle;
    int priority;  // Higher priority tools initialized first
    void* configure;
    void* set_table;
};

std::vector<tool_info> tools;
std::sort(tools.begin(), tools.end(), 
          [](auto& a, auto& b) { return a.priority > b.priority; });
```

### 4. Symbol Cache

**Goal:** Avoid repeated module enumeration for multiple symbol searches

**Implementation:**
```cpp
std::unordered_map<std::string, void*> g_symbol_cache;

void* find_symbol_cached(const char* name) {
    auto it = g_symbol_cache.find(name);
    if (it != g_symbol_cache.end()) {
        return it->second;  // Cache hit
    }
    
    void* symbol = find_symbol_in_any_module(name);
    g_symbol_cache[name] = symbol;
    return symbol;
}
```

**Note:** Only beneficial if searching for many symbols (not the case currently)

## Conclusion

The Windows implementation successfully replicates the Linux `rocprofiler-register` functionality by replacing `dlsym(RTLD_DEFAULT)` with a manual module enumeration and symbol search loop. While this adds ~50 lines of code, the resulting behavior is functionally equivalent to the Linux version.

Key achievements:
1. **Cross-platform API:** Same public interface as Linux version
2. **Graceful degradation:** Application runs without profiling if tool loading fails
3. **Thread-safe:** Proper synchronization for concurrent registration
4. **Diagnostic-friendly:** Verbose logging for debugging
5. **Efficient:** Minimal overhead (~microseconds for one-time initialization)

The implementation demonstrates that Windows dynamic library mechanisms, while different from Linux, provide all necessary capabilities for building a robust tool registration framework.
