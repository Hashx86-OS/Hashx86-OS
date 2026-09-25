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

#include <core/filesystem/File.h>
#include <debug.h>
#include <types.h>

#pragma pack(push, 1)

/**
 * struct BitmapFileHeader - On-disk BITMAPFILEHEADER (packed).
 * @type: Magic "BM" (0x4D42).
 * @size: File size in bytes.
 * @reserved1: Reserved by the format.
 * @reserved2: Reserved by the format.
 * @offBits: Offset in bytes to the pixel data.
 */
struct BitmapFileHeader {
    uint16_t type;
    uint32_t size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t offBits;
};

/**
 * struct BitmapInfoHeader - On-disk BITMAPINFOHEADER (packed).
 * @size: Header size (40 bytes).
 * @width: Image width in pixels.
 * @height: Image height in pixels.
 * @planes: Number of color planes; must be 1.
 * @bitCount: Bits per pixel, 24 or 32.
 * @compression: Compression method; 0 = uncompressed.
 * @sizeImage: Size of the raw image data.
 * @xPelsPerMeter: Horizontal resolution in pixels per meter.
 * @yPelsPerMeter: Vertical resolution in pixels per meter.
 * @clrUsed: Number of colors in the palette.
 * @clrImportant: Number of important colors.
 */
struct BitmapInfoHeader {
    uint32_t size;
    int32_t width;
    int32_t height;
    uint16_t planes;
    uint16_t bitCount;
    uint32_t compression;
    uint32_t sizeImage;
    int32_t xPelsPerMeter;
    int32_t yPelsPerMeter;
    uint32_t clrUsed;
    uint32_t clrImportant;
};

#pragma pack(pop)

/**
 * class Bitmap - A loaded BMP image with a raw ARGB pixel buffer.
 *
 * Decodes 24- or 32-bit uncompressed BMP bodies into an 0xAARRGGBB pixel
 * buffer, bottom row first, for blitting by the graphics driver.
 */
class Bitmap {
public:
    /**
     * Bitmap() - Load a bitmap from a path.
     * @fileName: Path of the BMP file relative to the boot partition.
     */
    Bitmap(const char* fileName);

    /**
     * Bitmap() - Load a bitmap from an already-open File object.
     * @file: Open file positioned at the BMP payload.
     */
    Bitmap(File* file);

    /**
     * Bitmap() - Construct a solid-color bitmap of the given size.
     * @width: Image width in pixels.
     * @height: Image height in pixels.
     * @color: Fill color as 0xAARRGGBB.
     */
    Bitmap(int width, int height, uint32_t color);

    /**
     * ~Bitmap() - Free the pixel buffer.
     */
    ~Bitmap();

    /** IsValid() - Report whether the last load produced a decodable image. */
    bool IsValid() {
        return valid;
    }

    /** GetWidth() - Return the image width in pixels. */
    int GetWidth() {
        return width;
    }

    /** GetHeight() - Return the image height in pixels. */
    int GetHeight() {
        return height;
    }

    /** GetBuffer() - Return the raw pixel buffer as 0xAARRGGBB. */
    uint32_t* GetBuffer() {
        return buffer;
    }

private:
    int width;
    int height;
    bool valid;

    /** Load() - Parse the BMP headers and decode the pixel data. */
    void Load(File* file);

    uint32_t* buffer;
};

#endif
