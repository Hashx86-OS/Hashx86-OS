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

#ifndef LABEL_H
#define LABEL_H

#include <gui/widget.h>
#include <types.h>

/**
 * enum HAlign - Horizontal text alignment inside a label.
 */
enum HAlign { LEFT = 0, CENTER = 1, RIGHT = 2 };

/**
 * enum VAlign - Vertical text alignment inside a label.
 */
enum VAlign { TOP = 0, MIDDLE = 1, BOTTOM = 2 };

/**
 * class Label - A widget that renders a single line of text.
 *
 * Draws text with configurable color, background, alignment, font size, and
 * style. When sized CONTENT, the label shrinks to fit the text.
 */
class Label : public Widget {
private:
    char* text;
    uint32_t textColor = LABEL_TEXT_NORMAL;
    uint32_t bgColor = 0;
    HAlign hAlign = LEFT;
    VAlign vAlign = TOP;

public:
    /**
     * Label() - Construct a label with initial text.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Label width in pixels.
     * @h: Label height in pixels.
     * @text: Initial text, copied into the backing buffer.
     */
    Label(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text);

    /**
     * ~Label() - Free the text buffer.
     */
    ~Label();

    /** update() - Repaint the label cache and redraw it. */
    void update();

    /**
     * setText() - Replace the label text and repaint.
     * @text: New text, copied into the backing buffer.
     */
    void setText(const char* text);

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

    /**
     * setFontSize() - Set the font size in pixels.
     * @px: Font size in pixels.
     */
    void setFontSize(int32_t px);

    /**
     * setColor() - Set the text color.
     * @argb: Text color as 0xAARRGGBB.
     */
    void setColor(uint32_t argb);

    /**
     * setBackground() - Set the label background color.
     * @argb: Background color as 0xAARRGGBB.
     */
    void setBackground(uint32_t argb);

    /**
     * setAlignment() - Set horizontal and vertical text alignment.
     * @ha: Horizontal alignment.
     * @va: Vertical alignment.
     */
    void setAlignment(HAlign ha, VAlign va);

    /** RedrawToCache() - Paint the background and aligned text. */
    void RedrawToCache() override;

    /** Recalc() - Size to the text when the size mode is CONTENT. */
    void Recalc() override;

    /** IsLabel() - Return true for label widget types. */
    bool IsLabel() const override {
        return true;
    }
};

#endif
