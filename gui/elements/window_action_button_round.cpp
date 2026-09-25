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

#define KDBG_COMPONENT "GUI:WINDOW.ACBTN.RN"
#include <gui/elements/window_action_button_round.h>

ACRButton::ACRButton(Widget* parent, int32_t x, int32_t y, const char* label)
    : ACButton(parent, x, y, label) {
    // Set a specific font for window controls.
    this->font = FontManager::activeInstance ? FontManager::activeInstance->getNewFont() : nullptr;
    if (this->font) this->font->setSize(SMALL);

    // Calculate the square/circle dimensions.
    int32_t textW = this->font ? this->font->getStringLength(label) : 0;
    int32_t textH = this->font ? this->font->getLineHeight() : 0;

    // Make it a square box that fits the text.
    int32_t diameter = (textW > textH) ? textW : textH;
    if (diameter == 0) diameter = 8;
    diameter += 4;  // Padding.

    this->w = diameter;
    this->h = diameter;

    // Allocate the cache immediately.
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
    // Clear the background (transparent).
    memset(cache, 0, sizeof(uint32_t) * w * h);

    int32_t radius = w / 2;

    // Fill the background.
    uint32_t bgColor = isPressed ? WINDOW_CLOSE_BTN_BG_PRESSED : WINDOW_CLOSE_BTN_BG_NORMAL;

    NINA::activeInstance->FillCircle(cache, w, h, radius, radius, radius, bgColor);

    // Draw the border.
    uint32_t borderColor =
        isPressed ? WINDOW_CLOSE_BTN_BORDER_PRESSED : WINDOW_CLOSE_BTN_BORDER_NORMAL;

    NINA::activeInstance->DrawCircle(cache, w, h, radius, radius, radius, borderColor);

    // Draw the centered text.
    if (this->font) {
        int32_t textW = font->getStringLength(label);
        int32_t textH = font->getLineHeight();

        int32_t textX = (w - textW) / 2;
        int32_t textY = (h - textH) / 2;

        uint32_t textColor = isPressed ? BUTTON_TEXT_PRESSED : BUTTON_TEXT_NORMAL;

        NINA::activeInstance->DrawString(cache, w, h, textX, textY - 2, label, this->font,
                                         textColor);
    }

    isDirty = false;
}
