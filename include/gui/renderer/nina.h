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

#ifndef NINA_H
#define NINA_H

#include <core/memory.h>
#include <gui/fonts/font.h>
#include <gui/icons.h>
#include <types.h>

/**
 * class NINA - Software rasterizer used by the graphics driver.
 *
 * Draws sprites, shapes, lines, and text into a 0xAARRGGBB pixel buffer.
 * All drawing operations take an explicit destination buffer and bounds; the
 * alpha table is precomputed once for per-pixel compositing.
 */
class NINA {
protected:
    uint8_t alphaTable[256][256];

    /** PrecomputeAlphaTable() - Build the 256x256 alpha compositing table. */
    void PrecomputeAlphaTable();

public:
    /**
     * NINA() - Construct the rasterizer and precompute the alpha table.
     */
    NINA();

    /**
     * ~NINA() - Destroy the rasterizer.
     */
    ~NINA();

    static NINA* activeInstance;

    /**
     * DrawBitmapToBuffer() - Composited bitmap blit with per-pixel alpha.
     * @dst: Destination buffer.
     * @dstW: Destination buffer width.
     * @dstH: Destination buffer height.
     * @dstX: Destination X offset.
     * @dstY: Destination Y offset.
     * @src: Source pixel buffer.
     * @srcW: Source buffer width.
     * @srcH: Source buffer height.
     */
    void DrawBitmapToBuffer(uint32_t* dst, int dstW, int dstH, int dstX, int dstY, uint32_t* src,
                            int srcW, int srcH);

    /**
     * DrawBitmap() - Copy a bitmap into a buffer at an offset.
     * @buffer: Destination buffer.
     * @bufferWidth: Destination buffer width.
     * @bufferHeight: Destination buffer height.
     * @x: Destination X offset.
     * @y: Destination Y offset.
     * @bitmapData: Source pixel buffer.
     * @bitmapWidth: Source width.
     * @bitmapHeight: Source height.
     */
    void DrawBitmap(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x,
                    int32_t y, const uint32_t* bitmapData, int32_t bitmapWidth,
                    int32_t bitmapHeight);

    /** FillRectangle() - Fill a clipped rectangle with a color. */
    void FillRectangle(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x,
                       int32_t y, uint32_t w, uint32_t h, uint32_t colorIndex);

    /** DrawRectangle() - Outline a clipped rectangle with a color. */
    void DrawRectangle(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x,
                       int32_t y, uint32_t w, uint32_t h, uint32_t colorIndex);

    /** FillRoundedRectangle() - Fill a clipped rectangle with rounded corners. */
    void FillRoundedRectangle(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight,
                              int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t radius,
                              uint32_t colorIndex);

    /** DrawRoundedRectangle() - Outline a clipped rectangle with rounded corners. */
    void DrawRoundedRectangle(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight,
                              int32_t x, int32_t y, uint32_t w, uint32_t h, uint32_t radius,
                              uint32_t colorIndex);

    /** FillCircle() - Fill a clipped circle with a color. */
    void FillCircle(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t cx,
                    int32_t cy, uint32_t radius, uint32_t colorIndex);

    /** DrawCircle() - Outline a clipped circle with a color. */
    void DrawCircle(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t cx,
                    int32_t cy, uint32_t radius, uint32_t colorIndex);

    /** DrawLine() - Rasterize a line between two endpoints. */
    void DrawLine(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x0,
                  int32_t y0, int32_t x1, int32_t y1, uint32_t color);

    /** DrawHorizontalLine() - Rasterize a horizontal line segment. */
    void DrawHorizontalLine(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x,
                            int32_t y, int32_t length, uint32_t colorIndex);

    /** DrawVerticalLine() - Rasterize a vertical line segment. */
    void DrawVerticalLine(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x,
                          int32_t y, int32_t length, uint32_t colorIndex);

    /** DrawCharacter() - Blit a glyph from a font at a position. */
    void DrawCharacter(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x,
                       int32_t y, uint32_t codepoint, Font* font, uint32_t colorIndex);

    /** DrawString() - Blit a NUL-terminated string with a font. */
    void DrawString(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x,
                    int32_t y, const char* str, Font* font, uint32_t colorIndex);
};

#endif  // NINA_H
