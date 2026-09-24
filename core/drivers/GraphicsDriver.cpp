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

#include <core/drivers/GraphicsDriver.h>

/**
 * GraphicsDriver::GraphicsDriver() - Set up a framebuffer-backed driver core.
 * @w: Framebuffer width in pixels.
 * @h: Framebuffer height in pixels.
 * @b: Bytes per pixel (only stored; the buffer layout is fixed to 4 bytes).
 * @vram: Pointer to the hardware framebuffer, or NULL.
 *
 * Allocates and clears the software back buffer, then precomputes the alpha
 * blending table. The back buffer is filled directly instead of going through
 * FillRectangle(), which needs the NINA renderer to exist.
 */
GraphicsDriver::GraphicsDriver(uint32_t w, uint32_t h, uint32_t b, uint32_t* vram) {
    this->width = w;
    this->height = h;
    this->bpp = b;
    this->videoMemory = vram;

    // Validate the framebuffer size before allocating.
    uint64_t pixel_count = (uint64_t)width * (uint64_t)height;
    if (width == 0 || height == 0 || pixel_count > (0xFFFFFFFFu / sizeof(uint32_t))) {
        HALT("CRITICAL: Invalid graphics dimensions!");
    }
    this->backBuffer = new uint32_t[width * height];
    if (!this->backBuffer) {
        HALT("CRITICAL: Failed to allocate graphics back buffer!\n");
    }

    // Clear the back buffer directly - FillRectangle() dereferences
    // NINA::activeInstance, which does not exist yet at construction time.
    uint32_t clearColor = 0xFF000000;  // Opaque black.
    for (uint64_t i = 0; i < pixel_count; i++) {
        backBuffer[i] = clearColor;
    }

    PrecomputeAlphaTable();
}

GraphicsDriver::~GraphicsDriver() {
    if (backBuffer) delete[] backBuffer;
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

/**
 * GraphicsDriver::PrecomputeAlphaTable() - Build the 8-bit alpha blend LUT.
 *
 * Fills alphaTable[alpha][color] with (color * alpha) / 255 for every
 * combination, so PutPixel() and friends blend without per-pixel divides.
 */
void GraphicsDriver::PrecomputeAlphaTable() {
    for (int c = 0; c < 256; c++) {
        for (int a = 0; a < 256; a++) {
            alphaTable[a][c] = (c * a) / 255;
        }
    }
}

/**
 * GraphicsDriver::PutPixel() - Blend one pixel into the back buffer.
 * @x: Column on screen.
 * @y: Row on screen.
 * @colorIndex: 32-bit ARGB color.
 *
 * Fully opaque pixels overwrite the destination; translucent pixels are
 * composited against the current back buffer value using the alpha LUT.
 * Pixels outside the screen are ignored.
 */
void GraphicsDriver::PutPixel(int32_t x, int32_t y, uint32_t colorIndex) {
    if ((uint32_t)x >= width || (uint32_t)y >= height) return;

    uint32_t* pixel = &backBuffer[y * width + x];
    uint8_t alpha = (colorIndex >> 24) & 0xFF;

    if (alpha == 0) return;
    if (alpha == 255) {
        *pixel = colorIndex;
        return;
    }

    uint32_t existingColor = *pixel;
    uint32_t invAlpha = 255 - alpha;

    uint8_t r = alphaTable[alpha][(colorIndex >> 16) & 0xFF] +
                alphaTable[invAlpha][(existingColor >> 16) & 0xFF];
    uint8_t g = alphaTable[alpha][(colorIndex >> 8) & 0xFF] +
                alphaTable[invAlpha][(existingColor >> 8) & 0xFF];
    uint8_t b = alphaTable[alpha][colorIndex & 0xFF] + alphaTable[invAlpha][existingColor & 0xFF];

    *pixel = (255 << 24) | (r << 16) | (g << 8) | b;
}

void GraphicsDriver::PutPixel(int32_t x, int32_t y, uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    PutPixel(x, y, (a << 24) | (r << 16) | (g << 8) | b);
}

/**
 * GraphicsDriver::DrawBitmap() - Draw a 32-bit ARGB bitmap, alpha-blended.
 * @x, @y: Top-left screen position (may be off-screen).
 * @bitmapData: Row-major ARGB pixels, bitmapWidth * bitmapHeight entries.
 * @bitmapWidth, @bitmapHeight: Bitmap dimensions in pixels.
 *
 * Clips each row and column to the screen before drawing, and composites
 * translucent pixels against the back buffer.
 */
void GraphicsDriver::DrawBitmap(int32_t x, int32_t y, const uint32_t* bitmapData,
                                int32_t bitmapWidth, int32_t bitmapHeight) {
    // Clip the vertical range to the screen.
    int32_t startRow = 0;
    int32_t endRow = bitmapHeight;
    if (y < 0) startRow = -y;
    if (y + bitmapHeight > this->height) endRow = this->height - y;

    // Clip the horizontal range to the screen.
    int32_t startCol = 0;
    int32_t endCol = bitmapWidth;
    if (x < 0) startCol = -x;
    if (x + bitmapWidth > this->width) endCol = this->width - x;

    for (int32_t row = startRow; row < endRow; ++row) {
        int32_t screenY = y + row;
        const uint32_t* bmpPtr = &bitmapData[row * bitmapWidth];
        uint32_t* rowDst = &backBuffer[screenY * this->width];

        for (int32_t col = startCol; col < endCol; ++col) {
            int32_t screenX = x + col;

            uint32_t srcColor = bmpPtr[col];
            uint8_t alpha = (srcColor >> 24) & 0xFF;

            if (alpha == 255) {
                rowDst[screenX] = srcColor;
            } else if (alpha > 0) {
                uint32_t dstColor = rowDst[screenX];
                uint32_t invAlpha = 255 - alpha;

                uint8_t srcRed = (srcColor >> 16) & 0xFF;
                uint8_t srcGreen = (srcColor >> 8) & 0xFF;
                uint8_t srcBlue = srcColor & 0xFF;

                uint8_t dstRed = (dstColor >> 16) & 0xFF;
                uint8_t dstGreen = (dstColor >> 8) & 0xFF;
                uint8_t dstBlue = dstColor & 0xFF;

                uint8_t blendedRed = alphaTable[alpha][srcRed] + alphaTable[invAlpha][dstRed];
                uint8_t blendedGreen = alphaTable[alpha][srcGreen] + alphaTable[invAlpha][dstGreen];
                uint8_t blendedBlue = alphaTable[alpha][srcBlue] + alphaTable[invAlpha][dstBlue];

                rowDst[screenX] =
                    (0xFF << 24) | (blendedRed << 16) | (blendedGreen << 8) | blendedBlue;
            }
        }
    }
}

void GraphicsDriver::FillRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h,
                                   uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->FillRectangle(this->backBuffer, this->width, this->height, x, y, w, h,
                                        colorIndex);
}

void GraphicsDriver::DrawRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h,
                                   uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawRectangle(this->backBuffer, this->width, this->height, x, y, w, h,
                                        colorIndex);
}

void GraphicsDriver::FillRoundedRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h,
                                          uint32_t radius, uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->FillRoundedRectangle(this->backBuffer, this->width, this->height, x, y, w,
                                               h, radius, colorIndex);
}

void GraphicsDriver::DrawRoundedRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h,
                                          uint32_t radius, uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawRoundedRectangle(this->backBuffer, this->width, this->height, x, y, w,
                                               h, radius, colorIndex);
}

void GraphicsDriver::DrawRoundedRectangleShadow(int32_t x, int32_t y, uint32_t w, uint32_t h,
                                                uint32_t shadowSize, uint32_t shadowRadius,
                                                uint32_t shadowColor) {
    int32_t startX = x - shadowSize;
    int32_t startY = y - shadowSize;
    int32_t endX = x + w + shadowSize;
    int32_t endY = y + h + shadowSize;

    uint8_t shadowAlpha = (shadowColor >> 24) & 0xFF;
    uint8_t shadowRed = (shadowColor >> 16) & 0xFF;
    uint8_t shadowGreen = (shadowColor >> 8) & 0xFF;
    uint8_t shadowBlue = shadowColor & 0xFF;

    for (int32_t Y = startY; Y < endY; ++Y) {
        if (Y < 0 || Y >= this->height) continue;
        int32_t clampedStartX = (startX < 0) ? 0 : startX;
        int32_t clampedEndX = (endX > (int32_t)this->width) ? (int32_t)this->width : endX;
        uint32_t* pixelPtr = &backBuffer[Y * this->width + clampedStartX];

        for (int32_t X = clampedStartX; X < clampedEndX; ++X) {
            int32_t dx = 0, dy = 0;
            if (X < x)
                dx = x - X;
            else if (X >= x + w)
                dx = X - (x + w - 1);
            if (Y < y)
                dy = y - Y;
            else if (Y >= y + h)
                dy = Y - (y + h - 1);

            int32_t distanceSquared = dx * dx + dy * dy;
            if (distanceSquared <= (shadowRadius * shadowRadius)) {
                uint8_t alpha;
                if (shadowRadius == 0) {
                    alpha = shadowAlpha;  // No falloff: constant opacity.
                } else {
                    alpha = alphaTable[shadowAlpha][255 - (distanceSquared * 255) /
                                                              (shadowRadius * shadowRadius)];
                }
                uint32_t dstColor = *pixelPtr;
                uint32_t invAlpha = 255 - alpha;

                uint8_t dstRed = (dstColor >> 16) & 0xFF;
                uint8_t dstGreen = (dstColor >> 8) & 0xFF;
                uint8_t dstBlue = dstColor & 0xFF;

                uint8_t blendedRed = alphaTable[alpha][shadowRed] + alphaTable[invAlpha][dstRed];
                uint8_t blendedGreen =
                    alphaTable[alpha][shadowGreen] + alphaTable[invAlpha][dstGreen];
                uint8_t blendedBlue = alphaTable[alpha][shadowBlue] + alphaTable[invAlpha][dstBlue];

                *pixelPtr = (0xFF << 24) | (blendedRed << 16) | (blendedGreen << 8) | blendedBlue;
            }
            pixelPtr++;
        }
    }
}

void GraphicsDriver::BlurRoundedRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h,
                                          uint32_t radius, uint32_t blurRadius) {
    uint64_t pixelCount = (uint64_t)w * (uint64_t)h;
    if (pixelCount == 0 || pixelCount > (uint64_t)(0xFFFFFFFFu / sizeof(uint32_t))) {
        return;
    }

    // Allocate a temporary buffer for the blurred pixels.
    uint32_t* tempBuffer = new uint32_t[pixelCount];
    if (!tempBuffer) {
        return;
    }

    // First pass: compute the blurred colors.
    for (int32_t dy = 0; dy < (int32_t)h; dy++) {
        for (int32_t dx = 0; dx < (int32_t)w; dx++) {
            int32_t px = x + dx;
            int32_t py = y + dy;

            // Skip pixels outside the screen bounds.
            if (px < 0 || py < 0 || px >= this->width || py >= this->height) continue;

            // Skip pixels outside the rounded region.
            int32_t distX = (dx < radius)                 ? radius - dx
                            : (dx >= (int32_t)w - radius) ? dx - ((int32_t)w - radius)
                                                          : 0;
            int32_t distY = (dy < radius)                 ? radius - dy
                            : (dy >= (int32_t)h - radius) ? dy - ((int32_t)h - radius)
                                                          : 0;
            int32_t dist = (distX * distX + distY * distY);

            if (dist > radius * radius) continue;  // Skip the corners.

            // Blur: average the nearby pixels, including alpha.
            uint32_t red = 0, green = 0, blue = 0, alpha = 0, count = 0;
            for (int32_t blurY = -blurRadius; blurY <= blurRadius; blurY++) {
                for (int32_t blurX = -blurRadius; blurX <= blurRadius; blurX++) {
                    int32_t nx = px + blurX;
                    int32_t ny = py + blurY;

                    // Average only the in-bounds neighbors.
                    if (nx >= 0 && ny >= 0 && nx < this->width && ny < this->height) {
                        uint32_t color = backBuffer[ny * this->width + nx];
                        red += (color >> 16) & 0xFF;
                        green += (color >> 8) & 0xFF;
                        blue += color & 0xFF;
                        alpha += (color >> 24) & 0xFF;  // Preserve alpha.
                        count++;
                    }
                }
            }

            // Store the blurred pixel in the temporary buffer.
            if (count > 0) {
                red = (red + count / 2) / count;
                green = (green + count / 2) / count;
                blue = (blue + count / 2) / count;
                alpha = (alpha + count / 2) / count;  // Averaged alpha.

                tempBuffer[dy * w + dx] = (alpha << 24) | (red << 16) | (green << 8) | blue;
            } else {
                tempBuffer[dy * w + dx] = backBuffer[py * this->width + px];  // Keep original.
            }
        }
    }

    // Second pass: write the blurred pixels back through PutPixel().
    for (int32_t dy = 0; dy < (int32_t)h; dy++) {
        for (int32_t dx = 0; dx < (int32_t)w; dx++) {
            int32_t px = x + dx;
            int32_t py = y + dy;

            if (px < 0 || py < 0 || px >= this->width || py >= this->height) continue;

            // Skip pixels outside the rounded region.
            int32_t distX = (dx < radius)                 ? radius - dx
                            : (dx >= (int32_t)w - radius) ? dx - ((int32_t)w - radius)
                                                          : 0;
            int32_t distY = (dy < radius)                 ? radius - dy
                            : (dy >= (int32_t)h - radius) ? dy - ((int32_t)h - radius)
                                                          : 0;
            int32_t dist = (distX * distX + distY * distY);

            if (dist > radius * radius) continue;

            // Write through PutPixel() so alpha blending applies.
            PutPixel(px, py, tempBuffer[dy * w + dx]);
        }
    }

    delete[] tempBuffer;
}

void GraphicsDriver::FillCircle(int32_t cx, int32_t cy, uint32_t radius, uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->FillCircle(this->backBuffer, this->width, this->height, cx, cy, radius,
                                     colorIndex);
}

void GraphicsDriver::DrawCircle(int32_t cx, int32_t cy, uint32_t radius, uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawCircle(this->backBuffer, this->width, this->height, cx, cy, radius,
                                     colorIndex);
}

void GraphicsDriver::DrawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawLine(this->backBuffer, this->width, this->height, x0, y0, x1, y1,
                                   color);
}

void GraphicsDriver::DrawHorizontalLine(int32_t x, int32_t y, int32_t length, uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawHorizontalLine(this->backBuffer, this->width, this->height, x, y,
                                             length, colorIndex);
}

void GraphicsDriver::DrawVerticalLine(int32_t x, int32_t y, int32_t length, uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawVerticalLine(this->backBuffer, this->width, this->height, x, y,
                                           length, colorIndex);
}

void GraphicsDriver::DrawCharacter(int32_t x, int32_t y, char c, Font* font, uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawCharacter(this->backBuffer, this->width, this->height, x, y, c, font,
                                        colorIndex);
}

void GraphicsDriver::DrawString(int32_t startX, int32_t startY, const char* str, Font* font,
                                uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawString(this->backBuffer, this->width, this->height, startX, startY,
                                     str, font, colorIndex);
}

// --- Utility functions ---
void GraphicsDriver::GetScreenCenter(uint32_t w, uint32_t h, int32_t& x, int32_t& y) {
    // Determine the horizontal center.
    if (w >= this->width) {
        x = 0;  // Object wider than the screen: align left.
    } else {
        x = (this->width - w) / 2;
    }

    // Determine the vertical center.
    if (h >= this->height) {
        y = 0;  // Object taller than the screen: align top.
    } else {
        y = (this->height - h) / 2;
    }
}
