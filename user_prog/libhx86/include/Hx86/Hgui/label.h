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

#ifndef LABEL_H
#define LABEL_H

#include <Hx86/Hgui/widget.h>

// Horizontal alignment of the label text within its widget bounds.
enum HAlign { LEFT = 0, CENTER = 1, RIGHT = 2 };
// Vertical alignment of the label text within its widget bounds.
enum VAlign { TOP = 0, MIDDLE = 1, BOTTOM = 2 };

/** class Label - Widget that displays a single text string. */
class Label : public Widget {
private:
    const char* text;
    FontSize fontSize;

public:
    /** Label() - Create a label widget.
     * @parent: Parent widget this label is attached to.
     * @x: Horizontal position relative to the parent.
     * @y: Vertical position relative to the parent.
     * @w: Widget width.
     * @h: Widget height.
     * @text: Text string to display.
     */
    Label(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text);
    ~Label();

    bool setText(const char* text);
    bool setSize(FontSize size);
    bool setType(FontType type);
    bool setColor(uint32_t argb);
    bool setFontSize(int32_t px);
    bool setAlignment(HAlign ha, VAlign va);
};

#endif
