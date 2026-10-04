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
    int32_t span = maxValue - minValue;
    int32_t track = TrackLength();
    if (span <= 0) return track;
    int32_t len = (int32_t)(track * pageSize / span);
    if (len < 16) len = 16;
    if (len > track) len = track;
    return len;
}

int32_t ScrollBar::MaxValue() const {
    int32_t m = maxValue - pageSize;
    return (m < minValue) ? minValue : m;
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
    EmitValue();
}

void ScrollBar::OnMouseDown(int32_t x, int32_t y, uint8_t button) {
    if (!enabled || !isVisible) return;
    if (button != 1) return;
    Widget::OnMouseDown(x, y, button);

    // If the press is on the thumb, start dragging; otherwise page toward it.
    int32_t track = TrackLength();
    int32_t span = maxValue - minValue;
    int32_t thumb = ThumbLength();
    int32_t pos = vertical ? y : x;
    int32_t thumbPos = 0;
    if (span > 0 && track > thumb) {
        thumbPos = (int32_t)((value - minValue) * (track - thumb) / span);
    }

    if (pos >= thumbPos && pos < thumbPos + thumb) {
        dragging = true;
        dragStart = pos;
        dragValue = value;
    } else if (pos < thumbPos) {
        SetValue(value - pageSize);
    } else {
        SetValue(value + pageSize);
    }
}

void ScrollBar::OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) {
    if (!dragging) return;
    (void)oldx;
    (void)oldy;

    int32_t track = TrackLength();
    int32_t span = maxValue - minValue;
    int32_t thumb = ThumbLength();
    int32_t usable = track - thumb;
    if (usable <= 0 || span <= 0) return;

    int32_t pos = vertical ? newy : newx;
    int32_t deltaPx = pos - dragStart;
    SetValue(dragValue + (int32_t)(deltaPx * span / usable));
}

void ScrollBar::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    (void)x;
    (void)y;
    (void)button;
    dragging = false;
}

void ScrollBar::OnMouseWheel(int32_t delta) {
    if (!enabled || !isVisible) return;
    SetValue(value - delta);
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

    int32_t track = TrackLength();
    int32_t span = maxValue - minValue;
    int32_t thumb = ThumbLength();
    int32_t thumbPos = 0;
    if (span > 0 && track > thumb) {
        thumbPos = (int32_t)((value - minValue) * (track - thumb) / span);
    }

    if (vertical) {
        NINA::activeInstance->FillRoundedRectangle(cache, w, h, 0, thumbPos, w, thumb, 2,
                                                   SCROLLBAR_THUMB);
    } else {
        NINA::activeInstance->FillRoundedRectangle(cache, w, h, thumbPos, 0, thumb, h, 2,
                                                   SCROLLBAR_THUMB);
    }
    isDirty = false;
}
