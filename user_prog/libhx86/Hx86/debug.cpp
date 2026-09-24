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

void vprintf(const char* format, va_list args) {
    char output[256];  // Stack buffer.
    int idx = 0;

    for (int i = 0; format[i] != '\0'; i++) {
        if (format[i] == '%') {
            i++;
            switch (format[i]) {
                case 'd': {
                    int num = va_arg(args, int);
                    char tmp[12];
                    int tIdx = 11;
                    tmp[tIdx] = '\0';
                    bool neg = (num < 0);
                    if (neg) num = -num;

                    if (num == 0)
                        tmp[--tIdx] = '0';
                    else {
                        while (num > 0) {
                            tmp[--tIdx] = (num % 10) + '0';
                            num /= 10;
                        }
                    }
                    if (neg) tmp[--tIdx] = '-';

                    // Copy tmp to the main buffer.
                    for (int k = tIdx; tmp[k]; k++) buffer_char(output, &idx, 256, tmp[k]);
                    break;
                }
                case 's': {
                    const char* s = va_arg(args, const char*);
                    for (int k = 0; s[k]; k++) buffer_char(output, &idx, 256, s[k]);
                    break;
                }
                case 'x': {
                    uint32_t num = va_arg(args, uint32_t);
                    char tmp[9];
                    int tIdx = 8;
                    tmp[tIdx] = '\0';
                    const char* hex = "0123456789ABCDEF";
                    if (num == 0)
                        tmp[--tIdx] = '0';
                    else {
                        while (num > 0) {
                            tmp[--tIdx] = hex[num % 16];
                            num /= 16;
                        }
                    }
                    for (int k = tIdx; tmp[k]; k++) buffer_char(output, &idx, 256, tmp[k]);
                    break;
                }
                // Add other cases (u, c, etc.) here.
                default:
                    break;
            }
        } else {
            buffer_char(output, &idx, 256, format[i]);
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
