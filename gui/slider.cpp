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

#define KDBG_COMPONENT "GUI:SLIDER"
#include <gui/event.h>
#include <gui/slider.h>
#include <gui/widgetHost.h>

namespace {
constexpr int32_t kThumbSize = 12;
}

Slider::Slider(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h)
    : Widget(parent, x, y, w, h), minValue(0), maxValue(100), value(0), dragging(false) {}

void Slider::SetRange(int32_t min, int32_t max) {
    if (max < min) {
        int32_t t = min;
        min = max;
        max = t;
    }
    minValue = min;
    maxValue = max;
    SetValue(value);
    MarkDirty();
}

void Slider::EmitValue() {
    if (g_widgetHost && PID != 0) {
        g_widgetHost->PostEvent(PID, ID, ON_CLICK, (uint32_t)value);
        g_widgetHost->WakeProcess(PID);
    }
}

void Slider::SetValue(int32_t v) {
    if (v < minValue) v = minValue;
    if (v > maxValue) v = maxValue;
    if (v == value) return;
    value = v;
    MarkDirty();
    EmitValue();
}

void Slider::OnMouseDown(int32_t x, int32_t y, uint8_t button) {
    if (!enabled || !isVisible) return;
    if (button != 1) return;
    Widget::OnMouseDown(x, y, button);
    dragging = true;

    int32_t span = maxValue - minValue;
    int32_t usable = w - kThumbSize;
    if (usable <= 0) return;
    int32_t rel = x - kThumbSize / 2;
    if (rel < 0) rel = 0;
    if (rel > usable) rel = usable;
    SetValue(minValue + (int32_t)(rel * span / usable));
}

void Slider::OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) {
    (void)oldx;
    (void)oldy;
    if (!dragging) return;

    int32_t span = maxValue - minValue;
    int32_t usable = w - kThumbSize;
    if (usable <= 0) return;
    int32_t rel = newx - kThumbSize / 2;
    if (rel < 0) rel = 0;
    if (rel > usable) rel = usable;
    SetValue(minValue + (int32_t)(rel * span / usable));
    (void)newy;
}

void Slider::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    (void)x;
    (void)y;
    (void)button;
    dragging = false;
}

void Slider::OnMouseWheel(int32_t delta) {
    if (!enabled || !isVisible) return;
    SetValue(value + delta);
}

void Slider::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }
    memset(cache, 0, sizeof(uint32_t) * w * h);
    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }

    int32_t trackY = h / 2 - 2;
    NINA::activeInstance->FillRectangle(cache, w, h, 0, trackY, w, 4, SLIDER_TRACK);

    int32_t span = maxValue - minValue;
    int32_t usable = w - kThumbSize;
    int32_t thumbX = 0;
    if (span > 0 && usable > 0) {
        thumbX = (int32_t)((value - minValue) * usable / span);
    }
    NINA::activeInstance->FillRoundedRectangle(cache, w, h, thumbX, h / 2 - kThumbSize / 2,
                                               kThumbSize, kThumbSize, kThumbSize / 2,
                                               SLIDER_THUMB);
    isDirty = false;
}
