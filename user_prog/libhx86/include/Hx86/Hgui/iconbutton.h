#ifndef HICONBUTTON_H
#define HICONBUTTON_H

#include <Hx86/Hgui/button.h>

class IconButton : public Button {
public:
    IconButton(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h,
               const char* iconName);
    IconButton(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h,
               const char* iconName, const char* label);
    ~IconButton();

    bool setIcon(const char* iconName);
    bool setIconFontSize(int32_t px);
};

#endif
