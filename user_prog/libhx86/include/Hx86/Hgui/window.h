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

#ifndef WINDOW_H
#define WINDOW_H

#include <Hx86/Hgui/widget.h>
#include <Hx86/Hsyscalls/Hsyscallsgui.h>
#include <Hx86/globals.h>

/** class Window - Title-bar window widget that can host child widgets. */
class Window : public CompositeWidget {
protected:
public:
    Window(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h);
    ~Window();
    void show();
    void Close();

    void OnMouseDown(int32_t x, int32_t y, uint8_t button);
    void OnMouseUp(int32_t x, int32_t y, uint8_t button);
    void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy);
    void setWindowTitle(const char* title);
};

#endif  // WINDOW_H
