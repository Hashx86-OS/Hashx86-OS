/**
 * @file        Bitmap.cpp
 * @brief       User-space BMP Loader
 *
 * @date        28/01/2026
 * @version     1.0.0
 */

#include <Bitmap.h>
#include <Hx86/debug.h>
#include <Hx86/memory.h>

Bitmap::Bitmap(uint8_t* rawData, uint32_t rawSize) {
    valid = false;
    buffer = 0;
    width = 0;
    height = 0;
    LoadFromMemory(rawData, rawSize);
}

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

void Bitmap::LoadFromMemory(uint8_t* rawData, uint32_t rawSize) {
    if (!rawData || rawSize < sizeof(BitmapFileHeader) + sizeof(BitmapInfoHeader)) return;

    BitmapFileHeader* fileHeader = (BitmapFileHeader*)rawData;
    BitmapInfoHeader* infoHeader = (BitmapInfoHeader*)(rawData + sizeof(BitmapFileHeader));

    // Validate BMP magic
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
