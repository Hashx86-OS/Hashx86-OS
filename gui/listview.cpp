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

#define KDBG_COMPONENT "GUI:LISTVIEW"
#include <gui/listview.h>
#include <gui/mathutil.h>
#include <gui/widgetHost.h>

ListView::ListView(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h)
    : Widget(parent, x, y, w, h),
      columnCount(0),
      itemCount(0),
      scrollOffset(0),
      selectedIndex(-1),
      hoveredIndex(-1) {
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;
    if (this->font) this->font->setSize(TINY);

    // The cache is allocated by the Widget constructor; do not reallocate here.
}

ListView::~ListView() {
    if (font) delete font;
}

void ListView::Clear() {
    itemCount = 0;
    scrollOffset = 0;
    selectedIndex = -1;
    hoveredIndex = -1;
    for (int i = 0; i < LISTVIEW_MAX_ITEMS; i++) {
        items[i].valid = false;
    }
    MarkDirty();
}

bool ListView::AddColumn(const char* title, int32_t width) {
    if (columnCount >= LISTVIEW_MAX_COLUMNS) return false;
    ListViewColumn& col = columns[columnCount];
    const char* safeTitle = title ? title : "";
    int i = 0;
    while (safeTitle[i] && i < LISTVIEW_TITLE_LEN - 1) {
        col.title[i] = safeTitle[i];
        i++;
    }
    col.title[i] = 0;
    col.width = width;
    columnCount++;
    MarkDirty();
    return true;
}

void ListView::AddItem(const char* const* cells, int cellCount, uint8_t type) {
    if (itemCount >= LISTVIEW_MAX_ITEMS) return;
    ListViewItem& item = items[itemCount];

    for (int c = 0; c < LISTVIEW_MAX_COLUMNS; c++) {
        item.cells[c][0] = 0;
    }
    int count = cellCount < LISTVIEW_MAX_COLUMNS ? cellCount : LISTVIEW_MAX_COLUMNS;
    for (int c = 0; c < count; c++) {
        const char* src = (cells && cells[c]) ? cells[c] : "";
        int j = 0;
        while (src[j] && j < LISTVIEW_CELL_LEN - 1) {
            item.cells[c][j] = src[j];
            j++;
        }
        item.cells[c][j] = 0;
    }

    item.type = type;
    item.valid = true;
    itemCount++;
    MarkDirty();
}

void ListView::AddItemData(const ListViewItemData& data) {
    const char* cellPtrs[GUI_LISTVIEW_MAX_COLUMNS];
    for (int c = 0; c < GUI_LISTVIEW_MAX_COLUMNS; c++) {
        cellPtrs[c] = data.cells[c];
    }
    AddItem(cellPtrs, GUI_LISTVIEW_MAX_COLUMNS, data.type);
}

const ListViewItem* ListView::GetItem(int index) const {
    if (index < 0 || index >= itemCount) return nullptr;
    return &items[index];
}

void ListView::SetItemHeight(int h) {
    if (h < 10) h = 10;
    int contentH = this->h - LISTVIEW_HEADER_HEIGHT - 2;
    if (contentH > 0 && h > contentH) h = contentH;
    itemHeight = h;
    ClampScrollOffset();
    MarkDirty();
}

void ListView::OnKeyDown(const char* key) {
    if (!enabled || !isVisible || !key) return;
    if (key[0] == 0) return;

    if (key[0] == '\r' || key[0] == '\n' || key[0] == ' ') {
        // Enter or Space fire a click event. PID 0 means no owning process.
        if (selectedIndex >= 0 && g_widgetHost && PID != 0) {
            g_widgetHost->PostEvent(this->PID, this->ID, ON_CLICK);
            g_widgetHost->WakeProcess(this->PID);
        }
    }
}

void ListView::OnSpecialKeyDown(uint8_t key) {
    if (!enabled || !isVisible || itemCount <= 0) return;

    const int visible = GetVisibleItemCount();
    const int last = itemCount - 1;
    bool handled = true;

    switch (key) {
        case 0x48:  // Up.
            if (selectedIndex < 0) {
                selectedIndex = last;
            } else if (selectedIndex > 0) {
                selectedIndex--;
            }
            break;
        case 0x50:  // Down.
            if (selectedIndex < 0) {
                selectedIndex = 0;
            } else if (selectedIndex < last) {
                selectedIndex++;
            }
            break;
        case 0x47:  // Home.
            selectedIndex = 0;
            break;
        case 0x4F:  // End.
            selectedIndex = last;
            break;
        case 0x49:  // Page up.
            selectedIndex = (selectedIndex < 0) ? 0 : selectedIndex - visible;
            if (selectedIndex < 0) selectedIndex = 0;
            break;
        case 0x51:  // Page down.
            if (selectedIndex < 0) {
                selectedIndex = 0;
            } else {
                selectedIndex += visible;
                if (selectedIndex > last) selectedIndex = last;
            }
            break;
        default:
            handled = false;
            break;
    }

    if (!handled) return;
    EnsureSelectionVisible();
    MarkDirty();
}

void ListView::update() {
    MarkDirty();
}

/**
 * ListView::SetSelectedIndex() - Highlight a row programmatically.
 * @index: Row to select, or -1 to clear the selection.
 *
 * The scroll offset is corrected so the row is actually visible, which matters
 * when preselecting something below the fold.
 *
 * Return: True when @index was in range, false otherwise.
 */
bool ListView::SetSelectedIndex(int index) {
    if (index < -1 || index >= itemCount) return false;

    selectedIndex = index;

    if (index >= 0) {
        EnsureSelectionVisible();
    }

    MarkDirty();
    return true;
}

/**
 * ListView::ContentWidth() - Return the pixel width available to columns.
 *
 * The scrollbar gutter is only subtracted when the list actually needs one, so
 * a short list uses the full width.
 */
int ListView::ContentWidth() const {
    return w - 2 - (NeedScrollbar() ? LISTVIEW_SCROLLBAR_WIDTH : 0);
}

/**
 * ListView::ColumnGeometry() - Compute each column's x position and width.
 * @x: Receives the left edge of each column.
 * @cw: Receives the width of each column.
 *
 * Fixed-width columns keep their width; zero-width columns split whatever is
 * left. With no columns at all the row is a single implicit column so a
 * column-less list still renders sensibly.
 */
void ListView::ColumnGeometry(int32_t* x, int32_t* cw) const {
    int contentW = ContentWidth();

    if (columnCount <= 0) {
        x[0] = 4;
        int single = contentW - 8;
        cw[0] = single > 0 ? single : 0;
        return;
    }

    int fixed = 0;
    int autos = 0;
    for (int c = 0; c < columnCount; c++) {
        if (columns[c].width > 0) {
            fixed += columns[c].width;
        } else {
            autos++;
        }
    }

    int pad = 4 * (columnCount + 1);
    int autoW = autos > 0 ? (contentW - fixed - pad) / autos : 0;
    if (autoW < 24) autoW = 24;

    int cur = 4;
    for (int c = 0; c < columnCount; c++) {
        int wc = columns[c].width > 0 ? columns[c].width : autoW;
        x[c] = cur;
        cw[c] = wc;
        cur += wc + 4;
    }
}

bool ListView::NeedScrollbar() const {
    int contentH = h - LISTVIEW_HEADER_HEIGHT - 2;
    if (contentH <= 0 || itemHeight <= 0) return false;
    // A list too narrow to hold the row text plus the gutter gets no
    // scrollbar; otherwise the whole widget would be treated as thumb.
    if (w <= LISTVIEW_SCROLLBAR_WIDTH + 2) return false;
    return itemCount > contentH / itemHeight;
}

int ListView::HitRow(int32_t localY) const {
    int visible = GetVisibleItemCount();
    if (visible <= 0 || itemCount <= 0) return -1;

    const int32_t top = LISTVIEW_HEADER_HEIGHT + 1;
    const int64_t rowsHeight = (int64_t)visible * itemHeight;
    if (localY < top || (int64_t)localY >= (int64_t)top + rowsHeight) return -1;

    int row = scrollOffset + (int)((localY - top) / itemHeight);
    if (row < 0 || row >= itemCount) return -1;
    return row;
}

int ListView::ScrollbarThumbHeight(int visibleItems, int sbH) const {
    if (visibleItems <= 0 || sbH <= 0 || itemCount <= 0) return 0;
    uint32_t thumbH = GuiMulDivU32((uint64_t)visibleItems * (uint64_t)sbH, (uint32_t)itemCount);
    if (thumbH < 10) thumbH = 10;
    if (thumbH > (uint32_t)sbH) thumbH = (uint32_t)sbH;
    return (int)thumbH;
}

int ListView::ScrollbarThumbY(int visibleItems, int sbH, int thumbH) const {
    const int top = LISTVIEW_HEADER_HEIGHT + 1;
    if (visibleItems <= 0 || itemCount <= visibleItems) return top;
    int travel = sbH - thumbH;
    if (travel <= 0) return top;
    uint32_t offset = GuiMulDivU32((uint64_t)scrollOffset * (uint64_t)travel,
                                   (uint32_t)(itemCount - visibleItems));
    return top + (int)offset;
}

void ListView::ClampScrollOffset() {
    int maxOff = itemCount - GetVisibleItemCount();
    if (maxOff < 0) maxOff = 0;
    if (scrollOffset < 0) scrollOffset = 0;
    if (scrollOffset > maxOff) scrollOffset = maxOff;
}

void ListView::EnsureSelectionVisible() {
    int visible = GetVisibleItemCount();
    if (selectedIndex >= 0) {
        if (selectedIndex < scrollOffset) {
            scrollOffset = selectedIndex;
        } else if (visible > 0 && selectedIndex >= scrollOffset + visible) {
            scrollOffset = selectedIndex - visible + 1;
        }
    }
    ClampScrollOffset();
}

void ListView::RedrawToCache() {
    if (!cache || !NINA::activeInstance) {
        isDirty = false;
        return;
    }
    memset(cache, 0, (size_t)w * (size_t)h * sizeof(uint32_t));

    // Background
    NINA::activeInstance->FillRoundedRectangle(cache, w, h, 0, 0, w, h, 4, LISTVIEW_BG);

    // Border
    NINA::activeInstance->DrawRoundedRectangle(cache, w, h, 0, 0, w, h, 4, LISTVIEW_BORDER);

    // Column geometry (shared by the header and the rows).
    int32_t colX[LISTVIEW_MAX_COLUMNS];
    int32_t colW[LISTVIEW_MAX_COLUMNS];
    ColumnGeometry(colX, colW);

    // Header bar: one title per appended column.
    NINA::activeInstance->FillRectangle(cache, w, h, 1, 1, w - 2, LISTVIEW_HEADER_HEIGHT,
                                        LISTVIEW_HEADER_BG);
    for (int c = 0; c < columnCount; c++) {
        int tx = colX[c] + (c == 0 ? 20 : 4);
        NINA::activeInstance->DrawString(cache, w, h, tx, 4, columns[c].title, font,
                                         LISTVIEW_HEADER_TEXT);
    }

    // Separator under the header.
    NINA::activeInstance->DrawHorizontalLine(cache, w, h, 1, LISTVIEW_HEADER_HEIGHT, w - 2,
                                             LISTVIEW_BORDER);

    // Calculate the visible range.
    int visibleItems = GetVisibleItemCount();
    int startItem = scrollOffset;
    int endItem = startItem + visibleItems;
    if (endItem > itemCount) endItem = itemCount;

    // Draw the visible items.
    for (int i = startItem; i < endItem; i++) {
        if (!items[i].valid) continue;

        int itemY = LISTVIEW_HEADER_HEIGHT + 1 + (i - startItem) * itemHeight;

        // Background
        uint32_t bgColor;
        if (i == selectedIndex) {
            bgColor = LISTVIEW_ITEM_BG_SELECTED;
        } else if (i == hoveredIndex) {
            bgColor = LISTVIEW_ITEM_BG_HOVER;
        } else {
            bgColor = (i % 2 == 0) ? LISTVIEW_ITEM_BG_EVEN : LISTVIEW_ITEM_BG_ODD;
        }
        NINA::activeInstance->FillRectangle(cache, w, h, 1, itemY, w - 2, itemHeight, bgColor);

        // Icon indicator (small colored circle) beside the first column.
        uint32_t iconColor;
        switch (items[i].type) {
            case 1:
                iconColor = LISTVIEW_ICON_DIR;
                break;
            case 2:
                iconColor = LISTVIEW_ICON_EXE;
                break;
            default:
                iconColor = LISTVIEW_ICON_FILE;
                break;
        }
        NINA::activeInstance->FillCircle(cache, w, h, 12, itemY + itemHeight / 2, 4, iconColor);

        // One text cell per column.
        if (columnCount > 0) {
            for (int c = 0; c < columnCount; c++) {
                int tx = colX[c] + (c == 0 ? 20 : 4);
                uint32_t textColor = (c == 0) ? LISTVIEW_ITEM_TEXT : 0xFF6C7086;
                NINA::activeInstance->DrawString(cache, w, h, tx, itemY + 2, items[i].cells[c],
                                                 font, textColor);
            }
        } else {
            // No columns appended yet: fall back to the first cell full width.
            NINA::activeInstance->DrawString(cache, w, h, 22, itemY + 2, items[i].cells[0], font,
                                             LISTVIEW_ITEM_TEXT);
        }
    }

    // Scrollbar, if needed.
    if (NeedScrollbar()) {
        int sbX = w - LISTVIEW_SCROLLBAR_WIDTH - 1;
        int sbY = LISTVIEW_HEADER_HEIGHT + 1;
        int sbH = h - LISTVIEW_HEADER_HEIGHT - 2;
        NINA::activeInstance->FillRectangle(cache, w, h, sbX, sbY, LISTVIEW_SCROLLBAR_WIDTH, sbH,
                                            LISTVIEW_SCROLLBAR_BG);

        // Thumb. The geometry helpers are shared with hit-testing so the drawn
        // thumb and the clickable thumb can never disagree by a few pixels.
        int thumbH = ScrollbarThumbHeight(visibleItems, sbH);
        int thumbY = ScrollbarThumbY(visibleItems, sbH, thumbH);
        NINA::activeInstance->FillRoundedRectangle(cache, w, h, sbX, thumbY,
                                                   LISTVIEW_SCROLLBAR_WIDTH, thumbH, 3,
                                                   LISTVIEW_SCROLLBAR_THUMB);
    }

    // Empty state.
    if (itemCount == 0 && FontManager::activeInstance) {
        Font* msgFont = FontManager::activeInstance->getNewFont();
        if (msgFont) {
            msgFont->setSize(SMALL);
            NINA::activeInstance->DrawString(cache, w, h, w / 2 - 40, h / 2 - 8, "No items",
                                             msgFont, 0xFF6C7086);
            delete msgFont;
        }
    }

    isDirty = false;
}

void ListView::OnMouseDown(int32_t mx, int32_t my, uint8_t button) {
    if (!enabled || !isVisible) return;

    Widget::OnMouseDown(mx, my, button);
    if (button != 1) return;

    // Coordinates arrive in the parent's frame; ListView-local geometry starts
    // at (this->x, this->y).
    int localX = mx - this->x;
    int localY = my - this->y;

    // Scrollbar column: grab the thumb or page toward the click.
    if (NeedScrollbar() && localX >= w - LISTVIEW_SCROLLBAR_WIDTH - 1 && localX < w &&
        localY > LISTVIEW_HEADER_HEIGHT && localY < h) {
        int visibleItems = GetVisibleItemCount();
        int sbH = h - LISTVIEW_HEADER_HEIGHT - 2;
        int thumbH = ScrollbarThumbHeight(visibleItems, sbH);
        int thumbY = ScrollbarThumbY(visibleItems, sbH, thumbH);

        if (localY >= thumbY && localY < thumbY + thumbH) {
            isDraggingThumb = true;
            dragStartY = localY;
            dragStartOffset = scrollOffset;
            SetMouseCaptured(true);
        } else {
            int64_t newOff =
                (int64_t)scrollOffset + ((localY < thumbY) ? -visibleItems : visibleItems);
            int maxOff = itemCount - visibleItems;
            if (maxOff < 0) maxOff = 0;
            if (newOff < 0) newOff = 0;
            if (newOff > maxOff) newOff = maxOff;
            scrollOffset = (int)newOff;
            MarkDirty();
        }
        return;
    }

    // Row selection. The right edge is only reserved for a scrollbar that is
    // actually shown; otherwise a short list uses the full width.
    int rowRight = NeedScrollbar() ? (w - LISTVIEW_SCROLLBAR_WIDTH - 1) : (w - 1);
    int clickedItem = HitRow(localY);
    if (clickedItem >= 0 && localX >= 0 && localX < rowRight) {
        selectedIndex = clickedItem;
        MarkDirty();

        if (g_widgetHost && PID != 0) {
            g_widgetHost->PostEvent(this->PID, this->ID, ON_CLICK);
            g_widgetHost->WakeProcess(this->PID);
        }
    }
}

void ListView::OnMouseWheel(int32_t delta) {
    if (!enabled || !isVisible) return;
    int visibleItems = GetVisibleItemCount();
    if (visibleItems <= 0) return;

    int maxOff = itemCount - visibleItems;
    if (maxOff < 0) maxOff = 0;

    // Compute in 64-bit so a hostile wheel delta cannot overflow the multiply.
    int64_t next = (int64_t)scrollOffset - (int64_t)delta * 3;
    if (next < 0) next = 0;
    if (next > maxOff) next = maxOff;
    if (next != scrollOffset) {
        scrollOffset = (int)next;
        MarkDirty();
    }
}

void ListView::OnMouseUp(int32_t, int32_t, uint8_t) {
    isDraggingThumb = false;
    SetMouseCaptured(false);
}

void ListView::OnMouseMove(int32_t, int32_t oldy, int32_t mx, int32_t my) {
    (void)oldy;
    if (!enabled || !isVisible) return;

    int localX = mx - this->x;
    int localY = my - this->y;

    // A thumb drag follows the pointer even after it leaves the list; the
    // capture set on press keeps this callback coming.
    if (isDraggingThumb) {
        int visibleItems = GetVisibleItemCount();
        if (visibleItems <= 0 || itemCount <= visibleItems) return;
        int sbH = h - LISTVIEW_HEADER_HEIGHT - 2;
        int thumbH = ScrollbarThumbHeight(visibleItems, sbH);
        int travel = sbH - thumbH;
        if (travel <= 0) return;

        int64_t delta = (int64_t)localY - dragStartY;
        if (delta > travel) delta = travel;
        if (delta < -travel) delta = -travel;
        int32_t range = itemCount - visibleItems;

        int32_t deltaOff = GuiMulDivSigned((int32_t)delta, range, travel);
        int64_t newOff = (int64_t)dragStartOffset + deltaOff;
        if (newOff < 0) newOff = 0;
        if (newOff > range) newOff = range;
        if (newOff != scrollOffset) {
            scrollOffset = (int)newOff;
            MarkDirty();
        }
        return;
    }

    if (!isFocused) return;

    int rowRight = NeedScrollbar() ? (w - LISTVIEW_SCROLLBAR_WIDTH - 1) : (w - 1);
    int hovered = HitRow(localY);
    if (hovered >= 0 && localX >= 0 && localX < rowRight) {
        if (hovered != hoveredIndex) {
            hoveredIndex = hovered;
            MarkDirty();
        }
    } else if (hoveredIndex >= 0) {
        hoveredIndex = -1;
        MarkDirty();
    }
}
