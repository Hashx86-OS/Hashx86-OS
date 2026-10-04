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

#define KDBG_COMPONENT "GUI:TOOLTIP"
#include <gui/guiplat.h>
#include <gui/tooltip.h>

Tooltip::Tooltip(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text)
    : Widget(parent, x, y, w, h) {
    this->isFocussable = false;
    this->isVisible = false;
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;
    const char* t = text ? text : "";
    this->text = new char[strlen(t) + 1];
    if (!this->text) {
        guiplat::Abort("CRITICAL: Failed to allocate tooltip text!\n");
    }
    strcpy(this->text, t);
}

Tooltip::~Tooltip() {
    delete[] text;
    if (font) delete font;
}

void Tooltip::SetText(const char* newText) {
    const char* t = newText ? newText : "";
    if (text) delete[] text;
    text = new char[strlen(t) + 1];
    if (!text) {
        guiplat::Abort("CRITICAL: Failed to allocate tooltip text!\n");
    }
    strcpy(text, t);
    MarkDirty();
}

void Tooltip::Show() {
    if (isVisible) return;
    isVisible = true;
    MarkDirty();
}

void Tooltip::Hide() {
    if (!isVisible) return;
    isVisible = false;
    MarkDirty();
}

void Tooltip::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }
    memset(cache, 0, sizeof(uint32_t) * w * h);
    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }

    NINA::activeInstance->FillRoundedRectangle(cache, w, h, 0, 0, w, h, 3, TOOLTIP_BG);
    if (font && text) {
        NINA::activeInstance->DrawString(cache, w, h, 4, (h - (int32_t)font->getLineHeight()) / 2,
                                         text, font, TOOLTIP_TEXT);
    }
    isDirty = false;
}
