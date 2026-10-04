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

#include <core/memory.h>
#include <gui/fonts/font.h>
#include <gui/renderer/nina.h>
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
 * @width: Screen width in pixels.
 * @height: Screen height in pixels.
 * @bpp: Bits per pixel.
 * @nina: Software renderer used to composite the scene.
 * @videoMemory: Hardware framebuffer.
 * @backBuffer: Double-buffer scratch buffer.
 * @alphaTable: Precomputed LUT for alpha blending.
 */
class GraphicsDriver {
protected:
    uint32_t width;
    uint32_t height;
    uint32_t bpp;
    NINA nina;

    // Hardware video memory.
    uint32_t* videoMemory;

    // Back buffer for double buffering.
    uint32_t* backBuffer;

    // Lookup table for alpha blending.
    uint8_t alphaTable[256][256];

    void PrecomputeAlphaTable();

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
     * undefined image, and updates width/height for the drawing primitives.
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
    uint32_t GetWidth() {
        return width;
    }
    uint32_t GetHeight() {
        return height;
    }
    uint32_t* GetVideoMemory() {
        return videoMemory;
    }
    uint32_t* GetBackBuffer() {
        return backBuffer;
    }

    // Drawing primitives.
    virtual void PutPixel(int32_t x, int32_t y, uint32_t color);
    void PutPixel(int32_t x, int32_t y, uint8_t a, uint8_t r, uint8_t g, uint8_t b);

    // Shapes.
    void FillRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color);
    void DrawRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t color);

    void FillCircle(int32_t cx, int32_t cy, uint32_t r, uint32_t color);
    void DrawCircle(int32_t cx, int32_t cy, uint32_t r, uint32_t color);

    void FillRoundedRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t r,
                              uint32_t color);
    void DrawRoundedRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t r,
                              uint32_t color);

    void DrawRoundedRectangleShadow(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t size,
                                    uint32_t r, uint32_t color);
    void BlurRoundedRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t r,
                              uint32_t blur);

    void DrawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
    void DrawHorizontalLine(int32_t x, int32_t y, int32_t length, uint32_t colorIndex);
    void DrawVerticalLine(int32_t x, int32_t y, int32_t length, uint32_t colorIndex);

    // Text and bitmaps.
    void DrawBitmap(int32_t x, int32_t y, const uint32_t* data, int32_t w, int32_t h);
    void DrawCharacter(int32_t x, int32_t y, char c, Font* font, uint32_t color);
    void DrawString(int32_t x, int32_t y, const char* str, Font* font, uint32_t color);

    // Calculate the X/Y coordinates to center an object of size w/h on screen.
    void GetScreenCenter(uint32_t w, uint32_t h, int32_t& x, int32_t& y);
};

#endif
