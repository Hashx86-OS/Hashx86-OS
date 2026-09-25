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

#include <core/drivers/vbe.h>

/**
 * VESA_BIOS_Extensions::VESA_BIOS_Extensions() - Construct the VBE driver.
 * @w: Framebuffer width in pixels.
 * @h: Framebuffer height in pixels.
 * @bpp: Color depth in bits per pixel.
 * @vram: Pointer to the linear framebuffer.
 *
 * Just forwards to the GraphicsDriver base, which allocates and clears the
 * software back buffer.
 */
VESA_BIOS_Extensions::VESA_BIOS_Extensions(uint32_t w, uint32_t h, uint32_t bpp, uint32_t* vram)
    : GraphicsDriver(w, h, bpp, vram) {}

VESA_BIOS_Extensions::~VESA_BIOS_Extensions() {}
