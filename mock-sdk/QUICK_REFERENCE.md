# Mock ROCProfiler SDK - Quick Reference

## Files Created

```
poc-v2/mock-sdk/
├── mock_rocprofiler_sdk.h          # Public API header (84 lines)
├── mock_rocprofiler_sdk.cpp        # Implementation (339 lines)
├── CMakeLists.txt                  # Build configuration (34 lines)
├── README.md                       # Detailed documentation (179 lines)
├── INTEGRATION.md                  # Integration guide (322 lines)
├── example_tool_usage.cpp          # Tool development example (171 lines)
├── verify_exports.bat              # Export verification script (40 lines)
└── QUICK_REFERENCE.md              # This file
```

## Exported Symbols (8 total)

### Primary Entry Point
- `rocprofiler_configure()` - SDK initialization, called by rocprofiler-register

### Context Management (4 APIs)
- `rocprofiler_create_context()` - Create profiling context
- `rocprofiler_start_context()` - Start profiling
- `rocprofiler_stop_context()` - Stop profiling  
- `rocprofiler_destroy_context()` - Cleanup context

### Buffer Tracing (2 APIs)
- `rocprofiler_create_buffer()` - Create trace buffer with callback
- `rocprofiler_configure_buffer_tracing()` - Attach buffer to context

### API Interception (1 API)
- `rocprofiler_set_api_table()` - Modify HIP/HSA dispatch tables

## Build Commands

```bash
# From poc-v2 directory
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

**Output**: `build/bin/Release/rocprofiler-sdk.dll`

## Verification

```bash
# Check exports
cd poc-v2/mock-sdk
verify_exports.bat

# Or manually
dumpbin /EXPORTS ..\build\bin\Release\rocprofiler-sdk.dll | findstr rocprofiler_
```

## Runtime Loading

### Method 1: Automatic Discovery
```cpp
// In rocprofiler-register.dll
HMODULE sdk = LoadLibraryA("rocprofiler-sdk.dll");
```

### Method 2: Environment Variable
```batch
set ROCPROFILER_REGISTER_LIBRARY=C:\path\to\rocprofiler-sdk.dll
```

## Tool Development Pattern

```cpp
// 1. Export configuration function
extern "C" __declspec(dllexport) void*
rocprofiler_configure(uint32_t version, const char* runtime_version,
                      uint32_t priority, void* client_id)
{
    // 2. Create context
    rocprofiler_context_id_t ctx;
    rocprofiler_create_context(&ctx);
    
    // 3. Create buffer with callback
    rocprofiler_buffer_id_t buf;
    rocprofiler_create_buffer(ctx, 1024*1024, my_callback, nullptr, &buf);
    
    // 4. Configure tracing
    rocprofiler_configure_buffer_tracing(ctx, buf);
    
    // 5. Start profiling
    rocprofiler_start_context(ctx);
    
    return nullptr;
}

// 6. Implement callback
void my_callback(rocprofiler_context_id_t ctx,
                 rocprofiler_buffer_id_t buf,
                 void* user_data)
{
    // Process trace records, write to file, etc.
}
```

## Key Design Decisions

1. **Standalone DLL**: Zero dependencies (except Windows system DLLs)
2. **Explicit Exports**: Uses `__declspec(dllexport)`, not `/WINDOWS_EXPORT_ALL_SYMBOLS`
3. **Console Logging**: All operations print to stdout with `[MOCK SDK]` prefix
4. **Thread-Safe**: Context/buffer management protected by mutex
5. **Opaque Handles**: Contexts and buffers use opaque pointers

## Differences from Real ROCProfiler SDK

This mock provides:
- API structure and symbols
- Context/buffer management scaffolding
- Console logging for verification

Missing (implement for production use):
- Actual dispatch table wrapping
- Trace record collection
- HSA runtime integration
- Hardware counter collection
- CSV/JSON output
- Correlation ID tracking
- Double-buffered tracing

For **full tracing**, see `../mock-sdk-tracer/` which implements dispatch table wrapping.

## Testing Workflow

```bash
# 1. Build all components
cd poc-v2/build
cmake --build . --config Release

# 2. Run test app
cd bin/Release
.\test-hip-app.exe

# 3. Verify SDK was loaded
# Look for console output:
#   [MOCK SDK] DLL_PROCESS_ATTACH
#   [MOCK SDK] rocprofiler_configure() called
#   [MOCK SDK] rocprofiler_set_api_table() called
```

## Environment Setup

```batch
REM Option 1: Add to PATH
set PATH=%PATH%;C:\path\to\poc-v2\build\bin\Release

REM Option 2: Explicit library path
set ROCPROFILER_REGISTER_LIBRARY=C:\path\to\rocprofiler-sdk.dll

REM Option 3: Copy DLL to app directory
copy build\bin\Release\rocprofiler-sdk.dll build\bin\Release\
```

## Common Issues

### SDK Not Loaded
**Problem**: `LoadLibraryA()` fails  
**Solution**: Check DLL exists, verify no missing dependencies with `dumpbin /DEPENDENTS`

### Symbol Not Found
**Problem**: `GetProcAddress()` returns null  
**Solution**: Run `verify_exports.bat`, ensure `extern "C"` and `__declspec(dllexport)`

### Wrong Function Signature
**Problem**: Access violation when calling function  
**Solution**: Verify typedef matches implementation exactly

### DLL Already Loaded
**Problem**: Changes not reflected after rebuild  
**Solution**: Ensure application process fully terminates before testing

## Next Steps

1. **Basic Integration**: Verify SDK loads and symbols are found
2. **Tool Development**: Create custom tool using `example_tool_usage.cpp`
3. **Add Tracing**: Implement dispatch table wrapping in `rocprofiler_set_api_table()`
4. **Buffer Processing**: Write trace records to CSV/JSON in buffer callback
5. **Extend APIs**: Add more ROCProfiler SDK APIs as needed

## Related Documentation

- `README.md` - Comprehensive SDK documentation
- `INTEGRATION.md` - Integration with rocprofiler-register
- `example_tool_usage.cpp` - Tool development example
- `../../ARCHITECTURE.md` - System architecture
- `../../ROCPROFILER_SDK_WINDOWS_PORT.md` - Porting guide
