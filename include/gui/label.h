#ifndef LABEL_H
#define LABEL_H

#include <gui/widget.h>
#include <types.h>

enum HAlign { LEFT = 0, CENTER = 1, RIGHT = 2 };
enum VAlign { TOP = 0, MIDDLE = 1, BOTTOM = 2 };

class Label : public Widget {
private:
    char* text;
    uint32_t textColor = LABEL_TEXT_NORMAL;
    uint32_t bgColor = 0;
    HAlign hAlign = LEFT;
    VAlign vAlign = TOP;

public:
    Label(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h, const char* text);
    ~Label();
    void update();
    void setText(const char* text);
    void setSize(FontSize size);
    void setType(FontType type);
    void SetFontSize(int32_t px);
    void SetColor(uint32_t argb);
    void SetBackground(uint32_t argb);
    void SetAlignment(HAlign ha, VAlign va);
    void RedrawToCache() override;
    void Recalc() override;
    bool IsLabel() const override {
        return true;
    }
};

#endif
