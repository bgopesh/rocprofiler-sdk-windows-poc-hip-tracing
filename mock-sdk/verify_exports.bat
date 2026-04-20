@echo off
REM Verify that rocprofiler-sdk.dll exports the expected symbols

echo ========================================
echo ROCProfiler SDK DLL Export Verification
echo ========================================
echo.

set DLL_PATH=..\build\bin\Release\rocprofiler-sdk.dll

if not exist "%DLL_PATH%" (
    echo ERROR: DLL not found at %DLL_PATH%
    echo Please build the project first:
    echo   cd poc-v2
    echo   mkdir build ^&^& cd build
    echo   cmake .. -G "Visual Studio 17 2022" -A x64
    echo   cmake --build . --config Release
    exit /b 1
)

echo Found DLL: %DLL_PATH%
echo.
echo Checking exports...
echo.

REM Use dumpbin to show exports
dumpbin /EXPORTS "%DLL_PATH%" | findstr /C:"rocprofiler_"

echo.
echo ========================================
echo Expected exports:
echo   - rocprofiler_configure
echo   - rocprofiler_create_context
echo   - rocprofiler_start_context
echo   - rocprofiler_stop_context
echo   - rocprofiler_destroy_context
echo   - rocprofiler_create_buffer
echo   - rocprofiler_configure_buffer_tracing
echo   - rocprofiler_set_api_table
echo ========================================
