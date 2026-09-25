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

#include <Hx86/Hgui/iconbutton.h>

IconButton::IconButton(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h,
                       const char* iconName)
    : Button(parent, x, y, w, h, "") {
    WidgetData data = {parent->ID, x, y, w, h, iconName, (char*)"", nullptr};
    uint32_t newID = HguiAPI(ICON_BUTTON, CREATE, (void*)&data);
    if (newID != UINT32_MAX) {
        HguiAPI(BUTTON, DELETE, (void*)&this->ID);
        this->ID = newID;
    }
}

IconButton::IconButton(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h,
                       const char* iconName, const char* labelText)
    : Button(parent, x, y, w, h, labelText ? labelText : "") {
    WidgetData data = {parent->ID, x, y, w, h, iconName, (char*)(labelText ? labelText : ""),
                       nullptr};
    uint32_t newID = HguiAPI(ICON_BUTTON, CREATE, (void*)&data);
    if (newID != UINT32_MAX) {
        HguiAPI(BUTTON, DELETE, (void*)&this->ID);
        this->ID = newID;
    }
}

IconButton::~IconButton() {}

bool IconButton::setIcon(const char* iconName) {
    WidgetData data = {ID, 0, 0, 0, 0, iconName, nullptr, nullptr};
    return HguiAPI(ICON_BUTTON, SET_ICON, (void*)&data);
}

bool IconButton::setIconFontSize(int32_t px) {
    WidgetData data = {ID, px, 0, 0, 0, nullptr, nullptr, nullptr};
    return HguiAPI(ICON_BUTTON, SET_ICON_FONT_SIZE, (void*)&data);
}
