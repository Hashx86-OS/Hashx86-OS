/**
 * @file        label.cpp
 * @brief       Label (part of #x86 GUI Framework)
 *
 * @date        10/02/2025
 * @version     1.0.0-beta
 */

#include <Hx86/Hgui/label.h>

Label::Label(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text)
    : Widget(parent, x, y, w, h), text(text) {
    WidgetData data = {parent->ID, x, y, w, h, text};
    this->ID = HguiAPI(LABEL, CREATE, (void*)&data);
}

Label::~Label() {}

bool Label::setText(const char* text) {
    this->text = text;
    WidgetData data = {ID, 0, 0, 0, 0, text};
    return HguiAPI(LABEL, SET_TEXT, (void*)&data);
}

bool Label::setSize(FontSize size) {
    this->fontSize = size;
    WidgetData data = {ID, (int32_t)fontSize};
    return HguiAPI(LABEL, SET_FONT_SIZE, (void*)&data);
}

bool Label::setType(FontType type) {
    WidgetData data = {ID, (int32_t)type};
    return HguiAPI(LABEL, SET_FONT_TYPE, (void*)&data);
}

bool Label::setColor(uint32_t argb) {
    WidgetData data = {ID, (int32_t)argb};
    return HguiAPI(LABEL, SET_COLOR, (void*)&data);
}

// Convert pixel size to FontSize enum (matches Font::PixelToFontSlot)
static FontSize PxToFontSlot(int32_t px) {
    if (px <= 17) return TINY;
    if (px <= 22) return SMALL;
    if (px <= 27) return MEDIUM;
    if (px <= 34) return LARGE;
    return XLARGE;
}

bool Label::setFontSize(int32_t px) {
    WidgetData data = {ID, (int32_t)PxToFontSlot(px)};
    return HguiAPI(LABEL, SET_FONT_SIZE, (void*)&data);
}

bool Label::setAlignment(HAlign ha, VAlign va) {
    WidgetData data = {ID, (int32_t)ha, (int32_t)va};
    return HguiAPI(LABEL, SET_ALIGNMENT, (void*)&data);
}
