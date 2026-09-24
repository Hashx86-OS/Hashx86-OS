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

#include <Hx86/Hgui/messagebox.h>

MessageBox::MessageBox(Widget* parent, const char* title, const char* message, Type type,
                       int* resultPtr)
    : Window(parent, 0, 0, MSGBOXWIDTH, MSGBOXHEIGHT), message(message), resultPtr(resultPtr) {
    this->setWindowTitle(title);

    Label* msgLabel = new Label(this, 10, 10, MSGBOXWIDTH - 20, 40, message);
    this->AddChild(msgLabel);

    if (type == YES_NO) {
        Button* yesBtn = new Button(this, MSGBOXWIDTH / 2 - 80, MSGBOXHEIGHT - 40, 70, 25, "Yes");
        Button* noBtn = new Button(this, MSGBOXWIDTH / 2 + 10, MSGBOXHEIGHT - 40, 70, 25, "No");
        yesBtn->OnClick(this, [](void* inst) {
            MessageBox* mb = static_cast<MessageBox*>(inst);
            if (mb->resultPtr) *mb->resultPtr = 1;
            mb->Close();
        });
        noBtn->OnClick(this, [](void* inst) {
            MessageBox* mb = static_cast<MessageBox*>(inst);
            if (mb->resultPtr) *mb->resultPtr = 0;
            mb->Close();
        });
        this->AddChild(yesBtn);
        this->AddChild(noBtn);
    } else {
        Button* okBtn = new Button(this, MSGBOXWIDTH / 2 - 35, MSGBOXHEIGHT - 40, 70, 25, "OK");
        okBtn->OnClick(this, [](void* inst) { static_cast<MessageBox*>(inst)->Close(); });
        this->AddChild(okBtn);
    }
}

MessageBox::~MessageBox() {}
