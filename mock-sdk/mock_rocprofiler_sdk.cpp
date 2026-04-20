#include "mock_rocprofiler_sdk.h"

#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <vector>

//=============================================================================
// Global State
//=============================================================================

namespace
{
// Context management
struct context_impl_t
{
    uint64_t                      id;
    bool                          is_active;
    std::vector<uint64_t>         buffer_ids;  // Store buffer IDs, not pointers
};

struct buffer_impl_t
{
    uint64_t                      id;
    size_t                        size;
    rocprofiler_buffer_callback_t callback;
    void*                         user_data;
    rocprofiler_context_id_t      context;
};

std::atomic<uint64_t>                            g_next_context_id{1};
std::atomic<uint64_t>                            g_next_buffer_id{1};
std::mutex                                       g_state_mutex;
std::unordered_map<uint64_t, context_impl_t*>   g_contexts;
std::unordered_map<uint64_t, buffer_impl_t*>    g_buffers;

// SDK initialization state
bool g_sdk_initialized = false;

// CSV trace file
std::ofstream g_trace_file;
std::mutex g_trace_mutex;
std::atomic<uint64_t> g_correlation_id{1};

// Timing helper
inline uint64_t get_timestamp_ns()
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
}

//=============================================================================
// HIP Dispatch Table Types (must match mock-hip-runtime)
//=============================================================================

enum hipError_t
{
    hipSuccess = 0,
    hipErrorInvalidValue = 1,
    hipErrorMemoryAllocation = 2,
    hipErrorInvalidHandle = 3
};

enum hipMemcpyKind
{
    hipMemcpyHostToHost = 0,
    hipMemcpyHostToDevice = 1,
    hipMemcpyDeviceToHost = 2,
    hipMemcpyDeviceToDevice = 3,
    hipMemcpyDefault = 4
};

struct dim3
{
    uint32_t x, y, z;
};

struct HipDispatchTable
{
    uint64_t size;
    hipError_t (*hipMalloc)(void** ptr, size_t size);
    hipError_t (*hipFree)(void* ptr);
    hipError_t (*hipMemcpy)(void* dst, const void* src, size_t count, hipMemcpyKind kind);
    hipError_t (*hipLaunchKernel)(const void* func, dim3 gridDim, dim3 blockDim,
                                   void** args, size_t sharedMem, void* stream);
};

// Original function pointers (saved before wrapping)
HipDispatchTable* g_original_hip_table = nullptr;

//=============================================================================
// CSV Tracing Helpers
//=============================================================================

void write_trace_header()
{
    std::lock_guard<std::mutex> lock(g_trace_mutex);
    if(g_trace_file.is_open())
    {
        g_trace_file << "Domain,Function,Process_ID,Thread_ID,Correlation_ID,Start_Timestamp,End_Timestamp,Duration_ns\n";
        g_trace_file.flush();
    }
}

void write_trace_record(const char* domain, const char* function, uint64_t correlation_id, uint64_t start_ns, uint64_t end_ns)
{
    std::lock_guard<std::mutex> lock(g_trace_mutex);
    if(g_trace_file.is_open())
    {
        uint64_t duration = end_ns - start_ns;
        g_trace_file << domain << ","
                     << function << ","
                     << GetCurrentProcessId() << ","
                     << GetCurrentThreadId() << ","
                     << correlation_id << ","
                     << start_ns << ","
                     << end_ns << ","
                     << duration << "\n";
        g_trace_file.flush();
    }
}

//=============================================================================
// HIP API Wrapper Functions
//=============================================================================

hipError_t wrapped_hipMalloc(void** ptr, size_t size)
{
    uint64_t cid = g_correlation_id.fetch_add(1);
    uint64_t start = get_timestamp_ns();

    hipError_t result = g_original_hip_table->hipMalloc(ptr, size);

    uint64_t end = get_timestamp_ns();
    write_trace_record("HIP", "hipMalloc", cid, start, end);

    return result;
}

hipError_t wrapped_hipFree(void* ptr)
{
    uint64_t cid = g_correlation_id.fetch_add(1);
    uint64_t start = get_timestamp_ns();

    hipError_t result = g_original_hip_table->hipFree(ptr);

    uint64_t end = get_timestamp_ns();
    write_trace_record("HIP", "hipFree", cid, start, end);

    return result;
}

hipError_t wrapped_hipMemcpy(void* dst, const void* src, size_t count, hipMemcpyKind kind)
{
    uint64_t cid = g_correlation_id.fetch_add(1);
    uint64_t start = get_timestamp_ns();

    hipError_t result = g_original_hip_table->hipMemcpy(dst, src, count, kind);

    uint64_t end = get_timestamp_ns();
    write_trace_record("HIP", "hipMemcpy", cid, start, end);

    return result;
}

hipError_t wrapped_hipLaunchKernel(const void* func, dim3 gridDim, dim3 blockDim,
                                     void** args, size_t sharedMem, void* stream)
{
    uint64_t cid = g_correlation_id.fetch_add(1);
    uint64_t start = get_timestamp_ns();

    hipError_t result = g_original_hip_table->hipLaunchKernel(func, gridDim, blockDim, args, sharedMem, stream);

    uint64_t end = get_timestamp_ns();
    write_trace_record("HIP", "hipLaunchKernel", cid, start, end);

    return result;
}

}  // anonymous namespace

//=============================================================================
// Exported SDK API Functions
//=============================================================================

extern "C" {

// Primary entry point - called by rocprofiler-register during tool discovery
ROCPROFILER_SDK_API void*
rocprofiler_configure(uint32_t    version,
                      const char* runtime_version,
                      uint32_t    priority,
                      void*       client_id)
{
    printf("\n[MOCK SDK] ========================================\n");
    printf("[MOCK SDK] rocprofiler_configure() called\n");
    printf("[MOCK SDK]   Version: %u\n", version);
    printf("[MOCK SDK]   Runtime: %s\n", runtime_version ? runtime_version : "unknown");
    printf("[MOCK SDK]   Priority: %u\n", priority);
    printf("[MOCK SDK]   Client ID: %p\n", client_id);
    printf("[MOCK SDK] ========================================\n\n");

    g_sdk_initialized = true;

    // Return configuration object (can be null for basic POC)
    return nullptr;
}

// Create a profiling context
ROCPROFILER_SDK_API rocprofiler_status_t
rocprofiler_create_context(rocprofiler_context_id_t* context)
{
    printf("[MOCK SDK] rocprofiler_create_context() called\n");

    if(!context)
    {
        printf("[MOCK SDK ERROR] Null context pointer\n");
        return ROCPROFILER_STATUS_ERROR;
    }

    std::lock_guard<std::mutex> lock(g_state_mutex);

    uint64_t context_id = g_next_context_id.fetch_add(1);
    auto*    ctx        = new context_impl_t();
    ctx->id             = context_id;
    ctx->is_active      = false;

    g_contexts[context_id] = ctx;
    *context               = reinterpret_cast<rocprofiler_context_id_t>(ctx);

    printf("[MOCK SDK] Created context ID: %llu\n", (unsigned long long) context_id);
    return ROCPROFILER_STATUS_SUCCESS;
}

// Start profiling on a context
ROCPROFILER_SDK_API rocprofiler_status_t
rocprofiler_start_context(rocprofiler_context_id_t context)
{
    printf("[MOCK SDK] rocprofiler_start_context() called\n");

    if(!context)
    {
        printf("[MOCK SDK ERROR] Null context\n");
        return ROCPROFILER_STATUS_ERROR;
    }

    std::lock_guard<std::mutex> lock(g_state_mutex);
    auto*                       ctx = reinterpret_cast<context_impl_t*>(context);

    if(ctx->is_active)
    {
        printf("[MOCK SDK WARNING] Context %llu already active\n",
               (unsigned long long) ctx->id);
        return ROCPROFILER_STATUS_SUCCESS;
    }

    ctx->is_active = true;
    printf("[MOCK SDK] Started context ID: %llu\n", (unsigned long long) ctx->id);
    return ROCPROFILER_STATUS_SUCCESS;
}

// Stop profiling on a context
ROCPROFILER_SDK_API rocprofiler_status_t
rocprofiler_stop_context(rocprofiler_context_id_t context)
{
    printf("[MOCK SDK] rocprofiler_stop_context() called\n");

    if(!context)
    {
        printf("[MOCK SDK ERROR] Null context\n");
        return ROCPROFILER_STATUS_ERROR;
    }

    std::lock_guard<std::mutex> lock(g_state_mutex);
    auto*                       ctx = reinterpret_cast<context_impl_t*>(context);

    if(!ctx->is_active)
    {
        printf("[MOCK SDK WARNING] Context %llu already inactive\n",
               (unsigned long long) ctx->id);
        return ROCPROFILER_STATUS_SUCCESS;
    }

    ctx->is_active = false;
    printf("[MOCK SDK] Stopped context ID: %llu\n", (unsigned long long) ctx->id);
    return ROCPROFILER_STATUS_SUCCESS;
}

// Destroy a profiling context
ROCPROFILER_SDK_API rocprofiler_status_t
rocprofiler_destroy_context(rocprofiler_context_id_t context)
{
    printf("[MOCK SDK] rocprofiler_destroy_context() called\n");

    if(!context)
    {
        printf("[MOCK SDK ERROR] Null context\n");
        return ROCPROFILER_STATUS_ERROR;
    }

    std::lock_guard<std::mutex> lock(g_state_mutex);
    auto*                       ctx = reinterpret_cast<context_impl_t*>(context);

    printf("[MOCK SDK] Destroying context ID: %llu\n", (unsigned long long) ctx->id);

    // Cleanup associated buffers
    for(auto buffer_id : ctx->buffer_ids)
    {
        auto it = g_buffers.find(buffer_id);
        if(it != g_buffers.end())
        {
            delete it->second;
            g_buffers.erase(it);
        }
    }
    ctx->buffer_ids.clear();

    // Remove from global map and delete
    g_contexts.erase(ctx->id);
    delete ctx;

    return ROCPROFILER_STATUS_SUCCESS;
}

// Create a trace buffer
ROCPROFILER_SDK_API rocprofiler_status_t
rocprofiler_create_buffer(rocprofiler_context_id_t      context,
                          size_t                        buffer_size,
                          rocprofiler_buffer_callback_t callback,
                          void*                         user_data,
                          rocprofiler_buffer_id_t*      buffer)
{
    printf("[MOCK SDK] rocprofiler_create_buffer() called\n");
    printf("[MOCK SDK]   Buffer size: %zu bytes\n", buffer_size);

    if(!context || !buffer)
    {
        printf("[MOCK SDK ERROR] Null pointer\n");
        return ROCPROFILER_STATUS_ERROR;
    }

    std::lock_guard<std::mutex> lock(g_state_mutex);
    auto*                       ctx = reinterpret_cast<context_impl_t*>(context);

    uint64_t buffer_id  = g_next_buffer_id.fetch_add(1);
    auto*    buf        = new buffer_impl_t();
    buf->id             = buffer_id;
    buf->size           = buffer_size;
    buf->callback       = callback;
    buf->user_data      = user_data;
    buf->context        = context;

    g_buffers[buffer_id] = buf;
    ctx->buffer_ids.push_back(buffer_id);
    *buffer = reinterpret_cast<rocprofiler_buffer_id_t>(buf);

    printf("[MOCK SDK] Created buffer ID: %llu\n", (unsigned long long) buffer_id);
    return ROCPROFILER_STATUS_SUCCESS;
}

// Configure buffer tracing
ROCPROFILER_SDK_API rocprofiler_status_t
rocprofiler_configure_buffer_tracing(rocprofiler_context_id_t context,
                                      rocprofiler_buffer_id_t  buffer)
{
    printf("[MOCK SDK] rocprofiler_configure_buffer_tracing() called\n");

    if(!context || !buffer)
    {
        printf("[MOCK SDK ERROR] Null pointer\n");
        return ROCPROFILER_STATUS_ERROR;
    }

    std::lock_guard<std::mutex> lock(g_state_mutex);
    auto*                       ctx = reinterpret_cast<context_impl_t*>(context);
    auto*                       buf = reinterpret_cast<buffer_impl_t*>(buffer);

    printf("[MOCK SDK] Configured buffer tracing for context %llu, buffer %llu\n",
           (unsigned long long) ctx->id,
           (unsigned long long) buf->id);

    return ROCPROFILER_STATUS_SUCCESS;
}

// API table interception hook
// This is called by rocprofiler-register after the SDK is loaded
// It allows the SDK to wrap HIP/HSA function pointers in the dispatch table
ROCPROFILER_SDK_API int
rocprofiler_set_api_table(const char* name,
                          uint64_t    lib_version,
                          uint64_t    lib_instance,
                          void**      tables,
                          uint64_t    num_tables)
{
    printf("\n[MOCK SDK] ========================================\n");
    printf("[MOCK SDK] rocprofiler_set_api_table() called\n");
    printf("[MOCK SDK]   Library: %s\n", name);
    printf("[MOCK SDK]   Version: %llu\n", (unsigned long long) lib_version);
    printf("[MOCK SDK]   Instance: %llu\n", (unsigned long long) lib_instance);
    printf("[MOCK SDK]   Num tables: %llu\n", (unsigned long long) num_tables);
    printf("[MOCK SDK] ========================================\n\n");

    if(!name || !tables)
    {
        printf("[MOCK SDK ERROR] Invalid parameters\n");
        return -1;
    }

    // Only handle HIP API table for this POC
    if(strcmp(name, "hip") == 0 && num_tables > 0)
    {
        // Open CSV trace file if not already open
        if(!g_trace_file.is_open())
        {
            g_trace_file.open("hip_trace.csv", std::ios::out | std::ios::trunc);
            if(g_trace_file.is_open())
            {
                printf("[MOCK SDK] Opened hip_trace.csv for writing\n");
                write_trace_header();
            }
            else
            {
                printf("[MOCK SDK ERROR] Failed to open hip_trace.csv\n");
                return -1;
            }
        }

        // Get the dispatch table from the runtime
        HipDispatchTable* hip_table = reinterpret_cast<HipDispatchTable*>(tables[0]);

        // Save original function pointers
        g_original_hip_table = new HipDispatchTable(*hip_table);

        // Now wrap the function pointers IN-PLACE
        // This modifies the actual table that the HIP runtime uses
        hip_table->hipMalloc = wrapped_hipMalloc;
        hip_table->hipFree = wrapped_hipFree;
        hip_table->hipMemcpy = wrapped_hipMemcpy;
        hip_table->hipLaunchKernel = wrapped_hipLaunchKernel;

        printf("[MOCK SDK] HIP API table wrapped successfully\n");
        printf("[MOCK SDK]   - hipMalloc intercepted\n");
        printf("[MOCK SDK]   - hipFree intercepted\n");
        printf("[MOCK SDK]   - hipMemcpy intercepted\n");
        printf("[MOCK SDK]   - hipLaunchKernel intercepted\n");
        printf("[MOCK SDK] Traces will be written to: hip_trace.csv\n\n");
    }
    else
    {
        printf("[MOCK SDK] API table interception ready for library: %s\n", name);
        printf("[MOCK SDK] Note: Only HIP tracing implemented in this POC\n\n");
    }

    return 0;  // Success
}

}  // extern "C"

//=============================================================================
// DllMain - Initialization/Cleanup
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
            printf("\n[MOCK SDK] DLL_PROCESS_ATTACH - Mock ROCProfiler SDK loaded\n");
            printf("[MOCK SDK] Module handle: %p\n", hModule);
            printf("[MOCK SDK] Waiting for rocprofiler_configure() call...\n\n");
            break;
        }

        case DLL_PROCESS_DETACH:
        {
            printf("\n[MOCK SDK] DLL_PROCESS_DETACH - Cleaning up...\n");

            // Close trace file
            if(g_trace_file.is_open())
            {
                g_trace_file.close();
                printf("[MOCK SDK] Closed hip_trace.csv\n");
            }

            // Cleanup all contexts and buffers
            std::lock_guard<std::mutex> lock(g_state_mutex);

            printf("[MOCK SDK] Cleaning up %zu buffers\n", g_buffers.size());
            for(auto& pair : g_buffers)
            {
                delete pair.second;
            }
            g_buffers.clear();

            printf("[MOCK SDK] Cleaning up %zu contexts\n", g_contexts.size());
            for(auto& pair : g_contexts)
            {
                delete pair.second;
            }
            g_contexts.clear();

            printf("[MOCK SDK] Mock ROCProfiler SDK unloaded\n\n");
            break;
        }

        case DLL_THREAD_ATTACH:
        case DLL_THREAD_DETACH:
            // No per-thread cleanup needed
            break;
    }

    return TRUE;
}
