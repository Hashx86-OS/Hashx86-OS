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

#ifndef HGUI_KERNEL_H
#define HGUI_KERNEL_H

#include <core/interrupts.h>
#include <core/memory.h>
#include <core/scheduler.h>
#include <debug.h>
#include <gui/eventHandler.h>
#include <gui/gui.h>
#include <gui/listview.h>
#include <types.h>
#include <utils/linkedList.h>

/**
 * enum REQ_Element - Widget type codes used by the GUI syscall interface.
 *
 * Each code selects which widget class the kernel instantiates or targets for
 * a syscall request.
 */
typedef enum {
    WIDGET = 0x0,
    WINDOW = 0x1,
    BUTTON = 0x2,
    EVENT = 0x3,
    DESKTOP = 0x4,
    LABEL = 0x5,
    LISTVIEW = 0x6,
    TERMINAL_VIEW = 0x7,
    FONT = 0x8,
    ICON_BUTTON = 0x9,
} REQ_Element;

/**
 * enum REQ_MODE - Operation codes for the GUI syscall interface.
 *
 * Each code selects the operation applied to the widget named by REQ_Element.
 */
typedef enum {
    CREATE = 0x0,
    ADD_CHILD = 0x1,
    REMOVE_CHILD = 0x2,
    DELETE = 0x3,
    GET = 0x4,
    SET_TEXT = 0x5,
    SET_FONT_SIZE = 0x6,
    SET_ITEMS = 0x7,
    CLEAR_ITEMS = 0x8,
    GET_SELECTED = 0x9,
    SET_SCROLL_META = 0xA,
    GET_SCROLL_ACTION = 0xB,
    MEASURE_TEXT = 0xC,
    SET_FONT_TYPE = 0xD,
    SET_COLOR = 0xE,
    SET_BACKGROUND = 0xF,
    SET_ALIGNMENT = 0x10,
    SET_WIDTH = 0x11,
    SET_HEIGHT = 0x12,
    SET_ITEM_HEIGHT = 0x13,
    SET_ENABLED = 0x14,
    SET_ICON = 0x15,
    SET_ICON_FONT_SIZE = 0x16,
} REQ_MODE;

/**
 * struct WidgetData - Register window for the GUI syscall.
 *
 * Carries the request's numeric and string parameters packed into the caller's
 * registers when the GUI interrupt fires.
 */
struct WidgetData {
    uint32_t param0;
    int32_t param1;
    uint32_t param2;
    uint32_t param3;
    uint32_t param4;
    char* param5;
    char* param6;
    char* param7;
};

/**
 * class HguiHandler - Kernel-side GUI interrupt handler.
 *
 * Services the GUI syscall interrupt: it decodes the request element and mode,
 * creates or manipulates widgets on behalf of the calling process, and keeps a
 * registry of every widget so they can be destroyed when a process exits.
 */
class HguiHandler : public InterruptHandler {
private:
    LinkedList<Widget*> HguiWidgets;
    uint32_t widgetIDCounter = 1000;

public:
    static HguiHandler* activeInstance;

    /**
     * HguiHandler() - Register the GUI syscall interrupt handler.
     * @InterruptNumber: IRQ number to bind.
     * @interruptManager: Manager that dispatches the interrupt.
     */
    HguiHandler(uint8_t InterruptNumber, InterruptManager* interruptManager);

    /**
     * ~HguiHandler() - Unregister the handler and destroy all widgets.
     */
    ~HguiHandler();

    /**
     * HandleInterrupt() - Dispatch an incoming GUI syscall.
     * @esp: User stack pointer carrying the request data.
     *
     * Return: The syscall completion code placed in EAX before resuming.
     */
    virtual uint32_t HandleInterrupt(uint32_t esp);

    /** HandleWidget() - Create or configure a base widget from a syscall. */
    virtual int32_t HandleWidget(CPUState* cpu, const WidgetData* data);

    /** HandleWindow() - Create or configure a window from a syscall. */
    virtual int32_t HandleWindow(CPUState* cpu, const WidgetData* data);

    /** HandleButton() - Create or configure a button from a syscall. */
    virtual int32_t HandleButton(CPUState* cpu, const WidgetData* data);

    /** HandleIconButton() - Create or configure an icon button from a syscall. */
    virtual int32_t HandleIconButton(CPUState* cpu, const WidgetData* data);

    /** HandleLabel() - Create or configure a label from a syscall. */
    virtual int32_t HandleLabel(CPUState* cpu, const WidgetData* data);

    /** HandleListView() - Create or configure a list view from a syscall. */
    virtual int32_t HandleListView(CPUState* cpu, const WidgetData* data);

    /** HandleTerminalView() - Create or configure a terminal view from a syscall. */
    virtual int32_t HandleTerminalView(CPUState* cpu, const WidgetData* data);

    /** HandleFont() - Create or configure a font from a syscall. */
    virtual int32_t HandleFont(CPUState* cpu, const WidgetData* data);

    /**
     * HandleEvent() - Forward an input event to the owning process's handler.
     * @cpu: CPU state holding the event payload.
     *
     * Return: Zero on success, nonzero if no handler owned the event.
     */
    virtual int32_t HandleEvent(CPUState* cpu);

    /**
     * RemoveAppByPID() - Destroy every widget owned by a process.
     * @PID: Owner process ID.
     */
    void RemoveAppByPID(uint32_t PID);

    /**
     * FindWidgetByID() - Look up a widget by instance ID.
     * @searchID: Instance ID to match.
     *
     * Return: The matching widget, or NULL if not found.
     */
    Widget* FindWidgetByID(uint32_t searchID);

    /**
     * getNewID() - Allocate a unique widget instance ID.
     *
     * Return: The next monotonically increasing ID.
     */
    uint32_t getNewID();

    /**
     * FindOwnedWidget() - Find a widget of type T owned by a process.
     * @id: Widget instance ID.
     * @pid: Owner process ID to match.
     * @typeCheck: Member type-check predicate (e.g. Widget::IsWindow).
     *
     * Return: A typeded pointer to the widget, or NULL if it is missing, of the
     *         wrong type, or owned by another process.
     */
    template <typename T>
    T* FindOwnedWidget(uint32_t id, uint32_t pid, bool (Widget::*typeCheck)() const) {
        Widget* w = FindWidgetByID(id);
        if (!w || !(w->*typeCheck)() || w->PID != pid) return nullptr;
        return static_cast<T*>(w);
    }

    /**
     * RemoveWidget() - Destroy a widget and detach it from its parent.
     * @w: Widget to remove.
     */
    static void RemoveWidget(Widget* w);
};

#endif  // HGUI_KERNEL_H
