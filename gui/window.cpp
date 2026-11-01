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

#define KDBG_COMPONENT "GUI:WINDOW"
#include <gui/window.h>

Window::Window(CompositeWidget* parent, int32_t x, int32_t y, int32_t w, int32_t h)
    : CompositeWidget(parent, x, y, w, h) {
    this->isDragging = false;
    this->windowTitle = new char[10];
    if (!this->windowTitle) {
        guiplat::Abort("CRITICAL: Failed to allocate window title!\n");
    }
    strcpy(this->windowTitle, "Untitled");
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;

    closeButton = new ACRButton(this, w - 22, 4, "x");
    if (!closeButton) {
        guiplat::Abort("CRITICAL: Failed to allocate window close button!\n");
    }
    closeButton->OnClick(this, [](void* instance) { static_cast<Window*>(instance)->OnClose(); });

    this->AddChild(closeButton);
}

Window::~Window() {
    delete[] windowTitle;
    if (font) delete font;
    // closeButton is owned by childrenList (added via AddChild in the
    // constructor), so ~CompositeWidget() handles its deletion automatically.
}

void Window::OnClose() {
    if (this->parent) this->parent->MarkDirty();

    // Route the close through the widget host so the owning process decides
    // what to do. When there is no host, no owner, or no handler for the owner,
    // fall back to hiding directly, which is what kernel/system windows rely on.
    if (g_widgetHost && this->PID != 0) {
        if (g_widgetHost->PostEvent(this->PID, this->ID, ON_WINDOW_CLOSE)) {
            g_widgetHost->WakeProcess(this->PID);
            return;
        }
    }

    this->setVisible(false);

    // Cached system dialogs (power confirmation, crash reporter) are hidden
    // and reused, so they stay attached and only become invisible.
    if (!deleteOnClose) {
        MarkDirty();
        return;
    }

    // Detach first so the widget is unreachable from the live tree, then hand
    // it to the environment for deferred destruction. Deleting here would free
    // the window while the close-button callback (or the composite walking its
    // children) is still on the stack.
    if (this->parent) this->parent->RemoveChild(this);
    if (g_widgetHost) {
        g_widgetHost->DestroyWidgetLater(this);
    }
    // With no host installed, ownership remains with the caller.
}

void Window::SetTitleFontSize(int32_t px) {
    if (!FontManager::activeInstance) return;
    FontSize slot = Font::PixelToFontSlot(px);
    FontType type = this->font ? this->font->fontType : REGULAR;
    Font* newFont = FontManager::activeInstance->getNewFont(slot, type);
    if (!newFont) return;
    delete this->font;
    this->font = newFont;
    MarkDirty();
}

void Window::setWindowTitle(const char* title) {
    if (!title) title = "";
    if (windowTitle) delete[] windowTitle;
    windowTitle = new char[strlen(title) + 1];
    if (!windowTitle) {
        guiplat::Abort("CRITICAL: Failed to allocate window title!\n");
    }
    strcpy(windowTitle, title);
    MarkDirty();
}

void Window::setVisible(bool val) {
    this->isVisible = val;
    closeButton->isVisible = val;
    MarkDirty();
}

void Window::Draw(RasterSurface* gc) {
    int X = 0, Y = 0;
    ModelToScreen(X, Y);

    for (auto& child : childrenList) {
        if (child->isDirty) {
            this->isDirty = true;
        }
    }

    if (isDirty) {
        if (isVisible) {
            RedrawToCache();
        } else if (cache && w > 0 && h > 0) {
            memset(cache, 0, sizeof(uint32_t) * (size_t)w * (size_t)h);
        }
        isDirty = false;
    }

    if (isVisible && cache && w > 0 && h > 0) {
        gc->DrawBitmap(X, Y, (const uint32_t*)cache, w, h);
    }
}

void Window::RedrawToCache() {
    if (!cache || !NINA::activeInstance) {
        isDirty = false;
        return;
    }

    uint32_t borderColor = isFocused ? WINDOW_BORDER_PRESSED : WINDOW_BORDER_NORMAL;
    NINA::activeInstance->FillRoundedRectangle(cache, w, h, 0, 0, w, h, 6, WINDOW_BG);
    NINA::activeInstance->DrawRoundedRectangle(cache, w, h, 0, 0, w, h, 6, borderColor);
    NINA::activeInstance->DrawBitmap(cache, w, h, 4, 2, (const uint32_t*)icon_main_20x20, 20, 20);
    if (font) {
        NINA::activeInstance->DrawString(cache, w, h, 28, 3, windowTitle, font, WINDOW_TITLE);
    }

    for (auto& child : childrenList) {
        if (!child->isVisible) continue;
        if (child->w <= 0 || child->h <= 0 || !child->cache) continue;
        if (child->isDirty) child->RedrawToCache();

        NINA::activeInstance->DrawBitmapToBuffer(cache, w, h, child->x, child->y, child->cache,
                                                 child->w, child->h);
    }
}

void Window::OnMouseDown(int32_t x, int32_t y, uint8_t button) {
    if (!enabled || !isVisible) return;

    // Convert desktop coordinates to window-local coordinates.
    int32_t localX = x - this->x;
    int32_t localY = y - this->y;

    // Check the header area (top 25 pixels).
    if (localX >= 0 && localX <= w - 26 && localY >= 0 && localY <= 25) {
        isDragging = (button == 1);
        if (isDragging) {
            this->RequestTopmostFocus();
        }
    }

    // Pass the original coordinates to the base (it converts them for children).
    CompositeWidget::OnMouseDown(x, y, button);
}

void Window::OnMouseUp(int32_t x, int32_t y, uint8_t button) {
    isDragging = false;
    CompositeWidget::OnMouseUp(x, y, button);
}

void Window::OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) {
    if (!isVisible) return;
    if (!enabled) {
        // A disabled window neither moves nor drags; drop any title-drag that
        // was in progress when it was disabled.
        isDragging = false;
        return;
    }

    bool inside = ContainsCoordinate(oldx, oldy);

    if (inside && isDragging) {
        int32_t dx = newx - oldx;
        int32_t dy = newy - oldy;

        this->x += dx;
        this->y += dy;

        if (this->parent) this->parent->MarkDirty();

        // Do not forward this move to children: the window origin just moved,
        // so translating the same pointer through the *new* origin would hand
        // the subtree a shifted pair of coordinates and fire phantom
        // enter/leave/hover events while the user is only dragging the title.
        return;
    }

    if (!inside) this->isDragging = false;

    CompositeWidget::OnMouseMove(oldx, oldy, newx, newy);
}
