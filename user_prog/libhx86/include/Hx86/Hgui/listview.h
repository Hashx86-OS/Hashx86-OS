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

/** struct ListViewItemData - One entry in a list view.
 *
 * @name: Entry name, up to 63 characters plus NUL.
 * @size: Size metadata for the entry.
 * @type: Entry type; 0 = file, 1 = directory, 2 = executable.
 */
struct ListViewItemData {
    char name[64];
    uint32_t size;
    uint8_t type;  // 0 = file, 1 = directory, 2 = executable.
};

/** class HListView - Scrollable list widget for displaying file-like entries. */
class HListView : public Widget {
public:
    HListView(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h);
    ~HListView();

    void SetItems(ListViewItemData* items, int count);
    void Clear();
    int GetSelectedIndex();
    void SetHeader(const char* text);
};

#endif  // HLISTVIEW_H
