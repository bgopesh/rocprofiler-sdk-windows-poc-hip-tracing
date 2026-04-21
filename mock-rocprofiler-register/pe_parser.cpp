#include "pe_parser.h"

#include <cstdio>
#include <cstring>

namespace pe_parser
{

namespace
{

// Helper: Convert protection flags to readable string
const char*
protection_to_string(DWORD protection)
{
    switch(protection)
    {
        case PAGE_NOACCESS: return "---";
        case PAGE_READONLY: return "r--";
        case PAGE_READWRITE: return "rw-";
        case PAGE_WRITECOPY: return "rwc";
        case PAGE_EXECUTE: return "--x";
        case PAGE_EXECUTE_READ: return "r-x";
        case PAGE_EXECUTE_READWRITE: return "rwx";
        case PAGE_EXECUTE_WRITECOPY: return "rwc";
        default: return "???";
    }
}

// Helper: Get DOS header from module base
IMAGE_DOS_HEADER*
get_dos_header(void* module_base)
{
    if(!module_base) return nullptr;
    return reinterpret_cast<IMAGE_DOS_HEADER*>(module_base);
}

// Helper: Get NT headers from module base
IMAGE_NT_HEADERS*
get_nt_headers(void* module_base)
{
    auto dos_header = get_dos_header(module_base);
    if(!dos_header || dos_header->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return nullptr;
    }

    auto nt_headers = reinterpret_cast<IMAGE_NT_HEADERS*>(
        reinterpret_cast<uint8_t*>(module_base) + dos_header->e_lfanew);

    if(nt_headers->Signature != IMAGE_NT_SIGNATURE)
    {
        return nullptr;
    }

    return nt_headers;
}

}  // anonymous namespace

//=============================================================================
// Memory Map (similar to /proc/self/maps)
//=============================================================================

std::vector<MemoryRegion>
get_memory_map()
{
    std::vector<MemoryRegion> regions;

    void*                     address = nullptr;
    MEMORY_BASIC_INFORMATION mbi;

    while(VirtualQuery(address, &mbi, sizeof(mbi)) == sizeof(mbi))
    {
        MemoryRegion region;
        region.base_address = mbi.BaseAddress;
        region.size         = mbi.RegionSize;
        region.protection   = mbi.Protect;
        region.state        = mbi.State;
        region.type         = mbi.Type;

        // Check protection flags
        region.is_readable   = (mbi.Protect & PAGE_READONLY) ||
                               (mbi.Protect & PAGE_READWRITE) ||
                               (mbi.Protect & PAGE_EXECUTE_READ) ||
                               (mbi.Protect & PAGE_EXECUTE_READWRITE);

        region.is_writable   = (mbi.Protect & PAGE_READWRITE) ||
                               (mbi.Protect & PAGE_EXECUTE_READWRITE) ||
                               (mbi.Protect & PAGE_WRITECOPY) ||
                               (mbi.Protect & PAGE_EXECUTE_WRITECOPY);

        region.is_executable = (mbi.Protect & PAGE_EXECUTE) ||
                               (mbi.Protect & PAGE_EXECUTE_READ) ||
                               (mbi.Protect & PAGE_EXECUTE_READWRITE);

        // Get module path for MEM_IMAGE regions
        if(mbi.Type == MEM_IMAGE && mbi.State == MEM_COMMIT)
        {
            char module_path[MAX_PATH] = {0};
            if(GetModuleFileNameExA(GetCurrentProcess(),
                                     reinterpret_cast<HMODULE>(mbi.AllocationBase),
                                     module_path,
                                     sizeof(module_path)))
            {
                region.module_path = module_path;

                // Extract just the filename
                const char* filename = strrchr(module_path, '\\');
                region.module_name   = filename ? (filename + 1) : module_path;
            }
        }

        // Only add committed or reserved regions (skip free)
        if(mbi.State != MEM_FREE)
        {
            regions.push_back(region);
        }

        // Move to next region
        address = reinterpret_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
    }

    return regions;
}

//=============================================================================
// PE Header Parsing
//=============================================================================

PEInfo
parse_pe_headers(void* module_base)
{
    PEInfo info{};
    info.base_address = module_base;

    // Get DOS header
    auto dos_header = get_dos_header(module_base);
    if(!dos_header || dos_header->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return info;  // Invalid PE
    }

    // Get NT headers
    auto nt_headers = get_nt_headers(module_base);
    if(!nt_headers)
    {
        return info;  // Invalid PE
    }

    // Fill in basic PE info
    info.signature           = nt_headers->Signature;
    info.machine             = nt_headers->FileHeader.Machine;
    info.number_of_sections  = nt_headers->FileHeader.NumberOfSections;
    info.time_date_stamp     = nt_headers->FileHeader.TimeDateStamp;
    info.size_of_image       = nt_headers->OptionalHeader.SizeOfImage;
    info.entry_point         = nt_headers->OptionalHeader.AddressOfEntryPoint;

    // Get module path and name
    char module_path[MAX_PATH] = {0};
    if(GetModuleFileNameExA(GetCurrentProcess(),
                             reinterpret_cast<HMODULE>(module_base),
                             module_path,
                             sizeof(module_path)))
    {
        info.path = module_path;

        const char* filename = strrchr(module_path, '\\');
        info.name            = filename ? (filename + 1) : module_path;
    }

    // Check for export directory
    DWORD export_rva = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]
                           .VirtualAddress;
    if(export_rva != 0)
    {
        info.has_exports = true;

        auto export_dir = reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(
            reinterpret_cast<uint8_t*>(module_base) + export_rva);

        info.number_of_exports = export_dir->NumberOfNames;
    }

    // Check for import directory
    DWORD import_rva = nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
                           .VirtualAddress;
    if(import_rva != 0)
    {
        info.has_imports = true;

        auto import_desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
            reinterpret_cast<uint8_t*>(module_base) + import_rva);

        // Count imports
        DWORD count = 0;
        while(import_desc[count].Name != 0)
        {
            count++;
        }
        info.number_of_imports = count;
    }

    return info;
}

PEInfo
parse_pe_file(const std::string& file_path)
{
    PEInfo info{};
    info.path = file_path;

    // Extract filename
    const char* filename = strrchr(file_path.c_str(), '\\');
    info.name            = filename ? (filename + 1) : file_path;

    // Open file
    HANDLE file = CreateFileA(file_path.c_str(),
                              GENERIC_READ,
                              FILE_SHARE_READ,
                              nullptr,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              nullptr);

    if(file == INVALID_HANDLE_VALUE)
    {
        return info;
    }

    // Map file into memory
    HANDLE mapping = CreateFileMappingA(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if(!mapping)
    {
        CloseHandle(file);
        return info;
    }

    void* view = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if(!view)
    {
        CloseHandle(mapping);
        CloseHandle(file);
        return info;
    }

    // Parse PE headers from mapped file
    info = parse_pe_headers(view);
    info.path = file_path;  // Restore path
    info.name = filename ? (filename + 1) : file_path;

    // Cleanup
    UnmapViewOfFile(view);
    CloseHandle(mapping);
    CloseHandle(file);

    return info;
}

//=============================================================================
// Address Validation
//=============================================================================

void*
find_module_for_address(void* address)
{
    MEMORY_BASIC_INFORMATION mbi;
    if(VirtualQuery(address, &mbi, sizeof(mbi)) != sizeof(mbi))
    {
        return nullptr;
    }

    if(mbi.Type == MEM_IMAGE)
    {
        return mbi.AllocationBase;
    }

    return nullptr;
}

std::string
get_module_path_for_address(void* address)
{
    void* module_base = find_module_for_address(address);
    if(!module_base)
    {
        return "";
    }

    char module_path[MAX_PATH] = {0};
    if(GetModuleFileNameExA(GetCurrentProcess(),
                             reinterpret_cast<HMODULE>(module_base),
                             module_path,
                             sizeof(module_path)))
    {
        return module_path;
    }

    return "";
}

bool
is_address_readable(void* address)
{
    MEMORY_BASIC_INFORMATION mbi;
    if(VirtualQuery(address, &mbi, sizeof(mbi)) != sizeof(mbi))
    {
        return false;
    }

    if(mbi.State != MEM_COMMIT)
    {
        return false;
    }

    return (mbi.Protect & PAGE_READONLY) || (mbi.Protect & PAGE_READWRITE) ||
           (mbi.Protect & PAGE_EXECUTE_READ) || (mbi.Protect & PAGE_EXECUTE_READWRITE);
}

bool
is_address_executable(void* address)
{
    MEMORY_BASIC_INFORMATION mbi;
    if(VirtualQuery(address, &mbi, sizeof(mbi)) != sizeof(mbi))
    {
        return false;
    }

    if(mbi.State != MEM_COMMIT)
    {
        return false;
    }

    return (mbi.Protect & PAGE_EXECUTE) || (mbi.Protect & PAGE_EXECUTE_READ) ||
           (mbi.Protect & PAGE_EXECUTE_READWRITE);
}

DWORD
get_memory_protection(void* address)
{
    MEMORY_BASIC_INFORMATION mbi;
    if(VirtualQuery(address, &mbi, sizeof(mbi)) != sizeof(mbi))
    {
        return 0;
    }

    return mbi.Protect;
}

//=============================================================================
// Module Enumeration
//=============================================================================

std::vector<PEInfo>
get_all_modules()
{
    std::vector<PEInfo> modules;

    HMODULE module_handles[1024];
    DWORD   needed = 0;

    if(!EnumProcessModules(
           GetCurrentProcess(), module_handles, sizeof(module_handles), &needed))
    {
        return modules;
    }

    DWORD module_count = needed / sizeof(HMODULE);

    for(DWORD i = 0; i < module_count; i++)
    {
        PEInfo info = parse_pe_headers(module_handles[i]);
        modules.push_back(info);
    }

    return modules;
}

//=============================================================================
// Export/Import Directories
//=============================================================================

IMAGE_EXPORT_DIRECTORY*
get_export_directory(void* module_base)
{
    auto nt_headers = get_nt_headers(module_base);
    if(!nt_headers)
    {
        return nullptr;
    }

    DWORD export_rva =
        nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;

    if(export_rva == 0)
    {
        return nullptr;
    }

    return reinterpret_cast<IMAGE_EXPORT_DIRECTORY*>(
        reinterpret_cast<uint8_t*>(module_base) + export_rva);
}

IMAGE_IMPORT_DESCRIPTOR*
get_import_directory(void* module_base)
{
    auto nt_headers = get_nt_headers(module_base);
    if(!nt_headers)
    {
        return nullptr;
    }

    DWORD import_rva =
        nt_headers->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

    if(import_rva == 0)
    {
        return nullptr;
    }

    return reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        reinterpret_cast<uint8_t*>(module_base) + import_rva);
}

//=============================================================================
// Debug Printing
//=============================================================================

void
print_memory_map()
{
    auto regions = get_memory_map();

    printf("Memory Map (%zu regions):\n", regions.size());
    printf("%-18s %-18s %-10s %-6s %-10s %s\n",
           "Start",
           "End",
           "Size",
           "Prot",
           "Type",
           "Module");
    printf("%s\n", std::string(100, '-').c_str());

    for(const auto& region : regions)
    {
        void*  start = region.base_address;
        void*  end   = reinterpret_cast<uint8_t*>(start) + region.size;
        size_t size  = region.size;

        const char* type_str = "UNKNOWN";
        if(region.type == MEM_IMAGE)
            type_str = "IMAGE";
        else if(region.type == MEM_MAPPED)
            type_str = "MAPPED";
        else if(region.type == MEM_PRIVATE)
            type_str = "PRIVATE";

        printf("%016llx-%016llx %10zu %s %-10s %s\n",
               (unsigned long long)start,
               (unsigned long long)end,
               size,
               protection_to_string(region.protection),
               type_str,
               region.module_name.c_str());
    }
}

}  // namespace pe_parser
