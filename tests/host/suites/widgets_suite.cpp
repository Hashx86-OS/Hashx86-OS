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

#include <cstring>

#include <gui/checkbox.h>
#include <gui/contextmenu.h>
#include <gui/imageview.h>
#include <gui/layout.h>
#include <gui/radiobutton.h>
#include <gui/scrollbar.h>
#include <gui/slider.h>
#include <gui/spinbox.h>
#include <gui/tabs.h>
#include <gui/textbox.h>
#include <gui/toggle.h>
#include <gui/tooltip.h>

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

TEST(radiobutton_is_mutually_exclusive_in_group) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    RadioButton* a = new RadioButton(&root, 0, 0, 120, 20, "A", 1);
    RadioButton* b = new RadioButton(&root, 0, 30, 120, 20, "B", 1);
    RadioButton* other = new RadioButton(&root, 0, 60, 120, 20, "C", 2);
    root.AddChild(a);
    root.AddChild(b);
    root.AddChild(other);
    a->SetID(600);
    a->SetPID(5);
    other->SetChecked(true);

    HostEventReset();
    a->OnMouseUp(10, 10, 1);
    CHECK(a->GetChecked());
    CHECK(!b->GetChecked());
    CHECK(other->GetChecked());  // different group untouched
    CHECK_EQ(HostEventCount(), 1);

    b->SetID(601);
    b->SetPID(5);
    b->OnMouseUp(10, 40, 1);
    CHECK(b->GetChecked());
    CHECK(!a->GetChecked());
}

TEST(spinbox_steps_and_clamps) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    SpinBox* sp = new SpinBox(&root, 0, 0, 80, 24);
    root.AddChild(sp);
    sp->SetID(700);
    sp->SetPID(6);
    sp->SetRange(0, 10, 2);

    CHECK_EQ(sp->GetValue(), 0);
    HostEventReset();
    sp->OnMouseUp(10, 4, 1);  // upper half: +2
    CHECK_EQ(sp->GetValue(), 2);
    CHECK_EQ(HostEventCount(), 1);

    for (int i = 0; i < 20; i++) {
        sp->OnMouseUp(10, 20, 1);  // lower half: -2
    }
    CHECK_EQ(sp->GetValue(), 0);

    sp->OnMouseWheel(5);
    CHECK(sp->GetValue() <= 10);
}

TEST(tooltip_show_hide) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    Tooltip* tip = new Tooltip(&root, 5, 5, 80, 20, "hint");
    root.AddChild(tip);
    CHECK(!tip->isVisible);
    CHECK(!tip->IsFocussable());
    tip->Show();
    CHECK(tip->isVisible);
    tip->SetText("other");
    tip->Hide();
    CHECK(!tip->isVisible);
}

TEST(tabs_select_and_emit) {
    CompositeWidget root(nullptr, 0, 0, 300, 200);
    Tabs* tabs = new Tabs(&root, 0, 0, 300, 200);
    root.AddChild(tabs);
    tabs->SetID(800);
    tabs->SetPID(7);
    CHECK(tabs->AddTab("One"));
    CHECK(tabs->AddTab("Two"));
    CHECK_EQ(tabs->GetTabCount(), 2);
    CHECK_EQ(tabs->GetActive(), 0);
    CHECK(tabs->GetContentArea() != nullptr);

    HostEventReset();
    tabs->OnMouseUp(90, 10, 1);  // second header (80..160)
    CHECK_EQ(tabs->GetActive(), 1);
    CHECK_EQ(HostEventCount(), 1);
    if (HostEventCount() == 1) CHECK_EQ(HostEventAt(0).param1, 1u);

    tabs->SetActive(0);
    CHECK_EQ(tabs->GetActive(), 0);
}

TEST(contextmenu_selects_and_hides) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ContextMenu* menu = new ContextMenu(&root);
    root.AddChild(menu);
    menu->SetID(900);
    menu->SetPID(8);
    CHECK(menu->AddItem("Cut"));
    CHECK(menu->AddItem("Copy"));
    CHECK_EQ(menu->GetItemCount(), 2);

    CHECK(!menu->isVisible);
    menu->ShowAt(40, 40);
    CHECK(menu->isVisible);
    CHECK_EQ(menu->h, 48);

    HostEventReset();
    menu->OnMouseUp(45, 40 + 24 + 5, 1);  // second item
    CHECK_EQ(HostEventCount(), 1);
    if (HostEventCount() == 1) CHECK_EQ(HostEventAt(0).param1, 1u);
    CHECK(!menu->isVisible);
}

TEST(imageview_copies_and_scales) {
    CompositeWidget root(nullptr, 0, 0, 100, 100);
    const uint32_t src[4] = {0xFFFF0000u, 0xFF00FF00u, 0xFF0000FFu, 0xFFFFFFFFu};
    ImageView* iv = new ImageView(&root, 0, 0, 2, 2, src, 2, 2);
    root.AddChild(iv);
    CHECK(!iv->IsFocussable());
    iv->RedrawToCache();
    CHECK(iv->cache != nullptr);
    if (iv->cache) {
        CHECK_EQ(iv->cache[0], 0xFFFF0000u);
        CHECK_EQ(iv->cache[3], 0xFFFFFFFFu);
    }

    // Replacing the image must copy, not alias.
    uint32_t other[4] = {0, 0, 0, 0};
    iv->SetImage(other, 2, 2);
    other[0] = 0xDEADBEEFu;
    iv->RedrawToCache();
    if (iv->cache) CHECK_EQ(iv->cache[0], 0u);
}

TEST(textbox_edits_and_clamps) {
    CompositeWidget root(nullptr, 0, 0, 200, 100);
    TextBox* tb = new TextBox(&root, 0, 0, 150, 60, 8);
    root.AddChild(tb);

    tb->SetText("hello");
    CHECK_EQ(tb->GetLength(), 5u);
    CHECK_EQ(strcmp(tb->GetText(), "hello"), 0);

    tb->OnKeyDown("X");
    CHECK_EQ(tb->GetLength(), 6u);
    CHECK_EQ(strcmp(tb->GetText(), "helloX"), 0);

    tb->OnSpecialKeyDown(0x0E);  // Backspace
    CHECK_EQ(strcmp(tb->GetText(), "hello"), 0);

    tb->SetReadOnly(true);
    tb->OnKeyDown("Z");
    CHECK_EQ(strcmp(tb->GetText(), "hello"), 0);

    // Capacity clamp: 8 bytes total, so at most 7 characters.
    tb->SetReadOnly(false);
    tb->SetText("0123456789");
    CHECK(tb->GetLength() <= 7u);
}
