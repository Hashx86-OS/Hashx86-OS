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

#ifndef LISTVIEW_H
#define LISTVIEW_H

#include <gui/abi.h>
#include <gui/config/config.h>
#include <gui/widget.h>
#include <types.h>

#define LISTVIEW_MAX_ITEMS 64
#define LISTVIEW_ITEM_HEIGHT 18
#define LISTVIEW_HEADER_HEIGHT 22
#define LISTVIEW_SCROLLBAR_WIDTH 8
#define LISTVIEW_MAX_COLUMNS 4
#define LISTVIEW_CELL_LEN 64
#define LISTVIEW_TITLE_LEN 32

/**
 * struct ListViewColumn - One column definition.
 * @title: Column header text.
 * @width: Fixed width in pixels, or 0 to share the leftover width with the
 *         other auto columns.
 */
struct ListViewColumn {
    char title[LISTVIEW_TITLE_LEN];
    int32_t width;
};

/**
 * struct ListViewItem - One row in a ListView.
 * @cells: Text for each column, indexed by column order.
 * @type: File type code: 0 = file, 1 = directory, 2 = executable. Drives the
 *        row's leading icon only.
 * @valid: Whether the slot holds a live item.
 */
struct ListViewItem {
    char cells[LISTVIEW_MAX_COLUMNS][LISTVIEW_CELL_LEN];
    uint8_t type;
    bool valid;
};

/**
 * class ListView - A scrollable multi-column list.
 *
 * A list begins with no columns; the caller appends them with AddColumn().
 * Each row supplies one text cell per column with AddItem(). Nothing about the
 * content (names, sizes, ...) is assumed by the widget: it only lays out the
 * columns it was given.
 */
class ListView : public Widget {
private:
    ListViewItem items[LISTVIEW_MAX_ITEMS];
    ListViewColumn columns[LISTVIEW_MAX_COLUMNS];
    int columnCount;
    int itemCount;
    int scrollOffset;
    int selectedIndex;
    int hoveredIndex;
    int itemHeight = 18;
    bool isDraggingThumb = false;
    int dragStartY = 0;
    int dragStartOffset = 0;

    int ContentWidth() const;
    void ColumnGeometry(int32_t* x, int32_t* cw) const;

    /**
     * HitRow() - Map a widget-local Y to a visible row index.
     * @localY: Y in this widget's coordinate space.
     *
     * Return: The row index, or -1 for the header, the empty strip below the
     *         last full row, or any out-of-range position.
     */
    int HitRow(int32_t localY) const;

    /** ScrollbarThumbHeight() - Height of the drawn thumb for @visibleItems. */
    int ScrollbarThumbHeight(int visibleItems, int sbH) const;

    /** ScrollbarThumbY() - Top of the drawn thumb in widget-local pixels. */
    int ScrollbarThumbY(int visibleItems, int sbH, int thumbH) const;

    /** ClampScrollOffset() - Keep scrollOffset within [0, maxOffset]. */
    void ClampScrollOffset();

    /** EnsureSelectionVisible() - Scroll so the selected row is on screen. */
    void EnsureSelectionVisible();

public:
    /**
     * ListView() - Construct an empty, column-less list view.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: List width in pixels.
     * @h: List height in pixels.
     */
    ListView(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h);

    /** ~ListView() - Destroy the list view. */
    ~ListView();

    /**
     * AddColumn() - Append a column.
     * @title: Header text.
     * @width: Fixed width in pixels, or 0 to share leftover space.
     *
     * Return: True on success, false when the column limit is reached.
     */
    bool AddColumn(const char* title, int32_t width = 0);

    /** GetColumnCount() - Return the number of columns appended so far. */
    int GetColumnCount() const {
        return columnCount;
    }

    /** ClearItems() - Empty the item list and reset scrolling. */
    void Clear();

    /**
     * AddItem() - Append a row.
     * @cells: Array of cell strings, one per column (missing cells are blank).
     * @cellCount: Number of entries in @cells.
     * @type: Leading icon type (0 file, 1 directory, 2 executable).
     */
    void AddItem(const char* const* cells, int cellCount, uint8_t type = 0);

    /**
     * AddItemData() - Append a row from a syscall payload entry.
     * @data: One ListViewItemData (cell strings plus leading-icon type).
     *
     * Converts the fixed 2D cell buffer into the pointer array AddItem()
     * expects. Centralised here so the kernel handler and tests share exactly
     * one conversion.
     */
    void AddItemData(const ListViewItemData& data);

    /**
     * SetItemHeight() - Set the per-row height in pixels.
     * @h: New row height.
     */
    void SetItemHeight(int h);

    /** GetSelectedIndex() - Return the currently selected row index. */
    int GetSelectedIndex() const {
        return selectedIndex;
    }

    /** GetScrollOffset() - Return the index of the first visible row. */
    int GetScrollOffset() const {
        return scrollOffset;
    }

    /** GetItemHeight() - Return the current per-row height in pixels. */
    int GetItemHeight() const {
        return itemHeight;
    }

    /** GetVisibleItemCount() - Return how many whole rows fit on screen. */
    int GetVisibleItemCount() const {
        int contentH = h - LISTVIEW_HEADER_HEIGHT - 2;
        return (itemHeight > 0 && contentH > 0) ? contentH / itemHeight : 0;
    }

    /**
     * NeedScrollbar() - Whether the row count exceeds the visible window.
     *
     * When false the rows use the full widget width; when true the right
     * gutter is reserved for the scrollbar (and excluded from row clicks).
     */
    bool NeedScrollbar() const;

    /**
     * SetSelectedIndex() - Highlight a row programmatically.
     * @index: Row to select, or -1 to clear the selection.
     *
     * Return: True when @index was in range, false otherwise.
     */
    bool SetSelectedIndex(int index);

    /**
     * GetItem() - Return the item at a row index.
     * @index: Row index.
     *
     * Return: The matching item, or NULL if @index is out of range.
     */
    const ListViewItem* GetItem(int index) const;

    /** GetItemCount() - Return the number of live items. */
    int GetItemCount() const {
        return itemCount;
    }

    /** update() - Repaint the list cache and redraw it. */
    void update();

    /** RedrawToCache() - Paint the header, rows, selection, and scrollbar. */
    void RedrawToCache() override;

    /** IsListView() - Return true for list view widget types. */
    bool IsListView() const override {
        return true;
    }

    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) override;

    /** OnMouseWheel() - Scroll the list a few rows per notch. */
    void OnMouseWheel(int32_t delta) override;

    /** OnKeyDown() - Move the selection and scroll the list. */
    void OnKeyDown(const char* key) override;

    /** OnSpecialKeyDown() - Handle Enter to confirm the current row. */
    void OnSpecialKeyDown(uint8_t key) override;
};

#endif  // LISTVIEW_H
