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

// Edge-case coverage for the scrolling widgets: out-of-bounds geometry, render
// bounds, scroll-offset clamping, selection bounds, and hostile inputs. Every
// test runs under ASan/UBSan, so an out-of-range write, a division by zero, or
// a signed overflow aborts the run instead of passing silently.

#include "../framework.h"
#include "../harness.h"

#include <cstdio>
#include <cstring>

#include <gui/listview.h>
#include <gui/scrollbar.h>
#include <gui/scrollcontainer.h>

namespace {

constexpr int LV_HEADER = LISTVIEW_HEADER_HEIGHT;

// Append @count rows named "row 0".."row N" in a single column.
void LvFill(ListView* list, int count) {
    for (int i = 0; i < count; i++) {
        char name[24];
        std::snprintf(name, sizeof(name), "row %d", i);
        const char* cells[] = {name};
        list->AddItem(cells, 1, 0);
    }
}

// True when the row at @index is fully within the visible window.
bool LvRowVisible(ListView* list, int index) {
    int off = list->GetScrollOffset();
    return index >= off && index < off + list->GetVisibleItemCount();
}

}  // namespace

// ---------------------------------------------------------------------------
// Out-of-bounds geometry and rendering.
// ---------------------------------------------------------------------------

TEST(listview_zero_size_is_render_safe) {
    CompositeWidget root(nullptr, 0, 0, 10, 10);
    ListView* list = new ListView(&root, 0, 0, 0, 0);
    root.AddChild(list);
    CHECK(list->cache == nullptr);

    list->AddColumn("Name", 0);
    const char* cells[] = {"x"};
    list->AddItem(cells, 1, 0);
    CHECK_EQ(list->GetItemCount(), 1);

    list->RedrawToCache();
    list->SetSelectedIndex(0);
    list->OnSpecialKeyDown(0x50);
    list->OnMouseDown(0, 0, 1);
    CHECK_EQ(list->GetItemCount(), 1);
}

TEST(listview_negative_size_is_render_safe) {
    CompositeWidget root(nullptr, 0, 0, 10, 10);
    ListView* list = new ListView(&root, 0, 0, -20, -30);
    root.AddChild(list);
    CHECK(list->cache == nullptr);
    list->AddColumn("Name", 0);
    LvFill(list, 3);
    list->RedrawToCache();
    CHECK_EQ(list->GetItemCount(), 3);
}

TEST(listview_tiny_size_render_is_safe) {
    CompositeWidget root(nullptr, 0, 0, 10, 10);
    ListView* list = new ListView(&root, 0, 0, 1, 1);
    root.AddChild(list);
    CHECK(list->cache != nullptr);
    list->AddColumn("Name", 0);
    LvFill(list, 5);
    list->RedrawToCache();
    list->SetItemHeight(10);
    list->RedrawToCache();
    CHECK_EQ(list->GetItemCount(), 5);
}

TEST(listview_fixed_column_wider_than_widget_is_bounded) {
    CompositeWidget root(nullptr, 0, 0, 40, 130);
    ListView* list = new ListView(&root, 0, 0, 40, 130);
    root.AddChild(list);
    list->AddColumn("Name", 0);
    list->AddColumn("Size", 90);  // Fixed column wider than the whole widget.
    LvFill(list, 6);
    list->RedrawToCache();
    CHECK(list->cache != nullptr);
    // Deterministic: a second render must be byte-identical.
    uint32_t before[40 * 130];
    std::memcpy(before, list->cache, sizeof(before));
    list->MarkDirty();
    list->RedrawToCache();
    CHECK_EQ(std::memcmp(before, list->cache, sizeof(before)), 0);
}

TEST(listview_columns_summing_past_width_is_bounded) {
    CompositeWidget root(nullptr, 0, 0, 50, 120);
    ListView* list = new ListView(&root, 0, 0, 50, 120);
    root.AddChild(list);
    for (int c = 0; c < LISTVIEW_MAX_COLUMNS; c++) list->AddColumn("col", 100);
    const char* cells[] = {"a", "b", "c", "d"};
    list->AddItem(cells, 4, 0);
    list->RedrawToCache();
    CHECK_EQ(list->GetColumnCount(), LISTVIEW_MAX_COLUMNS);
}

TEST(listview_render_is_bounded_by_widget) {
    // A tall, sparse list with a scrollbar must never write outside its cache.
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    list->AddColumn("Name", 0);
    LvFill(list, 60);
    list->SetSelectedIndex(30);
    list->RedrawToCache();
    CHECK(list->cache != nullptr);
    CHECK_EQ(list->GetItemCount(), 60);
}

// ---------------------------------------------------------------------------
// Item and column capacity.
// ---------------------------------------------------------------------------

TEST(listview_additem_caps_at_maximum) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    list->AddColumn("Name", 0);
    LvFill(list, LISTVIEW_MAX_ITEMS + 10);
    CHECK_EQ(list->GetItemCount(), LISTVIEW_MAX_ITEMS);
}

TEST(listview_additem_handles_null_cells) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    list->AddItem(nullptr, 0, 0);
    list->AddItem(nullptr, 3, 0);
    const char* mixed[] = {"n0", nullptr, "n2"};
    list->AddItem(mixed, 3, 0);
    CHECK_EQ(list->GetItemCount(), 3);

    const ListViewItem* it = list->GetItem(2);
    CHECK(it != nullptr);
    CHECK(std::strcmp(it->cells[0], "n0") == 0);
    CHECK(std::strcmp(it->cells[1], "") == 0);
    CHECK(std::strcmp(it->cells[2], "n2") == 0);
}

TEST(listview_additem_clamps_extra_cells) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    // Eight cells offered; only the first MAX_COLUMNS may be read.
    const char* cells8[] = {"a", "b", "c", "d", "e", "f", "g", "h"};
    list->AddItem(cells8, 8, 0);
    const ListViewItem* it = list->GetItem(0);
    CHECK(it != nullptr);
    CHECK(std::strcmp(it->cells[0], "a") == 0);
    CHECK(std::strcmp(it->cells[3], "d") == 0);
}

TEST(listview_additem_truncates_long_cell) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    char big[200];
    for (int i = 0; i < 199; i++) big[i] = 'x';
    big[199] = 0;
    const char* cells[] = {big};
    list->AddItem(cells, 1, 0);
    const ListViewItem* it = list->GetItem(0);
    CHECK(it != nullptr);
    CHECK_EQ((int)std::strlen(it->cells[0]), LISTVIEW_CELL_LEN - 1);
}

TEST(listview_addcolumn_negative_width_is_auto) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    CHECK(list->AddColumn("A", -5));
    CHECK_EQ(list->GetColumnCount(), 1);
    CHECK(list->AddColumn("B", 0));
    CHECK_EQ(list->GetColumnCount(), 2);
    LvFill(list, 4);
    list->RedrawToCache();
    CHECK(list->cache != nullptr);
}

// ---------------------------------------------------------------------------
// Selection bounds.
// ---------------------------------------------------------------------------

TEST(listview_setselected_rejects_out_of_range) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 3);

    CHECK(!list->SetSelectedIndex(3));
    CHECK(!list->SetSelectedIndex(1000));
    CHECK(!list->SetSelectedIndex(-2));
    CHECK(list->SetSelectedIndex(2));
    CHECK_EQ(list->GetSelectedIndex(), 2);
    CHECK(list->SetSelectedIndex(-1));
    CHECK_EQ(list->GetSelectedIndex(), -1);
}

TEST(listview_setselected_on_empty_list) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    CHECK(!list->SetSelectedIndex(0));
    CHECK(list->SetSelectedIndex(-1));
}

TEST(listview_selection_scrolls_into_view) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 50);
    CHECK_EQ(list->GetVisibleItemCount(), 5);

    CHECK(list->SetSelectedIndex(40));
    CHECK(LvRowVisible(list, 40));
    CHECK_EQ(list->GetScrollOffset(), 36);

    CHECK(list->SetSelectedIndex(0));
    CHECK_EQ(list->GetScrollOffset(), 0);

    CHECK(list->SetSelectedIndex(49));
    CHECK(LvRowVisible(list, 49));
    CHECK_EQ(list->GetScrollOffset(), 45);
}

TEST(listview_clear_resets_scroll_and_selection) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    list->AddColumn("Name", 0);
    LvFill(list, 50);
    list->SetSelectedIndex(40);
    CHECK(list->GetScrollOffset() > 0);

    list->Clear();
    CHECK_EQ(list->GetItemCount(), 0);
    CHECK_EQ(list->GetSelectedIndex(), -1);
    CHECK_EQ(list->GetScrollOffset(), 0);
    CHECK_EQ(list->GetColumnCount(), 1);  // Columns survive a Clear.
}

// ---------------------------------------------------------------------------
// Keyboard navigation and scrolling.
// ---------------------------------------------------------------------------

TEST(listview_down_arrow_navigates_and_scrolls) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 50);
    list->SetSelectedIndex(0);

    for (int i = 1; i <= 8; i++) {
        list->OnSpecialKeyDown(0x50);  // Down.
        CHECK_EQ(list->GetSelectedIndex(), i);
        CHECK(LvRowVisible(list, i));
        CHECK(list->GetScrollOffset() <= list->GetSelectedIndex());
    }
    // Past the fold the view must have scrolled.
    CHECK(list->GetScrollOffset() > 0);
}

TEST(listview_up_arrow_clamps_at_top) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 10);
    list->SetSelectedIndex(0);
    list->OnSpecialKeyDown(0x48);  // Up.
    CHECK_EQ(list->GetSelectedIndex(), 0);
    CHECK_EQ(list->GetScrollOffset(), 0);
}

TEST(listview_down_arrow_clamps_at_bottom) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 10);
    list->SetSelectedIndex(9);
    list->OnSpecialKeyDown(0x50);  // Down.
    CHECK_EQ(list->GetSelectedIndex(), 9);
}

TEST(listview_arrows_on_empty_list_are_safe) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    list->OnSpecialKeyDown(0x50);
    list->OnSpecialKeyDown(0x48);
    CHECK_EQ(list->GetSelectedIndex(), -1);
    CHECK_EQ(list->GetScrollOffset(), 0);
}

TEST(listview_enter_emits_click_only_when_selected) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    list->SetID(700);
    list->SetPID(5);
    LvFill(list, 3);

    list->SetSelectedIndex(-1);
    HostEventReset();
    list->OnKeyDown("\r");
    CHECK_EQ(HostEventCount(), 0);

    list->SetSelectedIndex(1);
    HostEventReset();
    list->OnKeyDown("\r");
    CHECK_EQ(HostEventCount(), 1);
    if (HostEventCount() == 1) CHECK_EQ(HostEventAt(0).widgetID, 700u);
}

TEST(listview_key_ignored_when_hidden) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    list->SetID(701);
    list->SetPID(5);
    LvFill(list, 3);
    list->SetSelectedIndex(1);

    list->isVisible = false;
    HostEventReset();
    list->OnKeyDown(" ");
    CHECK_EQ(HostEventCount(), 0);

    list->OnKeyDown("\n");
    CHECK_EQ(HostEventCount(), 0);
    list->isVisible = true;
    list->OnKeyDown("\n");
    CHECK_EQ(HostEventCount(), 1);
}

// ---------------------------------------------------------------------------
// Mouse selection and bounds.
// ---------------------------------------------------------------------------

TEST(listview_click_selects_correct_row) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    list->SetID(702);
    list->SetPID(5);
    LvFill(list, 10);

    HostEventReset();
    list->OnMouseDown(50, LV_HEADER + 1, 1);
    CHECK_EQ(list->GetSelectedIndex(), 0);
    CHECK_EQ(HostEventCount(), 1);

    int row3Y = LV_HEADER + 1 + 3 * list->GetItemHeight() + 2;
    list->OnMouseDown(50, row3Y, 1);
    CHECK_EQ(list->GetSelectedIndex(), 3);
}

TEST(listview_click_on_header_is_ignored) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    list->SetID(703);
    list->SetPID(5);
    LvFill(list, 10);
    list->SetSelectedIndex(-1);

    HostEventReset();
    list->OnMouseDown(50, LV_HEADER - 5, 1);
    CHECK_EQ(list->GetSelectedIndex(), -1);
    CHECK_EQ(HostEventCount(), 0);
}

TEST(listview_click_below_last_item_is_ignored) {
    CompositeWidget root(nullptr, 0, 0, 200, 300);
    ListView* list = new ListView(&root, 0, 0, 200, 300);
    root.AddChild(list);
    list->SetID(704);
    list->SetPID(5);
    LvFill(list, 3);
    list->SetSelectedIndex(-1);

    HostEventReset();
    // Well inside the widget, but past the last row.
    list->OnMouseDown(50, LV_HEADER + 1 + 8 * list->GetItemHeight(), 1);
    CHECK_EQ(list->GetSelectedIndex(), -1);
    CHECK_EQ(HostEventCount(), 0);
}

TEST(listview_click_scrollbar_region_is_not_a_row) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 30);

    list->SetSelectedIndex(-1);
    list->OnMouseDown(199, LV_HEADER + 10, 1);
    CHECK_EQ(list->GetSelectedIndex(), -1);
}

TEST(listview_wheel_scrolls_and_clamps) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 50);
    const int maxOff = 50 - list->GetVisibleItemCount();

    CHECK_EQ(list->GetScrollOffset(), 0);
    list->OnMouseWheel(-3);                // Wheel toward the end.
    CHECK_EQ(list->GetScrollOffset(), 9);  // 3 notches x 3 rows.
    list->OnMouseWheel(100000);            // Slam to the top.
    CHECK_EQ(list->GetScrollOffset(), 0);
    list->OnMouseWheel(-100000);  // Slam to the end.
    CHECK_EQ(list->GetScrollOffset(), maxOff);
}

TEST(listview_scrollbar_track_click_pages) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 50);  // Scrollbar present: 5 visible of 50.
    CHECK(list->GetItemCount() > list->GetVisibleItemCount());

    // A press far below the thumb pages down by one viewport.
    list->OnMouseDown(196, 120, 1);
    CHECK_EQ(list->GetScrollOffset(), list->GetVisibleItemCount());

    // A press above the thumb pages back up.
    list->OnMouseDown(196, LV_HEADER + 1, 1);
    CHECK_EQ(list->GetScrollOffset(), 0);
}

TEST(listview_scrollbar_track_click_clamps) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 50);
    const int maxOff = 50 - list->GetVisibleItemCount();

    list->OnMouseWheel(-100000);  // Already at the bottom.
    CHECK_EQ(list->GetScrollOffset(), maxOff);

    // Paging down again must stay clamped at the last full page.
    list->OnMouseDown(196, 129, 1);
    CHECK_EQ(list->GetScrollOffset(), maxOff);
}

TEST(listview_click_negative_coordinates_is_safe) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    list->SetID(705);
    list->SetPID(5);
    LvFill(list, 5);
    list->SetSelectedIndex(-1);

    HostEventReset();
    list->OnMouseDown(-50, -50, 1);
    list->OnMouseMove(0, 0, -50, -50);
    list->OnMouseUp(-50, -50, 1);
    CHECK_EQ(list->GetSelectedIndex(), -1);
    CHECK_EQ(HostEventCount(), 0);
}

TEST(listview_click_last_row_boundary) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 5);  // Rows 0..4, exactly the visible count.
    CHECK_EQ(list->GetVisibleItemCount(), 5);

    int lastRowY = LV_HEADER + 1 + 4 * list->GetItemHeight() + 2;
    list->OnMouseDown(50, lastRowY, 1);
    CHECK_EQ(list->GetSelectedIndex(), 4);

    // One pixel into the row after the last must not select anything.
    int pastLastY = LV_HEADER + 1 + 5 * list->GetItemHeight() + 2;
    list->OnMouseDown(50, pastLastY, 1);
    CHECK_EQ(list->GetSelectedIndex(), 4);
}

// ---------------------------------------------------------------------------
// Hover and dragging.
// ---------------------------------------------------------------------------

TEST(listview_hover_repaints_only_when_focused) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 10);
    list->SetFocus(true);
    list->RedrawToCache();
    CHECK(list->cache != nullptr);

    size_t bytes = (size_t)list->w * list->h * sizeof(uint32_t);
    uint32_t* base = new uint32_t[bytes / sizeof(uint32_t)];
    std::memcpy(base, list->cache, bytes);

    list->OnMouseMove(0, 0, 50, LV_HEADER + 2);
    list->RedrawToCache();
    CHECK(std::memcmp(base, list->cache, bytes) != 0);

    // Re-hovering the same row changes nothing.
    uint32_t* hovered = new uint32_t[bytes / sizeof(uint32_t)];
    std::memcpy(hovered, list->cache, bytes);
    list->OnMouseMove(0, 0, 50, LV_HEADER + 2);
    list->RedrawToCache();
    CHECK_EQ(std::memcmp(hovered, list->cache, bytes), 0);

    // Leaving the rows restores the un-hovered image.
    list->OnMouseMove(0, 0, 50, 300);
    list->RedrawToCache();
    CHECK_EQ(std::memcmp(base, list->cache, bytes), 0);

    delete[] base;
    delete[] hovered;
}

TEST(listview_hover_ignored_when_unfocused) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 10);
    CHECK(!list->IsFocused());
    list->RedrawToCache();

    size_t bytes = (size_t)list->w * list->h * sizeof(uint32_t);
    uint32_t* base = new uint32_t[bytes / sizeof(uint32_t)];
    std::memcpy(base, list->cache, bytes);

    list->OnMouseMove(0, 0, 50, LV_HEADER + 2);
    list->RedrawToCache();
    CHECK_EQ(std::memcmp(base, list->cache, bytes), 0);
    delete[] base;
}

TEST(listview_scrollbar_drag_scrolls_and_clamps) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 50);
    list->SetFocus(true);
    CHECK_EQ(list->GetScrollOffset(), 0);

    // Grab the thumb (spans y=23..33 at offset 0), then drag it down 60px.
    list->OnMouseDown(195, 28, 1);
    list->OnMouseMove(0, 28, 195, 88);
    CHECK_EQ(list->GetScrollOffset(), 28);

    // Drag far past the ends; the offset must clamp to [0, maxOff].
    list->OnMouseMove(0, 28, 195, 100000);
    CHECK_EQ(list->GetScrollOffset(), 45);
    list->OnMouseMove(0, 28, 195, -100000);
    CHECK_EQ(list->GetScrollOffset(), 0);

    list->OnMouseUp(195, 28, 1);
    list->OnMouseMove(0, 28, 195, 88);
    CHECK_EQ(list->GetScrollOffset(), 0);  // No longer dragging.
}

TEST(listview_mousedown_grants_focus) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 10);
    CHECK(!list->IsFocused());

    // A press focuses the widget, so the subsequent move can drag/hover.
    list->OnMouseDown(50, LV_HEADER + 2, 1);
    CHECK(list->IsFocused());
}

// ---------------------------------------------------------------------------
// Row-height edge cases.
// ---------------------------------------------------------------------------

TEST(listview_setitemheight_clamps_low) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 5);
    list->SetItemHeight(0);
    CHECK_EQ(list->GetItemHeight(), 10);
    list->SetItemHeight(-5);
    CHECK_EQ(list->GetItemHeight(), 10);
    list->RedrawToCache();
    CHECK(list->cache != nullptr);
}

TEST(listview_setitemheight_clamps_high) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 5);
    list->SetItemHeight(100000);
    // Clamped to the content height (130 - header - 2 = 106).
    CHECK_EQ(list->GetItemHeight(), 106);
    CHECK_EQ(list->GetVisibleItemCount(), 1);
    list->RedrawToCache();
    CHECK(list->cache != nullptr);
}

TEST(listview_setitemheight_reclamps_scroll_offset) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 50);
    list->SetSelectedIndex(49);  // Scrolls to offset 45 (5 visible rows).
    CHECK_EQ(list->GetScrollOffset(), 45);

    // More rows on screen means the maximum offset shrinks.
    list->SetItemHeight(10);
    CHECK_EQ(list->GetVisibleItemCount(), 10);
    CHECK_EQ(list->GetScrollOffset(), 40);
}

// ---------------------------------------------------------------------------
// Scroll and render determinism.
// ---------------------------------------------------------------------------

TEST(listview_scrolled_render_differs_from_top) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    list->AddColumn("Name", 0);
    LvFill(list, 50);

    list->SetSelectedIndex(0);
    list->RedrawToCache();
    size_t bytes = (size_t)list->w * list->h * sizeof(uint32_t);
    uint32_t* top = new uint32_t[bytes / sizeof(uint32_t)];
    std::memcpy(top, list->cache, bytes);

    list->SetSelectedIndex(40);
    list->RedrawToCache();
    CHECK(list->GetScrollOffset() > 0);
    CHECK(std::memcmp(top, list->cache, bytes) != 0);

    // Re-rendering the same scrolled view is deterministic.
    uint32_t* scrolled = new uint32_t[bytes / sizeof(uint32_t)];
    std::memcpy(scrolled, list->cache, bytes);
    list->MarkDirty();
    list->RedrawToCache();
    CHECK_EQ(std::memcmp(scrolled, list->cache, bytes), 0);

    delete[] top;
    delete[] scrolled;
}

TEST(listview_extreme_heights_render_is_safe) {
    CompositeWidget root(nullptr, 0, 0, 200, 130);
    ListView* list = new ListView(&root, 0, 0, 200, 130);
    root.AddChild(list);
    LvFill(list, 5);
    list->SetItemHeight(10);
    list->RedrawToCache();
    list->SetItemHeight(106);
    list->RedrawToCache();
    CHECK(list->cache != nullptr);
}

// ---------------------------------------------------------------------------
// ScrollBar edge cases.
// ---------------------------------------------------------------------------

TEST(scrollbar_inverted_range_is_safe) {
    CompositeWidget root(nullptr, 0, 0, 20, 120);
    ScrollBar* bar = new ScrollBar(&root, 0, 0, 20, 120, true);
    root.AddChild(bar);
    bar->SetRange(50, 10, 5);  // max < min.
    CHECK_EQ(bar->GetValue(), 50);
    bar->RedrawToCache();
    CHECK(bar->cache != nullptr);
}

TEST(scrollbar_page_larger_than_span) {
    CompositeWidget root(nullptr, 0, 0, 20, 120);
    ScrollBar* bar = new ScrollBar(&root, 0, 0, 20, 120, true);
    root.AddChild(bar);
    bar->SetRange(0, 100, 500);
    CHECK_EQ(bar->GetValue(), 0);
    bar->SetValue(1000);
    CHECK_EQ(bar->GetValue(), 0);  // No scrollable room.
}

TEST(scrollbar_setvalue_clamps) {
    CompositeWidget root(nullptr, 0, 0, 20, 120);
    ScrollBar* bar = new ScrollBar(&root, 0, 0, 20, 120, true);
    root.AddChild(bar);
    bar->SetRange(0, 100, 10);
    bar->SetValue(-1000);
    CHECK_EQ(bar->GetValue(), 0);
    bar->SetValue(1000);
    CHECK_EQ(bar->GetValue(), 90);  // max - page.
    bar->SetValue(45);
    CHECK_EQ(bar->GetValue(), 45);
}

TEST(scrollbar_wheel_clamps) {
    CompositeWidget root(nullptr, 0, 0, 20, 120);
    ScrollBar* bar = new ScrollBar(&root, 0, 0, 20, 120, true);
    root.AddChild(bar);
    bar->SetRange(0, 100, 10);
    bar->SetValue(50);
    bar->OnMouseWheel(20);
    CHECK_EQ(bar->GetValue(), 30);
    bar->OnMouseWheel(-1000);
    CHECK_EQ(bar->GetValue(), 90);
    bar->OnMouseWheel(1000);
    CHECK_EQ(bar->GetValue(), 0);
}

TEST(scrollbar_wheel_ignored_when_hidden) {
    CompositeWidget root(nullptr, 0, 0, 20, 120);
    ScrollBar* bar = new ScrollBar(&root, 0, 0, 20, 120, true);
    root.AddChild(bar);
    bar->SetRange(0, 100, 10);
    bar->SetValue(50);
    bar->isVisible = false;
    bar->OnMouseWheel(20);
    CHECK_EQ(bar->GetValue(), 50);
}

TEST(scrollbar_non_primary_button_ignored) {
    CompositeWidget root(nullptr, 0, 0, 20, 120);
    ScrollBar* bar = new ScrollBar(&root, 0, 0, 20, 120, true);
    root.AddChild(bar);
    bar->SetRange(0, 100, 10);
    bar->SetValue(50);
    bar->OnMouseDown(10, 60, 2);
    CHECK_EQ(bar->GetValue(), 50);
}

TEST(scrollbar_thumb_reaches_end_of_track) {
    CompositeWidget root(nullptr, 0, 0, 20, 120);
    ScrollBar* bar = new ScrollBar(&root, 0, 0, 20, 120, true);
    root.AddChild(bar);
    bar->SetRange(0, 100, 25);  // scrollable range = 75, not 100.

    const int track = 120;
    const int thumb = bar->ThumbLength();
    const int travel = track - thumb;
    CHECK(thumb > 0);
    CHECK_EQ(bar->ThumbPos(), 0);  // At min the thumb sits at the top.

    bar->SetValue(75);  // Max value (max - page).
    CHECK_EQ(bar->GetValue(), 75);
    // A full sweep of the value range must move the thumb the full travel.
    CHECK_EQ(bar->ThumbPos(), travel);
}

TEST(scrollbar_thumb_position_is_proportional) {
    CompositeWidget root(nullptr, 0, 0, 20, 120);
    ScrollBar* bar = new ScrollBar(&root, 0, 0, 20, 120, true);
    root.AddChild(bar);
    bar->SetRange(0, 100, 25);

    const int travel = 120 - bar->ThumbLength();
    bar->SetValue(75 / 2);  // Halfway through the value range.
    // Thumb should be at (about) the middle of its travel, not 3/8 of the track.
    int expected = (75 / 2) * travel / 75;
    CHECK_EQ(bar->ThumbPos(), expected);
    CHECK(bar->ThumbPos() > travel / 3);
    CHECK(bar->ThumbPos() < travel);
}

TEST(scrollbar_thumb_drag_tracks_cursor) {
    CompositeWidget root(nullptr, 0, 0, 20, 120);
    ScrollBar* bar = new ScrollBar(&root, 0, 0, 20, 120, true);
    root.AddChild(bar);
    bar->SetRange(0, 100, 25);

    const int travel = 120 - bar->ThumbLength();
    bar->OnMouseDown(10, 0, 1);  // Grab the thumb at the top.
    bar->OnMouseMove(10, 0, 10, travel / 2);
    // Dragging half the travel scrolls half the value range (75/2 -> 37).
    CHECK_EQ(bar->GetValue(), (travel / 2) * 75 / travel);
    CHECK(bar->GetValue() < 50);  // Old bug yielded 50 here.

    bar->OnMouseUp(10, travel / 2, 1);
    bar->OnMouseDown(10, bar->ThumbPos(), 1);
    bar->OnMouseMove(10, 0, 10, 100000);  // Drag beyond the bottom.
    CHECK_EQ(bar->GetValue(), 75);
}

TEST(scrollbar_track_click_pages_and_clamps) {
    CompositeWidget root(nullptr, 0, 0, 20, 120);
    ScrollBar* bar = new ScrollBar(&root, 0, 0, 20, 120, true);
    root.AddChild(bar);
    bar->SetRange(0, 100, 10);
    bar->SetValue(0);

    // Click far down the track: page forward by one page.
    bar->OnMouseDown(10, 100, 1);
    CHECK_EQ(bar->GetValue(), 10);

    // Click above the thumb: page back to the start.
    bar->OnMouseDown(10, 0, 1);
    CHECK_EQ(bar->GetValue(), 0);
}

TEST(scrollbar_zero_size_render_is_safe) {
    CompositeWidget root(nullptr, 0, 0, 10, 10);
    ScrollBar* bar = new ScrollBar(&root, 0, 0, 0, 0, false);
    root.AddChild(bar);
    CHECK(bar->cache == nullptr);
    bar->SetRange(0, 100, 10);
    bar->SetValue(50);
    bar->RedrawToCache();
    CHECK_EQ(bar->GetValue(), 50);
}

// ---------------------------------------------------------------------------
// ScrollContainer edge cases.
// ---------------------------------------------------------------------------

TEST(scrollcontainer_small_content_does_not_scroll) {
    CompositeWidget root(nullptr, 0, 0, 200, 150);
    ScrollContainer* sc = new ScrollContainer(&root, 0, 0, 200, 150);
    root.AddChild(sc);
    sc->SetContentSize(50, 50);

    sc->OnMouseWheel(1000);
    CHECK_EQ(sc->GetScrollY(), 0);
    sc->OnMouseWheel(-1000);
    CHECK_EQ(sc->GetScrollY(), 0);
    CHECK_EQ(sc->GetScrollX(), 0);
}

TEST(scrollcontainer_wheel_clamps_at_bounds) {
    CompositeWidget root(nullptr, 0, 0, 200, 150);
    ScrollContainer* sc = new ScrollContainer(&root, 0, 0, 200, 150);
    root.AddChild(sc);
    sc->SetContentSize(50, 1000);

    sc->OnMouseWheel(-100000);  // Scroll to the very bottom.
    CHECK_EQ(sc->GetScrollY(), 1000 - (150 - 12));
    sc->OnMouseWheel(100000);  // And back to the top.
    CHECK_EQ(sc->GetScrollY(), 0);
}

TEST(scrollcontainer_zero_size_is_safe) {
    CompositeWidget root(nullptr, 0, 0, 10, 10);
    ScrollContainer* sc = new ScrollContainer(&root, 0, 0, 0, 0);
    root.AddChild(sc);
    sc->SetContentSize(500, 500);
    sc->OnMouseWheel(-100);
    sc->RedrawToCache();
    // It exists and did not fault; content is larger than the (zero) viewport.
    CHECK(sc->GetScrollY() >= 0);
}

TEST(scrollcontainer_bar_drag_moves_content) {
    CompositeWidget root(nullptr, 0, 0, 200, 150);
    ScrollContainer* sc = new ScrollContainer(&root, 0, 0, 200, 150);
    root.AddChild(sc);
    sc->SetContentSize(50, 1000);  // Only vertical scrolling needed.

    ScrollBar* vbar = sc->GetVerticalBar();
    CHECK(vbar != nullptr);
    vbar->SetValue(200);
    CHECK_EQ(sc->GetScrollY(), 200);  // Content tracked the bar.

    // Dragging the bar's thumb drags the content by the same amount.
    vbar->SetValue(0);
    CHECK_EQ(sc->GetScrollY(), 0);
    const int thumb = vbar->ThumbLength();
    const int travel = (150 - 12) - thumb;
    vbar->OnMouseDown(vbar->w / 2, 0, 1);           // Thumb at the top.
    vbar->OnMouseMove(0, 0, vbar->w / 2, travel);   // To the bottom.
    CHECK_EQ(sc->GetScrollY(), 1000 - (150 - 12));  // Full range.
}

TEST(scrollcontainer_horizontal_bar_moves_content_x) {
    CompositeWidget root(nullptr, 0, 0, 200, 150);
    ScrollContainer* sc = new ScrollContainer(&root, 0, 0, 200, 150);
    root.AddChild(sc);
    sc->SetContentSize(1000, 50);  // Only horizontal scrolling needed.

    ScrollBar* hbar = sc->GetHorizontalBar();
    CHECK(hbar != nullptr);
    hbar->SetValue(300);
    CHECK_EQ(sc->GetScrollX(), 300);
    hbar->SetValue(0);
    CHECK_EQ(sc->GetScrollX(), 0);
}
