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

#include <Hx86/Hx86.h>
#include <Hx86/debug.h>

void vprintf(const char* format, va_list args);

void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

// Helper to write a single character to the output buffer.
void buffer_char(char* buffer, int* pIndex, int maxLen, char c) {
    if (*pIndex < maxLen - 1) {
        buffer[(*pIndex)++] = c;
    }
}

/**
 * append_uint() - Append an unsigned number in @base to the output buffer.
 * @output: Destination buffer.
 * @idx: In/out write index.
 * @maxLen: Capacity of @output.
 * @value: Value to render.
 * @base: 10 or 16.
 * @upper: True to use uppercase hex digits.
 */
static void append_uint(char* output, int* idx, int maxLen, uint32_t value, uint32_t base,
                        bool upper) {
    const char* digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[33];
    int n = 0;

    if (value == 0) {
        tmp[n++] = '0';
    } else {
        while (value > 0) {
            tmp[n++] = digits[value % base];
            value /= base;
        }
    }

    while (n > 0) buffer_char(output, idx, maxLen, tmp[--n]);
}

void vprintf(const char* format, va_list args) {
    char output[256];  // Stack buffer.
    int idx = 0;

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] != '%') {
            buffer_char(output, &idx, 256, format[i]);
            continue;
        }

        i++;
        char spec = format[i];
        if (spec == '\0') break;  // Trailing '%'.

        switch (spec) {
            case 'd':
            case 'i': {
                int num = va_arg(args, int);
                bool neg = (num < 0);
                uint32_t magnitude = (uint32_t)num;
                if (neg) {
                    buffer_char(output, &idx, 256, '-');
                    // Two's-complement magnitude also handles INT_MIN.
                    magnitude = 0u - magnitude;
                }
                append_uint(output, &idx, 256, magnitude, 10, false);
                break;
            }
            case 'u': {
                uint32_t num = va_arg(args, uint32_t);
                append_uint(output, &idx, 256, num, 10, false);
                break;
            }
            case 'x':
            case 'X': {
                uint32_t num = va_arg(args, uint32_t);
                append_uint(output, &idx, 256, num, 16, true);
                break;
            }
            case 'c': {
                int ch = va_arg(args, int);
                buffer_char(output, &idx, 256, (char)ch);
                break;
            }
            case 's': {
                const char* s = va_arg(args, const char*);
                if (!s) s = "(null)";
                for (int k = 0; s[k] != '\0'; k++) buffer_char(output, &idx, 256, s[k]);
                break;
            }
            case '%': {
                buffer_char(output, &idx, 256, '%');
                break;
            }
            default:
                // Unknown conversion: emit it literally instead of dropping it.
                buffer_char(output, &idx, 256, '%');
                buffer_char(output, &idx, 256, spec);
                break;
        }
    }

    output[idx] = '\0';  // Null-terminate the buffer.

    // Write through stdout; fallback to debug syscall if stdio is unavailable.
    if (idx > 0) {
        int32_t written = syscall_write(1, output, (uint32_t)idx);
        if (written < (int32_t)idx) {
            syscall_debug(output);
        }

        // If CLI mode created a terminal host window, mirror output there.
        cli_append_output(output);
    }
}

void DebugPrintf(const char* tag, const char* format, ...) {
    printf("%s:", tag);  // Print the tag and colon.

    va_list args;
    va_start(args, format);
    vprintf(format, args);  // Use vprintf to handle a va_list.
    va_end(args);
    printf("\n");  // Print the trailing newline.
}

void Printf(const char* tag, const char* format, ...) {
    printf("%s:", tag);  // Print the tag and colon.
    va_list args;
    va_start(args, format);
    vprintf(format, args);  // Use vprintf to handle a va_list.
    va_end(args);
}
