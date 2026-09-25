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

#include <Bitmap.h>
#include <Hx86/debug.h>
#include <Hx86/memory.h>

/**
 * Bitmap() - Decode a raw BMP file buffer into a 32-bit bitmap.
 * @rawData: Raw bytes of the BMP file.
 * @rawSize: Size of @rawData in bytes.
 */
Bitmap::Bitmap(uint8_t* rawData, uint32_t rawSize) {
    valid = false;
    buffer = 0;
    width = 0;
    height = 0;
    LoadFromMemory(rawData, rawSize);
}

/**
 * Bitmap() - Create a solid-color bitmap.
 * @width: Desired width in pixels.
 * @height: Desired height in pixels.
 * @color: Fill color packed as 0xAARRGGBB.
 */
Bitmap::Bitmap(int width, int height, uint32_t color) {
    valid = false;
    this->width = width;
    this->height = height;
    buffer = 0;

    if (width == 0 || height == 0) return;

    buffer = new uint32_t[width * height];
    if (!buffer) return;

    for (int i = 0; i < width * height; i++) {
        buffer[i] = color;
    }
    valid = true;
}

Bitmap::~Bitmap() {
    if (buffer) {
        delete[] buffer;
        buffer = 0;
    }
}

/**
 * LoadFromMemory() - Decode and validate a BMP image from memory.
 * @rawData: Raw bytes of the BMP file.
 * @rawSize: Size of @rawData in bytes.
 *
 * Validates the file and info headers, enforces the 24/32-bit pixel format,
 * un-flips bottom-up rows, and converts each pixel to 0xAARRGGBB in the
 * internal buffer.
 */
void Bitmap::LoadFromMemory(uint8_t* rawData, uint32_t rawSize) {
    if (!rawData || rawSize < sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader)) return;

    BitmapFileHeader* fileHeader = (BitmapFileHeader*)rawData;
    BitmapInfoHeader* infoHeader = (BitmapInfoHeader*)(rawData + sizeof(BitmapFileHeader));

    // Validate the BMP magic.
    if (fileHeader->type != 0x4D42) {
        printf("BMP Error: Invalid signature 0x%x\n", fileHeader->type);
        return;
    }

    if (infoHeader->bitCount != 24 && infoHeader->bitCount != 32) {
        printf("BMP Error: Only 24/32-bit supported (got %d)\n", infoHeader->bitCount);
        return;
    }

    width = infoHeader->width;
    height = infoHeader->height;

    bool isTopDown = false;
    if (height < 0) {
        height = -height;
        isTopDown = true;
    }

    if (width <= 0 || height == 0) {
        printf("BMP Error: Invalid dimensions %dx%d\n", width, height);
        return;
    }

    int bytesPerPixel = infoHeader->bitCount / 8;
    uint64_t rowBytes = (uint64_t)width * (uint64_t)bytesPerPixel;
    uint64_t rowPadding64 = (4 - (rowBytes % 4)) % 4;
    uint64_t totalPixelBytes = (rowBytes + rowPadding64) * (uint64_t)height;

    if (fileHeader->offBits >= rawSize ||
        totalPixelBytes > (uint64_t)(rawSize - fileHeader->offBits)) {
        printf("BMP Error: Pixel data out of bounds\n");
        return;
    }

    uint64_t pixelCount = (uint64_t)width * (uint64_t)height;
    if (pixelCount > (uint64_t)(0xFFFFFFFFu / sizeof(uint32_t))) {
        printf("BMP Error: Dimensions too large %dx%d\n", width, height);
        return;
    }

    buffer = new uint32_t[width * height];
    if (!buffer) return;

    uint8_t* pixelData = rawData + fileHeader->offBits;
    int rowPadding = (int)rowPadding64;

    for (int y = 0; y < height; y++) {
        int targetY = isTopDown ? y : (height - 1 - y);

        for (int x = 0; x < width; x++) {
            uint8_t b = *pixelData++;
            uint8_t g = *pixelData++;
            uint8_t r = *pixelData++;
            uint8_t a = 255;

            if (infoHeader->bitCount == 32) {
                a = *pixelData++;
            }

            uint32_t color = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            buffer[targetY * width + x] = color;
        }
        pixelData += rowPadding;
    }

    valid = true;
    printf("BMP Loaded: %dx%d\n", width, height);
}
