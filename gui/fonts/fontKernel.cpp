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

#define KDBG_COMPONENT "GUI:FONT.K"
#include <core/filesystem/FileSystem.h>
#include <core/globals.h>
#include <debug.h>
#include <gui/fonts/fontKernel.h>

namespace {
constexpr size_t MAX_FONT_BYTES = 10 * 1024 * 1024;
}

bool KernelLoadFontFile(const char* path, uint8_t** outData, size_t* outLen) {
    if (!path || !outData || !outLen) return false;
    *outData = nullptr;
    *outLen = 0;

    if (!g_bootPartition) return false;

    File* f = g_bootPartition->Open(path);
    if (!f || f->size == 0) {
        if (f) {
            f->Close();
            delete f;
        }
        return false;
    }

    size_t sz = f->size;
    if (sz > MAX_FONT_BYTES) {
        f->Close();
        delete f;
        return false;
    }

    uint8_t* buf = new uint8_t[sz];
    if (!buf) {
        f->Close();
        delete f;
        return false;
    }

    f->Seek(0);
    size_t total = 0;
    while (total < sz) {
        int32_t r = f->Read(buf + total, (uint32_t)(sz - total));
        if (r <= 0) break;
        total += (size_t)r;
    }
    f->Close();
    delete f;

    if (total != sz) {
        delete[] buf;
        return false;
    }

    *outData = buf;
    *outLen = sz;
    return true;
}

void LoadFontFromFile(FontManager* fm, File* file, FontType style, const char* ttfPath,
                      int firstChar, int numChars) {
    if (!fm || !file || file->size == 0) return;

    size_t sz = file->size;
    if (sz > MAX_FONT_BYTES) {
        KDBG1("Font error: file too large (%u bytes)", (uint32_t)sz);
        return;
    }

    uint8_t* buf = new uint8_t[sz];
    if (!buf) {
        KDBG1("Font error: allocation of %u bytes failed", (uint32_t)sz);
        return;
    }

    file->Seek(0);
    size_t total = 0;
    while (total < sz) {
        int32_t r = file->Read(buf + total, (uint32_t)(sz - total));
        if (r <= 0) break;
        total += (size_t)r;
    }

    if (total != sz) {
        delete[] buf;
        KDBG1("Font error: short read (%u of %u bytes)", (uint32_t)total, (uint32_t)sz);
        return;
    }

    fm->LoadMemory(buf, sz, style, ttfPath, firstChar, numChars);
    delete[] buf;
}
