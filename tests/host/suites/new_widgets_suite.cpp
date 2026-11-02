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

#include <cstring>

#include <gui/checkbox.h>
#include <gui/combobox.h>
#include <gui/contextmenu.h>
#include <gui/imageview.h>
#include <gui/layout.h>
#include <gui/progressbar.h>
#include <gui/radiobutton.h>
#include <gui/scrollbar.h>
#include <gui/slider.h>
#include <gui/spinbox.h>
#include <gui/table.h>
#include <gui/tabs.h>
#include <gui/textbox.h>
#include <gui/toggle.h>

namespace {

uint64_t g_rng2 = 0x9E3779B97F4A7C15ULL;

uint32_t Rnd2() {
    g_rng2 ^= g_rng2 << 13;
    g_rng2 ^= g_rng2 >> 7;
    g_rng2 ^= g_rng2 << 17;
    return (uint32_t)(g_rng2 >> 32);
}

int32_t RndRange2(int32_t lo, int32_t hi) {
    if (hi <= lo) return lo;
    return lo + (int32_t)(Rnd2() % (uint32_t)(hi - lo));
}

}  // namespace

TEST(slider_property_value_always_in_range) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    Slider* s = new Slider(&root, 0, 0, 120, 20);
    root.AddChild(s);
    for (int t = 0; t < 200; t++) {
        int32_t lo = RndRange2(0, 50);
        int32_t hi = lo + RndRange2(1, 200);
        s->SetRange(lo, hi);
        s->SetValue(RndRange2(-500, 500));
        CHECK(s->GetValue() >= lo);
        CHECK(s->GetValue() <= hi);
    }
}

TEST(scrollbar_property_respects_page) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ScrollBar* bar = new ScrollBar(&root, 0, 0, 120, 12, false);
    root.AddChild(bar);
    for (int t = 0; t < 200; t++) {
        int32_t hi = RndRange2(1, 400);
        int32_t page = RndRange2(1, hi + 1);
        bar->SetRange(0, hi, page);
        bar->SetValue(RndRange2(-200, 800));
        int32_t maxV = hi - page;
        if (maxV < 0) maxV = 0;
        CHECK(bar->GetValue() >= 0);
        CHECK(bar->GetValue() <= maxV);
    }
}

TEST(spinbox_property_clamps) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    SpinBox* sp = new SpinBox(&root, 0, 0, 80, 24);
    root.AddChild(sp);
    for (int t = 0; t < 200; t++) {
        int32_t lo = RndRange2(-100, 100);
        int32_t hi = lo + RndRange2(0, 200);
        int32_t step = RndRange2(1, 10);
        sp->SetRange(lo, hi, step);
        sp->SetValue(RndRange2(-1000, 1000));
        CHECK(sp->GetValue() >= lo);
        CHECK(sp->GetValue() <= hi);
    }
}

TEST(combobox_selection_stays_valid) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ComboBox* cb = new ComboBox(&root, 0, 0, 120, 24);
    root.AddChild(cb);
    const int n = 5;
    for (int i = 0; i < n; i++) cb->AddItem("item");
    for (int t = 0; t < 200; t++) {
        cb->SetSelected(RndRange2(-3, n + 3));
        CHECK(cb->GetSelected() >= -1);
        CHECK(cb->GetSelected() < n);
    }
}

TEST(table_selection_stays_valid) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    Table* table = new Table(&root, 0, 0, 200, 200, 3);
    root.AddChild(table);
    const int n = 10;
    for (int i = 0; i < n; i++) {
        const char* row[] = {"a", "b", "c"};
        table->AddRow(row);
    }
    for (int t = 0; t < 200; t++) {
        table->SetSelected(RndRange2(-5, n + 5));
        CHECK(table->GetSelected() >= -1);
        CHECK(table->GetSelected() < n);
    }
}

TEST(textbox_property_length_bounded) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    const uint32_t cap = 16;
    TextBox* tb = new TextBox(&root, 0, 0, 200, 80, cap);
    root.AddChild(tb);
    for (int t = 0; t < 500; t++) {
        if (Rnd2() % 2) {
            tb->OnKeyDown("a");
        } else {
            tb->OnSpecialKeyDown(0x0E);  // Backspace
        }
        CHECK(tb->GetLength() < cap);
        CHECK(tb->GetText()[tb->GetLength()] == '\0');
    }
}

TEST(tabs_active_stays_valid) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    Tabs* tabs = new Tabs(&root, 0, 0, 300, 200);
    root.AddChild(tabs);
    const int n = 4;
    for (int i = 0; i < n; i++) tabs->AddTab("t");
    for (int t = 0; t < 200; t++) {
        tabs->SetActive(RndRange2(-3, n + 3));
        CHECK(tabs->GetActive() >= -1);
        CHECK(tabs->GetActive() < n);
    }
}

TEST(radiobutton_property_single_selected) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    RadioButton* buttons[5];
    for (int i = 0; i < 5; i++) {
        buttons[i] = new RadioButton(&root, 0, i * 24, 120, 20, "r", 1);
        root.AddChild(buttons[i]);
    }
    for (int t = 0; t < 200; t++) {
        int idx = (int)(Rnd2() % 5);
        buttons[idx]->OnMouseDown(10, idx * 24 + 10, 1);
        buttons[idx]->OnMouseUp(10, idx * 24 + 10, 1);
        int selected = 0;
        for (int i = 0; i < 5; i++) {
            if (buttons[i]->GetChecked()) selected++;
        }
        CHECK_EQ(selected, 1);
    }
}

TEST(vbox_property_children_within_parent) {
    for (int t = 0; t < 100; t++) {
        CompositeWidget parent(nullptr, 0, 0, RndRange2(50, 300), RndRange2(50, 300));
        VBoxLayout layout(&parent, 4, 4);
        int n = 1 + (int)(Rnd2() % 5);
        Widget* kids[5];
        for (int i = 0; i < n; i++) {
            kids[i] = new Widget(&parent, 0, 0, 10, RndRange2(5, 30));
            layout.Add(kids[i], (Rnd2() % 2) ? 1 : 0);
        }
        layout.Relayout();
        for (int i = 0; i < n; i++) {
            CHECK(kids[i]->x >= 0);
            CHECK(kids[i]->y >= 0);
            // Width always fits; height may overflow when fixed content is
            // larger than the viewport (that is what a scroll container is for).
            CHECK(kids[i]->x + kids[i]->w <= parent.w);
            if (i > 0) CHECK(kids[i]->y >= kids[i - 1]->y);
        }
    }
}

TEST(hbox_property_children_within_parent) {
    for (int t = 0; t < 100; t++) {
        CompositeWidget parent(nullptr, 0, 0, RndRange2(50, 300), RndRange2(50, 300));
        HBoxLayout layout(&parent, 4, 4);
        int n = 1 + (int)(Rnd2() % 5);
        Widget* kids[5];
        for (int i = 0; i < n; i++) {
            kids[i] = new Widget(&parent, 0, 0, RndRange2(5, 30), 10);
            layout.Add(kids[i], (Rnd2() % 2) ? 1 : 0);
        }
        layout.Relayout();
        for (int i = 0; i < n; i++) {
            CHECK(kids[i]->x >= 0);
            CHECK(kids[i]->y >= 0);
            // Height always fits; width may overflow when fixed content is
            // larger than the viewport.
            CHECK(kids[i]->y + kids[i]->h <= parent.h);
            if (i > 0) CHECK(kids[i]->x >= kids[i - 1]->x);
        }
    }
}

// ---------------------------------------------------------------------------
// Deterministic pixel checks: rendering twice must produce identical bytes,
// and a state change must change at least one pixel.
// ---------------------------------------------------------------------------
TEST(checkbox_render_changes_with_state) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    CheckBox* box = new CheckBox(&root, 0, 0, 100, 24, "x");
    root.AddChild(box);

    box->SetChecked(false);
    box->RedrawToCache();
    uint32_t before[100 * 24];
    memcpy(before, box->cache, sizeof(before));

    box->SetChecked(true);
    box->RedrawToCache();
    int changed = 0;
    for (int i = 0; i < 100 * 24; i++) {
        if (before[i] != box->cache[i]) changed++;
    }
    CHECK(changed > 0);
}

TEST(progressbar_render_is_deterministic) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    ProgressBar* bar = new ProgressBar(&root, 0, 0, 100, 16, 0.5f);
    root.AddChild(bar);
    bar->RedrawToCache();
    uint32_t first[100 * 16];
    memcpy(first, bar->cache, sizeof(first));
    bar->MarkDirty();
    bar->RedrawToCache();
    CHECK_EQ(memcmp(first, bar->cache, sizeof(first)), 0);
}

TEST(toggle_render_changes_with_state) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    Toggle* t = new Toggle(&root, 0, 0, 40, 20);
    root.AddChild(t);
    t->SetOn(false);
    t->RedrawToCache();
    uint32_t before[40 * 20];
    memcpy(before, t->cache, sizeof(before));
    t->SetOn(true);
    t->RedrawToCache();
    int changed = 0;
    for (int i = 0; i < 40 * 20; i++) {
        if (before[i] != t->cache[i]) changed++;
    }
    CHECK(changed > 0);
}

TEST(imageview_render_is_deterministic) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    uint32_t src[4] = {0xFF010203u, 0xFF040506u, 0xFF070809u, 0xFF0A0B0Cu};
    ImageView* iv = new ImageView(&root, 0, 0, 4, 4, src, 2, 2);
    root.AddChild(iv);
    iv->RedrawToCache();
    uint32_t first[4 * 4];
    memcpy(first, iv->cache, sizeof(first));
    iv->MarkDirty();
    iv->RedrawToCache();
    CHECK_EQ(memcmp(first, iv->cache, sizeof(first)), 0);
}

TEST(contextmenu_render_stable) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    ContextMenu* menu = new ContextMenu(&root);
    root.AddChild(menu);
    menu->AddItem("one");
    menu->AddItem("two");
    menu->ShowAt(10, 10);
    menu->RedrawToCache();
    uint32_t first[140 * 48];
    memcpy(first, menu->cache, sizeof(first));
    menu->MarkDirty();
    menu->RedrawToCache();
    CHECK_EQ(memcmp(first, menu->cache, sizeof(first)), 0);
}
