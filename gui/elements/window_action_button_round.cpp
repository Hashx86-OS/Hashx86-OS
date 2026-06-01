/**
 * @file        window_action_button_round.cpp
 * @brief       Round Action Button (part of #x86 GUI Framework)
 *
 * @date        11/02/2026
 * @version     1.0.0
 */

#define KDBG_COMPONENT "GUI:WINDOW.ACBTN.RN"
#include <gui/elements/window_action_button_round.h>

ACRButton::ACRButton(Widget* parent, int32_t x, int32_t y, const char* label)
    : ACButton(parent, x, y, label) {
    // Set specific font for window controls
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;
    if (this->font) this->font->setSize(SMALL);

    // Calculate Square/Circle dimensions
    int32_t textW = this->font ? this->font->getStringLength(label) : 0;
    int32_t textH = this->font ? this->font->getLineHeight() : 0;

    // Make it a square box that fits the text
    int32_t diameter = (textW > textH) ? textW : textH;
    if (diameter == 0) diameter = 8;
    diameter += 4;  // Padding

    this->w = diameter;
    this->h = diameter;

    // Allocate proper cache immediately
    if (cache) {
        delete[] cache;
        cache = nullptr;
    }
    if (w > 0 && h > 0) {
        cache = new uint32_t[w * h]();
        if (!cache) {
            HALT("CRITICAL: Failed to allocate action button cache!\n");
        }
    }
}

ACRButton::~ACRButton() {}

void ACRButton::RedrawToCache() {
    if (!cache) return;
    if (!NINA::activeInstance) {
        isDirty = false;
        return;
    }
    // Clear Background (Transparent)
    memset(cache, 0, sizeof(uint32_t) * w * h);

    int32_t radius = w / 2;

    // Background Fill
    uint32_t bgColor = isPressed ? WINDOW_CLOSE_BUTTON_BACKGROUND_COLOR_PRESSED
                                 : WINDOW_CLOSE_BUTTON_BACKGROUND_COLOR_NORMAL;

    NINA::activeInstance->FillCircle(cache, w, h, radius, radius, radius, bgColor);

    // Border
    uint32_t borderColor = isPressed ? WINDOW_CLOSE_BUTTON_BORDER_COLOR_PRESSED
                                     : WINDOW_CLOSE_BUTTON_BORDER_COLOR_NORMAL;

    NINA::activeInstance->DrawCircle(cache, w, h, radius, radius, radius, borderColor);

    // Centered Text
    if (this->font) {
        int32_t textW = font->getStringLength(label);
        int32_t textH = font->getLineHeight();

        int32_t textX = (w - textW) / 2;
        int32_t textY = (h - textH) / 2;

        uint32_t textColor = isPressed ? BUTTON_TEXT_COLOR_PRESSED : BUTTON_TEXT_COLOR_NORMAL;

        NINA::activeInstance->DrawString(cache, w, h, textX, textY - 2, label, this->font, textColor);
    }

    isDirty = false;
}
