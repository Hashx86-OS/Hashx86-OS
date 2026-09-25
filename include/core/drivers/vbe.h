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

#include <core/drivers/GraphicsDriver.h>

/**
 * class VESA_BIOS_Extensions - GraphicsDriver backend for VESA linear-framebuffer modes.
 */
class VESA_BIOS_Extensions : public GraphicsDriver {
public:
    /**
     * VESA_BIOS_Extensions() - Set up a VBE linear framebuffer driver.
     * @w: Screen width in pixels.
     * @h: Screen height in pixels.
     * @bpp: Bits per pixel.
     * @vram: Memory-mapped framebuffer address.
     */
    VESA_BIOS_Extensions(uint32_t w, uint32_t h, uint32_t bpp, uint32_t* vram);
    ~VESA_BIOS_Extensions();
};

#endif
