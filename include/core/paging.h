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

#ifndef PAGING_H
#define PAGING_H

#include <core/memory.h>
#include <core/pmm.h>
#include <debug.h>
#include <stdint.h>

// Standard x86 paging flags.
#define PAGE_PRESENT 0x1
#define PAGE_RW 0x2
#define PAGE_USER 0x4
#define PAGE_WRITE_THRU 0x8
#define PAGE_NO_CACHE 0x10

#define PAGE_SIZE 4096

/**
 * class Paging - Manage x86 two-level page tables and address spaces.
 *
 * Owns the kernel master page directory and provides process address-space
 * creation, directory switching and page mapping primitives.
 */
class Paging {
public:
    Paging();
    ~Paging();

    /**
     * Activate() - Build the master page directory and enable paging.
     *
     * Identity-maps low memory (0 MiB - 256 MiB) for the kernel and high
     * memory (3 GiB - 4 GiB) for VRAM/MMIO, then enables paging by loading
     * CR3 and setting the PG bit in CR0.
     *
     * Context: Called once during boot, before any process exists.
     */
    void Activate();

    /**
     * CreateProcessDirectory() - Allocate a directory for a new process.
     *
     * Returns a zeroed user-space page directory that shares the kernel
     * page tables (low memory and hardware ranges).
     *
     * Return: The new page directory, or NULL on failure.
     */
    uint32_t* CreateProcessDirectory();

    /**
     * SwitchDirectory() - Load a page directory into CR3.
     * @new_dir: The page directory to activate.
     *
     * Context: Called on a context switch.
     */
    void SwitchDirectory(uint32_t* new_dir);

    /**
     * MapPage() - Map a physical address to a virtual address in a directory.
     * @directory: The page directory to modify.
     * @virtual_addr: Virtual address to map.
     * @physical_addr: Physical address to map to it.
     * @flags: Page flags (PAGE_PRESENT, PAGE_RW, PAGE_USER, ...).
     *
     * Allocates a page table if needed and flushes the TLB entry for the
     * virtual address. Rejects mappings in the kernel-shared ranges.
     *
     * Return: True on success, false on failure.
     */
    bool MapPage(uint32_t* directory, uint32_t virtual_addr, uint32_t physical_addr,
                 uint32_t flags);

    /**
     * GetPhysicalAddress() - Resolve the physical address of a virtual address.
     * @directory: The page directory to consult.
     * @virtual_addr: Virtual address to resolve.
     *
     * Return: The physical address, or 0xFFFFFFFF if it is not mapped.
     */
    uint32_t GetPhysicalAddress(uint32_t* directory, uint32_t virtual_addr);

    // Master directory template shared by all process address spaces.
    uint32_t* KernelPageDirectory;

private:
    bool is_paging_active;
};

#endif
