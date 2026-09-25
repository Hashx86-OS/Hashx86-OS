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

#ifndef EVENT_HANDLER_H
#define EVENT_HANDLER_H

#include <core/process_types.h>
#include <utils/linkedList.h>

/**
 * enum EVENT_TYPE - Event kinds queued for the owning process.
 */
typedef enum {
    ON_CLICK = 0x0,
    ON_KEYPRESS = 0x1,
    ON_WINDOW_CLOSE = 0x2,
} EVENT_TYPE;

/**
 * struct Event - A single queued GUI notification for a process.
 * @widgetID: Widget instance ID that produced the event.
 * @eventType: Kind of event (click, keypress, window close).
 * @param1: Event-specific payload.
 * @param2: Event-specific payload.
 */
struct Event {
    uint32_t widgetID;
    EVENT_TYPE eventType;
    uint32_t param1;
    uint32_t param2;
};

/**
 * struct EventHandler - Per-process queue of pending GUI events.
 * @pid: Owning process ID.
 * @thread: Owner thread that consumes the queue.
 * @eventQueue: FIFO of events awaiting delivery.
 */
struct EventHandler {
    uint32_t pid;
    ThreadControlBlock* thread;
    LinkedList<Event*> eventQueue;
};

#endif  // EVENT_HANDLER_H
