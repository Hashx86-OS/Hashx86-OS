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

#ifndef COMBOBOX_H
#define COMBOBOX_H

#include <gui/widget.h>

/**
 * class ComboBox - A drop-down list of selectable items.
 *
 * The closed box shows the selected item. Clicking it opens the list below;
 * clicking an entry selects it, emits ON_CLICK with the item index and closes.
 * Opening grows the widget's bounds to include the list, so a layout may need
 * to be relaxed while it is open.
 */
class ComboBox : public Widget {
private:
    static const int MAX_ITEMS = 16;
    static const int ITEM_HEIGHT = 22;
    char items[MAX_ITEMS][48];
    int count;
    int selected;
    bool open;
    bool pressed = false;
    int32_t closedHeight;

    void Select(int index);

public:
    /**
     * ComboBox() - Construct an empty combo box.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Widget width in pixels.
     * @h: Closed height in pixels.
     */
    ComboBox(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h);

    /** AddItem() - Append an item; the first becomes selected. */
    bool AddItem(const char* label);

    /** Clear() - Remove every item and reset the selection. */
    void Clear();

    /** GetItemCount() - Return the number of items. */
    int GetItemCount() const {
        return count;
    }

    /** GetSelected() - Return the selected index, or -1 when empty. */
    int GetSelected() const {
        return selected;
    }

    /** SetSelected() - Select an index without emitting an event. */
    void SetSelected(int index);

    /** GetItem() - Return the label at @index, or NULL. */
    const char* GetItem(int index) const;

    /** IsOpen() - Report whether the list is dropped down. */
    bool IsOpen() const {
        return open;
    }

    /** SetOpen() - Open or close the list without selecting. */
    void SetOpen(bool value);

    void RedrawToCache() override;
    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    bool IsPressed() const override;

    /** OnFocusLost() - Close the drop-down when focus moves elsewhere. */
    void OnFocusLost() override;
};

#endif  // COMBOBOX_H
