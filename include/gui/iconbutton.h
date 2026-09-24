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

#ifndef ICONBUTTON_H
#define ICONBUTTON_H

#include <gui/button.h>

/**
 * struct IconMapping - Maps a symbolic icon name to its glyph codepoint.
 * @name: Human-readable icon name, e.g. "close".
 * @codepoint: Unicode codepoint of the glyph in the icon font.
 */
struct IconMapping {
    const char* name;
    uint32_t codepoint;
};

/**
 * class IconButton - A button that renders a glyph from the icon font.
 *
 * Displays an icon either alone or with a label, resolved from a symbolic name
 * through the shared icon table. Icons are sized independently of the label.
 */
class IconButton : public Button {
public:
    /** enum IconMode - How the button lays out its icon and label. */
    enum IconMode {
        ICON_ONLY,        // Render only the icon glyph.
        ICON_WITH_LABEL,  // Render the icon beside the label text.
    };

    /**
     * IconButton() - Construct an icon-only button.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Button width in pixels.
     * @h: Button height in pixels.
     * @iconName: Symbolic icon name to look up.
     */
    IconButton(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* iconName);

    /**
     * IconButton() - Construct a button with an icon and a label.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Button width in pixels.
     * @h: Button height in pixels.
     * @iconName: Symbolic icon name to look up.
     * @label: Label text rendered beside the icon.
     */
    IconButton(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* iconName,
               const char* label);

    /**
     * ~IconButton() - Destroy the icon button.
     */
    ~IconButton();

    /**
     * SetIcon() - Switch the button to a different icon and repaint.
     * @iconName: Symbolic icon name to look up.
     */
    void SetIcon(const char* iconName);

    /**
     * SetIconMode() - Toggle between icon-only and icon-with-label layout.
     * @mode: New layout mode.
     */
    void SetIconMode(IconMode mode);

    /**
     * SetIconFontSize() - Set the icon glyph size.
     * @px: Font size in pixels.
     */
    void SetIconFontSize(int32_t px);

    /** IsIconButton() - Return true for icon button widget types. */
    bool IsIconButton() const override {
        return true;
    }

    /** RedrawToCache() - Paint the icon and optional label. */
    void RedrawToCache() override;

    /**
     * SetIconWidth() - Set the icon cell width.
     * @w: New width in pixels.
     */
    void SetIconWidth(int32_t w);

    /**
     * SetIconHeight() - Set the icon cell height.
     * @h: New height in pixels.
     */
    void SetIconHeight(int32_t h);

    /**
     * LookupIcon() - Resolve an icon name to a glyph codepoint.
     * @name: Symbolic icon name.
     *
     * Return: The matching codepoint, or 0 if @name is unknown.
     */
    static uint32_t LookupIcon(const char* name);

private:
    uint32_t iconCodepoint;
    IconMode iconMode;
    Font* iconFont;

    /** init() - Shared constructor setup applied before widget construction. */
    void init(const char* iconName);

    /** calculateMinSize() - Size the button to fit the icon and label. */
    void calculateMinSize();

    static const IconMapping iconTable[];
    static const int iconTableSize;
};

#endif
