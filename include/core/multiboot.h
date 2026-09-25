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

#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

/**
 * struct MultibootInfo - The Multiboot 1 information structure passed by the bootloader.
 * @flags: Bitmask marking which of the following fields are valid.
 * @mem_lower: Amount of lower memory, in kilobytes.
 * @mem_upper: Amount of upper memory, in kilobytes.
 * @boot_device: Boot device identifier.
 * @cmdline: Physical address of the kernel command line string.
 * @mods_count: Number of boot modules loaded.
 * @mods_addr: Physical address of the boot module array.
 * @syms[4]: Symbol table references (a.out or ELF format).
 * @mmap_length: Size of the memory map, in bytes.
 * @mmap_addr: Physical address of the memory map.
 * @drives_length: Size of the drives array, in bytes.
 * @drives_addr: Physical address of the drives array.
 * @config_table: Physical address of the ROM configuration table.
 * @boot_loader_name: Physical address of the boot loader name string.
 * @apm_table: Physical address of the APM table.
 * @vbe_control_info: Physical address of the VBE control information.
 * @vbe_mode_info: Physical address of the VBE mode information.
 * @vbe_mode: Current VBE mode.
 * @vbe_interface_seg: VBE interface segment.
 * @vbe_interface_off: VBE interface offset.
 * @vbe_interface_len: Length of the VBE interface.
 * @framebuffer_addr: Physical address of the framebuffer.
 * @framebuffer_pitch: Framebuffer pitch, in bytes.
 * @framebuffer_width: Framebuffer width, in pixels.
 * @framebuffer_height: Framebuffer height, in pixels.
 * @framebuffer_bpp: Framebuffer bits per pixel.
 * @framebuffer_type: Framebuffer type.
 * @color_info[6]: Framebuffer palette/color information.
 */
struct MultibootInfo {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
    uint32_t apm_table;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t color_info[6];
};

/**
 * struct multiboot_module - A boot module loaded by the bootloader.
 * @mod_start: Start address of the module.
 * @mod_end: End address of the module.
 * @cmdline: Command line string address, if any.
 * @pad: Padding to keep the structure aligned.
 */
struct multiboot_module {
    uint32_t mod_start;  // Start address of the module.
    uint32_t mod_end;    // End address of the module.
    uint32_t cmdline;    // Command line string, if any.
    uint32_t pad;        // Padding.
};

/**
 * enum MULTIBOOT_MEMORY_TYPE - Type tag for a Multiboot memory map entry.
 * @MULTIBOOT_MEMORY_AVAILABLE: Usable RAM.
 * @MULTIBOOT_MEMORY_RESERVED: Reserved memory.
 * @MULTIBOOT_MEMORY_ACPI_RECLAIMABLE: Reclaimable after ACPI tables are read.
 * @MULTIBOOT_MEMORY_NVS: Non-volatile memory used by the system.
 * @MULTIBOOT_MEMORY_BADRAM: Defective RAM, do not use.
 */
typedef enum {
    MULTIBOOT_MEMORY_AVAILABLE = 1,
    MULTIBOOT_MEMORY_RESERVED,
    MULTIBOOT_MEMORY_ACPI_RECLAIMABLE,
    MULTIBOOT_MEMORY_NVS,
    MULTIBOOT_MEMORY_BADRAM
} MULTIBOOT_MEMORY_TYPE;

/**
 * struct MULTIBOOT_MEMORY_MAP - One entry of the Multiboot memory map.
 * @size: Size of this entry, in bytes.
 * @addr_low: Low 32 bits of the region base address.
 * @addr_high: High 32 bits of the region base address.
 * @len_low: Low 32 bits of the region length.
 * @len_high: High 32 bits of the region length.
 * @type: Region type (MULTIBOOT_MEMORY_TYPE).
 */
typedef struct {
    uint32_t size;
    uint32_t addr_low;
    uint32_t addr_high;
    uint32_t len_low;
    uint32_t len_high;
    MULTIBOOT_MEMORY_TYPE type;
} MULTIBOOT_MEMORY_MAP;

#endif  // MULTIBOOT_H
