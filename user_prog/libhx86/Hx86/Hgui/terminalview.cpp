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

#include <Hx86/Hgui/terminalview.h>

TerminalView::TerminalView(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h,
                           const char* text)
    : Widget(parent, x, y, w, h), text(text), fontSize(TINY) {
    WidgetData data = {parent->ID, x, y, (uint32_t)w, (uint32_t)h, text};
    this->ID = HguiAPI(TERMINAL_VIEW, CREATE, (void*)&data);
}

TerminalView::~TerminalView() {}

bool TerminalView::setText(const char* text) {
    this->text = text;
    WidgetData data = {ID, 0, 0, 0, 0, text};
    return HguiAPI(TERMINAL_VIEW, SET_TEXT, (void*)&data);
}

bool TerminalView::setSize(FontSize size) {
    this->fontSize = size;
    WidgetData data = {ID, (int32_t)fontSize};
    return HguiAPI(TERMINAL_VIEW, SET_FONT_SIZE, (void*)&data);
}

bool TerminalView::setScrollMeta(int32_t totalLines, int32_t visibleLines, int32_t scrollOffset) {
    WidgetData data = {ID, totalLines, visibleLines, (uint32_t)scrollOffset, 0, nullptr};
    return HguiAPI(TERMINAL_VIEW, SET_SCROLL_META, (void*)&data);
}

int32_t TerminalView::consumeScrollAction() {
    WidgetData data = {ID, 0, 0, 0, 0, nullptr};
    return (int32_t)HguiAPI(TERMINAL_VIEW, GET_SCROLL_ACTION, (void*)&data);
}
