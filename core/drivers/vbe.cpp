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

#define KDBG_COMPONENT "CORE:VBE"
#include <core/drivers/vbe.h>
#include <core/globals.h>
#include <core/paging.h>
#include <debug.h>

VESA_BIOS_Extensions* VESA_BIOS_Extensions::activeInstance = nullptr;

#define VBE_OK 0x004F
#define VBE_FN_CONTROLLER_INFO 0x4F00
#define VBE_FN_MODE_INFO 0x4F01
#define VBE_FN_SET_MODE 0x4F02

// Preferred (width, height) targets. The scan takes the first one the card
// actually lists; a 32-bpp LFB mode is required for each.
static const struct {
    uint16_t width;
    uint16_t height;
} kPreferredModes[] = {
    {1024, 768}, {800, 600}, {1280, 1024}, {1280, 720}, {1366, 768}, {1440, 900}, {1600, 900},
};

// Fallback mode numbers (VBE 1.2+ direct-color block), tried if the mode
// list cannot be read or contains no usable entry.
static const uint16_t kFallbackModes[] = {0x118, 0x11C, 0x120, 0x114, 0x10F};

// Provided by asm/realmode.asm: the protected-to-real-mode int 0x10 shim this
// driver copies into low memory. Declared at file scope so they keep external
// linkage and resolve against the linked-in assembly.
extern const unsigned char realmode_stub_start[];
extern const unsigned char realmode_stub_end[];

namespace {

/**
 * vbe_read_u8/u16/u32() - Byte-byte accessors for a low-memory VBE buffer.
 * VBE writes the structures in real mode; read them back through volatile
 * byte loads so the compiler cannot reorder or cache them.
 */
inline uint8_t vbe_read_u8(uint32_t buf, uint32_t off) {
    return *(volatile uint8_t*)(buf + off);
}

inline uint16_t vbe_read_u16(uint32_t buf, uint32_t off) {
    return (uint16_t)vbe_read_u8(buf, off) | ((uint16_t)vbe_read_u8(buf, off + 1) << 8);
}

inline uint32_t vbe_read_u32(uint32_t buf, uint32_t off) {
    return (uint32_t)vbe_read_u16(buf, off) | ((uint32_t)vbe_read_u16(buf, off + 2) << 16);
}

/**
 * vbe_copy_stub() - Copy the int 0x10 shim into low memory (idempotent).
 * Ensures the block this driver depends on is present.
 */
void vbe_copy_stub() {
    const unsigned char* src = realmode_stub_start;
    volatile unsigned char* dst = (volatile unsigned char*)VBE_REALMODE_STUB_BASE;
    unsigned int count = (unsigned int)(realmode_stub_end - realmode_stub_start);
    for (unsigned int i = 0; i < count; i++) {
        dst[i] = src[i];
    }
}

/**
 * vbe_get_mode_info() - Ask the VGA BIOS for a mode's info block.
 * @mode: VBE mode number.
 *
 * Return: 0 when the BIOS reported success.
 */
int vbe_get_mode_info(uint16_t mode) {
    BIOSRegisters regs = {};
    regs.eax = VBE_FN_MODE_INFO;
    regs.ecx = mode;
    regs.es = 0;
    regs.edi = VBE_MODE_BUFFER;
    bios_int10(&regs, &regs);
    return ((uint16_t)regs.eax == VBE_OK) ? 0 : -1;
}

/**
 * vbe_mode_is_lfb_32bpp() - true when the buffer in memory describes a usable
 * 32-bpp mode with a linear framebuffer and valid geometry.
 */
int vbe_mode_is_lfb_32bpp(uint16_t* w, uint16_t* h) {
    uint16_t attrs = vbe_read_u16(VBE_MODE_BUFFER, 0x00);
    if (!(attrs & 0x0080)) {  // Linear framebuffer supported.
        return 0;
    }

    *w = vbe_read_u16(VBE_MODE_BUFFER, 0x12);
    *h = vbe_read_u16(VBE_MODE_BUFFER, 0x14);
    uint8_t bpp = vbe_read_u8(VBE_MODE_BUFFER, 0x19);
    if (bpp == 0 || *w == 0 || *h == 0) {
        return 0;
    }
    if (bpp != 32) {
        return 0;
    }

    // The driver addresses the back buffer as packed uint32_t pixels, so it
    // needs the scanline pitch to be exactly width * 4. A 32-bpp mode with a
    // wider pitch has padding the render path does not know about, and one with
    // a narrower pitch is not 4 bytes per pixel at all. Reject both rather than
    // shear the image.
    if ((uint32_t)(*w) * 4u != (uint32_t)vbe_read_u16(VBE_MODE_BUFFER, 0x10)) {
        return 0;
    }

    return 1;
}

/**
 * vbe_set_mode() - Switch the graphics hardware to @mode using the LFB flag.
 */
int vbe_set_mode(uint16_t mode) {
    BIOSRegisters regs = {};
    regs.eax = VBE_FN_SET_MODE;
    regs.ebx = mode | 0x4000;  // Request the linear-framebuffer variant.
    regs.es = 0;
    regs.edi = 0;
    bios_int10(&regs, &regs);
    return ((uint16_t)regs.eax == VBE_OK) ? 0 : -1;
}

/**
 * vbe_mode_rank() - Rank a mode against the preference table.
 * Exact matches beat larger fallbacks.
 */
int vbe_mode_rank(uint16_t w, uint16_t h) {
    int pref_count = (int)(sizeof(kPreferredModes) / sizeof(kPreferredModes[0]));
    for (int i = 0; i < pref_count; i++) {
        if (kPreferredModes[i].width == w && kPreferredModes[i].height == h) {
            return i;
        }
    }
    return -1;
}

/**
 * vbe_get_mode_list() - Read the controller info and return the mode list.
 *
 * The mode list is a far pointer into the VGA BIOS ROM terminated by 0xFFFF;
 * this converts it to a flat address. Performs the 'VESA' signature check.
 *
 * Return: The flat mode-list address, or 0 when there is no usable VBE
 * controller.
 */
uint32_t vbe_get_mode_list() {
    BIOSRegisters regs = {};
    regs.eax = VBE_FN_CONTROLLER_INFO;
    regs.es = 0;
    regs.edi = VBE_CONTROLLER_BUFFER;
    bios_int10(&regs, &regs);
    if ((uint16_t)regs.eax != VBE_OK) {
        KDBG1("No VBE controller (ax=0x%x)", (uint16_t)regs.eax);
        return 0;
    }
    if (vbe_read_u8(VBE_CONTROLLER_BUFFER, 0x00) != 'V' ||
        vbe_read_u8(VBE_CONTROLLER_BUFFER, 0x01) != 'E' ||
        vbe_read_u8(VBE_CONTROLLER_BUFFER, 0x02) != 'S' ||
        vbe_read_u8(VBE_CONTROLLER_BUFFER, 0x03) != 'A') {
        KDBG1("Bad VBE signature");
        return 0;
    }

    // Real-mode seg:offset -> flat linear address.
    uint32_t list_far = vbe_read_u32(VBE_CONTROLLER_BUFFER, 0x0E);
    return ((list_far >> 16) & 0xFFFF) * 16 + (list_far & 0xFFFF);
}

/**
 * vbe_find_mode() - Locate a 32-bpp LFB mode with exactly this geometry.
 * @width/@height: Requested geometry.
 * @out_lfb: Receives the mode's linear framebuffer physical address.
 *
 * Scans the VBE mode list. Return: The VBE mode number, or 0xFFFF when the BIOS
 * lists no usable match.
 */
uint16_t vbe_find_mode(uint16_t width, uint16_t height, uint32_t* out_lfb) {
    if (width == 0 || height == 0) {
        return 0xFFFF;
    }

    vbe_copy_stub();

    uint32_t list = vbe_get_mode_list();
    if (list == 0) {
        return 0xFFFF;
    }

    uint32_t cursor = list;
    for (int guard = 0; guard < 512; guard++) {
        uint16_t entry = vbe_read_u16(cursor, 0);
        if (entry == 0xFFFF) {
            break;
        }
        cursor += 2;

        if (entry & 0x8000) {  // Text/legacy mode; not usable for us.
            continue;
        }
        uint16_t mode = entry & 0x7FFF;

        if (vbe_get_mode_info(mode) != 0) {
            continue;
        }
        uint16_t w, h;
        if (!vbe_mode_is_lfb_32bpp(&w, &h)) {
            continue;
        }
        if (w == width && h == height) {
            if (out_lfb) {
                *out_lfb = vbe_read_u32(VBE_MODE_BUFFER, 0x28);
            }
            return mode;
        }
    }

    KDBG1("No 32-bpp LFB mode for %ux%u", width, height);
    return 0xFFFF;
}

/**
 * vbe_activate_mode() - Switch to @mode and read back its description.
 * @mode: VBE mode number.
 * @out_w/@out_h: Receive the geometry the BIOS activated.
 * @out_lfb: Receives the LFB physical base.
 *
 * Return: 0 on success, negative on failure.
 */
int vbe_activate_mode(uint16_t mode, uint16_t* out_w, uint16_t* out_h, uint32_t* out_lfb) {
    if (vbe_set_mode(mode) != 0) {
        KDBG1("set mode 0x%x failed", mode);
        return -1;
    }

    // Re-read the mode info so the reported LFB address and pitch belong to the
    // mode that is actually active.
    if (vbe_get_mode_info(mode) != 0) {
        KDBG1("re-read mode 0x%x failed", mode);
        return -1;
    }

    uint32_t lfb = vbe_read_u32(VBE_MODE_BUFFER, 0x28);
    if (lfb == 0) {
        KDBG1("mode 0x%x has no linear framebuffer", mode);
        return -1;
    }

    *out_w = vbe_read_u16(VBE_MODE_BUFFER, 0x12);
    *out_h = vbe_read_u16(VBE_MODE_BUFFER, 0x14);
    *out_lfb = lfb;
    return 0;
}

}  // namespace

/**
 * probeBios() - Find and activate the best 32-bpp linear-framebuffer mode.
 *
 * Scans the VBE mode list, recording every usable geometry it finds so
 * GetSupportedModes() can report the truth, and picks the best one against the
 * preference table. Falls back to a fixed list of popular mode numbers when the
 * list is unreadable, then switches to the winner and fills in
 * this->width/height/bpp/videoMemory.
 *
 * Return: True when a usable mode is active.
 *
 * Context: Must run before paging is enabled - real mode cannot operate while
 * CR0.PG is set.
 */
bool VESA_BIOS_Extensions::probeBios() {
    vbe_copy_stub();

    knownModeCount = 0;

    uint32_t list = vbe_get_mode_list();
    if (list == 0) {
        return false;
    }
    uint16_t vbe_version = vbe_read_u16(VBE_CONTROLLER_BUFFER, 0x04);

    uint16_t chosen_mode = 0xFFFF;
    int chosen_rank = -1;
    uint32_t chosen_area = 0;

    uint32_t cursor = list;
    for (int guard = 0; guard < 512; guard++) {
        uint16_t entry = vbe_read_u16(cursor, 0);
        if (entry == 0xFFFF) {
            break;
        }
        cursor += 2;

        if (entry & 0x8000) {  // Text/legacy mode; not usable for us.
            continue;
        }
        uint16_t mode = entry & 0x7FFF;

        if (vbe_get_mode_info(mode) != 0) {
            continue;
        }
        uint16_t w, h;
        if (!vbe_mode_is_lfb_32bpp(&w, &h)) {
            continue;
        }

        // Record it as a supported mode, skipping geometries already listed.
        bool dup = false;
        for (int i = 0; i < knownModeCount; i++) {
            if (knownModes[i].width == w && knownModes[i].height == h) {
                dup = true;
                break;
            }
        }
        if (!dup && knownModeCount < MAX_KNOWN_MODES) {
            knownModes[knownModeCount].width = w;
            knownModes[knownModeCount].height = h;
            knownModeCount++;
        }

        int rank = vbe_mode_rank(w, h);
        uint32_t area = (uint32_t)w * (uint32_t)h;
        if (chosen_mode == 0xFFFF || (rank >= 0 && (chosen_rank < 0 || rank < chosen_rank)) ||
            (rank < 0 && chosen_rank < 0 && area > chosen_area)) {
            chosen_mode = mode;
            chosen_rank = rank;
            chosen_area = area;
        }
    }

    // ---- Mode-list fallback: try a fixed list of popular 32-bpp modes -----
    if (chosen_mode == 0xFFFF) {
        int fallback_count = (int)(sizeof(kFallbackModes) / sizeof(kFallbackModes[0]));
        for (int i = 0; i < fallback_count; i++) {
            uint16_t mode = kFallbackModes[i];
            if (vbe_get_mode_info(mode) != 0) {
                continue;
            }
            uint16_t w, h;
            if (vbe_mode_is_lfb_32bpp(&w, &h)) {
                chosen_mode = mode;
                if (knownModeCount < MAX_KNOWN_MODES) {
                    knownModes[knownModeCount].width = w;
                    knownModes[knownModeCount].height = h;
                    knownModeCount++;
                }
                break;
            }
        }
    }

    if (chosen_mode == 0xFFFF) {
        KDBG1("No usable 32-bpp LFB mode found");
        return false;
    }

    uint16_t w = 0, h = 0;
    uint32_t lfb = 0;
    if (vbe_activate_mode(chosen_mode, &w, &h, &lfb) != 0) {
        return false;
    }

    this->width = w;
    this->height = h;
    this->bpp = vbe_read_u8(VBE_MODE_BUFFER, 0x19);
    this->videoMemory = (uint32_t*)lfb;

    KDBG1("VBE %x.%x mode 0x%x: %ux%u %u-bpp LFB 0x%x, %d modes known", vbe_version >> 8,
          vbe_version & 0xFF, chosen_mode, w, h, this->bpp, lfb, knownModeCount);
    return true;
}

/**
 * VESA_BIOS_Extensions() - Probe the VGA BIOS and take over the display.
 *
 * The BIOS probe has to happen before paging is enabled, because real mode
 * cannot run while CR0.PG is set. The GraphicsDriver base is entered with the
 * probed geometry, which allocates and clears the software back buffer.
 */
VESA_BIOS_Extensions::VESA_BIOS_Extensions() : GraphicsDriver(), knownModeCount(0) {
    this->SetName("VESA BIOS Extensions");

    // The base is entered unconfigured because the geometry only exists once the
    // BIOS has been asked for it, which needs the base object to already exist.
    if (!probeBios()) {
        // Leave the driver unusable rather than half-configured; the caller
        // checks GetVideoMemory() and halts if no display came up.
        this->videoMemory = nullptr;
        this->backBuffer = nullptr;
        this->width = 0;
        this->height = 0;
        KDBG1("VBE self-initialization failed");
        return;
    }

    // probeBios() filled in the probed geometry and framebuffer; hand them to the
    // base now so it can allocate and clear the back buffer.
    InitialiseFramebuffer(this->width, this->height, this->bpp, this->videoMemory);

    activeInstance = this;
}

VESA_BIOS_Extensions::~VESA_BIOS_Extensions() {
    if (activeInstance == this) {
        activeInstance = nullptr;
    }
}

const DisplayMode* VESA_BIOS_Extensions::GetSupportedModes(int* count) {
    if (count) {
        *count = knownModeCount;
    }
    return knownModeCount > 0 ? knownModes : nullptr;
}

/**
 * SetVideoMode() - Ask the VGA BIOS for a different VBE mode.
 * @w: Requested width in pixels.
 * @h: Requested height in pixels.
 *
 * The BIOS may report a new LFB base for the mode, so the framebuffer range is
 * re-mapped before the cached pointer is updated. Any failure after the BIOS has
 * switched modes re-applies the previous mode, so a failed mode switch always
 * leaves the driver on the geometry and framebuffer it already had.
 *
 * Return: True when the new mode is active.
 */
bool VESA_BIOS_Extensions::SetVideoMode(uint32_t w, uint32_t h) {
    if (w == 0 || h == 0 || w > 0xFFFF || h > 0xFFFF) return false;
    if (w == this->width && h == this->height) return true;

    // Re-mapping the new framebuffer below needs paging to be up. Checked up
    // front so a driver constructed before paging refuses cleanly instead of
    // dereferencing a null page directory.
    if (!g_paging || !g_paging->KernelPageDirectory) {
        KDBG1("paging unavailable, refusing mode switch");
        return false;
    }

    uint16_t oldWidth = (uint16_t)this->width;
    uint16_t oldHeight = (uint16_t)this->height;
    uint32_t* oldLfb = this->videoMemory;

    // Once the BIOS has switched, a later failure has to undo the switch to
    // keep the driver's geometry, back buffer, and cached LFB mutually
    // consistent. ResizeBackBuffer() only commits on success, so the back buffer
    // still matches oldWidth/oldHeight when rollback runs.
    auto rollback = [&](void) {
        uint16_t rbWidth = 0, rbHeight = 0;
        uint32_t rbLfb = 0;
        uint16_t mode = vbe_find_mode(oldWidth, oldHeight, &rbLfb);
        if (mode != 0xFFFF && vbe_activate_mode(mode, &rbWidth, &rbHeight, &rbLfb) == 0) {
            this->width = rbWidth;
            this->height = rbHeight;
            this->videoMemory = (rbLfb != 0) ? (uint32_t*)rbLfb : oldLfb;
            KDBG1("rolled back to %ux%u after failed switch to %ux%u", rbWidth, rbHeight, w, h);
        } else {
            KDBG1("CRITICAL - rollback to %ux%u failed, hardware left at %ux%u", oldWidth,
                  oldHeight, w, h);
        }
    };

    uint32_t newLfb = 0;
    uint16_t mode = vbe_find_mode((uint16_t)w, (uint16_t)h, &newLfb);
    if (mode == 0xFFFF) {
        KDBG1("BIOS refused %ux%u", w, h);
        return false;
    }

    uint16_t newWidth = 0, newHeight = 0;
    if (vbe_activate_mode(mode, &newWidth, &newHeight, &newLfb) != 0) {
        KDBG1("BIOS refused %ux%u", w, h);
        return false;
    }

    // Make sure the new framebuffer is mapped. Paging identity-maps both the
    // low 256 MB and 3-4 GB, which covers every LFB a VBE BIOS reports, so this
    // is normally already satisfied and the loop does no work.
    uint64_t fbSize = (uint64_t)newWidth * (uint64_t)newHeight * 4;
    uint32_t start = newLfb & ~(PAGE_SIZE - 1);
    uint64_t end = ((uint64_t)newLfb + fbSize + PAGE_SIZE - 1) & ~((uint64_t)PAGE_SIZE - 1);
    if (end > 0xFFFFFFFFu) {
        KDBG1("LFB range for %ux%u exceeds 4GB", newWidth, newHeight);
        rollback();
        return false;
    }

    for (uint64_t addr = start; addr < end; addr += PAGE_SIZE) {
        uint32_t existing = g_paging->GetPhysicalAddress(g_paging->KernelPageDirectory, addr);
        if (existing == 0xFFFFFFFF || existing != addr) {
            if (!g_paging->MapPage(g_paging->KernelPageDirectory, addr, addr,
                                   PAGE_PRESENT | PAGE_RW)) {
                KDBG1("failed to map LFB page 0x%x", addr);
                rollback();
                return false;
            }
        }
    }

    if (!ResizeBackBuffer(newWidth, newHeight)) {
        KDBG1("back buffer resize to %ux%u failed", newWidth, newHeight);
        rollback();
        return false;
    }

    this->width = newWidth;
    this->height = newHeight;
    this->videoMemory = (uint32_t*)newLfb;
    KDBG1("mode switch to %ux%u LFB 0x%x", newWidth, newHeight, newLfb);
    return true;
}
