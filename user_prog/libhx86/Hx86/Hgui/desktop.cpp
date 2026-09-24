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

#include <Hx86/Hgui/desktop.h>
#include <Hx86/Hx86.h>

Desktop* Desktop::activeInstance = nullptr;
static volatile uint32_t g_focusedWidgetID = 0;

void Desktop::RestoreFocus() {
    if (g_focusedWidgetID != 0) return;
    childrenList.ForEach([&](Widget* c) {
        if (g_focusedWidgetID == 0 &&
            (c->onKeyPressPtr || (c->onKeyPressMemberPtr && c->keyCallbackInstance))) {
            g_focusedWidgetID = c->ID;
        }
    });
}

static void DispatchKeyToFocused(uint8_t scancode, bool shiftPressed) {
    if (!Desktop::activeInstance) return;

    if (g_focusedWidgetID == 0) {
        Desktop::activeInstance->RestoreFocus();
    }
    if (g_focusedWidgetID == 0) return;

    Widget* target = Desktop::activeInstance->FindWidgetByID((uint32_t)g_focusedWidgetID);
    if (!target) {
        g_focusedWidgetID = 0;
        return;
    }

    if (target->onKeyPressPtr) {
        target->onKeyPressPtr(scancode, shiftPressed);
    } else if (target->onKeyPressMemberPtr && target->keyCallbackInstance) {
        target->onKeyPressMemberPtr(target->keyCallbackInstance, scancode, shiftPressed);
    }
}

Desktop::Desktop() : CompositeWidget(0, 0, 0, 1, 1) {
    activeInstance = this;
    this->ID = 0;
    this->initEventHandler();
}

Desktop::~Desktop() {}

void EventHandlerHGUI(void* arg) {
    (void)arg;
    printf("Event Handler thread started\n");

    uint8_t prevKeys[128];
    for (int i = 0; i < 128; i++) {
        prevKeys[i] = 0;
    }

    while (1) {
        if (!Desktop::activeInstance) {
            syscall_sleep(1);
            continue;
        }

        Widget* tmpWidget = nullptr;
        int32_t ret = (int32_t)HguiAPI(EVENT, GET, nullptr);

        if (ret >= 0) {
            uint32_t widgetID = (ret >> 16);
            EVENT_TYPE event = (EVENT_TYPE)(ret & 0xFFFF);

            switch (event) {
                case ON_WINDOW_CLOSE:
                    tmpWidget = Desktop::activeInstance->FindWidgetByID(widgetID);
                    if (tmpWidget) {
                        if (g_focusedWidgetID == widgetID) {
                            g_focusedWidgetID = 0;
                        }

                        if (!tmpWidget->parent || tmpWidget->parent->ID == 0) {
                            syscall_exit_group(10);
                        } else {
                            tmpWidget->parent->RemoveChild(tmpWidget);

                            // Also remove kernel-side widget record if present.
                            WidgetData deleteData = {0, (int32_t)tmpWidget->ID};
                            HguiAPI(WIDGET, DELETE, (void*)&deleteData);
                        }
                    }

                    break;

                case ON_CLICK:
                    tmpWidget = Desktop::activeInstance->FindWidgetByID(widgetID);
                    if (tmpWidget) {
                        g_focusedWidgetID = widgetID;

                        if (tmpWidget->onClickPtr) {
                            tmpWidget->onClickPtr();  // Call the non-member callback.
                        } else if (tmpWidget->onClickMemberPtr && tmpWidget->callbackInstance) {
                            tmpWidget->onClickMemberPtr(
                                tmpWidget
                                    ->callbackInstance);  // Call the member callback via instance.
                        }
                    }

                    break;

                default:
                    break;
            }
        }

        InputState input;
        syscall_get_input(&input);

        bool shiftPressed = input.keyStates[0x2A] || input.keyStates[0x36];

        for (uint8_t sc = 0; sc < 128; sc++) {
            if (input.keyStates[sc] && !prevKeys[sc]) {
                DispatchKeyToFocused(sc, shiftPressed);
            }
        }

        for (int i = 0; i < 128; i++) {
            prevKeys[i] = input.keyStates[i];
        }

        syscall_sleep(8);
    }
}

void Desktop::initEventHandler() {
    uint32_t eventTid = syscall_register_event_handler(EventHandlerHGUI, nullptr);
    printf("[PROG] : GUI event handler thread TID : %d\n", eventTid);
}
