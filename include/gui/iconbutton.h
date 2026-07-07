#ifndef ICONBUTTON_H
#define ICONBUTTON_H

#include <gui/button.h>

struct IconMapping {
    const char* name;
    uint32_t codepoint;
};

class IconButton : public Button {
public:
    enum IconMode {
        ICON_ONLY,
        ICON_WITH_LABEL,
    };

    IconButton(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h,
               const char* iconName);
    IconButton(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h,
               const char* iconName, const char* label);
    ~IconButton();

    void SetIcon(const char* iconName);
    void SetIconMode(IconMode mode);
    void SetIconFontSize(int32_t px);

    void RedrawToCache() override;
    void SetIconWidth(int32_t w);
    void SetIconHeight(int32_t h);

    static uint32_t LookupIcon(const char* name);

private:
    uint32_t iconCodepoint;
    IconMode iconMode;
    Font* iconFont;

    void init(const char* iconName);
    void calculateMinSize();

    static const IconMapping iconTable[];
    static const int iconTableSize;
};

#endif
