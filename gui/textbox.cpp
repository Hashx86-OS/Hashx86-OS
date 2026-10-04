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

#define KDBG_COMPONENT "GUI:TEXTBOX"
#include <gui/guiplat.h>
#include <gui/textbox.h>

TextBox::TextBox(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t capacity)
    : Widget(parent, x, y, w, h),
      capacity(capacity > 1 ? capacity : 2),
      length(0),
      cursor(0),
      readOnly(false) {
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;
    buffer = new char[this->capacity];
    if (!buffer) {
        guiplat::Abort("CRITICAL: Failed to allocate textbox buffer!\n");
    }
    buffer[0] = '\0';
}

TextBox::~TextBox() {
    delete[] buffer;
    if (font) delete font;
}

void TextBox::SetText(const char* text) {
    if (!text) text = "";
    uint32_t i = 0;
    while (text[i] != '\0' && i + 1 < capacity) {
        buffer[i] = text[i];
        i++;
    }
    buffer[i] = '\0';
    length = i;
    cursor = length;
    MarkDirty();
}

void TextBox::Insert(char c) {
    if (readOnly) return;
    if (length + 1 >= capacity) return;
    for (uint32_t i = length; i > cursor; i--) {
        buffer[i] = buffer[i - 1];
    }
    buffer[cursor] = c;
    cursor++;
    length++;
    buffer[length] = '\0';
    MarkDirty();
}

void TextBox::Backspace() {
    if (readOnly) return;
    if (cursor == 0) return;
    for (uint32_t i = cursor - 1; i < length; i++) {
        buffer[i] = buffer[i + 1];
    }
    cursor--;
    length--;
    MarkDirty();
}

void TextBox::Delete() {
    if (readOnly) return;
    if (cursor >= length) return;
    for (uint32_t i = cursor; i < length; i++) {
        buffer[i] = buffer[i + 1];
    }
    length--;
    MarkDirty();
}

void TextBox::OnKeyDown(const char* key) {
    if (readOnly || !key || key[0] == '\0') return;
    if (key[0] == '\r' || key[0] == '\n') {
        Insert('\n');
        return;
    }
    if ((unsigned char)key[0] >= 32 && (unsigned char)key[0] <= 126) {
        Insert(key[0]);
    }
}

void TextBox::OnSpecialKeyDown(uint8_t key) {
    if (readOnly) return;
    switch (key) {
        case 0x4B:  // Left.
            if (cursor > 0) cursor--;
            MarkDirty();
            break;
        case 0x4D:  // Right.
            if (cursor < length) cursor++;
            MarkDirty();
            break;
        case 0x0E:  // Backspace.
            Backspace();
            break;
        case 0x53:  // Delete.
            Delete();
            break;
        default:
            break;
    }
}

void TextBox::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }
    memset(cache, 0, sizeof(uint32_t) * w * h);
    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }

    NINA::activeInstance->FillRectangle(cache, w, h, 0, 0, w, h, INPUT_BG_NORMAL);
    NINA::activeInstance->DrawRectangle(cache, w, h, 0, 0, w, h, INPUT_BORDER_NORMAL);

    if (!font) {
        isDirty = false;
        return;
    }

    int32_t lineH = (int32_t)font->getLineHeight();
    int32_t x = 3;
    int32_t y = 3;
    for (uint32_t i = 0; i < length; i++) {
        char c = buffer[i];
        if (c == '\n') {
            x = 3;
            y += lineH;
            continue;
        }
        if (x + 8 > w - 3) {
            x = 3;
            y += lineH;
        }
        NINA::activeInstance->DrawCharacter(cache, w, h, x, y, c, font, INPUT_TEXT_NORMAL);
        x += 8;
    }
    isDirty = false;
}
