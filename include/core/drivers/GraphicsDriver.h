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

public:
    GraphicsDriver(uint32_t w, uint32_t h, uint32_t bpp, uint32_t* vram);
    virtual ~GraphicsDriver();

    // Hardware interface.
    virtual void Flush();

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
