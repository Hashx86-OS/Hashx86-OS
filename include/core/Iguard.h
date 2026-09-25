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

#ifndef InterruptGuard_H
#define InterruptGuard_H

#include <types.h>

/**
 * class InterruptGuard - RAII guard that masks and restores interrupts.
 *
 * Saving and restoring raw interrupt state rather than unconditionally
 * re-enabling interrupts, so a critical section entered with IRQs already
 * disabled stays disabled.
 */
class InterruptGuard {
    bool wasEnabled;

public:
    InterruptGuard() {
        uint32_t eflags;

        // Save the current EFLAGS register.
        asm volatile(
            "pushf\n\t"
            "pop %0"
            : "=r"(eflags));

        // Remember whether the interrupt flag (IF, bit 0x200) was set.
        wasEnabled = (eflags & 0x200);

        // Mask interrupts for the duration of the critical section.
        asm volatile("cli");
    }

    ~InterruptGuard() {
        // Restore the interrupt flag only if it was enabled on entry.
        if (wasEnabled) {
            asm volatile("sti");
        }
    }
};

#endif  // InterruptGuard_H
