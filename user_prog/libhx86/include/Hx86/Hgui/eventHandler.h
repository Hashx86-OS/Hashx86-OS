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

#include <Hx86/utils/linkedList.h>

// Widget event types delivered through the event handler loop.
typedef enum {
    ON_CLICK = 0x0,
    ON_KEYPRESS = 0x1,
    ON_WINDOW_CLOSE = 0x2,
} EVENT_TYPE;

/** struct Event - One widget event queued for delivery.
 * @widgetID: Kernel-side ID of the widget that produced the event.
 * @eventType: Type of event.
 * @param1: Event-specific parameter.
 * @param2: Event-specific parameter.
 */
struct Event {
    uint32_t widgetID;
    EVENT_TYPE eventType;
    uint32_t param1;
    uint32_t param2;
};

#endif  // EVENT_HANDLER_H
