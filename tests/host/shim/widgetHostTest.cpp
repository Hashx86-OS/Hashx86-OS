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

#include <gui/widgetHost.h>

#include "harness.h"

WidgetHost* g_widgetHost = nullptr;

namespace {

constexpr int MAX_EVENTS = 200000;

HostEvent g_events[MAX_EVENTS];
int g_count = 0;

/**
 * class RecordingWidgetHost - Host implementation of WidgetHost.
 *
 * Records every posted event so tests can assert on delivery without a kernel
 * scheduler. PostEvent() always succeeds.
 */
class RecordingWidgetHost : public WidgetHost {
public:
    bool PostEvent(uint32_t pid, uint32_t widgetID, EVENT_TYPE type, uint32_t param1,
                   uint32_t param2) override {
        if (g_count < MAX_EVENTS) {
            g_events[g_count].pid = pid;
            g_events[g_count].widgetID = widgetID;
            g_events[g_count].type = (uint8_t)type;
            g_events[g_count].param1 = param1;
            g_events[g_count].param2 = param2;
            g_count++;
        }
        return true;
    }

    void WakeProcess(uint32_t) override {}
};

RecordingWidgetHost g_host;

struct Installer {
    Installer() {
        g_widgetHost = &g_host;
    }
} g_installer;

}  // namespace

int HostEventCount() {
    return g_count;
}

const HostEvent& HostEventAt(int index) {
    return g_events[index];
}

void HostEventReset() {
    g_count = 0;
}
