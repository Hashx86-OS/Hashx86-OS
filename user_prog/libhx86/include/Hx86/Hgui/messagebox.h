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

#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#include <Hx86/Hgui/button.h>
#include <Hx86/Hgui/label.h>
#include <Hx86/Hgui/window.h>

// MessageBox dialog box size in pixels.
#define MSGBOXWIDTH 300
#define MSGBOXHEIGHT 120

// MessageBox button layout.
enum Type { INFO, YES_NO };

/** class MessageBox - Modal dialog window with optional Yes/No confirmation.
 * @resultPtr: Optional output; receives the user's choice when confirmed.
 */
class MessageBox : public Window {
private:
    const char* message;
    int* resultPtr;

public:
    MessageBox(Widget* parent, const char* title, const char* message, Type type,
               int* resultPtr = nullptr);
    ~MessageBox();
};

#endif  // MESSAGEBOX_H
