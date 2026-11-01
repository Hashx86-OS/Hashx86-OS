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

#define KDBG_COMPONENT "GUI:SCROLLBAR"
#include <gui/event.h>
#include <gui/mathutil.h>
#include <gui/scrollbar.h>
#include <gui/widgetHost.h>

ScrollBar::ScrollBar(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, bool vertical)
    : Widget(parent, x, y, w, h),
      minValue(0),
      maxValue(100),
      value(0),
      pageSize(10),
      vertical(vertical),
      dragging(false),
      dragStart(0),
      dragValue(0) {}

int32_t ScrollBar::TrackLength() const {
    return vertical ? h : w;
}

int32_t ScrollBar::ThumbLength() const {
    uint64_t span = (uint64_t)((int64_t)maxValue - minValue);
    int32_t track = TrackLength();
    if (span == 0 || track <= 0) return track > 0 ? track : 0;

    // A page at least as large as the range fills the whole track. Capping the
    // ratio first keeps the quotient inside 32 bits for GuiMulDivU32.
    if ((uint64_t)pageSize >= span) return track;

    uint32_t len = GuiMulDivU32((uint64_t)track * (uint64_t)pageSize, (uint32_t)span);
    if (len < 16) len = 16;
    if (len > (uint32_t)track) len = (uint32_t)track;
    return (int32_t)len;
}

int32_t ScrollBar::MaxValue() const {
    // Compute in 64-bit: maxValue - pageSize underflows for a full-width
    // negative range (maxValue == INT32_MIN, pageSize == 1).
    int64_t m = (int64_t)maxValue - pageSize;
    return (m < minValue) ? minValue : (int32_t)m;
}

int32_t ScrollBar::ValueRange() const {
    int64_t r = (int64_t)MaxValue() - minValue;
    if (r < 1) return 1;
    if (r > 2147483647LL) return 2147483647;
    return (int32_t)r;
}

int32_t ScrollBar::ThumbPos() const {
    int32_t track = TrackLength();
    int32_t thumb = ThumbLength();
    if (track <= thumb) return 0;

    int64_t offset = (int64_t)value - minValue;
    if (offset < 0) offset = 0;
    uint32_t pos =
        GuiMulDivU32((uint64_t)offset * (uint64_t)(track - thumb), (uint32_t)ValueRange());
    return (int32_t)pos;
}

void ScrollBar::SetRange(int32_t min, int32_t max, int32_t page) {
    minValue = min;
    maxValue = max;
    pageSize = (page < 1) ? 1 : page;
    if (maxValue < minValue) maxValue = minValue;
    SetValue(value);
    MarkDirty();
}

void ScrollBar::EmitValue() {
    if (g_widgetHost && PID != 0) {
        g_widgetHost->PostEvent(PID, ID, ON_CLICK, (uint32_t)value);
        g_widgetHost->WakeProcess(PID);
    }
}

void ScrollBar::SetValue(int32_t v) {
    if (v < minValue) v = minValue;
    if (v > MaxValue()) v = MaxValue();
    if (v == value) return;
    value = v;
    MarkDirty();
    if (onValueChanged) onValueChanged(this, onValueChangedUser);
    EmitValue();
}

void ScrollBar::OnMouseDown(int32_t x, int32_t y, uint8_t button) {
    if (!enabled || !isVisible) return;
    if (button != 1) return;
    Widget::OnMouseDown(x, y, button);

    // x/y arrive in the parent's frame, so subtract the bar's origin before
    // comparing against the thumb geometry (which is local to the bar).
    int32_t pos = vertical ? (y - this->y) : (x - this->x);

    // If the press is on the thumb, start dragging; otherwise page toward it.
    int32_t thumb = ThumbLength();
    int32_t thumbPos = ThumbPos();

    if (pos >= thumbPos && pos < thumbPos + thumb) {
        dragging = true;
        dragStart = pos;
        dragValue = value;
        SetMouseCaptured(true);
    } else if (pos < thumbPos) {
        int64_t next = (int64_t)value - pageSize;
        SetValue(next < minValue ? minValue : (int32_t)next);
    } else {
        int64_t next = (int64_t)value + pageSize;
        SetValue(next > MaxValue() ? MaxValue() : (int32_t)next);
    }
}

void ScrollBar::OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) {
    if (!dragging || !enabled || !isVisible) return;
    (void)oldx;
    (void)oldy;

    int32_t track = TrackLength();
    int32_t thumb = ThumbLength();
    int32_t usable = track - thumb;
    if (usable <= 0) return;

    int32_t pos = vertical ? (newy - this->y) : (newx - this->x);
    int64_t deltaPx = (int64_t)pos - dragStart;
    if (deltaPx > usable) deltaPx = usable;
    if (deltaPx < -usable) deltaPx = -usable;

    // |deltaPx| is at most the usable track, so the quotient can never exceed
    // the value range. MulDiv keeps the wide intermediate out of libgcc.
    int32_t scaled = GuiMulDivSigned((int32_t)deltaPx, ValueRange(), usable);
    int64_t next = (int64_t)dragValue + scaled;
    if (next < minValue) next = minValue;
    if (next > MaxValue()) next = MaxValue();
    SetValue((int32_t)next);
}

void ScrollBar::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    (void)x;
    (void)y;
    (void)button;
    dragging = false;
    SetMouseCaptured(false);
}

void ScrollBar::OnMouseWheel(int32_t delta) {
    if (!enabled || !isVisible) return;
    int64_t next = (int64_t)value - delta;
    if (next < minValue) next = minValue;
    if (next > MaxValue()) next = MaxValue();
    SetValue((int32_t)next);
}

void ScrollBar::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }
    memset(cache, 0, sizeof(uint32_t) * w * h);
    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }

    NINA::activeInstance->FillRectangle(cache, w, h, 0, 0, w, h, SCROLLBAR_BG);

    int32_t thumb = ThumbLength();
    int32_t thumbPos = ThumbPos();

    if (vertical) {
        NINA::activeInstance->FillRoundedRectangle(cache, w, h, 0, thumbPos, w, thumb, 2,
                                                   SCROLLBAR_THUMB);
    } else {
        NINA::activeInstance->FillRoundedRectangle(cache, w, h, thumbPos, 0, thumb, h, 2,
                                                   SCROLLBAR_THUMB);
    }
    isDirty = false;
}
