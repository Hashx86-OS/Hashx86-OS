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

#define KDBG_COMPONENT "GUI:MENU"
#include <gui/contextmenu.h>
#include <gui/event.h>
#include <gui/widgetHost.h>

ContextMenu::ContextMenu(Widget* parent) : Widget(parent, 0, 0, 140, 0), count(0) {
    this->isFocussable = false;
    this->isVisible = false;
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;
}

bool ContextMenu::AddItem(const char* label) {
    if (count >= MAX_ITEMS || !label) return false;
    size_t len = strlen(label);
    if (len >= sizeof(items[0])) len = sizeof(items[0]) - 1;
    memcpy(items[count], label, len);
    items[count][len] = '\0';
    count++;
    if (isVisible) h = count * ITEM_HEIGHT;
    MarkDirty();
    return true;
}

void ContextMenu::ShowAt(int32_t px, int32_t py) {
    this->x = px;
    this->y = py;
    this->h = count * ITEM_HEIGHT;
    this->isVisible = true;
    RequestTopmostFocus();
    MarkDirty();
}

void ContextMenu::Hide() {
    if (!isVisible) return;
    isVisible = false;
    MarkDirty();
}

void ContextMenu::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    if (!isVisible || button != 1) return;
    int32_t localY = y - this->y;
    int32_t localX = x - this->x;
    if (localX < 0 || localX >= w || localY < 0) return;

    int index = localY / ITEM_HEIGHT;
    if (index >= 0 && index < count) {
        if (g_widgetHost && PID != 0) {
            g_widgetHost->PostEvent(PID, ID, ON_CLICK, (uint32_t)index);
            g_widgetHost->WakeProcess(PID);
        }
    }
    Hide();
}

void ContextMenu::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }
    memset(cache, 0, sizeof(uint32_t) * w * h);
    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }

    NINA::activeInstance->FillRoundedRectangle(cache, w, h, 0, 0, w, h, 4, MENU_BG);
    NINA::activeInstance->DrawRoundedRectangle(cache, w, h, 0, 0, w, h, 4, MENU_BORDER);

    for (int i = 0; i < count; i++) {
        if (font) {
            int32_t th = (int32_t)font->getLineHeight();
            NINA::activeInstance->DrawString(cache, w, h, 8,
                                             i * ITEM_HEIGHT + (ITEM_HEIGHT - th) / 2, items[i],
                                             font, MENU_TEXT);
        }
    }
    isDirty = false;
}
