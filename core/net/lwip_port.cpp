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

// lwIP platform hooks for Hashx86.
//
// The kernel provides the millisecond clock (timerTicks), printf/vprintf and the
// fixed-memory allocator used by lwIP. This file supplies the functions the
// portable stack itself expects from a port.
//
// This is C++ so it can use the kernel's C++-linked debug/timer symbols; the
// functions called from lwIP's C sources are exported with C linkage.

#include <core/globals.h>
#include <core/random.h>
#include <debug.h>
#include <stdarg.h>
#include <stdlib.h>
#include <types.h>

#include <lwip/arch.h>
#include <lwip/ip_addr.h>
#include <lwip/opt.h>
#include <lwip/sys.h>

/* Declared by core/debug.cpp but not exported by debug.h. */
void vprintf(const char* format, va_list args);

extern "C" {

// lwIP's socket layer stores its last error here (LWIP_PROVIDE_ERRNO).
// Single global errno is sufficient until per-thread errno is added.
int errno = 0;

void lwip_diag(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
}

/**
 * lwip_assert_fail() - Report a failed lwIP invariant and stop.
 * @message: Assertion text supplied by lwIP.
 *
 * A failed stack invariant means memory or protocol state is corrupt; halt
 * rather than continue with undefined networking state.
 */
void lwip_assert_fail(const char* message) {
    printf("[NET] lwIP assertion failed: %s\n", message ? message : "(null)");
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

uint32_t lwip_rand(void) {
    uint32_t value;
    if (!RandomBytes(&value, sizeof(value))) lwip_assert_fail("hardware entropy unavailable");
    return value;
}

u32_t hashx86_tcp_isn(const ip_addr_t* local, u16_t lp, const ip_addr_t* remote, u16_t rp) {
    return RandomTcpIsn(ip4_addr_get_u32(ip_2_ip4(local)), lp, ip4_addr_get_u32(ip_2_ip4(remote)),
                        rp);
}

/**
 * sys_now() - lwIP millisecond clock.
 *
 * timerTicks advances once per PIT tick; the scheduler and GUI use the same
 * source, so lwIP timers stay consistent with Sleep().
 */
u32_t sys_now(void) {
    return (u32_t)timerTicks;
}

/**
 * atoi() - Minimal decimal parser required by lwIP's netif_find().
 *
 * The kernel's freestanding C library does not provide atoi; lwIP's netif.c
 * calls it unconditionally even though Hashx86 does not use netif_find yet.
 */
int atoi(const char* text) {
    if (!text) return 0;

    int sign = 1;
    if (*text == '-') {
        sign = -1;
        text++;
    } else if (*text == '+') {
        text++;
    }

    int value = 0;
    while (*text >= '0' && *text <= '9') {
        value = value * 10 + (*text - '0');
        text++;
    }
    return sign * value;
}

}  // extern "C"
