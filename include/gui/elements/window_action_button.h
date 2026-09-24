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

#ifndef AC_BUTTON_H
#define AC_BUTTON_H

#include <gui/button.h>

/**
 * class ACButton - A button that fires a caller-supplied callback on release.
 *
 * An action button emits either a free function callback or a member-function
 * callback bound to a caller object when the mouse is released over it.
 */
class ACButton : public Button {
public:
    /**
     * ACButton() - Construct an action button.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @label: Button label text.
     */
    ACButton(Widget* parent, int32_t x, int32_t y, const char* label);

    /**
     * ~ACButton() - Destroy the action button.
     */
    virtual ~ACButton();

    // Mouse event handlers.
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;

    // Callbacks.

    /**
     * OnClick() - Register a free-function click callback.
     * @callback: Function to run on click.
     */
    void OnClick(void (*callback)());

    /**
     * OnClick() - Register a member-function click callback with an instance.
     * @instance: Object on which to call @callback.
     * @callback: Member function to run on click.
     */
    void OnClick(void* instance, void (*callback)(void*));

protected:
    // Function pointer for the non-member callback.
    void (*onClickPtr)();

    // Member function pointer handling.
    void* callbackInstance;
    void (*onClickMemberPtr)(void*);
};

#endif  // AC_BUTTON_H
