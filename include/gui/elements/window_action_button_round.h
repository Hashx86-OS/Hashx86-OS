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

#ifndef ACR_BUTTON_H
#define ACR_BUTTON_H

#include <gui/elements/window_action_button.h>

/**
 * class ACRButton - An action button with a rounded, circular appearance.
 *
 * Used for window title-bar controls; only the drawing differs from ACButton.
 */
class ACRButton : public ACButton {
public:
    /**
     * ACRButton() - Construct a round action button.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @label: Button label text.
     */
    ACRButton(Widget* parent, int32_t x, int32_t y, const char* label);

    /**
     * ~ACRButton() - Destroy the round action button.
     */
    ~ACRButton();

    /** RedrawToCache() - Paint the circular button face with state shading. */
    void RedrawToCache() override;
};

#endif  // ACR_BUTTON_H
