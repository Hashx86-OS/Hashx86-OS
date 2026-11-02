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

#ifndef TABS_H
#define TABS_H

#include <gui/widget.h>

/**
 * class Tabs - A tab strip with a content area below it.
 *
 * Each tab has a fixed-width header; clicking a header activates it and emits
 * ON_CLICK with the tab index. Application content is added to the composite
 * returned by GetContentArea().
 */
class Tabs : public CompositeWidget {
private:
    static const int MAX_TABS = 16;
    static const int TAB_WIDTH = 80;
    char titles[MAX_TABS][32];
    int tabCount;
    int active;
    int pressedTab = -1;
    CompositeWidget* content;

    void Select(int index);

public:
    /**
     * Tabs() - Construct an empty tab control.
     * @parent: Parent widget, or NULL for a root widget.
     * @x: X position relative to @parent.
     * @y: Y position relative to @parent.
     * @w: Widget width in pixels.
     * @h: Widget height in pixels.
     */
    Tabs(CompositeWidget* parent, int32_t x, int32_t y, int32_t w, int32_t h);

    /** ~Tabs() - Destroy the tab control and its content area. */
    ~Tabs();

    /** AddTab() - Append a tab; the first tab added becomes active. */
    bool AddTab(const char* title);

    /** GetTabCount() - Return the number of tabs. */
    int GetTabCount() const {
        return tabCount;
    }

    /** GetActive() - Return the active tab index, or -1 when empty. */
    int GetActive() const {
        return active;
    }

    /** SetActive() - Activate a tab without emitting an event. */
    void SetActive(int index);

    /** GetContentArea() - Return the composite that hosts tab content. */
    CompositeWidget* GetContentArea() {
        return content;
    }

    void RedrawToCache() override;
    void OnMouseDown(int32_t x, int32_t y, uint8_t button) override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
};

#endif  // TABS_H
