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

#define KDBG_COMPONENT "GUI:NINA"
#include <gui/renderer/nina.h>
#include <string.h>

NINA* NINA::activeInstance = nullptr;

NINA::NINA() {
    PrecomputeAlphaTable();
    activeInstance = this;
}

void NINA::PrecomputeAlphaTable() {
    for (int c = 0; c < 256; c++) {
        for (int a = 0; a < 256; a++) {
            this->alphaTable[a][c] = (c * a) / 255;
        }
    }
}

NINA::~NINA() {}

void NINA::DrawBitmapToBuffer(uint32_t* dst, int dstW, int dstH, int dstX, int dstY, uint32_t* src,
                              int srcW, int srcH) {
    if (!dst || !src || dstW <= 0 || dstH <= 0 || srcW <= 0 || srcH <= 0) return;

    // Clip in 64-bit space so a huge offset or size cannot wrap around and
    // turn a fully off-surface blit into a full-surface one.
    int64_t x0 = dstX;
    int64_t y0 = dstY;
    int64_t x1 = (int64_t)dstX + srcW;
    int64_t y1 = (int64_t)dstY + srcH;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > dstW) x1 = dstW;
    if (y1 > dstH) y1 = dstH;
    if (x0 >= x1 || y0 >= y1) return;

    const int64_t srcX0 = x0 - (int64_t)dstX;
    const int span = (int)(x1 - x0);

    for (int64_t y = y0; y < y1; ++y) {
        int64_t dy = y;
        uint32_t* dstRow = dst + dy * dstW + x0;
        uint32_t* srcRow = src + (y - dstY) * srcW + srcX0;

        for (int x = 0; x < span; ++x) {
            uint32_t srcPixel = srcRow[x];
            uint8_t srcA = (srcPixel >> 24) & 0xFF;

            if (srcA == 255) {
                dstRow[x] = srcPixel;
            } else if (srcA > 0) {
                uint32_t dstPixel = dstRow[x];
                uint8_t srcR = (srcPixel >> 16) & 0xFF;
                uint8_t srcG = (srcPixel >> 8) & 0xFF;
                uint8_t srcB = srcPixel & 0xFF;

                uint8_t dstR = (dstPixel >> 16) & 0xFF;
                uint8_t dstG = (dstPixel >> 8) & 0xFF;
                uint8_t dstB = dstPixel & 0xFF;

                uint8_t invA = 255 - srcA;
                uint8_t outR = alphaTable[srcA][srcR] + alphaTable[invA][dstR];
                uint8_t outG = alphaTable[srcA][srcG] + alphaTable[invA][dstG];
                uint8_t outB = alphaTable[srcA][srcB] + alphaTable[invA][dstB];

                dstRow[x] = (0xFF << 24) | (outR << 16) | (outG << 8) | outB;
            }
        }
    }
}

void NINA::DrawBitmap(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x,
                      int32_t y, const uint32_t* bitmapData, int32_t bitmapWidth,
                      int32_t bitmapHeight) {
    if (!buffer || !bitmapData || bufferWidth <= 0 || bufferHeight <= 0 || bitmapWidth <= 0 ||
        bitmapHeight <= 0) {
        return;
    }
    if (x >= bufferWidth || y >= bufferHeight) return;
    if ((int64_t)x + bitmapWidth <= 0 || (int64_t)y + bitmapHeight <= 0) return;

    int32_t startCol = (x < 0) ? -x : 0;
    int32_t drawWidth = bitmapWidth - startCol;
    int32_t startX = (x < 0) ? 0 : x;
    if (startX + drawWidth > bufferWidth) {
        drawWidth = bufferWidth - startX;
    }
    if (drawWidth <= 0) return;

    for (int32_t row = 0; row < bitmapHeight; ++row) {
        int32_t screenY = y + row;
        if (screenY < 0 || screenY >= bufferHeight) continue;

        uint32_t* pixelPtr = &buffer[screenY * bufferWidth + startX];
        const uint32_t* bmpPtr = &bitmapData[row * bitmapWidth + startCol];

        int32_t col = 0;

        // Fast-path memcpy if fully opaque and it fits.
        while (col < drawWidth) {
            if ((startX + col) >= bufferWidth) break;

            int32_t runStart = col;
            while (col < drawWidth && ((startX + col) < bufferWidth) &&
                   ((bmpPtr[col] >> 24) == 255)) {
                col++;
            }

            int runLength = col - runStart;
            if (runLength > 0) {
                memcpy(&pixelPtr[runStart], &bmpPtr[runStart], runLength * sizeof(uint32_t));
            }

            // Blend the rest.
            while (col < drawWidth && (startX + col) < bufferWidth) {
                uint32_t srcColor = bmpPtr[col];
                uint8_t alpha = (srcColor >> 24) & 0xFF;

                if (alpha > 0) {
                    uint32_t dstColor = pixelPtr[col];
                    uint32_t invAlpha = 255 - alpha;

                    uint8_t srcRed = (srcColor >> 16) & 0xFF;
                    uint8_t srcGreen = (srcColor >> 8) & 0xFF;
                    uint8_t srcBlue = srcColor & 0xFF;

                    uint8_t dstRed = (dstColor >> 16) & 0xFF;
                    uint8_t dstGreen = (dstColor >> 8) & 0xFF;
                    uint8_t dstBlue = dstColor & 0xFF;

                    uint8_t blendedRed = alphaTable[alpha][srcRed] + alphaTable[invAlpha][dstRed];
                    uint8_t blendedGreen =
                        alphaTable[alpha][srcGreen] + alphaTable[invAlpha][dstGreen];
                    uint8_t blendedBlue =
                        alphaTable[alpha][srcBlue] + alphaTable[invAlpha][dstBlue];

                    pixelPtr[col] =
                        (255 << 24) | (blendedRed << 16) | (blendedGreen << 8) | blendedBlue;
                }
                col++;
            }
        }
    }
}

void NINA::FillRectangle(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x,
                         int32_t y, int32_t w, int32_t h, uint32_t colorIndex) {
    if (!buffer || bufferWidth <= 0 || bufferHeight <= 0 || w <= 0 || h <= 0) return;

    // Clip in 64-bit space: signed dimensions plus negative origins must not
    // wrap around the 32-bit range (a rect fully left of the surface used to
    // clamp to "fill the whole surface").
    int64_t x0 = x;
    int64_t y0 = y;
    int64_t x1 = (int64_t)x + w;
    int64_t y1 = (int64_t)y + h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > bufferWidth) x1 = bufferWidth;
    if (y1 > bufferHeight) y1 = bufferHeight;
    if (x0 >= x1 || y0 >= y1) return;

    for (int64_t Y = y0; Y < y1; ++Y) {
        uint32_t* row = buffer + Y * bufferWidth;
        for (int64_t X = x0; X < x1; ++X) {
            row[X] = colorIndex;
        }
    }
}

void NINA::DrawRectangle(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x,
                         int32_t y, int32_t w, int32_t h, uint32_t colorIndex) {
    if (!buffer || bufferWidth <= 0 || bufferHeight <= 0 || w <= 0 || h <= 0) return;

    int64_t x0 = x;
    int64_t y0 = y;
    int64_t x1 = (int64_t)x + w - 1;
    int64_t y1 = (int64_t)y + h - 1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= bufferWidth) x1 = bufferWidth - 1;
    if (y1 >= bufferHeight) y1 = bufferHeight - 1;
    if (x0 > x1 || y0 > y1) return;

    for (int64_t X = x0; X <= x1; ++X) {
        buffer[y0 * bufferWidth + X] = colorIndex;
        buffer[y1 * bufferWidth + X] = colorIndex;
    }
    for (int64_t Y = y0; Y <= y1; ++Y) {
        buffer[Y * bufferWidth + x0] = colorIndex;
        buffer[Y * bufferWidth + x1] = colorIndex;
    }
}

void NINA::FillRoundedRectangle(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight,
                                int32_t x, int32_t y, int32_t w, int32_t h, uint32_t radius,
                                uint32_t colorIndex) {
    if (!buffer || bufferWidth <= 0 || bufferHeight <= 0 || w <= 0 || h <= 0) return;
    if (radius > (uint32_t)(w / 2)) radius = (uint32_t)(w / 2);
    if (radius > (uint32_t)(h / 2)) radius = (uint32_t)(h / 2);

    const int32_t r = (int32_t)radius;
    // Edge origins are computed in 64-bit space; a rectangle that lands
    // outside the int32 range cannot intersect the surface anyway.
    int64_t edgeX = (int64_t)x + r;
    int64_t edgeY = (int64_t)y + r;
    if (edgeX >= INT32_MIN && edgeX <= INT32_MAX)
        FillRectangle(buffer, bufferWidth, bufferHeight, (int32_t)edgeX, y, w - 2 * r, h,
                      colorIndex);
    if (edgeY >= INT32_MIN && edgeY <= INT32_MAX)
        FillRectangle(buffer, bufferWidth, bufferHeight, x, (int32_t)edgeY, w, h - 2 * r,
                      colorIndex);
    if (r <= 0) return;

    const uint64_t r2 = (uint64_t)r * (uint64_t)r;
    for (int32_t dy = 0; dy <= r; ++dy) {
        for (int32_t dx = 0; dx <= r; ++dx) {
            if ((uint64_t)dx * (uint64_t)dx + (uint64_t)dy * (uint64_t)dy > r2) continue;

            int64_t tlX = (int64_t)x + r - dx;
            int64_t tlY = (int64_t)y + r - dy;
            int64_t trX = (int64_t)x + w - r + dx - 1;
            int64_t brY = (int64_t)y + h - r + dy - 1;

            if (tlX >= 0 && tlY >= 0 && tlX < bufferWidth && tlY < bufferHeight)
                buffer[tlY * bufferWidth + tlX] = colorIndex;
            if (trX >= 0 && tlY >= 0 && trX < bufferWidth && tlY < bufferHeight)
                buffer[tlY * bufferWidth + trX] = colorIndex;
            if (tlX >= 0 && brY >= 0 && tlX < bufferWidth && brY < bufferHeight)
                buffer[brY * bufferWidth + tlX] = colorIndex;
            if (trX >= 0 && brY >= 0 && trX < bufferWidth && brY < bufferHeight)
                buffer[brY * bufferWidth + trX] = colorIndex;
        }
    }
}

void NINA::DrawRoundedRectangle(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight,
                                int32_t x, int32_t y, int32_t w, int32_t h, uint32_t radius,
                                uint32_t colorIndex) {
    if (!buffer || bufferWidth <= 0 || bufferHeight <= 0 || w <= 0 || h <= 0) return;
    if (radius > (uint32_t)(w / 2)) radius = (uint32_t)(w / 2);
    if (radius > (uint32_t)(h / 2)) radius = (uint32_t)(h / 2);

    const int32_t r = (int32_t)radius;

    // Straight edges. The y coordinates are signed and checked explicitly so a
    // rectangle above or below the surface never indexes the buffer.
    int64_t top = y;
    int64_t bottom = (int64_t)y + h - 1;
    for (int64_t i = (int64_t)x + r; i < (int64_t)x + w - r; ++i) {
        if (i < 0 || i >= bufferWidth) continue;
        if (top >= 0 && top < bufferHeight) buffer[top * bufferWidth + i] = colorIndex;
        if (bottom >= 0 && bottom < bufferHeight) buffer[bottom * bufferWidth + i] = colorIndex;
    }

    int64_t left = x;
    int64_t right = (int64_t)x + w - 1;
    for (int64_t i = (int64_t)y + r; i < (int64_t)y + h - r; ++i) {
        if (i < 0 || i >= bufferHeight) continue;
        if (left >= 0 && left < bufferWidth) buffer[i * bufferWidth + left] = colorIndex;
        if (right >= 0 && right < bufferWidth) buffer[i * bufferWidth + right] = colorIndex;
    }

    if (r <= 0) return;

    const uint64_t outer2 = (uint64_t)r * (uint64_t)r;
    const uint64_t inner2 = (uint64_t)(r - 1) * (uint64_t)(r - 1);
    for (int32_t dy = 0; dy <= r; ++dy) {
        for (int32_t dx = 0; dx <= r; ++dx) {
            uint64_t d2 = (uint64_t)dx * (uint64_t)dx + (uint64_t)dy * (uint64_t)dy;
            if (d2 < inner2 || d2 > outer2) continue;

            int64_t tlX = (int64_t)x + r - dx;
            int64_t tlY = (int64_t)y + r - dy;
            int64_t trX = (int64_t)x + w - r + dx - 1;
            int64_t brY = (int64_t)y + h - r + dy - 1;

            if (tlX >= 0 && tlY >= 0 && tlX < bufferWidth && tlY < bufferHeight)
                buffer[tlY * bufferWidth + tlX] = colorIndex;
            if (trX >= 0 && tlY >= 0 && trX < bufferWidth && tlY < bufferHeight)
                buffer[tlY * bufferWidth + trX] = colorIndex;
            if (tlX >= 0 && brY >= 0 && tlX < bufferWidth && brY < bufferHeight)
                buffer[brY * bufferWidth + tlX] = colorIndex;
            if (trX >= 0 && brY >= 0 && trX < bufferWidth && brY < bufferHeight)
                buffer[brY * bufferWidth + trX] = colorIndex;
        }
    }
}

void NINA::FillCircle(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t cx,
                      int32_t cy, uint32_t radius, uint32_t colorIndex) {
    if (!buffer || bufferWidth <= 0 || bufferHeight <= 0) return;
    if (radius == 0) {
        if (cx >= 0 && cy >= 0 && cx < bufferWidth && cy < bufferHeight)
            buffer[cy * bufferWidth + cx] = colorIndex;
        return;
    }
    if (radius > 0x3FFFFFFFu) return;

    int32_t x = (int32_t)radius;
    int32_t y = 0;
    int32_t err = 0;

    while (x >= y) {
        for (int32_t dx = cx - x; dx <= cx + x; ++dx) {
            if (cy + y >= 0 && cy + y < bufferHeight && dx >= 0 && dx < bufferWidth)
                buffer[(cy + y) * bufferWidth + dx] = colorIndex;
            if (cy - y >= 0 && cy - y < bufferHeight && dx >= 0 && dx < bufferWidth)
                buffer[(cy - y) * bufferWidth + dx] = colorIndex;
        }
        for (int32_t dx = cx - y; dx <= cx + y; ++dx) {
            if (cy + x >= 0 && cy + x < bufferHeight && dx >= 0 && dx < bufferWidth)
                buffer[(cy + x) * bufferWidth + dx] = colorIndex;
            if (cy - x >= 0 && cy - x < bufferHeight && dx >= 0 && dx < bufferWidth)
                buffer[(cy - x) * bufferWidth + dx] = colorIndex;
        }

        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void NINA::DrawCircle(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t cx,
                      int32_t cy, uint32_t radius, uint32_t colorIndex) {
    if (!buffer || bufferWidth <= 0 || bufferHeight <= 0) return;
    if (radius == 0) {
        if (cx >= 0 && cy >= 0 && cx < bufferWidth && cy < bufferHeight)
            buffer[cy * bufferWidth + cx] = colorIndex;
        return;
    }
    if (radius > 0x3FFFFFFFu) return;

    int32_t x = (int32_t)radius;
    int32_t y = 0;
    int32_t err = 0;

    while (x >= y) {
        auto set = [&](int32_t px, int32_t py) {
            if (px >= 0 && py >= 0 && px < bufferWidth && py < bufferHeight)
                buffer[py * bufferWidth + px] = colorIndex;
        };

        set(cx + x, cy + y);
        set(cx + y, cy + x);
        set(cx - y, cy + x);
        set(cx - x, cy + y);
        set(cx - x, cy - y);
        set(cx - y, cy - x);
        set(cx + y, cy - x);
        set(cx + x, cy - y);

        y++;
        if (err <= 0) {
            err += 2 * y + 1;
        } else {
            x--;
            err += 2 * (y - x) + 1;
        }
    }
}

void NINA::DrawLine(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x0,
                    int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
    if (!buffer || bufferWidth <= 0 || bufferHeight <= 0) return;

    // Bresenham's line algorithm. The step counters are 64-bit so a line
    // between extreme coordinates cannot overflow the error term.
    int64_t dx = (x1 > x0) ? (int64_t)x1 - x0 : (int64_t)x0 - x1;
    int64_t dy = (y1 > y0) ? (int64_t)y1 - y0 : (int64_t)y0 - y1;
    int32_t sx = (x0 < x1) ? 1 : -1;
    int32_t sy = (y0 < y1) ? 1 : -1;
    int64_t err = dx - dy;

    while (true) {
        // The bounds check is critical to prevent kernel crashes.
        if (x0 >= 0 && x0 < bufferWidth && y0 >= 0 && y0 < bufferHeight) {
            buffer[y0 * bufferWidth + x0] = color;
        }

        if (x0 == x1 && y0 == y1) break;

        int64_t e2 = 2 * err;

        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }

        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void NINA::DrawHorizontalLine(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight,
                              int32_t x, int32_t y, int32_t length, uint32_t colorIndex) {
    if (!buffer || bufferWidth <= 0 || bufferHeight <= 0) return;
    if (y < 0 || y >= bufferHeight) return;
    for (int64_t i = 0; i < length; ++i) {
        int64_t px = (int64_t)x + i;
        if (px >= 0 && px < bufferWidth) buffer[y * bufferWidth + px] = colorIndex;
    }
}

void NINA::DrawVerticalLine(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x,
                            int32_t y, int32_t length, uint32_t colorIndex) {
    if (!buffer || bufferWidth <= 0 || bufferHeight <= 0) return;
    if (x < 0 || x >= bufferWidth) return;
    for (int64_t i = 0; i < length; ++i) {
        int64_t py = (int64_t)y + i;
        if (py >= 0 && py < bufferHeight) buffer[py * bufferWidth + x] = colorIndex;
    }
}

void NINA::DrawCharacter(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x,
                         int32_t y, uint32_t codepoint, Font* font, uint32_t colorIndex) {
    if (!buffer || !font || !font->font_glyphs || !font->font_atlas) return;
    int idx = (int)(codepoint - font->firstChar);
    if (idx < 0 || idx >= (int)font->glyph_count) return;
    int16_t* g = &font->font_glyphs[idx * 8];  // Each glyph stores 8 values.

    // g[0] is the glyph id and g[7] the advance; neither is needed for a
    // single-character blit (DrawString applies the advance).
    int gridX = g[1];
    int gridY = g[2];
    int charWidth = g[3];
    int charHeight = g[4];
    int xoffset = g[5];
    int yoffset = g[6];

    if (charWidth <= 0 || charHeight <= 0) return;
    if (charWidth > font->atlas_width || charHeight > font->atlas_height) return;
    // Validate the atlas region bounds before reading pixel data.
    if (gridX < 0 || gridY < 0 || gridX + charWidth > font->atlas_width ||
        gridY + charHeight > font->atlas_height)
        return;
    const uint32_t maxGlyphPixels = 262144;  // 1024x256.
    uint32_t glyphPixels = (uint32_t)(charWidth * charHeight);
    if (glyphPixels == 0 || glyphPixels > maxGlyphPixels) return;

    // Use a static scratch buffer instead of per-glyph allocation.
    static uint32_t* s_croppedBitmap = nullptr;
    static uint32_t s_croppedBitmapSize = 0;
    if (s_croppedBitmapSize < glyphPixels) {
        delete[] s_croppedBitmap;
        s_croppedBitmap = new uint32_t[glyphPixels];
        s_croppedBitmapSize = glyphPixels;
    }
    uint32_t* croppedBitmap = s_croppedBitmap;

    for (int row = 0; row < charHeight; ++row) {
        for (int col = 0; col < charWidth; ++col) {
            uint32_t fontPixel =
                font->font_atlas[(gridY + row) * font->atlas_width + (gridX + col)];
            uint8_t fontAlpha = (fontPixel >> 24) & 0xFF;

            uint8_t inputAlpha = (colorIndex >> 24) & 0xFF;
            uint8_t inputRed = (colorIndex >> 16) & 0xFF;
            uint8_t inputGreen = (colorIndex >> 8) & 0xFF;
            uint8_t inputBlue = colorIndex & 0xFF;

            uint8_t blendedAlpha = (fontAlpha * inputAlpha) / 255;
            uint32_t blendedColor =
                (blendedAlpha << 24) | (inputRed << 16) | (inputGreen << 8) | inputBlue;

            croppedBitmap[row * charWidth + col] = blendedColor;
        }
    }

    // Apply the offsets from the glyph table.
    DrawBitmap(buffer, bufferWidth, bufferHeight, x + xoffset, y + yoffset, croppedBitmap,
               charWidth, charHeight);
}

void NINA::DrawString(uint32_t* buffer, int32_t bufferWidth, int32_t bufferHeight, int32_t x,
                      int32_t y, const char* str, Font* font, uint32_t colorIndex) {
    if (!str || !font || !font->font_glyphs) return;
    int penX = x;
    int penY = y;

    for (int i = 0; str[i] != '\0'; ++i) {
        uint32_t c = (uint8_t)str[i];

        // Handle a newline.
        if (c == '\n') {
            penX = x;
            penY += font->getLineHeight();
            continue;
        }

        // Clamp unsupported characters (subtraction-based to avoid overflow).
        uint32_t next_c = (uint8_t)str[i + 1];
        if (c < font->firstChar || c - font->firstChar >= (uint32_t)font->glyph_count) {
            c = (font->firstChar <= '?' &&
                 (uint32_t)('?' - font->firstChar) < (uint32_t)font->glyph_count)
                    ? '?'
                    : font->firstChar;
        }

        int16_t* g = &font->font_glyphs[(c - font->firstChar) * 8];
        int xadvance = g[7];

        // Kerning lookup.
        int kernAdjust = 0;
        if (font->font_kernings) {
            for (int k = 0; k < font->font_kerning_count; k++) {
                int16_t* kdata = &font->font_kernings[k * 3];
                if (kdata[0] == (int16_t)c && kdata[1] == (int16_t)next_c) {
                    kernAdjust = kdata[2];
                    break;
                }
            }
        }

        // Draw this character at the current pen position.
        DrawCharacter(buffer, bufferWidth, bufferHeight, penX, penY, c, font, colorIndex);

        // Advance the pen.
        penX += xadvance + kernAdjust;
    }
}
