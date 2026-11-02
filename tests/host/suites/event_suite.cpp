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

#include <gui/input/inputQueue.h>
#include <gui/inputEvent.h>
#include <gui/widget.h>

namespace {

/** Probe - A leaf widget that counts each extended input event it receives. */
struct Probe : public Widget {
    int wheel = 0;
    int dbl = 0;
    int repeat = 0;
    int mod = 0;

    Probe(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h)
        : Widget(parent, x, y, w, h) {}

    void OnMouseWheel(int32_t) override {
        wheel++;
    }
    void OnDoubleClick(int32_t, int32_t, uint8_t) override {
        dbl++;
    }
    void OnKeyRepeat(const char*, uint32_t) override {
        repeat++;
    }
    void OnKeyDownMod(const char*, uint8_t) override {
        mod++;
    }
};

}  // namespace

TEST(wheel_routes_to_child_under_last_position) {
    CompositeWidget root(nullptr, 0, 0, 100, 100);
    Probe* a = new Probe(&root, 0, 0, 50, 50);
    Probe* b = new Probe(&root, 50, 50, 50, 50);
    root.AddChild(a);
    root.AddChild(b);

    root.OnMouseMove(0, 0, 10, 10);  // cursor over a
    root.OnMouseWheel(1);
    CHECK_EQ(a->wheel, 1);
    CHECK_EQ(b->wheel, 0);

    root.OnMouseMove(10, 10, 60, 60);  // cursor over b
    root.OnMouseWheel(-1);
    CHECK_EQ(a->wheel, 1);
    CHECK_EQ(b->wheel, 1);
    // root destructor deletes a and b.
}

TEST(double_click_routes_to_hit_child) {
    CompositeWidget root(nullptr, 0, 0, 100, 100);
    Probe* a = new Probe(&root, 0, 0, 50, 50);
    Probe* b = new Probe(&root, 50, 50, 50, 50);
    root.AddChild(a);
    root.AddChild(b);

    root.OnDoubleClick(20, 20, 1);
    CHECK_EQ(a->dbl, 1);
    CHECK_EQ(b->dbl, 0);

    root.OnDoubleClick(70, 70, 1);
    CHECK_EQ(a->dbl, 1);
    CHECK_EQ(b->dbl, 1);
}

TEST(tab_traversal_skips_non_focussable) {
    CompositeWidget root(nullptr, 0, 0, 300, 40);
    Probe* a = new Probe(&root, 0, 0, 50, 40);
    Probe* mid = new Probe(&root, 60, 0, 50, 40);
    Probe* b = new Probe(&root, 120, 0, 50, 40);
    mid->SetFocussable(false);
    root.AddChild(a);
    root.AddChild(mid);
    root.AddChild(b);

    root.GetFocus(a);
    CHECK(a->IsFocused());

    root.OnTabFocus(false);
    CHECK(b->IsFocused());
    CHECK(!a->IsFocused());
    CHECK(!mid->IsFocused());

    root.OnTabFocus(true);
    CHECK(a->IsFocused());
    CHECK(!b->IsFocused());
}

TEST(tab_traversal_descends_into_nested_composites) {
    CompositeWidget root(nullptr, 0, 0, 400, 40);
    CompositeWidget* panel = new CompositeWidget(&root, 0, 0, 200, 40);
    Probe* a = new Probe(panel, 0, 0, 50, 40);
    Probe* b = new Probe(panel, 60, 0, 50, 40);
    Probe* c = new Probe(&root, 220, 0, 50, 40);
    root.AddChild(panel);
    panel->AddChild(a);
    panel->AddChild(b);
    root.AddChild(c);

    c->GetFocus(c);
    CHECK(c->IsFocused());

    // Wrapping from the last stop enters the nested panel's first child; the
    // panel itself is a container, not a stop.
    root.OnTabFocus(false);
    CHECK(a->IsFocused());
    CHECK(!c->IsFocused());

    // The remaining stops are reachable, and only one leaf holds focus at a
    // time even though the panel is focused as the leaf's ancestor.
    int visitedB = 0;
    int visitedC = 0;
    for (int i = 0; i < 2; i++) {
        root.OnTabFocus(false);
        if (a->IsFocused()) CHECK(!b->IsFocused() && !c->IsFocused());
        if (b->IsFocused()) {
            visitedB++;
            CHECK(!a->IsFocused() && !c->IsFocused());
        }
        if (c->IsFocused()) {
            visitedC++;
            CHECK(!a->IsFocused() && !b->IsFocused());
        }
    }
    CHECK_EQ(visitedB, 1);
    CHECK_EQ(visitedC, 1);
}

TEST(key_repeat_and_modifiers_reach_focused_child) {
    CompositeWidget root(nullptr, 0, 0, 100, 100);
    Probe* a = new Probe(&root, 0, 0, 50, 50);
    Probe* b = new Probe(&root, 50, 50, 50, 50);
    root.AddChild(a);
    root.AddChild(b);

    root.GetFocus(b);
    root.OnKeyRepeat("x", 3);
    root.OnKeyDownMod("y", MOD_SHIFT | MOD_CTRL);
    CHECK_EQ(a->repeat, 0);
    CHECK_EQ(a->mod, 0);
    CHECK_EQ(b->repeat, 1);
    CHECK_EQ(b->mod, 1);
}

TEST(input_queue_is_fifo_and_wraps) {
    InputQueue queue;
    CHECK_EQ(queue.Count(), 0u);

    for (uint32_t i = 0; i < InputQueue::CAPACITY; i++) {
        InputEvent e = {};
        e.type = IE_KEY_DOWN;
        e.x = (int32_t)i;
        CHECK(queue.Push(e));
    }
    CHECK_EQ(queue.Count(), InputQueue::CAPACITY);

    // Full: the next push is dropped and counted.
    InputEvent extra = {};
    extra.type = IE_KEY_REPEAT;
    CHECK(!queue.Push(extra));
    CHECK_EQ(queue.OverflowCount(), 1u);

    // Drain half, then push again to exercise wraparound.
    InputEvent out = {};
    for (uint32_t i = 0; i < InputQueue::CAPACITY / 2; i++) {
        CHECK(queue.Pop(out));
        CHECK_EQ(out.x, (int32_t)i);
    }
    for (uint32_t i = 0; i < InputQueue::CAPACITY / 2; i++) {
        InputEvent e = {};
        e.type = IE_MOUSE_MOVE;
        e.x = (int32_t)(1000 + i);
        CHECK(queue.Push(e));
    }

    // FIFO order continues across the wrap.
    for (uint32_t i = InputQueue::CAPACITY / 2; i < InputQueue::CAPACITY; i++) {
        CHECK(queue.Pop(out));
        CHECK_EQ(out.x, (int32_t)i);
    }
    for (uint32_t i = 0; i < InputQueue::CAPACITY / 2; i++) {
        CHECK(queue.Pop(out));
        CHECK_EQ(out.x, (int32_t)(1000 + i));
    }
    CHECK_EQ(queue.Count(), 0u);
    CHECK(!queue.Pop(out));
}

TEST(input_queue_clear) {
    InputQueue queue;
    InputEvent e = {};
    e.type = IE_MOUSE_DOWN;
    queue.Push(e);
    queue.Push(e);
    queue.Clear();
    CHECK_EQ(queue.Count(), 0u);
    CHECK(!queue.Pop(e));
}
