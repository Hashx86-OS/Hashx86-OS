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

#include <stdint.h>
#include <stdlib.h>

/**
 * itoa() - Convert an integer to a string in the given base.
 * @num: Integer to convert.
 * @str: Output buffer for the resulting NUL-terminated string.
 * @base: Numeric base, between 2 and 36.
 * @capacity: Size of the output buffer in bytes.
 *
 * Return: Pointer to the output string.
 */
char* itoa(int32_t num, char* str, uint32_t base, size_t capacity) {
    return itoa_safe(num, str, base, capacity);
}

/**
 * itoa_safe() - Convert an integer to a string with bounds checking.
 * @num: Integer to convert.
 * @str: Output buffer for the resulting NUL-terminated string.
 * @base: Numeric base, between 2 and 36.
 * @capacity: Size of the output buffer in bytes.
 *
 * Digits are produced from the least significant end and then reversed.
 *
 * Return: Pointer to the output string.
 */
char* itoa_safe(int32_t num, char* str, uint32_t base, size_t capacity) {
    if (!str || capacity == 0 || base < 2 || base > 36) {
        if (str && capacity > 0) str[0] = '\0';
        return str;
    }
    size_t i = 0;
    bool isNegative = false;

    if (num == 0) {
        if (i + 1 < capacity) {
            str[i++] = '0';
            str[i] = '\0';
        } else {
            str[capacity - 1] = '\0';
        }
        return str;
    }

    int32_t n = num;
    uint32_t unum;
    if (n < 0 && base == 10) {
        isNegative = true;
        unum = (uint32_t)(-(int64_t)n);
    } else {
        unum = (uint32_t)n;
    }

    while (unum != 0) {
        if (i + 1 >= capacity) break;  // No room for more digits plus the null.
        uint32_t rem = unum % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
        unum = unum / base;
    }

    if (isNegative) {
        if (i + 1 >= capacity) {
            // No room for the minus sign; leave unsigned to avoid truncating digits.
        } else {
            str[i++] = '-';
        }
    }

    str[i] = '\0';

    // Reverse the digits into the right order.
    size_t start = 0, end = (i > 0) ? i - 1 : 0;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }

    return str;
}
