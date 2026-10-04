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

#define KDBG_COMPONENT "GUI:TABS"
#include <gui/event.h>
#include <gui/tabs.h>
#include <gui/widgetHost.h>

Tabs::Tabs(CompositeWidget* parent, int32_t x, int32_t y, int32_t w, int32_t h)
    : CompositeWidget(parent, x, y, w, h), tabCount(0), active(-1) {
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;
    content = new CompositeWidget(this, 0, TABS_HEIGHT, w, h - TABS_HEIGHT);
    AddChild(content);
}

Tabs::~Tabs() {
    if (font) delete font;
    // content is owned by childrenList.
}

bool Tabs::AddTab(const char* title) {
    if (tabCount >= MAX_TABS || !title) return false;
    size_t len = strlen(title);
    if (len >= sizeof(titles[0])) len = sizeof(titles[0]) - 1;
    memcpy(titles[tabCount], title, len);
    titles[tabCount][len] = '\0';
    tabCount++;
    if (active < 0) active = 0;
    MarkDirty();
    return true;
}

void Tabs::SetActive(int index) {
    if (index < 0 || index >= tabCount) return;
    if (index == active) return;
    active = index;
    MarkDirty();
}

void Tabs::Select(int index) {
    if (index < 0 || index >= tabCount) return;
    active = index;
    MarkDirty();
    if (g_widgetHost && PID != 0) {
        g_widgetHost->PostEvent(PID, ID, ON_CLICK, (uint32_t)index);
        g_widgetHost->WakeProcess(PID);
    }
}

void Tabs::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    int32_t localX = x - this->x;
    int32_t localY = y - this->y;

    if (localY >= 0 && localY < TABS_HEIGHT && button == 1) {
        int index = localX / TAB_WIDTH;
        if (index >= 0 && index < tabCount) {
            Select(index);
            return;
        }
    }

    CompositeWidget::OnMouseUp(x, y, button);
}

void Tabs::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }
    memset(cache, 0, sizeof(uint32_t) * w * h);
    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }

    NINA::activeInstance->FillRectangle(cache, w, h, 0, 0, w, TABS_HEIGHT, TABS_STRIP_BG);

    for (int i = 0; i < tabCount; i++) {
        int32_t tx = i * TAB_WIDTH;
        bool isActive = (i == active);
        uint32_t bg = isActive ? TABS_TAB_ACTIVE_BG : TABS_TAB_BG;
        uint32_t fg = isActive ? TABS_TAB_ACTIVE_TEXT : TABS_TAB_TEXT;
        NINA::activeInstance->FillRectangle(cache, w, h, tx + 1, 2, TAB_WIDTH - 2, TABS_HEIGHT - 2,
                                            bg);
        if (font) {
            int32_t th = (int32_t)font->getLineHeight();
            NINA::activeInstance->DrawString(cache, w, h, tx + 6, (TABS_HEIGHT - th) / 2, titles[i],
                                             font, fg);
        }
    }
    isDirty = false;
}
