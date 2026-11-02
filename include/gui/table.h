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

#ifndef TABLE_H
#define TABLE_H

#include <gui/widget.h>

/**
 * class Table - A simple grid of text cells with a header row.
 *
 * Cells are copied on insertion. Clicking a data row selects it and emits
 * ON_CLICK with the row index. Column widths are even.
 */
class Table : public Widget {
private:
    static const int MAX_COLUMNS = 8;
    static const int MAX_ROWS = 64;
    static const int HEADER_HEIGHT = 24;
    static const int ROW_HEIGHT = 20;
    int columns;
    int rowCount;
    int selected;
    bool pressed = false;
    char header[MAX_COLUMNS][32];
    char cells[MAX_ROWS][MAX_COLUMNS][32];

public:
    /**
     * Table() - Construct an empty table.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Widget width in pixels.
     * @h: Widget height in pixels.
     * @columns: Number of columns (clamped to 1..8).
     */
    Table(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, int columns);

    /** SetHeader() - Set the header text for a column. */
    void SetHeader(int col, const char* text);

    /** AddRow() - Append a row from an array of @columns cell strings. */
    bool AddRow(const char* const* cells);

    /** Clear() - Remove every row and the selection. */
    void Clear();

    /** GetRowCount() - Return the number of rows. */
    int GetRowCount() const {
        return rowCount;
    }

    /** GetSelected() - Return the selected row index, or -1. */
    int GetSelected() const {
        return selected;
    }

    /** SetSelected() - Select a row without emitting an event. */
    void SetSelected(int row);

    void RedrawToCache() override;
    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    bool IsPressed() const override;
};

#endif  // TABLE_H
