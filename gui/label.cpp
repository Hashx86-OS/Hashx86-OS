/**
 * @file        label.cpp
 * @brief       Label Component (part of #x86 GUI Framework)
 *
 * @date        01/02/2026
 * @version     1.0.0
 */

#define KDBG_COMPONENT "GUI:LABEL"
#include <gui/label.h>

Label::Label(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text)
    : Widget(parent, x, y, w, h) {
    this->isFocussable = false;
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;
    const char* t = text ? text : "";
    this->text = new char[strlen(t) + 1];
    if (!this->text) {
        HALT("CRITICAL: Failed to allocate label text!\n");
    }
    strcpy(this->text, t);
}

Label::~Label() {
    delete[] text;
    if (font) delete font;
    // cache deleted by ~Widget
}

void Label::setText(const char* newText) {
    const char* safeText = newText ? newText : "";
    if (this->text && strcmp(this->text, safeText) == 0) return;

    if (this->text) delete[] this->text;
    this->text = new char[strlen(safeText) + 1];
    if (!this->text) {
        HALT("CRITICAL: Failed to allocate label text!\n");
    }
    strcpy(this->text, safeText);
    if (sizeMode == CONTENT) Recalc();
    MarkDirty();
}

void Label::setSize(FontSize size) {
    if (!this->font || !FontManager::activeInstance) return;
    Font* newFont = FontManager::activeInstance->getNewFont(size, this->font->fontType);
    if (!newFont) return;
    delete this->font;
    this->font = newFont;
    if (sizeMode == CONTENT) Recalc();
    MarkDirty();
}

void Label::setType(FontType type) {
    if (!this->font || !FontManager::activeInstance) return;
    Font* newFont = FontManager::activeInstance->getNewFont((FontSize)this->font->fontSize, type);
    if (!newFont) return;
    delete this->font;
    this->font = newFont;
    if (sizeMode == CONTENT) Recalc();
    MarkDirty();
}

void Label::RedrawToCache() {
    if (!cache) {
        isDirty = false;
        return;
    }

    // Fill background if non-transparent
    if (bgColor != 0 && NINA::activeInstance) {
        NINA::activeInstance->FillRectangle(cache, w, h, 0, 0, w, h, bgColor);
    } else {
        memset(cache, 0, sizeof(uint32_t) * w * h);
    }

    if (NINA::activeInstance && font && text) {
        int textW = font->getStringLength(text);
        int textH = font->getLineHeight();

        int xOff = 2;
        int yOff = 2;

        switch (hAlign) {
            case CENTER: xOff = (w - textW) / 2; break;
            case RIGHT:  xOff = w - textW - 2; break;
            default: break;
        }
        switch (vAlign) {
            case MIDDLE: yOff = (h - textH) / 2; break;
            case BOTTOM: yOff = h - textH - 2; break;
            default: break;
        }

        NINA::activeInstance->DrawString(cache, w, h, xOff, yOff, text, font, textColor);
    }
    isDirty = false;
}

void Label::Recalc() {
    int32_t newW = w, newH = h;
    switch (sizeMode) {
        case CONTENT:
            if (font && text) {
                newW = font->getStringLength(text) + (int32_t)padding.l + (int32_t)padding.r;
                newH = (int32_t)font->getLineHeight() + (int32_t)padding.t + (int32_t)padding.b;
            }
            if (newW < (int32_t)minWidth) newW = (int32_t)minWidth;
            if (newH < (int32_t)minHeight) newH = (int32_t)minHeight;
            if (newW != w || newH != h) {
                w = newW;
                h = newH;
                if (cache) delete[] cache;
                cache = nullptr;
                if (w > 0 && h > 0) {
                    size_t count = (size_t)w * (size_t)h;
                    if (count / (size_t)w != (size_t)h || count > (0xFFFFFFFFu / sizeof(uint32_t))) {
                        HALT("CRITICAL: Label dimensions overflow in Recalc!\n");
                    }
                    cache = new uint32_t[count]();
                    if (!cache) {
                        HALT("CRITICAL: Failed to allocate label cache in Recalc!\n");
                    }
                }
            }
            break;
        case FILL:
            if (parent) {
                w = parent->w - padding.l - padding.r;
                h = parent->h - padding.t - padding.b;
            }
            if ((int32_t)minWidth > 0 && w < (int32_t)minWidth) w = (int32_t)minWidth;
            if ((int32_t)minHeight > 0 && h < (int32_t)minHeight) h = (int32_t)minHeight;
            if (newW != w || newH != h) {
                if (cache) delete[] cache;
                cache = nullptr;
                if (w > 0 && h > 0) {
                    size_t count = (size_t)w * (size_t)h;
                    if (count / (size_t)w != (size_t)h || count > (0xFFFFFFFFu / sizeof(uint32_t))) {
                        HALT("CRITICAL: Label dimensions overflow in Recalc!\n");
                    }
                    cache = new uint32_t[count]();
                    if (!cache) {
                        HALT("CRITICAL: Failed to allocate label cache in Recalc!\n");
                    }
                }
            }
            break;
        case FIXED:
        default:
            break;
    }
}

void Label::setFontSize(int32_t px) {
    if (!FontManager::activeInstance) return;
    FontSize slot = Font::PixelToFontSlot(px);
    FontType type = this->font ? this->font->fontType : REGULAR;
    Font* newFont = FontManager::activeInstance->getNewFont(slot, type);
    if (!newFont) return;
    delete this->font;
    this->font = newFont;
    this->fontSize = px;
    if (sizeMode == CONTENT) Recalc();
    MarkDirty();
}

void Label::setColor(uint32_t argb) {
    this->textColor = argb;
    MarkDirty();
}

void Label::setBackground(uint32_t argb) {
    this->bgColor = argb;
    MarkDirty();
}

void Label::setAlignment(HAlign ha, VAlign va) {
    this->hAlign = ha;
    this->vAlign = va;
    MarkDirty();
}

void Label::update() {
    MarkDirty();
}
