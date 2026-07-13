#ifndef WINDOW_H
#define WINDOW_H

#include <Hx86/Hgui/widget.h>
#include <Hx86/Hsyscalls/Hsyscallsgui.h>
#include <Hx86/globals.h>

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
