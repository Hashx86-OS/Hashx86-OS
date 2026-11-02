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

#include <gui/guiplat.h>

#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "harness.h"

namespace {
uint64_t g_fakeMillis = 0;
}  // namespace

void guiplatTestAdvanceClock(uint64_t ms) {
    g_fakeMillis += ms;
}

void guiplatTestResetClock() {
    g_fakeMillis = 0;
}

namespace guiplat {

void Abort(const char* msg) {
    std::fprintf(stderr, "%s", msg);
    std::abort();
}

void Sleep(uint32_t ms) {
    usleep(ms * 1000);
}

uint64_t MonotonicMillis() {
    return g_fakeMillis;
}

}  // namespace guiplat

GuiCriticalGuard::GuiCriticalGuard() {
    wasEnabled = false;
}

GuiCriticalGuard::~GuiCriticalGuard() {}
