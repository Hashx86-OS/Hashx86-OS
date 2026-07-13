#ifndef DESKTOP_H
#define DESKTOP_H

#include <Hx86/Hgui/eventHandler.h>
#include <Hx86/Hgui/widget.h>
#include <Hx86/debug.h>

class Desktop : public CompositeWidget {
protected:
public:
    static Desktop* activeInstance;
    Desktop();
    ~Desktop();
    void initEventHandler();
    void RestoreFocus();
};

#endif  // DESKTOP_H
