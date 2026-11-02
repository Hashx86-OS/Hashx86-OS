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

#define KDBG_COMPONENT "GUI:TOGGLE"
#include <gui/event.h>
#include <gui/toggle.h>
#include <gui/widgetHost.h>

Toggle::Toggle(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h)
    : Widget(parent, x, y, w, h), on(false) {}

void Toggle::SetOn(bool value) {
    if (on == value) return;
    on = value;
    MarkDirty();
}

void Toggle::Flip() {
    on = !on;
    MarkDirty();
    if (g_widgetHost && PID != 0) {
        g_widgetHost->PostEvent(PID, ID, ON_CLICK, on ? 1 : 0);
        g_widgetHost->WakeProcess(PID);
    }
}

void Toggle::OnMouseDown(int32_t x, int32_t y, uint8_t button) {
    if (!enabled || !isVisible || button != 1) return;
    if (!ContainsCoordinate(x, y)) return;
    Widget::OnMouseDown(x, y, button);
    pressed = true;
}

void Toggle::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    if (button != 1) return;
    bool wasPressed = pressed;
    pressed = false;
    if (!enabled || !isVisible || !wasPressed) return;
    if (ContainsCoordinate(x, y)) {
        Flip();
    }
}

bool Toggle::IsPressed() const {
    return pressed;
}

void Toggle::OnKeyDown(const char* key) {
    if (!enabled || !isVisible || !key) return;
    if (key[0] == ' ' || key[0] == '\r' || key[0] == '\n') {
        Flip();
    }
}

void Toggle::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }
    memset(cache, 0, sizeof(uint32_t) * w * h);
    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }

    int32_t trackH = (h < 20) ? h : 20;
    int32_t trackY = (h - trackH) / 2;
    uint32_t trackColor = on ? TOGGLE_TRACK_ON : TOGGLE_TRACK_OFF;
    NINA::activeInstance->FillRoundedRectangle(cache, w, h, 0, trackY, w, trackH, trackH / 2,
                                               trackColor);

    int32_t knobD = trackH - 4;
    int32_t knobX = on ? (w - knobD - 2) : 2;
    NINA::activeInstance->FillCircle(cache, w, h, knobX + knobD / 2, trackY + trackH / 2, knobD / 2,
                                     TOGGLE_KNOB);
    isDirty = false;
}
