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

#ifndef TERMINALVIEW_H
#define TERMINALVIEW_H

#include <gui/fonts/font.h>
#include <gui/widget.h>
#include <utils/linkedList.h>

/**
 * class TerminalView - A scrollable fixed-pitch terminal text view.
 *
 * Renders a NUL-terminated text buffer with an optional scrollbar, queues key
 * events for the owning process, and reports scroll actions through a
 * consume/ack pair. The scroll range is set externally via setScrollMeta().
 */
class TerminalView : public Widget {
public:
    /** enum KeyEventType - Classification of a queued key event. */
    enum KeyEventType {
        KEY_EVENT_NONE,     // Empty queue slot.
        KEY_EVENT_NORMAL,   // Printable character key.
        KEY_EVENT_SPECIAL,  // Non-printable key (arrows, function keys).
    };

    /** struct KeyEvent - One queued key event awaiting delivery. */
    struct KeyEvent {
        KeyEventType type;
        char key;
        uint8_t specialKey;
    };

private:
    char* text;
    FontSize fontSize;
    int scrollTotal;
    int scrollVisible;
    int scrollOffset;
    int pendingScrollAction;

    bool isDraggingThumb;
    int dragStartY;
    int dragStartOffset;
    LinkedList<KeyEvent> keyEventQueue;

    /** PutPixel() - Write a single pixel into the cache. */
    void PutPixel(int32_t px, int32_t py, uint32_t color);

    /** DrawScrollBar() - Paint the scrollbar thumb and track. */
    void DrawScrollBar();

    /** DrawCharacter() - Blit one glyph into the cache. */
    void DrawCharacter(char c, int32_t x, int32_t y, uint32_t color);

    // Scrollbar geometry helpers.
    int ScrollBarX() const;
    int ScrollBarY() const;
    int ScrollBarW() const;
    int ScrollBarH() const;
    int ScrollBtnH() const;
    int TrackY() const;
    int TrackH() const;
    int ThumbH() const;
    int ThumbY() const;
    int MaxOffset() const;

public:
    /**
     * TerminalView() - Construct a terminal view.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: View width in pixels.
     * @h: View height in pixels.
     * @text: Initial text, copied into the backing buffer.
     */
    TerminalView(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text);

    /**
     * ~TerminalView() - Free the text buffer.
     */
    ~TerminalView();

    /**
     * setText() - Replace the terminal text and repaint.
     * @newText: New text, copied into the backing buffer.
     */
    void setText(const char* newText);

    /**
     * setSize() - Select the font size slot.
     * @size: FontSize slot to use.
     */
    void setSize(FontSize size);

    /**
     * SetFontSize() - Set the font size in pixels.
     * @px: Font size in pixels.
     */
    void SetFontSize(int32_t px);

    /**
     * setScrollMeta() - Configure the scrollbar geometry.
     * @totalLines: Total line count in the buffer.
     * @visibleLines: Line count visible without scrolling.
     * @offset: Current line offset.
     */
    void setScrollMeta(int totalLines, int visibleLines, int offset);

    /**
     * consumeScrollAction() - Drain the pending scroll action.
     *
     * Returns the pending offset-change action encoded as a large negative
     * sentinel value and clears it.
     *
     * Return: The pending action, or 0 if none.
     */
    int consumeScrollAction();

    /** IsTerminalView() - Return true for terminal view widget types. */
    bool IsTerminalView() const override {
        return true;
    }

    /** RedrawToCache() - Paint the text and scrollbar. */
    void RedrawToCache() override;

    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) override;

    /** OnKeyDown() - Queue a printable key event. */
    void OnKeyDown(const char* key) override;

    /** OnSpecialKeyDown() - Queue a special key event. */
    void OnSpecialKeyDown(uint8_t key) override;

    /**
     * consumeKeyEvent() - Pop the oldest queued key event.
     *
     * Return: The front KeyEvent, or an empty KEY_EVENT_NONE entry.
     */
    KeyEvent consumeKeyEvent();
};

#endif  // TERMINALVIEW_H
