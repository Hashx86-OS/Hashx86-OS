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

#include <gui/config/config.h>
#include <gui/widget.h>
#include <types.h>

#define LISTVIEW_MAX_ITEMS 64
#define LISTVIEW_ITEM_HEIGHT 18
#define LISTVIEW_HEADER_HEIGHT 22
#define LISTVIEW_SCROLLBAR_WIDTH 8

/**
 * struct ListViewItem - One row in a ListView.
 * @name: Display name of the item.
 * @size: Byte size associated with the item.
 * @type: File type code: 0 = file, 1 = directory, 2 = executable.
 * @valid: Whether the slot holds a live item.
 */
struct ListViewItem {
    char name[64];
    uint32_t size;
    uint8_t type;
    bool valid;
};

/**
 * class ListView - A scrollable single-column list with header and scrollbar.
 *
 * Renders items in a fixed-height rows with keyboard and mouse navigation,
 * a clickable scrollbar thumb, and row selection for the owning process.
 */
class ListView : public Widget {
private:
    ListViewItem items[LISTVIEW_MAX_ITEMS];
    int itemCount;
    int scrollOffset;
    int selectedIndex;
    int hoveredIndex;
    char headerText[32];
    int itemHeight = 18;
    bool isDraggingThumb = false;
    int dragStartY = 0;
    int dragStartOffset = 0;

public:
    /**
     * ListView() - Construct an empty list view.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: List width in pixels.
     * @h: List height in pixels.
     */
    ListView(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h);

    /**
     * ~ListView() - Destroy the list view.
     */
    ~ListView();

    /** Clear() - Empty the item list and reset the scroll position. */
    void Clear();

    /**
     * AddItem() - Append an item to the list.
     * @name: Display name of the item.
     * @size: Byte size associated with the item.
     * @type: File type code.
     */
    void AddItem(const char* name, uint32_t size, uint8_t type);

    /**
     * SetHeader() - Set the column header text.
     * @text: Header text.
     */
    void SetHeader(const char* text);

    /**
     * SetItemHeight() - Set the per-row height in pixels.
     * @h: New row height.
     */
    void SetItemHeight(int h);

    /** GetSelectedIndex() - Return the currently selected row index. */
    int GetSelectedIndex() const {
        return selectedIndex;
    }

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

    /** OnKeyDown() - Move the selection and scroll the list. */
    void OnKeyDown(const char* key) override;

    /** OnSpecialKeyDown() - Handle Enter to confirm the current row. */
    void OnSpecialKeyDown(uint8_t key) override;
};

#endif  // LISTVIEW_H
