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

#define KDBG_COMPONENT "GUI:SPINBOX"
#include <gui/event.h>
#include <gui/spinbox.h>
#include <gui/widgetHost.h>

namespace {

void IntToText(int32_t v, char* out) {
    char tmp[12];
    int n = 0;
    bool neg = v < 0;
    uint32_t u = neg ? (uint32_t)(-v) : (uint32_t)v;
    if (u == 0) tmp[n++] = '0';
    while (u > 0) {
        tmp[n++] = (char)('0' + (u % 10));
        u /= 10;
    }
    int p = 0;
    if (neg) out[p++] = '-';
    for (int i = n - 1; i >= 0; i--) out[p++] = tmp[i];
    out[p] = '\0';
}

}  // namespace

SpinBox::SpinBox(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h)
    : Widget(parent, x, y, w, h), minValue(0), maxValue(100), step(1), value(0) {
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;
}

void SpinBox::SetRange(int32_t min, int32_t max, int32_t stepValue) {
    if (max < min) {
        int32_t t = min;
        min = max;
        max = t;
    }
    minValue = min;
    maxValue = max;
    step = (stepValue < 1) ? 1 : stepValue;
    SetValue(value);
    MarkDirty();
}

void SpinBox::EmitValue() {
    if (g_widgetHost && PID != 0) {
        g_widgetHost->PostEvent(PID, ID, ON_CLICK, (uint32_t)value);
        g_widgetHost->WakeProcess(PID);
    }
}

void SpinBox::SetValue(int32_t v) {
    if (v < minValue) v = minValue;
    if (v > maxValue) v = maxValue;
    if (v == value) return;
    value = v;
    MarkDirty();
    EmitValue();
}

void SpinBox::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    if (!enabled || !isVisible) return;
    if (button != 1) return;
    if (!ContainsCoordinate(x, y)) return;
    int32_t local = y - this->y;
    if (local < h / 2) {
        SetValue(value + step);
    } else {
        SetValue(value - step);
    }
}

void SpinBox::OnMouseWheel(int32_t delta) {
    if (!enabled || !isVisible) return;
    SetValue(value + delta * step);
}

void SpinBox::OnSpecialKeyDown(uint8_t key) {
    if (!enabled || !isVisible) return;
    if (key == 0x48) {  // Up arrow.
        SetValue(value + step);
    } else if (key == 0x50) {  // Down arrow.
        SetValue(value - step);
    }
}

void SpinBox::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }
    memset(cache, 0, sizeof(uint32_t) * w * h);
    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }

    NINA::activeInstance->FillRoundedRectangle(cache, w, h, 0, 0, w, h, 3, BUTTON_BG_NORMAL);
    NINA::activeInstance->DrawRoundedRectangle(cache, w, h, 0, 0, w, h, 3, BUTTON_BORDER_NORMAL);

    char text[12];
    IntToText(value, text);
    if (font) {
        int32_t tw = (int32_t)font->getStringLength(text);
        int32_t th = (int32_t)font->getLineHeight();
        NINA::activeInstance->DrawString(cache, w, h, (w - tw) / 2, (h - th) / 2, text, font,
                                         BUTTON_TEXT_NORMAL);
    }

    // Up/down chevrons on the right edge.
    int32_t cx = w - 10;
    for (int32_t i = 0; i < 3; i++) {
        NINA::activeInstance->DrawLine(cache, w, h, cx - i, h / 4 - 2 + i, cx + i, h / 4 - 2 + i,
                                       BUTTON_TEXT_NORMAL);
        NINA::activeInstance->DrawLine(cache, w, h, cx - i, 3 * h / 4 + 2 - i, cx + i,
                                       3 * h / 4 + 2 - i, BUTTON_TEXT_NORMAL);
    }
    isDirty = false;
}
