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

#ifndef SCROLLCONTAINER_H
#define SCROLLCONTAINER_H

#include <gui/scrollbar.h>
#include <gui/widget.h>

/**
 * class ScrollContainer - A viewport over a larger scrollable content area.
 *
 * Owns a content composite that can be larger than the viewport, plus vertical
 * and horizontal scrollbars. Wheel input scrolls the content by moving it
 * behind the viewport; scrollbars are kept in range as the content size changes.
 */
class ScrollContainer : public CompositeWidget {
private:
    static const int BAR = 12;
    CompositeWidget* content;
    ScrollBar* vbar;
    ScrollBar* hbar;
    int32_t contentW;
    int32_t contentH;

    void SyncBars();

public:
    /**
     * ScrollContainer() - Construct a scroll container.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Viewport width in pixels.
     * @h: Viewport height in pixels.
     */
    ScrollContainer(CompositeWidget* parent, int32_t x, int32_t y, int32_t w, int32_t h);

    /**
     * SetContentSize() - Set the scrollable content extent.
     * @cw: Content width in pixels.
     * @ch: Content height in pixels.
     */
    void SetContentSize(int32_t cw, int32_t ch);

    /** GetContent() - Return the composite that hosts content widgets. */
    CompositeWidget* GetContent() {
        return content;
    }

    /** GetScrollX() - Return the horizontal scroll offset. */
    int32_t GetScrollX() const;

    /** GetScrollY() - Return the vertical scroll offset. */
    int32_t GetScrollY() const;

    void OnMouseWheel(int32_t delta) override;
};

#endif  // SCROLLCONTAINER_H
