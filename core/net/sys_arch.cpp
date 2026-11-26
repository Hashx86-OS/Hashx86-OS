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

// lwIP NO_SYS=0 sys_arch implementation.
//
// Semaphores, mutexes and mailboxes are small kernel objects. Waiting is
// implemented with a 1 ms poll plus the kernel's explicit yield interrupt
// (vector 0x2E), which can also be used from inside a syscall handler where
// interrupts are disabled: the yield schedules another thread, then resumes
// this one when the scheduler wakes it.

#include <core/Iguard.h>
#include <core/globals.h>
#include <core/scheduler.h>
#include <debug.h>
#include <string.h>
#include <types.h>

extern "C" {
#include <lwip/err.h>
#include <lwip/opt.h>
#include <lwip/sys.h>
}

struct HxSemaphore {
    volatile int count;
};

struct HxMutex {
    volatile int locked;
};

constexpr int kMaxMailboxMessages = 64;

struct HxMailbox {
    int capacity;
    volatile int head;
    volatile int tail;
    volatile int count;
    void* slots[kMaxMailboxMessages];
};

namespace {

/**
 * KernelYield() - Block the current thread for @milliseconds and reschedule.
 *
 * The timer interrupt normally provides yield points, but syscall handlers run
 * with interrupts disabled. The explicit yield vector lets a blocked syscall
 * give the CPU to the tcpip thread (and receive timer ticks) before continuing.
 */
void KernelYield(uint32_t milliseconds) {
    Scheduler* scheduler = Scheduler::activeInstance;
    if (scheduler == nullptr || scheduler->currentThread == nullptr) {
        // Very early boot: no scheduler to switch to. Busy-wait instead.
        uint32_t start = (uint32_t)timerTicks;
        while (milliseconds != 0 && ((uint32_t)timerTicks - start) < milliseconds) {
            asm volatile("pause");
        }
        return;
    }

    // A termination request cannot cancel this wait by abandoning the stack:
    // netconn->current_msg may still point into it. NetworkCallGuard defers
    // teardown until the bounded request completes and lwIP acknowledges it.
    scheduler->Sleep(milliseconds);
    asm volatile("int $0x2E" ::: "memory");
}

/** Try to consume one semaphore count without blocking. */
bool SemaphoreTryTake(HxSemaphore* sem) {
    if (sem == nullptr) return false;
    InterruptGuard guard;
    if (sem->count <= 0) return false;
    sem->count--;
    return true;
}

/** Try to enqueue a message. Return false when the mailbox is full. */
bool MailboxTryPost(HxMailbox* mbox, void* message) {
    if (mbox == nullptr) return false;
    InterruptGuard guard;
    if (mbox->count >= mbox->capacity) return false;
    mbox->slots[mbox->tail] = message;
    mbox->tail++;
    if (mbox->tail >= mbox->capacity) mbox->tail = 0;
    mbox->count++;
    return true;
}

/** Try to dequeue a message. Return false when the mailbox is empty. */
bool MailboxTryFetch(HxMailbox* mbox, void** message) {
    if (mbox == nullptr) return false;
    InterruptGuard guard;
    if (mbox->count <= 0) return false;
    if (message != nullptr) *message = mbox->slots[mbox->head];
    mbox->head++;
    if (mbox->head >= mbox->capacity) mbox->head = 0;
    mbox->count--;
    return true;
}

}  // namespace

extern "C" {

void sys_init(void) {}

u32_t sys_jiffies(void) {
    return (u32_t)timerTicks;
}

/**
 * sys_arch_protect() - Enter a short critical section.
 *
 * Return: The saved EFLAGS value, to be passed to sys_arch_unprotect().
 */
sys_prot_t sys_arch_protect(void) {
    sys_prot_t flags;
    asm volatile("pushfl; popl %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

void sys_arch_unprotect(sys_prot_t savedFlags) {
    asm volatile("pushl %0; popfl" : : "r"(savedFlags) : "memory", "cc");
}

err_t sys_sem_new(sys_sem_t* sem, u8_t count) {
    if (sem == nullptr) return ERR_ARG;
    HxSemaphore* object = new HxSemaphore();
    if (object == nullptr) return ERR_MEM;
    object->count = (int)count;
    *sem = object;
    return ERR_OK;
}

void sys_sem_free(sys_sem_t* sem) {
    if (sem == nullptr || *sem == nullptr) return;
    delete *sem;
    *sem = nullptr;
}

void sys_sem_signal(sys_sem_t* sem) {
    if (sem == nullptr || *sem == nullptr) return;
    InterruptGuard guard;
    (*sem)->count++;
}

u32_t sys_arch_sem_wait(sys_sem_t* sem, u32_t timeout) {
    if (sem == nullptr || *sem == nullptr) return SYS_ARCH_TIMEOUT;

    HxSemaphore* object = *sem;
    u32_t start = (u32_t)timerTicks;

    for (;;) {
        if (SemaphoreTryTake(object)) {
            return (u32_t)timerTicks - start;
        }
        if (timeout != 0 && ((u32_t)timerTicks - start) >= timeout) {
            return SYS_ARCH_TIMEOUT;
        }
        KernelYield(1);
    }
}

err_t sys_mutex_new(sys_mutex_t* mutex) {
    if (mutex == nullptr) return ERR_ARG;
    HxMutex* object = new HxMutex();
    if (object == nullptr) return ERR_MEM;
    object->locked = 0;
    *mutex = object;
    return ERR_OK;
}

void sys_mutex_free(sys_mutex_t* mutex) {
    if (mutex == nullptr || *mutex == nullptr) return;
    delete *mutex;
    *mutex = nullptr;
}

void sys_mutex_lock(sys_mutex_t* mutex) {
    if (mutex == nullptr || *mutex == nullptr) return;
    HxMutex* object = *mutex;

    for (;;) {
        bool acquired = false;
        {
            InterruptGuard guard;
            if (object->locked == 0) {
                object->locked = 1;
                acquired = true;
            }
        }
        if (acquired) return;
        KernelYield(1);
    }
}

void sys_mutex_unlock(sys_mutex_t* mutex) {
    if (mutex == nullptr || *mutex == nullptr) return;
    InterruptGuard guard;
    (*mutex)->locked = 0;
}

err_t sys_mbox_new(sys_mbox_t* mbox, int size) {
    if (mbox == nullptr) return ERR_ARG;

    HxMailbox* object = new HxMailbox();
    if (object == nullptr) return ERR_MEM;

    int capacity = size > 0 ? size : 16;
    if (capacity > kMaxMailboxMessages) capacity = kMaxMailboxMessages;
    object->capacity = capacity;
    object->head = 0;
    object->tail = 0;
    object->count = 0;
    memset(object->slots, 0, sizeof(object->slots));

    *mbox = object;
    return ERR_OK;
}

void sys_mbox_free(sys_mbox_t* mbox) {
    if (mbox == nullptr || *mbox == nullptr) return;
    delete *mbox;
    *mbox = nullptr;
}

void sys_mbox_post(sys_mbox_t* mbox, void* message) {
    if (mbox == nullptr || *mbox == nullptr) return;
    while (!MailboxTryPost(*mbox, message)) {
        KernelYield(1);
    }
}

err_t sys_mbox_trypost(sys_mbox_t* mbox, void* message) {
    if (mbox == nullptr || *mbox == nullptr) return ERR_ARG;
    return MailboxTryPost(*mbox, message) ? ERR_OK : ERR_MEM;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t* mbox, void* message) {
    return sys_mbox_trypost(mbox, message);
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t* mbox, void** message) {
    if (mbox == nullptr || *mbox == nullptr) return SYS_MBOX_EMPTY;
    return MailboxTryFetch(*mbox, message) ? 0 : SYS_MBOX_EMPTY;
}

u32_t sys_arch_mbox_fetch(sys_mbox_t* mbox, void** message, u32_t timeout) {
    if (mbox == nullptr || *mbox == nullptr) return SYS_ARCH_TIMEOUT;

    u32_t start = (u32_t)timerTicks;
    for (;;) {
        if (MailboxTryFetch(*mbox, message)) {
            return (u32_t)timerTicks - start;
        }
        if (timeout != 0 && ((u32_t)timerTicks - start) >= timeout) {
            return SYS_ARCH_TIMEOUT;
        }
        KernelYield(1);
    }
}

sys_thread_t sys_thread_new(const char* name, lwip_thread_fn thread, void* arg, int stackSize,
                            int priority) {
    (void)name;
    (void)stackSize;
    (void)priority;

    if (g_scheduler == nullptr || thread == nullptr) return nullptr;

    ProcessControlBlock* process = g_scheduler->CreateProcess(true, thread, arg);
    if (process == nullptr) {
        printf("[NET] sys_thread_new: CreateProcess failed\n");
        return nullptr;
    }
    return (sys_thread_t)process;
}

}  // extern "C"
