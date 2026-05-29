/**
 * @file        stdlib.cpp
 * @brief       Standard Library Implementation
 *
 * @date        01/02/2026
 * @version     1.0.0
 */

#include <stdint.h>
#include <stdlib.h>

char* itoa(int32_t num, char* str, uint32_t base, size_t capacity) {
    return itoa_safe(num, str, base, capacity);
}

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
        if (i + 1 >= capacity) break;  // No room for more digits + null
        uint32_t rem = unum % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
        unum = unum / base;
    }

    if (isNegative) {
        if (i + 1 >= capacity) {
            // No room for minus sign; leave unsigned to avoid truncating digits
        } else {
            str[i++] = '-';
        }
    }

    str[i] = '\0';

    // Reverse the string
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
