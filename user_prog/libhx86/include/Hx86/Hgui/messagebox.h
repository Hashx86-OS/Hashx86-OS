#ifndef MESSAGEBOX_H
#define MESSAGEBOX_H

#include <Hx86/Hgui/button.h>
#include <Hx86/Hgui/label.h>
#include <Hx86/Hgui/window.h>

#define MSGBOXWIDTH  300
#define MSGBOXHEIGHT 120

enum Type { INFO, YES_NO };

class MessageBox : public Window {
private:
    const char* message;
    int* resultPtr;

public:
    MessageBox(Widget* parent, const char* title, const char* message, Type type, int* resultPtr = nullptr);
    ~MessageBox();
};

#endif  // MESSAGEBOX_H
