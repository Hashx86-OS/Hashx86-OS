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

#ifndef RADIOBUTTON_H
#define RADIOBUTTON_H

#include <gui/widget.h>

/**
 * class RadioButton - A labelled selector that is mutually exclusive within a
 * group of siblings.
 *
 * Selecting a radio button clears every sibling radio button that shares its
 * group id and emits ON_CLICK. Clicking or pressing Space/Enter selects it.
 */
class RadioButton : public Widget {
private:
    char* label;
    bool checked;
    bool pressed = false;
    uint32_t group;

    void Select();

public:
    /**
     * RadioButton() - Construct a radio button.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Widget width in pixels.
     * @h: Widget height in pixels.
     * @label: Label text, copied into the backing buffer.
     * @group: Group id; only siblings with the same id are mutually exclusive.
     */
    RadioButton(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* label,
                uint32_t group = 0);

    /** ~RadioButton() - Release the label buffer. */
    ~RadioButton();

    /** GetChecked() - Return the selected state. */
    bool GetChecked() const {
        return checked;
    }

    /** GetGroup() - Return the group id. */
    uint32_t GetGroup() const {
        return group;
    }

    /** SetChecked() - Set the state without emitting an event. */
    void SetChecked(bool value);

    bool IsRadioButton() const override {
        return true;
    }

    void RedrawToCache() override;
    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    void OnKeyDown(const char* key) override;
    bool IsPressed() const override;
};

#endif  // RADIOBUTTON_H
