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

#ifndef HOST_SHIM_DEBUG_H
#define HOST_SHIM_DEBUG_H

// Host-test shim for <debug.h>. Keeps the KDBG macros inert and routes HALT to
// a hard abort so a violated invariant fails the test run loudly.

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#ifndef KDBG_ENABLE
#define KDBG_ENABLE 0
#endif

#ifndef KDBG_LEVEL
#define KDBG_LEVEL 0
#endif

#ifndef KDBG_COMPONENT
#define KDBG_COMPONENT "HOST"
#endif

#define KDBG1(...) ((void)0)
#define KDBG2(...) ((void)0)
#define KDBG3(...) ((void)0)
#define KDBG_L1(...) ((void)0)
#define KDBG_L2(...) ((void)0)
#define KDBG_L3(...) ((void)0)

#define HALT(msg)                        \
    do {                                 \
        std::fprintf(stderr, "%s", msg); \
        std::abort();                    \
    } while (0)

#endif  // HOST_SHIM_DEBUG_H
