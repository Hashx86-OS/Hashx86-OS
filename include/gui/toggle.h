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

#ifndef TOGGLE_H
#define TOGGLE_H

#include <gui/widget.h>

/**
 * class Toggle - An on/off switch.
 *
 * Clicking the switch, or pressing Space/Enter while focused, flips the state
 * and emits ON_CLICK with param1 set to the new state.
 */
class Toggle : public Widget {
private:
    bool on;
    bool pressed = false;

    void Flip();

public:
    /**
     * Toggle() - Construct a switch.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Widget width in pixels.
     * @h: Widget height in pixels.
     */
    Toggle(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h);

    /** SetOn() - Set the state without emitting an event. */
    void SetOn(bool value);

    /** IsOn() - Return the current state. */
    bool IsOn() const {
        return on;
    }

    void RedrawToCache() override;
    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    void OnKeyDown(const char* key) override;
    bool IsPressed() const override;
};

#endif  // TOGGLE_H
