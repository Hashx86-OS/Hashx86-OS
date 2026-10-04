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
#include "../harness.h"

#include <gui/checkbox.h>
#include <gui/layout.h>
#include <gui/scrollbar.h>
#include <gui/slider.h>
#include <gui/toggle.h>

TEST(checkbox_toggles_and_emits) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    CheckBox* box = new CheckBox(&root, 10, 10, 120, 24, "Enable");
    root.AddChild(box);
    box->SetID(500);
    box->SetPID(3);

    CHECK(!box->GetChecked());
    HostEventReset();
    box->OnMouseUp(20, 20, 1);
    CHECK(box->GetChecked());
    CHECK_EQ(HostEventCount(), 1);
    if (HostEventCount() == 1) CHECK_EQ(HostEventAt(0).param1, 1u);

    box->OnMouseUp(20, 20, 1);
    CHECK(!box->GetChecked());
    CHECK_EQ(HostEventCount(), 2);
    if (HostEventCount() == 2) CHECK_EQ(HostEventAt(1).param1, 0u);
}

TEST(checkbox_key_toggles_when_focused) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    CheckBox* box = new CheckBox(&root, 10, 10, 120, 24, "Enable");
    root.AddChild(box);
    box->SetID(501);
    box->SetPID(3);
    HostEventReset();
    box->OnKeyDown(" ");
    CHECK(box->GetChecked());
    CHECK_EQ(HostEventCount(), 1);
}

TEST(toggle_flips_and_clamps) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    Toggle* t = new Toggle(&root, 10, 10, 40, 20);
    root.AddChild(t);
    t->SetID(502);
    t->SetPID(4);

    CHECK(!t->IsOn());
    HostEventReset();
    t->OnMouseUp(20, 20, 1);
    CHECK(t->IsOn());
    CHECK_EQ(HostEventCount(), 1);
    t->SetOn(false);
    CHECK(!t->IsOn());
}

TEST(slider_value_clamps_to_range) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    Slider* s = new Slider(&root, 0, 0, 100, 20);
    root.AddChild(s);
    s->SetRange(0, 10);

    s->SetValue(5);
    CHECK_EQ(s->GetValue(), 5);
    s->SetValue(999);
    CHECK_EQ(s->GetValue(), 10);
    s->SetValue(-999);
    CHECK_EQ(s->GetValue(), 0);

    s->SetRange(20, 30);
    CHECK(s->GetValue() >= 20);
    CHECK(s->GetValue() <= 30);
}

TEST(slider_drag_tracks_pointer) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    Slider* s = new Slider(&root, 0, 0, 100, 20);
    root.AddChild(s);
    s->SetRange(0, 100);

    s->OnMouseDown(0, 10, 1);
    CHECK_EQ(s->GetValue(), 0);
    s->OnMouseMove(0, 10, 100, 10);
    CHECK(s->GetValue() >= 95);  // near the right edge
    s->OnMouseUp(100, 10, 1);
}

TEST(scrollbar_value_respects_page) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    ScrollBar* bar = new ScrollBar(&root, 0, 0, 100, 12, false);
    root.AddChild(bar);
    bar->SetRange(0, 100, 20);

    bar->SetValue(90);
    CHECK_EQ(bar->GetValue(), 80);  // max = 100 - 20

    bar->SetValue(-5);
    CHECK_EQ(bar->GetValue(), 0);
}

TEST(vbox_layout_stacks_children) {
    CompositeWidget container(nullptr, 0, 0, 200, 100);
    VBoxLayout layout(&container, 4, 4);
    Widget* a = new Widget(&container, 0, 0, 10, 20);
    Widget* b = new Widget(&container, 0, 0, 10, 20);
    layout.Add(a);
    layout.Add(b);
    layout.Relayout();

    CHECK_EQ(a->x, 4);
    CHECK_EQ(a->y, 4);
    CHECK_EQ(a->w, 192);
    CHECK_EQ(a->h, 20);
    CHECK_EQ(b->y, 4 + 20 + 4);
}

TEST(hbox_layout_positions_children) {
    CompositeWidget container(nullptr, 0, 0, 200, 100);
    HBoxLayout layout(&container, 4, 4);
    Widget* a = new Widget(&container, 0, 0, 30, 10);
    Widget* b = new Widget(&container, 0, 0, 30, 10);
    layout.Add(a);
    layout.Add(b);
    layout.Relayout();

    CHECK_EQ(a->x, 4);
    CHECK_EQ(a->y, 4);
    CHECK_EQ(a->h, 92);
    CHECK_EQ(b->x, 4 + 30 + 4);
}

TEST(grid_layout_places_cells) {
    CompositeWidget container(nullptr, 0, 0, 200, 100);
    GridLayout layout(&container, 2, 4, 4);
    Widget* cells[4];
    for (int i = 0; i < 4; i++) {
        cells[i] = new Widget(&container, 0, 0, 10, 10);
        layout.Add(cells[i]);
    }
    layout.Relayout();

    CHECK_EQ(cells[0]->x, 4);
    CHECK_EQ(cells[0]->y, 4);
    CHECK_EQ(cells[1]->x, 4 + 94 + 4);
    CHECK_EQ(cells[2]->y, 4 + 44 + 4);
    CHECK_EQ(cells[0]->w, 94);
    CHECK_EQ(cells[0]->h, 44);
}

TEST(vbox_layout_stretch_fills_free_space) {
    CompositeWidget container(nullptr, 0, 0, 200, 100);
    VBoxLayout layout(&container, 0, 0);
    Widget* fixed = new Widget(&container, 0, 0, 10, 20);
    Widget* flex = new Widget(&container, 0, 0, 10, 10);
    layout.Add(fixed, 0);
    layout.Add(flex, 1);
    layout.Relayout();

    CHECK_EQ(fixed->h, 20);
    CHECK_EQ(flex->h, 80);
    CHECK_EQ(flex->y, 20);
}
