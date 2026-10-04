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

#define KDBG_COMPONENT "GUI:COMBO"
#include <gui/combobox.h>
#include <gui/event.h>
#include <gui/widgetHost.h>

ComboBox::ComboBox(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h)
    : Widget(parent, x, y, w, h), count(0), selected(-1), open(false), closedHeight(h) {
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;
}

bool ComboBox::AddItem(const char* label) {
    if (count >= MAX_ITEMS || !label) return false;
    size_t len = strlen(label);
    if (len >= sizeof(items[0])) len = sizeof(items[0]) - 1;
    memcpy(items[count], label, len);
    items[count][len] = '\0';
    count++;
    if (selected < 0) selected = 0;
    if (open) Resize(w, closedHeight + count * ITEM_HEIGHT);
    MarkDirty();
    return true;
}

void ComboBox::Clear() {
    count = 0;
    selected = -1;
    SetOpen(false);
    MarkDirty();
}

void ComboBox::Select(int index) {
    if (index < 0 || index >= count) return;
    selected = index;
    SetOpen(false);
    if (g_widgetHost && PID != 0) {
        g_widgetHost->PostEvent(PID, ID, ON_CLICK, (uint32_t)index);
        g_widgetHost->WakeProcess(PID);
    }
    MarkDirty();
}

void ComboBox::SetSelected(int index) {
    if (index < -1 || index >= count) return;
    selected = index;
    MarkDirty();
}

const char* ComboBox::GetItem(int index) const {
    if (index < 0 || index >= count) return nullptr;
    return items[index];
}

void ComboBox::SetOpen(bool value) {
    if (open == value) return;
    open = value;
    Resize(w, value ? closedHeight + count * ITEM_HEIGHT : closedHeight);
    MarkDirty();
}

void ComboBox::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    if (!enabled || !isVisible || button != 1) return;
    if (!ContainsCoordinate(x, y)) return;

    int32_t localY = y - this->y;
    if (!open) {
        SetOpen(true);
        return;
    }
    if (localY >= closedHeight) {
        int index = (localY - closedHeight) / ITEM_HEIGHT;
        Select(index);
    } else {
        SetOpen(false);
    }
}

void ComboBox::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }
    memset(cache, 0, sizeof(uint32_t) * w * h);
    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }

    NINA::activeInstance->FillRoundedRectangle(cache, w, h, 0, 0, w, closedHeight, 3,
                                               BUTTON_BG_NORMAL);
    NINA::activeInstance->DrawRoundedRectangle(cache, w, h, 0, 0, w, closedHeight, 3,
                                               BUTTON_BORDER_NORMAL);

    if (selected >= 0 && selected < count && font) {
        int32_t th = (int32_t)font->getLineHeight();
        NINA::activeInstance->DrawString(cache, w, h, 6, (closedHeight - th) / 2, items[selected],
                                         font, BUTTON_TEXT_NORMAL);
    }

    // Down arrow.
    int32_t ax = w - 14;
    int32_t ay = closedHeight / 2 - 2;
    for (int32_t i = 0; i < 4; i++) {
        NINA::activeInstance->DrawLine(cache, w, h, ax - i, ay + i, ax + i, ay + i,
                                       BUTTON_TEXT_NORMAL);
    }

    if (!open) {
        isDirty = false;
        return;
    }

    NINA::activeInstance->FillRectangle(cache, w, h, 0, closedHeight, w, count * ITEM_HEIGHT,
                                        MENU_BG);
    NINA::activeInstance->DrawRectangle(cache, w, h, 0, closedHeight, w, count * ITEM_HEIGHT,
                                        MENU_BORDER);
    for (int i = 0; i < count; i++) {
        if (i == selected) {
            NINA::activeInstance->FillRectangle(cache, w, h, 1, closedHeight + i * ITEM_HEIGHT + 1,
                                                w - 2, ITEM_HEIGHT - 2, MENU_ITEM_HOVER);
        }
        if (font) {
            int32_t th = (int32_t)font->getLineHeight();
            NINA::activeInstance->DrawString(
                cache, w, h, 8, closedHeight + i * ITEM_HEIGHT + (ITEM_HEIGHT - th) / 2, items[i],
                font, MENU_TEXT);
        }
    }
    isDirty = false;
}
