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

#define KDBG_COMPONENT "PAGING"
#include <core/paging.h>

Paging::Paging() : is_paging_active(false) {}

Paging::~Paging() {}

// Recursion guard: incremented before any PMM allocation in MapPage and
// decremented after. Page-fault handlers check this to avoid a recursive fault
// when pmm_alloc_block_low touches the PMM bitmap.
static int paging_map_depth = 0;

void Paging::Activate() {
    // Allocate the master page directory. It must live in the identity-mapped
    // range (<256 MB) so the kernel can access it after paging is enabled.
    KernelPageDirectory = (uint32_t*)pmm_alloc_block_low(256 * 1024 * 1024);

    if (!KernelPageDirectory || ((uint32_t)KernelPageDirectory & 0xFFF)) {
        KDBG1("CRITICAL ERROR: Page Directory NOT Aligned! Addr: 0x%x", KernelPageDirectory);
        while (1);  // Halt.
    }

    // Clear the directory.
    memset(KernelPageDirectory, 0, 4096);

    // Map lower memory (0-256 MB) for kernel code: 256 MB / 4 MB per table = 64 tables.
    for (uint32_t i = 0; i < 64; i++) {
        // Allocate a page table (1024 pages), again from low memory.
        uint32_t* page_table = (uint32_t*)pmm_alloc_block_low(256 * 1024 * 1024);
        if (!page_table) {
            KDBG1("CRITICAL: Failed to allocate page table for index %d!", i);
            while (1);
        }
        memset(page_table, 0, 4096);

        // Fill the table with identity mappings (virtual X = physical X).
        for (uint32_t j = 0; j < 1024; j++) {
            uint32_t phys_addr = (i * 1024 + j) * 4096;
            // Present and read/write flags.
            page_table[j] = phys_addr | PAGE_PRESENT | PAGE_RW;
        }

        // Install the table into the directory.
        KernelPageDirectory[i] = ((uint32_t)page_table) | PAGE_PRESENT | PAGE_RW;
    }

    // Map high memory (3-4 GB) for VRAM/MMIO: indices 768 to 1024, covering
    // 0xC0000000 to 0xFFFFFFFF.
    for (uint32_t i = 768; i < 1024; i++) {
        // Also from the identity-mapped range so the kernel can reach the tables.
        uint32_t* page_table = (uint32_t*)pmm_alloc_block_low(256 * 1024 * 1024);
        if (!page_table) {
            KDBG1("CRITICAL: Failed to allocate high-mem page table for index %d!", i);
            while (1);
        }
        memset(page_table, 0, 4096);

        // Identity-map the high memory addresses.
        for (uint32_t j = 0; j < 1024; j++) {
            uint32_t phys_addr = (i * 1024 + j) * 4096;
            page_table[j] = phys_addr | PAGE_PRESENT | PAGE_RW;
        }

        KernelPageDirectory[i] = ((uint32_t)page_table) | PAGE_PRESENT | PAGE_RW;
    }

    // Enable paging: load CR3 with the physical address of the directory.
    asm volatile("mov %0, %%cr3" : : "r"(KernelPageDirectory));

    // Enable the PG bit in CR0.
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" : : "r"(cr0));

    is_paging_active = true;
    KDBG1("Activated. Kernel (Low) and Hardware (High) Mapped.");
}

uint32_t* Paging::CreateProcessDirectory() {
    // Allocate a new directory with pmm_alloc for 4KB alignment. Must be in
    // the identity-mapped range (<256 MB) so the kernel can read/write entries.
    uint32_t* new_dir = (uint32_t*)pmm_alloc_block_low(256 * 1024 * 1024);
    if (!new_dir) return 0;

    // Clear the user space.
    memset(new_dir, 0, 4096);

    // Link the kernel space (low memory: 0-256 MB).
    for (int i = 0; i < 64; i++) {
        new_dir[i] = KernelPageDirectory[i];
    }

    // Link the hardware space (high memory: 3-4 GB).
    for (int i = 768; i < 1024; i++) {
        new_dir[i] = KernelPageDirectory[i];
    }

    KDBG2("CreateProcessDirectory addr=0x%x", new_dir);
    return new_dir;
}

void Paging::SwitchDirectory(uint32_t* new_dir) {
    if (!new_dir) return;
    asm volatile("mov %0, %%cr3" : : "r"(new_dir));
    KDBG3("SwitchDirectory addr=0x%x", new_dir);
}

bool Paging::MapPage(uint32_t* directory, uint32_t virtual_addr, uint32_t physical_addr,
                     uint32_t flags) {
    uint32_t pd_idx = virtual_addr >> 22;
    uint32_t pt_idx = (virtual_addr >> 12) & 0x03FF;

    // Guard against modifying kernel-shared PDE ranges (0-64 and 768-1023).
    // These entries are shared across all processes via CreateProcessDirectory;
    // altering them would corrupt kernel mappings for every process.
    if (pd_idx < 64 || (pd_idx >= 768 && pd_idx < 1024)) {
        KDBG1(
            "MapPage: rejected attempt to map kernel-range virtual address 0x%x "
            "(pd_idx=%u)",
            virtual_addr, pd_idx);
        return false;
    }

    // Allocate the page table if one does not exist yet.
    if (!(directory[pd_idx] & PAGE_PRESENT)) {
        // Recursion guard: detect if the PMM allocation causes a page fault.
        if (paging_map_depth > 0) {
            KDBG1("MapPage: RECURSION DETECTED! paging_map_depth=%d", paging_map_depth);
            return false;
        }
        paging_map_depth++;

        // Allocate the new table via the PMM (low memory, <256 MB).
        uint32_t* new_table = (uint32_t*)pmm_alloc_block_low(256 * 1024 * 1024);

        paging_map_depth--;

        if (!new_table) {
            KDBG1("MapPage: Failed to allocate Page Table! Low Memory Exhausted?");
            return false;
        }

        memset(new_table, 0, 4096);

        // Link the table into the directory.
        directory[pd_idx] = (uint32_t)new_table | PAGE_PRESENT | PAGE_RW | PAGE_USER;
    }

    uint32_t* table = (uint32_t*)(directory[pd_idx] & 0xFFFFF000);
    table[pt_idx] = (physical_addr & 0xFFFFF000) | flags;

    // Invalidate the TLB entry for this virtual address. Without this, stale
    // TLB entries can cause phantom page faults when pages are newly mapped
    // or permissions are changed.
    asm volatile("invlpg (%0)" ::"r"(virtual_addr) : "memory");
    KDBG2("MapPage virt=0x%x phys=0x%x flags=0x%x", virtual_addr, physical_addr, flags);
    return true;
}

uint32_t Paging::GetPhysicalAddress(uint32_t* directory, uint32_t virtual_addr) {
    uint32_t pd_idx = virtual_addr >> 22;
    uint32_t pt_idx = (virtual_addr >> 12) & 0x03FF;

    if (!(directory[pd_idx] & PAGE_PRESENT)) return 0xFFFFFFFF;

    uint32_t* table = (uint32_t*)(directory[pd_idx] & 0xFFFFF000);
    if (!(table[pt_idx] & PAGE_PRESENT)) return 0xFFFFFFFF;

    return (table[pt_idx] & 0xFFFFF000) + (virtual_addr & 0xFFF);
}
