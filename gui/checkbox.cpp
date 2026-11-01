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

#define KDBG_COMPONENT "GUI:CHECKBOX"
#include <gui/checkbox.h>
#include <gui/event.h>
#include <gui/guiplat.h>
#include <gui/widgetHost.h>

CheckBox::CheckBox(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* label)
    : Widget(parent, x, y, w, h), checked(false) {
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;
    const char* text = label ? label : "";
    this->label = new char[strlen(text) + 1];
    if (!this->label) {
        guiplat::Abort("CRITICAL: Failed to allocate checkbox label!\n");
    }
    strcpy(this->label, text);
}

CheckBox::~CheckBox() {
    if (label) delete[] label;
    if (font) delete font;
}

void CheckBox::SetChecked(bool value) {
    if (checked == value) return;
    checked = value;
    MarkDirty();
}

void CheckBox::Toggle() {
    checked = !checked;
    MarkDirty();
    if (g_widgetHost && PID != 0) {
        g_widgetHost->PostEvent(PID, ID, ON_CLICK, checked ? 1 : 0);
        g_widgetHost->WakeProcess(PID);
    }
}

void CheckBox::OnMouseDown(int32_t x, int32_t y, uint8_t button) {
    if (!enabled || !isVisible || button != 1) return;
    if (!ContainsCoordinate(x, y)) return;
    Widget::OnMouseDown(x, y, button);
    pressed = true;
}

void CheckBox::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    if (button != 1) return;
    // Require a press that started on this widget: a release that merely lands
    // here (for example after dragging a slider) must not toggle anything.
    bool wasPressed = pressed;
    pressed = false;
    if (!enabled || !isVisible || !wasPressed) return;
    if (ContainsCoordinate(x, y)) {
        Toggle();
    }
}

bool CheckBox::IsPressed() const {
    return pressed;
}

void CheckBox::OnKeyDown(const char* key) {
    if (!enabled || !isVisible || !key) return;
    if (key[0] == ' ' || key[0] == '\r' || key[0] == '\n') {
        Toggle();
    }
}

void CheckBox::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }
    memset(cache, 0, sizeof(uint32_t) * w * h);
    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }

    const int32_t box = 16;
    int32_t by = (h - box) / 2;
    NINA::activeInstance->FillRectangle(cache, w, h, 2, by, box, box, CHECKBOX_BOX_BG);
    NINA::activeInstance->DrawRectangle(cache, w, h, 2, by, box, box, CHECKBOX_BOX_BORDER);

    if (checked) {
        // A simple check mark drawn with two thick line segments.
        for (int32_t i = 0; i < 4; i++) {
            NINA::activeInstance->DrawLine(cache, w, h, 5, by + 8 + i, 8, by + 11 + i,
                                           CHECKBOX_MARK);
            NINA::activeInstance->DrawLine(cache, w, h, 8, by + 11 + i, 13, by + 4 + i,
                                           CHECKBOX_MARK);
        }
    }

    if (font && label) {
        int32_t ty = (h - (int32_t)font->getLineHeight()) / 2;
        NINA::activeInstance->DrawString(cache, w, h, box + 6, ty, label, font, CHECKBOX_TEXT);
    }
    isDirty = false;
}
