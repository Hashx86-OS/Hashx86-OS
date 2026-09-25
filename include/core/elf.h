/*
 * MIT License
 *
 * Copyright (c) 2025 Malaka Gunawardana
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef ELF_H
#define ELF_H

#include <core/filesystem/File.h>
#include <core/memory.h>
#include <core/paging.h>
#include <core/pmm.h>
#include <core/scheduler.h>
#include <debug.h>
#include <stdint.h>
#include <types.h>

#define ELF_MAGIC 0x464C457F

#define HX86_APP_META_MAGIC 0x36385848
#define HX86_APP_META_VERSION 1

// ELF file header (identifies the executable and locates its section tables).
struct elf_header {
    uint32_t magic;
    uint8_t ident[12];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t ph_offset;
    uint32_t sh_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t ph_entry_size;
    uint16_t ph_entry_count;
    uint16_t sh_size;
    uint16_t sh_entry_count;
    uint16_t sh_str_index;
} __attribute__((packed));

// Program header: describes a memory segment to load from the file.
struct elf_program_header {
    uint32_t type;
    uint32_t offset;
    uint32_t virt_addr;
    uint32_t phys_addr;
    uint32_t file_size;
    uint32_t mem_size;
    uint32_t flags;
    uint32_t align;
} __attribute__((packed));

// Section header: describes a section inside the ELF file.
struct elf_section_header {
    uint32_t name;
    uint32_t type;
    uint32_t flags;
    uint32_t addr;
    uint32_t offset;
    uint32_t size;
    uint32_t link;
    uint32_t info;
    uint32_t align;
    uint32_t ent_size;
} __attribute__((packed));

// Symbol table entry (ELF32).
struct elf32_symbol {
    uint32_t name;
    uint32_t value;
    uint32_t size;
    uint8_t info;
    uint8_t other;
    uint16_t shndx;
} __attribute__((packed));

// Relocation entry (SHT_REL).
struct elf32_rel {
    uint32_t offset;
    uint32_t info;
} __attribute__((packed));

// Relocation entry (SHT_RELA).
struct elf_rela_entry {
    uint32_t offset;
    uint32_t info;
    int32_t addend;
} __attribute__((packed));

/**
 * struct ProgramArguments - Up to five string arguments passed to a new program.
 * @str1: First argument string.
 * @str2: Second argument string.
 * @str3: Third argument string.
 * @str4: Fourth argument string.
 * @str5: Fifth argument string.
 */
struct ProgramArguments {
    const char* str1;
    const char* str2;
    const char* str3;
    const char* str4;
    const char* str5;
};

/**
 * struct hx86_app_meta - Application metadata embedded in the .hx86meta section.
 * @magic: Magic value identifying a valid metadata block (HX86_APP_META_MAGIC).
 * @version: Version of the metadata format (HX86_APP_META_VERSION).
 * @appType: Application binary type (GUI or CLI).
 */
struct hx86_app_meta {
    uint32_t magic;
    uint16_t version;
    uint16_t appType;
} __attribute__((packed));

/**
 * FreeProgramArguments() - Free all strings held by a ProgramArguments struct.
 * @a: The arguments to free, or NULL.
 *
 * Frees each non-NULL string and then the struct itself.
 */
inline void FreeProgramArguments(ProgramArguments* a) {
    if (!a) return;
    if (a->str1) {
        kfree((void*)a->str1);
    }
    if (a->str2) {
        kfree((void*)a->str2);
    }
    if (a->str3) {
        kfree((void*)a->str3);
    }
    if (a->str4) {
        kfree((void*)a->str4);
    }
    if (a->str5) {
        kfree((void*)a->str5);
    }
    delete a;
}

/**
 * class ELFLoader - Load ELF executables into new processes.
 *
 * Uses the paging and scheduler layers to parse an ELF file, allocate pages,
 * and create the process control block for a new program.
 */
class ELFLoader {
private:
    Paging* pager;
    Scheduler* scheduler;  // Renamed to pManager for consistency, but implies Scheduler.

public:
    ELFLoader(Paging* pager, Scheduler* pManager);
    ~ELFLoader();

    /**
     * loadELF() - Load an ELF program held in a memory buffer.
     * @mod_start: Start address of the loaded module.
     * @mod_end: End address of the loaded module.
     * @args: Arguments passed to the new program.
     *
     * Return: The new process control block, or NULL on failure.
     */
    ProcessControlBlock* loadELF(uint32_t mod_start, uint32_t mod_end, void* args);

    /**
     * loadELF() - Load an ELF program from a File object.
     * @elf: The open ELF file.
     * @args: Arguments passed to the new program.
     *
     * Return: The new process control block, or NULL on failure.
     */
    ProcessControlBlock* loadELF(File* elf, void* args);

    /**
     * ElevatetoKernel() - Promote a process to kernel privileges.
     * @pcb: The process control block to promote.
     */
    void ElevatetoKernel(ProcessControlBlock* pcb);
};

#endif  // ELF_H
