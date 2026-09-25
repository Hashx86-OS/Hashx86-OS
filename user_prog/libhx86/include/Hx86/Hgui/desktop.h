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

#ifndef DESKTOP_H
#define DESKTOP_H

#include <Hx86/Hgui/eventHandler.h>
#include <Hx86/Hgui/widget.h>
#include <Hx86/debug.h>

/** class Desktop - Root widget that owns the event handler and input focus.
 *
 * A single desktop exists per process; init_graphics() creates it and
 * initEventHandler() registers the kernel-side event loop thread.
 */
class Desktop : public CompositeWidget {
protected:
public:
    static Desktop* activeInstance;
    Desktop();
    ~Desktop();
    /** initEventHandler() - Register the GUI event handler thread with the kernel. */
    void initEventHandler();
    /** RestoreFocus() - Reassign keyboard focus to the first widget with a key callback. */
    void RestoreFocus();
};

#endif  // DESKTOP_H
