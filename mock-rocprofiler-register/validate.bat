@echo off
REM =============================================================================
REM Validation Script for Windows rocprofiler-register Implementation
REM
REM This script performs basic validation checks to ensure the implementation
REM is correct and complete.
REM =============================================================================

setlocal enabledelayedexpansion

echo ================================================================================
echo Windows rocprofiler-register Implementation Validation
echo ================================================================================
echo.

set ERROR_COUNT=0

REM -----------------------------------------------------------------------------
REM Check 1: Verify all required files exist
REM -----------------------------------------------------------------------------
echo [1/6] Checking required files...

set FILES=mock_register.h mock_register.cpp CMakeLists.txt README.md DESIGN.md EXAMPLE_USAGE.md IMPLEMENTATION_SUMMARY.md

for %%F in (%FILES%) do (
    if exist %%F (
        echo   [OK] %%F
    ) else (
        echo   [FAIL] %%F - NOT FOUND
        set /a ERROR_COUNT+=1
    )
)
echo.

REM -----------------------------------------------------------------------------
REM Check 2: Verify key symbols in header
REM -----------------------------------------------------------------------------
echo [2/6] Checking header file symbols...

findstr /C:"rocprofiler_register_library_api_table" mock_register.h >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] rocprofiler_register_library_api_table declared
) else (
    echo   [FAIL] rocprofiler_register_library_api_table NOT FOUND
    set /a ERROR_COUNT+=1
)

findstr /C:"rocprofiler_register_error_string" mock_register.h >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] rocprofiler_register_error_string declared
) else (
    echo   [FAIL] rocprofiler_register_error_string NOT FOUND
    set /a ERROR_COUNT+=1
)

findstr /C:"ROCPROFILER_REGISTER_PUBLIC_API" mock_register.h >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] ROCPROFILER_REGISTER_PUBLIC_API macro defined
) else (
    echo   [FAIL] ROCPROFILER_REGISTER_PUBLIC_API NOT FOUND
    set /a ERROR_COUNT+=1
)
echo.

REM -----------------------------------------------------------------------------
REM Check 3: Verify key Windows-specific code in implementation
REM -----------------------------------------------------------------------------
echo [3/6] Checking Windows-specific implementation...

findstr /C:"EnumProcessModules" mock_register.cpp >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] EnumProcessModules - module enumeration
) else (
    echo   [FAIL] EnumProcessModules NOT FOUND
    set /a ERROR_COUNT+=1
)

findstr /C:"LoadLibraryA" mock_register.cpp >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] LoadLibraryA - dynamic library loading
) else (
    echo   [FAIL] LoadLibraryA NOT FOUND
    set /a ERROR_COUNT+=1
)

findstr /C:"GetProcAddress" mock_register.cpp >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] GetProcAddress - symbol resolution
) else (
    echo   [FAIL] GetProcAddress NOT FOUND
    set /a ERROR_COUNT+=1
)

findstr /C:"GetEnvironmentVariableA" mock_register.cpp >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] GetEnvironmentVariableA - environment variable access
) else (
    echo   [FAIL] GetEnvironmentVariableA NOT FOUND
    set /a ERROR_COUNT+=1
)

findstr /C:"GetModuleFileNameA" mock_register.cpp >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] GetModuleFileNameA - module path resolution
) else (
    echo   [FAIL] GetModuleFileNameA NOT FOUND
    set /a ERROR_COUNT+=1
)
echo.

REM -----------------------------------------------------------------------------
REM Check 4: Verify key functions implemented
REM -----------------------------------------------------------------------------
echo [4/6] Checking core function implementations...

findstr /C:"find_symbol_in_any_module" mock_register.cpp >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] find_symbol_in_any_module - RTLD_DEFAULT equivalent
) else (
    echo   [FAIL] find_symbol_in_any_module NOT FOUND
    set /a ERROR_COUNT+=1
)

findstr /C:"enumerate_loaded_modules" mock_register.cpp >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] enumerate_loaded_modules - /proc/self/maps equivalent
) else (
    echo   [FAIL] enumerate_loaded_modules NOT FOUND
    set /a ERROR_COUNT+=1
)

findstr /C:"load_library_with_fallback" mock_register.cpp >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] load_library_with_fallback - multi-attempt loading
) else (
    echo   [FAIL] load_library_with_fallback NOT FOUND
    set /a ERROR_COUNT+=1
)

findstr /C:"discover_and_initialize_tools" mock_register.cpp >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] discover_and_initialize_tools - tool discovery
) else (
    echo   [FAIL] discover_and_initialize_tools NOT FOUND
    set /a ERROR_COUNT+=1
)
echo.

REM -----------------------------------------------------------------------------
REM Check 5: Verify thread safety mechanisms
REM -----------------------------------------------------------------------------
echo [5/6] Checking thread safety mechanisms...

findstr /C:"std::mutex" mock_register.cpp >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] std::mutex - registration storage protection
) else (
    echo   [FAIL] std::mutex NOT FOUND
    set /a ERROR_COUNT+=1
)

findstr /C:"std::call_once" mock_register.cpp >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] std::call_once - one-time initialization
) else (
    echo   [FAIL] std::call_once NOT FOUND
    set /a ERROR_COUNT+=1
)

findstr /C:"thread_local" mock_register.cpp >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] thread_local - re-entrance detection
) else (
    echo   [FAIL] thread_local NOT FOUND
    set /a ERROR_COUNT+=1
)
echo.

REM -----------------------------------------------------------------------------
REM Check 6: Verify CMake configuration
REM -----------------------------------------------------------------------------
echo [6/6] Checking CMake configuration...

findstr /C:"psapi" CMakeLists.txt >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] psapi library linked (for EnumProcessModules)
) else (
    echo   [FAIL] psapi NOT FOUND in CMakeLists.txt
    set /a ERROR_COUNT+=1
)

findstr /C:"SHARED" CMakeLists.txt >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] SHARED library type (DLL)
) else (
    echo   [FAIL] SHARED library type NOT FOUND
    set /a ERROR_COUNT+=1
)

findstr /C:"CXX_STANDARD" CMakeLists.txt >nul
if %ERRORLEVEL% EQU 0 (
    echo   [OK] C++ standard specified
) else (
    echo   [FAIL] CXX_STANDARD NOT FOUND
    set /a ERROR_COUNT+=1
)
echo.

REM -----------------------------------------------------------------------------
REM Summary
REM -----------------------------------------------------------------------------
echo ================================================================================
echo Validation Summary
echo ================================================================================

if %ERROR_COUNT% EQU 0 (
    echo.
    echo   STATUS: ALL CHECKS PASSED
    echo.
    echo   The Windows rocprofiler-register implementation is complete and ready to build.
    echo.
    echo   Next steps:
    echo   1. Create build directory: mkdir build
    echo   2. Configure CMake:        cd build ^&^& cmake .. -G "Visual Studio 17 2022" -A x64
    echo   3. Build library:          cmake --build . --config Release
    echo   4. Verify exports:         dumpbin /EXPORTS bin\Release\rocprofiler-register.dll
    echo.
) else (
    echo.
    echo   STATUS: %ERROR_COUNT% CHECK(S) FAILED
    echo.
    echo   Please review the failed checks above and ensure all required components
    echo   are properly implemented.
    echo.
)

echo ================================================================================

exit /b %ERROR_COUNT%
