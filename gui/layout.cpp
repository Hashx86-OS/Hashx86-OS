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

#include <gui/layout.h>

Layout::Layout(CompositeWidget* parent, int32_t spacing, int32_t padding)
    : parent(parent), spacing(spacing), padding(padding), count(0) {}

void Layout::Add(Widget* widget, int32_t stretchValue) {
    if (!widget || !parent || count >= 64) return;
    parent->AddChild(widget);
    managed[count] = widget;
    stretch[count] = stretchValue;
    count++;
}

void VBoxLayout::Relayout() {
    if (count == 0) return;

    int32_t availH = parent->h - 2 * padding - spacing * (count - 1);
    if (availH < 0) availH = 0;

    int32_t flexTotal = 0;
    int32_t fixedTotal = 0;
    for (int i = 0; i < count; i++) {
        if (stretch[i] > 0) {
            flexTotal += stretch[i];
        } else {
            fixedTotal += managed[i]->h;
        }
    }

    int32_t freeH = availH - fixedTotal;
    if (freeH < 0) freeH = 0;

    int32_t y = padding;
    for (int i = 0; i < count; i++) {
        int32_t hh = managed[i]->h;
        if (stretch[i] > 0 && flexTotal > 0) {
            hh = freeH * stretch[i] / flexTotal;
        }
        managed[i]->Resize(parent->w - 2 * padding, hh);
        managed[i]->x = padding;
        managed[i]->y = y;
        managed[i]->MarkDirty();
        y += hh + spacing;
    }
}

void HBoxLayout::Relayout() {
    if (count == 0) return;

    int32_t availW = parent->w - 2 * padding - spacing * (count - 1);
    if (availW < 0) availW = 0;

    int32_t flexTotal = 0;
    int32_t fixedTotal = 0;
    for (int i = 0; i < count; i++) {
        if (stretch[i] > 0) {
            flexTotal += stretch[i];
        } else {
            fixedTotal += managed[i]->w;
        }
    }

    int32_t freeW = availW - fixedTotal;
    if (freeW < 0) freeW = 0;

    int32_t x = padding;
    for (int i = 0; i < count; i++) {
        int32_t ww = managed[i]->w;
        if (stretch[i] > 0 && flexTotal > 0) {
            ww = freeW * stretch[i] / flexTotal;
        }
        managed[i]->Resize(ww, parent->h - 2 * padding);
        managed[i]->x = x;
        managed[i]->y = padding;
        managed[i]->MarkDirty();
        x += ww + spacing;
    }
}

void GridLayout::Relayout() {
    if (count == 0) return;

    int rows = (count + columns - 1) / columns;
    int32_t cellW = (parent->w - 2 * padding - spacing * (columns - 1)) / columns;
    int32_t cellH = (parent->h - 2 * padding - spacing * (rows - 1)) / rows;
    if (cellW < 1) cellW = 1;
    if (cellH < 1) cellH = 1;

    for (int i = 0; i < count; i++) {
        int col = i % columns;
        int row = i / columns;
        managed[i]->Resize(cellW, cellH);
        managed[i]->x = padding + col * (cellW + spacing);
        managed[i]->y = padding + row * (cellH + spacing);
        managed[i]->MarkDirty();
    }
}
