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

#ifndef TEXTBOX_H
#define TEXTBOX_H

#include <gui/widget.h>

/**
 * class TextBox - A multi-line editable text area.
 *
 * Stores up to a fixed capacity of characters including newlines. Accepts
 * printable characters, Enter, Backspace, Delete and the arrow keys while
 * focused. Read-only mode ignores all edit input.
 */
class TextBox : public Widget {
private:
    char* buffer;
    uint32_t capacity;
    uint32_t length;
    uint32_t cursor;
    bool readOnly;

    void Insert(char c);
    void Backspace();
    void Delete();

public:
    /**
     * TextBox() - Construct an empty text box.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Widget width in pixels.
     * @h: Widget height in pixels.
     * @capacity: Maximum stored characters including the terminator.
     */
    TextBox(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t capacity);

    /** ~TextBox() - Release the text buffer. */
    ~TextBox();

    /** SetText() - Replace the contents and move the cursor to the end. */
    void SetText(const char* text);

    /** GetText() - Return the NUL-terminated contents. */
    const char* GetText() const {
        return buffer;
    }

    /** GetLength() - Return the number of stored characters. */
    uint32_t GetLength() const {
        return length;
    }

    /** SetReadOnly() - Toggle whether edit input is ignored. */
    void SetReadOnly(bool value) {
        readOnly = value;
    }

    void RedrawToCache() override;
    void OnKeyDown(const char* key) override;
    void OnSpecialKeyDown(uint8_t key) override;
};

#endif  // TEXTBOX_H
