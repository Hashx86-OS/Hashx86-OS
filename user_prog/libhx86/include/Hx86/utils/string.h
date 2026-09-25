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

#ifndef STRING_H
#define STRING_H

#include <Hx86/memory.h>
#include <Hx86/types.h>

int strlen(const char* s);

int strcmp(const char* s1, char* s2);

int strcpy(char* dst, const char* src);

void strcat(char* dest, const char* src);

int isspace(char c);

int isalpha(char c);
char upper(char c);
char lower(char c);

/** itoa() - Convert an integer to an ASCII string.
 * @buf: Output buffer for the resulting string.
 * @base: Conversion base; 'd' for decimal, 'x' for hexadecimal.
 * @d: Integer value to convert.
 */
void itoa(char* buf, int base, int d);

#endif
