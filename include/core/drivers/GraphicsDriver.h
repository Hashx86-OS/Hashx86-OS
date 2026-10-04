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

#ifndef GRAPHICS_DRIVER_H
#define GRAPHICS_DRIVER_H

#include <gui/fonts/font.h>
#include <gui/render/raster.h>
#include <types.h>

/**
 * struct DisplayMode - A framebuffer geometry a driver can program.
 * @width: Width in pixels.
 * @height: Height in pixels.
 */
struct DisplayMode {
    uint16_t width;
    uint16_t height;
};

/**
 * class GraphicsDriver - Framebuffer-based 2D graphics backend.
 *
 * Derives from RasterSurface, which owns the software back buffer and every
 * drawing primitive. GraphicsDriver adds the hardware side: video memory, the
 * flush to the panel, and runtime mode switching. Widget code only ever sees
 * the RasterSurface base, so it stays independent of any particular driver.
 */
class GraphicsDriver : public RasterSurface {
protected:
    uint32_t bpp;

    // Hardware video memory.
    uint32_t* videoMemory;

    /**
     * GraphicsDriver() - Deferred-initialization constructor.
     *
     * Leaves the driver unconfigured so a subclass can learn its geometry by
     * talking to hardware first. Unusable until InitialiseFramebuffer() runs.
     */
    GraphicsDriver();

    /**
     * InitialiseFramebuffer() - Adopt a geometry and allocate the back buffer.
     * @w: Screen width in pixels.
     * @h: Screen height in pixels.
     * @b: Bits per pixel.
     * @vram: Hardware framebuffer, or NULL to defer mapping.
     *
     * Allocates and clears the software back buffer and precomputes the alpha
     * table. HALTs on an invalid geometry or a failed allocation, because a
     * driver without a back buffer cannot draw.
     */
    void InitialiseFramebuffer(uint32_t w, uint32_t h, uint32_t b, uint32_t* vram);

    /**
     * ResizeBackBuffer() - Reallocate the scratch buffer for a new geometry.
     * @w: New width in pixels.
     * @h: New height in pixels.
     *
     * Preserves the overlapping top-left region so the change does not flash an
     * undefined image.
     *
     * Return: True on success, false on invalid geometry or failed allocation
     * (the old buffer is kept in that case).
     */
    bool ResizeBackBuffer(uint32_t w, uint32_t h);

public:
    GraphicsDriver(uint32_t w, uint32_t h, uint32_t bpp, uint32_t* vram);
    virtual ~GraphicsDriver();

    // Hardware interface.
    virtual void Flush();

    /**
     * SetVideoMode() - Switch the display to a new resolution at runtime.
     * @w: Requested width in pixels.
     * @h: Requested height in pixels.
     *
     * Programs the hardware, makes sure the linear framebuffer is mapped, and
     * resizes the back buffer. Purely virtual on purpose: a driver that omitted
     * it would otherwise inherit a software-only resize that reports success
     * while the panel never changes.
     *
     * Context: Called from the GUI syscall dispatcher with interrupts disabled.
     *
     * Return: True on success, false when the driver cannot honour the request.
     */
    virtual bool SetVideoMode(uint32_t w, uint32_t h) = 0;

    /**
     * GetSupportedModes() - Report the geometries this driver can program.
     * @count: Receives the number of entries.
     *
     * The single source of truth for what the Settings app may offer. A driver
     * lists only modes it has confirmed it can activate, so a request is never
     * advertised and then rejected.
     *
     * Return: A DisplayMode array valid for the lifetime of the driver, or NULL
     * when the driver supports no fixed list.
     */
    virtual const DisplayMode* GetSupportedModes(int* count) = 0;

    // Getters.
    uint32_t* GetVideoMemory() {
        return videoMemory;
    }
};

#endif
