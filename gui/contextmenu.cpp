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

ContextMenu* ContextMenu::activeMenu = nullptr;

ContextMenu::ContextMenu(Widget* parent) : Widget(parent, 0, 0, 140, 0), count(0) {
    this->isFocussable = false;
    this->isVisible = false;
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;
}

ContextMenu::~ContextMenu() {
    if (activeMenu == this) activeMenu = nullptr;
    if (font) delete font;
}

bool ContextMenu::AddItem(const char* label) {
    if (count >= MAX_ITEMS || !label) return false;
    size_t len = strlen(label);
    if (len >= sizeof(items[0])) len = sizeof(items[0]) - 1;
    memcpy(items[count], label, len);
    items[count][len] = '\0';
    count++;
    if (isVisible) Resize(w, count * ITEM_HEIGHT);
    MarkDirty();
    return true;
}

void ContextMenu::ShowAt(int32_t px, int32_t py) {
    // An empty menu has no rows to show; activating it would leave a
    // zero-height popup that swallows the next press.
    if (count == 0) return;

    // A second menu replaces the first.
    if (activeMenu && activeMenu != this) activeMenu->Hide();

    Resize(w, count * ITEM_HEIGHT);

    // Keep the whole popup on the parent surface.
    if (parent) {
        if (px + w > parent->w) px = parent->w - w;
        if (py + h > parent->h) py = parent->h - h;
    }
    if (px < 0) px = 0;
    if (py < 0) py = 0;

    this->x = px;
    this->y = py;
    this->isVisible = true;
    activeMenu = this;
    RequestTopmostFocus();
    MarkDirty();
}

void ContextMenu::Hide() {
    if (activeMenu == this) activeMenu = nullptr;
    if (!isVisible) return;
    isVisible = false;
    MarkDirty();
}

void ContextMenu::DismissActiveAt(int32_t screenX, int32_t screenY) {
    ContextMenu* menu = activeMenu;
    if (!menu) return;

    int32_t originX = 0;
    int32_t originY = 0;
    menu->ModelToScreen(originX, originY);
    if (screenX < originX || screenX >= originX + menu->w || screenY < originY ||
        screenY >= originY + menu->h) {
        menu->Hide();
    }
}

void ContextMenu::HideActive() {
    if (activeMenu) activeMenu->Hide();
}

void ContextMenu::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    if (!isVisible || button != 1) return;

    int32_t localY = y - this->y;
    int32_t localX = x - this->x;
    if (localX < 0 || localX >= w || localY < 0) return;

    int index = localY / ITEM_HEIGHT;
    bool selected = (index >= 0 && index < count);

    // Hide before notifying: the callback may open a window or launch a
    // process, and the menu must already be out of the tree's visible state.
    Hide();

    if (selected) {
        if (g_widgetHost && PID != 0) {
            g_widgetHost->PostEvent(PID, ID, ON_CLICK, (uint32_t)index);
            g_widgetHost->WakeProcess(PID);
        }
        if (onClick) onClick(this, index, onClickUser);
    }
}

void ContextMenu::Draw(RasterSurface* gc) {
    if (!isVisible || !cache || !gc || w <= 0 || h <= 0) return;

    if (isDirty) {
        RedrawToCache();
        isDirty = false;
    }

    int32_t X = 0;
    int32_t Y = 0;
    ModelToScreen(X, Y);
    gc->DrawBitmap(X, Y, (const uint32_t*)cache, w, h);
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
