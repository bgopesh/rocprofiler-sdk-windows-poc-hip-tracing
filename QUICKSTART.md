# Quick Start Guide

Get up and running with ROCProfiler SDK HIP Tracing POC in 5 minutes.

## Prerequisites

- Windows 10/11
- Visual Studio 2022
- Python 3.8+

## Build (2 minutes)

```powershell
# Clone and navigate
cd poc-v2

# Build
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

## Run (1 minute)

```bash
# From poc-v2/ directory
python rocprofv3-poc.py ./build/test-hip-app/Release/test-hip-app.exe
```

## View Output

```bash
# Check CSV trace
cat hip_trace.csv
```

Expected output:
```csv
Domain,Function,Process_ID,Thread_ID,Correlation_ID,Start_Timestamp,End_Timestamp,Duration_ns
HIP,hipMalloc,...
HIP,hipMemcpy,...
HIP,hipLaunchKernel,...
```

## Troubleshooting

### Build Errors

**CMake not found:**
```powershell
# Use full path to VS-bundled CMake
"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -G "Visual Studio 17 2022" -A x64 ..
```

### Runtime Errors

**DLL not found:**
```powershell
# Copy all DLLs to app directory
cd build
cp bin/Release/*.dll test-hip-app/Release/
cp mock-sdk/Release/*.dll test-hip-app/Release/
cp mock-tool/Release/*.dll test-hip-app/Release/
```

**No trace file created:**
```powershell
# Enable verbose logging
$env:ROCPROFILER_REGISTER_VERBOSE="1"
python rocprofv3-poc.py ./build/test-hip-app/Release/test-hip-app.exe
```

## Next Steps

- Read [README.md](README.md) for full documentation
- See [ARCHITECTURE.md](ARCHITECTURE.md) for implementation details
- Integrate with real HIP runtime
