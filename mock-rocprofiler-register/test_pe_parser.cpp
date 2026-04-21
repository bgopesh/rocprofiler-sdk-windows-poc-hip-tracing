// Test program for PE parser functionality
//
// This standalone program tests the PE parsing and memory mapping features
// to ensure they work correctly before integration.

#include "pe_parser.h"

#include <cstdio>
#include <windows.h>

int
main()
{
    printf("=== PE Parser Test Program ===\n\n");

    // Test 1: Get all loaded modules
    printf("Test 1: Get All Modules\n");
    printf("----------------------------------------\n");
    auto modules = pe_parser::get_all_modules();
    printf("Found %zu loaded modules:\n", modules.size());
    for(size_t i = 0; i < modules.size() && i < 10; ++i)
    {
        const auto& mod = modules[i];
        printf("  [%2zu] %s\n", i, mod.name.c_str());
        printf("       Base: %p, Size: %lu bytes\n",
               mod.base_address,
               (unsigned long)mod.size_of_image);
        printf("       Exports: %u, Imports: %u\n",
               (unsigned)mod.number_of_exports,
               (unsigned)mod.number_of_imports);
    }
    if(modules.size() > 10)
    {
        printf("  ... and %zu more\n", modules.size() - 10);
    }
    printf("\n");

    // Test 2: Memory map
    printf("Test 2: Memory Map (first 20 regions)\n");
    printf("----------------------------------------\n");
    auto regions = pe_parser::get_memory_map();
    printf("Found %zu memory regions:\n", regions.size());
    for(size_t i = 0; i < regions.size() && i < 20; ++i)
    {
        const auto& region = regions[i];
        printf("  [%2zu] %016llx-%016llx (%8zu bytes) %c%c%c ",
               i,
               (unsigned long long)region.base_address,
               (unsigned long long)((char*)region.base_address + region.size),
               region.size,
               region.is_readable ? 'r' : '-',
               region.is_writable ? 'w' : '-',
               region.is_executable ? 'x' : '-');

        if(!region.module_name.empty())
        {
            printf("%s\n", region.module_name.c_str());
        }
        else
        {
            printf("(private)\n");
        }
    }
    if(regions.size() > 20)
    {
        printf("  ... and %zu more\n", regions.size() - 20);
    }
    printf("\n");

    // Test 3: Address validation
    printf("Test 3: Address Validation\n");
    printf("----------------------------------------\n");

    // Test with main function address (should be executable)
    void* main_addr = reinterpret_cast<void*>(&main);
    printf("Testing address of main(): %p\n", main_addr);
    printf("  is_readable: %s\n", pe_parser::is_address_readable(main_addr) ? "YES" : "NO");
    printf("  is_executable: %s\n",
           pe_parser::is_address_executable(main_addr) ? "YES" : "NO");
    printf("  module: %s\n", pe_parser::get_module_path_for_address(main_addr).c_str());

    // Test with NULL (should fail)
    printf("\nTesting NULL address:\n");
    printf("  is_readable: %s\n", pe_parser::is_address_readable(nullptr) ? "YES" : "NO");
    printf("  is_executable: %s\n", pe_parser::is_address_executable(nullptr) ? "YES" : "NO");

    // Test with kernel32.dll GetProcAddress function
    HMODULE kernel32 = GetModuleHandleA("kernel32.dll");
    if(kernel32)
    {
        void* getproc_addr = reinterpret_cast<void*>(GetProcAddress(kernel32, "GetProcAddress"));
        printf("\nTesting GetProcAddress in kernel32.dll: %p\n", getproc_addr);
        printf("  is_readable: %s\n",
               pe_parser::is_address_readable(getproc_addr) ? "YES" : "NO");
        printf("  is_executable: %s\n",
               pe_parser::is_address_executable(getproc_addr) ? "YES" : "NO");
        printf("  module: %s\n",
               pe_parser::get_module_path_for_address(getproc_addr).c_str());
    }
    printf("\n");

    // Test 4: PE header parsing
    printf("Test 4: PE Header Parsing\n");
    printf("----------------------------------------\n");
    if(kernel32)
    {
        auto pe_info = pe_parser::parse_pe_headers(kernel32);
        printf("kernel32.dll PE Info:\n");
        printf("  Name: %s\n", pe_info.name.c_str());
        printf("  Path: %s\n", pe_info.path.c_str());
        printf("  Base: %p\n", pe_info.base_address);
        printf("  Machine: 0x%04x\n", (unsigned)pe_info.machine);
        printf("  Sections: %u\n", (unsigned)pe_info.number_of_sections);
        printf("  Size: %lu bytes\n", (unsigned long)pe_info.size_of_image);
        printf("  Entry Point: 0x%08lx\n", (unsigned long)pe_info.entry_point);
        printf("  Exports: %u\n", (unsigned)pe_info.number_of_exports);
        printf("  Imports: %u\n", (unsigned)pe_info.number_of_imports);
    }
    printf("\n");

    // Test 5: Export directory
    printf("Test 5: Export Directory\n");
    printf("----------------------------------------\n");
    if(kernel32)
    {
        auto export_dir = pe_parser::get_export_directory(kernel32);
        if(export_dir)
        {
            printf("kernel32.dll Export Directory:\n");
            printf("  Number of functions: %lu\n", (unsigned long)export_dir->NumberOfFunctions);
            printf("  Number of names: %lu\n", (unsigned long)export_dir->NumberOfNames);
            printf("  Name: %s\n",
                   (char*)kernel32 + export_dir->Name);

            // List first 10 exports
            DWORD* name_rvas = (DWORD*)((char*)kernel32 + export_dir->AddressOfNames);
            printf("\n  First 10 exported functions:\n");
            for(DWORD i = 0; i < export_dir->NumberOfNames && i < 10; ++i)
            {
                const char* name = (const char*)((char*)kernel32 + name_rvas[i]);
                printf("    [%2lu] %s\n", (unsigned long)i, name);
            }
            if(export_dir->NumberOfNames > 10)
            {
                printf("    ... and %lu more\n",
                       (unsigned long)(export_dir->NumberOfNames - 10));
            }
        }
        else
        {
            printf("No export directory found\n");
        }
    }
    printf("\n");

    printf("=== All Tests Complete ===\n");
    return 0;
}
