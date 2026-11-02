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

#ifndef FONT_KERNEL_H
#define FONT_KERNEL_H

#include <core/filesystem/File.h>
#include <gui/fonts/font.h>
#include <types.h>

/**
 * KernelLoadFontFile() - FontFileLoader backed by the boot partition.
 * @path: Font file path to read.
 * @outData: Receives a heap buffer of @outLen bytes; caller frees with delete[].
 * @outLen: Receives the number of bytes read.
 *
 * Return: True on success.
 */
bool KernelLoadFontFile(const char* path, uint8_t** outData, size_t* outLen);

/**
 * LoadFontFromFile() - Read a TTF from an open File and register it.
 * @fm: Font manager to register the loaded font with.
 * @file: Open file positioned anywhere; it is read from the start.
 * @style: Style slot to register the rasterized variants under.
 * @ttfPath: Source path stored for lazy variant loading.
 * @firstChar: Starting codepoint of the glyph range.
 * @numChars: Number of glyphs to rasterize.
 *
 * The caller retains ownership of @file and must close and delete it.
 */
void LoadFontFromFile(FontManager* fm, File* file, FontType style, const char* ttfPath = nullptr,
                      int firstChar = 32, int numChars = 95);

#endif  // FONT_KERNEL_H
