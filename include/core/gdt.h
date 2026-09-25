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

#ifndef GDT_H
#define GDT_H

#include <types.h>

#define NO_GDT_DESCRIPTORS 8

// GDT selector indices (index * 8).
#define NULL_SELECTOR 0x00         // 0 * 8
#define KERNEL_CODE_SELECTOR 0x08  // 1 * 8
#define KERNEL_DATA_SELECTOR 0x10  // 2 * 8
#define USER_CODE_SELECTOR 0x18    // 3 * 8
#define USER_DATA_SELECTOR 0x20    // 4 * 8
#define TSS_SELECTOR 0x28          // 5 * 8

/**
 * struct GDT - An 8-byte segment descriptor in the global descriptor table.
 * @segment_limit: Lower 16 bits of the segment limit.
 * @base_low: Lower 16 bits of the segment base address.
 * @base_middle: Bits 16..23 of the segment base address.
 * @access: Access byte (present, privilege, type flags).
 * @granularity: High nibble holds the flags, low nibble the upper 4 bits of
 *               the 20-bit limit.
 * @base_high: Bits 24..31 of the segment base address.
 */
typedef struct {
    uint16_t segment_limit;  // Segment limit, bits 0..15.
    uint16_t base_low;       // Segment base, bits 0..15.
    uint8_t base_middle;     // Segment base, bits 16..23.
    uint8_t access;          // Access byte.
    uint8_t granularity;     // Flags (high nibble) + upper limit bits (low nibble).
    uint8_t base_high;       // Segment base, bits 24..31.
} __attribute__((packed)) GDT;

/**
 * struct GDT_PTR - Packed 48-bit GDT pointer used by the LGDT instruction.
 * @limit: Size of the GDT in bytes, minus one.
 * @base_address: Linear address of the first GDT descriptor.
 */
typedef struct {
    uint16_t limit;         // Size of all GDT segments.
    uint32_t base_address;  // Base address of the first GDT segment.
} __attribute__((packed)) GDT_PTR;

/**
 * load_gdt() - Load the GDT via the LGDT assembly instruction.
 * @gdt_ptr: Address of a packed GDT_PTR structure.
 *
 * Context: Called once during early boot, before the protected-mode segment
 *          selectors are set up.
 */
extern "C" void load_gdt(uint32_t gdt_ptr);

/**
 * gdt_set_entry() - Program one segment descriptor in the GDT.
 * @index: Descriptor index (0..NO_GDT_DESCRIPTORS - 1).
 * @base: Segment base address.
 * @limit: Segment limit (up to 20 bits).
 * @access: Access byte describing the segment.
 * @gran: Granularity / flags byte.
 */
void gdt_set_entry(int index, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran);

/**
 * gdt_init() - Build and load the flat 32-bit GDT for the kernel.
 *
 * Lays out the null, kernel code/data and user code/data segments plus the
 * TSS descriptor, then loads the table with load_gdt().
 */
void gdt_init();

#endif  // GDT_H
