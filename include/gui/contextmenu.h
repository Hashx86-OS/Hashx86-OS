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

#ifndef CONTEXTMENU_H
#define CONTEXTMENU_H

#include <gui/widget.h>

/**
 * class ContextMenu - A popup list of selectable items.
 *
 * Add items, then ShowAt() to place it at a point. Clicking an item emits
 * ON_CLICK with param1 set to the item index and hides the menu. The menu is
 * non-focussable and starts hidden.
 */
class ContextMenu : public Widget {
private:
    static const int MAX_ITEMS = 16;
    static const int ITEM_HEIGHT = 24;
    char items[MAX_ITEMS][48];
    int count;

public:
    /** ContextMenu() - Construct an empty hidden menu. */
    explicit ContextMenu(Widget* parent);

    /** AddItem() - Append a selectable item. */
    bool AddItem(const char* label);

    /** ShowAt() - Position and show the menu at (@x, @y) in parent coordinates. */
    void ShowAt(int32_t x, int32_t y);

    /** Hide() - Hide the menu. */
    void Hide();

    /** GetItemCount() - Return the number of items. */
    int GetItemCount() const {
        return count;
    }

    void RedrawToCache() override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
};

#endif  // CONTEXTMENU_H
