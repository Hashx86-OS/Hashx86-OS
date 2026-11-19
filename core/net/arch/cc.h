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

/*
 * lwIP compiler/platform abstraction for Hashx86 (32-bit x86, freestanding).
 *
 * This file is part of the lwIP port layer, not of upstream lwIP.
 */

#ifndef LWIP_ARCH_CC_H
#define LWIP_ARCH_CC_H

#include <stdint.h>

/* The kernel has stdint.h and stddef.h (GCC freestanding headers), but no
 * inttypes.h, unistd.h, ctype.h or a usable glibc limits.h. */
#define LWIP_NO_INTTYPES_H 1
#define LWIP_NO_UNISTD_H 1
#define LWIP_NO_CTYPE_H 1
#define LWIP_NO_LIMITS_H 1
#define INT_MAX 2147483647

#define BYTE_ORDER LITTLE_ENDIAN

/* Packed protocol headers. x86 tolerates unaligned access, but keep the
 * layout exact for on-wire structures. */
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_END
#define PACK_STRUCT_FIELD(x) x

/* Diagnostics and assertions route to the kernel console. These are called
 * from lwIP's C sources, so keep C linkage even when this header is included
 * by a C++ translation unit. */
#ifdef __cplusplus
extern "C" {
#endif
void lwip_diag(const char* format, ...);
void lwip_assert_fail(const char* message);
uint32_t lwip_rand(void);
#ifdef __cplusplus
}
#endif

#define LWIP_PLATFORM_DIAG(x) \
    do {                      \
        lwip_diag x;          \
    } while (0)
#define LWIP_PLATFORM_ASSERT(x) lwip_assert_fail(x)

/* Format specifiers for lwIP's debug output (the kernel printf understands
 * these; it does not support the PRI* length modifiers). */
#define U8_F "c"
#define S8_F "d"
#define X8_F "x"
#define U16_F "u"
#define S16_F "d"
#define X16_F "x"
#define U32_F "u"
#define S32_F "d"
#define X32_F "x"
#define SZT_F "u"

#define LWIP_RAND() lwip_rand()

#endif  // LWIP_ARCH_CC_H
