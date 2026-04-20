#pragma once

//=============================================================================
// Mock rocprofiler-register.h - Windows Implementation
//
// This header provides the Windows-compatible mock implementation of the
// rocprofiler-register API. The real implementation would be in the ROCm
// installation (C:/opt/rocm/include/rocprofiler-register/).
//
// Key Windows-specific considerations:
// - Uses __declspec(dllexport) instead of GCC visibility attributes
// - Compatible with MSVC compiler
// - Follows the same API contract as the Linux version
//=============================================================================

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#    define ROCPROFILER_REGISTER_PUBLIC_API __declspec(dllexport)
#    define ROCPROFILER_REGISTER_ATTRIBUTE(...)
#else
#    define ROCPROFILER_REGISTER_PUBLIC_API __attribute__((visibility("default")))
#    define ROCPROFILER_REGISTER_ATTRIBUTE(...) __attribute__((__VA_ARGS__))
#endif

// Helper macros for preprocessor token manipulation
#define ROCPROFILER_REGISTER_PP_COMBINE_IMPL(X, Y) X##Y
#define ROCPROFILER_REGISTER_PP_COMBINE(X, Y) ROCPROFILER_REGISTER_PP_COMBINE_IMPL(X, Y)

#ifdef __cplusplus
extern "C" {
#endif

//=============================================================================
// Type Definitions
//=============================================================================

/// @brief Function pointer type for import functions
/// These functions return the version number of the registering library
typedef uint32_t (*rocprofiler_register_import_func_t)(void);

/// @brief Opaque library identifier returned from registration
typedef struct
{
    uint64_t handle;
} rocprofiler_register_library_indentifier_t;

/// @brief Error codes returned by rocprofiler-register API functions
typedef enum rocprofiler_register_error_code_t
{
    ROCP_REG_SUCCESS = 0,                   ///< Operation succeeded
    ROCP_REG_NO_TOOLS,                      ///< No tools found to configure
    ROCP_REG_DEADLOCK,                      ///< Re-entrant call detected
    ROCP_REG_BAD_API_TABLE_LENGTH,          ///< Invalid table length (zero)
    ROCP_REG_UNSUPPORTED_API,               ///< Unknown API name
    ROCP_REG_INVALID_API_ADDRESS,           ///< Invalid table pointer
    ROCP_REG_ROCPROFILER_ERROR,             ///< Error in tool configuration
    ROCP_REG_EXCESS_API_INSTANCES,          ///< Too many registrations
    ROCP_REG_INVALID_ARGUMENT,              ///< Invalid function argument
    ROCP_REG_ATTACHMENT_NOT_AVAILABLE,      ///< Attachment library not loaded
    ROCP_REG_ERROR_CODE_END,
} rocprofiler_register_error_code_t;

/// @brief Information about a registered library
typedef struct rocprofiler_register_registration_info_t
{
    size_t      size;              ///< Structure size for versioning
    const char* common_name;       ///< Library name (e.g., "hip")
    uint32_t    lib_version;       ///< Version in encoded format
    uint64_t    api_table_length;  ///< Number of API tables
} rocprofiler_register_registration_info_t;

/// @brief Callback type for iterating over registered libraries
typedef int (*rocprofiler_register_registration_info_cb_t)(
    rocprofiler_register_registration_info_t* info,
    void*                                     data);

//=============================================================================
// API Functions
//=============================================================================

/// @brief Main registration function called by runtime libraries (HIP, HSA, etc.)
///
/// This is the entry point for runtime libraries to register their API dispatch
/// tables with the profiling framework. On Windows, this function will:
///
/// 1. Store the registration info and API table pointers
/// 2. Search for rocprofiler tools using EnumProcessModules + GetProcAddress
/// 3. Load tool libraries via LoadLibraryA from ROCP_TOOL_LIBRARIES env var
/// 4. Call rocprofiler_configure() if found in any loaded module
/// 5. Call rocprofiler_set_api_table() to allow tools to wrap API functions
///
/// @param lib_name Library identifier (e.g., "hip", "hsa")
/// @param import_func Function returning library version
/// @param lib_version Encoded version: 10000*major + 100*minor + patch
/// @param api_tables Array of pointers to API dispatch tables (modifiable)
/// @param api_table_length Number of tables in the array
/// @param register_id Output: unique identifier for this registration
/// @return Error code (ROCP_REG_SUCCESS on success)
ROCPROFILER_REGISTER_PUBLIC_API rocprofiler_register_error_code_t
rocprofiler_register_library_api_table(
    const char*                                 lib_name,
    rocprofiler_register_import_func_t          import_func,
    uint32_t                                    lib_version,
    void**                                      api_tables,
    uint64_t                                    api_table_length,
    rocprofiler_register_library_indentifier_t* register_id)
    ROCPROFILER_REGISTER_ATTRIBUTE(nonnull(4, 6));

/// @brief Get error string for an error code
ROCPROFILER_REGISTER_PUBLIC_API const char*
rocprofiler_register_error_string(rocprofiler_register_error_code_t error_code);

/// @brief Iterate over all registered libraries
ROCPROFILER_REGISTER_PUBLIC_API rocprofiler_register_error_code_t
rocprofiler_register_iterate_registration_info(
    rocprofiler_register_registration_info_cb_t callback,
    void*                                       data)
    ROCPROFILER_REGISTER_ATTRIBUTE(nonnull(1));

#ifdef __cplusplus
}
#endif

//=============================================================================
// Helper Macros
//=============================================================================

/// @brief Compute version number from major version only
#define ROCPROFILER_REGISTER_COMPUTE_VERSION_1(MAJOR_VERSION) (10000 * MAJOR_VERSION)

/// @brief Compute version number from major and minor versions
#define ROCPROFILER_REGISTER_COMPUTE_VERSION_2(MAJOR_VERSION, MINOR_VERSION)             \
    (ROCPROFILER_REGISTER_COMPUTE_VERSION_1(MAJOR_VERSION) + (100 * MINOR_VERSION))

/// @brief Compute version number from major, minor, and patch versions
#define ROCPROFILER_REGISTER_COMPUTE_VERSION_3(                                          \
    MAJOR_VERSION, MINOR_VERSION, PATCH_VERSION)                                         \
    (ROCPROFILER_REGISTER_COMPUTE_VERSION_2(MAJOR_VERSION, MINOR_VERSION) +              \
     (PATCH_VERSION))

/// @brief Get the import function name for a library
#define ROCPROFILER_REGISTER_IMPORT_FUNC(NAME)                                           \
    ROCPROFILER_REGISTER_PP_COMBINE(rocprofiler_register_import_, NAME)

/// @brief Define an import function for a library
#ifdef __cplusplus
#    define ROCPROFILER_REGISTER_DEFINE_IMPORT(NAME, VERSION)                            \
        extern "C" {                                                                     \
        ROCPROFILER_REGISTER_PUBLIC_API uint32_t ROCPROFILER_REGISTER_IMPORT_FUNC(NAME)(); \
        uint32_t ROCPROFILER_REGISTER_IMPORT_FUNC(NAME)() { return VERSION; }            \
        }
#else
#    define ROCPROFILER_REGISTER_DEFINE_IMPORT(NAME, VERSION)                            \
        ROCPROFILER_REGISTER_PUBLIC_API uint32_t ROCPROFILER_REGISTER_IMPORT_FUNC(NAME)(void); \
        uint32_t ROCPROFILER_REGISTER_IMPORT_FUNC(NAME)(void) { return VERSION; }
#endif
