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

#define KDBG_COMPONENT "GUI:HOST"
#include <core/Iguard.h>
#include <core/globals.h>
#include <core/scheduler.h>
#include <gui/desktop.h>
#include <gui/eventHandler.h>
#include <gui/widget.h>
#include <gui/widgetHost.h>

WidgetHost* g_widgetHost = nullptr;

namespace {

// Widgets waiting to be deleted once no event dispatch is on the stack.
LinkedList<Widget*> g_pendingWidgetDestroy;

/**
 * class KernelWidgetHost - Delivers widget events to the owning process.
 *
 * Events are queued on the process's EventHandler and its thread is woken so
 * the userspace event loop can drain them. Mirrors the behavior that used to
 * live inline in Button::EmitClickEvent() and Window::OnClose().
 */
class KernelWidgetHost : public WidgetHost {
public:
    bool PostEvent(uint32_t pid, uint32_t widgetID, EVENT_TYPE type, uint32_t param1,
                   uint32_t param2) override {
        if (pid == 0 || !Desktop::activeInstance) return false;

        EventHandler* handler = Desktop::activeInstance->getHandler(pid);
        if (!handler) return false;

        Event* event = new Event{widgetID, type, param1, param2};
        if (!event) return false;
        handler->eventQueue.Add(event);
        return true;
    }

    void WakeProcess(uint32_t pid) override {
        if (pid == 0 || !Desktop::activeInstance || !g_scheduler) return;

        EventHandler* handler = Desktop::activeInstance->getHandler(pid);
        if (handler && handler->thread) {
            g_scheduler->WakeThread(handler->thread);
        }
    }

    void DestroyWidgetLater(Widget* widget) override {
        if (!widget) return;
        // Called from the desktop task, but also possibly from a syscall
        // thread; the push has to be atomic with respect to the drain.
        InterruptGuard guard;
        g_pendingWidgetDestroy.PushBack(widget);
    }
};

}  // namespace

void DrainDeferredWidgetDeletes() {
    while (true) {
        Widget* widget = nullptr;
        {
            InterruptGuard guard;
            if (g_pendingWidgetDestroy.IsEmpty()) break;
            widget = g_pendingWidgetDestroy.PopFront();
        }

        // Delete outside the critical section: destructors mark ancestors
        // dirty and unlink from the registry, which is more work than the
        // queue bookkeeping needs to hold interrupts for.
        delete widget;
    }
}

void InstallKernelWidgetHost() {
    if (!g_widgetHost) {
        g_widgetHost = new KernelWidgetHost();
    }
}
