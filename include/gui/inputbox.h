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

#ifndef INPUTBOX_H
#define INPUTBOX_H

#include <gui/widget.h>
#include <types.h>

/**
 * class InputBox - A single-line text input field with a cursor.
 *
 * Buffers typed characters up to a fixed capacity, tracks a cursor position,
 * and paints a blinking caret. Supports printable input and backspace.
 */
class InputBox : public Widget {
private:
    char* text;
    uint32_t capacity;   // Max buffer size.
    uint32_t length;     // Current text length.
    uint32_t cursorPos;  // Cursor index into @text.

public:
    /**
     * InputBox() - Construct an input field.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Input width in pixels.
     * @h: Input height in pixels.
     * @capacity: Maximum number of characters the buffer can hold.
     */
    InputBox(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, uint32_t capacity = 256);

    /**
     * ~InputBox() - Free the text buffer.
     */
    ~InputBox();

    /** update() - Refresh the input display. */
    void update();

    /**
     * setText() - Replace the input text and reset the cursor to the start.
     * @newText: New text, truncated to capacity.
     */
    void setText(const char* newText);

    /** getText() - Return the current input text. */
    const char* getText() const {
        return text;
    }

    /**
     * setSize() - Select the font size slot.
     * @size: FontSize slot to use.
     */
    void setSize(FontSize size);

    /**
     * setType() - Select the font style slot.
     * @type: FontType slot to use.
     */
    void setType(FontType type);

    /** RedrawToCache() - Paint the field background, text, and caret. */
    void RedrawToCache() override;

    /** Draw() - Blit the input field to the screen. */
    void Draw(GraphicsDriver* gc) override;

    /** OnKeyDown() - Insert the typed character and move the cursor. */
    void OnKeyDown(const char* key) override;

    /** OnKeyUp() - Acknowledged key release. */
    void OnKeyUp(const char* key) override;
};

#endif  // INPUTBOX_H
