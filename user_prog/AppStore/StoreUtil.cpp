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

#include "StoreUtil.h"

#include <Hx86/Hsyscalls/syscalls.h>
#include <Hx86/memory.h>
#include <stdarg.h>

namespace {

char LowerAscii(char c) {
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

bool IsSpace(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

void AppendChar(char* dst, uint32_t capacity, uint32_t* pos, char c) {
    if (*pos + 1 < capacity) dst[(*pos)++] = c;
    if (capacity != 0) dst[*pos < capacity ? *pos : capacity - 1] = '\0';
}

void AppendText(char* dst, uint32_t capacity, uint32_t* pos, const char* text) {
    if (text == nullptr) return;
    while (*text != '\0') AppendChar(dst, capacity, pos, *text++);
}

void AppendUnsigned(char* dst, uint32_t capacity, uint32_t* pos, uint32_t value, uint32_t base,
                    bool negative) {
    char digits[16];
    uint32_t count = 0;
    if (value == 0) {
        digits[count++] = '0';
    } else {
        while (value != 0 && count < sizeof(digits)) {
            uint32_t digit = value % base;
            digits[count++] = (char)(digit < 10 ? '0' + digit : 'a' + digit - 10);
            value /= base;
        }
    }
    if (negative) AppendChar(dst, capacity, pos, '-');
    while (count != 0) AppendChar(dst, capacity, pos, digits[--count]);
}

}  // namespace

uint32_t StoreStrLen(const char* text) {
    uint32_t length = 0;
    if (text == nullptr) return 0;
    while (text[length] != '\0') length++;
    return length;
}

bool StoreStrCopy(char* dst, uint32_t capacity, const char* src) {
    if (dst == nullptr || capacity == 0) return false;
    uint32_t i = 0;
    if (src != nullptr) {
        while (src[i] != '\0' && i + 1 < capacity) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
    return src == nullptr || src[i] == '\0';
}

bool StoreStrEqual(const char* left, const char* right) {
    if (left == nullptr || right == nullptr) return left == right;
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }
    return *left == *right;
}

bool StoreStrCaseEqual(const char* left, const char* right) {
    if (left == nullptr || right == nullptr) return left == right;
    while (*left != '\0' && LowerAscii(*left) == LowerAscii(*right)) {
        left++;
        right++;
    }
    return LowerAscii(*left) == LowerAscii(*right);
}

bool StoreStrStartsWith(const char* text, const char* prefix) {
    if (text == nullptr || prefix == nullptr) return false;
    while (*prefix != '\0') {
        if (*text != *prefix) return false;
        text++;
        prefix++;
    }
    return true;
}

bool StoreStrStartsWithCase(const char* text, const char* prefix) {
    if (text == nullptr || prefix == nullptr) return false;
    while (*prefix != '\0') {
        if (LowerAscii(*text) != LowerAscii(*prefix)) return false;
        text++;
        prefix++;
    }
    return true;
}

void StoreFormat(char* dst, uint32_t capacity, const char* format, ...) {
    if (dst == nullptr || capacity == 0) return;
    dst[0] = '\0';
    uint32_t pos = 0;
    va_list args;
    va_start(args, format);

    while (*format != '\0') {
        if (*format != '%') {
            AppendChar(dst, capacity, &pos, *format++);
            continue;
        }

        format++;
        if (*format == '%') {
            AppendChar(dst, capacity, &pos, '%');
            format++;
            continue;
        }
        if (*format == 's') {
            AppendText(dst, capacity, &pos, va_arg(args, const char*));
            format++;
            continue;
        }
        if (*format == 'c') {
            AppendChar(dst, capacity, &pos, (char)va_arg(args, int));
            format++;
            continue;
        }
        if (*format == 'd') {
            int32_t value = va_arg(args, int32_t);
            bool negative = value < 0;
            uint32_t magnitude = negative ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
            AppendUnsigned(dst, capacity, &pos, magnitude, 10, negative);
            format++;
            continue;
        }
        if (*format == 'u') {
            AppendUnsigned(dst, capacity, &pos, va_arg(args, uint32_t), 10, false);
            format++;
            continue;
        }
        if (*format == 'x') {
            AppendUnsigned(dst, capacity, &pos, va_arg(args, uint32_t), 16, false);
            format++;
            continue;
        }

        // Unknown conversion: emit it literally rather than misreading the stack.
        AppendChar(dst, capacity, &pos, '%');
        AppendChar(dst, capacity, &pos, *format++);
    }

    va_end(args);
    dst[pos < capacity ? pos : capacity - 1] = '\0';
}

bool StoreParseU32(const char* text, uint32_t* value) {
    if (text == nullptr || value == nullptr) return false;
    while (IsSpace(*text)) text++;
    if (*text == '\0') return false;

    uint32_t result = 0;
    while (*text != '\0' && !IsSpace(*text)) {
        if (*text < '0' || *text > '9') return false;
        uint32_t digit = (uint32_t)(*text - '0');
        if (result > (0xFFFFFFFFu - digit) / 10u) return false;
        result = result * 10u + digit;
        text++;
    }
    *value = result;
    return true;
}

void StoreU32ToHex(char* dst, uint32_t capacity, uint32_t value) {
    static const char kHex[] = "0123456789abcdef";
    if (dst == nullptr || capacity == 0) return;
    if (capacity < 9) {
        dst[0] = '\0';
        return;
    }
    for (int i = 7; i >= 0; i--) {
        dst[i] = kHex[value & 0xFu];
        value >>= 4;
    }
    dst[8] = '\0';
}

bool StoreHexEqual(const char* left, const char* right) {
    if (left == nullptr || right == nullptr) return left == right;
    while (*left != '\0' && *right != '\0') {
        if (LowerAscii(*left) != LowerAscii(*right)) return false;
        left++;
        right++;
    }
    return *left == *right;
}

bool StoreFileExists(const char* path, uint32_t* size) {
    if (path == nullptr) return false;
    struct stat st;
    if (syscall_stat(path, &st) != 0) return false;
    if ((st.st_mode & 0xF000u) == 0x4000u) return false;
    if (size != nullptr) *size = st.st_size;
    return true;
}

bool StoreDirExists(const char* path) {
    if (path == nullptr) return false;
    struct stat st;
    if (syscall_stat(path, &st) != 0) return false;
    return (st.st_mode & 0xF000u) == 0x4000u;
}

bool StoreEnsureDir(const char* path) {
    if (StoreDirExists(path)) return true;
    if (syscall_mkdir(path, 0755) != 0) return StoreDirExists(path);
    return true;
}

bool StoreReadFile(const char* path, uint8_t** data, uint32_t* size) {
    if (path == nullptr || data == nullptr || size == nullptr) return false;

    struct stat st;
    if (syscall_stat(path, &st) != 0) return false;
    if (st.st_size == 0 || st.st_size > 4u * 1024u * 1024u) return false;

    uint8_t* buffer = (uint8_t*)kmalloc((int)(st.st_size + 1));
    if (buffer == nullptr) return false;

    int32_t fd = syscall_open(path, O_RDONLY);
    if (fd < 0) {
        kfree(buffer);
        return false;
    }

    uint32_t total = 0;
    while (total < st.st_size) {
        int32_t got = syscall_read((uint32_t)fd, (char*)buffer + total, st.st_size - total);
        if (got <= 0) {
            syscall_close((uint32_t)fd);
            kfree(buffer);
            return false;
        }
        total += (uint32_t)got;
    }
    syscall_close((uint32_t)fd);

    buffer[total] = '\0';
    *data = buffer;
    *size = total;
    return true;
}

bool StoreWriteAll(int32_t fd, const uint8_t* data, uint32_t length) {
    if (fd < 0 || (data == nullptr && length != 0)) return false;
    uint32_t written = 0;
    while (written < length) {
        int32_t result = syscall_write((uint32_t)fd, (const char*)data + written, length - written);
        if (result <= 0) return false;
        written += (uint32_t)result;
    }
    return true;
}
