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

#ifndef TERMINAL_VIEW_H
#define TERMINAL_VIEW_H

#include <Hx86/Hgui/widget.h>

/** class TerminalView - Widget displaying multi-line terminal output.
 *
 * Mirrors a kernel-side terminal view; scroll metadata and scroll actions are
 * exchanged directly with the kernel-side scroll buffer.
 */
class TerminalView : public Widget {
private:
    const char* text;
    FontSize fontSize;

public:
    TerminalView(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text);
    ~TerminalView();

    bool setText(const char* text);
    bool setSize(FontSize size);
    bool setScrollMeta(int32_t totalLines, int32_t visibleLines, int32_t scrollOffset);
    int32_t consumeScrollAction();
};

#endif  // TERMINAL_VIEW_H
