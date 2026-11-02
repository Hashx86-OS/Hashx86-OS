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

#ifndef GUI_INPUT_QUEUE_H
#define GUI_INPUT_QUEUE_H

#include <gui/inputEvent.h>
#include <types.h>

/**
 * class InputQueue - A fixed-capacity FIFO of decoded input events.
 *
 * Drivers push events from interrupt context; the desktop task pops them once
 * per frame, giving a single deterministic drain point. No allocation, so it is
 * safe to use from an interrupt handler.
 */
class InputQueue {
public:
    static const uint32_t CAPACITY = 256;

    InputQueue();

    /**
     * Push() - Append an event.
     * @event: Event to enqueue.
     *
     * Return: True on success, false when the queue is full (the event is
     *         dropped and the overflow counter is incremented).
     */
    bool Push(const InputEvent& event);

    /**
     * Pop() - Remove the oldest event.
     * @out: Receives the event when the queue is non-empty.
     *
     * Return: True when an event was returned.
     */
    bool Pop(InputEvent& out);

    /** Count() - Number of queued events. */
    uint32_t Count() const {
        return count;
    }

    /** OverflowCount() - Number of events dropped because the queue was full. */
    uint32_t OverflowCount() const {
        return overflow;
    }

    /** Clear() - Discard every queued event. */
    void Clear();

private:
    InputEvent buffer[CAPACITY];
    uint32_t head;
    uint32_t tail;
    uint32_t count;
    uint32_t overflow;
};

#endif  // GUI_INPUT_QUEUE_H
