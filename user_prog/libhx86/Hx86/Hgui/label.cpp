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

#include <Hx86/Hgui/label.h>

Label::Label(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text)
    : Widget(parent, x, y, w, h), text(text) {
    WidgetData data = {parent->ID, x, y, w, h, text};
    this->ID = HguiAPI(LABEL, CREATE, (void*)&data);
}

Label::~Label() {}

bool Label::setText(const char* text) {
    this->text = text;
    WidgetData data = {ID, 0, 0, 0, 0, text};
    return HguiAPI(LABEL, SET_TEXT, (void*)&data);
}

bool Label::setSize(FontSize size) {
    this->fontSize = size;
    WidgetData data = {ID, (int32_t)fontSize};
    return HguiAPI(LABEL, SET_FONT_SIZE, (void*)&data);
}

bool Label::setType(FontType type) {
    WidgetData data = {ID, (int32_t)type};
    return HguiAPI(LABEL, SET_FONT_TYPE, (void*)&data);
}

bool Label::setColor(uint32_t argb) {
    WidgetData data = {ID, (int32_t)argb};
    return HguiAPI(LABEL, SET_COLOR, (void*)&data);
}

/** PxToFontSlot() - Convert a pixel size to a FontSize enum value.
 * @px: Font size in pixels.
 *
 * Matches the kernel-side Font::PixelToFontSlot() mapping.
 *
 * Return: The closest FontSize slot for the given pixel size.
 */
static FontSize PxToFontSlot(int32_t px) {
    if (px <= 18) return TINY;
    if (px <= 22) return SMALL;
    if (px <= 27) return MEDIUM;
    if (px <= 34) return LARGE;
    return XLARGE;
}

bool Label::setFontSize(int32_t px) {
    WidgetData data = {ID, (int32_t)PxToFontSlot(px)};
    return HguiAPI(LABEL, SET_FONT_SIZE, (void*)&data);
}

bool Label::setAlignment(HAlign ha, VAlign va) {
    WidgetData data = {ID, (int32_t)ha, (int32_t)va};
    return HguiAPI(LABEL, SET_ALIGNMENT, (void*)&data);
}
