#ifndef LABEL_H
#define LABEL_H

#include <Hx86/Hgui/widget.h>

enum HAlign { LEFT = 0, CENTER = 1, RIGHT = 2 };
enum VAlign { TOP = 0, MIDDLE = 1, BOTTOM = 2 };

class Label : public Widget {
private:
    const char* text;
    FontSize fontSize;

public:
    Label(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text);
    ~Label();

    bool setText(const char* text);
    bool setSize(FontSize size);
    bool setType(FontType type);
    bool setColor(uint32_t argb);
    bool setFontSize(int32_t px);
    bool setAlignment(HAlign ha, VAlign va);
};

#endif
