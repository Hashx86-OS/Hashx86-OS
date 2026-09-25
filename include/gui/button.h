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

#ifndef BUTTON_H
#define BUTTON_H

#include <core/scheduler.h>
#include <gui/desktop.h>
#include <gui/widget.h>

/**
 * class Button - A clickable button widget with label text.
 *
 * Renders a label centered on the widget, tracks press/hover state, and fires
 * a click event to the owning process when the mouse button is released over
 * the button.
 */
class Button : public Widget {
public:
    /**
     * Button() - Construct a button with a label.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Button width in pixels.
     * @h: Button height in pixels.
     * @label: Initial button label, copied into the backing buffer.
     */
    Button(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* label);

    /**
     * ~Button() - Release the label buffer.
     */
    ~Button();

    /** update() - Repaint the button cache and redraw it. */
    void update();

    /**
     * SetLabel() - Replace the button label and repaint.
     * @label: New label, copied into the backing buffer.
     */
    void SetLabel(const char* label);

    /**
     * SetWidth() - Resize the button horizontally.
     * @w: New width in pixels.
     */
    void SetWidth(int32_t w);

    /**
     * SetHeight() - Resize the button vertically.
     * @h: New height in pixels.
     */
    void SetHeight(int32_t h);

    /**
     * SetFontSize() - Set the label text size.
     * @px: Font size in pixels.
     */
    void SetFontSize(int32_t px);

    /** RedrawToCache() - Paint the button face and label with state shading. */
    void RedrawToCache() override;

    /** IsButton() - Return true for button widget types. */
    bool IsButton() const override {
        return true;
    }

    // Mouse event handlers.
    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) override;

    /** OnKeyDown() - Trigger the click when Enter or Space is pressed. */
    void OnKeyDown(const char* key) override;

    /**
     * IsPressed() - Return whether the button is currently pressed.
     *
     * Returns the transient press state; distinct from the persistent cached
     * state used for Z-order handling in composites.
     */
    bool IsPressed() const override;

protected:
    /** EmitClickEvent() - Send a click event to the owning process. */
    void EmitClickEvent();

    char* label;
    bool isPressed;
    bool isHovered = false;
};

#endif  // BUTTON_H
