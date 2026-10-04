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

#ifndef LAYOUT_H
#define LAYOUT_H

#include <gui/widget.h>

/**
 * class Layout - Arranges the children of a CompositeWidget.
 *
 * A layout manages a list of widgets, positions them in two passes (measure
 * then place), and is pure geometry: it only sets x/y/w/h and marks widgets
 * dirty. A widget with a positive stretch factor absorbs free space.
 */
class Layout {
protected:
    CompositeWidget* parent;
    int32_t spacing;
    int32_t padding;
    Widget* managed[64];
    int32_t stretch[64];
    int count;

public:
    /**
     * Layout() - Construct a layout over a container.
     * @parent: Container whose children are managed.
     * @spacing: Gap between adjacent widgets, in pixels.
     * @padding: Inner padding around the container edges, in pixels.
     */
    Layout(CompositeWidget* parent, int32_t spacing, int32_t padding);

    virtual ~Layout() {}

    /**
     * Add() - Manage a widget and add it to the container.
     * @widget: Widget to manage.
     * @stretch: Share of free space (0 = fixed size).
     */
    virtual void Add(Widget* widget, int32_t stretch = 0);

    /** Relayout() - Recompute every managed widget's geometry. */
    virtual void Relayout() = 0;
};

/** class VBoxLayout - Stacks managed widgets top to bottom. */
class VBoxLayout : public Layout {
public:
    VBoxLayout(CompositeWidget* parent, int32_t spacing, int32_t padding)
        : Layout(parent, spacing, padding) {}
    void Relayout() override;
};

/** class HBoxLayout - Lays managed widgets out left to right. */
class HBoxLayout : public Layout {
public:
    HBoxLayout(CompositeWidget* parent, int32_t spacing, int32_t padding)
        : Layout(parent, spacing, padding) {}
    void Relayout() override;
};

/** class GridLayout - Places managed widgets in a fixed-column grid. */
class GridLayout : public Layout {
private:
    int columns;

public:
    GridLayout(CompositeWidget* parent, int columns, int32_t spacing, int32_t padding)
        : Layout(parent, spacing, padding), columns(columns < 1 ? 1 : columns) {}
    void Relayout() override;
};

#endif  // LAYOUT_H
