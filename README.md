# ROCProfiler SDK HIP Tracing POC for Windows

A proof-of-concept implementation demonstrating HIP API tracing on Windows using the ROCProfiler SDK architecture. This POC shows how to port the Linux-based ROCProfiler system to Windows with proper component separation and Windows-native APIs.

## Overview

This POC implements the complete ROCProfiler V3 stack for Windows:

- **Clean Architecture**: Application links only to HIP runtime, no direct SDK dependencies
- **Windows-Native**: Uses `LoadLibraryA`, `GetProcAddress`, `EnumProcessModules` instead of Linux `dlopen`/`dlsym`
- **Runtime Discovery**: SDK and tools loaded dynamically via environment variables
- **CSV Tracing**: Generates `hip_trace.csv` with timestamps, correlation IDs, and duration data
- **Python Launcher**: `rocprofv3-poc.py` mimics the real `rocprofv3` tool

## Architecture

```
┌─────────────────┐
│  test-hip-app   │  (Links ONLY to HIP runtime)
└────────┬────────┘
         │
         v
┌─────────────────┐
│  amdhip64.dll   │  (HIP Runtime - Links ONLY to register)
└────────┬────────┘
         │ rocprofiler_register_library_api_table()
         v
┌──────────────────────┐
│ rocprofiler-register │  (Discovery Layer)
│  - LoadLibraryA()    │  - Loads SDK from env var
│  - EnumProcessModules│  - Searches for symbols
│  - GetProcAddress    │  - Calls rocprofiler_configure()
└────────┬─────────────┘
         │ dlopen at runtime
         v
┌──────────────────────┐
│ rocprofiler-sdk.dll  │  (Profiling Infrastructure)
│  - Wraps dispatch    │  - Records traces
│    table in-place    │  - Writes hip_trace.csv
└──────────────────────┘
```

### Key Design Principles

1. **App → HIP Runtime only**: Test application has zero knowledge of profiling
2. **HIP Runtime → Register only**: No compile-time SDK dependency
3. **Register loads SDK**: Dynamic loading via `ROCPROFILER_REGISTER_LIBRARY`
4. **Symbol Discovery**: Windows module enumeration replaces Linux `/proc/self/maps`
5. **In-Place Wrapping**: SDK modifies dispatch table directly (not copy)

## Components

### 1. mock-rocprofiler-register/
Registration and discovery layer - the critical Windows porting component.

**Key Windows Adaptations:**
- `LoadLibraryA()` instead of `dlopen()`
- `GetProcAddress()` instead of `dlsym()`
- `EnumProcessModules()` instead of `/proc/self/maps` parsing
- `GetEnvironmentVariableA()` instead of `getenv()`
- Semicolon (`;`) path separator instead of colon (`:`)

### 2. mock-hip-runtime/
Mock HIP runtime library that applications link against.

**Responsibilities:**
- Provides HIP API: `hipMalloc`, `hipFree`, `hipMemcpy`, `hipLaunchKernel`, `hipDeviceSynchronize`
- Maintains dispatch table with function pointers
- Registers with `rocprofiler-register` during initialization
- Delegates all calls through (potentially wrapped) dispatch table

### 3. mock-sdk/
ROCProfiler SDK - provides the profiling infrastructure.

**Features:**
- Exports `rocprofiler_configure()` - discovered by register
- Exports `rocprofiler_set_api_table()` - receives dispatch tables
- Wraps HIP function pointers in-place
- Records high-resolution timestamps (nanoseconds)
- Writes CSV traces with correlation IDs

### 4. mock-tool/
Example profiling tool library.

**Purpose:**
- Demonstrates tool discovery mechanism
- Can be extended for custom profiling logic
- Currently used for testing the infrastructure

### 5. test-hip-app/
Simple HIP application for testing.

**Demonstrates:**
- Application with zero profiling dependencies
- Links only to `amdhip64.dll`
- All profiling happens transparently

## Building

### Prerequisites

- Windows 10/11
- Visual Studio 2022 (with C++ Desktop Development)
- CMake 3.20+ (bundled with VS)
- Python 3.8+ (for launcher script)

### Build Steps

```powershell
# Navigate to poc-v2 directory
cd poc-v2

# Create build directory
mkdir build
cd build

# Configure with CMake
cmake -G "Visual Studio 17 2022" -A x64 ..

# Build Release configuration
cmake --build . --config Release
```

### Build Output

```
poc-v2/build/
├── bin/Release/
│   ├── rocprofiler-register.dll
│   └── amdhip64.dll
├── mock-sdk/Release/
│   └── rocprofiler-sdk.dll
├── mock-tool/Release/
│   └── mock-tool.dll
└── test-hip-app/Release/
    └── test-hip-app.exe
```

## Usage

### Method 1: Using the Python Launcher (Recommended)

```bash
# Run from poc-v2/ directory
python rocprofv3-poc.py ./build/test-hip-app/Release/test-hip-app.exe
```

Output:
```
============================================================
ROCProfiler V3 POC - HIP Tracing on Windows
============================================================
Application: test-hip-app.exe
SDK Library: rocprofiler-sdk.dll
Output: hip_trace.csv
============================================================

=== POC v2: Clean HIP Application ===
...
[OK] Trace file created: hip_trace.csv
[OK] Number of traced API calls: 5
```

### Method 2: Manual Environment Setup

```powershell
# Copy DLLs to application directory
cd build
cp bin/Release/*.dll test-hip-app/Release/
cp mock-sdk/Release/*.dll test-hip-app/Release/
cp mock-tool/Release/*.dll test-hip-app/Release/

# Set environment variables
$env:ROCPROFILER_REGISTER_LIBRARY="rocprofiler-sdk.dll"
$env:ROCP_TOOL_LIBRARIES="mock-tool.dll"
$env:ROCPROFILER_REGISTER_VERBOSE="1"  # Optional: verbose logging

# Run application
cd test-hip-app/Release
./test-hip-app.exe
```

## Output

### CSV Trace File (hip_trace.csv)

```csv
Domain,Function,Process_ID,Thread_ID,Correlation_ID,Start_Timestamp,End_Timestamp,Duration_ns
HIP,hipMalloc,24344,28556,1,317452425280600,317452425293600,13000
HIP,hipMemcpy,24344,28556,2,317452425347000,317452425348300,1300
HIP,hipLaunchKernel,24344,28556,3,317452425361700,317452425363500,1800
HIP,hipMemcpy,24344,28556,4,317452425375900,317452425376400,500
HIP,hipFree,24344,28556,5,317452425388100,317452425400200,12100
```

**Fields:**
- **Domain**: API category (HIP, HSA, etc.)
- **Function**: API function name
- **Process_ID**: Windows process ID
- **Thread_ID**: Windows thread ID
- **Correlation_ID**: Unique ID for correlating async operations
- **Start_Timestamp**: Start time in nanoseconds since epoch
- **End_Timestamp**: End time in nanoseconds
- **Duration_ns**: Duration in nanoseconds (End - Start)

## Environment Variables

| Variable | Description | Default |
|----------|-------------|---------|
| `ROCPROFILER_REGISTER_LIBRARY` | Path to `rocprofiler-sdk.dll` | None (required) |
| `ROCP_TOOL_LIBRARIES` | Semicolon-separated tool DLL paths | None (optional) |
| `ROCPROFILER_REGISTER_VERBOSE` | Enable verbose logging (1/0) | 0 |

## Windows-Specific Implementation Details

### No RTLD_DEFAULT Equivalent

Linux can search all loaded modules with `dlsym(RTLD_DEFAULT, "symbol")`. Windows requires manual enumeration:

```cpp
HMODULE modules[1024];
DWORD needed;
EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed);
for (each module) {
    void* symbol = GetProcAddress(module, "symbol_name");
    if (symbol) return symbol;
}
```

### Multi-Attempt Library Loading

The register tries multiple strategies:
1. Direct path as-is
2. Add `.dll` extension if missing
3. Prepend current directory

This matches Linux's flexible `dlopen()` behavior.

### In-Place Dispatch Table Modification

Critical difference: The SDK must modify the dispatch table **in-place**, not create a copy:

```cpp
// WRONG (creates copy, runtime still uses original)
HipDispatchTable* wrapped = new HipDispatchTable(*original);
wrapped->hipMalloc = my_wrapper;
tables[0] = wrapped;  // ❌ Runtime won't see this

// CORRECT (modifies actual table)
HipDispatchTable* table = reinterpret_cast<HipDispatchTable*>(tables[0]);
table->hipMalloc = my_wrapper;  // ✅ Runtime uses this
```

## Testing

### Verify Symbol Export

Check DLL exports (requires Visual Studio command tools):

```cmd
dumpbin /EXPORTS build\mock-sdk\Release\rocprofiler-sdk.dll
```

Should show:
- `rocprofiler_configure`
- `rocprofiler_set_api_table`

### Verify Tracing

Run test and check CSV output:

```bash
python rocprofv3-poc.py ./build/test-hip-app/Release/test-hip-app.exe
cat hip_trace.csv  # Should show 5 HIP API calls
```

### Verbose Debugging

Enable verbose logging:

```powershell
$env:ROCPROFILER_REGISTER_VERBOSE="1"
python rocprofv3-poc.py ./build/test-hip-app/Release/test-hip-app.exe
```

Shows detailed information about:
- Module loading
- Symbol discovery
- Table registration
- API interception

## Comparison to Original POC

| Feature | Original POC | POC v2 |
|---------|-------------|--------|
| OS Support | Linux assumptions | Native Windows |
| App Dependencies | Links to SDK | Links only to HIP runtime |
| Symbol Discovery | `/proc/self/maps` | `EnumProcessModules` |
| Dynamic Loading | `dlopen`/`dlsym` | `LoadLibraryA`/`GetProcAddress` |
| SDK Loading | Compile-time link | Runtime environment var |
| CSV Output | ✅ | ✅ |
| Timestamp Precision | Nanoseconds | Nanoseconds |
| Launcher Script | Python | Python |
| Architecture | Monolithic | Clean separation |

## Known Limitations

1. **Mock Implementation**: This is a POC - HIP APIs are mocked (no real GPU operations)
2. **Limited APIs**: Only basic HIP APIs implemented (malloc, free, memcpy, kernel launch)
3. **Single Tool**: Only supports one tool at a time (can be extended)
4. **No Buffering**: Traces written immediately (no buffering optimization)

## Future Enhancements

- [ ] Real HIP runtime integration
- [ ] HSA API tracing support
- [ ] Multiple tool registration
- [ ] Buffered trace writing
- [ ] Binary trace format option
- [ ] Performance counters integration
- [ ] Kernel name resolution

## References

- **Design Document**: See `ARCHITECTURE.md` for detailed Windows porting notes
- **ROCProfiler SDK**: Based on AMD ROCm profiling architecture
- **Windows Profiling**: See reference document `rocp-register.txt`

## License

This is a proof-of-concept implementation for AMD internal development.

## Authors

- **Windows Port POC**: Developed for ROCProfiler SDK Windows porting effort
- **Based on**: Linux ROCProfiler V3 architecture

---

**Status**: ✅ Fully functional POC - Ready for production integration

Last Updated: 2026-04-20
