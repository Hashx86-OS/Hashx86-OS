#ifndef FONT_H
#define FONT_H

#include <core/filesystem/File.h>
#include <core/memory.h>
#include <debug.h>
#include <types.h>
#include <utils/linkedList.h>

typedef enum {
    REGULAR = 0x0,
    BOLD = 0x1,
    ITALIC = 0x2,
    BOLD_ITALIC = 0x3,
} FontType;

typedef enum {
    TINY = 0,
    SMALL = 1,
    MEDIUM = 2,
    LARGE = 3,
    XLARGE = 4,
} FontSize;

// -----------------------------
// Font File Data Structure
// -----------------------------
struct FontData {
    uint32_t magic;

    uint16_t size;  // font size (px)
    uint8_t style;  // 0=normal, 1=bold, 2=italic
    uint16_t atlas_width;
    uint16_t atlas_height;
    uint16_t glyph_count;
    uint16_t kerning_count;

    uint32_t firstChar;  // starting codepoint for glyph array indexing

    uint32_t* atlas;    // atlas ARGB pixels
    int16_t* glyphs;    // glyph metrics (glyph_count * 8)
    int16_t* kernings;  // kerning pairs (kerning_count * 3)
};

// -----------------------------
// Font File Class
// -----------------------------
class FontFile {
    friend class Font;
    friend class FontManager;

private:
    FontData* font_data_list[10][4];  // size, type - 0=Normal, 1=Bold, 2=Italic, 3=BoldItalic
public:
    FontFile();
    ~FontFile();
    char filePath[128] = {};  // source TTF path (empty if loaded from archive)
    int firstChar = 32;  // starting codepoint for this font's glyph range
    int numChars = 95;   // number of glyphs loaded
};

// -----------------------------
// Runtime Font Wrapper
// -----------------------------
class Font {
    friend class FontManager;

public:
    Font(FontFile* file, FontSize fontSize, FontType fontType);
    ~Font();

    // ---- Atlas bitmap ----
    uint32_t* font_atlas;
    int atlas_width;
    int atlas_height;

    // ---- Glyph metrics ----
    int16_t* font_glyphs;
    int16_t* font_kernings;
    int font_kerning_count;
    int glyph_count;

    uint8_t fontSize;   // chosen size
    FontType fontType;  // chosen style
    uint32_t firstChar;  // starting codepoint for glyph indexing

    uint32_t getStringLength(const char* str);
    uint32_t MeasureString(const char* str);
    void setType(FontType type);
    void setSize(FontSize size);
    uint16_t getLineHeight();

    static inline FontSize PixelToFontSlot(int32_t px) {
        if (px <= 18) return TINY;
        if (px <= 22) return SMALL;
        if (px <= 27) return MEDIUM;
        if (px <= 34) return LARGE;
        return XLARGE;
    }

private:
    FontFile* sourceFile;  // reference to loaded data
    void update();
};

// -----------------------------
// Manager for All Fonts
// -----------------------------
class FontManager {
public:
    FontManager();
    ~FontManager();
    static FontManager* activeInstance;

    void LoadFile(uint32_t mod_start, uint32_t mod_end);
    void LoadFile(File* file, FontType style, const char* ttfPath = nullptr,
                  int firstChar = 32, int numChars = 95);
    Font* getNewFont(FontSize size = SMALL, FontType type = REGULAR);
    Font* getFontByIndex(uint32_t index, FontSize size = SMALL, FontType type = REGULAR);
    Font* getFontByFilePath(const char* path, FontSize size = SMALL, FontType type = REGULAR);

private:
    LinkedList<FontFile*>* font_list;
    bool LazyLoadStyle(FontFile* ff, FontType style);  // load TTF variant on demand
};

// TTF rasterizer interface — returns true and fills outData on success
struct FontData;
bool TTF_RasterizeFont(const uint8_t* ttfData, uint32_t ttfSize,
                        int sizeSlot, int styleSlot, FontData* outData,
                        int firstChar = 32, int numChars = 95);

#endif  // FONT_H
