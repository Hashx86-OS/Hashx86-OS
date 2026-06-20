
#ifndef HGUI_H
#define HGUI_H

#include <core/interrupts.h>
#include <core/memory.h>
#include <core/scheduler.h>
#include <debug.h>
#include <gui/eventHandler.h>
#include <gui/gui.h>
#include <gui/listview.h>
#include <types.h>
#include <utils/linkedList.h>

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
    ANIMATION = 0x9,
} REQ_Element;

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
    ANIM_START = 0x15,
    ANIM_START_EX = 0x16,
    ANIM_CANCEL = 0x17,
    ANIM_CANCEL_ALL = 0x18,
    ANIM_CHAIN = 0x19,
} REQ_MODE;

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

class HguiHandler : public InterruptHandler {
private:
    LinkedList<Widget*> HguiWidgets;
    uint32_t widgetIDCounter = 1000;

public:
    static HguiHandler* activeInstance;
    HguiHandler(uint8_t InterruptNumber, InterruptManager* interruptManager);
    ~HguiHandler();

    virtual uint32_t HandleInterrupt(uint32_t esp);
    virtual int32_t HandleWidget(CPUState* cpu, const WidgetData* data);
    virtual int32_t HandleWindow(CPUState* cpu, const WidgetData* data);
    virtual int32_t HandleButton(CPUState* cpu, const WidgetData* data);
    virtual int32_t HandleLabel(CPUState* cpu, const WidgetData* data);
    virtual int32_t HandleListView(CPUState* cpu, const WidgetData* data);
    virtual int32_t HandleTerminalView(CPUState* cpu, const WidgetData* data);
    virtual int32_t HandleFont(CPUState* cpu, const WidgetData* data);
    virtual int32_t HandleAnimation(CPUState* cpu, const WidgetData* data);
    virtual int32_t HandleEvent(CPUState* cpu);
    void RemoveAppByPID(uint32_t PID);
    Widget* FindWidgetByID(uint32_t searchID);
    uint32_t getNewID();
};

#endif  // HGUI_H
