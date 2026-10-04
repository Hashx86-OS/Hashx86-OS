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

#ifndef GUI_TEST_HARNESS_H
#define GUI_TEST_HARNESS_H

#include <types.h>

/**
 * struct HostEvent - A widget event recorded by the host test widget host.
 */
struct HostEvent {
    uint32_t pid;
    uint32_t widgetID;
    uint8_t type;
    uint32_t param1;
    uint32_t param2;
};

/** HostEventCount() - Number of events recorded so far. */
int HostEventCount();

/** HostEventAt() - Event recorded at @index (must be in range). */
const HostEvent& HostEventAt(int index);

/** HostEventReset() - Discard all recorded events. */
void HostEventReset();

/** guiplatTestAdvanceClock() - Advance the fake millisecond clock. */
void guiplatTestAdvanceClock(uint64_t ms);

/** guiplatTestResetClock() - Reset the fake clock to zero. */
void guiplatTestResetClock();

#endif  // GUI_TEST_HARNESS_H
