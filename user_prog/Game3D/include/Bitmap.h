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

#ifndef BITMAP_H
#define BITMAP_H

#include <Hx86/stdint.h>

#pragma pack(push, 1)

/**
 * struct BitmapFileHeader - On-disk BITMAPFILEHEADER (14 bytes).
 * @type: File-type magic "BM" (0x4D42).
 * @size: Total file size in bytes.
 * @reserved1: Reserved, must be zero.
 * @reserved2: Reserved, must be zero.
 * @offBits: Byte offset from the file start to the pixel data.
 */
struct BitmapFileHeader {
    uint16_t type;  // Magic "BM" (0x4D42).
    uint32_t size;  // Total file size.
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offBits;  // Offset to the image data.
};

/**
 * struct BitmapInfoHeader - On-disk BITMAPINFOHEADER (40 bytes).
 * @size: Header size in bytes (40).
 * @width: Bitmap width in pixels.
 * @height: Bitmap height in pixels (positive for bottom-up, negative for
 *          top-down).
 * @planes: Number of color planes, must be 1.
 * @bitCount: Bits per pixel, 24 or 32.
 * @compression: Compression scheme, 0 for uncompressed.
 * @sizeImage: Size of the pixel data in bytes.
 * @xPelsPerMeter: Horizontal pixels per meter.
 * @yPelsPerMeter: Vertical pixels per meter.
 * @clrUsed: Number of colors in the palette.
 * @clrImportant: Number of important colors.
 */
struct BitmapInfoHeader {
    uint32_t size;  // Header size (40 bytes).
    int32_t width;
    int32_t height;
    uint16_t planes;       // Must be 1.
    uint16_t bitCount;     // 24 or 32.
    uint32_t compression;  // 0 = uncompressed.
    uint32_t sizeImage;
    int32_t xPelsPerMeter;
    int32_t yPelsPerMeter;
    uint32_t clrUsed;
    uint32_t clrImportant;
};

#pragma pack(pop)

/**
 * class Bitmap - A software-decoded image, always stored as 32-bit BGRA.
 *
 * Decodes 24/32-bit BMP data into an internal 32-bit buffer where every pixel
 * is packed as 0xAARRGGBB.
 */
class Bitmap {
public:
    // Construct a bitmap by decoding a raw file buffer (e.g. from syscall_read).
    Bitmap(uint8_t* rawData, uint32_t rawSize);
    // Construct a solid-color bitmap of the given dimensions.
    Bitmap(int width, int height, uint32_t color);
    ~Bitmap();

    bool IsValid() {
        return valid;
    }
    int GetWidth() {
        return width;
    }
    int GetHeight() {
        return height;
    }
    uint32_t* GetBuffer() {
        return buffer;
    }

private:
    int width;
    int height;
    bool valid;
    uint32_t* buffer;

    void LoadFromMemory(uint8_t* rawData, uint32_t rawSize);
};

#endif  // BITMAP_H
