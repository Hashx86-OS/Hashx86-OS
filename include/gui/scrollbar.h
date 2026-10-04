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

#ifndef SCROLLBAR_H
#define SCROLLBAR_H

#include <gui/widget.h>

/**
 * class ScrollBar - A draggable position indicator for a scrolling viewport.
 *
 * Owns a [min, max) value range and a page size. The thumb covers the fraction
 * of the range equal to the page. Dragging the thumb, clicking the track, or
 * scrolling over the bar changes the value and emits ON_CLICK with param1 set
 * to the new value.
 */
class ScrollBar : public Widget {
private:
    int32_t minValue;
    int32_t maxValue;
    int32_t value;
    int32_t pageSize;
    bool vertical;
    bool dragging;
    int32_t dragStart;
    int32_t dragValue;

    int32_t TrackLength() const;
    int32_t ThumbLength() const;
    int32_t MaxValue() const;

    void EmitValue();

public:
    /**
     * ScrollBar() - Construct a scrollbar.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Widget width in pixels.
     * @h: Widget height in pixels.
     * @vertical: True for a vertical bar, false for horizontal.
     */
    ScrollBar(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, bool vertical = false);

    /** SetRange() - Set the range and page size; clamps the value. */
    void SetRange(int32_t min, int32_t max, int32_t page);

    /** SetValue() - Set the value, clamped; emits when it changes. */
    void SetValue(int32_t v);

    /** GetValue() - Return the current value. */
    int32_t GetValue() const {
        return value;
    }

    void RedrawToCache() override;
    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseWheel(int32_t delta) override;
};

#endif  // SCROLLBAR_H
