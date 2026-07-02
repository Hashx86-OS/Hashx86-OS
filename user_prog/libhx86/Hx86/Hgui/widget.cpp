/**
 * @file        widget.cpp
 * @brief       Widget (part of #x86 GUI Framework)
 *
 * @date        10/02/2025
 * @version     1.0.0-beta
 */

#include <Hx86/Hgui/widget.h>

Widget::Widget(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h) {
    this->parent = parent;
    this->onClickPtr = nullptr;
    this->callbackInstance = nullptr;
    this->onClickMemberPtr = nullptr;
    this->onKeyPressPtr = nullptr;
    this->keyCallbackInstance = nullptr;
    this->onKeyPressMemberPtr = nullptr;
    this->ID = 0;
    this->pid = 0;
}

Widget::~Widget() {}

Widget* Widget::FindWidgetByID(uint32_t searchID) {
    if (this->ID == searchID) return this;

    Widget* result = nullptr;

    childrenList.ForEach([&](Widget* c) {
        if (result) return;  // Already found

        if (c->ID == searchID)
            result = c;
        else {
            Widget* nested = c->FindWidgetByID(searchID);
            if (nested) result = nested;
        }
    });

    return result;
}

bool Widget::AddChild(Widget* child) {
    WidgetData data = {(uint32_t)this->ID, (uint32_t)child->ID};
    uint32_t ret = HguiAPI(WIDGET, ADD_CHILD, (void*)&data);

    if (ret == 1) {
        childrenList.Add(child);
        return true;
    }

    return false;
}

bool Widget::RemoveChild(Widget* child) {
    return childrenList.Remove([&](Widget* c) { return c == child; });
}

void Widget::OnMouseDown(int32_t x, int32_t y, uint8_t button) {}

void Widget::OnMouseUp(int32_t x, int32_t y, uint8_t button) {}

void Widget::OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy) {}

void Widget::OnClick(void (*callback)()) {
    onClickPtr = callback;
}

void Widget::OnClick(void* instance, void (*callback)(void*)) {
    callbackInstance = instance;
    onClickMemberPtr = callback;
}

void Widget::OnKeyPress(void (*callback)(uint8_t scancode, bool shiftPressed)) {
    onKeyPressPtr = callback;
}

void Widget::OnKeyPress(void* instance,
                        void (*callback)(void*, uint8_t scancode, bool shiftPressed)) {
    keyCallbackInstance = instance;
    onKeyPressMemberPtr = callback;
}

uint32_t Widget::MeasureText(const char* text, int32_t fontSizePx) {
    WidgetData data = {0, fontSizePx, 0, 0, 0, text};
    uint32_t result = HguiAPI(FONT, MEASURE_TEXT, (void*)&data);
    // HguiAPI returns (int32_t)-1 on failure, which becomes 0xFFFFFFFF as uint32_t
    if (result == 0xFFFFFFFF) return 0;
    return result;
}

bool Widget::setEnabled(bool en) {
    WidgetData data = {ID, en ? 1 : 0};
    return HguiAPI(WIDGET, SET_ENABLED, (void*)&data);
}

CompositeWidget::CompositeWidget(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h)
    : Widget(parent, x, y, w, h) {}

CompositeWidget::~CompositeWidget() {}
