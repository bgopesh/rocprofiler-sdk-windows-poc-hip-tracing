#!/usr/bin/env python3
"""
rocprofv3-poc.py - ROCProfiler V3 POC Launcher for Windows

This script mimics the behavior of the real rocprofv3 tool by setting up
the environment for HIP tracing and launching the target application.

Usage:
    python rocprofv3-poc.py <application> [args...]

Example:
    python rocprofv3-poc.py ./build/test-hip-app/Release/test-hip-app.exe

Environment Variables Set:
    - ROCPROFILER_REGISTER_LIBRARY: Path to rocprofiler-sdk.dll
    - ROCP_TOOL_LIBRARIES: Path to mock-tool.dll (optional)
    - ROCPROFILER_REGISTER_VERBOSE: Enable verbose logging (1/0)

Output:
    - hip_trace.csv: CSV file with HIP API call traces
"""

import os
import sys
import subprocess
import pathlib

def find_dll(name, search_dirs):
    """Find a DLL in the given search directories."""
    for search_dir in search_dirs:
        dll_path = search_dir / name
        if dll_path.exists():
            return str(dll_path.resolve())
    return None

def main():
    if len(sys.argv) < 2:
        print("Usage: python rocprofv3-poc.py <application> [args...]")
        print("\nExample:")
        print("  python rocprofv3-poc.py ./build/test-hip-app/Release/test-hip-app.exe")
        sys.exit(1)

    # Get the application to run
    app_path = pathlib.Path(sys.argv[1])
    app_args = sys.argv[2:]

    if not app_path.exists():
        print(f"Error: Application not found: {app_path}")
        sys.exit(1)

    # Determine base directory (where this script is located)
    script_dir = pathlib.Path(__file__).parent.resolve()
    build_dir = script_dir / "build"

    # Search paths for DLLs
    dll_search_paths = [
        build_dir / "bin" / "Release",
        build_dir / "mock-sdk" / "Release",
        build_dir / "mock-tool" / "Release",
        app_path.parent,  # Same directory as the application
    ]

    # Find SDK DLL
    sdk_dll = find_dll("rocprofiler-sdk.dll", dll_search_paths)
    if not sdk_dll:
        print("Error: rocprofiler-sdk.dll not found in:")
        for path in dll_search_paths:
            print(f"  - {path}")
        sys.exit(1)

    # Find tool DLL (optional)
    tool_dll = find_dll("mock-tool.dll", dll_search_paths)

    # Set up environment
    env = os.environ.copy()
    env["ROCPROFILER_REGISTER_LIBRARY"] = sdk_dll

    if tool_dll:
        env["ROCP_TOOL_LIBRARIES"] = tool_dll
        print(f"[rocprofv3-poc] Tool library: {tool_dll}")

    # Check for verbose mode
    verbose = os.environ.get("ROCPROFILER_REGISTER_VERBOSE", "0")
    env["ROCPROFILER_REGISTER_VERBOSE"] = verbose

    print("=" * 60)
    print("ROCProfiler V3 POC - HIP Tracing on Windows")
    print("=" * 60)
    print(f"Application: {app_path.resolve()}")
    print(f"SDK Library: {sdk_dll}")
    print(f"Verbose: {verbose}")
    print(f"Output: hip_trace.csv")
    print("=" * 60)
    print()

    # Run the application
    try:
        cmd = [str(app_path.resolve())] + app_args
        result = subprocess.run(cmd, env=env, check=False)

        print()
        print("=" * 60)

        # Check if trace file was created
        trace_file = pathlib.Path("hip_trace.csv")
        if trace_file.exists():
            print(f"[OK] Trace file created: {trace_file.resolve()}")

            # Show some statistics
            with open(trace_file, 'r') as f:
                lines = f.readlines()
                num_records = len(lines) - 1  # Subtract header
                print(f"[OK] Number of traced API calls: {num_records}")

                # Show first few lines
                if num_records > 0:
                    print(f"\nFirst few records:")
                    print("".join(lines[:min(6, len(lines))]))
        else:
            print("[WARN] Warning: hip_trace.csv was not created")

        print("=" * 60)

        return result.returncode

    except KeyboardInterrupt:
        print("\nInterrupted by user")
        return 130
    except Exception as e:
        print(f"Error running application: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
