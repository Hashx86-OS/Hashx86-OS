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
#include <gui/mathutil.h>
#include <gui/slider.h>
#include <gui/widgetHost.h>

namespace {
constexpr int32_t kThumbSize = 12;
}

Slider::Slider(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h)
    : Widget(parent, x, y, w, h),
      minValue(0),
      maxValue(100),
      value(0),
      dragging(false),
      dragOffset(kThumbSize / 2) {}

int32_t Slider::UsableWidth() const {
    int32_t usable = w - kThumbSize;
    return usable > 0 ? usable : 0;
}

/** ThumbX() - Left edge of the thumb in widget-local pixels. */
int32_t Slider::ThumbX() const {
    uint32_t span = (uint32_t)((int64_t)maxValue - minValue);
    int32_t usable = UsableWidth();
    if (span == 0 || usable <= 0) return 0;

    uint64_t offset = (uint64_t)((int64_t)value - minValue);
    uint32_t pos = GuiMulDivU32(offset * (uint64_t)usable, span);
    if (pos > (uint32_t)usable) return usable;
    return (int32_t)pos;
}

/** ValueForThumbX() - Value selected when the thumb's left edge is at @thumbX. */
int32_t Slider::ValueForThumbX(int32_t thumbX) const {
    uint64_t span = (uint64_t)((int64_t)maxValue - minValue);
    int32_t usable = UsableWidth();
    if (span == 0 || usable <= 0) return minValue;

    if (thumbX < 0) thumbX = 0;
    if (thumbX > usable) thumbX = usable;
    uint32_t offset = GuiMulDivU32((uint64_t)thumbX * span, (uint32_t)usable);
    int64_t result = (int64_t)minValue + (int64_t)offset;
    if (result > maxValue) result = maxValue;
    return (int32_t)result;
}

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

    // x/y arrive in the parent's frame; convert to this widget's local space.
    // Treating them as local made a click land further right by exactly the
    // widget's x offset (the slider "jumped right" under the cursor).
    int32_t localX = x - this->x;
    int32_t thumbX = ThumbX();

    if (localX >= thumbX && localX < thumbX + kThumbSize) {
        // Press on the thumb: keep the value and remember where it was grabbed.
        dragOffset = localX - thumbX;
    } else {
        // Press on the track: centre the thumb on the click, then drag.
        dragOffset = kThumbSize / 2;
        SetValue(ValueForThumbX(localX - dragOffset));
    }

    dragging = true;
    SetMouseCaptured(true);
}

void Slider::OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) {
    (void)oldx;
    (void)oldy;
    (void)newy;
    if (!dragging || !enabled || !isVisible) return;

    int32_t localX = newx - this->x;
    SetValue(ValueForThumbX(localX - dragOffset));
}

void Slider::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    (void)x;
    (void)y;
    if (button != 1) return;
    dragging = false;
    SetMouseCaptured(false);
}

void Slider::OnMouseWheel(int32_t delta) {
    if (!enabled || !isVisible) return;
    // Clamp against the range without computing value + delta, which would
    // overflow for a hostile wheel delta.
    if (delta > 0) {
        int64_t next = (int64_t)value + delta;
        SetValue(next > maxValue ? maxValue : (int32_t)next);
    } else {
        int64_t next = (int64_t)value + delta;
        SetValue(next < minValue ? minValue : (int32_t)next);
    }
}

void Slider::OnSpecialKeyDown(uint8_t key) {
    if (!enabled || !isVisible) return;

    // Step by a tenth of the range. The difference of two int32s fits in an
    // unsigned 32-bit value, and 32-bit division avoids a libgcc helper.
    uint32_t span = (uint32_t)((int64_t)maxValue - minValue);
    int32_t step = (int32_t)(span / 10u);
    if (step < 1) step = 1;

    auto moveBy = [&](int64_t delta) {
        int64_t next = (int64_t)value + delta;
        if (next < minValue) next = minValue;
        if (next > maxValue) next = maxValue;
        SetValue((int32_t)next);
    };

    switch (key) {
        case 0x4B:  // Left.
        case 0x50:  // Down.
            moveBy(-1);
            break;
        case 0x4D:  // Right.
        case 0x48:  // Up.
            moveBy(1);
            break;
        case 0x49:  // Page up.
            moveBy(step);
            break;
        case 0x51:  // Page down.
            moveBy(-step);
            break;
        case 0x47:  // Home.
            SetValue(minValue);
            break;
        case 0x4F:  // End.
            SetValue(maxValue);
            break;
        default:
            break;
    }
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

    int32_t thumbX = ThumbX();
    NINA::activeInstance->FillRoundedRectangle(cache, w, h, thumbX, h / 2 - kThumbSize / 2,
                                               kThumbSize, kThumbSize, kThumbSize / 2,
                                               SLIDER_THUMB);
    isDirty = false;
}
