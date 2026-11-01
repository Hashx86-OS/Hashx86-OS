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

#ifndef HLISTVIEW_H
#define HLISTVIEW_H

#include <Hx86/Hgui/widget.h>

/** class HListView - Scrollable multi-column list widget.
 *
 * The list starts with no columns; append them with AddColumn() and fill the
 * rows with SetItems(). Nothing about the cell content is assumed.
 */
class HListView : public Widget {
public:
    HListView(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h);
    ~HListView();

    /**
     * AddColumn() - Append a column to the list.
     * @title: Column header text.
     * @width: Fixed width in pixels, or 0 to share leftover width.
     */
    void AddColumn(const char* title, int32_t width = 0);

    void SetItems(ListViewItemData* items, int count);
    void Clear();
    int GetSelectedIndex();

    /**
     * SetSelectedIndex() - Highlight a row programmatically.
     * @index: Row to select, or -1 to clear the selection.
     *
     * Return: True when @index was in range, false otherwise.
     */
    bool SetSelectedIndex(int index);
};

#endif  // HLISTVIEW_H
