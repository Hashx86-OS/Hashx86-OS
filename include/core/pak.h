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

#ifndef PAK_H
#define PAK_H

#include <stdint.h>

#define PAK_MAGIC "PACK"
#define PAK_NAME_LEN 56

/**
 * struct PakHeader - File header of a PAK archive.
 * @magic: Archive magic, must equal PAK_MAGIC ("PACK").
 * @dirOffset: Offset of the directory table, in bytes.
 * @dirSize: Size of the directory table, in bytes.
 */
struct PakHeader {
    char magic[4];
    uint32_t dirOffset;
    uint32_t dirSize;
} __attribute__((packed));

/**
 * struct PakDirEntry - One directory entry of a PAK archive.
 * @name: NUL-terminated entry name (PAK_NAME_LEN bytes).
 * @offset: Offset of the entry data, in bytes.
 * @size: Size of the entry data, in bytes.
 */
struct PakDirEntry {
    char name[PAK_NAME_LEN];
    uint32_t offset;
    uint32_t size;
} __attribute__((packed));

#endif
