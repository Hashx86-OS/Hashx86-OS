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

#define KDBG_COMPONENT "GUI:RADIO"
#include <gui/event.h>
#include <gui/guiplat.h>
#include <gui/radiobutton.h>
#include <gui/widgetHost.h>

RadioButton::RadioButton(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h,
                         const char* label, uint32_t group)
    : Widget(parent, x, y, w, h), checked(false), group(group) {
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;
    const char* text = label ? label : "";
    this->label = new char[strlen(text) + 1];
    if (!this->label) {
        guiplat::Abort("CRITICAL: Failed to allocate radio label!\n");
    }
    strcpy(this->label, text);
}

RadioButton::~RadioButton() {
    if (label) delete[] label;
    if (font) delete font;
}

void RadioButton::SetChecked(bool value) {
    if (checked == value) return;
    checked = value;
    MarkDirty();
}

void RadioButton::Select() {
    if (checked) return;

    // Clear every sibling radio button in the same group.
    if (parent) {
        parent->childrenList.ForEach([&](Widget* child) {
            if (child != this && child->IsRadioButton()) {
                RadioButton* sibling = static_cast<RadioButton*>(child);
                if (sibling->GetGroup() == group) {
                    sibling->SetChecked(false);
                }
            }
        });
    }

    checked = true;
    MarkDirty();
    if (g_widgetHost && PID != 0) {
        g_widgetHost->PostEvent(PID, ID, ON_CLICK, 1);
        g_widgetHost->WakeProcess(PID);
    }
}

void RadioButton::OnMouseDown(int32_t x, int32_t y, uint8_t button) {
    if (!enabled || !isVisible || button != 1) return;
    if (!ContainsCoordinate(x, y)) return;
    Widget::OnMouseDown(x, y, button);
    pressed = true;
}

void RadioButton::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    if (button != 1) return;
    bool wasPressed = pressed;
    pressed = false;
    if (!enabled || !isVisible || !wasPressed) return;
    if (ContainsCoordinate(x, y)) {
        Select();
    }
}

bool RadioButton::IsPressed() const {
    return pressed;
}

void RadioButton::OnKeyDown(const char* key) {
    if (!enabled || !isVisible || !key) return;
    if (key[0] == ' ' || key[0] == '\r' || key[0] == '\n') {
        Select();
    }
}

void RadioButton::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }
    memset(cache, 0, sizeof(uint32_t) * w * h);
    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }

    const int32_t d = 16;
    int32_t cy = h / 2;
    NINA::activeInstance->FillCircle(cache, w, h, 2 + d / 2, cy, d / 2, CHECKBOX_BOX_BG);
    NINA::activeInstance->DrawCircle(cache, w, h, 2 + d / 2, cy, d / 2, CHECKBOX_BOX_BORDER);
    if (checked) {
        NINA::activeInstance->FillCircle(cache, w, h, 2 + d / 2, cy, d / 2 - 3, CHECKBOX_MARK);
    }

    if (font && label) {
        int32_t ty = (h - (int32_t)font->getLineHeight()) / 2;
        NINA::activeInstance->DrawString(cache, w, h, d + 6, ty, label, font, CHECKBOX_TEXT);
    }
    isDirty = false;
}
