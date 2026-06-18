/**
 * @file        elf.cpp
 * @brief       ELF Binary Loader for #x86
 *
 * @date        29/01/2026
 * @version     1.0.0-beta
 */

#define KDBG_COMPONENT "ELFLOADER"
#include <core/elf.h>

namespace {

bool SectionNameEquals(const char* namesTable, uint32_t tableSize, uint32_t nameOffset,
                       const char* target) {
    if (!namesTable || !target || nameOffset >= tableSize) return false;

    uint32_t idx = 0;
    while ((nameOffset + idx) < tableSize) {
        char a = namesTable[nameOffset + idx];
        char b = target[idx];
        if (a != b) return false;
        if (a == '\0') return true;
        idx++;
    }
    return false;
}

uint16_t DetectELFAppType(File* elf, const elf_header& header) {
    if (!elf) return APP_BINARY_GUI;
    if (header.sh_offset == 0 || header.sh_entry_count == 0) return APP_BINARY_GUI;
    if (header.sh_size != sizeof(elf_section_header)) return APP_BINARY_GUI;

    uint32_t shTableSize = sizeof(elf_section_header) * header.sh_entry_count;
    if (shTableSize > elf->size || header.sh_offset > elf->size - shTableSize) {
        return APP_BINARY_GUI;
    }
    elf_section_header* shTable = new elf_section_header[header.sh_entry_count];
    if (!shTable) return APP_BINARY_GUI;

    elf->Seek(header.sh_offset);
    if ((uint32_t)elf->Read((uint8_t*)shTable, shTableSize) != shTableSize) {
        delete[] shTable;
        return APP_BINARY_GUI;
    }

    if (header.sh_str_index >= header.sh_entry_count) {
        delete[] shTable;
        return APP_BINARY_GUI;
    }

    elf_section_header* shStr = &shTable[header.sh_str_index];
    if (shStr->size == 0) {
        delete[] shTable;
        return APP_BINARY_GUI;
    }

    if (shStr->size > elf->size || shStr->offset > elf->size - shStr->size) {
        delete[] shTable;
        return APP_BINARY_GUI;
    }

    char* shNames = new char[shStr->size];
    if (!shNames) {
        delete[] shTable;
        return APP_BINARY_GUI;
    }

    elf->Seek(shStr->offset);
    if ((uint32_t)elf->Read((uint8_t*)shNames, shStr->size) != shStr->size) {
        delete[] shNames;
        delete[] shTable;
        return APP_BINARY_GUI;
    }

    uint16_t appType = APP_BINARY_GUI;

    for (uint32_t i = 0; i < header.sh_entry_count; i++) {
        elf_section_header* sh = &shTable[i];

        if (!SectionNameEquals(shNames, shStr->size, sh->name, ".hx86meta")) {
            continue;
        }

        if (sh->size < sizeof(hx86_app_meta)) {
            continue;
        }

        hx86_app_meta meta;
        elf->Seek(sh->offset);
        if ((uint32_t)elf->Read((uint8_t*)&meta, sizeof(meta)) != sizeof(meta)) {
            continue;
        }

        if (meta.magic != HX86_APP_META_MAGIC || meta.version != HX86_APP_META_VERSION) {
            continue;
        }

        if (meta.appType == APP_BINARY_CLI || meta.appType == APP_BINARY_GUI) {
            appType = meta.appType;
        }
        break;
    }

    delete[] shNames;
    delete[] shTable;
    return appType;
}

}  // namespace

ELFLoader::ELFLoader(Paging* pager, Scheduler* scheduler) {
    this->pager = pager;
    this->scheduler = scheduler;
};
ELFLoader::~ELFLoader(){

};

ProcessControlBlock* ELFLoader::loadELF(File* elf, void* args) {
    if (!elf) return nullptr;

    // Validate Header
    struct elf_header header;
    elf->Seek(0);
    if (elf->Read((uint8_t*)&header, sizeof(elf_header)) != sizeof(elf_header)) {
        KDBG1("Error: ELF file too short");
        return nullptr;
    }

    if (header.magic != ELF_MAGIC) {
        KDBG1("Error: Invalid ELF Magic!");
        return nullptr;
    }

    uint16_t detectedType = DetectELFAppType(elf, header);

    // Create new PCB
    ProcessControlBlock* pELF =
        scheduler->CreateProcess(false, (void (*)(void*))header.entry, args);

    if (!pELF) {
        KDBG1("Error: Failed to create process for ELF");
        return nullptr;
    }
    pELF->appType = detectedType;

    auto cleanup_process = [&]() {
        if (pELF) {
            scheduler->KillProcess(pELF->pid);
            pELF = nullptr;
        }
    };

    // Read ELF Headers
    // Validate ph_entry_count before allocation to prevent oversized allocation
    if (header.ph_entry_count == 0 || header.ph_entry_count > 65536) {
        KDBG1("Error: Invalid program header count (%d)", header.ph_entry_count);
        cleanup_process();
        return nullptr;
    }
    uint32_t ph_size = sizeof(elf_program_header) * header.ph_entry_count;
    elf_program_header* ph_table = new elf_program_header[header.ph_entry_count];
    if (!ph_table) {
        KDBG1("Error: Failed to allocate ELF program header table");
        cleanup_process();
        return nullptr;
    }

    elf->Seek(header.ph_offset);
    if (elf->Read((uint8_t*)ph_table, ph_size) != ph_size) {
        KDBG1("Error: Could not read Program Headers");
        delete[] ph_table;
        // Ideally kill the process here too
        cleanup_process();
        return nullptr;
    }

    uint32_t max_virt_end = 0;

    // Load ELF Segments
    for (int i = 0; i < header.ph_entry_count; i++) {
        elf_program_header* ph = &ph_table[i];

        if (ph->type != 1) continue;  // PT_LOAD only
        KDBG3("Segment: Virt=0x%x MemSize=0x%x FileSize=0x%x", ph->virt_addr, ph->mem_size,
              ph->file_size);

        if (ph->file_size > ph->mem_size) {
            KDBG1("ELF Load Error: file_size > mem_size");
            delete[] ph_table;
            cleanup_process();
            return nullptr;
        }
        uint64_t file_end = (uint64_t)ph->offset + (uint64_t)ph->file_size;
        if (file_end > (uint64_t)elf->size) {
            KDBG1("ELF Load Error: Segment exceeds file size");
            delete[] ph_table;
            cleanup_process();
            return nullptr;
        }

        // Calculate alignment
        uint32_t start = (uint32_t)ph->virt_addr;
        uint32_t end = start + ph->mem_size;
        uint32_t page_start = start & ~(PAGE_SIZE - 1);
        uint32_t page_end = (end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        // Allocate Pages
        // Must be in identity-mapped range (<256MB) because kernel reads ELF data
        // and zeroes BSS via physical addresses during loading
        for (uint32_t addr = page_start; addr < page_end; addr += PAGE_SIZE) {
            uint32_t phys_frame = (uint32_t)pmm_alloc_block_low(256 * 1024 * 1024);
            if (!phys_frame) {
                KDBG1("ELF Load: Out of low memory for segment pages!");
                delete[] ph_table;
                cleanup_process();
                return nullptr;
            }
            if (!this->pager->MapPage(pELF->page_directory, addr, phys_frame,
                                      PAGE_PRESENT | PAGE_RW | PAGE_USER)) {
                KDBG1("ELF Load: Failed to map segment page");
                delete[] ph_table;
                cleanup_process();
                return nullptr;
            }
            KDBG3("Mapped Page: Virt=0x%x Phys=0x%x", addr, phys_frame);
        }

        // Load Data into those pages
        uint32_t bytes_to_read = ph->file_size;
        uint32_t virtual_addr = (uint32_t)ph->virt_addr;

        // Read ph header
        elf->Seek(ph->offset);

        while (bytes_to_read > 0) {
            uint32_t phys_ptr = pager->GetPhysicalAddress(pELF->page_directory, virtual_addr);
            if (phys_ptr == 0xFFFFFFFF) {
                KDBG1("ELF Load Error: Failed to resolve physical address");
                delete[] ph_table;
                cleanup_process();
                return nullptr;
            }

            uint32_t offset_in_page = virtual_addr % PAGE_SIZE;
            uint32_t space_in_page = PAGE_SIZE - offset_in_page;
            uint32_t chunk = (bytes_to_read < space_in_page) ? bytes_to_read : space_in_page;

            if (elf->Read((uint8_t*)phys_ptr, chunk) != (int)chunk) {
                KDBG1("ELF Load Error: Failed to read segment data");
                delete[] ph_table;
                cleanup_process();
                return nullptr;
            }

            virtual_addr += chunk;
            bytes_to_read -= chunk;
        }

        // Zero out BSS
        uint32_t bytes_to_zero = ph->mem_size - ph->file_size;
        while (bytes_to_zero > 0) {
            uint32_t phys_ptr = pager->GetPhysicalAddress(pELF->page_directory, virtual_addr);
            if (phys_ptr == 0xFFFFFFFF) {
                KDBG1("ELF Load Error: Failed to resolve physical address for BSS");
                delete[] ph_table;
                cleanup_process();
                return nullptr;
            }

            uint32_t offset_in_page = virtual_addr % PAGE_SIZE;
            uint32_t space_in_page = PAGE_SIZE - offset_in_page;
            uint32_t chunk = (bytes_to_zero < space_in_page) ? bytes_to_zero : space_in_page;

            memset((void*)phys_ptr, 0, chunk);

            virtual_addr += chunk;
            bytes_to_zero -= chunk;
        }

        if (end > max_virt_end) max_virt_end = end;
    }

    delete[] ph_table;

    // Set up heap segment — no pages pre-allocated, brk will grow on demand
    max_virt_end = (max_virt_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    pELF->heap.startAddress = max_virt_end;
    pELF->heap.endAddress = max_virt_end;  // Zero-size initially, grown by brk
    {  // Prevent overflow when max_virt_end is near top of address space
        uint64_t maxAddr = (uint64_t)max_virt_end + (1024ULL * 1024ULL * 256ULL);
        if (maxAddr > 0xFFFFFFFFULL) maxAddr = 0xFFFFFFFFULL;
        pELF->heap.maxAddress = (uint32_t)maxAddr;
    }

    KDBG1("ELF Loaded. Entry: 0x%x Heap start: 0x%x Type: %s", header.entry, max_virt_end,
           (pELF->appType == APP_BINARY_CLI) ? "CLI" : "GUI");

    return pELF;
};

void ELFLoader::ElevatetoKernel(ProcessControlBlock* pELF) {
    // TODO: implement elevation — set the kernel-process flag and perform
    // any required privilege/credential updates and sanity checks.
    // Future implementation should assign pELF->isKernelProcess = true and
    // update the page directory mask to allow access to kernel-space mappings.
    (void)pELF;
};
