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

#define KDBG_COMPONENT "GUI:SCROLLCONTAINER"
#include <gui/scrollcontainer.h>

ScrollContainer::ScrollContainer(CompositeWidget* parent, int32_t x, int32_t y, int32_t w,
                                 int32_t h)
    : CompositeWidget(parent, x, y, w, h), contentW(w), contentH(h) {
    content = new CompositeWidget(this, 0, 0, w - BAR, h - BAR);
    vbar = new ScrollBar(this, w - BAR, 0, BAR, h - BAR, true);
    hbar = new ScrollBar(this, 0, h - BAR, w - BAR, BAR, false);
    AddChild(content);
    AddChild(vbar);
    AddChild(hbar);
    SyncBars();
}

void ScrollContainer::SetContentSize(int32_t cw, int32_t ch) {
    contentW = cw;
    contentH = ch;
    content->Resize(cw, ch);
    SyncBars();
}

void ScrollContainer::SyncBars() {
    vbar->SetRange(0, contentH, h - BAR);
    hbar->SetRange(0, contentW, w - BAR);
}

int32_t ScrollContainer::GetScrollX() const {
    return -content->x;
}

int32_t ScrollContainer::GetScrollY() const {
    return -content->y;
}

void ScrollContainer::OnMouseWheel(int32_t delta) {
    int32_t next = vbar->GetValue() - delta;
    vbar->SetValue(next);
    content->y = -vbar->GetValue();
    content->MarkDirty();
    MarkDirty();
}
