/**
 * @file        button.cpp
 * @brief       Button (part of #x86 GUI Framework)
 *
 * @date        11/02/2026
 * @version     1.0.0-beta
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

    // cache is allocated by Widget constructor; do not reallocate here
}

Button::~Button() {
    if (label) delete[] label;
    if (font) delete font;
    // cache is owned and freed by Widget::~Widget
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
        NINA::activeInstance->DrawRoundedRectangle(cache, w, h, 1, 1, w - 2, h - 2, 3, BUTTON_BORDER_FOCUS);
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

        // Only emit click if release is still inside button bounds.
        if (!ContainsCoordinate(x, y)) {
            return;
        }

        Event* new_event = new Event{this->ID, ON_CLICK};
        if (!new_event) {
            HALT("CRITICAL: Failed to allocate button click event!\n");
        }

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
}

void Button::OnMouseMove(int32_t x, int32_t y, int32_t newx, int32_t newy) {
    // Coordinates newx and newy are parent-relative (window coordinates)
    // Use ContainsCoordinate for accurate hit testing
    bool inside = this->ContainsCoordinate(newx, newy);

    // If the mouse dragged OUTSIDE the button, release the press visual
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
        Event* new_event = new Event{this->ID, ON_CLICK};
        if (!new_event) return;
        if (!Desktop::activeInstance) { delete new_event; return; }
        EventHandler* handler = Desktop::activeInstance->getHandler(this->PID);
        if (!handler) { delete new_event; return; }
        handler->eventQueue.Add(new_event);
        if (g_scheduler && handler->thread) {
            g_scheduler->WakeThread(handler->thread);
        }
    }
}

bool Button::IsPressed() const {
    return isPressed;
}
