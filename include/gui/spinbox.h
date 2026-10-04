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

#ifndef SPINBOX_H
#define SPINBOX_H

#include <gui/widget.h>

/**
 * class SpinBox - A numeric field with up/down steppers.
 *
 * Clicking the upper half increments by the step, the lower half decrements,
 * the wheel steps either way, and Up/Down special keys step when focused.
 * Every change emits ON_CLICK with param1 set to the new value.
 */
class SpinBox : public Widget {
private:
    int32_t minValue;
    int32_t maxValue;
    int32_t step;
    int32_t value;

    void EmitValue();

public:
    /**
     * SpinBox() - Construct a spin box.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Widget width in pixels.
     * @h: Widget height in pixels.
     */
    SpinBox(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h);

    /** SetRange() - Set the inclusive range and step; clamps the value. */
    void SetRange(int32_t min, int32_t max, int32_t step = 1);

    /** SetValue() - Set the value, clamped; emits when it changes. */
    void SetValue(int32_t v);

    /** GetValue() - Return the current value. */
    int32_t GetValue() const {
        return value;
    }

    void RedrawToCache() override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseWheel(int32_t delta) override;
    void OnSpecialKeyDown(uint8_t key) override;
};

#endif  // SPINBOX_H
