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

#define KDBG_COMPONENT "GUI:IMAGEVIEW"
#include <gui/imageview.h>

ImageView::ImageView(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h,
                     const uint32_t* source, int32_t srcW, int32_t srcH)
    : Widget(parent, x, y, w, h), pixels(nullptr), srcW(0), srcH(0) {
    this->isFocussable = false;
    SetImage(source, srcW, srcH);
}

ImageView::~ImageView() {
    if (pixels) delete[] pixels;
}

void ImageView::SetImage(const uint32_t* source, int32_t newSrcW, int32_t newSrcH) {
    if (pixels) {
        delete[] pixels;
        pixels = nullptr;
    }
    srcW = 0;
    srcH = 0;
    if (!source || newSrcW <= 0 || newSrcH <= 0) {
        MarkDirty();
        return;
    }
    size_t count = (size_t)newSrcW * (size_t)newSrcH;
    pixels = new uint32_t[count];
    if (!pixels) {
        MarkDirty();
        return;
    }
    memcpy(pixels, source, count * sizeof(uint32_t));
    srcW = newSrcW;
    srcH = newSrcH;
    MarkDirty();
}

void ImageView::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }
    memset(cache, 0, sizeof(uint32_t) * w * h);
    if (!pixels || srcW <= 0 || srcH <= 0 || w <= 0 || h <= 0) {
        isDirty = false;
        return;
    }

    // Nearest-neighbor scale from the source into the widget cache.
    for (int32_t dy = 0; dy < h; dy++) {
        int32_t sy = (int32_t)(dy * srcH / h);
        if (sy >= srcH) sy = srcH - 1;
        for (int32_t dx = 0; dx < w; dx++) {
            int32_t sx = (int32_t)(dx * srcW / w);
            if (sx >= srcW) sx = srcW - 1;
            cache[dy * w + dx] = pixels[sy * srcW + sx];
        }
    }
    isDirty = false;
}
