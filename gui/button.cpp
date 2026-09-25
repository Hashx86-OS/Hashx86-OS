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

#define KDBG_COMPONENT "GUI:BUTTON"
#include <gui/button.h>

Button::Button(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* label)
    : Widget(parent, x, y, w, h), isPressed(false) {
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;

    if (label == nullptr) label = "";
    this->label = new char[strlen(label) + 1];
    if (!this->label) {
        HALT("CRITICAL: Failed to allocate button label!\n");
    }
    strcpy(this->label, label);

    // The cache is allocated by the Widget constructor; do not reallocate here.
}

Button::~Button() {
    if (label) delete[] label;
    if (font) delete font;
    // The cache is owned and freed by ~Widget().
}

void Button::update() {
    MarkDirty();
}

void Button::SetLabel(const char* newLabel) {
    if (newLabel == nullptr) newLabel = "";
    if (this->label) delete[] this->label;
    this->label = new char[strlen(newLabel) + 1];
    if (!this->label) {
        HALT("CRITICAL: Failed to allocate button label!\n");
    }
    strcpy(this->label, newLabel);
    MarkDirty();
}

void Button::SetWidth(int32_t reqW) {
    int32_t minW = this->font ? this->font->getStringLength(label) + 4 : 0;
    this->w = (reqW < minW) ? minW : reqW;

    if (cache) delete[] cache;
    cache = nullptr;
    if (w > 0 && h > 0 && (size_t)this->w * (size_t)this->h / (size_t)this->w == (size_t)this->h) {
        cache = new uint32_t[this->w * this->h]();
    }
    MarkDirty();
}

void Button::SetHeight(int32_t reqH) {
    int32_t minH = this->font ? this->font->getLineHeight() + 4 : 0;
    this->h = (reqH < minH) ? minH : reqH;

    if (cache) delete[] cache;
    cache = nullptr;
    if (w > 0 && h > 0 && (size_t)this->w * (size_t)this->h / (size_t)this->w == (size_t)this->h) {
        cache = new uint32_t[this->w * this->h]();
    }
    MarkDirty();
}

void Button::RedrawToCache() {
    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }

    uint32_t bgColor;
    uint32_t borderColor;
    uint32_t textColor;

    if (!enabled) {
        bgColor = BUTTON_BG_DISABLED;
        borderColor = BUTTON_BORDER_DISABLED;
        textColor = BUTTON_TEXT_DISABLED;
    } else if (isPressed) {
        bgColor = BUTTON_BG_PRESSED;
        borderColor = BUTTON_BORDER_PRESSED;
        textColor = BUTTON_TEXT_PRESSED;
    } else if (isHovered) {
        bgColor = BUTTON_BG_HOVER;
        borderColor = BUTTON_BORDER_NORMAL;
        textColor = BUTTON_TEXT_NORMAL;
    } else {
        bgColor = BUTTON_BG_NORMAL;
        borderColor = BUTTON_BORDER_NORMAL;
        textColor = BUTTON_TEXT_NORMAL;
    }

    NINA::activeInstance->FillRoundedRectangle(cache, w, h, 0, 0, w, h, 3, bgColor);
    NINA::activeInstance->DrawRoundedRectangle(cache, w, h, 0, 0, w, h, 3, borderColor);

    if (this->font) {
        int textX = (w - this->font->getStringLength(label)) / 2;
        int textY = (h - this->font->getLineHeight()) / 2;
        NINA::activeInstance->DrawString(cache, w, h, textX, textY, label, font, textColor);
    }

    if (isFocused && enabled) {
        NINA::activeInstance->DrawRoundedRectangle(cache, w, h, 1, 1, w - 2, h - 2, 3,
                                                   BUTTON_BORDER_FOCUS);
    }

    isDirty = false;
}

void Button::OnMouseDown(int32_t x, int32_t y, uint8_t button) {
    if (!isVisible) return;

    Widget::OnMouseDown(x, y, button);

    isPressed = true;
    MarkDirty();
}

void Button::OnMouseUp(int32_t x, int32_t y, uint8_t) {
    if (!isVisible) return;

    if (isPressed) {
        isPressed = false;
        MarkDirty();

        // Emit the click only if the release is still inside the button bounds.
        if (!ContainsCoordinate(x, y)) {
            return;
        }

        EmitClickEvent();
    }
}

void Button::OnMouseMove(int32_t x, int32_t y, int32_t newx, int32_t newy) {
    // newx and newy are parent-relative (window) coordinates; use
    // ContainsCoordinate for accurate hit testing.
    bool inside = this->ContainsCoordinate(newx, newy);

    // Release the press visual if the mouse dragged outside the button.
    if (isPressed && !inside) {
        isPressed = false;
        MarkDirty();
    }

    if (inside != isHovered) {
        isHovered = inside;
        MarkDirty();
    }
}

void Button::SetFontSize(int32_t px) {
    if (!FontManager::activeInstance) return;
    FontSize slot = Font::PixelToFontSlot(px);
    FontType type = this->font ? this->font->fontType : REGULAR;
    Font* newFont = FontManager::activeInstance->getNewFont(slot, type);
    if (!newFont) return;
    delete this->font;
    this->font = newFont;
    MarkDirty();
}

void Button::OnKeyDown(const char* key) {
    if (!enabled || !isVisible) return;
    if (key && (key[0] == '\r' || key[0] == '\n' || key[0] == ' ')) {
        EmitClickEvent();
    }
}

void Button::EmitClickEvent() {
    Event* new_event = new Event{this->ID, ON_CLICK};
    if (!new_event) return;
    if (!Desktop::activeInstance) {
        delete new_event;
        return;
    }
    EventHandler* handler = Desktop::activeInstance->getHandler(this->PID);
    if (!handler) {
        delete new_event;
        return;
    }
    handler->eventQueue.Add(new_event);
    if (g_scheduler && handler->thread) {
        g_scheduler->WakeThread(handler->thread);
    }
}

bool Button::IsPressed() const {
    return isPressed;
}
