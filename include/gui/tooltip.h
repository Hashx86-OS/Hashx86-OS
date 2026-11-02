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

#ifndef TOOLTIP_H
#define TOOLTIP_H

#include <gui/widget.h>

/**
 * class Tooltip - A non-interactive label drawn with a boxed background.
 *
 * Tooltips never take focus and ignore all input; they exist only to explain
 * another widget and are shown or hidden by the application.
 */
class Tooltip : public Widget {
private:
    char* text;

public:
    /**
     * Tooltip() - Construct a tooltip.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Widget width in pixels.
     * @h: Widget height in pixels.
     * @text: Initial text, copied into the backing buffer.
     */
    Tooltip(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text);

    /** ~Tooltip() - Release the text buffer. */
    ~Tooltip();

    /** SetText() - Replace the tooltip text. */
    void SetText(const char* text);

    /** Show() - Make the tooltip visible. */
    void Show();

    /** Hide() - Make the tooltip invisible. */
    void Hide();

    void RedrawToCache() override;
};

#endif  // TOOLTIP_H
