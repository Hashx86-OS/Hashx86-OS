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

#ifndef WIDGET_HOST_H
#define WIDGET_HOST_H

#include <gui/event.h>
#include <types.h>

class Widget;

/**
 * class WidgetHost - Environment bridge for the portable widget layer.
 *
 * Widgets never talk to the scheduler, the syscall dispatcher, or any other
 * kernel subsystem directly. When a widget needs to deliver an event to its
 * owning process it calls through this interface. The kernel implementation
 * queues the event on the process and wakes its thread; the host-test
 * implementation records the call; a future userspace-server implementation
 * would forward it over IPC.
 */
class WidgetHost {
public:
    virtual ~WidgetHost() {}

    /**
     * PostEvent() - Deliver a GUI event raised by @widgetID to process @pid.
     * @pid: Owning process ID (0 means "no process", so the event is dropped).
     * @widgetID: Widget instance ID that produced the event.
     * @type: Event kind.
     * @param1: Event-specific payload.
     * @param2: Event-specific payload.
     *
     * Return: True when the event was accepted for delivery (a handler exists
     *         for @pid or the environment records events unconditionally),
     *         false when it could not be delivered.
     */
    virtual bool PostEvent(uint32_t pid, uint32_t widgetID, EVENT_TYPE type, uint32_t param1 = 0,
                           uint32_t param2 = 0) = 0;

    /**
     * WakeProcess() - Wake the thread that owns @pid, if any is registered.
     * @pid: Owning process ID.
     */
    virtual void WakeProcess(uint32_t pid) = 0;

    /**
     * DestroyWidgetLater() - Ask the environment to destroy @widget safely.
     * @widget: Widget to delete once the current event dispatch has unwound.
     *
     * A widget cannot delete itself from inside one of its own callbacks (the
     * caller may still be walking the tree). The kernel queues the pointer on
     * the desktop task; tests queue it and call DrainDeferredWidgetDeletes().
     *
     * The default is a no-op so an environment that does not support deferred
     * deletion leaves ownership with the caller rather than risking a
     * use-after-free.
     */
    virtual void DestroyWidgetLater(Widget* widget) {
        (void)widget;
    }
};

/**
 * g_widgetHost - The active host, set once by the environment.
 *
 * NULL until the kernel installs its adapter or a test installs its recorder.
 * Every call site must be NULL-safe.
 */
extern WidgetHost* g_widgetHost;

/**
 * DrainDeferredWidgetDeletes() - Destroy widgets queued by DestroyWidgetLater().
 *
 * Called by the desktop task between input dispatches, where no widget code is
 * on the stack. Must be defined by every environment that installs a host.
 */
void DrainDeferredWidgetDeletes();

#endif  // WIDGET_HOST_H
