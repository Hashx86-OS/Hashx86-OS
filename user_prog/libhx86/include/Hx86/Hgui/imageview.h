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

#ifndef HIMAGEVIEW_H
#define HIMAGEVIEW_H

#include <Hx86/Hgui/widget.h>

/** class HImageView - Displays a scaled 0xAARRGGBB bitmap. */
class HImageView : public Widget {
public:
    /**
     * HImageView() - Create an image view.
     * @parent: Parent widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Widget width.
     * @h: Widget height.
     */
    HImageView(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h);
    ~HImageView();

    /**
     * SetImage() - Copy pixels into the view, or clear it with a null buffer.
     * @pixels: 0xAARRGGBB pixels.
     * @srcW: Source width.
     * @srcH: Source height.
     *
     * Return: true when the kernel accepted the image.
     */
    bool SetImage(const uint32_t* pixels, int32_t srcW, int32_t srcH);
};

#endif  // HIMAGEVIEW_H
