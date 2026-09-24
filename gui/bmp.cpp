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

#define KDBG_COMPONENT "GUI:BMP"
#include <core/filesystem/msdospart.h>
#include <core/globals.h>
#include <gui/bmp.h>

Bitmap::Bitmap(File* file) {
    // Initialize defaults.
    this->valid = false;
    this->buffer = 0;
    this->width = 0;
    this->height = 0;
    Load(file);
}

Bitmap::Bitmap(const char* path) {
    this->valid = false;
    this->buffer = 0;
    this->width = 0;
    this->height = 0;

    if (!g_bootPartition) {
        KDBG1("Error: No active boot partition");
        return;
    }

    FileSystem* fs = g_bootPartition;

    File* file = fs->Open(path);

    if (file == 0) {
        KDBG1("Error: File not found %s", path);
        return;
    }

    if (file->size == 0) {
        KDBG1("Error: File is empty %s", path);
        file->Close();
        delete file;
        return;
    }

    Load(file);
    file->Close();
    delete file;
}

Bitmap::Bitmap(int width, int height, uint32_t color) {
    this->valid = false;
    this->width = width;
    this->height = height;
    this->buffer = 0;

    if (width <= 0 || height <= 0) {
        KDBG1("Error: width or height is <= 0.");
        return;
    }
    if ((uint64_t)width * (uint64_t)height > (uint64_t)(0xFFFFFFFFu / sizeof(uint32_t))) {
        KDBG1("Error: Bitmap dimensions overflow: %dx%d", width, height);
        return;
    }

    // Allocate the pixel buffer.
    this->buffer = new uint32_t[width * height];
    if (!this->buffer) {
        HALT("CRITICAL: Failed to allocate bitmap buffer!\n");
    }

    // Fill the buffer with the requested color.
    for (int i = 0; i < width * height; i++) {
        this->buffer[i] = color;
    }

    this->valid = true;
}

Bitmap::~Bitmap() {
    if (buffer) {
        delete[] buffer;
        buffer = 0;
    }
}

void Bitmap::Load(File* file) {
    if (file == 0) return;

    // Allocate a buffer for the raw file.
    uint8_t* rawFile = new uint8_t[file->size];
    if (!rawFile) {
        HALT("CRITICAL: Failed to allocate bitmap raw file buffer!\n");
    }

    // Read the entire file into RAM.
    file->Seek(0);
    int bytesRead = file->Read(rawFile, file->size);

    if (bytesRead != file->size) {
        KDBG1("Warning: Read %d bytes, expected %d", bytesRead, file->size);
    }

    // Parse the headers.
    if (bytesRead < (int)(sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader))) {
        KDBG1("Error: BMP file too small (%d bytes)", bytesRead);
        delete[] rawFile;
        return;
    }
    BitmapFileHeader* fileHeader = (BitmapFileHeader*)rawFile;
    BitmapInfoHeader* infoHeader = (BitmapInfoHeader*)(rawFile + sizeof(BitmapFileHeader));

    // Validate the headers.
    if (fileHeader->type != 0x4D42) {  // 'BM' magic.
        KDBG1("Error: Invalid signature 0x%x", fileHeader->type);
        delete[] rawFile;
        return;
    }

    if (infoHeader->bitCount != 24 && infoHeader->bitCount != 32) {
        KDBG1("Error: Only 24/32-bit supported (got %d)", infoHeader->bitCount);
        delete[] rawFile;
        return;
    }

    this->width = infoHeader->width;
    this->height = infoHeader->height;

    bool isTopDown = false;
    if (this->height < 0) {
        this->height = -this->height;
        isTopDown = true;
    }

    if (this->width <= 0 || this->height == 0) {
        KDBG1("Error: Invalid BMP dimensions %dx%d", this->width, this->height);
        delete[] rawFile;
        return;
    }

    int bytesPerPixel = infoHeader->bitCount / 8;
    uint64_t rowBytes = (uint64_t)this->width * (uint64_t)bytesPerPixel;
    uint64_t rowPadding64 = (4 - (rowBytes % 4)) % 4;
    uint64_t totalPixelBytes = (rowBytes + rowPadding64) * (uint64_t)this->height;

    if (fileHeader->offBits >= (uint32_t)bytesRead ||
        totalPixelBytes > (uint64_t)(bytesRead - (int)fileHeader->offBits)) {
        KDBG1("Error: BMP pixel data out of bounds");
        delete[] rawFile;
        return;
    }

    uint64_t pixelCount = (uint64_t)this->width * (uint64_t)this->height;
    if (pixelCount > (uint64_t)(0xFFFFFFFFu / sizeof(uint32_t))) {
        KDBG1("Error: BMP dimensions too large %dx%d", this->width, this->height);
        delete[] rawFile;
        return;
    }

    // Allocate the pixel buffer.
    this->buffer = new uint32_t[width * height];
    if (!this->buffer) {
        HALT("CRITICAL: Failed to allocate bitmap pixel buffer!\n");
    }

    // Decode the pixel rows bottom-up.
    uint8_t* pixelData = rawFile + fileHeader->offBits;
    int rowPadding = (int)rowPadding64;

    for (int y = 0; y < height; y++) {
        int targetY = isTopDown ? y : (height - 1 - y);

        for (int x = 0; x < width; x++) {
            uint8_t b = *pixelData++;
            uint8_t g = *pixelData++;
            uint8_t r = *pixelData++;
            uint8_t a = 255;

            // 32-bit images carry an alpha channel byte.
            if (infoHeader->bitCount == 32) {
                a = *pixelData++;
            }

            // Combine the channels into 0xAARRGGBB.
            uint32_t color = ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
            this->buffer[targetY * width + x] = color;
        }
        pixelData += rowPadding;
    }

    delete[] rawFile;
    this->valid = true;
    KDBG2("Loaded: %dx%d", width, height);
}
