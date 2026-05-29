/**
 * @file        ModuleLoader.cpp
 * @brief       Module Loader for Kernel Drivers
 *
 * @date        01/02/2026
 * @version     1.0.0
 */

#define KDBG_COMPONENT "K.MODULELDR"
#include <core/drivers/ModuleLoader.h>

// ELF Macros for x86 Relocation
#define ELF32_R_SYM(i) ((i) >> 8)
#define ELF32_R_TYPE(i) ((unsigned char)(i))
#define R_386_32 1    // Absolute 32-bit
#define R_386_PC32 2  // PC-Relative 32-bit

void* ModuleLoader::LoadMatchingDriver(File* file, uint16_t target_vid, uint16_t target_did) {
    if (!file) return 0;

    DriverManifest manifest;

    // Read the header
    if (!ModuleLoader::Probe(file, &manifest)) {
        KDBG1("Error: File is not a valid driver (Missing .driver_info)");
        return 0;
    }

    // CHECK: Loop through the supported devices array
    bool match = false;
    for (int i = 0; i < 4; i++) {
        // Break if hit an empty slot
        if (manifest.devices[i].vendor_id == 0) break;

        if (manifest.devices[i].vendor_id == target_vid &&
            manifest.devices[i].device_id == target_did) {
            match = true;
            break;
        }
    }

    if (match) {
        KDBG1("Match: %s v%s supports hardware %x:%x. Loading...", manifest.name, manifest.version,
              target_vid, target_did);
        return ModuleLoader::LoadDriver(file);

    } else {
        KDBG2("Skip: %s does not support %x:%x", manifest.name, target_vid, target_did);
        return 0;
    }
}

void* ModuleLoader::LoadDriver(File* file) {
    if (!file) return 0;

    // Read ELF Header
    struct elf_header header;
    file->Seek(0);
    if (file->Read((uint8_t*)&header, sizeof(header)) != (int)sizeof(header)) {
        KDBG1("Module Error: Failed to read ELF header");
        return 0;
    }

    if (header.magic != ELF_MAGIC) {
        KDBG1("Module Error: Invalid ELF Magic");
        return 0;
    }
    if (header.type != 1) {  // ET_REL = 1 (Relocatable)
        KDBG1("Module Error: Not a relocatable object (.o)");
        return 0;
    }

    // Validate section header metadata before using it
    if (header.sh_entry_count == 0 || header.sh_size == 0) {
        KDBG1("Module Error: Empty section header table");
        return 0;
    }
    if (header.sh_entry_count > 65536 || header.sh_size != sizeof(elf_section_header)) {
        KDBG1("Module Error: Section header size mismatch (expected %d, got %d)",
              sizeof(elf_section_header), header.sh_size);
        return 0;
    }
    if (header.sh_str_index >= header.sh_entry_count) {
        KDBG1("Module Error: String table index out of range");
        return 0;
    }
    uint64_t sh_size64 = (uint64_t)header.sh_entry_count * (uint64_t)sizeof(elf_section_header);
    if (sh_size64 > 0xFFFFFFFFu) {
        KDBG1("Module Error: Section header table too large");
        return 0;
    }
    uint32_t sh_size = (uint32_t)sh_size64;

    // Read Section Headers
    struct elf_section_header* sections = (struct elf_section_header*)kmalloc(sh_size);
    if (!sections) {
        KDBG1("Module Error: Failed to allocate section headers");
        return 0;
    }
    file->Seek(header.sh_offset);
    if (file->Read((uint8_t*)sections, sh_size) != (int)sh_size) {
        KDBG1("Module Error: Failed to read section headers");
        kfree(sections);
        return 0;
    }

    // Read Section String Table (to find section names if needed)
    struct elf_section_header* strtab_hdr = &sections[header.sh_str_index];
    char* strtab = (char*)kmalloc(strtab_hdr->size);
    if (!strtab) {
        KDBG1("Module Error: Failed to allocate section string table");
        kfree(sections);
        return 0;
    }
    file->Seek(strtab_hdr->offset);
    if (file->Read((uint8_t*)strtab, strtab_hdr->size) != (int)strtab_hdr->size) {
        KDBG1("Module Error: Failed to read section string table");
        kfree(strtab);
        kfree(sections);
        return 0;
    }

    // Allocate Memory for Sections
    // Iterate all sections. If flags has SHF_ALLOC (0x2).
    bool alloc_failed = false;
    for (int i = 0; i < header.sh_entry_count; i++) {
        if (sections[i].flags & 0x2) {
            // Skip sections with zero size (e.g. .note.GNU-stack, empty BSS)
            if (sections[i].size == 0) {
                sections[i].addr = 0;
                continue;
            }
            void* mem = kmalloc(sections[i].size);
            if (!mem) {
                KDBG1("Module Error: Failed to allocate section %d (size %d)", i,
                      sections[i].size);
                alloc_failed = true;
                break;
            }

            // If it is NOT BSS (NOBITS), read data from file
            if (sections[i].type != 8) {
                file->Seek(sections[i].offset);
                if (file->Read((uint8_t*)mem, sections[i].size) != (int)sections[i].size) {
                    KDBG1("Module Error: Failed to read section %d data", i);
                    kfree(mem);
                    alloc_failed = true;
                    break;
                }
            } else {
                // Zero out BSS
                memset(mem, 0, sections[i].size);
            }
            // Store the KERNEL VIRTUAL ADDRESS in the section header's 'addr' field
            // Use this later to resolve addresses.
            sections[i].addr = (uint32_t)mem;
        } else {
            sections[i].addr = 0;
        }
    }
    if (alloc_failed) {
        // Free any SHF_ALLOC sections allocated so far
        for (int j = 0; j < header.sh_entry_count; j++) {
            if (sections[j].addr != 0 && (sections[j].flags & 0x2)) {
                kfree((void*)sections[j].addr);
            }
        }
        kfree(sections);
        kfree(strtab);
        return 0;
    }

    // Link (Relocate)
    struct elf32_symbol* symtab = 0;
    uint32_t symtab_count = 0;
    char* strtab_sym = 0;
    uint32_t strtab_sym_size = 0;

    // Find Symbol Table
    for (int i = 0; i < header.sh_entry_count; i++) {
        if (sections[i].type == 2) {  // SHT_SYMTAB
            symtab = (struct elf32_symbol*)kmalloc(sections[i].size);
            if (!symtab) {
                KDBG1("Module Error: Failed to allocate symbol table");
                break;
            }
            file->Seek(sections[i].offset);
            if (file->Read((uint8_t*)symtab, sections[i].size) != (int)sections[i].size) {
                KDBG1("Module Error: Failed to read symbol table");
                kfree(symtab);
                symtab = nullptr;
                break;
            }
            symtab_count = sections[i].size / sizeof(elf32_symbol);

            // Load associated string table
            int link = sections[i].link;
            if (link < 0 || (uint32_t)link >= header.sh_entry_count) {
                KDBG1("Module Error: Symbol string table link out of range (%d)", link);
                kfree(symtab);
                symtab = nullptr;
                break;
            }
            strtab_sym = (char*)kmalloc(sections[link].size);
            if (!strtab_sym) {
                KDBG1("Module Error: Failed to allocate symbol string table");
                kfree(symtab);
                symtab = nullptr;
                break;
            }
            file->Seek(sections[link].offset);
            if (file->Read((uint8_t*)strtab_sym, sections[link].size) != (int)sections[link].size) {
                KDBG1("Module Error: Failed to read symbol string table");
                kfree(strtab_sym);
                strtab_sym = nullptr;
                kfree(symtab);
                symtab = nullptr;
                break;
            }
            strtab_sym_size = sections[link].size;
            break;
        }
    }

    // Process Relocation Sections
    bool relocation_failed = false;
    for (int i = 0; i < header.sh_entry_count; i++) {
        if (sections[i].type == 9) {  // SHT_REL (Relocation without Addend)
            if (sections[i].ent_size != sizeof(elf32_rel)) {
                KDBG1("Module Link Error: Relocation ent_size=%u (expected %u)",
                      sections[i].ent_size, (uint32_t)sizeof(elf32_rel));
                relocation_failed = true;
                break;
            }
            uint32_t count = sections[i].size / sections[i].ent_size;
            if (count == 0 || (sections[i].size % sections[i].ent_size) != 0) {
                KDBG1("Module Link Error: Invalid relocation table size");
                relocation_failed = true;
                break;
            }
            if (!symtab || symtab_count == 0) {
                KDBG1("Module Link Error: Missing symbol table for relocations");
                relocation_failed = true;
                break;
            }

            // Allocate buffer for relocs
            struct elf32_rel* rels = (struct elf32_rel*)kmalloc(sections[i].size);
            if (!rels) {
                KDBG1("Module Link Error: Failed to allocate relocation buffer");
                relocation_failed = true;
                break;
            }
            file->Seek(sections[i].offset);
            int relBytes = file->Read((uint8_t*)rels, sections[i].size);
            if (relBytes != (int)sections[i].size) {
                KDBG1("Module Link Error: Failed to read relocation entries");
                kfree(rels);
                relocation_failed = true;
                break;
            }

            // The section modifying (patching)
            uint32_t target_section_idx = sections[i].info;
            if (target_section_idx >= header.sh_entry_count) {
                KDBG1("Module Link Error: Invalid relocation target section index");
                kfree(rels);
                relocation_failed = true;
                break;
            }
            if ((sections[target_section_idx].flags & 0x2) == 0) {
                KDBG2("Module Link: Skipping relocations for non-alloc section index %d",
                      target_section_idx);
                kfree(rels);
                continue;
            }
            uint32_t target_size = sections[target_section_idx].size;
            uint32_t target_base = sections[target_section_idx].addr;
            if (target_base == 0 || target_size == 0) {
                KDBG1("Module Link Error: Relocation target section not loaded");
                kfree(rels);
                relocation_failed = true;
                break;
            }
            if (target_size < sizeof(uint32_t)) {
                KDBG1("Module Link Error: Relocation target section too small");
                kfree(rels);
                relocation_failed = true;
                break;
            }

            for (uint32_t r = 0; r < count; r++) {
                uint32_t sym_idx = ELF32_R_SYM(rels[r].info);
                uint32_t type = ELF32_R_TYPE(rels[r].info);
                uint32_t offset = rels[r].offset;  // Offset inside the target section

                if (sym_idx >= symtab_count) {
                    KDBG1("Module Link Error: Relocation symbol index out of range");
                    relocation_failed = true;
                    break;
                }
                if (type != R_386_32 && type != R_386_PC32) {
                    KDBG1("Module Link Error: Unsupported relocation type %d", type);
                    relocation_failed = true;
                    break;
                }
                if (offset > target_size - sizeof(uint32_t)) {
                    KDBG1("Module Link Error: Relocation offset out of bounds");
                    relocation_failed = true;
                    break;
                }

                // Need to write the patched address
                uint32_t* patch_addr = (uint32_t*)(target_base + offset);

                // Resolve Symbol Value
                uint32_t sym_val = 0;

                if (symtab[sym_idx].shndx == 0) {
                    // SHN_UNDEF: External Symbol
                    // Lookup in Kernel Symbol Table
                    uint32_t name_off = symtab[sym_idx].name;
                    if (name_off >= strtab_sym_size) {
                        KDBG1("Module Link Error: Symbol name offset out of string-table bounds");
                        relocation_failed = true;
                        break;
                    }
                    // Verify NUL terminator exists within the string table bounds
                    const char* name = strtab_sym + name_off;
                    size_t max_len = strtab_sym_size - name_off;
                    bool found_nul = false;
                    for (size_t k = 0; k < max_len; k++) {
                        if (name[k] == '\0') { found_nul = true; break; }
                    }
                    if (!found_nul) {
                        KDBG1("Module Link Error: Symbol name at offset %u not NUL-terminated "
                              "within %u-byte strtab", name_off, strtab_sym_size);
                        relocation_failed = true;
                        break;
                    }
                    sym_val = SymbolTable::Lookup(name);

                    if (sym_val == 0) {
                        KDBG1("Module Link Error: Undefined symbol '%s'", name);
                        relocation_failed = true;
                        break;
                    }
                } else {
                    // Internal Symbol (defined in another section of this module)
                    uint32_t sec_idx = symtab[sym_idx].shndx;
                    if (sec_idx >= header.sh_entry_count) {
                        KDBG1("Module Link Error: Symbol section index out of range");
                        relocation_failed = true;
                        break;
                    }
                    // Address = Base of Section + Offset inside section
                    sym_val = sections[sec_idx].addr + symtab[sym_idx].value;
                    if (sym_val == 0) {
                        KDBG1("Module Link Error: Internal symbol has zero address");
                        relocation_failed = true;
                        break;
                    }
                }

                // Apply Logic
                if (type == R_386_32) {
                    // S + A (Addend is implicit in the target memory location)
                    *patch_addr += sym_val;
                } else if (type == R_386_PC32) {
                    // S + A - P (Symbol - Location)
                    *patch_addr += (sym_val - (uint32_t)patch_addr);
                }
            }
            kfree(rels);
            if (relocation_failed) break;
        }
    }

    if (relocation_failed) {
        // Free any SHF_ALLOC section memory before releasing section table
        for (int i = 0; i < header.sh_entry_count; i++) {
            if (sections[i].addr != 0 && (sections[i].flags & 0x2)) {
                kfree((void*)sections[i].addr);
            }
        }
        kfree(sections);
        kfree(strtab);
        kfree(symtab);
        kfree(strtab_sym);
        return 0;
    }

    // Find Entry Point (CreateDriverInstance)
    void* entry_point = 0;
    if (symtab && strtab_sym) {
        for (uint32_t i = 0; i < symtab_count; i++) {
            uint32_t name_off = symtab[i].name;
            if (name_off >= strtab_sym_size) continue;
            const char* name = strtab_sym + name_off;

            // Check for the magic function name
            const char* target = "CreateDriverInstance";
            uint32_t target_len = 20;
            bool match = false;
            if (name_off + target_len + 1 <= strtab_sym_size) {
                match = true;
                for (uint32_t c = 0; c < target_len; c++)
                    if ((uint8_t)target[c] != (uint8_t)name[c]) { match = false; break; }
                if (name[target_len] != 0) match = false;
            }

            if (match) {
                uint32_t sec_idx = symtab[i].shndx;
                if (sec_idx != 0 && sec_idx < header.sh_entry_count) {
                    entry_point = (void*)(sections[sec_idx].addr + symtab[i].value);
                    break;
                }
            }
        }
    }

    // Cleanup
    kfree(sections);
    kfree(strtab);
    kfree(symtab);
    kfree(strtab_sym);

    return entry_point;
}

// Returns true if valid metadata is found, filling the 'info' struct
bool ModuleLoader::Probe(File* file, DriverManifest* info) {
    if (!file || !info) return false;

    // Read ELF Header
    struct elf_header header;
    file->Seek(0);
    if (file->Read((uint8_t*)&header, sizeof(header)) != (int32_t)sizeof(header)) return false;

    if (header.magic != ELF_MAGIC) return false;

    // Validate section header metadata before using it
    if (header.sh_entry_count == 0 || header.sh_size == 0) return false;
    if (header.sh_entry_count > 65536 || header.sh_size != sizeof(elf_section_header)) {
        KDBG1("Probe: Section header size mismatch (expected %d, got %d)",
              sizeof(elf_section_header), header.sh_size);
        return false;
    }
    if (header.sh_str_index >= header.sh_entry_count) return false;
    uint64_t sh_size64 = (uint64_t)header.sh_entry_count * (uint64_t)sizeof(elf_section_header);
    if (sh_size64 > 0xFFFFFFFFu) return false;
    uint32_t sh_size = (uint32_t)sh_size64;

    // Read Section Headers
    struct elf_section_header* sections = (struct elf_section_header*)kmalloc(sh_size);
    if (!sections) return false;
    file->Seek(header.sh_offset);
    if (file->Read((uint8_t*)sections, sh_size) != (int)sh_size) {
        kfree(sections);
        return false;
    }

    // Read Section String Table (to find section names)
    // Need this to search for ".driver_info" by name
    struct elf_section_header* strtab_hdr = &sections[header.sh_str_index];
    char* strtab = 0;
    uint32_t strtab_size = strtab_hdr->size;
    if (strtab_size > 0) {
        strtab = (char*)kmalloc(strtab_hdr->size);
        if (strtab) {
            file->Seek(strtab_hdr->offset);
            if (file->Read((uint8_t*)strtab, strtab_size) != (int)strtab_size) {
                kfree(strtab);
                strtab = 0;
            }
        }
    }

    bool found = false;

    // Iterate Sections to find ".driver_info"
    for (int i = 0; i < header.sh_entry_count; i++) {
        if (!strtab) break;
        uint32_t name_off = sections[i].name;
        if (name_off >= strtab_size) continue;
        const char* sec_name = strtab + name_off;

        // Manual string comparison for ".driver_info"
        const char* target = ".driver_info";
        bool match = true;
        for (int c = 0; target[c] != 0; c++) {
            uint32_t idx = name_off + c;
            if (idx >= strtab_size) { match = false; break; }
            if (target[c] != sec_name[c]) {
                match = false;
                break;
            }
        }
        // Ensure the names are the same length (null terminator check)
        if (match) {
            uint32_t idx = name_off + 12;
            if (idx >= strtab_size) match = false;
            else if (sec_name[12] != 0) match = false;
        }

        // If found, verify size and read data
        if (match) {
            // Safety Check:
            // This prevents reading garbage if the driver was compiled with an old struct
            // definition.
            if (sections[i].size >= sizeof(DriverManifest)) {
                file->Seek(sections[i].offset);
                int bytesRead = file->Read((uint8_t*)info, sizeof(DriverManifest));
                if (bytesRead == sizeof(DriverManifest) && info->magic == DRIVER_INFO_MAGIC) {
                    found = true;
                } else if (bytesRead != sizeof(DriverManifest)) {
                    KDBG1("Probe: short read of .driver_info at section %d "
                          "(offset=0x%x, size=%u, got=%d)",
                          i, sections[i].offset, sizeof(DriverManifest), bytesRead);
                }
            } else {
                KDBG1("Warning: '.driver_info' section too small (Old driver version?)");
            }
            break;  // Stop searching once found
        }
    }

    // Cleanup Heap
    kfree(sections);
    kfree(strtab);

    // Important: Reset file pointer to 0
    file->Seek(0);

    return found;
}
