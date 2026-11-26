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

#include "ImageDecoder.h"

#include <stddef.h>

// libhx86 allocator declarations. Do not include Hx86/memory.h here: stb_image
// includes the kernel's C-linkage <string.h>, and the C++ declarations of
// memcpy/memset would conflict.
void* kmalloc(int size);
void* krealloc(void* ptr, int size);
void kfree(void* ptr);

#define STBI_NO_STDIO
#define STBI_NO_HDR
#define STBI_NO_LINEAR
#define STBI_NO_THREAD_LOCALS
#define STBI_NO_PNG
#define STBI_NO_BMP
#define STBI_ONLY_JPEG
#define STBI_ASSERT(x) ((void)0)
#define STBI_MALLOC(sz) kmalloc((int)(sz))
#define STBI_FREE(p) kfree(p)
#define STBI_REALLOC(p, sz) krealloc((p), (int)(sz))
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

bool DecodeJpeg(const uint8_t* data, uint32_t length, ImagePixels* image) {
    if (image == nullptr) return false;
    image->pixels = nullptr;
    image->width = 0;
    image->height = 0;
    if (data == nullptr || length == 0 || length > 0x7FFFFFFF) return false;

    int infoWidth = 0;
    int infoHeight = 0;
    int infoChannels = 0;
    if (!stbi_info_from_memory(data, (int)length, &infoWidth, &infoHeight, &infoChannels)) {
        return false;
    }
    if (infoWidth <= 0 || infoHeight <= 0 || infoWidth > 4096 || infoHeight > 4096 ||
        (uint64_t)infoWidth * (uint64_t)infoHeight > (4u * 1024u * 1024u)) {
        return false;
    }

    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* rgba = stbi_load_from_memory(data, (int)length, &width, &height, &channels, 4);
    if (rgba == nullptr || width <= 0 || height <= 0 || width > 4096 || height > 4096) {
        if (rgba != nullptr) stbi_image_free(rgba);
        return false;
    }

    uint64_t count = (uint64_t)width * (uint64_t)height;
    if (count == 0 || count > (uint64_t)(0x7FFFFFFF / sizeof(uint32_t))) {
        stbi_image_free(rgba);
        return false;
    }

    uint32_t* pixels = (uint32_t*)kmalloc((int)(count * sizeof(uint32_t)));
    if (pixels == nullptr) {
        stbi_image_free(rgba);
        return false;
    }

    for (uint64_t i = 0; i < count; i++) {
        uint8_t r = rgba[i * 4 + 0];
        uint8_t g = rgba[i * 4 + 1];
        uint8_t b = rgba[i * 4 + 2];
        uint8_t a = rgba[i * 4 + 3];
        pixels[i] = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    }
    stbi_image_free(rgba);

    image->pixels = pixels;
    image->width = width;
    image->height = height;
    return true;
}
