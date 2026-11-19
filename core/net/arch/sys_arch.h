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

#ifndef CORE_NET_ARCH_SYS_ARCH_H
#define CORE_NET_ARCH_SYS_ARCH_H

#include <stdint.h>

/*
 * lwIP sys_arch types for Hashx86.
 *
 * Handles are pointers to kernel objects allocated by core/net/sys_arch.cpp.
 * The validity macros take a pointer to the handle, matching lwIP's
 * sys_*_valid(&field) convention.
 */

typedef uint32_t sys_prot_t;
typedef void* sys_thread_t;

struct HxSemaphore;
struct HxMutex;
struct HxMailbox;

typedef struct HxSemaphore* sys_sem_t;
typedef struct HxMutex* sys_mutex_t;
typedef struct HxMailbox* sys_mbox_t;

#define sys_sem_valid(sem) ((sem) != 0 && *(sem) != 0)
#define sys_sem_set_invalid(sem)    \
    do {                            \
        if ((sem) != 0) *(sem) = 0; \
    } while (0)

#define sys_mutex_valid(mutex) ((mutex) != 0 && *(mutex) != 0)
#define sys_mutex_set_invalid(mutex)    \
    do {                                \
        if ((mutex) != 0) *(mutex) = 0; \
    } while (0)

#define sys_mbox_valid(mbox) ((mbox) != 0 && *(mbox) != 0)
#define sys_mbox_set_invalid(mbox)    \
    do {                              \
        if ((mbox) != 0) *(mbox) = 0; \
    } while (0)

#endif  // CORE_NET_ARCH_SYS_ARCH_H
