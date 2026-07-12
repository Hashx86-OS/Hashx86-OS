#define KDBG_COMPONENT "GUI:ICONBTN"
#include <gui/iconbutton.h>
#include <core/filesystem/Paths.h>

const IconMapping IconButton::iconTable[] = {
    {"fa-user",         0xf007},
    {"fa-home",         0xf015},
    {"fa-search",       0xf002},
    {"fa-cog",          0xf013},
    {"fa-gear",         0xf013},
    {"fa-envelope",     0xf0e0},
    {"fa-star",         0xf005},
    {"fa-heart",        0xf004},
    {"fa-plus",         0x2b},
    {"fa-edit",         0xf044},
    {"fa-pencil",       0xf303},
    {"fa-trash",        0xf2ed},
    {"fa-folder",       0xf07b},
    {"fa-folder-open",  0xf07c},
    {"fa-file",         0xf15b},
    {"fa-download",     0xf019},
    {"fa-upload",       0xf093},
    {"fa-save",         0xf0c7},
    {"fa-print",        0xf02f},
    {"fa-play",         0xf04b},
    {"fa-pause",        0xf04c},
    {"fa-stop",         0xf04d},
    {"fa-close",        0xf00d},
    {"fa-times",        0xf00d},
    {"fa-check",        0xf00c},
    {"fa-arrow-left",   0xf060},
    {"fa-arrow-right",  0xf061},
    {"fa-arrow-up",     0xf062},
    {"fa-arrow-down",   0xf063},
    {"fa-power-off",    0xf011},
    {"fa-info",         0xf129},
    {"fa-warning",      0xf071},
    {"fa-exclamation",  0xf12a},
    {"fa-bars",         0xf0c9},
    {"fa-menu",         0xf0c9},
    {"fa-clock",        0xf017},
    {"fa-calendar",     0xf073},
    {"fa-camera",       0xf030},
    {"fa-image",        0xf03e},
    {"fa-music",        0xf001},
    {"fa-video",        0xf03d},
    {"fa-lock",         0xf023},
    {"fa-unlock",       0xf09c},
    {"fa-globe",        0xf0ac},
    {"fa-wifi",         0xf1eb},
    {"fa-bluetooth",    0xf293},
    {"fa-bell",         0xf0f3},
    {"fa-book",         0xf02d},
    {"fa-code",         0xf121},
    {"fa-database",     0xf1c0},
    {"fa-terminal",     0xf120},
    {"fa-key",          0xf084},
    {"fa-flag",         0xf024},
    {"fa-filter",       0xf0b0},
    {"fa-refresh",      0xf021},
    {"fa-sync",         0xf021},
    {"fa-share",        0xf064},
    {"fa-tag",          0xf02b},
    {"fa-tags",         0xf02c},
    {"fa-thumbs-up",    0xf164},
    {"fa-thumbs-down",  0xf165},
    {"fa-user-plus",    0xf234},
    {"fa-users",        0xf0c0},
    {"fa-wrench",       0xf0ad},
    {"fa-zoom-in",      0xf00e},
    {"fa-zoom-out",     0xf010},
    {"fa-copy",         0xf0c5},
    {"fa-cut",          0xf0c4},
    {"fa-paste",        0xf0ea},
    {"fa-undo",         0xf0e2},
    {"fa-redo",         0xf01e},
    {"fa-forward",      0xf04e},
    {"fa-backward",     0xf04a},
    {"fa-step-forward", 0xf051},
    {"fa-step-backward",0xf048},
    {"fa-fast-forward", 0xf050},
    {"fa-fast-backward",0xf049},
    {"fa-eject",        0xf052},
    {"fa-random",       0xf074},
    {"fa-volume-up",    0xf028},
    {"fa-volume-down",  0xf027},
    {"fa-volume-off",   0xf026},
    {"fa-mute",         0xf6a9},
    {"fa-microphone",   0xf130},
    {"fa-map-marker",   0xf041},
    {"fa-phone",        0xf095},
    {"fa-comment",      0xf075},
    {"fa-comments",     0xf086},
    {"fa-question",     0xf128},
    {"fa-cart-plus",    0xf217},
    {"fa-shopping-cart",0xf07a},
    {"fa-bolt",         0xf0e7},
    {"fa-fire",         0xf06d},
    {"fa-moon",         0xf186},
    {"fa-sun",          0xf185},
    {"fa-cloud",        0xf0c2},
    {"fa-rain",         0xf73d},
    {"fa-snowflake",    0xf2dc},
    {"fa-palette",      0xf53f},
    {"fa-crown",        0xf521},
    {"fa-gem",          0xf3a5},
    {"fa-rocket",       0xf135},
    {"fa-plane",        0xf072},
    {"fa-car",          0xf1b9},
    {"fa-bus",          0xf207},
    {"fa-train",        0xf238},
    {"fa-bicycle",      0xf206},
    {"fa-anchor",       0xf13d},
    {"fa-leaf",         0xf06c},
    {"fa-eye",          0xf06e},
    {"fa-eye-slash",    0xf070},
    {"fa-lightbulb",    0xf0eb},
    {"fa-plug",         0xf1e6},
    {"fa-battery",      0xf240},
    {"fa-tint",         0xf043},
    {"fa-road",         0xf018},
    {"fa-list",         0xf03a},
    {"fa-align-left",   0xf036},
    {"fa-align-center", 0xf037},
    {"fa-align-right",  0xf038},
    {"fa-bold",         0xf032},
    {"fa-italic",       0xf033},
    {"fa-underline",    0xf0cd},
    {"fa-link",         0xf0c1},
    {"fa-paperclip",    0xf0c6},
    {"fa-table",        0xf0ce},
    {"fa-columns",      0xf0db},
    {"fa-chart-bar",    0xf080},
    {"fa-chart-line",   0xf201},
    {"fa-chart-pie",    0xf200},
    {"fa-at",           0x40},
    {"fa-hashtag",      0x23},
    {"fa-dollar",       0x24},
    {"fa-euro",         0xf153},
    {"fa-pound",        0xf154},
    {"fa-yen",          0xf157},
    {"fa-bitcoin",      0xf379},
    {"fa-gamepad",      0xf11b},
    {"fa-headphones",   0xf025},
    {"fa-trophy",       0xf091},
    {"fa-medal",        0xf5a0},
    {"fa-shirt",        0xf553},
};

const int IconButton::iconTableSize = sizeof(iconTable) / sizeof(iconTable[0]);

uint32_t IconButton::LookupIcon(const char* name) {
    if (!name) return 0;
    for (int i = 0; i < iconTableSize; i++) {
        if (strcmp(iconTable[i].name, name) == 0) {
            return iconTable[i].codepoint;
        }
    }
    return 0;
}

static Font* loadIconFont() {
    if (!FontManager::activeInstance) return nullptr;
    Font* f = FontManager::activeInstance->getFontByFilePath(PATH_ICON_FONT, SMALL, REGULAR);
    if (!f) {
        f = FontManager::activeInstance->getFontByFilePath(PATH_ICON_FONT, MEDIUM, REGULAR);
    }
    return f;
}

void IconButton::init(const char* iconName) {
    this->iconCodepoint = LookupIcon(iconName);
    if (this->iconCodepoint == 0 && iconName) {
        KDBG1("Unknown icon: %s", iconName);
    }
    this->iconMode = (label && label[0]) ? ICON_WITH_LABEL : ICON_ONLY;
    this->iconFont = loadIconFont();
    calculateMinSize();
}

IconButton::IconButton(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h,
                       const char* iconName)
    : Button(parent, x, y, w, h, "") {
    init(iconName);
}

IconButton::IconButton(Widget* parent, int32_t x, int32_t y, int32_t w, int32_t h,
                       const char* iconName, const char* labelText)
    : Button(parent, x, y, w, h, labelText ? labelText : "") {
    init(iconName);
}

IconButton::~IconButton() {
    if (iconFont && iconFont != this->font) {
        delete iconFont;
    }
}

void IconButton::SetIcon(const char* iconName) {
    this->iconCodepoint = LookupIcon(iconName);
    if (this->iconCodepoint == 0 && iconName) {
        KDBG1("Unknown icon: %s", iconName);
    }
    calculateMinSize();
    MarkDirty();
}

void IconButton::SetIconMode(IconMode mode) {
    this->iconMode = mode;
    calculateMinSize();
    MarkDirty();
}

void IconButton::SetIconFontSize(int32_t px) {
    if (!FontManager::activeInstance) return;
    FontSize slot = Font::PixelToFontSlot(px);
    Font* newFont = FontManager::activeInstance->getFontByFilePath(PATH_ICON_FONT, slot, REGULAR);
    if (!newFont) return;
    if (iconFont && iconFont != this->font) delete iconFont;
    iconFont = newFont;
    calculateMinSize();
    MarkDirty();
}

void IconButton::calculateMinSize() {
    if (!iconFont) return;
    int iconW = iconFont->getLineHeight();
    int iconH = iconFont->getLineHeight();
    int labelW = (this->font && label) ? (int)this->font->getStringLength(label) : 0;
    int labelH = (this->font) ? (int)this->font->getLineHeight() : 0;

    if (iconMode == ICON_ONLY) {
        minWidth = iconW + 8;
        minHeight = iconH + 8;
    } else {
        int gap = 4;
        minWidth = iconW + gap + labelW + 8;
        minHeight = (iconH > labelH ? iconH : labelH) + 8;
    }
}

static void reallocateCache(uint32_t*& cache, int32_t w, int32_t h) {
    if (cache) delete[] cache;
    cache = nullptr;
    if (w > 0 && h > 0 && (size_t)w * (size_t)h / (size_t)w == (size_t)h) {
        cache = new uint32_t[(size_t)w * (size_t)h]();
    }
}

void IconButton::SetIconWidth(int32_t reqW) {
    int32_t minW = (int32_t)minWidth;
    this->w = (reqW < minW) ? minW : reqW;
    reallocateCache(cache, this->w, this->h);
    MarkDirty();
}

void IconButton::SetIconHeight(int32_t reqH) {
    int32_t minH = (int32_t)minHeight;
    this->h = (reqH < minH) ? minH : reqH;
    reallocateCache(cache, this->w, this->h);
    MarkDirty();
}

void IconButton::RedrawToCache() {
    if (!cache || !NINA::activeInstance) {
        isDirty = false;
        return;
    }

    uint32_t bgColor;
    uint32_t borderColor;
    uint32_t textColor;
    uint32_t iconColor;

    if (!enabled) {
        bgColor = BUTTON_BG_DISABLED;
        borderColor = BUTTON_BORDER_DISABLED;
        textColor = BUTTON_TEXT_DISABLED;
        iconColor = BUTTON_TEXT_DISABLED;
    } else if (isPressed) {
        bgColor = BUTTON_BG_PRESSED;
        borderColor = BUTTON_BORDER_PRESSED;
        textColor = BUTTON_TEXT_PRESSED;
        iconColor = BUTTON_TEXT_PRESSED;
    } else if (isHovered) {
        bgColor = BUTTON_BG_HOVER;
        borderColor = BUTTON_BORDER_NORMAL;
        textColor = BUTTON_TEXT_NORMAL;
        iconColor = BUTTON_TEXT_NORMAL;
    } else {
        bgColor = BUTTON_BG_NORMAL;
        borderColor = BUTTON_BORDER_NORMAL;
        textColor = BUTTON_TEXT_NORMAL;
        iconColor = BUTTON_TEXT_NORMAL;
    }

    NINA::activeInstance->FillRoundedRectangle(cache, w, h, 0, 0, w, h, 3, bgColor);
    NINA::activeInstance->DrawRoundedRectangle(cache, w, h, 0, 0, w, h, 3, borderColor);

    if (iconFont && iconCodepoint != 0) {
        int iconH = iconFont->getLineHeight();
        int iconW = iconH;

        if (iconMode == ICON_ONLY || !label || label[0] == '\0') {
            int iconX = (w - iconW) / 2;
            int iconY = (h - iconH) / 2;
            NINA::activeInstance->DrawCharacter(cache, w, h, iconX, iconY,
                                                iconCodepoint, iconFont, iconColor);
        } else {
            int gap = 4;
            int labelW = this->font ? (int)this->font->getStringLength(label) : 0;
            int totalW = iconW + gap + labelW;
            int startX = (w - totalW) / 2;
            int iconY = (h - iconH) / 2;
            NINA::activeInstance->DrawCharacter(cache, w, h, startX, iconY,
                                                iconCodepoint, iconFont, iconColor);

            if (this->font) {
                int textX = startX + iconW + gap;
                int textY = (h - (int)this->font->getLineHeight()) / 2;
                NINA::activeInstance->DrawString(cache, w, h, textX, textY,
                                                 label, this->font, textColor);
            }
        }
    } else if (this->font && label) {
        int textX = (w - (int)this->font->getStringLength(label)) / 2;
        int textY = (h - (int)this->font->getLineHeight()) / 2;
        NINA::activeInstance->DrawString(cache, w, h, textX, textY, label, this->font, textColor);
    }

    if (isFocused && enabled) {
        NINA::activeInstance->DrawRoundedRectangle(cache, w, h, 1, 1, w - 2, h - 2, 3, BUTTON_BORDER_FOCUS);
    }

    isDirty = false;
}
