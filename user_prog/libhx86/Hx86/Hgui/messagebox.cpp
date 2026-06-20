/**
 * @file        messagebox.cpp
 * @brief       MessageBox Component for Hx86 GUI
 *
 * @date        01/02/2026
 * @version     1.0.0
 */

#include <Hx86/Hgui/messagebox.h>

MessageBox::MessageBox(Widget* parent, const char* title, const char* message, Type type, int* resultPtr)
    : Window(parent, 0, 0, MSGBOXWIDTH, MSGBOXHEIGHT), message(message), resultPtr(resultPtr) {
    this->setWindowTitle(title);

    Label* msgLabel = new Label(this, 10, 10, MSGBOXWIDTH - 20, 40, message);
    this->AddChild(msgLabel);

    if (type == YES_NO) {
        Button* yesBtn = new Button(this, MSGBOXWIDTH / 2 - 80, MSGBOXHEIGHT - 40, 70, 25, "Yes");
        Button* noBtn = new Button(this, MSGBOXWIDTH / 2 + 10, MSGBOXHEIGHT - 40, 70, 25, "No");
        if (resultPtr) {
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
        }
        this->AddChild(yesBtn);
        this->AddChild(noBtn);
    } else {
        Button* okBtn = new Button(this, MSGBOXWIDTH / 2 - 35, MSGBOXHEIGHT - 40, 70, 25, "OK");
        okBtn->OnClick(this, [](void* inst) { static_cast<MessageBox*>(inst)->Close(); });
        this->AddChild(okBtn);
    }
}

MessageBox::~MessageBox() {
}
