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

#ifndef GUIPLAT_H
#define GUIPLAT_H

#include <types.h>

/**
 * namespace guiplat - The only OS services the portable GUI layer may use.
 *
 * Two implementations exist: the kernel adapter (gui/guiplat_kernel.cpp) binds
 * these to the interrupt guard, the scheduler and the kernel serial log; the
 * host-test adapter (tests/host/shim/guiplat_host.cpp) binds them to the C
 * library and a controllable fake clock. Keeping the portable GUI layer behind
 * this interface is what lets the same widget sources compile into the kernel
 * and into a host test binary.
 */
namespace guiplat {

/**
 * Abort() - Report a fatal GUI error and stop.
 * @msg: NUL-terminated message describing the failure.
 *
 * Does not return. The kernel implementation prints to serial and halts; the
 * host implementation prints to stderr and aborts.
 */
[[noreturn]] void Abort(const char* msg);

/**
 * Sleep() - Block the caller for a number of milliseconds.
 * @ms: Duration to sleep.
 */
void Sleep(uint32_t ms);

/**
 * MonotonicMillis() - Milliseconds since an unspecified epoch.
 *
 * Monotonic and never decreases. Host tests drive it with a fake clock so
 * timing-dependent behavior is deterministic.
 */
uint64_t MonotonicMillis();

}  // namespace guiplat

/**
 * class GuiCriticalGuard - RAII critical section that nests safely.
 *
 * Constructing saves the current interrupt state and masks interrupts;
 * destroying restores the saved state, so a section entered with interrupts
 * already disabled stays disabled on exit.
 */
class GuiCriticalGuard {
private:
    bool wasEnabled;

public:
    GuiCriticalGuard();
    ~GuiCriticalGuard();

    GuiCriticalGuard(const GuiCriticalGuard&) = delete;
    GuiCriticalGuard& operator=(const GuiCriticalGuard&) = delete;
};

#endif  // GUIPLAT_H
