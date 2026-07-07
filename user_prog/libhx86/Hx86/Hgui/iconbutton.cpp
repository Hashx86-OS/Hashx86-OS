/**
 * @file        iconbutton.cpp
 * @brief       IconButton (part of #x86 GUI Framework)
 *
 * @date        07/07/2026
 * @version     1.0.0
 */

#include <Hx86/Hgui/iconbutton.h>

IconButton::IconButton(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h,
                       const char* iconName)
    : Button(parent, x, y, w, h, "") {
    WidgetData data = {parent->ID, x, y, w, h, iconName, (char*)"", nullptr};
    this->ID = HguiAPI(ICON_BUTTON, CREATE, (void*)&data);
}

IconButton::IconButton(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h,
                       const char* iconName, const char* labelText)
    : Button(parent, x, y, w, h, labelText ? labelText : "") {
    WidgetData data = {parent->ID, x, y, w, h, iconName, (char*)(labelText ? labelText : ""), nullptr};
    this->ID = HguiAPI(ICON_BUTTON, CREATE, (void*)&data);
}

IconButton::~IconButton() {}

bool IconButton::setIcon(const char* iconName) {
    WidgetData data = {ID, 0, 0, 0, 0, iconName, nullptr, nullptr};
    return HguiAPI(ICON_BUTTON, SET_ICON, (void*)&data);
}

bool IconButton::setIconFontSize(int32_t px) {
    WidgetData data = {ID, px, 0, 0, 0, nullptr, nullptr, nullptr};
    return HguiAPI(ICON_BUTTON, SET_ICON_FONT_SIZE, (void*)&data);
}
