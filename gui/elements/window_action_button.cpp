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

#define KDBG_COMPONENT "GUI:WINDOW.ACBTN"
#include <gui/elements/window_action_button.h>

ACButton::ACButton(Widget* parent, int32_t x, int32_t y, const char* label)
    : Button(parent, x, y, 0, 0, label)  // Size 0 initially; set by the subclass or SetWidth().
{
    this->onClickPtr = nullptr;
    this->onClickMemberPtr = nullptr;
    this->callbackInstance = nullptr;
}

ACButton::~ACButton() {}

void ACButton::OnClick(void (*callback)()) {
    onClickPtr = callback;
    onClickMemberPtr = nullptr;
    callbackInstance = nullptr;
}

void ACButton::OnClick(void* instance, void (*callback)(void*)) {
    callbackInstance = instance;
    onClickMemberPtr = callback;
    onClickPtr = nullptr;
}

void ACButton::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    // Note: Button::OnMouseUp handles the "isPressed" state change.
    if (isPressed && isVisible) {
        if (!ContainsCoordinate(x, y)) {
            isPressed = false;
            MarkDirty();
            return;
        }
        // Reset the pressed state.
        isPressed = false;
        MarkDirty();

        // Fire the callbacks.
        if (onClickPtr) {
            onClickPtr();
        } else if (onClickMemberPtr && callbackInstance) {
            onClickMemberPtr(callbackInstance);
        }
    }
}
