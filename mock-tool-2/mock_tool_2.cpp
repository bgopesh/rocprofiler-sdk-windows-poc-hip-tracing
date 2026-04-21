#include <cstdio>
#include <cstdint>
#include <atomic>
#include <thread>
#include <chrono>

#ifdef _WIN32
#    include <windows.h>
#    define TOOL_API __declspec(dllexport)
#else
#    define TOOL_API __attribute__((visibility("default")))
#endif

//=============================================================================
// Global state with thread ID tracking
//=============================================================================

namespace
{
std::atomic<int> g_call_count{0};
DWORD g_configure_thread_id = 0;

}  // anonymous namespace

//=============================================================================
// Exported rocprofiler_configure function - TOOL 2 VERSION
//=============================================================================

extern "C" {

TOOL_API void*
rocprofiler_configure(uint32_t    version,
                      const char* runtime_version,
                      uint32_t    priority,
                      void*       client_id)
{
    // Track which thread called us
    g_configure_thread_id = GetCurrentThreadId();
    int call_order = g_call_count.fetch_add(1);

    printf("\n========================================\n");
    printf("[MOCK TOOL 2] rocprofiler_configure called\n");
    printf("  Version: %u\n", version);
    printf("  Runtime: %s\n", runtime_version ? runtime_version : "unknown");
    printf("  Priority: %u\n", priority);
    printf("  Client ID ptr: %p\n", client_id);
    printf("  Call Order: %d (global counter)\n", call_order);
    printf("  Thread ID: %lu\n", (unsigned long)g_configure_thread_id);
    printf("  Timestamp: %llu ns\n",
           (unsigned long long)std::chrono::high_resolution_clock::now()
               .time_since_epoch().count());
    printf("========================================\n\n");

    // Simulate some work to expose potential race conditions
    printf("[MOCK TOOL 2] Simulating initialization work (100ms)...\n");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    printf("[MOCK TOOL 2] Initialization complete\n\n");

    return nullptr;  // Return NULL for client_id in this POC
}

}  // extern "C"

//=============================================================================
// DllMain - Track DLL lifecycle
//=============================================================================

BOOL APIENTRY
DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    (void) hModule;
    (void) lpReserved;

    switch(reason)
    {
        case DLL_PROCESS_ATTACH:
        {
            printf("\n[MOCK TOOL 2] DLL_PROCESS_ATTACH - Mock Tool 2 loaded\n");
            printf("[MOCK TOOL 2] Module handle: %p\n", hModule);
            printf("[MOCK TOOL 2] Thread ID: %lu\n\n", (unsigned long)GetCurrentThreadId());
            break;
        }

        case DLL_PROCESS_DETACH:
        {
            printf("\n[MOCK TOOL 2] DLL_PROCESS_DETACH - Tool 2 shutting down\n");
            printf("[MOCK TOOL 2] Total configure calls: %d\n", g_call_count.load());
            printf("[MOCK TOOL 2] Last configure thread: %lu\n\n",
                   (unsigned long)g_configure_thread_id);
            break;
        }
    }

    return TRUE;
}
