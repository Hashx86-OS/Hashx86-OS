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

// mbedTLS platform port for Hashx86 user programs.
//
// mbedTLS normally expects a hosted libc (calloc/free, time/gmtime, /dev/
// urandom, stdio). This file supplies the minimum it needs from libhx86 and
// the kernel syscalls.

#include <Hx86/Hsyscalls/syscalls.h>
#include <hashx86_mbedtls.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

// libhx86 heap entry points (C++ linkage in memory.cpp).
void* kcalloc(int count, int size);
void kfree(void* pointer);

extern "C" {
#include <mbedtls/entropy.h>
#include <mbedtls/platform.h>
#include <mbedtls/platform_time.h>
#include <mbedtls/platform_util.h>
}

namespace {

void* TlsCalloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return nullptr;
    if (count > 0x7FFFFFFFu / size) return nullptr;
    return kcalloc((int)count, (int)size);
}

void TlsFree(void* pointer) {
    if (pointer != nullptr) kfree(pointer);
}

mbedtls_time_t TlsTime(mbedtls_time_t* timer) {
    int32_t now = syscall_time(nullptr);
    if (now < 0) now = 0;
    if (timer != nullptr) *timer = (mbedtls_time_t)now;
    return (mbedtls_time_t)now;
}

uint32_t g_entropyState = 0;

uint32_t NextEntropyWord() {
    if (g_entropyState == 0) {
        uint32_t low;
        uint32_t high;
        asm volatile("rdtsc" : "=a"(low), "=d"(high));
        g_entropyState = low ^ (high * 2654435761u) ^ (uint32_t)syscall_time(nullptr) ^
                         (uint32_t)(uintptr_t)&g_entropyState;
        if (g_entropyState == 0) g_entropyState = 0xA5A5A5A5u;
    }
    g_entropyState ^= g_entropyState << 13;
    g_entropyState ^= g_entropyState >> 17;
    g_entropyState ^= g_entropyState << 5;
    return g_entropyState;
}

}  // namespace

extern "C" {

// C-linkage string routines for the C mbedTLS objects. libhx86's equivalents
// are C++ definitions, so they do not satisfy references from C code.
void* memcpy(void* destination, const void* source, size_t size) {
    uint8_t* dst = (uint8_t*)destination;
    const uint8_t* src = (const uint8_t*)source;
    for (size_t i = 0; i < size; i++) dst[i] = src[i];
    return destination;
}

void* memset(void* destination, int value, size_t size) {
    uint8_t* dst = (uint8_t*)destination;
    for (size_t i = 0; i < size; i++) dst[i] = (uint8_t)value;
    return destination;
}

int memcmp(const void* left, const void* right, size_t size) {
    const uint8_t* a = (const uint8_t*)left;
    const uint8_t* b = (const uint8_t*)right;
    for (size_t i = 0; i < size; i++) {
        if (a[i] != b[i]) return (int)a[i] - (int)b[i];
    }
    return 0;
}

void* memmove(void* destination, const void* source, size_t size) {
    uint8_t* dst = (uint8_t*)destination;
    const uint8_t* src = (const uint8_t*)source;
    if (dst < src) {
        for (size_t i = 0; i < size; i++) dst[i] = src[i];
    } else if (dst > src) {
        for (size_t i = size; i > 0; i--) dst[i - 1] = src[i - 1];
    }
    return destination;
}

/*
 * mbedTLS uses snprintf only for human-readable certificate/OID info paths
 * that Hashx86 does not call. Keep the symbol present, emit an empty string,
 * and never write past the caller's buffer.
 */
int snprintf(char* buffer, size_t size, const char* format, ...) {
    (void)format;
    if (buffer != nullptr && size != 0) buffer[0] = '\0';
    return 0;
}

int vsnprintf(char* buffer, size_t size, const char* format, va_list args) {
    (void)format;
    (void)args;
    if (buffer != nullptr && size != 0) buffer[0] = '\0';
    return 0;
}

size_t strlen(const char* text) {
    size_t length = 0;
    while (text[length] != '\0') length++;
    return length;
}

int strcmp(const char* left, const char* right) {
    while (*left != '\0' && *left == *right) {
        left++;
        right++;
    }
    return (int)(uint8_t)*left - (int)(uint8_t)*right;
}

int strncmp(const char* left, const char* right, size_t size) {
    for (size_t i = 0; i < size; i++) {
        uint8_t a = (uint8_t)left[i];
        uint8_t b = (uint8_t)right[i];
        if (a != b) return (int)a - (int)b;
        if (a == 0) break;
    }
    return 0;
}

char* strncpy(char* destination, const char* source, size_t size) {
    size_t i = 0;
    for (; i < size && source[i] != '\0'; i++) destination[i] = source[i];
    for (; i < size; i++) destination[i] = '\0';
    return destination;
}

char* strchr(const char* text, int character) {
    char target = (char)character;
    while (*text != '\0') {
        if (*text == target) return (char*)text;
        text++;
    }
    return target == '\0' ? (char*)text : nullptr;
}

char* strstr(const char* haystack, const char* needle) {
    if (*needle == '\0') return (char*)haystack;
    for (; *haystack != '\0'; haystack++) {
        const char* h = haystack;
        const char* n = needle;
        while (*h != '\0' && *n != '\0' && *h == *n) {
            h++;
            n++;
        }
        if (*n == '\0') return (char*)haystack;
    }
    return nullptr;
}

/**
 * mbedtls_hardware_poll() - Entropy source for mbedTLS.
 *
 * Mixes the CPU timestamp counter, RTC seconds and a per-process address into
 * an xorshift generator. This is adequate for the educational TLS bring-up;
 * a production port should prefer RDRAND/RDSEED or a hardware RNG.
 */
int mbedtls_hardware_poll(void* data, unsigned char* output, size_t len, size_t* olen) {
    (void)data;
    if (output == nullptr) return -1;

    for (size_t i = 0; i < len; i++) {
        output[i] = (unsigned char)(NextEntropyWord() & 0xFF);
    }
    if (olen != nullptr) *olen = len;
    return 0;
}

/**
 * mbedtls_platform_gmtime_r() - Convert Unix seconds to a UTC struct tm.
 *
 * mbedTLS uses this for X.509 not-before/not-after checks.
 */
struct tm* mbedtls_platform_gmtime_r(const mbedtls_time_t* tt, struct tm* tmBuf) {
    if (tt == nullptr || tmBuf == nullptr || *tt < 0) return nullptr;

    uint32_t seconds = (uint32_t)*tt;
    int32_t days = (int32_t)(seconds / 86400u);
    uint32_t secondsOfDay = seconds % 86400u;

    int32_t z = days + 719468;
    int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    uint32_t doe = (uint32_t)(z - era * 146097);
    uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int32_t year = (int32_t)yoe + era * 400;
    uint32_t dayOfYear = doe - (365 * yoe + yoe / 4 - yoe / 100);
    uint32_t mp = (5 * dayOfYear + 2) / 153;
    uint32_t day = dayOfYear - (153 * mp + 2) / 5 + 1;
    uint32_t month = mp + (mp < 10 ? 3 : (uint32_t)-9);
    if (month <= 2) year++;

    tmBuf->tm_year = year - 1900;
    tmBuf->tm_mon = (int)month - 1;
    tmBuf->tm_mday = (int)day;
    tmBuf->tm_hour = (int)(secondsOfDay / 3600u);
    tmBuf->tm_min = (int)((secondsOfDay % 3600u) / 60u);
    tmBuf->tm_sec = (int)(secondsOfDay % 60u);
    tmBuf->tm_wday = 0;
    tmBuf->tm_yday = (int)dayOfYear;
    tmBuf->tm_isdst = 0;
    return tmBuf;
}

void Hashx86MbedtlsPlatformInit(void) {
    mbedtls_platform_set_calloc_free(TlsCalloc, TlsFree);
    mbedtls_platform_set_time(TlsTime);
}

void __assert_fail(const char* expression, const char* file, unsigned int line) {
    (void)expression;
    (void)file;
    (void)line;
    syscall_debug("[TLS] mbedTLS assertion failed\n");
    syscall_exit_group(4);
    for (;;) {
    }
}

}  // extern "C"
