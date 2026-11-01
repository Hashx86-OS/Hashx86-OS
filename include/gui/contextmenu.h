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

#ifndef CONTEXTMENU_H
#define CONTEXTMENU_H

#include <gui/widget.h>

/**
 * class ContextMenu - A popup list of selectable items.
 *
 * Add items, then ShowAt() to place it at a point. Clicking an item emits
 * ON_CLICK with param1 set to the item index and hides the menu. The menu is
 * non-focussable and starts hidden.
 */
class ContextMenu : public Widget {
private:
    static const int MAX_ITEMS = 16;
    static const int ITEM_HEIGHT = 24;
    char items[MAX_ITEMS][48];
    int count;

    // Optional owner callback, fired with the selected index. Lets a
    // kernel-owned menu act without an owning process to post an event to.
    void (*onClick)(ContextMenu*, int32_t, void*) = nullptr;
    void* onClickUser = nullptr;

    // The menu currently shown, if any. Used to dismiss it on an outside press
    // regardless of which window owns it.
    static ContextMenu* activeMenu;

public:
    /** ContextMenu() - Construct an empty hidden menu. */
    explicit ContextMenu(Widget* parent);

    /** ~ContextMenu() - Clear the active-popup registration. */
    ~ContextMenu();

    /** AddItem() - Append a selectable item. */
    bool AddItem(const char* label);

    /**
     * SetClickHandler() - Install an owner callback for item selection.
     * @cb: Called as cb(this, index, user).
     * @user: Opaque pointer handed back to @cb.
     */
    void SetClickHandler(void (*cb)(ContextMenu*, int32_t, void*), void* user) {
        onClick = cb;
        onClickUser = user;
    }

    /** ShowAt() - Position and show the menu at (@x, @y) in parent coordinates. */
    void ShowAt(int32_t x, int32_t y);

    /** Hide() - Hide the menu and clear its active registration. */
    void Hide();

    /**
     * DismissActiveAt() - Hide the active menu if a press lands outside it.
     * @screenX: Press X in screen coordinates.
     * @screenY: Press Y in screen coordinates.
     *
     * Called by the desktop before routing any mouse-down, so menus owned by a
     * window are dismissed just like desktop-owned ones.
     */
    static void DismissActiveAt(int32_t screenX, int32_t screenY);

    /** HideActive() - Hide whichever context menu is open, if any. */
    static void HideActive();

    /** GetItemCount() - Return the number of items. */
    int GetItemCount() const {
        return count;
    }

    /** Draw() - Blit the menu, so it works as a top-level popup. */
    void Draw(RasterSurface* gc) override;

    void RedrawToCache() override;
    void OnMouseUp(int32_t x, int32_t y, uint8_t button) override;
};

#endif  // CONTEXTMENU_H
