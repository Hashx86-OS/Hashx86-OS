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

#define KDBG_COMPONENT "GDT"
#include <core/gdt.h>
#include <core/tss.h>
#include <debug.h>
#include <string.h>

extern "C" void tss_flush();

GDT g_gdt[NO_GDT_DESCRIPTORS];

GDT_PTR g_gdt_ptr;
TaskStateSegment g_tss;

/**
 * gdt_set_entry() - Program one GDT descriptor.
 * @index: Descriptor slot to fill.
 * @base: 32-bit segment base address.
 * @limit: 20-bit segment limit.
 * @access: Descriptor access byte.
 * @gran: Granularity/flags nibble (high four bits).
 */
void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    GDT* entry = &g_gdt[index];

    entry->segment_limit = limit & 0xFFFF;
    entry->base_low = base & 0xFFFF;
    entry->base_middle = (base >> 16) & 0xFF;
    entry->access = access;

    entry->granularity = (limit >> 16) & 0x0F;
    entry->granularity = entry->granularity | (gran & 0xF0);

    entry->base_high = (base >> 24 & 0xFF);

    KDBG1("SetEntry idx=%d base=0x%x limit=0x%x access=0x%x gran=0x%x", index, base, limit, access,
          gran);
}

/**
 * gdt_init() - Set up the flat kernel/user segments and a TSS.
 *
 * Installs the NULL, kernel code/data, user code/data, and TSS descriptors,
 * then loads the GDT and flushes the task register.
 */
void gdt_init() {
    g_gdt_ptr.limit = sizeof(g_gdt) - 1;
    g_gdt_ptr.base_address = (uint32_t)g_gdt;

    // NULL segment.
    gdt_set_entry(0, 0, 0, 0, 0);
    // Kernel code segment.
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    // Kernel data segment.
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    // User code segment.
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    // User data segment.
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    // TSS segment.
    memset(&g_tss, 0, sizeof(TaskStateSegment));

    // Use kernel data (0x10) as the ring-0 stack segment.
    g_tss.ss0 = 0x10;
    // Seed esp0 with the current boot stack pointer. Scheduler::Schedule()
    // overwrites esp0 per thread later, but without this any ring3->ring0
    // transition before the first schedule would use esp0=0, causing a page
    // fault and then a triple fault.
    asm volatile("mov %%esp, %0" : "=r"(g_tss.esp0));
    // Point iomap_base at size to disable the bitmap.
    g_tss.iomap_base = sizeof(TaskStateSegment);

    // Program the TSS descriptor: base &g_tss, limit size - 1, access 0x89
    // (present, ring-0, system, 32-bit available TSS), byte granularity.
    gdt_set_entry(5, (uint32_t)&g_tss, sizeof(g_tss) - 1, 0x89, 0x00);

    // Load the GDT.
    load_gdt((uint32_t)&g_gdt_ptr);

    // Load the TSS into the task register.
    tss_flush();

    KDBG1("Initialized base=0x%x limit=0x%x", (uint32_t)g_gdt, sizeof(g_gdt) - 1);
}
