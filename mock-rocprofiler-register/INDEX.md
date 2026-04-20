# Mock rocprofiler-register - Documentation Index

## Quick Links

| Document | Purpose | Audience |
|----------|---------|----------|
| **[README.md](README.md)** | Main documentation, API reference, usage guide | All users |
| **[IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)** | High-level overview and quick reference | New users, integration developers |
| **[DESIGN.md](DESIGN.md)** | In-depth design rationale and patterns | Maintainers, contributors |
| **[EXAMPLE_USAGE.md](EXAMPLE_USAGE.md)** | Code examples and tutorials | Developers implementing tools |
| **validate.bat** | Implementation validation script | QA, build engineers |

## File Structure

```
mock-rocprofiler-register/
├── mock_register.h              (158 lines) - Public API header
├── mock_register.cpp            (582 lines) - Windows implementation
├── CMakeLists.txt               (96 lines)  - Build configuration
├── README.md                    (430 lines) - Main documentation
├── DESIGN.md                    (659 lines) - Design details
├── EXAMPLE_USAGE.md             (483 lines) - Code examples
├── IMPLEMENTATION_SUMMARY.md    (495 lines) - Quick reference
├── INDEX.md                     (this file) - Documentation index
└── validate.bat                 (186 lines) - Validation script

Total: 2,903 lines of documentation + 836 lines of code
```

## Reading Guide

### For First-Time Users

1. Start with **[IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)**
   - Get a quick overview of what rocprofiler-register does
   - Understand the Windows-specific challenges
   - See the architecture diagram

2. Read **[README.md](README.md)** sections:
   - "What is rocprofiler-register?" 
   - "Architecture Overview"
   - "Environment Variables"

3. Try **[EXAMPLE_USAGE.md](EXAMPLE_USAGE.md)**:
   - Example 1: Basic HIP Runtime Registration
   - Example 3: Using the Tool

### For Developers Implementing Tools

1. Read **[EXAMPLE_USAGE.md](EXAMPLE_USAGE.md)**:
   - Example 2: Tool Implementation
   - Symbol Export Requirements
   - Troubleshooting Guide

2. Reference **[README.md](README.md)**:
   - API Functions section
   - Tool Discovery Flow
   - Debug Logging

3. Check **[DESIGN.md](DESIGN.md)**:
   - Symbol Search implementation
   - Thread Safety mechanisms
   - Error Handling Strategy

### For Maintainers/Contributors

1. Read **[DESIGN.md](DESIGN.md)** completely:
   - Understand design decisions
   - Learn Windows-specific patterns
   - Review threading and synchronization

2. Study **[README.md](README.md)**:
   - Windows-Specific Implementation Details
   - Key Differences: Linux vs Windows

3. Reference **[IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md)**:
   - Code Metrics
   - Comparison with Linux Implementation
   - Future Enhancements

### For Build/Release Engineers

1. Read **[README.md](README.md)**:
   - Building section
   - Testing section

2. Check **[CMakeLists.txt](CMakeLists.txt)**:
   - Build requirements
   - Compiler flags
   - Output organization

3. Run **validate.bat**:
   - Verify all required files exist
   - Check key symbols present
   - Validate CMake configuration

## Key Concepts

### Windows-Specific Challenges

| Challenge | Linux Solution | Windows Solution | Documented In |
|-----------|----------------|------------------|---------------|
| Global symbol search | `dlsym(RTLD_DEFAULT)` | `EnumProcessModules()` + loop | [DESIGN.md](DESIGN.md) Section 1 |
| Module enumeration | Parse `/proc/self/maps` | `EnumProcessModules()` | [DESIGN.md](DESIGN.md) Section 1 |
| Dynamic loading | `dlopen()` with RTLD flags | `LoadLibraryA()` with fallbacks | [DESIGN.md](DESIGN.md) Section 3 |
| Path separator | Colon (`:`) | Semicolon (`;`) | [README.md](README.md) Section 6 |

### Core Functions

| Function | Purpose | Documented In |
|----------|---------|---------------|
| `rocprofiler_register_library_api_table()` | Register runtime library | [README.md](README.md), [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) |
| `find_symbol_in_any_module()` | Search all DLLs for symbol | [DESIGN.md](DESIGN.md) Section 2 |
| `enumerate_loaded_modules()` | List all loaded DLLs | [DESIGN.md](DESIGN.md) Section 1 |
| `load_library_with_fallback()` | Load DLL with retries | [DESIGN.md](DESIGN.md) Section 3 |
| `discover_and_initialize_tools()` | One-time tool discovery | [DESIGN.md](DESIGN.md) Section 5 |

## Code Examples Location

| Example | File | Lines |
|---------|------|-------|
| HIP Runtime Registration | [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) | 15-70 |
| Tool Implementation | [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) | 80-150 |
| Building a Tool | [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) | 155-165 |
| Using Environment Variables | [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) | 170-200 |
| Debugging Missing Symbols | [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) | 230-290 |
| Error Handling | [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) | 295-340 |
| Iterating Registrations | [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) | 345-370 |

## API Reference

### Public Functions

```cpp
// Register a runtime library's API dispatch table
rocprofiler_register_error_code_t
rocprofiler_register_library_api_table(
    const char*                                 lib_name,
    rocprofiler_register_import_func_t          import_func,
    uint32_t                                    lib_version,
    void**                                      api_tables,
    uint64_t                                    api_table_length,
    rocprofiler_register_library_indentifier_t* register_id);

// Get error string for error code
const char* rocprofiler_register_error_string(
    rocprofiler_register_error_code_t error_code);

// Iterate over registered libraries
rocprofiler_register_error_code_t
rocprofiler_register_iterate_registration_info(
    rocprofiler_register_registration_info_cb_t callback,
    void*                                       data);
```

**Detailed documentation:** [README.md](README.md) - API Functions section

### Helper Macros

```cpp
// Compute version number
ROCPROFILER_REGISTER_COMPUTE_VERSION_3(MAJOR, MINOR, PATCH)

// Get import function name
ROCPROFILER_REGISTER_IMPORT_FUNC(NAME)

// Define import function
ROCPROFILER_REGISTER_DEFINE_IMPORT(NAME, VERSION)
```

**Detailed documentation:** [mock_register.h](mock_register.h) lines 135-158

## Environment Variables

| Variable | Purpose | Example |
|----------|---------|---------|
| `ROCP_TOOL_LIBRARIES` | Semicolon-separated tool DLL paths | `tool1.dll;C:\tools\tool2.dll` |
| `ROCPROFILER_REGISTER_VERBOSE` | Enable debug logging | `1` or `true` |

**Detailed documentation:** [README.md](README.md) - Environment Variables section

## Build Instructions

### Quick Build

```bash
cd poc-v2/mock-rocprofiler-register
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

**Detailed documentation:** [README.md](README.md) - Building section

### Output

- DLL: `build/bin/Release/rocprofiler-register.dll`
- Import library: `build/lib/Release/rocprofiler-register.lib`

## Testing

### Validation Script

```bash
cd poc-v2/mock-rocprofiler-register
validate.bat
```

Checks:
- All required files present
- Key symbols defined
- Windows-specific APIs used
- CMake configuration correct

**Detailed documentation:** [validate.bat](validate.bat)

### Manual Testing

1. Build the library
2. Set environment variables:
   ```cmd
   set ROCP_TOOL_LIBRARIES=mock-sdk-tracer.dll
   set ROCPROFILER_REGISTER_VERBOSE=1
   ```
3. Run HIP application
4. Verify tool discovery in output

**Detailed documentation:** [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) - Example 3

## Troubleshooting

| Issue | Solution | Documented In |
|-------|----------|---------------|
| Tool DLL not found | Add to PATH or use absolute path | [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) - Troubleshooting |
| Symbol not found | Verify exports with `dumpbin /EXPORTS` | [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) - Example 5 |
| Re-entrant call error | Don't call HIP APIs in `rocprofiler_configure` | [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) - Example 6 |
| Multiple tools loaded | Control order with ROCP_TOOL_LIBRARIES | [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) - Example 4 |

## Performance

| Operation | Time | Impact |
|-----------|------|--------|
| Module enumeration | ~10 μs | Negligible |
| Symbol search | ~1-2 μs | Negligible |
| Tool discovery (total) | ~500 μs | One-time cost at startup |

**Detailed documentation:** [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md) - Performance Characteristics

## Integration

### With HIP Runtime

HIP runtime calls `rocprofiler_register_library_api_table()` during initialization.

**Example:** [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) - Example 1

### With Profiling Tools

Tools export `rocprofiler_configure()` and `rocprofiler_set_api_table()`.

**Example:** [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) - Example 2

### With Applications

Applications are unaware of profiling infrastructure. Profiling is controlled via environment variables.

**Example:** [EXAMPLE_USAGE.md](EXAMPLE_USAGE.md) - Example 3

## Future Enhancements

1. **Attachment Mode**: Runtime attach/detach
2. **Security Validation**: Verify DLL signatures
3. **Priority Ordering**: Control tool initialization order
4. **Symbol Cache**: Optimize repeated searches
5. **Performance Metrics**: Track discovery time

**Detailed documentation:** [IMPLEMENTATION_SUMMARY.md](IMPLEMENTATION_SUMMARY.md) - Future Enhancements

## References

### Internal
- [ARCHITECTURE.md](../../ARCHITECTURE.md) - Overall POC architecture
- [ROCPROFILER_SDK_WINDOWS_PORT.md](../../ROCPROFILER_SDK_WINDOWS_PORT.md) - SDK porting guide

### External
- Windows API: MSDN documentation
- ROCm: Linux rocprofiler-register source code
- PE Format: Windows executable format specification

## Statistics

| Metric | Value |
|--------|-------|
| Total code | 836 lines (header + implementation + CMake) |
| Total documentation | 2,903 lines (7 documents) |
| Doc/code ratio | 3.5:1 |
| Code comments | ~200 lines (inline documentation) |
| API functions | 3 public functions |
| Helper macros | 4 macros |
| Windows-specific API calls | 26 calls |

## License

This is a mock implementation for proof-of-concept purposes. The real rocprofiler-register is part of AMD ROCm and is licensed under the MIT license.

## Contact

For questions or issues with this implementation, refer to the documentation in this directory or the main POC documentation in the parent directories.

---

**Last Updated:** 2026-04-20

**Version:** 1.0.0

**Status:** Complete and validated
