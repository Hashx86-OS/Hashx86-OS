#define KDBG_COMPONENT "TTF"
#include <debug.h>
#include <core/memory.h>
#include <stdlib/fdlibm.h>
#include <gui/fonts/font.h>

// Override stb_truetype macros — use fdlibm for math, kernel for memory
static inline int stbtt_ifloor_impl(float x) { int i = (int)x; return (x < 0 && x != (float)i) ? i - 1 : i; }
#define STBTT_ifloor(x)   (stbtt_ifloor_impl(x))
#define STBTT_iceil(x)    ((int)((x) + 0.99999997f))
#define STBTT_sqrt(x)     sqrt(x)
#define STBTT_pow(x,y)    pow(x,y)
#define STBTT_fmod(x,y)   fmod(x,y)
#define STBTT_cos(x)      cos(x)
#define STBTT_acos(x)     acos(x)
#define STBTT_fabs(x)     fabs(x)
#define STBTT_malloc(x,u) (kmalloc(x))
#define STBTT_free(x,u)   (kfree(x))
#define STBTT_assert(x)   ((void)0)
static inline int stbtt_strlen_impl(const char* s) { int n = 0; if (s) while (s[n]) n++; return n; }
#define STBTT_strlen(x)   (stbtt_strlen_impl(x))
#define STBTT_memcpy      memcpy
#define STBTT_memset      memset

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb/stb_truetype.h>

namespace {

constexpr int TTF_FIRST_CHAR = 32;
constexpr int TTF_NUM_CHARS = 95;
constexpr int TTF_PIXEL_SIZES[5] = {16, 20, 24, 30, 38};
constexpr int TTF_MAX_ATLAS_W = 512;
constexpr int TTF_MAX_ATLAS_H = 512;
constexpr int TTF_GLYPH_PAD = 2;

struct RasterizedFont {
    int pixelSize;
    uint8_t* atlasBuf;
    int atlasW;
    int atlasH;
    stbtt_bakedchar chardata[TTF_NUM_CHARS];
    bool valid;

    RasterizedFont() : pixelSize(0), atlasBuf(nullptr), atlasW(0), atlasH(0), valid(false) {
        for (int i = 0; i < TTF_NUM_CHARS; i++) chardata[i] = stbtt_bakedchar{};
    }
};

bool RasterizeSize(stbtt_fontinfo* font, int pixelSize, RasterizedFont* out) {
    if (!font || !out) return false;
    out->pixelSize = pixelSize;
    out->atlasBuf = new uint8_t[TTF_MAX_ATLAS_W * TTF_MAX_ATLAS_H];
    if (!out->atlasBuf) return false;
    memset(out->atlasBuf, 0, TTF_MAX_ATLAS_W * TTF_MAX_ATLAS_H);
    int baked = stbtt_BakeFontBitmap((const unsigned char*)font->data, 0,
                                      (float)pixelSize, out->atlasBuf,
                                      TTF_MAX_ATLAS_W, TTF_MAX_ATLAS_H,
                                      TTF_FIRST_CHAR, TTF_NUM_CHARS, out->chardata);
    if (baked <= 0) { delete[] out->atlasBuf; out->atlasBuf = nullptr; return false; }
    int maxX = 0, maxY = 0;
    for (int i = 0; i < TTF_NUM_CHARS; i++) {
        int x1 = out->chardata[i].x1 + TTF_GLYPH_PAD;
        int y1 = out->chardata[i].y1 + TTF_GLYPH_PAD;
        if (x1 > maxX) maxX = x1;
        if (y1 > maxY) maxY = y1;
    }
    out->atlasW = maxX > 0 ? maxX : 1;
    out->atlasH = maxY > 0 ? maxY : 1;
    if (out->atlasW > TTF_MAX_ATLAS_W) out->atlasW = TTF_MAX_ATLAS_W;
    if (out->atlasH > TTF_MAX_ATLAS_H) out->atlasH = TTF_MAX_ATLAS_H;
    out->valid = true;
    return true;
}

uint32_t* GrayscaleToARGB(const RasterizedFont* src, int* outW, int* outH) {
    if (!src || !src->valid) return nullptr;
    int w = src->atlasW, h = src->atlasH;
    uint32_t* argb = new uint32_t[w * h];
    if (!argb) return nullptr;
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            argb[y * w + x] = ((uint32_t)src->atlasBuf[y * TTF_MAX_ATLAS_W + x] << 24) | 0x00FFFFFF;
    *outW = w; *outH = h;
    return argb;
}

int16_t* BuildGlyphArray(const RasterizedFont* src, int* outCount) {
    if (!src || !src->valid) return nullptr;
    int count = TTF_NUM_CHARS;
    int16_t* glyphs = new int16_t[count * 8];
    if (!glyphs) return nullptr;

    // stb_truetype yoff is relative to baseline (negative = above),
    // but the renderer expects yoffset relative to top-of-line (non-negative).
    // Find the tallest glyph's yoff so we can normalize.
    int minYoff = 0;
    for (int i = 0; i < count; i++)
        if ((int16_t)src->chardata[i].yoff < minYoff)
            minYoff = (int16_t)src->chardata[i].yoff;

    for (int i = 0; i < count; i++) {
        int idx = i * 8;
        int16_t ch = (int16_t)(src->chardata[i].y1 - src->chardata[i].y0);
        int16_t yo = (int16_t)src->chardata[i].yoff - minYoff;
        glyphs[idx + 0] = (int16_t)(TTF_FIRST_CHAR + i);
        glyphs[idx + 1] = (int16_t)src->chardata[i].x0;
        glyphs[idx + 2] = (int16_t)src->chardata[i].y0;
        glyphs[idx + 3] = (int16_t)(src->chardata[i].x1 - src->chardata[i].x0);
        glyphs[idx + 4] = ch;
        glyphs[idx + 5] = (int16_t)src->chardata[i].xoff;
        glyphs[idx + 6] = yo;
        glyphs[idx + 7] = (int16_t)src->chardata[i].xadvance;
    }
    *outCount = count;
    return glyphs;
}

int16_t* BuildKerningArray(stbtt_fontinfo* font, int* outCount) {
    if (!font) return nullptr;
    int count = 0;
    for (int i = 0; i < TTF_NUM_CHARS; i++) {
        int g1 = stbtt_FindGlyphIndex(font, TTF_FIRST_CHAR + i);
        for (int j = 0; j < TTF_NUM_CHARS; j++)
            if (stbtt_GetGlyphKernAdvance(font, g1, stbtt_FindGlyphIndex(font, TTF_FIRST_CHAR + j)) != 0) count++;
    }
    if (count == 0) { *outCount = 0; return nullptr; }
    int16_t* kernings = new int16_t[count * 3];
    if (!kernings) { *outCount = 0; return nullptr; }
    int idx = 0;
    for (int i = 0; i < TTF_NUM_CHARS && idx < count; i++) {
        int g1 = stbtt_FindGlyphIndex(font, TTF_FIRST_CHAR + i);
        for (int j = 0; j < TTF_NUM_CHARS && idx < count; j++) {
            int g2 = stbtt_FindGlyphIndex(font, TTF_FIRST_CHAR + j);
            int kern = stbtt_GetGlyphKernAdvance(font, g1, g2);
            if (kern != 0) {
                kernings[idx * 3 + 0] = (int16_t)(TTF_FIRST_CHAR + i);
                kernings[idx * 3 + 1] = (int16_t)(TTF_FIRST_CHAR + j);
                kernings[idx * 3 + 2] = (int16_t)kern;
                idx++;
            }
        }
    }
    *outCount = idx;
    return kernings;
}

}  // namespace

bool TTF_RasterizeFont(const uint8_t* ttfData, uint32_t ttfSize,
                        int sizeSlot, int styleSlot, FontData* outData) {
    (void)ttfSize;
    if (!ttfData || !outData) return false;
    if (sizeSlot < 0 || sizeSlot > 4) return false;
    if (styleSlot < 0 || styleSlot > 3) return false;

    stbtt_fontinfo font;
    if (!stbtt_InitFont(&font, ttfData, stbtt_GetFontOffsetForIndex(ttfData, 0))) {
        KDBG1("TTF: Failed to init font");
        return false;
    }

    RasterizedFont raster;
    if (!RasterizeSize(&font, TTF_PIXEL_SIZES[sizeSlot], &raster)) {
        KDBG1("TTF: Failed to rasterize %dpx", TTF_PIXEL_SIZES[sizeSlot]);
        return false;
    }

    int atlasW = 0, atlasH = 0;
    uint32_t* argb = GrayscaleToARGB(&raster, &atlasW, &atlasH);
    if (!argb) { delete[] raster.atlasBuf; return false; }

    int glyphCount = 0;
    int16_t* glyphs = BuildGlyphArray(&raster, &glyphCount);
    if (!glyphs) { delete[] argb; delete[] raster.atlasBuf; return false; }

    int kerningCount = 0;
    int16_t* kernings = BuildKerningArray(&font, &kerningCount);

    outData->magic = 0x464E5432;
    outData->size = (uint16_t)sizeSlot;
    outData->style = (uint8_t)styleSlot;
    outData->atlas_width = (uint16_t)atlasW;
    outData->atlas_height = (uint16_t)atlasH;
    outData->glyph_count = (uint16_t)glyphCount;
    outData->kerning_count = (uint16_t)kerningCount;
    outData->atlas = argb;
    outData->glyphs = glyphs;
    outData->kernings = kernings;

    delete[] raster.atlasBuf;
    return true;
}
