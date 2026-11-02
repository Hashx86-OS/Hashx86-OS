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

#ifndef RASTER_H
#define RASTER_H

#include <gui/render/nina.h>
#include <types.h>

/**
 * class RasterSurface - A software 32bpp (0xAARRGGBB) pixel surface.
 *
 * Owns a back buffer and the drawing primitives that operate on it: pixels,
 * rectangles, rounded rectangles, circles, lines, bitmaps and text. It has no
 * hardware knowledge at all — the kernel GraphicsDriver derives from it and
 * adds video memory, flushing and mode switching; host tests instantiate it
 * directly and assert on the pixels.
 *
 * All coordinates are surface-relative and clipped to the surface bounds.
 * Rectangle sizes are signed: a non-positive width or height draws nothing,
 * so `w - 2` on a two-pixel-wide widget is harmless instead of wrapping.
 */
class RasterSurface {
protected:
    uint32_t width;
    uint32_t height;
    uint32_t* backBuffer;
    NINA nina;
    uint8_t alphaTable[256][256];

    /** RasterSurface() - Deferred-initialization constructor for subclasses. */
    RasterSurface();

    /** PrecomputeAlphaTable() - Build the 8-bit alpha blend LUT. */
    void PrecomputeAlphaTable();

    /**
     * Resize() - Reallocate the back buffer for a new geometry.
     * @w: New width in pixels.
     * @h: New height in pixels.
     *
     * Carries over the overlapping top-left region and clears the rest. The old
     * buffer is freed only after the replacement exists, so a failed allocation
     * leaves the surface unchanged.
     *
     * Return: True on success, false on invalid geometry or allocation failure.
     */
    bool Resize(uint32_t w, uint32_t h);

public:
    /**
     * RasterSurface() - Construct a surface of the given geometry.
     * @w: Surface width in pixels.
     * @h: Surface height in pixels.
     */
    RasterSurface(uint32_t w, uint32_t h);

    virtual ~RasterSurface();

    // Getters.
    uint32_t GetWidth() const {
        return width;
    }
    uint32_t GetHeight() const {
        return height;
    }
    uint32_t* GetBackBuffer() {
        return backBuffer;
    }

    // Drawing primitives.
    virtual void PutPixel(int32_t x, int32_t y, uint32_t color);
    void PutPixel(int32_t x, int32_t y, uint8_t a, uint8_t r, uint8_t g, uint8_t b);

    void FillRectangle(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
    void DrawRectangle(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);

    void FillCircle(int32_t cx, int32_t cy, uint32_t r, uint32_t color);
    void DrawCircle(int32_t cx, int32_t cy, uint32_t r, uint32_t color);

    void FillRoundedRectangle(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t r,
                              uint32_t color);
    void DrawRoundedRectangle(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t r,
                              uint32_t color);
    void DrawRoundedRectangleShadow(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t size,
                                    uint32_t r, uint32_t color);
    void BlurRoundedRectangle(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t r,
                              uint32_t blur);

    void DrawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
    void DrawHorizontalLine(int32_t x, int32_t y, int32_t length, uint32_t colorIndex);
    void DrawVerticalLine(int32_t x, int32_t y, int32_t length, uint32_t colorIndex);

    void DrawBitmap(int32_t x, int32_t y, const uint32_t* data, int32_t w, int32_t h);
    void DrawCharacter(int32_t x, int32_t y, char c, Font* font, uint32_t color);
    void DrawString(int32_t x, int32_t y, const char* str, Font* font, uint32_t color);

    // Calculate the X/Y coordinates to center an object of size w/h on the surface.
    void GetScreenCenter(uint32_t w, uint32_t h, int32_t& x, int32_t& y);
};

#endif  // RASTER_H
