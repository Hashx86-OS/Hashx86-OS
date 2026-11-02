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

#define KDBG_COMPONENT "GUI:TABLE"
#include <gui/config/config.h>
#include <gui/event.h>
#include <gui/table.h>
#include <gui/widgetHost.h>

Table::Table(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, int columns)
    : Widget(parent, x, y, w, h), columns(columns), rowCount(0), selected(-1) {
    if (this->columns < 1) this->columns = 1;
    if (this->columns > MAX_COLUMNS) this->columns = MAX_COLUMNS;
    for (int c = 0; c < MAX_COLUMNS; c++) header[c][0] = '\0';
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;
}

void Table::SetHeader(int col, const char* text) {
    if (col < 0 || col >= columns || !text) return;
    size_t len = strlen(text);
    if (len >= sizeof(header[0])) len = sizeof(header[0]) - 1;
    memcpy(header[col], text, len);
    header[col][len] = '\0';
    MarkDirty();
}

bool Table::AddRow(const char* const* rowCells) {
    if (rowCount >= MAX_ROWS || !rowCells) return false;
    for (int c = 0; c < columns; c++) {
        const char* src = rowCells[c] ? rowCells[c] : "";
        size_t len = strlen(src);
        if (len >= sizeof(cells[0][0])) len = sizeof(cells[0][0]) - 1;
        memcpy(cells[rowCount][c], src, len);
        cells[rowCount][c][len] = '\0';
    }
    rowCount++;
    MarkDirty();
    return true;
}

void Table::Clear() {
    rowCount = 0;
    selected = -1;
    MarkDirty();
}

void Table::SetSelected(int row) {
    if (row < -1 || row >= rowCount) return;
    selected = row;
    MarkDirty();
}

void Table::OnMouseDown(int32_t x, int32_t y, uint8_t button) {
    if (!enabled || !isVisible || button != 1) return;
    if (!ContainsCoordinate(x, y)) return;
    Widget::OnMouseDown(x, y, button);
    pressed = true;
}

void Table::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    if (button != 1) return;
    bool wasPressed = pressed;
    pressed = false;
    if (!enabled || !isVisible || !wasPressed) return;
    if (!ContainsCoordinate(x, y)) return;

    int32_t localY = y - this->y;
    if (localY < HEADER_HEIGHT) return;
    int row = (localY - HEADER_HEIGHT) / ROW_HEIGHT;
    if (row >= 0 && row < rowCount) {
        selected = row;
        MarkDirty();
        if (g_widgetHost && PID != 0) {
            g_widgetHost->PostEvent(PID, ID, ON_CLICK, (uint32_t)row);
            g_widgetHost->WakeProcess(PID);
        }
    }
}

bool Table::IsPressed() const {
    return pressed;
}

void Table::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }
    memset(cache, 0, sizeof(uint32_t) * w * h);
    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }

    int32_t colW = (columns > 0) ? w / columns : w;

    NINA::activeInstance->FillRectangle(cache, w, h, 0, 0, w, HEADER_HEIGHT, LISTVIEW_HEADER_BG);
    for (int c = 0; c < columns; c++) {
        if (font) {
            int32_t th = (int32_t)font->getLineHeight();
            NINA::activeInstance->DrawString(cache, w, h, c * colW + 4, (HEADER_HEIGHT - th) / 2,
                                             header[c], font, LISTVIEW_HEADER_TEXT);
        }
    }

    for (int r = 0; r < rowCount; r++) {
        int32_t ry = HEADER_HEIGHT + r * ROW_HEIGHT;
        uint32_t bg = (r == selected) ? LISTVIEW_ITEM_BG_SELECTED
                                      : ((r % 2) ? LISTVIEW_ITEM_BG_ODD : LISTVIEW_ITEM_BG_EVEN);
        NINA::activeInstance->FillRectangle(cache, w, h, 0, ry, w, ROW_HEIGHT, bg);
        for (int c = 0; c < columns; c++) {
            if (font) {
                int32_t th = (int32_t)font->getLineHeight();
                NINA::activeInstance->DrawString(cache, w, h, c * colW + 4,
                                                 ry + (ROW_HEIGHT - th) / 2, cells[r][c], font,
                                                 LISTVIEW_ITEM_TEXT);
            }
        }
    }
    isDirty = false;
}
