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

#ifndef FONT_H
#define FONT_H

#include <core/filesystem/File.h>
#include <core/memory.h>
#include <debug.h>
#include <types.h>
#include <utils/linkedList.h>

/**
 * enum FontType - Supported font styles.
 */
typedef enum {
    REGULAR = 0x0,
    BOLD = 0x1,
    ITALIC = 0x2,
    BOLD_ITALIC = 0x3,
} FontType;

/**
 * enum FontSize - Predefined glyph size slots.
 */
typedef enum {
    TINY = 0,
    SMALL = 1,
    MEDIUM = 2,
    LARGE = 3,
    XLARGE = 4,
} FontSize;

/**
 * struct FontData - On-disk font file header and glyph payload.
 * @magic: File magic to validate the format.
 * @size: Font size in pixels.
 * @style: Style code: 0=normal, 1=bold, 2=italic.
 * @atlas_width: Atlas bitmap width in pixels.
 * @atlas_height: Atlas bitmap height in pixels.
 * @glyph_count: Number of glyphs in @glyphs.
 * @kerning_count: Number of kerning pairs in @kernings.
 * @firstChar: Starting codepoint for glyph-array indexing.
 * @atlas: Atlas ARGB pixels.
 * @glyphs: Glyph metrics, glyph_count * 8 entries.
 * @kernings: Kerning pairs, kerning_count * 3 entries.
 */
struct FontData {
    uint32_t magic;

    uint16_t size;
    uint8_t style;
    uint16_t atlas_width;
    uint16_t atlas_height;
    uint16_t glyph_count;
    uint16_t kerning_count;

    uint32_t firstChar;

    uint32_t* atlas;
    int16_t* glyphs;
    int16_t* kernings;
};

/**
 * class FontFile - A loaded font file holding up to ten sizes across four styles.
 *
 * Holds the raw FontData blocks (size x style grid) plus the source TTF path.
 * Style variants are lazy-loaded on first use by the FontManager.
 */
class FontFile {
    friend class Font;
    friend class FontManager;

private:
    FontData* font_data_list[10][4];  // size, style: 0=Normal, 1=Bold, 2=Italic, 3=BoldItalic.

public:
    /**
     * FontFile() - Construct an empty font file container.
     */
    FontFile();

    /**
     * ~FontFile() - Free every loaded font data block.
     */
    ~FontFile();

    char filePath[128] = {};  // Source TTF path (empty if loaded from archive).
    int firstChar = 32;       // Starting codepoint for this font's glyph range.
    int numChars = 95;        // Number of glyphs loaded.
};

/**
 * class Font - A runtime wrapper around one loaded font size/style variant.
 *
 * Exposes the atlas bitmap and glyph metrics, and measures string widths for
 * layout. Widgets hold a Font* obtained from the FontManager.
 */
class Font {
    friend class FontManager;

public:
    /**
     * Font() - Construct a font wrapper from a loaded file.
     * @file: Source font file providing the raw data.
     * @fontSize: Size slot to select.
     * @fontType: Style slot to select.
     */
    Font(FontFile* file, FontSize fontSize, FontType fontType);

    /**
     * ~Font() - Destroy the font wrapper.
     */
    ~Font();

    // Atlas bitmap.
    uint32_t* font_atlas;
    int atlas_width;
    int atlas_height;

    // Glyph metrics.
    int16_t* font_glyphs;
    int16_t* font_kernings;
    int font_kerning_count;
    int glyph_count;

    uint8_t fontSize;    // Chosen size slot.
    FontType fontType;   // Chosen style.
    uint32_t firstChar;  // Starting codepoint for glyph indexing.

    /**
     * getStringLength() - Return the rendered width of a string in pixels.
     * @str: NUL-terminated string to measure.
     */
    uint32_t getStringLength(const char* str);

    /**
     * MeasureString() - Return the rendered width of a string in pixels.
     * @str: NUL-terminated string to measure.
     *
     * Alias of getStringLength().
     */
    uint32_t MeasureString(const char* str);

    /**
     * setType() - Switch this font to a different style variant.
     * @type: New style.
     */
    void setType(FontType type);

    /**
     * setSize() - Switch this font to a different size variant.
     * @size: New size slot.
     */
    void setSize(FontSize size);

    /**
     * getLineHeight() - Return the glyph line height in pixels.
     */
    uint16_t getLineHeight();

    /**
     * PixelToFontSlot() - Map a pixel size to the nearest FontSize slot.
     * @px: Font size in pixels.
     *
     * Return: The smallest slot whose upper bound covers @px.
     */
    static inline FontSize PixelToFontSlot(int32_t px) {
        if (px <= 18) return TINY;
        if (px <= 22) return SMALL;
        if (px <= 27) return MEDIUM;
        if (px <= 34) return LARGE;
        return XLARGE;
    }

private:
    FontFile* sourceFile;  // Reference to the loaded data.
    void update();
};

/**
 * class FontManager - Owner of every loaded font and the font-file registry.
 *
 * Loads font files from disk or memory, lazily materializes style variants,
 * and hands out Font wrappers on request.
 */
class FontManager {
public:
    /**
     * FontManager() - Construct the shared font registry.
     */
    FontManager();

    /**
     * ~FontManager() - Free every loaded font file.
     */
    ~FontManager();

    static FontManager* activeInstance;

    /**
     * LoadFile() - Load a font file embedded in a multiboot module.
     * @mod_start: Start address of the font module.
     * @mod_end: End address of the font module.
     */
    void LoadFile(uint32_t mod_start, uint32_t mod_end);

    /**
     * LoadFile() - Load a font file from a File object.
     * @file: Open file positioned at the font payload.
     * @style: Style to register the default variant as.
     * @ttfPath: Source TTF path (empty if loaded from an archive).
     * @firstChar: Starting codepoint of the glyph range.
     * @numChars: Number of glyphs to load.
     */
    void LoadFile(File* file, FontType style, const char* ttfPath = nullptr, int firstChar = 32,
                  int numChars = 95);

    /**
     * getNewFont() - Return a font wrapper for the requested size and style.
     * @size: Size slot to select.
     * @type: Style slot to select.
     *
     * Return: A Font wrapper, or NULL if no font file is loaded yet.
     */
    Font* getNewFont(FontSize size = SMALL, FontType type = REGULAR);

    /**
     * getFontByIndex() - Return a font wrapper by file index.
     * @index: Zero-based index into the loaded file list.
     * @size: Size slot to select.
     * @type: Style slot to select.
     *
     * Return: A Font wrapper, or NULL if the index is out of range.
     */
    Font* getFontByIndex(uint32_t index, FontSize size = SMALL, FontType type = REGULAR);

    /**
     * getFontByFilePath() - Return a font wrapper by file path.
     * @path: Registered font file path.
     * @size: Size slot to select.
     * @type: Style slot to select.
     *
     * Return: A Font wrapper, or NULL if no file matches @path.
     */
    Font* getFontByFilePath(const char* path, FontSize size = SMALL, FontType type = REGULAR);

private:
    LinkedList<FontFile*>* font_list;

    /** LazyLoadStyle() - Materialize a style variant of a file on demand. */
    bool LazyLoadStyle(FontFile* ff, FontType style);
};

/**
 * TTF_RasterizeFont() - Rasterize a TTF body into FontData.
 * @ttfData: TTF file bytes.
 * @ttfSize: Byte length of @ttfData.
 * @sizeSlot: Target FontSize slot.
 * @styleSlot: Target FontType slot.
 * @outData: Receives the rasterized font data.
 * @firstChar: Starting codepoint of the glyph range.
 * @numChars: Number of glyphs to rasterize.
 *
 * Return: True and fills @outData on success.
 */
struct FontData;
bool TTF_RasterizeFont(const uint8_t* ttfData, uint32_t ttfSize, int sizeSlot, int styleSlot,
                       FontData* outData, int firstChar = 32, int numChars = 95);

#endif  // FONT_H
