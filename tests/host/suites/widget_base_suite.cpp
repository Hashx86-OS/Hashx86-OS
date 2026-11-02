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

#include "../framework.h"

#include <gui/button.h>
#include <gui/label.h>
#include <gui/render/raster.h>
#include <gui/widget.h>

TEST(raster_surface_fills_rectangle) {
    RasterSurface surface(32, 16);
    surface.FillRectangle(0, 0, 32, 16, 0xFFFF0000);
    CHECK_EQ(surface.GetBackBuffer()[0], 0xFFFF0000u);
    CHECK_EQ(surface.GetBackBuffer()[31], 0xFFFF0000u);
    CHECK_EQ(surface.GetBackBuffer()[32 * 16 - 1], 0xFFFF0000u);
}

TEST(raster_surface_put_pixel_clips) {
    RasterSurface surface(8, 8);
    surface.PutPixel(-1, -1, 0xFF00FF00);
    surface.PutPixel(8, 8, 0xFF00FF00);
    surface.PutPixel(0, 0, 0xFF00FF00);
    CHECK_EQ(surface.GetBackBuffer()[0], 0xFF00FF00u);
    CHECK_EQ(surface.GetBackBuffer()[63], 0xFF000000u);
}

TEST(widget_contains_coordinate) {
    Widget widget(nullptr, 10, 20, 30, 40);
    CHECK(widget.ContainsCoordinate(10, 20));
    CHECK(widget.ContainsCoordinate(39, 59));
    CHECK(!widget.ContainsCoordinate(9, 20));
    CHECK(!widget.ContainsCoordinate(40, 20));
    CHECK(!widget.ContainsCoordinate(10, 60));
}

TEST(widget_add_and_remove_child) {
    Widget parent(nullptr, 0, 0, 100, 100);
    Widget child(&parent, 5, 5, 10, 10);
    CHECK_EQ(parent.childrenList.GetSize(), 0u);
    CHECK(parent.AddChild(&child));
    CHECK_EQ(parent.childrenList.GetSize(), 1u);
    CHECK_EQ(child.parent, &parent);
    CHECK(parent.RemoveChild(&child));
    CHECK_EQ(parent.childrenList.GetSize(), 0u);
    CHECK_EQ(child.parent, nullptr);
}

TEST(label_stores_text) {
    Label label(nullptr, 0, 0, 50, 20, "hello");
    label.setText("world");
    CHECK_EQ(label.isVisible, true);
}

TEST(button_constructs) {
    Button button(nullptr, 0, 0, 40, 20, "OK");
    CHECK(button.IsButton());
    CHECK(!button.IsPressed());
}
