# Repository Setup Checklist

Use this checklist when creating the new repository.

## Files Ready for Git ✅

### Root Documentation
- [x] README.md - Comprehensive project overview
- [x] ARCHITECTURE.md - Detailed Windows implementation guide
- [x] QUICKSTART.md - 5-minute getting started guide
- [x] .gitignore - Build artifacts, DLLs, CSV traces excluded

### Build System
- [x] CMakeLists.txt - Root build configuration
- [x] All component CMakeLists.txt files

### Source Code
- [x] mock-rocprofiler-register/ - Windows registration layer
- [x] mock-hip-runtime/ - Mock HIP runtime
- [x] mock-sdk/ - Profiling SDK with CSV tracing
- [x] mock-tool/ - Example profiling tool
- [x] test-hip-app/ - Test application

### Tools
- [x] rocprofv3-poc.py - Python launcher script

### Component Documentation
- [x] Each component has its own README.md
- [x] Detailed design docs in key components

## Pre-Push Steps

### 1. Clean Build Directory
```bash
cd poc-v2
rm -rf build/
rm -f hip_trace.csv
rm -f *.log
```

### 2. Verify .gitignore
```bash
git status  # Should NOT show build/, *.dll, *.csv
```

### 3. Test Clean Build
```bash
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
cd ..
python rocprofv3-poc.py ./build/test-hip-app/Release/test-hip-app.exe
# Should create hip_trace.csv with 5 records
```

### 4. Check File Structure
```
poc-v2/
├── .gitignore
├── README.md
├── ARCHITECTURE.md
├── QUICKSTART.md
├── CMakeLists.txt
├── rocprofv3-poc.py
├── mock-rocprofiler-register/
│   ├── CMakeLists.txt
│   ├── mock_register.h
│   ├── mock_register.cpp
│   └── *.md (documentation)
├── mock-hip-runtime/
│   ├── CMakeLists.txt
│   ├── mock_hip_runtime.h
│   ├── mock_hip_runtime.cpp
│   └── README.md
├── mock-sdk/
│   ├── CMakeLists.txt
│   ├── mock_rocprofiler_sdk.h
│   ├── mock_rocprofiler_sdk.cpp
│   └── *.md (documentation)
├── mock-tool/
│   ├── CMakeLists.txt
│   ├── mock_tool.cpp
│   └── README.md
└── test-hip-app/
    ├── CMakeLists.txt
    ├── test_app.cpp
    └── README.md
```

## Git Commands for New Repository

### Option 1: New Repository from Scratch

```bash
cd poc-v2

# Initialize repo
git init
git add .
git commit -m "Initial commit: ROCProfiler SDK HIP Tracing POC for Windows

- Complete Windows-native implementation
- Clean architecture with proper component separation
- CSV tracing with nanosecond timestamps
- Python launcher (rocprofv3-poc.py)
- Full documentation

Features:
- LoadLibraryA/GetProcAddress for dynamic loading
- EnumProcessModules for symbol discovery
- In-place dispatch table wrapping
- High-resolution timestamp recording"

# Add remote (replace with your repo URL)
git remote add origin <your-repo-url>
git branch -M main
git push -u origin main
```

### Option 2: New Directory in Existing Repo

```bash
# From parent directory
git add poc-v2/
git commit -m "Add POC v2: Windows-native ROCProfiler SDK implementation"
git push
```

## Repository Settings Recommendations

### Branch Protection
- Require pull request reviews
- Require status checks to pass
- Include administrators

### README Badges (Optional)
Add to top of README.md:
```markdown
![Windows](https://img.shields.io/badge/OS-Windows%2010%2F11-blue)
![VS2022](https://img.shields.io/badge/Visual%20Studio-2022-purple)
![CMake](https://img.shields.io/badge/CMake-3.20+-green)
![Status](https://img.shields.io/badge/Status-POC-yellow)
```

### Topics/Tags
- rocprofiler
- hip
- windows
- profiling
- tracing
- amd
- rocm

## Post-Push Verification

### Clone Fresh Copy
```bash
git clone <your-repo-url> test-clone
cd test-clone/poc-v2
mkdir build && cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

### Run Test
```bash
cd ..
python rocprofv3-poc.py ./build/test-hip-app/Release/test-hip-app.exe
cat hip_trace.csv  # Should show 5 HIP API calls
```

## License Considerations

**Current Status**: No LICENSE file (internal AMD development)

**Options:**
1. Add internal AMD license
2. Keep proprietary until release decision
3. Use MIT/Apache 2.0 if open-sourcing

## Collaboration Setup

### For Team Members

1. Clone repository
2. Follow QUICKSTART.md
3. Build succeeds on first try ✅

### For Contributors

1. See README.md for architecture
2. See ARCHITECTURE.md for Windows details
3. Each component has README explaining its role

## Success Criteria ✅

- [x] Clean build from scratch
- [x] Test application runs
- [x] CSV trace generated
- [x] All documentation complete
- [x] .gitignore excludes build artifacts
- [x] No binaries in git
- [x] Code is well-commented
- [x] Architecture is clear

## Ready to Push!

Everything is prepared. Execute the git commands above to create your new repository.

---

**Recommended Repository Name:**
- `rocprofiler-sdk-windows-poc`
- `hip-tracing-windows-poc`
- `rocprof-v3-windows-port`

**Recommended Description:**
> Windows proof-of-concept for ROCProfiler SDK HIP API tracing. Demonstrates clean architecture with runtime discovery, dispatch table interception, and CSV trace generation.
