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

#ifndef KERNEL_H
#define KERNEL_H

#include <audio/wav.h>
#include <core/KernelSymbolResolver.h>
#include <core/driver.h>
#include <core/drivers/AudioDriver.h>
#include <core/drivers/AudioMixer.h>
#include <core/drivers/GraphicsDriver.h>
#include <core/drivers/ata.h>
#include <core/drivers/keyboard.h>
#include <core/drivers/mouse.h>
#include <core/drivers/vbe.h>
#include <core/drivers/vga.h>
#include <core/elf.h>
#include <core/filesystem/msdospart.h>
#include <core/gdt.h>
#include <core/globals.h>
#include <core/interrupts.h>
#include <core/memory.h>
#include <core/multiboot.h>
#include <core/paging.h>
#include <core/pci.h>
#include <core/pmm.h>
#include <core/scheduler.h>
#include <core/syscalls.h>
#include <core/timing.h>
#include <core/tss.h>
#include <debug.h>
#include <gui/Hgui.h>
#include <gui/bmp.h>
#include <gui/fonts/vga.h>
#include <gui/gui.h>
#include <gui/renderer/nina.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdlib/fdlibm.h>

// Symbols from linker.ld giving the kernel section addresses.
extern uint8_t __kernel_section_start;
extern uint8_t __kernel_section_end;
extern uint8_t __kernel_text_section_start;
extern uint8_t __kernel_text_section_end;
extern uint8_t __kernel_data_section_start;
extern uint8_t __kernel_data_section_end;
extern uint8_t __kernel_rodata_section_start;
extern uint8_t __kernel_rodata_section_end;
extern uint8_t __kernel_bss_section_start;
extern uint8_t __kernel_bss_section_end;

/**
 * struct KERNEL_MEMORY_MAP - Kernel memory layout derived from multiboot.
 *
 * Records the kernel section spans, the total system memory and the region
 * available for runtime allocations.
 */
typedef struct {
    struct {
        uint32_t k_start_addr;
        uint32_t k_end_addr;
        uint32_t k_len;
        uint32_t text_start_addr;
        uint32_t text_end_addr;
        uint32_t text_len;
        uint32_t data_start_addr;
        uint32_t data_end_addr;
        uint32_t data_len;
        uint32_t rodata_start_addr;
        uint32_t rodata_end_addr;
        uint32_t rodata_len;
        uint32_t bss_start_addr;
        uint32_t bss_end_addr;
        uint32_t bss_len;
    } kernel;

    struct {
        uint32_t total_memory;
    } system;

    struct {
        uint32_t start_addr;
        uint32_t end_addr;
        uint32_t size;
    } available;
} KERNEL_MEMORY_MAP;

extern KERNEL_MEMORY_MAP g_kmap;

/**
 * get_kernel_memory_map() - Populate the kernel memory layout from multiboot.
 * @kmap: KERNEL_MEMORY_MAP structure to fill in.
 * @mboot_info: Multiboot info structure carrying the memory map (bit 6).
 *
 * Return: 0 on success, -1 when the layout could not be determined.
 */
int get_kernel_memory_map(KERNEL_MEMORY_MAP* kmap, MultibootInfo* mboot_info);
/**
 * display_kernel_memory_map() - Log every section of the kernel memory map.
 * @kmap: Kernel memory map to display.
 */
void display_kernel_memory_map(KERNEL_MEMORY_MAP* kmap);

/**
 * class ConsoleKeyboardEventHandler - Handle keyboard events for the console.
 */
class ConsoleKeyboardEventHandler : public KeyboardEventHandler {
    /**
     * OnKeyDown() - Handle a normal key press.
     * @key: ASCII representation of the pressed key.
     */
    void OnKeyDown(const char* key);

    /**
     * OnKeyUp() - Handle a normal key release.
     * @key: ASCII representation of the released key.
     */
    void OnKeyUp(const char* key);

    /**
     * OnSpecialKeyDown() - Handle a special key press (function or arrow keys).
     * @key: Key code of the pressed special key.
     */
    void OnSpecialKeyDown(uint8_t key);

    /**
     * OnSpecialKeyUp() - Handle a special key release.
     * @key: Key code of the released special key.
     */
    void OnSpecialKeyUp(uint8_t key);
};

/**
 * class ConsoleMouseEventHandler - Handle mouse events for the console.
 */
class ConsoleMouseEventHandler : public MouseEventHandler {
public:
    /**
     * OnMouseMove() - Handle mouse pointer movement.
     * @x: New x-coordinate of the mouse pointer.
     * @y: New y-coordinate of the mouse pointer.
     */
    virtual void OnMouseMove(int x, int y);

    /**
     * OnLeftMouseDown() - Handle a left mouse button press.
     * @x: x-coordinate of the mouse pointer at the event.
     * @y: y-coordinate of the mouse pointer at the event.
     */
    virtual void OnLeftMouseDown(int x, int y);

    /**
     * OnLeftMouseUp() - Handle a left mouse button release.
     * @x: x-coordinate of the mouse pointer at the event.
     * @y: y-coordinate of the mouse pointer at the event.
     */
    virtual void OnLeftMouseUp(int x, int y);

    /**
     * OnRightMouseDown() - Handle a right mouse button press.
     * @x: x-coordinate of the mouse pointer at the event.
     * @y: y-coordinate of the mouse pointer at the event.
     */
    virtual void OnRightMouseDown(int x, int y);

    /**
     * OnRightMouseUp() - Handle a right mouse button release.
     * @x: x-coordinate of the mouse pointer at the event.
     * @y: y-coordinate of the mouse pointer at the event.
     */
    virtual void OnRightMouseUp(int x, int y);

private:
    int X = 80;  // Previous x-coordinate of the mouse pointer.
    int Y = 24;  // Previous y-coordinate of the mouse pointer.
};

#endif  // KERNEL_H
