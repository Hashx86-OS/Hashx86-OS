/**
 * @file        font.cpp
 * @brief       Font (part of #x86 GUI Framework)
 *
 * @date        12/02/2025
 * @version     1.0.0-beta
 */

#define KDBG_COMPONENT "GUI:FONT"
#include <gui/fonts/font.h>
#include <core/filesystem/FileSystem.h>

#define FONT_MAGIC 0x464E5432  // "FNT2"

FontManager* FontManager::activeInstance = nullptr;

FontFile::FontFile(){};

FontFile::~FontFile(){};

Font::Font(FontFile* file, FontSize fontSize, FontType fontType) {
    this->sourceFile = file;
    this->fontSize = fontSize;
    this->fontType = fontType;

    this->atlas_width = 0;
    this->atlas_height = 0;
    this->font_atlas = nullptr;
    this->font_glyphs = nullptr;
    this->font_kernings = nullptr;
    this->font_kerning_count = 0;
    this->firstChar = 32;

    this->update();
}

Font::~Font() {}

uint32_t Font::getStringLength(const char* str) {
    if (!str || !font_glyphs) return 0;
    int32_t length = 0;
    uint32_t prevChar = 0;

    while (*str) {
        uint32_t c = (uint8_t)(*str);

        // Clamp unsupported characters to fallback
        if (c < firstChar || c >= firstChar + glyph_count) {
            c = (firstChar <= '?' && '?' < firstChar + glyph_count) ? '?' : firstChar;
        }

        // Find glyph index using firstChar base
        int glyph_index = c - firstChar;
        if (glyph_index < 0 || glyph_index >= (int)this->glyph_count) break;
        int16_t xadvance = this->font_glyphs[glyph_index * 8 + 7];
        length += xadvance;

        // Apply kerning (if previous char exists)
        if (prevChar && font_kernings && font_kerning_count > 0) {
            for (int k = 0; k < this->font_kerning_count; k++) {
                int16_t first = this->font_kernings[k * 3 + 0];
                int16_t second = this->font_kernings[k * 3 + 1];
                int16_t amount = this->font_kernings[k * 3 + 2];
                if (first == prevChar && second == c) {
                    length += amount;
                    break;  // assume only one kerning entry per pair
                }
            }
        }

        prevChar = c;
        str++;
    }

    return length < 0 ? 0 : (uint32_t)length;
}

uint32_t Font::MeasureString(const char* str) {
    uint32_t w = getStringLength(str);
    uint16_t h = getLineHeight();
    return (w << 16) | h;
}

void Font::setSize(FontSize size) {
    this->fontSize = size;
    this->update();
}

void Font::setType(FontType type) {
    this->fontType = type;
    this->update();
}

void Font::update() {
    if (!this->sourceFile) {
        this->atlas_width = 0;
        this->atlas_height = 0;
        this->font_atlas = nullptr;
        this->font_glyphs = nullptr;
        this->font_kernings = nullptr;
        this->font_kerning_count = 0;
        return;
    }

    // Guard against out-of-bounds indexing: font_data_list[10][4]
    if (this->fontSize > 9 || this->fontType > BOLD_ITALIC) {
        this->atlas_width = 0;
        this->atlas_height = 0;
        this->font_atlas = nullptr;
        this->font_glyphs = nullptr;
        this->font_kernings = nullptr;
        this->font_kerning_count = 0;
        return;
    }

    if (!this->sourceFile->font_data_list[this->fontSize][this->fontType]) {
        this->atlas_width = 0;
        this->atlas_height = 0;
        this->font_atlas = nullptr;
        this->font_glyphs = nullptr;
        this->font_kernings = nullptr;
        this->font_kerning_count = 0;
        return;
    }

    FontData* data = this->sourceFile->font_data_list[this->fontSize][this->fontType];
    this->atlas_width = data->atlas_width;
    this->atlas_height = data->atlas_height;
    this->font_atlas = data->atlas;
    this->font_glyphs = data->glyphs;
    this->font_kernings = data->kernings;
    this->font_kerning_count = data->kerning_count;
    this->glyph_count = data->glyph_count;
    this->firstChar = data->firstChar;
}

uint16_t Font::getLineHeight() {
    if (!this->sourceFile || !this->font_glyphs) return 0;
    if (this->fontSize > 9 || this->fontType > BOLD_ITALIC) return 0;
    FontData* data = this->sourceFile->font_data_list[this->fontSize][this->fontType];
    if (!data) return 0;
    int maxH = 0;
    for (int i = 0; i < data->glyph_count; i++) {
        int16_t* g = &this->font_glyphs[i * 8];
        int h = g[4] + g[6];  // height + yoffset
        if (h > maxH) maxH = h;
    }
    return maxH;
}

// -----------------------------
// Font Manager
// -----------------------------
FontManager::FontManager() {
    activeInstance = this;
    font_list = new LinkedList<FontFile*>();
    if (!font_list) {
        HALT("CRITICAL: Failed to allocate font list!\n");
    }
}

FontManager::~FontManager() {}

void FontManager::LoadFile(uint32_t mod_start, uint32_t mod_end) {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(mod_start);
    uint8_t* end = reinterpret_cast<uint8_t*>(mod_end);

    if (end <= ptr) {
        KDBG1("Error: Invalid font module bounds");
        return;
    }

    auto has_bytes = [&](size_t n) -> bool { return (size_t)(end - ptr) >= n; };

    // ---- Main header ----
    if (!has_bytes(8)) {
        KDBG1("Error: Font header too small");
        return;
    }

    uint32_t magic = *(uint32_t*)ptr;
    ptr += 4;
    uint16_t version = *(uint16_t*)ptr;
    ptr += 2;
    uint16_t font_count = *(uint16_t*)ptr;
    ptr += 2;

    if (magic != FONT_MAGIC) {
        KDBG1("Error: Invalid font magic");
        return;
    }

    FontFile* new_font_file = new FontFile();
    if (!new_font_file) {
        HALT("CRITICAL: Failed to allocate font file!\n");
    }

    for (int s = 0; s < 10; s++) {
        for (int t = 0; t < 4; t++) {
            new_font_file->font_data_list[s][t] = nullptr;
        }
    }

    auto cleanup_font_file = [&]() {
        for (int s = 0; s < 10; s++) {
            for (int t = 0; t < 4; t++) {
                FontData* f = new_font_file->font_data_list[s][t];
                if (!f) continue;
                delete[] f->atlas;
                delete[] f->glyphs;
                delete[] f->kernings;
                delete f;
                new_font_file->font_data_list[s][t] = nullptr;
            }
        }
        delete new_font_file;
    };

    (void)version;
    const uint16_t max_fonts = 64;
    if (font_count == 0 || font_count > max_fonts) {
        KDBG1("Error: Invalid font count %d", font_count);
        cleanup_font_file();
        return;
    }

    for (int i = 0; i < font_count; i++) {
        // ---- Per-font header ----
        if (!has_bytes(11)) {
            KDBG1("Error: Font entry header truncated at entry %d", i);
            break;  // Can't parse further entries without header
        }

        uint16_t size = *(uint16_t*)ptr;
        ptr += 2;
        uint8_t style = *(uint8_t*)ptr;
        ptr += 1;
        uint16_t atlas_width = *(uint16_t*)ptr;
        ptr += 2;
        uint16_t atlas_height = *(uint16_t*)ptr;
        ptr += 2;
        uint16_t glyph_count = *(uint16_t*)ptr;
        ptr += 2;
        uint16_t kerning_count = *(uint16_t*)ptr;
        ptr += 2;

        const uint16_t max_atlas_dim = 4096;
        const uint16_t max_glyphs = 2048;
        const uint16_t max_kernings = 8192;

        // Compute data sizes first so we can skip past the entry on validation failure
        uint64_t atlas_elems = (uint64_t)atlas_width * (uint64_t)atlas_height;
        uint64_t glyph_elems = (uint64_t)glyph_count * 8u;
        uint64_t kerning_elems = (uint64_t)kerning_count * 3u;
        size_t atlas_bytes = (size_t)atlas_elems * sizeof(uint32_t);
        size_t glyph_bytes = (size_t)glyph_elems * sizeof(int16_t);
        size_t kerning_bytes = (size_t)kerning_elems * sizeof(int16_t);
        size_t entry_total = atlas_bytes + glyph_bytes + kerning_bytes;

        // Validate index bounds — skip entry if it won't fit in font_data_list[10][4]
        if (size >= 10 || style >= 4) {
            KDBG1("Warning: Skipping font entry %d (size=%d style=%d) — out of array bounds", i,
                  size, style);
            // Advance ptr past this entry's data so we can parse the next one
            if (has_bytes(entry_total)) {
                ptr += entry_total;
                continue;
            }
            break;
        }

        // Validate dimensions
        if (atlas_width == 0 || atlas_height == 0 || atlas_width > max_atlas_dim ||
            atlas_height > max_atlas_dim) {
            KDBG1("Warning: Skipping font entry %d — invalid atlas size %dx%d", i, atlas_width,
                  atlas_height);
            if (has_bytes(entry_total)) {
                ptr += entry_total;
                continue;
            }
            break;
        }
        if (glyph_count == 0 || glyph_count > max_glyphs) {
            KDBG1("Warning: Skipping font entry %d — invalid glyph count %d", i, glyph_count);
            if (has_bytes(entry_total)) {
                ptr += entry_total;
                continue;
            }
            break;
        }
        if (kerning_count > max_kernings) {
            KDBG1("Warning: Skipping font entry %d — invalid kerning count %d", i, kerning_count);
            if (has_bytes(entry_total)) {
                ptr += entry_total;
                continue;
            }
            break;
        }

        // Overflow checks
        if (atlas_elems > (uint64_t)(0xFFFFFFFFu / sizeof(uint32_t))) {
            KDBG1("Warning: Skipping font entry %d — atlas overflow", i);
            if (has_bytes(entry_total)) {
                ptr += entry_total;
                continue;
            }
            break;
        }
        if (glyph_elems > (uint64_t)(0xFFFFFFFFu / sizeof(int16_t)) ||
            kerning_elems > (uint64_t)(0xFFFFFFFFu / sizeof(int16_t))) {
            KDBG1("Warning: Skipping font entry %d — glyph/kerning overflow", i);
            if (has_bytes(entry_total)) {
                ptr += entry_total;
                continue;
            }
            break;
        }

        // Bounds check against remaining buffer
        if (!has_bytes(entry_total)) {
            KDBG1("Error: Font entry %d data exceeds module bounds", i);
            break;  // Can't recover — remaining data is truncated
        }

        FontData* new_font = new FontData{};
        if (!new_font) {
            HALT("CRITICAL: Failed to allocate font data!\n");
        }
        new_font->magic = magic;
        new_font->size = size;
        new_font->style = style;
        new_font->atlas_width = atlas_width;
        new_font->atlas_height = atlas_height;
        new_font->glyph_count = glyph_count;
        new_font->kerning_count = kerning_count;
        new_font->firstChar = 32;  // FNT2 format uses ASCII range by default

        // ---- Atlas ----
        size_t atlasSize = (size_t)atlas_elems;
        new_font->atlas = new uint32_t[atlasSize];
        if (!new_font->atlas) {
            HALT("CRITICAL: Failed to allocate font atlas!\n");
        }
        memcpy(new_font->atlas, ptr, atlas_bytes);
        ptr += atlas_bytes;

        // ---- Glyphs ----
        size_t glyphSize = (size_t)glyph_elems;
        new_font->glyphs = new int16_t[glyphSize];
        if (!new_font->glyphs) {
            HALT("CRITICAL: Failed to allocate font glyphs!\n");
        }
        memcpy(new_font->glyphs, ptr, glyph_bytes);
        ptr += glyph_bytes;

        // ---- Kernings ----
        size_t kerningSize = (size_t)kerning_elems;
        new_font->kernings = new int16_t[kerningSize];
        if (!new_font->kernings) {
            HALT("CRITICAL: Failed to allocate font kernings!\n");
        }
        memcpy(new_font->kernings, ptr, kerning_bytes);
        ptr += kerning_bytes;

        new_font_file->font_data_list[size][style] = new_font;

        KDBG1("Font loaded: size=%d, style=%d, glyphs=%d, kernings=%d", size, style, glyph_count,
              kerning_count);
    }

    font_list->Add(new_font_file);
}

void FontManager::LoadFile(File* file, FontType style, const char* ttfPath,
                           int firstChar, int numChars) {
    if (!file || file->size == 0) {
        KDBG1("Font error: file is null or empty");
        return;
    }

    uint8_t* buffer = new uint8_t[file->size];
    if (!buffer) {
        HALT("CRITICAL: Failed to allocate TTF file buffer!\n");
    }

    file->Seek(0);
    uint32_t bytesRead = 0;
    while (bytesRead < file->size) {
        int32_t result = file->Read(buffer + bytesRead, file->size - bytesRead);
        if (result <= 0) {
            KDBG1("Font error: failed to read file (got %d bytes)", result);
            delete[] buffer;
            return;
        }
        bytesRead += (uint32_t)result;
    }

    FontFile* new_font_file = new FontFile();
    if (!new_font_file) {
        HALT("CRITICAL: Failed to allocate font file!\n");
    }
    for (int s = 0; s < 10; s++) {
        for (int t = 0; t < 4; t++) {
            new_font_file->font_data_list[s][t] = nullptr;
        }
    }
    new_font_file->firstChar = firstChar;
    new_font_file->numChars = numChars;

    // Store source path so on-demand loading can find the variant files
    if (ttfPath) {
        size_t len = strlen(ttfPath);
        if (len >= sizeof(new_font_file->filePath)) len = sizeof(new_font_file->filePath) - 1;
        memcpy(new_font_file->filePath, ttfPath, len);
        new_font_file->filePath[len] = '\0';
    }

    // Rasterize each size slot (TINY..XLARGE) for the requested style
    for (int slot = 0; slot < 5; slot++) {
        FontData* data = new FontData{};
        if (!data) {
            HALT("CRITICAL: Failed to allocate FontData!\n");
        }
        if (TTF_RasterizeFont(buffer, file->size, slot, (int)style, data,
                              firstChar, numChars)) {
            new_font_file->font_data_list[slot][(int)style] = data;
            KDBG1("TTF font loaded: slot=%d, style=%d, atlas=%dx%d, glyphs=%d, kernings=%d",
                   slot, (int)style, data->atlas_width, data->atlas_height,
                   data->glyph_count, data->kerning_count);
        } else {
            delete data;
        }
    }

    delete[] buffer;
    font_list->PushBack(new_font_file);
}

bool FontManager::LazyLoadStyle(FontFile* ff, FontType style) {
    if (style <= REGULAR || style > BOLD_ITALIC || !ff || ff->filePath[0] == '\0') return false;

    // Already loaded? (all sizes populated for this style)
    bool anyLoaded = false;
    for (int s = 0; s < 5; s++) {
        if (ff->font_data_list[s][style]) anyLoaded = true;
    }
    if (anyLoaded) return true;

    // Construct variant path
    char variantPath[128];
    const char* regularSuffix = "-Regular.ttf";
    size_t pathLen = strlen(ff->filePath);
    size_t suffixLen = strlen(regularSuffix);
    bool isRegularConvention = (pathLen >= suffixLen &&
        memcmp(ff->filePath + pathLen - suffixLen, regularSuffix, suffixLen) == 0);

    if (isRegularConvention) {
        // Cascadia-style: replace "-Regular" with "-Bold"/"-Italic"/"-BoldItalic"
        if (pathLen + 6 >= sizeof(variantPath)) return false;
        const char* variantSuffixes[] = {"-Bold", "-Italic", "-BoldItalic"};
        size_t baseLen = pathLen - suffixLen;  // length before "-Regular.ttf"
        memcpy(variantPath, ff->filePath, baseLen);
        strcpy(variantPath + baseLen, variantSuffixes[style - 1]);
        strcpy(variantPath + baseLen + strlen(variantSuffixes[style - 1]), ".ttf");
    } else {
        // Segoe-style: insert 'b'/'i'/'z' before ".ttf"
        const char* dot = strrchr(ff->filePath, '.');
        if (!dot || dot == ff->filePath) return false;
        size_t baseLen = (size_t)(dot - ff->filePath);
        // Need space for: base + 1 char + extension + null
        if (baseLen + 1 + strlen(dot) >= sizeof(variantPath)) return false;
        memcpy(variantPath, ff->filePath, baseLen);
        variantPath[baseLen] = "biz"[style - 1];  // BOLD=1→b, ITALIC=2→i, BOLD_ITALIC=3→z
        strcpy(variantPath + baseLen + 1, dot);
    }

    // Open the variant file
    extern FileSystem* g_bootPartition;
    if (!g_bootPartition) return false;
    File* f = g_bootPartition->Open(variantPath);
    if (!f || f->size == 0) {
        if (f) { f->Close(); delete f; }
        KDBG1("LazyLoad: %s not found", variantPath);
        return false;
    }

    // Read entire TTF into buffer
    uint32_t fileSz = f->size;
    uint8_t* buffer = new uint8_t[fileSz];
    if (!buffer) { f->Close(); delete f; return false; }
    f->Seek(0);
    uint32_t totalRead = 0;
    while (totalRead < fileSz) {
        int32_t r = f->Read(buffer + totalRead, fileSz - totalRead);
        if (r <= 0) break;
        totalRead += (uint32_t)r;
    }
    f->Close();
    delete f;

    if (totalRead != fileSz) {
        delete[] buffer;
        return false;
    }

    // Rasterize all 5 sizes into this FontFile's style slot
    bool anyRasterized = false;
    for (int slot = 0; slot < 5; slot++) {
        FontData* data = new FontData{};
        if (TTF_RasterizeFont(buffer, fileSz, slot, (int)style, data)) {
            ff->font_data_list[slot][(int)style] = data;
            anyRasterized = true;
            KDBG1("LazyLoad: slot=%d style=%d from %s", slot, (int)style, variantPath);
        } else {
            delete data;
        }
    }

    delete[] buffer;
    return anyRasterized;
}

Font* FontManager::getNewFont(FontSize size, FontType type) {
    if ((uint32_t)size > 9 || (uint32_t)type > BOLD_ITALIC) return nullptr;

    // First pass: check if already loaded
    auto it = font_list->begin();
    auto end = font_list->end();
    while (it != end) {
        FontFile* ff = *it;
        if (ff && ff->font_data_list[size][type]) {
            Font* sysFont = new Font(ff, size, type);
            if (!sysFont) {
                HALT("CRITICAL: Failed to allocate Font object!\n");
            }
            return sysFont;
        }
        ++it;
    }

    // Second pass: try lazy-load missing style for each font family
    if (type != REGULAR) {
        it = font_list->begin();
        while (it != end) {
            FontFile* ff = *it;
            // Only attempt if this FontFile has REGULAR data (valid family) and a known path
            if (ff && ff->filePath[0] && ff->font_data_list[size][REGULAR]) {
                if (LazyLoadStyle(ff, type) && ff->font_data_list[size][type]) {
                    Font* sysFont = new Font(ff, size, type);
                    if (!sysFont) {
                        HALT("CRITICAL: Failed to allocate Font object!\n");
                    }
                    return sysFont;
                }
            }
            ++it;
        }
    }

    return nullptr;
}

Font* FontManager::getFontByIndex(uint32_t index, FontSize size, FontType type) {
    if ((uint32_t)size > 9 || (uint32_t)type > BOLD_ITALIC) return nullptr;
    uint32_t i = 0;
    auto it = font_list->begin();
    auto end = font_list->end();
    while (it != end) {
        FontFile* ff = *it;
        if (i == index) {
            if (!ff) return nullptr;
            if (ff->font_data_list[size][type]) {
                return new Font(ff, size, type);
            }
            if (type != REGULAR && ff->filePath[0] && ff->font_data_list[size][REGULAR]) {
                if (LazyLoadStyle(ff, type) && ff->font_data_list[size][type]) {
                    return new Font(ff, size, type);
                }
            }
            return nullptr;
        }
        i++;
        ++it;
    }
    return nullptr;
}

Font* FontManager::getFontByFilePath(const char* path, FontSize size, FontType type) {
    if ((uint32_t)size > 9 || (uint32_t)type > BOLD_ITALIC || !path) return nullptr;
    auto it = font_list->begin();
    auto end = font_list->end();
    while (it != end) {
        FontFile* ff = *it;
        if (ff && strcmp(ff->filePath, path) == 0) {
            if (ff->font_data_list[size][type]) {
                return new Font(ff, size, type);
            }
            if (type != REGULAR && ff->filePath[0] && ff->font_data_list[size][REGULAR]) {
                if (LazyLoadStyle(ff, type) && ff->font_data_list[size][type]) {
                    return new Font(ff, size, type);
                }
            }
            return nullptr;
        }
        ++it;
    }
    return nullptr;
}
