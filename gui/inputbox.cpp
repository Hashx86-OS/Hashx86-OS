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

#define KDBG_COMPONENT "GUI:INPUTBOX"
#include <gui/inputbox.h>
#include <string.h>

// Minimal helpers (the standard C library is not available).
static void memmove_local(char* dst, const char* src, uint32_t n) {
    if (dst < src) {
        for (uint32_t i = 0; i < n; i++) dst[i] = src[i];
    } else {
        for (uint32_t i = n; i > 0; i--) dst[i - 1] = src[i - 1];
    }
}

static int isprint_local(char c) {
    return (c >= 32 && c <= 126);  // Printable ASCII.
}

InputBox::InputBox(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t capacity)
    : Widget(parent, x, y, w, h), capacity(capacity > 0 ? capacity : 1), length(0), cursorPos(0) {
    this->font = FontManager::activeInstance->getNewFont();
    text = new char[this->capacity];
    if (!text) {
        HALT("CRITICAL: Failed to allocate inputbox text buffer!\n");
    }
    text[0] = '\0';  // Start empty.
}

InputBox::~InputBox() {
    delete[] text;
}

void InputBox::update() {
    if (!cache) return;
    // Clear the cache.
    for (uint32_t i = 0; i < w * h; i++) cache[i] = 0;
    isDirty = true;
}

void InputBox::setText(const char* newText) {
    if (!newText) return;

    uint32_t srcLen = strlen(newText);
    uint32_t newLen = (srcLen < capacity) ? srcLen : (capacity - 1);

    for (uint32_t i = 0; i < newLen; i++) text[i] = newText[i];

    text[newLen] = '\0';
    length = newLen;
    if (cursorPos > length) cursorPos = length;
    update();
}

void InputBox::setSize(FontSize size) {
    this->font->setSize(size);
    update();
}

void InputBox::setType(FontType type) {
    // Future: allow bold/italic variations.
    (void)type;
}

void InputBox::RedrawToCache() {
    NINA::activeInstance->FillRoundedRectangle(cache, w, h, 0, 0, w, h, 3,
                                               isFocused ? INPUT_BG_ACTIVE : INPUT_BG_NORMAL);
    NINA::activeInstance->DrawRoundedRectangle(
        cache, w, h, 0, 0, w, h, 3, isFocused ? INPUT_BORDER_ACTIVE : INPUT_BORDER_NORMAL);
    NINA::activeInstance->DrawString(cache, w, h, 2, 2, text, font,
                                     isFocused ? INPUT_TEXT_ACTIVE : INPUT_TEXT_NORMAL);

    isDirty = false;
}

void InputBox::Draw(GraphicsDriver* gc) {
    Widget::Draw(gc);
}

void InputBox::OnKeyDown(const char* key) {
    if (!key || !*key) return;

    // Backspace.
    if (strcmp(key, (char*)"Backspace") == 0) {
        if (cursorPos > 0) {
            memmove_local(&text[cursorPos - 1], &text[cursorPos], length - cursorPos + 1);
            cursorPos--;
            length--;
            update();
        }
        return;
    }

    // Normal printable characters.
    if (length < capacity - 1 && key[1] == '\0' && isprint_local(key[0])) {
        memmove_local(&text[cursorPos + 1], &text[cursorPos], length - cursorPos + 1);
        text[cursorPos] = key[0];
        cursorPos++;
        length++;
        text[length] = '\0';
        update();
    }
}

void InputBox::OnKeyUp(const char* key) {
    // Usually not needed for text input.
    (void)key;
}
