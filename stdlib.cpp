/**
 * @file        stdlib.cpp
 * @brief       Standard Library Implementation
 *
 * @date        01/02/2026
 * @version     1.0.0
 */

#include <stdint.h>
#include <stdlib.h>

char* itoa(int32_t num, char* str, uint32_t base) {
    if (!str || base < 2 || base > 36) {
        if (str) str[0] = '\0';
        return str;
    }
    uint32_t i = 0;
    bool isNegative = false;

    if (num == 0) {
        str[i++] = '0';
        str[i] = '\0';
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
        uint32_t rem = unum % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'A' : rem + '0';
        unum = unum / base;
    }

    if (isNegative) {
        str[i++] = '-';
    }

    str[i] = '\0';

    // Reverse the string
    uint32_t start = 0, end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }

    return str;
}
