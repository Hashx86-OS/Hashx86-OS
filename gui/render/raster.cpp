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

#define KDBG_COMPONENT "GUI:RASTER"
#include <gui/guiplat.h>
#include <gui/render/raster.h>
#include <string.h>

RasterSurface::RasterSurface() {
    this->width = 0;
    this->height = 0;
    this->backBuffer = nullptr;
}

RasterSurface::RasterSurface(uint32_t w, uint32_t h) : RasterSurface() {
    if (!Resize(w, h)) {
        guiplat::Abort("CRITICAL: Failed to allocate raster surface!\n");
    }
}

RasterSurface::~RasterSurface() {
    if (backBuffer) delete[] backBuffer;
}

void RasterSurface::PrecomputeAlphaTable() {
    for (int c = 0; c < 256; c++) {
        for (int a = 0; a < 256; a++) {
            alphaTable[a][c] = (c * a) / 255;
        }
    }
}

bool RasterSurface::Resize(uint32_t w, uint32_t h) {
    if (w == 0 || h == 0) return false;

    uint64_t pixel_count = (uint64_t)w * (uint64_t)h;
    if (pixel_count > (0xFFFFFFFFu / sizeof(uint32_t))) return false;

    uint32_t* newBuffer = new uint32_t[(size_t)pixel_count];
    if (!newBuffer) return false;

    const uint32_t clearColor = 0xFF000000;  // Opaque black.
    for (uint64_t i = 0; i < pixel_count; i++) {
        newBuffer[i] = clearColor;
    }

    // Carry over the region that exists in both geometries.
    if (backBuffer) {
        uint32_t copyW = (w < this->width) ? w : this->width;
        uint32_t copyH = (h < this->height) ? h : this->height;
        for (uint32_t y = 0; y < copyH; y++) {
            memcpy(&newBuffer[(size_t)y * w], &backBuffer[(size_t)y * this->width],
                   (size_t)copyW * sizeof(uint32_t));
        }
        delete[] backBuffer;
    }

    this->backBuffer = newBuffer;
    this->width = w;
    this->height = h;
    PrecomputeAlphaTable();
    return true;
}

void RasterSurface::PutPixel(int32_t x, int32_t y, uint32_t colorIndex) {
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

void RasterSurface::PutPixel(int32_t x, int32_t y, uint8_t a, uint8_t r, uint8_t g, uint8_t b) {
    PutPixel(x, y, (a << 24) | (r << 16) | (g << 8) | b);
}

void RasterSurface::DrawBitmap(int32_t x, int32_t y, const uint32_t* bitmapData,
                               int32_t bitmapWidth, int32_t bitmapHeight) {
    // Clip the vertical range to the surface.
    int32_t startRow = 0;
    int32_t endRow = bitmapHeight;
    if (y < 0) startRow = -y;
    if (y + bitmapHeight > (int32_t)this->height) endRow = this->height - y;

    // Clip the horizontal range to the surface.
    int32_t startCol = 0;
    int32_t endCol = bitmapWidth;
    if (x < 0) startCol = -x;
    if (x + bitmapWidth > (int32_t)this->width) endCol = this->width - x;

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

void RasterSurface::FillRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h,
                                  uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->FillRectangle(this->backBuffer, this->width, this->height, x, y, w, h,
                                        colorIndex);
}

void RasterSurface::DrawRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h,
                                  uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawRectangle(this->backBuffer, this->width, this->height, x, y, w, h,
                                        colorIndex);
}

void RasterSurface::FillRoundedRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h,
                                         uint32_t radius, uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->FillRoundedRectangle(this->backBuffer, this->width, this->height, x, y, w,
                                               h, radius, colorIndex);
}

void RasterSurface::DrawRoundedRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h,
                                         uint32_t radius, uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawRoundedRectangle(this->backBuffer, this->width, this->height, x, y, w,
                                               h, radius, colorIndex);
}

void RasterSurface::DrawRoundedRectangleShadow(int32_t x, int32_t y, uint32_t w, uint32_t h,
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
        if (Y < 0 || Y >= (int32_t)this->height) continue;
        int32_t clampedStartX = (startX < 0) ? 0 : startX;
        int32_t clampedEndX = (endX > (int32_t)this->width) ? (int32_t)this->width : endX;
        uint32_t* pixelPtr = &backBuffer[Y * this->width + clampedStartX];

        for (int32_t X = clampedStartX; X < clampedEndX; ++X) {
            int32_t dx = 0, dy = 0;
            if (X < x)
                dx = x - X;
            else if (X >= x + (int32_t)w)
                dx = X - (x + w - 1);
            if (Y < y)
                dy = y - Y;
            else if (Y >= y + (int32_t)h)
                dy = Y - (y + h - 1);

            int32_t distanceSquared = dx * dx + dy * dy;
            if (distanceSquared <= (int32_t)(shadowRadius * shadowRadius)) {
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

void RasterSurface::BlurRoundedRectangle(int32_t x, int32_t y, uint32_t w, uint32_t h,
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

            // Skip pixels outside the surface bounds.
            if (px < 0 || py < 0 || px >= (int32_t)this->width || py >= (int32_t)this->height)
                continue;

            // Skip pixels outside the rounded region.
            int32_t distX = (dx < (int32_t)radius)                 ? radius - dx
                            : (dx >= (int32_t)w - (int32_t)radius) ? dx - ((int32_t)w - radius)
                                                                   : 0;
            int32_t distY = (dy < (int32_t)radius)                 ? radius - dy
                            : (dy >= (int32_t)h - (int32_t)radius) ? dy - ((int32_t)h - radius)
                                                                   : 0;
            int32_t dist = (distX * distX + distY * distY);

            if (dist > (int32_t)(radius * radius)) continue;  // Skip the corners.

            // Blur: average the nearby pixels, including alpha.
            uint32_t red = 0, green = 0, blue = 0, alpha = 0, count = 0;
            for (int32_t blurY = -(int32_t)blurRadius; blurY <= (int32_t)blurRadius; blurY++) {
                for (int32_t blurX = -(int32_t)blurRadius; blurX <= (int32_t)blurRadius; blurX++) {
                    int32_t nx = px + blurX;
                    int32_t ny = py + blurY;

                    // Average only the in-bounds neighbors.
                    if (nx >= 0 && ny >= 0 && nx < (int32_t)this->width &&
                        ny < (int32_t)this->height) {
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

            if (px < 0 || py < 0 || px >= (int32_t)this->width || py >= (int32_t)this->height)
                continue;

            // Skip pixels outside the rounded region.
            int32_t distX = (dx < (int32_t)radius)                 ? radius - dx
                            : (dx >= (int32_t)w - (int32_t)radius) ? dx - ((int32_t)w - radius)
                                                                   : 0;
            int32_t distY = (dy < (int32_t)radius)                 ? radius - dy
                            : (dy >= (int32_t)h - (int32_t)radius) ? dy - ((int32_t)h - radius)
                                                                   : 0;
            int32_t dist = (distX * distX + distY * distY);

            if (dist > (int32_t)(radius * radius)) continue;

            // Write through PutPixel() so alpha blending applies.
            PutPixel(px, py, tempBuffer[dy * w + dx]);
        }
    }

    delete[] tempBuffer;
}

void RasterSurface::FillCircle(int32_t cx, int32_t cy, uint32_t radius, uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->FillCircle(this->backBuffer, this->width, this->height, cx, cy, radius,
                                     colorIndex);
}

void RasterSurface::DrawCircle(int32_t cx, int32_t cy, uint32_t radius, uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawCircle(this->backBuffer, this->width, this->height, cx, cy, radius,
                                     colorIndex);
}

void RasterSurface::DrawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawLine(this->backBuffer, this->width, this->height, x0, y0, x1, y1,
                                   color);
}

void RasterSurface::DrawHorizontalLine(int32_t x, int32_t y, int32_t length, uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawHorizontalLine(this->backBuffer, this->width, this->height, x, y,
                                             length, colorIndex);
}

void RasterSurface::DrawVerticalLine(int32_t x, int32_t y, int32_t length, uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawVerticalLine(this->backBuffer, this->width, this->height, x, y,
                                           length, colorIndex);
}

void RasterSurface::DrawCharacter(int32_t x, int32_t y, char c, Font* font, uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawCharacter(this->backBuffer, this->width, this->height, x, y, c, font,
                                        colorIndex);
}

void RasterSurface::DrawString(int32_t startX, int32_t startY, const char* str, Font* font,
                               uint32_t colorIndex) {
    if (!NINA::activeInstance) return;
    NINA::activeInstance->DrawString(this->backBuffer, this->width, this->height, startX, startY,
                                     str, font, colorIndex);
}

void RasterSurface::GetScreenCenter(uint32_t w, uint32_t h, int32_t& x, int32_t& y) {
    // Determine the horizontal center.
    if (w >= this->width) {
        x = 0;  // Object wider than the surface: align left.
    } else {
        x = (this->width - w) / 2;
    }

    // Determine the vertical center.
    if (h >= this->height) {
        y = 0;  // Object taller than the surface: align top.
    } else {
        y = (this->height - h) / 2;
    }
}
