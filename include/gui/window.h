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

#ifndef WINDOW_H
#define WINDOW_H

#include <core/scheduler.h>
#include <gui/desktop.h>
#include <gui/elements/window_action_button_round.h>
#include <gui/widget.h>

/**
 * class Window - A draggable container window with a title bar and close button.
 *
 * A composite widget that hosts the client window content, renders a title
 * bar, and supports dragging by its title bar. Closing via the title-bar
 * button calls OnClose() to tear down the window.
 */
class Window : public CompositeWidget {
protected:
    bool isDragging;
    char* windowTitle;
    ACRButton* closeButton;

public:
    /**
     * Window() - Construct a window with an empty title and a close button.
     * @parent: Parent composite widget, or NULL for the root.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Window width in pixels.
     * @h: Window height in pixels.
     */
    Window(CompositeWidget* parent, int32_t x, int32_t y, int32_t w, int32_t h);

    /**
     * ~Window() - Release the title string and remove the window from the GUI.
     */
    ~Window();

    // Core Lifecycle.

    /** OnClose() - Request window teardown through the GUI event handler. */
    void OnClose();

    /**
     * setVisible() - Show or hide the window.
     * @val: True to show, false to hide.
     */
    void setVisible(bool val);

    /**
     * setWindowTitle() - Replace the title-bar text.
     * @title: New title; copied into the backing buffer.
     */
    void setWindowTitle(const char* title);

    /**
     * SetTitleFontSize() - Set the title-bar text size.
     * @px: Font size in pixels.
     */
    void SetTitleFontSize(int32_t px);

    /** getWindowTitle() - Return the current window title. */
    const char* getWindowTitle() const {
        return windowTitle;
    }

    // Drawing.

    /** Draw() - Raise the window in the Z-order and paint it with children. */
    void Draw(GraphicsDriver* gc) override;

    /** RedrawToCache() - Paint the window frame, title bar, and close button. */
    void RedrawToCache() override;

    // Type check.

    /** IsWindow() - Return true for window widget types. */
    bool IsWindow() const override {
        return true;
    }

    // Events.
    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) override;
};

#endif  // WINDOW_H
