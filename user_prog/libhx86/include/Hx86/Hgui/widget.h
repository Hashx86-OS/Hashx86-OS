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

#ifndef HWIDGET_H
#define HWIDGET_H

#include <Hx86/Hsyscalls/Hsyscallsgui.h>
#include <Hx86/stdint.h>
#include <Hx86/utils/linkedList.h>

/** struct WidgetData - Payload buffer passed to HguiAPI() for widget operations.
 * @param0: Device-side widget ID or target widget ID.
 * @param1: Optional signed parameter (geometry, font size, etc.).
 * @param2: Optional signed parameter (geometry, alignment, etc.).
 * @param3: Optional unsigned parameter.
 * @param4: Optional unsigned parameter.
 * @param5: Optional string parameter (text, label, icon name).
 * @param6: Optional string output buffer.
 * @param7: Optional string output buffer.
 */
struct WidgetData {
    uint32_t param0;
    int32_t param1;
    int32_t param2;
    uint32_t param3;
    uint32_t param4;
    const char* param5;
    char* param6;
    char* param7;
};

// Font style used to render widget text.
typedef enum {
    REGULAR = 0x0,
    BOLD = 0x1,
    ITALIC = 0x2,
    BOLD_ITALIC = 0x3,
} FontType;

// Named font slot sizes.
typedef enum {
    TINY = 0,
    SMALL = 1,
    MEDIUM = 2,
    LARGE = 3,
    XLARGE = 4,
} FontSize;

// Widget sizing behavior.
typedef enum {
    FIXED = 0,
    CONTENT = 1,
    FILL = 2,
} SizeMode;

/** struct Padding - Uniform widget padding around the content area. */
struct Padding {
    uint8_t t, r, b, l;
};

/** class Widget - Base class for all GUI widgets.
 *
 * Every widget mirrors a kernel-side widget record that is created through the
 * HguiAPI() syscall interface. Callbacks can be registered as global functions
 * or as member functions handled through a callback instance.
 */
class Widget {
protected:
    int32_t pid;
    LinkedList<Widget*> childrenList;

public:
    Widget(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h);
    ~Widget();
    uint32_t ID;
    Widget* parent;
    // Callback invoked by a non-member (global) onClick handler.
    void (*onClickPtr)();

    // Instance and callback used to dispatch onClick to a member function.
    void* callbackInstance;
    void (*onClickMemberPtr)(void*);

    // Keyboard callback hooks used by the desktop event/input dispatcher.
    void (*onKeyPressPtr)(uint8_t scancode, bool shiftPressed);
    void* keyCallbackInstance;
    void (*onKeyPressMemberPtr)(void*, uint8_t scancode, bool shiftPressed);

    /** FindWidgetByID() - Search this widget and its children for a widget ID.
     * @searchID: Kernel-side widget ID to find.
     *
     * Return: Pointer to the matching widget, or nullptr if not found.
     */
    Widget* FindWidgetByID(uint32_t searchID);
    uint32_t MeasureText(const char* text, int32_t fontSizePx);
    virtual bool AddChild(Widget* child);
    virtual bool RemoveChild(Widget* child);

    virtual void OnMouseDown(int32_t x, int32_t y, uint8_t button);
    virtual void OnMouseUp(int32_t x, int32_t y, uint8_t button);
    virtual void OnMouseMove(int32_t oldx, int32_t oldy, int32_t newx, int32_t newy);

    // Allow both global and member function callbacks.
    void OnClick(void (*callback)());                       // Non-member function callback.
    void OnClick(void* instance, void (*callback)(void*));  // Member function callback wrapper.

    void OnKeyPress(void (*callback)(uint8_t scancode, bool shiftPressed));
    void OnKeyPress(void* instance, void (*callback)(void*, uint8_t scancode, bool shiftPressed));

    bool setEnabled(bool en);
};

/** class CompositeWidget - Widget that can host child widgets. */
class CompositeWidget : public Widget {
private:
public:
    CompositeWidget(Widget* parent, int32_t x, int32_t y, uint32_t w, uint32_t h);
    ~CompositeWidget();
};

#endif  // HWIDGET_H
