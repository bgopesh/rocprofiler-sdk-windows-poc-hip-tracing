# Mock Tool Library

This is the actual profiling tool component that gets discovered and loaded by the `rocprofiler-register` component.

## Purpose

The mock tool library demonstrates a complete profiling tool implementation that:

1. Exports `rocprofiler_configure()` - the entry point that rocprofiler-register discovers
2. Links to mock-sdk (NOT to HIP runtime or register)
3. Uses SDK APIs to set up profiling contexts
4. Logs HIP API calls when they are intercepted
5. Maintains statistics about traced operations

## Architecture

```
rocprofiler-register.dll
   |
   | (LoadLibraryA via ROCPROFILER_REGISTER_LIBRARY)
   v
mock-sdk.dll
   |
   | (link-time dependency)
   v
mock-tool.dll
   |
   | (GetProcAddress search finds)
   v
rocprofiler_configure() <-- Entry point called by register
```

## Key Components

### Exported Symbol

```cpp
extern "C" {
    __declspec(dllexport) void* rocprofiler_configure(
        uint32_t version,
        const char* runtime_version,
        uint32_t priority,
        void* client_id
    );
}
```

This function is discovered by rocprofiler-register through:
1. Register loads mock-sdk.dll (via ROCPROFILER_REGISTER_LIBRARY env var)
2. Loading mock-sdk brings in mock-tool.dll (link dependency)
3. Register calls `GetProcAddress()` on all loaded modules to find `rocprofiler_configure`
4. Register invokes the function to initialize the tool

### Callback Functions

The tool provides two types of callbacks:

1. **Callback Tracing** - Called on each API enter/exit
   - `hip_api_callback()` - Logs each HIP API call phase
   - Tracks correlation IDs and thread IDs
   - Counts enter/exit events

2. **Buffer Tracing** - Called with batches of completed records
   - `hip_api_buffer_callback()` - Processes buffered trace records
   - Calculates and logs API durations
   - More efficient for high-volume tracing

### Tool Initialization

When `rocprofiler_configure()` is called, the tool:
1. Opens a log file (ROCPROFILER_TOOL_LOG env var or "mock_tool.log")
2. Logs configuration parameters
3. Would register callbacks with SDK (in full implementation)
4. Returns tool configuration handle

### Cleanup

On DLL unload (DLL_PROCESS_DETACH):
1. Prints statistics summary
2. Writes final stats to log file
3. Closes log file

## Dependencies

- **mock-sdk**: Provides the profiling SDK APIs (linked at build time)
- **Windows SDK**: For DllMain and Windows-specific APIs

Does NOT link to:
- HIP runtime (tool is runtime-agnostic)
- rocprofiler-register (register loads the tool, not vice versa)

## Build

Built as part of poc-v2:

```bash
cmake --build build --config Release
```

Produces: `build/poc-v2/mock-tool/Release/mock-tool.dll`

## Environment Variables

- `ROCPROFILER_TOOL_LOG`: Path for tool log file (default: "mock_tool.log")

## Usage

The tool is loaded automatically when:
1. Application uses HIP APIs
2. `ROCPROFILER_REGISTER_LIBRARY` points to mock-sdk.dll
3. rocprofiler-register searches for and calls `rocprofiler_configure()`

No direct usage - it's injected into the process by the registration framework.

## Example Output

```
========================================
[MOCK TOOL] rocprofiler_configure called
  Version: 1
  Runtime: 6.0.0
  Priority: 0
  Client ID: 0x0000000000000000
========================================

[MOCK TOOL] Logging to: mock_tool.log
[MOCK TOOL] Tool initialization complete
[MOCK TOOL] Ready to receive HIP API notifications

[TOOL CALLBACK] ENTER hipMalloc (corr_id=1, thread=12345)
[TOOL CALLBACK] EXIT hipMalloc (corr_id=1, thread=12345)
[TOOL BUFFER] hipMalloc (corr_id=1, duration=7800 ns)

[MOCK TOOL] DLL_PROCESS_DETACH - Tool shutting down
[MOCK TOOL] Statistics:
  ENTER callbacks: 7
  EXIT callbacks: 7
  Buffer records: 7
```

## Implementation Notes

### Why Not Link to HIP Runtime?

The tool must be **runtime-agnostic**. It should work with any HIP implementation (real or mock) without compile-time dependencies. This matches the rocprofiler design principle of "inject, don't link".

### Why Link to SDK?

The SDK provides the profiling APIs that tools use to:
- Create profiling contexts
- Configure tracing services
- Register callbacks
- Control profiling sessions

This is the "uses" relationship: tool uses SDK APIs.

### Symbol Discovery

On Windows, `rocprofiler_configure` must be:
1. Exported with `__declspec(dllexport)`
2. In `extern "C"` block (no name mangling)
3. Present in a DLL loaded in the process space

Register finds it by:
```cpp
void* find_symbol_in_any_module(const char* symbol_name) {
    HMODULE modules[1024];
    EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed);
    
    for (each module) {
        void* symbol = GetProcAddress(module, symbol_name);
        if (symbol) return symbol;
    }
}
```

## Comparison with Real rocprofiler-sdk Tool

Real tools would:
- Use actual SDK APIs (rocprofiler_create_context, rocprofiler_configure_*, etc.)
- Implement buffering and output formatting
- Support configuration files
- Provide multiple output formats (CSV, JSON, Perfetto, etc.)
- Handle multi-GPU scenarios
- Implement filtering and sampling

This mock demonstrates the **interface contract** and **loading mechanism** without the full complexity.
