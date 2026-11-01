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

#define KDBG_COMPONENT "GUI:PLAT"
#include <core/globals.h>
#include <core/scheduler.h>
#include <debug.h>
#include <gui/guiplat.h>

namespace guiplat {

void Abort(const char* msg) {
    printf("%s", msg);
    do {
        asm volatile("hlt");
    } while (1);
}

void Sleep(uint32_t ms) {
    if (Scheduler::activeInstance) {
        Scheduler::activeInstance->Sleep(ms);
    }
}

uint64_t MonotonicMillis() {
    return timerTicks;
}

}  // namespace guiplat

GuiCriticalGuard::GuiCriticalGuard() {
    uint32_t eflags;
    asm volatile(
        "pushf\n\t"
        "pop %0"
        : "=r"(eflags));

    wasEnabled = (eflags & 0x200) != 0;
    asm volatile("cli");
}

GuiCriticalGuard::~GuiCriticalGuard() {
    if (wasEnabled) {
        asm volatile("sti");
    }
}
