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

// Regression coverage for interaction and compositing edge cases:
//   - mouse coordinates are parent-relative, not widget-local
//   - a drag released outside its widget still terminates (pointer capture)
//   - hidden widgets never steal clicks
//   - a release only activates a widget that received the matching press
//   - nested composites are actually rendered into their parent's cache
//   - raster primitives clip negative origins and non-positive sizes

#include "../framework.h"
#include "../harness.h"

#include <cstring>

#include <gui/button.h>
#include <gui/checkbox.h>
#include <gui/combobox.h>
#include <gui/contextmenu.h>
#include <gui/imageview.h>
#include <gui/listview.h>
#include <gui/mathutil.h>
#include <gui/render/nina.h>
#include <gui/render/raster.h>
#include <gui/scrollbar.h>
#include <gui/scrollcontainer.h>
#include <gui/slider.h>
#include <gui/tabs.h>
#include <gui/widget.h>
#include <gui/widgetHost.h>
#include <gui/window.h>

namespace {

constexpr int ILV_HEADER = LISTVIEW_HEADER_HEIGHT;

/** TestRaster() - A process-lifetime rasterizer for primitive tests. */
NINA& TestRaster() {
    static NINA nina;
    return nina;
}

/** TestRastersAreInstalled - Make TestRaster() the active renderer. */
TEST(interaction_suite_installs_rasterizer) {
    NINA::activeInstance = &TestRaster();
    CHECK(NINA::activeInstance != nullptr);
}

/** ClickProbe - Counts routed events and can capture the pointer. */
struct ClickProbe : public Widget {
    int downs = 0;
    int ups = 0;
    int moves = 0;
    int wheels = 0;
    int enters = 0;
    int leaves = 0;
    bool captureOnDown = false;
    bool held = false;

    ClickProbe(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h)
        : Widget(parent, x, y, w, h) {}

    void OnMouseDown(int32_t, int32_t, uint8_t) override {
        downs++;
        if (captureOnDown) {
            held = true;
            SetMouseCaptured(true);
        }
    }

    void OnMouseUp(int32_t, int32_t, uint8_t) override {
        ups++;
        if (held) {
            held = false;
            SetMouseCaptured(false);
        }
    }

    void OnMouseMove(int32_t, int32_t, int32_t, int32_t) override {
        moves++;
    }

    void OnMouseWheel(int32_t) override {
        wheels++;
    }

    void OnMouseEnter() override {
        enters++;
    }
    void OnMouseLeave() override {
        leaves++;
    }

    bool IsPressed() const override {
        return held;
    }
};

/** SolidWidget - Paints every cache pixel a single opaque color. */
struct SolidWidget : public Widget {
    uint32_t color;

    SolidWidget(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color)
        : Widget(parent, x, y, w, h), color(color) {
        isFocussable = false;
    }

    void RedrawToCache() override {
        if (!cache) {
            isDirty = false;
            return;
        }
        size_t count = (size_t)w * (size_t)h;
        for (size_t i = 0; i < count; i++) cache[i] = color;
        isDirty = false;
    }
};

void ILvFill(ListView* list, int count) {
    for (int i = 0; i < count; i++) {
        char name[24];
        std::snprintf(name, sizeof(name), "row %d", i);
        const char* cells[] = {name};
        list->AddItem(cells, 1, 0);
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Slider: parent-relative coordinates, thumb grab, capture, keyboard.
// ---------------------------------------------------------------------------

TEST(slider_click_thumb_is_widget_relative) {
    CompositeWidget root(nullptr, 0, 0, 500, 500);
    Slider* s = new Slider(&root, 24, 312, 230, 20);
    root.AddChild(s);
    s->SetRange(0, 100);
    s->SetValue(40);

    // usable = 218, thumb left = 40*218/100 = 87, centre = 93 local.
    // Parent-frame press at x = 24 + 93 must leave the value alone; treating
    // the parent coordinate as local used to jump it to 50.
    HostEventReset();
    s->OnMouseDown(24 + 93, 312 + 10, 1);
    CHECK_EQ(s->GetValue(), 40);
    CHECK(s->IsMouseCaptured());

    // Dragging ten pixels right moves the value by ~10/218 of the range.
    s->OnMouseMove(24 + 93, 312 + 10, 24 + 103, 312 + 10);
    CHECK_EQ(s->GetValue(), 44);

    s->OnMouseUp(24 + 103, 312 + 10, 1);
    CHECK(!s->IsMouseCaptured());
}

TEST(slider_track_click_centres_thumb) {
    CompositeWidget root(nullptr, 0, 0, 500, 500);
    Slider* s = new Slider(&root, 24, 312, 230, 20);
    root.AddChild(s);
    s->SetRange(0, 100);
    s->SetValue(0);

    // Local x = 115 is the centre of the thumb at value 50; the track press
    // should centre the thumb there rather than offsetting by the widget x.
    s->OnMouseDown(24 + 115, 312 + 10, 1);
    CHECK_EQ(s->GetValue(), 50);
    s->OnMouseUp(24 + 115, 312 + 10, 1);
}

TEST(slider_release_outside_ends_drag) {
    CompositeWidget root(nullptr, 0, 0, 500, 500);
    Slider* s = new Slider(&root, 24, 312, 230, 20);
    root.AddChild(s);
    s->SetRange(0, 100);
    s->SetValue(40);

    root.OnMouseDown(24 + 93, 312 + 10, 1);
    root.OnMouseMove(24 + 93, 312 + 10, 500, 312 + 10);
    CHECK_EQ(s->GetValue(), 100);

    // Release far outside the slider: capture must still deliver the release.
    root.OnMouseUp(500, 400, 1);
    CHECK(!s->IsMouseCaptured());

    int afterRelease = s->GetValue();
    root.OnMouseMove(500, 400, 10, 10);
    CHECK_EQ(s->GetValue(), afterRelease);
}

TEST(slider_keyboard_steps_and_extremes) {
    CompositeWidget root(nullptr, 0, 0, 300, 100);
    Slider* s = new Slider(&root, 0, 0, 200, 20);
    root.AddChild(s);
    s->SetRange(0, 100);
    s->SetValue(50);

    s->OnSpecialKeyDown(0x4D);  // Right.
    CHECK_EQ(s->GetValue(), 51);
    s->OnSpecialKeyDown(0x47);  // Home.
    CHECK_EQ(s->GetValue(), 0);
    s->OnSpecialKeyDown(0x4F);  // End.
    CHECK_EQ(s->GetValue(), 100);
}

// ---------------------------------------------------------------------------
// ScrollBar: parent-relative coordinates and capture.
// ---------------------------------------------------------------------------

TEST(scrollbar_click_thumb_is_widget_relative) {
    CompositeWidget root(nullptr, 0, 0, 800, 600);
    ScrollBar* bar = new ScrollBar(&root, 344, 484, 260, 12, false);
    root.AddChild(bar);
    bar->SetRange(0, 100, 25);  // thumb 65 long, travel 195, range 75.

    // Thumb spans local x=0..65. A press at parent x=374 lands on the thumb and
    // must not page; the old code compared the parent coordinate to the local
    // thumb and jumped the value to 25.
    bar->OnMouseDown(344 + 30, 484 + 6, 1);
    CHECK_EQ(bar->GetValue(), 0);
    CHECK(bar->IsMouseCaptured());

    bar->OnMouseMove(344 + 30, 484 + 6, 344 + 69, 484 + 6);
    CHECK_EQ(bar->GetValue(), 15);  // 39 px of 195 travel over a 75 range.

    bar->OnMouseUp(344 + 69, 484 + 6, 1);
    CHECK(!bar->IsMouseCaptured());
}

TEST(scrollbar_track_click_is_widget_relative) {
    CompositeWidget root(nullptr, 0, 0, 800, 600);
    ScrollBar* bar = new ScrollBar(&root, 344, 484, 260, 12, false);
    root.AddChild(bar);
    bar->SetRange(0, 100, 25);
    bar->SetValue(75);  // Thumb at the right end.

    // Local x=10 is above the thumb: page back one page.
    bar->OnMouseDown(344 + 10, 484 + 6, 1);
    CHECK_EQ(bar->GetValue(), 50);
}

// ---------------------------------------------------------------------------
// ListView: click mapping, scrollbar gutter, wheel overflow, capture.
// ---------------------------------------------------------------------------

TEST(listview_click_below_last_visible_row_is_ignored) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    list->SetID(2000);
    list->SetPID(12);
    ILvFill(list, 50);

    CHECK_EQ(list->GetVisibleItemCount(), 5);
    HostEventReset();
    // y = 115 lies in the empty strip below five 18px rows: the old row math
    // selected the first off-screen row (index 5).
    list->OnMouseDown(50, ILV_HEADER + 1 + 5 * list->GetItemHeight() + 2, 1);
    CHECK_EQ(list->GetSelectedIndex(), -1);
    CHECK_EQ(HostEventCount(), 0);
}

TEST(listview_short_list_click_uses_full_width) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    list->SetID(2001);
    list->SetPID(12);
    ILvFill(list, 3);

    CHECK(!list->NeedScrollbar());
    HostEventReset();
    list->OnMouseDown(198, ILV_HEADER + 2, 1);
    CHECK_EQ(list->GetSelectedIndex(), 0);
    CHECK_EQ(HostEventCount(), 1);

    // The one-pixel border is still not a row.
    list->OnMouseDown(199, ILV_HEADER + 2, 1);
    CHECK_EQ(HostEventCount(), 1);
}

TEST(listview_click_is_parent_relative) {
    CompositeWidget root(nullptr, 0, 0, 400, 400);
    ListView* list = new ListView(&root, 100, 50, 200, 130);
    root.AddChild(list);
    list->SetID(2003);
    list->SetPID(12);
    ILvFill(list, 5);

    // Window-local click on the first row of a list placed at (100, 50).
    root.OnMouseDown(150, 50 + ILV_HEADER + 2, 1);
    CHECK_EQ(list->GetSelectedIndex(), 0);
    root.OnMouseUp(150, 50 + ILV_HEADER + 2, 1);
}

TEST(listview_right_button_does_not_select) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    list->SetID(2002);
    list->SetPID(12);
    ILvFill(list, 5);

    HostEventReset();
    list->OnMouseDown(50, ILV_HEADER + 2, 3);
    CHECK_EQ(list->GetSelectedIndex(), -1);
    CHECK_EQ(HostEventCount(), 0);
}

TEST(listview_wheel_extreme_delta_is_safe) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    ILvFill(list, 50);

    const int maxOff = 50 - list->GetVisibleItemCount();
    list->OnMouseWheel(0x7FFFFFFF);
    CHECK_EQ(list->GetScrollOffset(), 0);
    list->OnMouseWheel((int32_t)0x80000000u);
    CHECK_EQ(list->GetScrollOffset(), maxOff);
}

TEST(listview_release_outside_ends_thumb_drag) {
    CompositeWidget root(nullptr, 0, 0, 400, 400);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    ILvFill(list, 50);

    // Grab the thumb (starts at y=23, 10px tall) and drag it down.
    root.OnMouseDown(195, ILV_HEADER + 5, 1);
    CHECK(list->IsMouseCaptured());
    root.OnMouseMove(195, ILV_HEADER + 5, 195, ILV_HEADER + 65);
    CHECK_EQ(list->GetScrollOffset(), 28);

    // Release outside the list; capture must deliver the mouse-up.
    root.OnMouseUp(390, 390, 1);
    CHECK(!list->IsMouseCaptured());

    int afterRelease = list->GetScrollOffset();
    root.OnMouseMove(390, 390, 195, ILV_HEADER + 5);
    CHECK_EQ(list->GetScrollOffset(), afterRelease);
}

TEST(listview_home_end_and_empty_selection_keys) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    ILvFill(list, 50);

    CHECK_EQ(list->GetSelectedIndex(), -1);
    list->OnSpecialKeyDown(0x4F);  // End.
    CHECK_EQ(list->GetSelectedIndex(), 49);
    CHECK(list->GetScrollOffset() > 0);

    list->OnSpecialKeyDown(0x47);  // Home.
    CHECK_EQ(list->GetSelectedIndex(), 0);
    CHECK_EQ(list->GetScrollOffset(), 0);

    list->SetSelectedIndex(-1);
    list->OnSpecialKeyDown(0x48);  // Up with no selection selects the last row.
    CHECK_EQ(list->GetSelectedIndex(), 49);

    list->SetSelectedIndex(-1);
    list->OnSpecialKeyDown(0x50);  // Down with no selection selects the first.
    CHECK_EQ(list->GetSelectedIndex(), 0);
}

// ---------------------------------------------------------------------------
// Composite routing: visibility, press/release pairing, capture.
// ---------------------------------------------------------------------------

TEST(hidden_child_receives_no_mouse_events) {
    CompositeWidget root(nullptr, 0, 0, 200, 100);
    ClickProbe* hidden = new ClickProbe(&root, 0, 0, 50, 100);
    ClickProbe* shown = new ClickProbe(&root, 50, 0, 100, 100);
    hidden->isVisible = false;
    root.AddChild(hidden);
    root.AddChild(shown);

    root.OnMouseDown(10, 10, 1);
    CHECK_EQ(hidden->downs, 0);
    root.OnMouseMove(0, 0, 10, 10);
    CHECK_EQ(hidden->enters, 0);

    root.OnMouseDown(60, 10, 1);
    CHECK_EQ(shown->downs, 1);
}

TEST(disabled_composite_blocks_child_input) {
    CompositeWidget root(nullptr, 0, 0, 100, 100);
    ClickProbe* child = new ClickProbe(&root, 0, 0, 50, 50);
    root.AddChild(child);

    root.SetEnabled(false);
    root.OnMouseDown(10, 10, 1);
    root.OnMouseUp(10, 10, 1);
    root.OnMouseWheel(1);
    root.OnKeyDown("a");
    CHECK_EQ(child->downs, 0);
    CHECK_EQ(child->ups, 0);
    CHECK_EQ(child->wheels, 0);

    root.SetEnabled(true);
    root.OnMouseDown(10, 10, 1);
    CHECK_EQ(child->downs, 1);
}

TEST(click_updates_wheel_target_without_a_move) {
    CompositeWidget root(nullptr, 0, 0, 100, 100);
    ClickProbe* a = new ClickProbe(&root, 0, 0, 50, 50);
    ClickProbe* b = new ClickProbe(&root, 50, 50, 50, 50);
    root.AddChild(a);
    root.AddChild(b);

    // Click b, then scroll without moving the pointer: the wheel must go to b.
    root.OnMouseDown(60, 60, 1);
    root.OnMouseUp(60, 60, 1);
    root.OnMouseWheel(-1);
    CHECK_EQ(a->wheels, 0);
    CHECK_EQ(b->wheels, 1);
}

TEST(captured_child_receives_release_outside_bounds) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ClickProbe* child = new ClickProbe(&root, 0, 0, 50, 50);
    child->captureOnDown = true;
    root.AddChild(child);

    root.OnMouseDown(10, 10, 1);
    CHECK_EQ(child->downs, 1);
    CHECK(child->IsMouseCaptured());

    root.OnMouseUp(250, 250, 1);
    CHECK_EQ(child->ups, 1);
    CHECK(!child->IsMouseCaptured());

    root.OnMouseUp(250, 250, 1);
    CHECK_EQ(child->ups, 1);
}

TEST(press_elsewhere_then_release_on_checkbox_does_not_toggle) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    CheckBox* box = new CheckBox(&root, 100, 100, 120, 24, "Enable");
    root.AddChild(box);
    box->SetID(2100);
    box->SetPID(13);

    HostEventReset();
    root.OnMouseDown(10, 10, 1);  // Background: no widget pressed.
    root.OnMouseUp(110, 110, 1);  // Release lands on the checkbox.
    CHECK(!box->GetChecked());
    CHECK_EQ(HostEventCount(), 0);

    root.OnMouseDown(110, 110, 1);  // A real click still works.
    root.OnMouseUp(110, 110, 1);
    CHECK(box->GetChecked());
    CHECK_EQ(HostEventCount(), 1);
}

TEST(disabled_button_does_not_emit) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    Button* button = new Button(&root, 10, 10, 100, 30, "Click");
    root.AddChild(button);
    button->SetID(2200);
    button->SetPID(14);
    button->SetEnabled(false);

    HostEventReset();
    root.OnMouseDown(20, 20, 1);
    root.OnMouseUp(20, 20, 1);
    CHECK_EQ(HostEventCount(), 0);

    button->SetEnabled(true);
    root.OnMouseDown(20, 20, 1);
    root.OnMouseUp(20, 20, 1);
    CHECK_EQ(HostEventCount(), 1);
}

TEST(combobox_closes_when_focus_moves_away) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ComboBox* combo = new ComboBox(&root, 0, 0, 120, 24);
    Button* button = new Button(&root, 0, 100, 100, 30, "Other");
    root.AddChild(combo);
    root.AddChild(button);
    combo->AddItem("Alpha");
    combo->AddItem("Beta");

    root.OnMouseDown(10, 10, 1);
    root.OnMouseUp(10, 10, 1);
    CHECK(combo->IsOpen());

    // Pressing a sibling moves focus, which must collapse the drop-down.
    root.OnMouseDown(10, 110, 1);
    CHECK(!combo->IsOpen());
    root.OnMouseUp(10, 110, 1);
}

TEST(button_ignores_non_primary_buttons) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    Button* button = new Button(&root, 10, 10, 100, 30, "Click");
    root.AddChild(button);
    button->SetID(2250);
    button->SetPID(16);

    HostEventReset();
    root.OnMouseDown(20, 20, 3);  // Right button.
    root.OnMouseUp(20, 20, 3);
    CHECK_EQ(HostEventCount(), 0);

    root.OnMouseDown(20, 20, 1);
    root.OnMouseUp(20, 20, 1);
    CHECK_EQ(HostEventCount(), 1);
}

TEST(tabs_click_is_parent_relative) {
    CompositeWidget root(nullptr, 0, 0, 400, 400);
    Tabs* tabs = new Tabs(&root, 100, 50, 300, 200);
    root.AddChild(tabs);
    tabs->SetID(2301);
    tabs->SetPID(15);
    tabs->AddTab("One");
    tabs->AddTab("Two");

    // Second header of a Tabs placed at (100, 50).
    root.OnMouseDown(100 + 90, 50 + 10, 1);
    root.OnMouseUp(100 + 90, 50 + 10, 1);
    CHECK_EQ(tabs->GetActive(), 1);
}

TEST(tabs_release_without_press_does_not_select) {
    CompositeWidget root(nullptr, 0, 0, 300, 200);
    Tabs* tabs = new Tabs(&root, 0, 0, 300, 200);
    root.AddChild(tabs);
    tabs->SetID(2300);
    tabs->SetPID(15);
    tabs->AddTab("One");
    tabs->AddTab("Two");
    tabs->AddTab("Three");

    HostEventReset();
    tabs->OnMouseUp(90, 10, 1);  // Release only: no matching press.
    CHECK_EQ(tabs->GetActive(), 0);
    CHECK_EQ(HostEventCount(), 0);

    tabs->OnMouseDown(90, 10, 1);
    tabs->OnMouseUp(90, 10, 1);
    CHECK_EQ(tabs->GetActive(), 1);
    CHECK_EQ(HostEventCount(), 1);
}

TEST(window_title_drag_does_not_forward_shifted_moves) {
    CompositeWidget root(nullptr, 0, 0, 400, 400);
    Window* win = new Window(&root, 50, 50, 200, 150);
    root.AddChild(win);
    ClickProbe* probe = new ClickProbe(win, 0, 0, 100, 100);
    win->AddChild(probe);

    // Press the title bar and drag the window ten pixels right. The pointer is
    // stationary relative to the window, so the child must receive no move.
    root.OnMouseDown(60, 60, 1);
    root.OnMouseMove(60, 60, 70, 60);
    CHECK_EQ(win->x, 60);
    CHECK_EQ(probe->moves, 0);
    CHECK_EQ(probe->enters, 0);
}

// ---------------------------------------------------------------------------
// Nested composite rendering.
// ---------------------------------------------------------------------------

TEST(tabs_content_area_is_composited) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    Tabs* tabs = new Tabs(&root, 0, 0, 200, 150);
    root.AddChild(tabs);
    tabs->AddTab("One");

    SolidWidget* solid = new SolidWidget(tabs->GetContentArea(), 0, 0, 50, 20, 0xFFFF0000u);
    tabs->GetContentArea()->AddChild(solid);

    tabs->RedrawToCache();
    CHECK(tabs->cache != nullptr);
    const int y = TABS_HEIGHT + 5;
    CHECK_EQ(tabs->cache[y * tabs->w + 5], 0xFFFF0000u);
}

TEST(scrollcontainer_content_is_composited_and_scrolls) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ScrollContainer* sc = new ScrollContainer(&root, 0, 0, 200, 150);
    root.AddChild(sc);
    sc->SetContentSize(500, 500);

    SolidWidget* solid = new SolidWidget(sc->GetContent(), 0, 0, 50, 50, 0xFF00FF00u);
    sc->GetContent()->AddChild(solid);

    sc->RedrawToCache();
    CHECK(sc->cache != nullptr);
    CHECK_EQ(sc->cache[10 * sc->w + 10], 0xFF00FF00u);

    // Scrolling by 100 moves the content up out of the sampled pixel.
    sc->GetVerticalBar()->SetValue(100);
    sc->RedrawToCache();
    CHECK_EQ(sc->cache[10 * sc->w + 10], 0u);
}

TEST(imageview_rejects_overflowing_source_dimensions) {
    CompositeWidget root(nullptr, 0, 0, 50, 50);
    uint32_t dummy = 0xFFFF00FFu;
    ImageView* view = new ImageView(&root, 0, 0, 10, 10, &dummy, 0x7FFFFFFF, 0x7FFFFFFF);
    root.AddChild(view);

    // The source is rejected (its pixel count does not fit 32 bits), so the
    // widget renders empty instead of attempting a multi-gigabyte copy.
    view->RedrawToCache();
    CHECK(view->cache != nullptr);
    CHECK_EQ(view->cache[0], 0u);
}

TEST(scrollcontainer_shrink_resets_scroll) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ScrollContainer* sc = new ScrollContainer(&root, 0, 0, 200, 150);
    root.AddChild(sc);

    sc->SetContentSize(50, 1000);
    sc->OnMouseWheel(-100000);
    CHECK(sc->GetScrollY() > 0);

    // Content that now fits must snap back to the origin instead of leaving
    // the viewport parked below a zero-length scroll range.
    sc->SetContentSize(10, 10);
    CHECK_EQ(sc->GetScrollY(), 0);

    sc->OnMouseWheel(-1000);
    CHECK_EQ(sc->GetScrollY(), 0);
}

TEST(scrollcontainer_extreme_wheel_is_safe) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ScrollContainer* sc = new ScrollContainer(&root, 0, 0, 200, 150);
    root.AddChild(sc);
    sc->SetContentSize(50, 1000);  // Page 138, max scroll 862.

    sc->OnMouseWheel(0x7FFFFFFF);
    CHECK_EQ(sc->GetScrollY(), 0);
    sc->OnMouseWheel((int32_t)0x80000000u);
    CHECK_EQ(sc->GetScrollY(), 862);
}

TEST(mathutil_muldiv_is_exact_without_64bit_helpers) {
    CHECK_EQ(GuiMulDivU32(8720, 100), 87u);
    CHECK_EQ(GuiMulDivU32(12000000000ULL, 7), 1714285714u);
    CHECK_EQ(GuiMulDivU32(123, 0), 0u);
    CHECK_EQ(GuiMulDivSigned(-39, 75, 195), -15);
    CHECK_EQ(GuiMulDivSigned(39, -75, 195), -15);
    CHECK_EQ(GuiMulDivSigned(-39, -75, 195), 15);
    CHECK_EQ(GuiMulDivSigned(39, 75, 0), 0);
}

// ---------------------------------------------------------------------------
// Widget hit-testing, deferred window destruction, and context menus.
// ---------------------------------------------------------------------------

namespace {

Widget* g_trackedWidget = nullptr;
int g_trackedDestroyed = 0;

void TrackDestruction(Widget* widget) {
    if (widget == g_trackedWidget) g_trackedDestroyed++;
}

void MenuChoiceHandler(ContextMenu*, int32_t index, void* user) {
    if (user) *(int*)user = index;
}

}  // namespace

TEST(hittest_returns_deepest_visible_widget) {
    CompositeWidget root(nullptr, 0, 0, 200, 200);
    CompositeWidget* panel = new CompositeWidget(&root, 10, 10, 100, 100);
    ClickProbe* child = new ClickProbe(panel, 5, 5, 20, 20);
    root.AddChild(panel);
    panel->AddChild(child);

    CHECK(root.HitTest(20, 20) == child);
    CHECK(root.HitTest(90, 90) == panel);
    CHECK(root.HitTest(0, 0) == &root);
    CHECK(root.HitTest(300, 300) == nullptr);

    child->isVisible = false;
    CHECK(root.HitTest(20, 20) == panel);
}

TEST(window_close_defers_destruction) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    Window* win = new Window(&root, 0, 0, 120, 90);
    root.AddChild(win);

    void (*oldNotify)(Widget*) = Widget::destructorNotify;
    g_trackedWidget = win;
    g_trackedDestroyed = 0;
    Widget::destructorNotify = &TrackDestruction;

    win->OnClose();
    CHECK_EQ(root.childrenList.GetSize(), 0u);
    CHECK(!win->isVisible);
    CHECK_EQ(g_trackedDestroyed, 0);  // Still alive until the deferred drain.

    DrainDeferredWidgetDeletes();
    CHECK_EQ(g_trackedDestroyed, 1);

    Widget::destructorNotify = oldNotify;
    g_trackedWidget = nullptr;
}

TEST(window_close_keeps_reusable_dialog_attached) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    Window* win = new Window(&root, 0, 0, 120, 90);
    root.AddChild(win);
    win->SetDeleteOnClose(false);

    void (*oldNotify)(Widget*) = Widget::destructorNotify;
    g_trackedWidget = win;
    g_trackedDestroyed = 0;
    Widget::destructorNotify = &TrackDestruction;

    win->OnClose();
    CHECK(!win->isVisible);
    CHECK_EQ(root.childrenList.GetSize(), 1u);
    DrainDeferredWidgetDeletes();
    CHECK_EQ(g_trackedDestroyed, 0);  // Cached dialog survives for reuse.

    Widget::destructorNotify = oldNotify;
    g_trackedWidget = nullptr;
}

TEST(contextmenu_dismisses_on_outside_press) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ContextMenu* menu = new ContextMenu(&root);
    root.AddChild(menu);
    CHECK(menu->AddItem("One"));
    CHECK(menu->AddItem("Two"));

    menu->ShowAt(40, 40);
    CHECK(menu->isVisible);

    ContextMenu::DismissActiveAt(10, 10);  // Outside.
    CHECK(!menu->isVisible);

    menu->ShowAt(40, 40);
    ContextMenu::DismissActiveAt(45, 45);  // Inside: stays open.
    CHECK(menu->isVisible);

    ContextMenu::HideActive();
    CHECK(!menu->isVisible);
}

TEST(contextmenu_click_handler_reports_index) {
    CompositeWidget root(nullptr, 0, 0, 300, 300);
    ContextMenu* menu = new ContextMenu(&root);
    root.AddChild(menu);
    menu->AddItem("Alpha");
    menu->AddItem("Beta");

    int chosen = -1;
    menu->SetClickHandler(&MenuChoiceHandler, &chosen);
    menu->ShowAt(0, 0);
    menu->OnMouseUp(5, 24 + 5, 1);
    CHECK_EQ(chosen, 1);
    CHECK(!menu->isVisible);
}

// ---------------------------------------------------------------------------
// Raster clipping. These run last: constructing a local NINA here would leave
// NINA::activeInstance pointing at a dead object for any later test.
// ---------------------------------------------------------------------------

TEST(raster_fill_rectangle_clips_negative_origin) {
    uint32_t buffer[100];
    for (uint32_t& px : buffer) px = 0xFF000000u;

    // Entirely left of the surface: nothing may be painted.
    TestRaster().FillRectangle(buffer, 10, 10, -5, 0, 3, 1, 0xFFFF0000u);
    CHECK_EQ(buffer[0], 0xFF000000u);

    // Straddling the left edge: only x=0..2 are inside.
    TestRaster().FillRectangle(buffer, 10, 10, -5, 1, 8, 1, 0xFFFF0000u);
    CHECK_EQ(buffer[1 * 10 + 0], 0xFFFF0000u);
    CHECK_EQ(buffer[1 * 10 + 2], 0xFFFF0000u);
    CHECK_EQ(buffer[1 * 10 + 3], 0xFF000000u);
}

TEST(raster_fill_rectangle_nonpositive_size_draws_nothing) {
    uint32_t buffer[100];
    for (uint32_t& px : buffer) px = 0xFF000000u;

    TestRaster().FillRectangle(buffer, 10, 10, 0, 0, -1, 5, 0xFFFF0000u);
    TestRaster().FillRectangle(buffer, 10, 10, 0, 0, 5, -1, 0xFFFF0000u);
    TestRaster().FillRectangle(buffer, 10, 10, 0, 0, 0, 0, 0xFFFF0000u);
    for (uint32_t px : buffer) CHECK_EQ(px, 0xFF000000u);

    // A one-pixel rounded rectangle must not wrap its `w - 2*radius` fills.
    TestRaster().FillRoundedRectangle(buffer, 10, 10, 0, 0, 1, 1, 8, 0xFF00FF00u);
    CHECK_EQ(buffer[0], 0xFF00FF00u);
    CHECK_EQ(buffer[1], 0xFF000000u);
    CHECK_EQ(buffer[10], 0xFF000000u);
}
