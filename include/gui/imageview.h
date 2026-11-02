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

#ifndef IMAGEVIEW_H
#define IMAGEVIEW_H

#include <gui/widget.h>

/**
 * class ImageView - Displays a 0xAARRGGBB bitmap scaled to fit its bounds.
 *
 * The view copies the source pixels into its own buffer, so the caller may
 * free or reuse the source after SetImage(). It takes no focus and ignores
 * input.
 */
class ImageView : public Widget {
private:
    uint32_t* pixels;
    int32_t srcW;
    int32_t srcH;

public:
    /**
     * ImageView() - Construct an image view.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Widget width in pixels.
     * @h: Widget height in pixels.
     * @source: Initial pixel buffer (0xAARRGGBB), or NULL.
     * @srcW: Source width in pixels.
     * @srcH: Source height in pixels.
     */
    ImageView(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const uint32_t* source,
              int32_t srcW, int32_t srcH);

    /** ~ImageView() - Release the copied pixel buffer. */
    ~ImageView();

    /**
     * SetImage() - Replace the displayed image, copying @source.
     * @source: New pixel buffer, or NULL to clear.
     * @srcW: Source width in pixels.
     * @srcH: Source height in pixels.
     */
    void SetImage(const uint32_t* source, int32_t srcW, int32_t srcH);

    void RedrawToCache() override;
};

#endif  // IMAGEVIEW_H
