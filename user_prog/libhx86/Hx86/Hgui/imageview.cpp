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

#include <Hx86/Hgui/imageview.h>

HImageView::HImageView(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h)
    : Widget(parent, x, y, w, h) {
    WidgetData data = {parent->ID, x, y, w, h, nullptr};
    this->ID = HguiAPI(IMAGEVIEW, CREATE, (void*)&data);
}

HImageView::~HImageView() {}

bool HImageView::SetImage(const uint32_t* pixels, int32_t srcW, int32_t srcH) {
    WidgetData data = {this->ID, srcW, srcH, 0, 0, (const char*)pixels};
    return (int32_t)HguiAPI(IMAGEVIEW, SET_IMAGE, (void*)&data) == 1;
}
