#pragma once

#include <windows.h>
#include <psapi.h>

#include <cstdint>
#include <string>
#include <vector>

namespace pe_parser
{

//=============================================================================
// Memory Region Information (similar to /proc/self/maps on Linux)
//=============================================================================

struct MemoryRegion
{
    void*       base_address;      // Start address of region
    size_t      size;              // Size in bytes
    DWORD       protection;        // Memory protection flags
    DWORD       state;             // MEM_COMMIT, MEM_RESERVE, MEM_FREE
    DWORD       type;              // MEM_IMAGE, MEM_MAPPED, MEM_PRIVATE
    std::string module_path;       // Full path to module (if MEM_IMAGE)
    std::string module_name;       // Just filename (e.g., "kernel32.dll")
    bool        is_executable;     // True if contains executable code
    bool        is_readable;       // True if readable
    bool        is_writable;       // True if writable
};

//=============================================================================
// PE Header Information
//=============================================================================

struct PEInfo
{
    void*       base_address;           // Module base address
    std::string path;                   // Full path to PE file
    std::string name;                   // Module name

    // PE Header fields
    DWORD       signature;              // PE signature
    WORD        machine;                // Machine type (x64, x86, etc.)
    WORD        number_of_sections;     // Number of sections
    DWORD       time_date_stamp;        // Time stamp
    DWORD       size_of_image;          // Size of loaded image
    DWORD       entry_point;            // Entry point RVA

    // Export information
    bool        has_exports;            // True if export table exists
    DWORD       number_of_exports;      // Number of exported symbols

    // Import information
    bool        has_imports;            // True if import table exists
    DWORD       number_of_imports;      // Number of imported DLLs
};

//=============================================================================
// Functions
//=============================================================================

/**
 * @brief Get all memory regions in the current process
 *
 * Similar to reading /proc/self/maps on Linux. Uses VirtualQuery to
 * enumerate all memory regions.
 *
 * @return Vector of memory regions
 */
std::vector<MemoryRegion> get_memory_map();

/**
 * @brief Parse PE headers from a loaded module
 *
 * @param module_base Base address of loaded module
 * @return PE information structure
 */
PEInfo parse_pe_headers(void* module_base);

/**
 * @brief Parse PE headers from a file on disk
 *
 * @param file_path Path to PE file
 * @return PE information structure
 */
PEInfo parse_pe_file(const std::string& file_path);

/**
 * @brief Find which module contains a given address
 *
 * @param address Address to look up
 * @return Module base address, or nullptr if not in any module
 */
void* find_module_for_address(void* address);

/**
 * @brief Get module path for a given address
 *
 * @param address Address to look up
 * @return Full path to module, or empty string if not found
 */
std::string get_module_path_for_address(void* address);

/**
 * @brief Validate that an address is readable
 *
 * @param address Address to validate
 * @return True if address is in readable memory
 */
bool is_address_readable(void* address);

/**
 * @brief Validate that an address is executable
 *
 * @param address Address to validate
 * @return True if address is in executable memory
 */
bool is_address_executable(void* address);

/**
 * @brief Get memory protection flags for an address
 *
 * @param address Address to query
 * @return Protection flags (PAGE_EXECUTE_READ, etc.), or 0 if invalid
 */
DWORD get_memory_protection(void* address);

/**
 * @brief Get all loaded modules with their PE information
 *
 * Combines EnumProcessModules with PE parsing to get detailed info
 * about all loaded DLLs/EXEs.
 *
 * @return Vector of PE information for all modules
 */
std::vector<PEInfo> get_all_modules();

/**
 * @brief Print memory map to stdout (for debugging)
 *
 * Prints a formatted table similar to /proc/self/maps output
 */
void print_memory_map();

/**
 * @brief Get export directory from a module
 *
 * @param module_base Base address of module
 * @return Pointer to export directory, or nullptr if not found
 */
IMAGE_EXPORT_DIRECTORY* get_export_directory(void* module_base);

/**
 * @brief Get import directory from a module
 *
 * @param module_base Base address of module
 * @return Pointer to first import descriptor, or nullptr if not found
 */
IMAGE_IMPORT_DESCRIPTOR* get_import_directory(void* module_base);

}  // namespace pe_parser
