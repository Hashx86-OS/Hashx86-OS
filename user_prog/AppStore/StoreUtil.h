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

#ifndef APPSTORE_STOREUTIL_H
#define APPSTORE_STOREUTIL_H

#include <stdint.h>

/** StoreStrLen() - Length of a NUL-terminated string. */
uint32_t StoreStrLen(const char* text);

/** StoreStrCopy() - Copy @src into @dst, always NUL-terminating on success. */
bool StoreStrCopy(char* dst, uint32_t capacity, const char* src);

/** StoreStrEqual() - Case-sensitive string comparison. */
bool StoreStrEqual(const char* left, const char* right);

/** StoreStrCaseEqual() - ASCII case-insensitive string comparison. */
bool StoreStrCaseEqual(const char* left, const char* right);

/** StoreStrStartsWith() - Case-sensitive prefix test. */
bool StoreStrStartsWith(const char* text, const char* prefix);

/** StoreStrStartsWithCase() - ASCII case-insensitive prefix test. */
bool StoreStrStartsWithCase(const char* text, const char* prefix);

/** StoreFormat() - Small printf-like formatter for %s, %c, %d, %u, %x and %%. */
void StoreFormat(char* dst, uint32_t capacity, const char* format, ...);

/** StoreParseU32() - Parse a decimal unsigned integer, rejecting overflow. */
bool StoreParseU32(const char* text, uint32_t* value);

/** StoreU32ToHex() - Format a 32-bit value as lowercase hexadecimal. */
void StoreU32ToHex(char* dst, uint32_t capacity, uint32_t value);

/** StoreHexEqual() - Compare two hexadecimal strings case-insensitively. */
bool StoreHexEqual(const char* left, const char* right);

/** StoreFileExists() - Test for a regular file and optionally return its size. */
bool StoreFileExists(const char* path, uint32_t* size);

/** StoreDirExists() - Test whether @path currently resolves to a directory. */
bool StoreDirExists(const char* path);

/** StoreEnsureDir() - Create @path if needed; succeeds when it exists as a directory. */
bool StoreEnsureDir(const char* path);

/** StoreReadFile() - Read a whole file into a NUL-terminated heap buffer. */
bool StoreReadFile(const char* path, uint8_t** data, uint32_t* size);

/** StoreWriteAll() - Write the whole buffer, looping over short writes. */
bool StoreWriteAll(int32_t fd, const uint8_t* data, uint32_t length);

#endif  // APPSTORE_STOREUTIL_H
