//=============================================================================
// Mock rocprofiler-register - Windows Implementation
//
// This file implements the Windows-specific version of rocprofiler-register,
// which is responsible for:
// 1. Accepting API table registrations from runtime libraries (HIP, HSA, etc.)
// 2. Discovering and loading profiling tools dynamically
// 3. Coordinating tool initialization and API table modification
//
// KEY WINDOWS-SPECIFIC CHANGES FROM LINUX:
// - Uses LoadLibraryA() instead of dlopen()
// - Uses GetProcAddress() instead of dlsym()
// - Uses EnumProcessModules() instead of parsing /proc/self/maps
// - No RTLD_DEFAULT equivalent - must enumerate all loaded modules manually
// - Uses GetEnvironmentVariableA() instead of getenv()
//
// ARCHITECTURE:
// Linux:   /proc/self/maps + dlsym(RTLD_DEFAULT) for global symbol search
// Windows: EnumProcessModules + GetProcAddress for each module
//
// REFERENCE: ARCHITECTURE.md sections on "Symbol Search Across All Loaded DLLs"
//=============================================================================

#include "mock_register.h"
#include "pe_parser.h"

#include <windows.h>
#include <psapi.h>  // For EnumProcessModules

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

//=============================================================================
// Debug Logging (controlled by ROCPROFILER_REGISTER_VERBOSE env var)
//=============================================================================

namespace
{
bool g_verbose = false;

void
init_verbose_mode()
{
    static bool initialized = false;
    if(!initialized)
    {
        char buffer[32] = {0};
        DWORD result = GetEnvironmentVariableA("ROCPROFILER_REGISTER_VERBOSE", buffer, sizeof(buffer));
        g_verbose    = (result > 0 && (strcmp(buffer, "1") == 0 || strcmp(buffer, "true") == 0));
        initialized  = true;
    }
}

#define VERBOSE_LOG(...)                                                                 \
    do                                                                                   \
    {                                                                                    \
        init_verbose_mode();                                                             \
        if(g_verbose)                                                                    \
        {                                                                                \
            printf("[ROCPROF-REG] ");                                                    \
            printf(__VA_ARGS__);                                                         \
            printf("\n");                                                                \
        }                                                                                \
    } while(0)

}  // namespace

//=============================================================================
// Registration Storage
//=============================================================================

namespace
{
/// @brief Stores information about a single library registration
struct registration_record_t
{
    std::string                                name;
    rocprofiler_register_import_func_t         import_func;
    uint32_t                                   version;
    std::vector<void*>                         api_tables;  // Pointers to dispatch tables
    rocprofiler_register_library_indentifier_t id;
};

std::vector<registration_record_t> g_registrations;
std::mutex                         g_registration_mutex;
uint64_t                           g_next_handle = 1;

// Thread-local re-entrance detection
thread_local bool g_in_registration = false;

}  // namespace

//=============================================================================
// Tool Discovery Function Prototypes (used by SDK/tools)
//=============================================================================

/// @brief Tool configuration callback (defined by rocprofiler-sdk or tool)
///
/// This function is called once per tool to initialize it. The tool receives:
/// - version: ROCProfiler SDK version
/// - runtime_version: HIP/HSA runtime version string
/// - priority: Tool priority (for ordering when multiple tools exist)
/// - client_id: Opaque ID for this tool instance
///
/// WINDOWS NOTE: This symbol is discovered using GetProcAddress() across all
/// loaded modules (since Windows has no RTLD_DEFAULT equivalent).
typedef int (*rocprofiler_configure_func_t)(uint32_t    version,
                                            const char* runtime_version,
                                            uint32_t    priority,
                                            uint64_t*   client_id);

/// @brief API table modification callback (defined by rocprofiler-sdk)
///
/// This function is called once for each registered library (HIP, HSA, etc.)
/// to allow the tool to wrap API functions by modifying dispatch tables.
///
/// WINDOWS NOTE: This symbol is discovered using GetProcAddress() in the
/// SDK DLL loaded from ROCP_TOOL_LIBRARIES environment variable.
typedef int (*rocprofiler_set_api_table_func_t)(const char* lib_name,
                                                uint64_t    lib_version,
                                                uint64_t    lib_instance,
                                                void**      tables,
                                                uint64_t    num_tables);

//=============================================================================
// Windows-Specific: Enumerate Loaded Modules
//
// LINUX EQUIVALENT: Parse /proc/self/maps to get loaded shared libraries
// WINDOWS: Use EnumProcessModules() to enumerate all DLLs in the process
//
// This is necessary because Windows has no global symbol search like
// dlsym(RTLD_DEFAULT, symbol). We must iterate through all modules and
// call GetProcAddress() on each one.
//=============================================================================

namespace
{
/// @brief Get list of all loaded module handles
/// @return Vector of HMODULE handles for all DLLs loaded in the process
std::vector<HMODULE>
enumerate_loaded_modules()
{
    std::vector<HMODULE> modules;
    HMODULE              module_handles[1024];
    DWORD                bytes_needed = 0;

    // Get all module handles
    // MSDN: EnumProcessModules returns modules in load order
    if(!EnumProcessModules(
           GetCurrentProcess(), module_handles, sizeof(module_handles), &bytes_needed))
    {
        VERBOSE_LOG("EnumProcessModules failed: error %lu", GetLastError());
        return modules;
    }

    DWORD module_count = bytes_needed / sizeof(HMODULE);
    modules.assign(module_handles, module_handles + module_count);

    VERBOSE_LOG("Enumerated %lu loaded modules", (unsigned long) module_count);

    // Dump module names in verbose mode
    if(g_verbose)
    {
        for(DWORD i = 0; i < module_count; ++i)
        {
            char module_path[MAX_PATH] = {0};
            if(GetModuleFileNameA(module_handles[i], module_path, sizeof(module_path)))
            {
                // Extract just the filename from full path
                const char* filename = strrchr(module_path, '\\');
                filename             = filename ? (filename + 1) : module_path;
                VERBOSE_LOG("  [%2lu] %s", (unsigned long) i, filename);
            }
        }
    }

    return modules;
}

/// @brief Search for a symbol across all loaded modules
///
/// LINUX EQUIVALENT: dlsym(RTLD_DEFAULT, symbol_name)
/// WINDOWS: Enumerate all modules and call GetProcAddress() on each
///
/// @param symbol_name Name of the symbol to find
/// @return Function pointer if found, nullptr otherwise
void*
find_symbol_in_any_module(const char* symbol_name)
{
    VERBOSE_LOG("Searching for symbol: %s", symbol_name);

    auto modules = enumerate_loaded_modules();
    for(size_t i = 0; i < modules.size(); ++i)
    {
        void* symbol = reinterpret_cast<void*>(GetProcAddress(modules[i], symbol_name));
        if(symbol)
        {
            // Found it! Get module name for logging
            char module_path[MAX_PATH] = {0};
            GetModuleFileNameA(modules[i], module_path, sizeof(module_path));
            const char* filename = strrchr(module_path, '\\');
            filename             = filename ? (filename + 1) : module_path;

            VERBOSE_LOG("  -> Found '%s' in %s (module index %zu)",
                        symbol_name,
                        filename,
                        i);
            return symbol;
        }
    }

    VERBOSE_LOG("  -> Symbol '%s' not found in any loaded module", symbol_name);
    return nullptr;
}

/// @brief Enhanced symbol search with PE validation and address checking
///
/// Uses PE parser to validate symbol addresses and provide detailed information
///
/// @param symbol_name Name of the symbol to find
/// @return Function pointer if found and validated, nullptr otherwise
void*
find_symbol_with_validation(const char* symbol_name)
{
    VERBOSE_LOG("Searching for symbol with validation: %s", symbol_name);

    auto modules = enumerate_loaded_modules();
    for(size_t i = 0; i < modules.size(); ++i)
    {
        void* symbol = reinterpret_cast<void*>(GetProcAddress(modules[i], symbol_name));
        if(symbol)
        {
            // Found symbol - validate it using PE parser
            char module_path[MAX_PATH] = {0};
            GetModuleFileNameA(modules[i], module_path, sizeof(module_path));
            const char* filename = strrchr(module_path, '\\');
            filename             = filename ? (filename + 1) : module_path;

            // Validate address is in executable memory
            if(!pe_parser::is_address_executable(symbol))
            {
                VERBOSE_LOG("  -> WARNING: Symbol '%s' in %s is not executable (address: %p)",
                            symbol_name,
                            filename,
                            symbol);
                continue;  // Skip invalid symbol
            }

            // Get memory protection info
            DWORD protection = pe_parser::get_memory_protection(symbol);
            VERBOSE_LOG("  -> Found '%s' in %s at %p (protection: 0x%lx)",
                        symbol_name,
                        filename,
                        symbol,
                        (unsigned long)protection);

            // Verify symbol is in the correct module
            void* symbol_module = pe_parser::find_module_for_address(symbol);
            if(symbol_module != modules[i])
            {
                VERBOSE_LOG("  -> WARNING: Symbol module mismatch (expected %p, got %p)",
                            modules[i],
                            symbol_module);
            }

            return symbol;
        }
    }

    VERBOSE_LOG("  -> Symbol '%s' not found or validated", symbol_name);
    return nullptr;
}

}  // namespace

//=============================================================================
// Windows-Specific: Dynamic Library Loading
//
// LINUX EQUIVALENT: dlopen() for loading libraries
// WINDOWS: LoadLibraryA() with multi-attempt strategy
//
// The Linux implementation tries multiple loading strategies with RTLD flags.
// On Windows, we use a simpler approach but still support multiple attempts.
//=============================================================================

namespace
{
/// @brief Load a library with multiple fallback strategies
///
/// LINUX EQUIVALENT: dlopen() with different RTLD flags (RTLD_NOW, RTLD_LAZY, etc.)
/// WINDOWS: LoadLibraryA() with different path variants
///
/// @param library_path Path to the library (may be relative or absolute)
/// @return Module handle if successful, nullptr otherwise
HMODULE
load_library_with_fallback(const char* library_path)
{
    VERBOSE_LOG("Attempting to load library: %s", library_path);

    // Attempt 1: Load as specified (searches current dir, system dirs, PATH)
    HMODULE handle = LoadLibraryA(library_path);
    if(handle)
    {
        VERBOSE_LOG("  -> Loaded successfully (attempt 1)");
        return handle;
    }

    VERBOSE_LOG("  -> Attempt 1 failed (error %lu)", GetLastError());

    // Attempt 2: Try with explicit .dll extension if not present
    std::string path_with_dll(library_path);
    if(path_with_dll.find(".dll") == std::string::npos)
    {
        path_with_dll += ".dll";
        handle = LoadLibraryA(path_with_dll.c_str());
        if(handle)
        {
            VERBOSE_LOG("  -> Loaded successfully (attempt 2 with .dll)");
            return handle;
        }
        VERBOSE_LOG("  -> Attempt 2 failed (error %lu)", GetLastError());
    }

    // Attempt 3: Try current directory + library name
    char current_dir[MAX_PATH] = {0};
    GetCurrentDirectoryA(sizeof(current_dir), current_dir);
    std::string full_path = std::string(current_dir) + "\\" + library_path;
    handle                = LoadLibraryA(full_path.c_str());
    if(handle)
    {
        VERBOSE_LOG("  -> Loaded successfully (attempt 3 with current dir)");
        return handle;
    }

    VERBOSE_LOG("  -> Attempt 3 failed (error %lu)", GetLastError());
    VERBOSE_LOG("  -> Failed to load library: %s", library_path);
    return nullptr;
}

}  // namespace

//=============================================================================
// Tool Discovery and Initialization
//
// This is the core logic that differs most from Linux:
// 1. Read ROCP_TOOL_LIBRARIES environment variable
// 2. Load each tool library using LoadLibraryA()
// 3. Search for rocprofiler_configure symbol using GetProcAddress()
// 4. Call rocprofiler_configure() for each tool
// 5. Search for rocprofiler_set_api_table symbol
// 6. Call rocprofiler_set_api_table() for each registered library
//=============================================================================

namespace
{
/// @brief Discover and initialize profiling tools
///
/// LINUX: Uses dlopen() + dlsym(RTLD_DEFAULT) for symbol search
/// WINDOWS: Uses LoadLibraryA() + EnumProcessModules + GetProcAddress()
///
/// Environment variables:
/// - ROCP_TOOL_LIBRARIES: Semicolon-separated list of tool DLL paths
///   (Linux uses colon separator, Windows uses semicolon)
void
discover_and_initialize_tools()
{
    VERBOSE_LOG("========================================");
    VERBOSE_LOG("Tool Discovery Phase");
    VERBOSE_LOG("========================================");

    // Optional: Print memory map for debugging (enabled via env var)
    char debug_memmap[32] = {0};
    GetEnvironmentVariableA("ROCPROFILER_DEBUG_MEMMAP", debug_memmap, sizeof(debug_memmap));
    if(debug_memmap[0] == '1')
    {
        VERBOSE_LOG("----------------------------------------");
        VERBOSE_LOG("Memory Map (ROCPROFILER_DEBUG_MEMMAP=1)");
        VERBOSE_LOG("----------------------------------------");
        pe_parser::print_memory_map();
        VERBOSE_LOG("----------------------------------------");
    }

    // STEP 1: Load the SDK library first
    // The SDK provides rocprofiler_configure and rocprofiler_set_api_table
    char sdk_library[4096] = {0};
    DWORD sdk_env_result = GetEnvironmentVariableA("ROCPROFILER_REGISTER_LIBRARY",
                                                     sdk_library,
                                                     sizeof(sdk_library));

    HMODULE sdk_handle = nullptr;
    if(sdk_env_result > 0 && sdk_env_result < sizeof(sdk_library))
    {
        VERBOSE_LOG("ROCPROFILER_REGISTER_LIBRARY: %s", sdk_library);
        VERBOSE_LOG("Loading SDK library...");
        sdk_handle = load_library_with_fallback(sdk_library);
        if(sdk_handle)
        {
            VERBOSE_LOG("  -> SDK loaded successfully: %s", sdk_library);
        }
        else
        {
            VERBOSE_LOG("  -> WARNING: Failed to load SDK library: %s", sdk_library);
        }
    }
    else
    {
        VERBOSE_LOG("ROCPROFILER_REGISTER_LIBRARY not set - SDK not explicitly loaded");
    }

    // STEP 2: Read ROCP_TOOL_LIBRARIES environment variable
    // WINDOWS NOTE: Use GetEnvironmentVariableA() instead of getenv()
    char tool_libraries[4096] = {0};
    DWORD env_result = GetEnvironmentVariableA("ROCP_TOOL_LIBRARIES",
                                                tool_libraries,
                                                sizeof(tool_libraries));

    if(env_result == 0 || env_result >= sizeof(tool_libraries))
    {
        VERBOSE_LOG("ROCP_TOOL_LIBRARIES not set or too long - no tools to load");
        return;
    }

    VERBOSE_LOG("ROCP_TOOL_LIBRARIES: %s", tool_libraries);

    // Parse semicolon-separated library paths
    // WINDOWS NOTE: Use ';' separator instead of ':' (Linux)
    std::vector<std::string> library_paths;
    char*                    token = strtok(tool_libraries, ";");
    while(token != nullptr)
    {
        // Trim whitespace
        while(*token == ' ' || *token == '\t') ++token;
        if(*token != '\0')
        {
            library_paths.push_back(token);
        }
        token = strtok(nullptr, ";");
    }

    if(library_paths.empty())
    {
        VERBOSE_LOG("No tool libraries specified");
        return;
    }

    // Load each tool library
    std::vector<HMODULE> loaded_tools;
    for(const auto& path : library_paths)
    {
        HMODULE handle = load_library_with_fallback(path.c_str());
        if(handle)
        {
            loaded_tools.push_back(handle);
            VERBOSE_LOG("Loaded tool library: %s", path.c_str());
        }
        else
        {
            VERBOSE_LOG("WARNING: Failed to load tool library: %s", path.c_str());
        }
    }

    if(loaded_tools.empty())
    {
        VERBOSE_LOG("No tool libraries loaded successfully");
        return;
    }

    // Initialize each tool by calling rocprofiler_configure on each one
    VERBOSE_LOG("----------------------------------------");
    VERBOSE_LOG("Initializing tools (calling rocprofiler_configure on each)");
    VERBOSE_LOG("----------------------------------------");

    int tools_initialized = 0;

    for(size_t i = 0; i < loaded_tools.size(); ++i)
    {
        HMODULE tool_handle = loaded_tools[i];

        // Get rocprofiler_configure from THIS specific tool
        void* configure_symbol = GetProcAddress(tool_handle, "rocprofiler_configure");

        if(!configure_symbol)
        {
            VERBOSE_LOG("Tool [%zu]: No rocprofiler_configure symbol found - skipping", i);
            continue;
        }

        VERBOSE_LOG("Tool [%zu]: Found rocprofiler_configure - calling...", i);

        // Call this tool's configure function
        auto configure_func = reinterpret_cast<rocprofiler_configure_func_t>(configure_symbol);

        uint64_t client_id = 0;
        int      result    = configure_func(
            1,                // version
            "6.4.0",          // runtime version (mock)
            0,                // priority
            &client_id);      // output: client ID

        VERBOSE_LOG("Tool [%zu]: rocprofiler_configure returned: %d (client_id=%llu)",
                    i,
                    result,
                    (unsigned long long) client_id);

        tools_initialized++;
    }

    VERBOSE_LOG("Initialized %d tool(s)", tools_initialized);

    if(tools_initialized == 0)
    {
        VERBOSE_LOG("WARNING: No tools were successfully initialized");
        return;
    }

    // Now search for rocprofiler_set_api_table
    // NOTE: This symbol is provided by the SDK (not individual tools)
    // so we search globally across all loaded modules
    VERBOSE_LOG("----------------------------------------");
    VERBOSE_LOG("Searching for rocprofiler_set_api_table symbol");
    VERBOSE_LOG("----------------------------------------");

    void* set_table_symbol = find_symbol_in_any_module("rocprofiler_set_api_table");

    if(!set_table_symbol)
    {
        VERBOSE_LOG("WARNING: rocprofiler_set_api_table symbol not found");
        VERBOSE_LOG("Tool initialized but cannot modify API tables");
        return;
    }

    auto set_table_func =
        reinterpret_cast<rocprofiler_set_api_table_func_t>(set_table_symbol);

    // Call rocprofiler_set_api_table for each registered library
    VERBOSE_LOG("----------------------------------------");
    VERBOSE_LOG("Calling rocprofiler_set_api_table for each registered library");
    VERBOSE_LOG("----------------------------------------");

    std::lock_guard<std::mutex> lock(g_registration_mutex);
    for(const auto& reg : g_registrations)
    {
        VERBOSE_LOG("  Library: %s (version %u, %zu tables)",
                    reg.name.c_str(),
                    reg.version,
                    reg.api_tables.size());

        int result = set_table_func(reg.name.c_str(),
                                     reg.version,
                                     reg.id.handle,
                                     const_cast<void**>(reg.api_tables.data()),
                                     reg.api_tables.size());

        VERBOSE_LOG("  -> rocprofiler_set_api_table returned: %d", result);
    }

    VERBOSE_LOG("========================================");
    VERBOSE_LOG("Tool Discovery Phase Complete");
    VERBOSE_LOG("========================================");
}

}  // namespace

//=============================================================================
// Public API Implementation
//=============================================================================

extern "C" {

ROCPROFILER_REGISTER_PUBLIC_API rocprofiler_register_error_code_t
rocprofiler_register_library_api_table(
    const char*                                 lib_name,
    rocprofiler_register_import_func_t          import_func,
    uint32_t                                    lib_version,
    void**                                      api_tables,
    uint64_t                                    api_table_length,
    rocprofiler_register_library_indentifier_t* register_id)
{
    init_verbose_mode();

    VERBOSE_LOG("========================================");
    VERBOSE_LOG("rocprofiler_register_library_api_table called");
    VERBOSE_LOG("  Library: %s", lib_name ? lib_name : "(null)");
    VERBOSE_LOG("  Version: %u", lib_version);
    VERBOSE_LOG("  Tables: %llu", (unsigned long long) api_table_length);
    VERBOSE_LOG("========================================");

    // Validate arguments
    if(!lib_name || !api_tables || api_table_length == 0 || !register_id)
    {
        VERBOSE_LOG("ERROR: Invalid arguments");
        return ROCP_REG_INVALID_ARGUMENT;
    }

    if(!*api_tables)
    {
        VERBOSE_LOG("ERROR: Invalid API table pointer");
        return ROCP_REG_INVALID_API_ADDRESS;
    }

    // Check for re-entrance (deadlock detection)
    if(g_in_registration)
    {
        VERBOSE_LOG("ERROR: Re-entrant call detected (deadlock)");
        return ROCP_REG_DEADLOCK;
    }

    g_in_registration = true;

    // Create registration record
    registration_record_t record;
    record.name        = lib_name;
    record.import_func = import_func;
    record.version     = lib_version;
    record.id.handle   = g_next_handle++;

    // Store API table pointers
    for(uint64_t i = 0; i < api_table_length; ++i)
    {
        record.api_tables.push_back(api_tables[i]);
    }

    // Store registration
    {
        std::lock_guard<std::mutex> lock(g_registration_mutex);
        g_registrations.push_back(record);
    }

    // Return ID
    *register_id = record.id;

    VERBOSE_LOG("Registration successful: ID=%llu", (unsigned long long) record.id.handle);

    // Check if this is the first registration
    // If so, trigger tool discovery
    static std::once_flag init_flag;
    std::call_once(init_flag, discover_and_initialize_tools);

    g_in_registration = false;

    return ROCP_REG_SUCCESS;
}

ROCPROFILER_REGISTER_PUBLIC_API const char*
rocprofiler_register_error_string(rocprofiler_register_error_code_t error_code)
{
    switch(error_code)
    {
        case ROCP_REG_SUCCESS: return "Success";
        case ROCP_REG_NO_TOOLS: return "No tools found";
        case ROCP_REG_DEADLOCK: return "Deadlock detected (re-entrant call)";
        case ROCP_REG_BAD_API_TABLE_LENGTH: return "Invalid API table length";
        case ROCP_REG_UNSUPPORTED_API: return "Unsupported API";
        case ROCP_REG_INVALID_API_ADDRESS: return "Invalid API table address";
        case ROCP_REG_ROCPROFILER_ERROR: return "ROCProfiler error";
        case ROCP_REG_EXCESS_API_INSTANCES: return "Too many API instances";
        case ROCP_REG_INVALID_ARGUMENT: return "Invalid argument";
        case ROCP_REG_ATTACHMENT_NOT_AVAILABLE: return "Attachment not available";
        default: return "Unknown error";
    }
}

ROCPROFILER_REGISTER_PUBLIC_API rocprofiler_register_error_code_t
rocprofiler_register_iterate_registration_info(
    rocprofiler_register_registration_info_cb_t callback,
    void*                                       data)
{
    if(!callback)
    {
        return ROCP_REG_INVALID_ARGUMENT;
    }

    std::lock_guard<std::mutex> lock(g_registration_mutex);

    for(auto& reg : g_registrations)
    {
        rocprofiler_register_registration_info_t info = {};
        info.size              = sizeof(info);
        info.common_name       = reg.name.c_str();
        info.lib_version       = reg.version;
        info.api_table_length  = reg.api_tables.size();

        int result = callback(&info, data);
        if(result != 0)
        {
            break;  // Callback requested early termination
        }
    }

    return ROCP_REG_SUCCESS;
}

}  // extern "C"
