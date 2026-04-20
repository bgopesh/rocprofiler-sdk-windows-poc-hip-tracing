// Example Tool Using Mock ROCProfiler SDK
// This demonstrates how a profiling tool would use the SDK APIs

#include "mock_rocprofiler_sdk.h"
#include <cstdio>

// Global context and buffer handles
static rocprofiler_context_id_t g_context = nullptr;
static rocprofiler_buffer_id_t  g_buffer  = nullptr;

// Buffer callback - called when buffer is full or flushed
void
buffer_callback(rocprofiler_context_id_t context,
                rocprofiler_buffer_id_t  buffer,
                void*                    user_data)
{
    printf("[TOOL] Buffer callback invoked\n");
    printf("[TOOL]   Context: %p\n", context);
    printf("[TOOL]   Buffer: %p\n", buffer);
    printf("[TOOL]   User data: %p\n", user_data);

    // In a real tool, you would:
    // 1. Process records in the buffer
    // 2. Write them to file (CSV, JSON, etc.)
    // 3. Clear the buffer for reuse
}

// Tool initialization function
// This is called by rocprofiler-register when it discovers the tool library
extern "C" __declspec(dllexport) void*
rocprofiler_configure(uint32_t    version,
                      const char* runtime_version,
                      uint32_t    priority,
                      void*       client_id)
{
    printf("\n[TOOL] Tool configuration starting...\n");
    printf("[TOOL]   SDK Version: %u\n", version);
    printf("[TOOL]   Runtime Version: %s\n", runtime_version);
    printf("[TOOL]   Priority: %u\n", priority);

    // Step 1: Create profiling context
    rocprofiler_status_t status = rocprofiler_create_context(&g_context);
    if(status != ROCPROFILER_STATUS_SUCCESS)
    {
        printf("[TOOL ERROR] Failed to create context\n");
        return nullptr;
    }
    printf("[TOOL] Created profiling context: %p\n", g_context);

    // Step 2: Create trace buffer
    const size_t BUFFER_SIZE = 1024 * 1024;  // 1 MB buffer
    void*        user_data   = nullptr;       // Optional user data

    status = rocprofiler_create_buffer(
        g_context, BUFFER_SIZE, buffer_callback, user_data, &g_buffer);

    if(status != ROCPROFILER_STATUS_SUCCESS)
    {
        printf("[TOOL ERROR] Failed to create buffer\n");
        rocprofiler_destroy_context(g_context);
        return nullptr;
    }
    printf("[TOOL] Created trace buffer: %p (%zu bytes)\n", g_buffer, BUFFER_SIZE);

    // Step 3: Configure buffer tracing
    status = rocprofiler_configure_buffer_tracing(g_context, g_buffer);
    if(status != ROCPROFILER_STATUS_SUCCESS)
    {
        printf("[TOOL ERROR] Failed to configure buffer tracing\n");
        rocprofiler_destroy_context(g_context);
        return nullptr;
    }
    printf("[TOOL] Configured buffer tracing\n");

    // Step 4: Start profiling context
    status = rocprofiler_start_context(g_context);
    if(status != ROCPROFILER_STATUS_SUCCESS)
    {
        printf("[TOOL ERROR] Failed to start context\n");
        rocprofiler_destroy_context(g_context);
        return nullptr;
    }
    printf("[TOOL] Started profiling context\n");

    printf("[TOOL] Tool initialization complete!\n\n");

    // Return tool configuration (optional, can be nullptr)
    return nullptr;
}

// Tool cleanup function (called at DLL unload)
// In a real implementation, this would be in DllMain(DLL_PROCESS_DETACH)
void
tool_finalize()
{
    printf("\n[TOOL] Tool finalization starting...\n");

    if(g_context)
    {
        // Stop profiling
        rocprofiler_status_t status = rocprofiler_stop_context(g_context);
        if(status == ROCPROFILER_STATUS_SUCCESS)
        {
            printf("[TOOL] Stopped profiling context\n");
        }

        // Destroy context (automatically cleans up buffers)
        status = rocprofiler_destroy_context(g_context);
        if(status == ROCPROFILER_STATUS_SUCCESS)
        {
            printf("[TOOL] Destroyed profiling context\n");
        }

        g_context = nullptr;
        g_buffer  = nullptr;
    }

    printf("[TOOL] Tool finalization complete!\n\n");
}

// DLL entry point
#include <windows.h>

BOOL APIENTRY
DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    (void) hModule;
    (void) lpReserved;

    switch(reason)
    {
        case DLL_PROCESS_ATTACH:
            printf("[TOOL] Tool DLL loaded (waiting for rocprofiler_configure call)\n");
            break;

        case DLL_PROCESS_DETACH:
            tool_finalize();
            break;

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            break;
    }

    return TRUE;
}

// Example usage notes:
//
// 1. Build this as a DLL: example-tool.dll
// 2. Link against rocprofiler-sdk.dll (or use runtime loading)
// 3. Set environment variable: ROCPROFILER_REGISTER_LIBRARY=example-tool.dll
// 4. Run HIP application - register will find and call rocprofiler_configure()
// 5. Tool receives callbacks as HIP APIs are traced
//
// Expected flow:
//   test-hip-app.exe
//     -> loads mock-hip-runtime.dll
//       -> loads rocprofiler-register.dll
//         -> LoadLibraryA("example-tool.dll")
//         -> Searches for rocprofiler_configure
//         -> Calls rocprofiler_configure()
//           -> Tool creates context/buffers
//           -> Tool starts profiling
//     -> Application runs HIP code
//     -> HIP APIs are traced via SDK wrappers
//     -> Buffer callback fires when buffer fills
//     -> Tool processes trace records
//   (application exits)
//     -> DLL_PROCESS_DETACH
//       -> tool_finalize() stops profiling
