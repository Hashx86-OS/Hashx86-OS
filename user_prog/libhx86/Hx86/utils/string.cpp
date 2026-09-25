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

#include <Hx86/utils/string.h>

int strlen(const char* s) {
    int len = 0;
    while (*s++) len++;
    return len;
}

int strcmp(const char* s1, char* s2) {
    int i = 0;

    while ((s1[i] == s2[i])) {
        if (s2[i++] == 0) return 0;
    }
    return 1;
}

int strcpy(char* dst, const char* src) {
    int i = 0;
    while ((*dst++ = *src++) != 0) i++;
    return i;
}

void strcat(char* dest, const char* src) {
    char* end = (char*)dest + strlen(dest);
    memcpy((void*)end, (void*)src, strlen(src));
    end = end + strlen(src);
    *end = '\0';
}

int strncmp(const char* s1, const char* s2, size_t n) {
    while (n > 0) {
        unsigned char c1 = (unsigned char)*s1++;
        unsigned char c2 = (unsigned char)*s2++;
        if (c1 != c2) {
            return c1 - c2;
        }
        if (c1 == '\0') {
            return 0;
        }
        n--;
    }
    return 0;
}

int isspace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

int isalpha(char c) {
    return (((c >= 'A') && (c <= 'Z')) || ((c >= 'a') && (c <= 'z')));
}

char upper(char c) {
    if ((c >= 'a') && (c <= 'z')) return (c - 32);
    return c;
}

char lower(char c) {
    if ((c >= 'A') && (c <= 'Z')) return (c + 32);
    return c;
}

void itoa(char* buf, int base, int d) {
    char* p = buf;
    char *p1, *p2;
    unsigned long ud = d;
    int divisor = 10;

    // If base is 'd' and d is negative, put '-' at the head.
    if (base == 'd' && d < 0) {
        *p++ = '-';
        buf++;
        ud = -d;
    } else if (base == 'x')
        divisor = 16;

    // Divide ud by divisor until ud reaches 0.
    do {
        int remainder = ud % divisor;
        *p++ = (remainder < 10) ? remainder + '0' : remainder + 'a' - 10;
    } while (ud /= divisor);

    // Terminate the buffer.
    *p = 0;

    // Reverse the buffer.
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1 = *p2;
        *p2 = tmp;
        p1++;
        p2--;
    }
}
