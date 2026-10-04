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

#ifndef VBE_H
#define VBE_H

#include <core/driver.h>
#include <core/drivers/GraphicsDriver.h>
#include <stdint.h>

/*
 * Low-memory buffers used by the real-mode int 0x10 shim (asm/realmode.asm).
 * They sit below 640 KiB and are only live during early boot, before the
 * PMM starts handing pages out.
 */
#define VBE_REALMODE_STUB_BASE 0x00004000u  // Mirrors STUB_BASE (asm).
#define VBE_CONTROLLER_BUFFER 0x00005000u   // 512-byte VBE controller info.
#define VBE_MODE_BUFFER 0x00005400u         // 256-byte VBE mode info.

/**
 * typedef BIOSRegisters - Register block exchanged with the real-mode shim.
 * @eax/ebx/ecx/edx: 32-bit general-purpose BIOS call registers.
 * @edi/esi: Pointer/interrupt registers (ES:DI is used for VBE buffers).
 * @es/ds/fs/gs: Real-mode segment registers passed to int 0x10.
 *
 * The order must match the IOM_IN_* layout in asm/realmode.asm; it is copied
 * verbatim by bios_int10().
 */
typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t edi, esi;
    uint16_t es, ds, fs, gs;
} __attribute__((packed)) BIOSRegisters;

/**
 * bios_int10() - Run one real-mode BIOS interrupt 0x10 call from protected mode.
 * @in:  Register values loaded before the interrupt.
 * @out: Receives the registers returned by the BIOS.
 *
 * Implemented in asm/realmode.asm and linked straight into the kernel. Disables
 * interrupts and only clears CR0.PE, so it is also safe once paging is running:
 * everything the shim touches lives below 1 MiB, which Paging identity-maps.
 */
extern "C" void bios_int10(BIOSRegisters* in, BIOSRegisters* out);

/**
 * class VESA_BIOS_Extensions - Built-in VBE graphics driver.
 *
 * The bootstrap display driver. Constructed before paging and before the
 * DriverManager exists, it drops to real mode to ask the VGA BIOS for a 32-bpp
 * linear framebuffer, so the kernel has something to draw the boot splash on.
 *
 * Unlike the built-in mouse and keyboard drivers, which stay registered and are
 * merely activated, this is retired and freed once an external driver takes over
 * the panel - it exists only to stand in. See init_pci() and the release after
 * the splash exits.
 */
class VESA_BIOS_Extensions : public GraphicsDriver, public Driver {
private:
    // Modes confirmed by the BIOS scan, used by GetSupportedModes().
    static const int MAX_KNOWN_MODES = 64;
    DisplayMode knownModes[MAX_KNOWN_MODES];
    int knownModeCount;

    /**
     * probeBios() - Find and activate the best 32-bpp linear-framebuffer mode.
     *
     * Scans the VBE mode list, recording every usable geometry so
     * GetSupportedModes() can report it, falls back to a fixed list of popular
     * mode numbers when the list is unreadable, then switches to the best
     * candidate and fills in this->width/height/bpp/videoMemory.
     *
     * Return: True when a usable mode is active.
     *
     * Context: Must run before paging is enabled - real mode cannot operate
     * while CR0.PG is set.
     */
    bool probeBios();

public:
    /** The active bootstrap driver, or NULL once an external driver replaced it. */
    static VESA_BIOS_Extensions* activeInstance;

    /**
     * VESA_BIOS_Extensions() - Probe the VGA BIOS and take over the display.
     *
     * On failure the driver is left unusable rather than half-configured:
     * GetVideoMemory() stays NULL and the dimensions stay zero, so callers must
     * check it before drawing.
     */
    VESA_BIOS_Extensions();

    /**
     * ~VESA_BIOS_Extensions() - Destroy the driver.
     */
    ~VESA_BIOS_Extensions();

    /**
     * SetVideoMode() - Ask the VGA BIOS for a different VBE mode.
     * @w: Requested width in pixels.
     * @h: Requested height in pixels.
     *
     * Programs the mode through the real-mode int 0x10 shim, re-maps the linear
     * framebuffer when the BIOS moved it, and resizes the back buffer. The
     * previous mode is restored if the BIOS refuses the request.
     *
     * Return: True when the new mode is active.
     */
    bool SetVideoMode(uint32_t w, uint32_t h) override;

    /**
     * GetSupportedModes() - Report the 32-bpp LFB modes the BIOS advertised.
     * @count: Receives the number of entries.
     *
     * Return: The scan result, or NULL when the scan found nothing.
     */
    const DisplayMode* GetSupportedModes(int* count) override;
};

#endif
