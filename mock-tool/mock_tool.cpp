#include <cstdio>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <vector>
#include <fstream>

#ifdef _WIN32
#    include <windows.h>
#    define TOOL_API __declspec(dllexport)
#else
#    define TOOL_API __attribute__((visibility("default")))
#endif

//=============================================================================
// Forward declarations for SDK API types (these would come from mock-sdk headers)
//=============================================================================

// Opaque context handle
typedef void* rocprofiler_context_id_t;
typedef void* rocprofiler_buffer_id_t;

// Callback kinds
enum rocprofiler_callback_tracing_kind_t
{
    ROCPROFILER_CALLBACK_TRACING_HIP_RUNTIME_API = 0,
    ROCPROFILER_CALLBACK_TRACING_HIP_COMPILER_API = 1,
};

// Callback phases
enum rocprofiler_callback_phase_t
{
    ROCPROFILER_CALLBACK_PHASE_ENTER = 0,
    ROCPROFILER_CALLBACK_PHASE_EXIT = 1,
};

// HIP API Operation IDs
enum HipApiId : uint32_t
{
    HIPAPI_hipMalloc            = 0,
    HIPAPI_hipFree              = 1,
    HIPAPI_hipMemcpy            = 2,
    HIPAPI_hipStreamCreate      = 3,
    HIPAPI_hipStreamDestroy     = 4,
    HIPAPI_hipLaunchKernel      = 5,
    HIPAPI_hipDeviceSynchronize = 6,
    HIPAPI_COUNT                = 7
};

// Callback data structure
struct rocprofiler_callback_tracing_record_t
{
    rocprofiler_callback_tracing_kind_t kind;
    uint32_t operation;
    rocprofiler_callback_phase_t phase;
    uint64_t correlation_id;
    uint64_t thread_id;
};

// Buffer tracing record (passed to buffered callback)
struct rocprofiler_buffer_tracing_hip_api_record_t
{
    uint64_t size;
    uint32_t kind;
    uint32_t operation;
    uint64_t correlation_id_internal;
    uint64_t start_timestamp;
    uint64_t end_timestamp;
    uint64_t thread_id;
};

// SDK API function signatures
typedef int (*rocprofiler_create_context_t)(rocprofiler_context_id_t* context_id);
typedef int (*rocprofiler_configure_callback_tracing_service_t)(
    rocprofiler_context_id_t context_id,
    rocprofiler_callback_tracing_kind_t kind,
    uint32_t* operations,
    size_t operations_count,
    void (*callback)(rocprofiler_callback_tracing_record_t, void*),
    void* user_data);
typedef int (*rocprofiler_configure_buffer_tracing_service_t)(
    rocprofiler_context_id_t context_id,
    rocprofiler_callback_tracing_kind_t kind,
    uint32_t* operations,
    size_t operations_count,
    rocprofiler_buffer_id_t buffer_id);
typedef int (*rocprofiler_create_buffer_t)(
    rocprofiler_buffer_id_t* buffer_id,
    size_t size,
    void (*callback)(rocprofiler_buffer_id_t, void*, size_t, void*),
    void* user_data);
typedef int (*rocprofiler_start_context_t)(rocprofiler_context_id_t context_id);

//=============================================================================
// Global state
//=============================================================================

namespace
{
// Tracing statistics
std::atomic<uint64_t> g_enter_count{0};
std::atomic<uint64_t> g_exit_count{0};
std::atomic<uint64_t> g_buffer_count{0};

// Output file for logging
std::mutex g_log_mutex;
std::ofstream g_log_file;

//=============================================================================
// Utility functions
//=============================================================================

const char*
get_api_name(uint32_t operation_id)
{
    static const char* names[] = {"hipMalloc",
                                  "hipFree",
                                  "hipMemcpy",
                                  "hipStreamCreate",
                                  "hipStreamDestroy",
                                  "hipLaunchKernel",
                                  "hipDeviceSynchronize"};
    return (operation_id < HIPAPI_COUNT) ? names[operation_id] : "Unknown";
}

const char*
get_phase_name(rocprofiler_callback_phase_t phase)
{
    return (phase == ROCPROFILER_CALLBACK_PHASE_ENTER) ? "ENTER" : "EXIT";
}

//=============================================================================
// Callback tracing callback
//=============================================================================

void
hip_api_callback(rocprofiler_callback_tracing_record_t record, void* user_data)
{
    (void) user_data;

    const char* api_name = get_api_name(record.operation);
    const char* phase_name = get_phase_name(record.phase);

    if(record.phase == ROCPROFILER_CALLBACK_PHASE_ENTER)
    {
        g_enter_count.fetch_add(1);
    }
    else
    {
        g_exit_count.fetch_add(1);
    }

    std::lock_guard<std::mutex> lock(g_log_mutex);
    if(g_log_file.is_open())
    {
        g_log_file << "[TOOL CALLBACK] " << phase_name << " " << api_name
                   << " (corr_id=" << record.correlation_id
                   << ", thread=" << record.thread_id << ")\n";
        g_log_file.flush();
    }

    printf("[TOOL CALLBACK] %s %s (corr_id=%llu, thread=%llu)\n",
           phase_name,
           api_name,
           (unsigned long long) record.correlation_id,
           (unsigned long long) record.thread_id);
}

//=============================================================================
// Buffer tracing callback
//=============================================================================

void
hip_api_buffer_callback(rocprofiler_buffer_id_t buffer_id,
                        void* records,
                        size_t num_records,
                        void* user_data)
{
    (void) buffer_id;
    (void) user_data;

    auto* record_array = static_cast<rocprofiler_buffer_tracing_hip_api_record_t*>(records);

    for(size_t i = 0; i < num_records; i++)
    {
        const auto& record = record_array[i];
        const char* api_name = get_api_name(record.operation);
        uint64_t duration_ns = record.end_timestamp - record.start_timestamp;

        g_buffer_count.fetch_add(1);

        std::lock_guard<std::mutex> lock(g_log_mutex);
        if(g_log_file.is_open())
        {
            g_log_file << "[TOOL BUFFER] " << api_name
                       << " (corr_id=" << record.correlation_id_internal
                       << ", duration=" << duration_ns << " ns)\n";
            g_log_file.flush();
        }

        printf("[TOOL BUFFER] %s (corr_id=%llu, duration=%llu ns)\n",
               api_name,
               (unsigned long long) record.correlation_id_internal,
               (unsigned long long) duration_ns);
    }
}

}  // anonymous namespace

//=============================================================================
// Exported rocprofiler_configure function
//=============================================================================

extern "C" {

// This is the tool entry point that rocprofiler-register will find and call
TOOL_API void*
rocprofiler_configure(uint32_t version,
                      const char* runtime_version,
                      uint32_t priority,
                      void* client_id)
{
    printf("\n========================================\n");
    printf("[MOCK TOOL] rocprofiler_configure called\n");
    printf("  Version: %u\n", version);
    printf("  Runtime: %s\n", runtime_version ? runtime_version : "unknown");
    printf("  Priority: %u\n", priority);
    printf("  Client ID: %p\n", client_id);
    printf("========================================\n\n");

    // Open log file
    const char* log_path = getenv("ROCPROFILER_TOOL_LOG");
    if(!log_path || strlen(log_path) == 0)
    {
        log_path = "mock_tool.log";
    }

    {
        std::lock_guard<std::mutex> lock(g_log_mutex);
        g_log_file.open(log_path);
        if(g_log_file.is_open())
        {
            g_log_file << "[MOCK TOOL] Tool configured\n";
            g_log_file << "  Version: " << version << "\n";
            g_log_file << "  Runtime: " << (runtime_version ? runtime_version : "unknown") << "\n";
            g_log_file.flush();
            printf("[MOCK TOOL] Logging to: %s\n", log_path);
        }
        else
        {
            printf("[MOCK TOOL] Warning: Failed to open log file: %s\n", log_path);
        }
    }

    // NOTE: In a real implementation, this would:
    // 1. Call rocprofiler_create_context() to create a profiling context
    // 2. Call rocprofiler_configure_callback_tracing_service() to set up callbacks
    //    or rocprofiler_configure_buffer_tracing_service() for buffered tracing
    // 3. Call rocprofiler_start_context() to activate profiling
    //
    // Example (pseudo-code, would need actual SDK API):
    //
    // rocprofiler_context_id_t context;
    // rocprofiler_create_context(&context);
    //
    // uint32_t operations[] = { HIPAPI_hipMalloc, HIPAPI_hipFree, ... };
    // rocprofiler_configure_callback_tracing_service(
    //     context,
    //     ROCPROFILER_CALLBACK_TRACING_HIP_RUNTIME_API,
    //     operations,
    //     HIPAPI_COUNT,
    //     hip_api_callback,
    //     nullptr);
    //
    // rocprofiler_start_context(context);
    //
    // For this POC, the SDK will handle the actual interception via
    // rocprofiler_set_api_table(), so we just log that we were called.

    printf("[MOCK TOOL] Tool initialization complete\n");
    printf("[MOCK TOOL] Ready to receive HIP API notifications\n\n");

    return nullptr;  // Return tool config (nullptr is acceptable for POC)
}

}  // extern "C"

//=============================================================================
// DllMain - Cleanup on unload
//=============================================================================

#ifdef _WIN32
BOOL APIENTRY
DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved)
{
    (void) hModule;
    (void) lpReserved;

    if(reason == DLL_PROCESS_DETACH)
    {
        printf("\n[MOCK TOOL] DLL_PROCESS_DETACH - Tool shutting down\n");
        printf("[MOCK TOOL] Statistics:\n");
        printf("  ENTER callbacks: %llu\n", (unsigned long long) g_enter_count.load());
        printf("  EXIT callbacks: %llu\n", (unsigned long long) g_exit_count.load());
        printf("  Buffer records: %llu\n", (unsigned long long) g_buffer_count.load());

        {
            std::lock_guard<std::mutex> lock(g_log_mutex);
            if(g_log_file.is_open())
            {
                g_log_file << "[MOCK TOOL] Tool shutting down\n";
                g_log_file << "  ENTER callbacks: " << g_enter_count.load() << "\n";
                g_log_file << "  EXIT callbacks: " << g_exit_count.load() << "\n";
                g_log_file << "  Buffer records: " << g_buffer_count.load() << "\n";
                g_log_file.close();
            }
        }
    }

    return TRUE;
}
#endif
