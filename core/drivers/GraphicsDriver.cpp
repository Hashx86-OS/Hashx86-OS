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

#define KDBG_COMPONENT "GRAPHICS"
#include <core/drivers/GraphicsDriver.h>
#include <debug.h>
#include <string.h>

/**
 * GraphicsDriver() - Deferred-initialization constructor.
 *
 * The RasterSurface base leaves the back buffer empty; InitialiseFramebuffer()
 * completes setup once the hardware geometry is known.
 */
GraphicsDriver::GraphicsDriver() : RasterSurface() {
    this->bpp = 0;
    this->videoMemory = nullptr;
}

/**
 * GraphicsDriver::InitialiseFramebuffer() - Adopt a geometry and allocate the
 * back buffer.
 * @w: Screen width in pixels.
 * @h: Screen height in pixels.
 * @b: Bits per pixel.
 * @vram: Hardware framebuffer, or NULL to defer mapping.
 */
void GraphicsDriver::InitialiseFramebuffer(uint32_t w, uint32_t h, uint32_t b, uint32_t* vram) {
    if (w == 0 || h == 0) {
        HALT("CRITICAL: Invalid graphics dimensions!");
    }

    this->bpp = b;
    this->videoMemory = vram;

    // Resize() validates the geometry and allocation, clears the buffer and
    // precomputes the alpha table.
    if (!Resize(w, h)) {
        HALT("CRITICAL: Failed to allocate graphics back buffer!\n");
    }
}

GraphicsDriver::GraphicsDriver(uint32_t w, uint32_t h, uint32_t b, uint32_t* vram)
    : RasterSurface() {
    InitialiseFramebuffer(w, h, b, vram);
}

GraphicsDriver::~GraphicsDriver() {}

/**
 * GraphicsDriver::ResizeBackBuffer() - Reallocate the scratch buffer for a new
 * geometry.
 * @w: New width in pixels.
 * @h: New height in pixels.
 *
 * Return: True on success, false on invalid geometry or allocation failure.
 */
bool GraphicsDriver::ResizeBackBuffer(uint32_t w, uint32_t h) {
    return Resize(w, h);
}

/**
 * GraphicsDriver::Flush() - Copy the back buffer to video memory.
 *
 * No-op when either pointer is not available.
 */
void GraphicsDriver::Flush() {
    if (videoMemory && backBuffer) {
        uint64_t byte_count = (uint64_t)width * (uint64_t)height * sizeof(uint32_t);
        memcpy(videoMemory, backBuffer, (size_t)byte_count);
    }
}
